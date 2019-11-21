/*
 ESP8266 MQTT client for driving WS28xx individually addressable LEDs

 Changing of effect is done via Domotiz MQTT API Slector call or via custom MQTT topic.

 (c) blaz@kristan-sp.si / 2019-11-16
*/

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <DNSServer.h>            //Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     //Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"

#define DEBUG   1

// Data pin that led data will be written out over
#define DATA_PIN D2   // use GPIO2 on ESP01
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define CLOCK_PIN D1  // use GPIO0 on ESP01

// This is an array of leds.  One item for each led in your strip.
CRGB *leds = NULL;
int selectedEffect = -1;
int numLEDs = 1;
byte *heat; // Fire effect static data
int gHue = 0;

// Update these with values suitable for your network.
char mqtt_server[40] = "192.168.70.11";
char mqtt_port[7]    = "1883";
char username[33]    = "";
char password[33]    = "";

char c_LEDtype[10]   = "WS2812";    // LED strip type
char c_numLEDs[4]    = "1";         // total number of LEDs on strip
char c_idx[4]        = "0";         // domoticz switch IDX (type Selector, values define type of LED effect)

// flag for saving data from WiFiManager
bool shouldSaveConfig = false;

long lastMsg = 0;
char msg[256];
char mac_address[16];
char outTopic[64];
char clientId[20];  // MQTT client ID

char MQTTBASE[16]    = "LEDstrip";

WiFiClient espClient;
PubSubClient client(espClient);

// private functions
void mqtt_callback(char*, byte*, unsigned int);
void mqtt_reconnect();
char *ftoa(float,char*,int d=2);
void saveConfigCallback();
void ledTimer();
void subtractInterval();

