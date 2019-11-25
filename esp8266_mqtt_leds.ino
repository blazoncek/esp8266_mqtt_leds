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

#include "effects.h"

#define DEBUG   1

// Data pin that led data will be written out over
#define DATA_PIN D2   // use GPIO2 on ESP01
// Clock pin only needed for SPI based chipsets when not using hardware SPI
#define CLOCK_PIN D1  // use GPIO0 on ESP01

// This is an array of leds.  One item for each led in your strip.
CRGB *leds = NULL;
int numLEDs = 1;

int selectedEffect = -1;
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
              fadeToBlackBy(leds, numLEDs, 25);  // 10% fade
              delay(500);
              break;
              }
    
    case 0  : {
              RGBLoop();
              break;
              }

    case 1  : {
              FadeInOut(rRGB);
              break;
              }
              
    case 2  : {
              Strobe(CRGB::White);
              break;
              }

    case 3  : {
              HalloweenEyes(CRGB::Red, 1, 1, true, 10);
              break;
              }
              
    case 4  : {
              CylonBounce(min(numLEDs/20,4), 50);
              break;
              }
              
    case 5  : {
              NewKITT(rRGB, min((int)numLEDs/20,1), 30);
              break;
              }
              
    case 6  : {
              Twinkle(rRGB, 250, false);
              break;
              }
              
    case 7  : { 
              TwinkleRandom(10, 250, false);
              break;
              }
              
    case 8  : {
              // Sparkle
              Twinkle(CRGB::White, 250, true);
              break;
              }
               
    case 9  : {
              // SnowSparkle
              snowSparkle(20, random(100,750));
              break;
              }
              
    case 10 : {
              runningLights(rRGB, 50);
              break;
              }
              
    case 11 : {
              colorWipe(rRGB, 30);
              colorWipe(CRGB::Black, 30);
              break;
              }

    case 12 : {
              rainbowCycle(50);
              break;
              }

    case 13 : {
              theaterChase(rRGB, 100);
              break;
              }

    case 14 : {
              theaterChaseRainbow(100);
              break;
              }

    case 15 : {
              // Fire - Cooling rate, Sparking rate, speed delay (1000/FPS)
              Fire(55, 120, 30);
              break;
              }

    case 16 : {
              // simple bouncingBalls not included, since BouncingColoredBalls can perform this as well as shown below
              bouncingColoredBalls(1, &rRGB);
              break;
              }

    case 17 : {
              // multiple colored balls
              CRGB colors[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
              bouncingColoredBalls(3, colors);
              break;
              }

    case 18 : {
              meteorRain(CRGB::White, min((int)numLEDs/20,5), 64, true, 30);
              break;
              }

    case 19 : {
              sinelon(rRGB);
              break;
              }

  }
  if ( selectedEffect > 18 ) selectedEffect = 0;

}

// Apply LED color changes
void showStrip() {
  yield();    // allow other tasks
  if ( client.connected() ) client.loop(); //check MQTT
  
  // FastLED
  FastLED.show();
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