//-----------------------------------------------------------
// main setup
void setup() {
  char str[16];

  Serial.begin(115200);
  delay(5000);

  // Initialize the BUILTIN_LED pin as an output & set initial state LED on
  pinMode(BUILTIN_LED, OUTPUT);
  digitalWrite(BUILTIN_LED, LOW);

  String WiFiMAC = WiFi.macAddress();
  WiFiMAC.replace(":","");
  WiFiMAC.toCharArray(mac_address, 16);
  // Create client ID from MAC address
  sprintf(clientId, "esp-%s", &mac_address[6]);

  #if DEBUG
  Serial.println("");
  Serial.print("Host name: ");
  Serial.println(WiFi.hostname());
  Serial.print("MAC address: ");
  Serial.println(mac_address);
  Serial.println("Starting WiFi manager");
  #endif

  // request 16 bytes from EEPROM & write LED info
  EEPROM.begin(16);
//  EEPROM.get(0, str);

  #if DEBUG
  Serial.println("");
  Serial.print("EEPROM data: ");
  #endif
  for ( int i=0; i<15; i++ ) {
    str[i] = EEPROM.read(i);
    #if DEBUG
    Serial.print(str[i], HEX);
    Serial.print(":");
    #endif
  }
  #if DEBUG
  Serial.print(" (");
  Serial.print(str);
  Serial.println(")");
  #endif

  EEPROM.commit();
  EEPROM.end();
  if ( strncmp(str,"WS",2) == 0 ) {
    sscanf(str,"%6s%3s%3s",c_LEDtype,c_numLEDs,c_idx);
  }
  delay(120);

//----------------------------------------------------------
  //clean FS, for testing
  //SPIFFS.format();

  //read configuration from FS json
  if ( SPIFFS.begin() ) {
    #if DEBUG
    Serial.println("SPIFFS begin.");
    #endif

    if ( SPIFFS.exists("/config.json") ) {
      //file exists, reading and loading
      #if DEBUG
      Serial.println("File found.");
      #endif
      
      File configFile = SPIFFS.open("/config.json", "r");
      if ( configFile ) {
        size_t size = configFile.size();
        
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        
        DynamicJsonDocument doc(size);
        DeserializationError error = deserializeJson(doc, buf.get());
        if ( !error ) {
          strcpy(mqtt_server, doc["mqtt_server"]);
          strcpy(mqtt_port, doc["mqtt_port"]);
          strcpy(username, doc["username"]);
          strcpy(password, doc["password"]);
          #if DEBUG
          serializeJson(doc, Serial);
          #endif
        } else {
          #if DEBUG
          Serial.println("Error reading JSON from config file.");
          #endif
        }
      } else {
        #if DEBUG
        Serial.println("Config file does not exist.");
        #endif
      }
    }
  } else {
    #if DEBUG
    Serial.println("SPIFFS failed.");
    #endif
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 5);
  WiFiManagerParameter custom_username("username", "username", username, 32);
  WiFiManagerParameter custom_password("password", "password", password, 32);

  WiFiManagerParameter custom_ledtype("LEDtype", "LED type (WS28xx)", c_LEDtype, 6);
  WiFiManagerParameter custom_numleds("numLEDs", "Number of LEDs", c_numLEDs, 3);
  WiFiManagerParameter custom_idx("idx", "Domoticz switch IDX", c_idx, 3);

  // WiFiManager
  // Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  // reset settings (for debugging)
  //wifiManager.resetSettings();
  
  // set config save notify callback
  wifiManager.setSaveConfigCallback(saveConfigCallback);

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));
  
  //add all your parameters here
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_mqtt_port);
  wifiManager.addParameter(&custom_username);
  wifiManager.addParameter(&custom_password);

  wifiManager.addParameter(&custom_ledtype);
  wifiManager.addParameter(&custom_numleds);
  wifiManager.addParameter(&custom_idx);

  // set minimum quality of signal so it ignores AP's under that quality
  // defaults to 8%
  //wifiManager.setMinimumSignalQuality(10);
  
  // sets timeout until configuration portal gets turned off
  // useful to make it all retry or go to sleep
  // in seconds
  //wifiManager.setTimeout(120);

  // fetches ssid and pass and tries to connect
  // if it does not connect it starts an access point with the specified name
  // and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(clientId)) {
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi

  //read updated parameters
  strcpy(mqtt_server, custom_mqtt_server.getValue());
  strcpy(mqtt_port, custom_mqtt_port.getValue());
  strcpy(username, custom_username.getValue());
  strcpy(password, custom_password.getValue());

  strcpy(c_LEDtype, custom_ledtype.getValue());
  strcpy(c_numLEDs, custom_numleds.getValue());
  strcpy(c_idx, custom_idx.getValue());

  // sanity check
  numLEDs = max(min(atoi(c_numLEDs),999),1);
  sprintf(c_numLEDs,"%d",numLEDs);

  //save the custom parameters to FS
  if ( shouldSaveConfig ) {
    DynamicJsonDocument doc(1024);
    doc["mqtt_server"] = mqtt_server;
    doc["mqtt_port"] = mqtt_port;
    doc["username"] = username;
    doc["password"] = password;

    File configFile = SPIFFS.open("/config.json", "w");
    if ( !configFile ) {
      // failed to open config file for writing
      #if DEBUG
      Serial.println("Error opening config file fo writing.");
      #endif
    } else {
      serializeJson(doc, configFile);
      configFile.close();
      #if DEBUG
      serializeJson(doc, Serial);
      #endif
    }

    // request 16 bytes from EEPROM & write LED info
    sprintf(str,"%6s%3s%3s",c_LEDtype,c_numLEDs,c_idx);
    EEPROM.begin(16);
    EEPROM.put(0, str);
    EEPROM.commit();
    EEPROM.end();
    delay(120);

  }
//----------------------------------------------------------

  #if DEBUG
  Serial.print("LED type: ");
  Serial.println(c_LEDtype);
  Serial.print("# of LEDs: ");
  Serial.println(c_numLEDs);
  Serial.print("idx: ");
  Serial.println(c_idx);
  #endif

  // if connected set state LED off
  digitalWrite(BUILTIN_LED, HIGH);

  // allocate memory for LED array and initialize FastLED library
  //leds = new CRGB[numLEDs];
  leds = (CRGB*)calloc(numLEDs, sizeof(CRGB));
  if ( strcmp(c_LEDtype,"WS2801")==0 ) {
    FastLED.addLeds<WS2801, DATA_PIN, CLOCK_PIN, RGB>(leds, numLEDs).setCorrection(TypicalLEDStrip);
  } else if ( strcmp(c_LEDtype,"WS2811")==0 ) {
    FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, numLEDs).setCorrection(TypicalLEDStrip);
  } else if ( strcmp(c_LEDtype,"WS2812")==0 ) {
    FastLED.addLeds<WS2812, DATA_PIN, GRB>(leds, numLEDs).setCorrection(TypicalLEDStrip);
  } else {
  }
  heat = (byte*)calloc(numLEDs, sizeof(byte));  // Fire effect static data buffer

  randomSeed(millis());
  //selectedEffect = random(0,18);

  // initialize MQTT connection & provide callback function
  sprintf(outTopic, "%s/%s", MQTTBASE, clientId);
      
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(mqtt_callback);

}

void loop() {
  CRGB rRGB;
  
  if (!client.connected()) {
    mqtt_reconnect();
  }
  client.loop();

  rRGB = CHSV(gHue++,255,255);
  if ( gHue > 255 ) gHue = 0;
   
  #if DEBUG
  Serial.print("Color: ");
  Serial.print(rRGB.r,HEX);
  Serial.print(rRGB.g,HEX);
  Serial.println(rRGB.b,HEX);
  #endif

  switch ( selectedEffect ) {

    case -1 : {
              setAll(0,0,0);
              delay(1000);
              break;
              }
    
    case 0  : {
                // RGBLoop - no parameters
                RGBLoop();
                break;
              }

    case 1  : {
                // FadeInOut - Color (red, green. blue), end pause
                FadeInOut(rRGB.r, rRGB.g, rRGB.b, 250);
                break;
              }
              
    case 2  : {
                // Strobe - Color (red, green, blue), number of flashes, flash speed, end pause
                Strobe(0xff, 0xff, 0xff, 100, 50, 10);
                break;
              }

    case 3  : {
                // HalloweenEyes - Color (red, green, blue), Size of eye, space between eyes, fade (true/false),
                //   steps, fade delay, end pause
                HalloweenEyes(0xff, 0x00, 0x00, 
                              1, 1, 
                              true, random(5,50), random(50,100), 
                              random(500, 5000));
                break;
              }
              
    case 4  : {
                // CylonBounce - Color (red, green, blue), eye size, speed delay, end pause
                CylonBounce(0xff, 0x00, 0x00, 4, 70, 10);
                break;
              }
              
    case 5  : {
                // NewKITT - Color (red, green, blue), eye size, speed delay, end pause
                NewKITT(rRGB.r, rRGB.g, rRGB.b, (int)numLEDs/20+1, 30, 100);
                break;
              }
              
    case 6  : {
                // Twinkle - Color (red, green, blue), count, speed delay, only one twinkle (true/false) 
                Twinkle(0x00, 0x00, 0xff, 20, 100, false);
                break;
              }
              
    case 7  : { 
                // TwinkleRandom - twinkle count, speed delay, only one (true/false)
                TwinkleRandom(20, 100, false);
                break;
              }
              
    case 8  : {
                // Sparkle - Color (red, green, blue), speed delay
                Sparkle(0xff, 0xff, 0xff, 100);
                break;
              }
               
    case 9  : {
                // SnowSparkle - Color (red, green, blue), sparkle delay, speed delay
                SnowSparkle(0x10, 0x10, 0x10, 20, random(100,1000));
                break;
              }
              
    case 10 : {
                // Running Lights - Color (red, green, blue), wave dealy
                RunningLights(rRGB.r,rRGB.g,rRGB.b, 50);
                break;
              }
              
    case 11 : {
                // colorWipe - Color (red, green, blue), speed delay
                colorWipe(rRGB.r,rRGB.g,rRGB.b, 50);
                colorWipe(0,0,0, 50);
                break;
              }

    case 12 : {
                // rainbowCycle - speed delay
                rainbowCycle(20);
                break;
              }

    case 13 : {
                // theatherChase - Color (red, green, blue), speed delay
                theaterChase(rRGB.r,rRGB.g,rRGB.b,100);
                break;
              }

    case 14 : {
                // theaterChaseRainbow - Speed delay
                theaterChaseRainbow(100);
                break;
              }

    case 15 : {
                // Fire - Cooling rate, Sparking rate, speed delay (1000/FPS)
                Fire(55,120,30);
                break;
              }


              // simple bouncingBalls not included, since BouncingColoredBalls can perform this as well as shown below
              // BouncingColoredBalls - Number of balls, color (red, green, blue) array, continuous
              // CAUTION: If set to continuous then this effect will never stop!!! 
              
    case 16 : {
                // mimic BouncingBalls
                byte onecolor[1][3] = { {rRGB.r,rRGB.g,rRGB.b} };
                BouncingColoredBalls(1, onecolor, false);
                break;
              }

    case 17 : {
                // multiple colored balls
                byte colors[3][3] = { {0xff, 0x00, 0x00}, 
                                      {0x00, 0xff, 0x00}, 
                                      {0x00, 0x00, 0xff} };
                BouncingColoredBalls(3, colors, false);
                break;
              }

    case 18 : {
                // meteorRain - Color (red, green, blue), meteor size, trail decay, random trail decay (true/false), speed delay 
                meteorRain(0xff,0xff,0x40, (int)numLEDs/20+1, 64, true, 30);
                break;
              }

  }
  if ( selectedEffect > 18 ) selectedEffect = 0;

}


// *************************
// ** LEDEffect Functions **
// *************************
void RandomColor(char *c) {
  randomSeed(millis());
  c[0] = random(0,255);
  c[1] = random(0,255);
  c[2] = random(0,255);
}

void RGBLoop(){
  for(int j = 0; j < 3; j++ ) { 
    #if DEBUG
    Serial.println("RGBLoop FadeIn");
    #endif
    // Fade IN
    for(int k = 0; k < 256; k++) { 
      switch(j) { 
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;
        case 2: setAll(0,0,k); break;
      }
    }
    #if DEBUG
    Serial.println("RGBLoop FadeOut");
    #endif
    // Fade OUT
    for(int k = 255; k >= 0; k--) { 
      switch(j) { 
        case 0: setAll(k,0,0); break;
        case 1: setAll(0,k,0); break;
        case 2: setAll(0,0,k); break;
      }
    }
  }
}

void FadeInOut(byte red, byte green, byte blue, int EndPause){
  float r, g, b;
      
  #if DEBUG
  Serial.println("FadeInOut In");
  #endif
  for(int k = 0; k < 256; k=k+1) { 
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
  }
  delay(5);
     
  #if DEBUG
  Serial.println("FadeInOut Out");
  #endif
  for(int k = 255; k >= 0; k=k-2) {
    r = (k/256.0)*red;
    g = (k/256.0)*green;
    b = (k/256.0)*blue;
    setAll(r,g,b);
  }
  delay(EndPause);
}

void Strobe(byte red, byte green, byte blue, int StrobeCount, int FlashDelay, int EndPause){
  #if DEBUG
  Serial.println("Strobe");
  #endif
  for(int j = 0; j < StrobeCount; j++) {
    setAll(red,green,blue);
    delay(FlashDelay);
    setAll(0,0,0);
    delay(FlashDelay);
  }
  delay(EndPause);
}

void HalloweenEyes(byte red, byte green, byte blue, 
                   int EyeWidth, int EyeSpace, 
                   boolean Fade, int Steps, int FadeDelay,
                   int EndPause){
  int i;
  int StartPoint  = random( 0, numLEDs - (2*EyeWidth) - EyeSpace );
  int Start2ndEye = StartPoint + EyeWidth + EyeSpace;
  
  #if DEBUG
  Serial.println("HalloweenEyes");
  #endif  
  setAll(0,0,0); // Set all black
  #if DEBUG
  Serial.println("  Show eyes");
  #endif
  for(i = 0; i < EyeWidth; i++) {
    setPixel(StartPoint + i, red, green, blue);
    setPixel(Start2ndEye + i, red, green, blue);
  }
  showStrip();
  delay(1000);
  #if DEBUG
  Serial.println("  Blink");
  #endif
  for(i = 0; i < EyeWidth; i++) {
    setPixel(StartPoint + i, 0, 0, 0);
    setPixel(Start2ndEye + i, 0, 0, 0);
  }
  showStrip();
  delay(150);
  for(i = 0; i < EyeWidth; i++) {
    setPixel(StartPoint + i, red, green, blue);
    setPixel(Start2ndEye + i, red, green, blue);
  }
  showStrip();
  delay(750);
  
  if(Fade==true) {
    float r, g, b;
  
    #if DEBUG
    Serial.println("  Fade eyes");
    #endif
    for(int j = Steps; j >= 0; j--) {
      r = j*(red/Steps);
      g = j*(green/Steps);
      b = j*(blue/Steps);
      
      for(i = 0; i < EyeWidth; i++) {
        setPixel(StartPoint + i, r, g, b);
        setPixel(Start2ndEye + i, r, g, b);
      }
      showStrip();
      delay(FadeDelay);
    }
  }
  
  setAll(0,0,0); // Set all black
  delay(EndPause);
}

void CylonBounce(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){

  #if DEBUG
  Serial.println("CylonBounce");
  #endif
  setAll(0,0,0);
  for ( int i=0; i < numLEDs-EyeSize-2; i++ ) {
    if ( i>0 ) setPixel(i-1,0,0,0);
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    showStrip();
    delay(SpeedDelay);
  }

  delay(ReturnDelay);

  setAll(0,0,0);
  for ( int i=numLEDs-EyeSize-2; i > 0; i-- ) {
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    if ( i<numLEDs-EyeSize-2 ) setPixel(i+EyeSize+2,0,0,0);
    showStrip();
    delay(SpeedDelay);
  }
  
  delay(ReturnDelay);
}

void NewKITT(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay){
  #if DEBUG
  Serial.println("NewKITT");
  #endif
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  LeftToRight(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  RightToLeft(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  OutsideToCenter(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
  CenterToOutside(red, green, blue, EyeSize, SpeedDelay, ReturnDelay);
}

// used by NewKITT
void CenterToOutside(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  #if DEBUG
  Serial.println("CenterToOutside");
  #endif
  setAll(0,0,0);
  for(int i =((numLEDs-EyeSize)/2); i>=0; i--) {
    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      fadeToBlack(j, 64);
    }
    
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    
    setPixel(numLEDs-i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(numLEDs-i-j, red, green, blue); 
    }
    setPixel(numLEDs-i-EyeSize-1, red/10, green/10, blue/10);
    
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

// used by NewKITT
void OutsideToCenter(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  #if DEBUG
  Serial.println("OutsideToCenter");
  #endif
  setAll(0,0,0);
  for(int i = 0; i<=((numLEDs-EyeSize)/2); i++) {
    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      fadeToBlack(j, 64);
    }
    
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    
    setPixel(numLEDs-i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(numLEDs-i-j, red, green, blue); 
    }
    setPixel(numLEDs-i-EyeSize-1, red/10, green/10, blue/10);
    
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

// used by NewKITT
void LeftToRight(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  #if DEBUG
  Serial.println("LeftToRight");
  #endif
  setAll(0,0,0);
  for(int i = 0; i < numLEDs-EyeSize-2; i++) {
    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      fadeToBlack(j, 64);
    }
    
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

// used by NewKITT
void RightToLeft(byte red, byte green, byte blue, int EyeSize, int SpeedDelay, int ReturnDelay) {
  #if DEBUG
  Serial.println("RightToLeft");
  #endif
  setAll(0,0,0);
  for(int i = numLEDs-EyeSize-2; i > 0; i--) {
    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      fadeToBlack(j, 64);
    }
    
    setPixel(i, red/10, green/10, blue/10);
    for(int j = 1; j <= EyeSize; j++) {
      setPixel(i+j, red, green, blue); 
    }
    setPixel(i+EyeSize+1, red/10, green/10, blue/10);
    showStrip();
    delay(SpeedDelay);
  }
  delay(ReturnDelay);
}

void Twinkle(byte red, byte green, byte blue, int Count, int SpeedDelay, boolean OnlyOne) {
  #if DEBUG
  Serial.println("Twinkle");
  #endif
  setAll(0,0,0);
  for ( int i=0; i<Count; i++ ) {
    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      fadeToBlack(j, 64);
    }
    setPixel(random(numLEDs),red,green,blue);
    showStrip();
    delay(SpeedDelay);
    if ( OnlyOne ) { 
      setAll(0,0,0); 
    }
  }
 
  delay(SpeedDelay);
}

void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne) {
  #if DEBUG
  Serial.println("TwinkleRandom");
  #endif
  setAll(0,0,0);
  for (int i=0; i<Count; i++) {
     setPixel(random(numLEDs),random(0,255),random(0,255),random(0,255));
     showStrip();
     delay(SpeedDelay);
     if ( OnlyOne ) { 
       setAll(0,0,0); 
     }
   }
  
  delay(SpeedDelay);
}

void Sparkle(byte red, byte green, byte blue, int SpeedDelay) {
  int Pixel = random(numLEDs);
  #if DEBUG
  Serial.println("Sparkle");
  #endif
  setPixel(Pixel,red,green,blue);
  showStrip();
  delay(SpeedDelay);
  setPixel(Pixel,0,0,0);
  showStrip();
}

void SnowSparkle(byte red, byte green, byte blue, int SparkleDelay, int SpeedDelay) {
  #if DEBUG
  Serial.println("SnowSparkle");
  #endif
  setAll(red,green,blue);
  
  int Pixel = random(numLEDs);
  setPixel(Pixel,0xff,0xff,0xff);
  showStrip();
  delay(SparkleDelay);
  setPixel(Pixel,red,green,blue);
  showStrip();
  delay(SpeedDelay);
}

void RunningLights(byte red, byte green, byte blue, int WaveDelay) {
  int Position=0;
  
  #if DEBUG
  Serial.println("RunningLights");
  #endif
  for ( int j=0; j<numLEDs*2; j++ ) {
    Position++; // = 0; //Position + Rate;
    for ( int i=0; i<numLEDs; i++ ) {
      // sine wave, 3 offset waves make a rainbow!
      //float level = sin(i+Position) * 127 + 128;
      //setPixel(i,level,0,0);
      //float level = sin(i+Position) * 127 + 128;
      setPixel(i,((sin(i+Position) * 127 + 128)/255)*red,
                 ((sin(i+Position) * 127 + 128)/255)*green,
                 ((sin(i+Position) * 127 + 128)/255)*blue);
    }
    showStrip();
    delay(WaveDelay);
  }
}

void colorWipe(byte red, byte green, byte blue, int SpeedDelay) {
  #if DEBUG
  Serial.println("colorWipe");
  #endif
  for ( int i=0; i<numLEDs; i++ ) {
      setPixel(i, red, green, blue);
      showStrip();
      delay(SpeedDelay);
  }
}

void rainbowCycle(int SpeedDelay) {
  byte *c;

  #if DEBUG
  Serial.println("RainbowCycle");
  #endif
  for ( int j=0; j<256*5; j++ ) { // 5 cycles of all colors on wheel
    for ( int i=0; i< numLEDs; i++ ) {
      c = Wheel(((i * 256 / numLEDs) + j) & 255);
      setPixel(i, *c, *(c+1), *(c+2));
    }
    showStrip();
    delay(SpeedDelay);
  }
}

// used by rainbowCycle and theaterChaseRainbow
byte * Wheel(byte WheelPos) {
  static byte c[3];
  
  if(WheelPos < 85) {
   c[0]=WheelPos * 3;
   c[1]=255 - WheelPos * 3;
   c[2]=0;
  } else if(WheelPos < 170) {
   WheelPos -= 85;
   c[0]=255 - WheelPos * 3;
   c[1]=0;
   c[2]=WheelPos * 3;
  } else {
   WheelPos -= 170;
   c[0]=0;
   c[1]=WheelPos * 3;
   c[2]=255 - WheelPos * 3;
  }

  return c;
}

void theaterChase(byte red, byte green, byte blue, int SpeedDelay) {
  #if DEBUG
  Serial.println("theaterChase");
  #endif
  for ( int j=0; j<10; j++ ) {  //do 10 cycles of chasing
    for ( int q=0; q < 3; q++ ) {
      for ( int i=0; i < numLEDs; i=i+3 ) {
        setPixel(i+q, red, green, blue);    //turn every third pixel on
      }
      showStrip();
     
      delay(SpeedDelay);
     
      for ( int i=0; i < numLEDs; i=i+3 ) {
        setPixel(i+q, 0,0,0);        //turn every third pixel off
      }
      showStrip();
    }
  }
}

void theaterChaseRainbow(int SpeedDelay) {
  byte *c;
  
  #if DEBUG
  Serial.println("theaterChaseRainbow");
  #endif
  for ( int j=0; j < 256; j++ ) {     // cycle all 256 colors in the wheel
    for ( int q=0; q < 3; q++ ) {
        for ( int i=0; i < numLEDs; i=i+3 ) {
          c = Wheel( (i+j) % 255);
          setPixel(i+q, *c, *(c+1), *(c+2));    //turn every third pixel on
        }
        showStrip();
       
        delay(SpeedDelay);
       
        for (int i=0; i < numLEDs; i=i+3) {
          setPixel(i+q, 0,0,0);        //turn every third pixel off
        }
        showStrip();
    }
  }
}

void Fire(int Cooling, int Sparking, int SpeedDelay) {
  //static byte heat[NUM_LEDS]; // moved to setup()
  int cooldown;
  
  #if DEBUG
  Serial.println("Fire");
  #endif
  // Step 1.  Cool down every cell a little
  for( int i = 0; i < numLEDs; i++) {
    cooldown = random(0, ((Cooling * 10) / numLEDs) + 2);
    
    if(cooldown>heat[i]) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
  
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for( int k= numLEDs - 1; k >= 2; k--) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
  }
    
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
  }

  // Step 4.  Convert heat to LED colors
  for( int j = 0; j < numLEDs; j++) {
    setPixelHeatColor(j, heat[j] );
  }

  showStrip();
  delay(SpeedDelay);
}

void setPixelHeatColor (int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    setPixel(Pixel, 255, 255, heatramp);
  } else if( t192 > 0x40 ) {             // middle
    setPixel(Pixel, 255, heatramp, 0);
  } else {                               // coolest
    setPixel(Pixel, heatramp, 0, 0);
  }
}

void BouncingColoredBalls(int BallCount, byte colors[][3], boolean continuous) {
  float Gravity = -9.81;
  int StartHeight = 1;
  
  float Height[BallCount];
  float ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int   Position[BallCount];
  long  ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  boolean ballBouncing[BallCount];
  boolean ballsStillBouncing = true;
  
  #if DEBUG
  Serial.println("BouncingColoredBall");
  #endif
  for ( int i = 0 ; i < BallCount ; i++ ) {   
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0; 
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i)/pow(BallCount,2);
    ballBouncing[i]=true; 
  }

  while ( ballsStillBouncing ) {
    for ( int i = 0 ; i < BallCount ; i++ ) {
      TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
      Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i]/1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i]/1000;
  
      if ( Height[i] < 0 ) {                      
        Height[i] = 0;
        ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
        ClockTimeSinceLastBounce[i] = millis();
  
        if ( ImpactVelocity[i] < 0.01 ) {
          if (continuous) {
            ImpactVelocity[i] = ImpactVelocityStart;
          } else {
            ballBouncing[i]=false;
          }
        }
      }
      Position[i] = round( Height[i] * (numLEDs - 1) / StartHeight);
    }

    ballsStillBouncing = false; // assume no balls bouncing
    for (int i = 0 ; i < BallCount ; i++) {
      setPixel(Position[i],colors[i][0],colors[i][1],colors[i][2]);
      if ( ballBouncing[i] ) {
        ballsStillBouncing = true;
      }
    }
    
    showStrip();
    setAll(0,0,0);
  }
}

void meteorRain(byte red, byte green, byte blue, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  #if DEBUG
  Serial.println("meteorRain");
  #endif
  setAll(0,0,0);
  
  for ( int i = 0; i < numLEDs+numLEDs; i++ ) {

    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      if ( (!meteorRandomDecay) || (random(10)>5) ) {
        fadeToBlack(j, meteorTrailDecay );        
      }
    }
    
    // draw meteor
    for ( int j = 0; j < meteorSize; j++ ) {
      if ( ( i-j <numLEDs) && (i-j>=0) ) {
        setPixel(i-j, red, green, blue);
      } 
    }
    showStrip();
    delay(SpeedDelay);
  }
}

// used by meteorrain
void fadeToBlack(int ledNo, byte fadeValue) {
   // FastLED
   leds[ledNo].fadeToBlackBy( fadeValue );
}

// *** REPLACE TO HERE ***


// ***************************************
// ** Common Functions **
// ***************************************

// Apply LED color changes
void showStrip() {
  yield();    // allow other tasks
  if ( client.connected() ) client.loop(); //check MQTT
  
  // FastLED
  FastLED.show();
}

// Set a LED color (not yet visible)
void setPixel(int Pixel, byte red, byte green, byte blue) {
   // FastLED
   leds[Pixel].r = red;
   leds[Pixel].g = green;
   leds[Pixel].b = blue;
}

// Set all LEDs to a given color and apply it (visible)
void setAll(byte red, byte green, byte blue) {
  for(int i = 0; i < numLEDs; i++ ) {
    setPixel(i, red, green, blue); 
  }
  showStrip();
}


//---------------------------------------------------
// MQTT callback function
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char tmp[80];
  
  payload[length] = '\0'; // "just in case" fix
/*
  #if DEBUG
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.println("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  #endif
*/
  // convert to String & int for easier handling
  String newPayload = String((char *)payload);

  if ( strcmp(topic,"domoticz/out") == 0 ) {

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);
  
    if ( doc["idx"] == atoi(c_idx) ) {
      // proper IDX found
      String action = doc["svalue1"];
      int actionId = action.toInt();
  
      // apply new effect
      selectedEffect = (actionId/10)-1;
  
      // Publish effect state
      sprintf(tmp, "%s/effect", outTopic);
      sprintf(msg, "%d", selectedEffect);
      client.publish(tmp, msg);
    
      #if DEBUG
      Serial.print("Selected effect: ");
      Serial.println(selectedEffect, DEC);
      #endif
    }

  } else if ( strstr(topic, MQTTBASE) ) {
    
    if ( strstr(topic,"/command/leds") ) {
      
      numLEDs = max(min((int)newPayload.toInt(),999),1);
      sprintf(c_numLEDs,"%d",numLEDs);
      #if DEBUG
      Serial.print("New # of LEDs: ");
      Serial.println(numLEDs, DEC);
      #endif
      
    } else if ( strstr(topic,"/command/idx") ) {
      
      sprintf(c_idx,"%d",max(min((int)newPayload.toInt(),999),1));
      #if DEBUG
      Serial.print("New idx: ");
      Serial.println(c_idx);
      #endif
  
    } else if ( strstr(topic,"/command/ledtype") ) {
      
      newPayload.toCharArray(c_LEDtype,7);
      #if DEBUG
      Serial.print("New LED type: ");
      Serial.println(c_LEDtype);
      #endif
  
    } else if ( strstr(topic,"/command/effect") ) {
      
      selectedEffect = max(min((int)newPayload.toInt(),18),-1);
      #if DEBUG
      Serial.print("New effect: ");
      Serial.println(selectedEffect, DEC);
      #endif
      return;
  
    } else if ( strstr(topic,"/command/reset") ) {
      
      ESP.reset();
  
    }

    // request 16 bytes from EEPROM & write LED info
    sprintf(tmp,"%6s%3s%3s",c_LEDtype,c_numLEDs,c_idx);
    Serial.println(tmp);
    EEPROM.begin(16);
    for ( int i=0; i<13; i++ ) {
      EEPROM.write(i, tmp[i]);
    }
    EEPROM.commit();
    EEPROM.end();
    delay(120);
    
  }
}

//----------------------------------------------------
// MQTT reconnect handling
void mqtt_reconnect() {
  char tmp[64];

  #if DEBUG
  Serial.print("Reconnecting.");
  #endif

  digitalWrite(BUILTIN_LED, LOW); // LED on

  // Loop until we're reconnected
  while ( !client.connected() ) {
    // Attempt to connect
    if ( strlen(username)==0? client.connect(clientId): client.connect(clientId, username, password) ) {
      
      // Once connected, publish an announcement...
      DynamicJsonDocument doc(256);
      doc["mac"] = WiFi.macAddress(); //.toString().c_str();
      doc["ip"] = WiFi.localIP().toString();  //.c_str();
      doc["LEDtype"] = c_LEDtype;
      doc["numLEDs"] = c_numLEDs;
      doc["idx"] = c_idx;

      size_t n = serializeJson(doc, msg);
      sprintf(tmp, "%s/announce", outTopic);
      client.publish(tmp, msg);
      
      // ... and resubscribe
      sprintf(tmp, "%s/%s/command/#", MQTTBASE, clientId);
      client.subscribe(tmp);
      client.subscribe("domoticz/out");
      #if DEBUG
      Serial.println("Connected & subscribed.");
      #endif
    } else {
      #if DEBUG
      Serial.print(".");
      #endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  digitalWrite(BUILTIN_LED, HIGH);  // LED off

}

// reverses a string 'str' of length 'len' 
void reverse(char *str, int len) 
{ 
    int i=0, j=len-1, temp; 
    while (i<j) 
    { 
        temp = str[i]; 
        str[i] = str[j]; 
        str[j] = temp; 
        i++; j--; 
    } 
} 

// Converts a given integer x to string str.  d is the number 
// of digits required in output. If d is more than the number 
// of digits in x, then 0s are added at the beginning. 
int intToStr(int x, char *str, int d) 
{ 
    int i = 0, s = x<0;
    while (x) 
    { 
      str[i++] = (abs(x)%10) + '0'; 
      x = x/10; 
    } 
  
    // If number of digits required is more, then 
    // add 0s at the beginning 
    while (i < d)
      str[i++] = '0';

    if ( s )
      str[i++] = '-';
  
    reverse(str, i); 
    str[i] = '\0'; 
    return i; 
} 
  
// Converts a floating point number to string. 
char *ftoa(float n, char *res, int afterpoint) 
{ 
  // Extract integer part 
  int ipart = (int)n; 
  
  // Extract floating part 
  float fpart = n - (float)ipart; 
  
  // convert integer part to string 
  int i = intToStr(ipart, res, 0); 
  
  // check for display option after point 
  if (afterpoint != 0) 
  { 
    res[i] = '.';  // add dot 

    // Get the value of fraction part upto given no. 
    // of points after dot. The third parameter is needed 
    // to handle cases like 233.007 
    fpart = fpart * pow(10, afterpoint); 

    intToStr(abs((int)fpart), res + i + 1, afterpoint); 
  }
  return res;
} 

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  shouldSaveConfig = true;
}
