/*
 ESP8266 MQTT client for driving WS28xx individually addressable LEDs

 Changing of effect is done via Domotiz MQTT API Slector call or via custom MQTT topic.

 (c) blaz@kristan-sp.si / 2019-11-16
*/

// global includes (libraries)

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <ArduinoOTA.h>           // OTA updates
#include <EEPROM.h>
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#define MQTT_MAX_PACKET_SIZE 1024
#include <PubSubClient.h>         // MQTT client

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include "FastLED.h"


// local includes

#include "effects.h"

#define DEBUG   1

int numZones = 0;                           // number of zones (LED strips), each zone requires a GPIO pin (or two)
int numLEDs[MAXZONES];                      // number of pixels in each zone
int numSections[MAXZONES];                  // number of sections in each zone
int sectionStart[MAXZONES][MAXSECTIONS];    // start pixel of each section in each zone
int sectionEnd[MAXZONES][MAXSECTIONS];      // last pixel of each section in each zone
char zoneLEDType[MAXZONES][10];             // LED type for each zone (WS2801, WS2811, WS2812, ...)

// This is an array of leds.  One item for each led in your strip.
CRGB *leds[MAXZONES];

// EEPROM layout:
// [0-2]   esp ................ verification string
// [3-5]   999 ................ Domoticz idx
// [6]     9   ................ number of used zones
// [7-12]  XXXXXX ............. zone 1 LED type (WS2801, WS2811, WS2812)
// [13-15] 999 ................ number of LEDs in zone
// [16-18] 999 ................ start of section 1
// [19-21] 999 ................ start of section 2
// [22-24] 999 ................ start of section 3
// [25-27] 999 ................ start of section 4
// [28-30] 999 ................ start of section 5
// [31-33] 999 ................ start of section 6
// [34-36] 999 ................ start of section 7
// [37-39] 999 ................ start of section 8
// [40-42] 999 ................ start of section 9
// [43-48] XXXXXX ............. zone 2 LED type
// ....
#define EEPROM_SIZE (7+(6+3+3*MAXSECTIONS)*MAXZONES)   // limit is 512 bytes

// global variables

int selectedEffect = -1;
int gHue = 0;
// Effect's speed should be between 30 FPS and 60 FPS, depending on length (density) of LED strip
// <51 pixels -> 30 FPS
// 51-100 pixels -> 45 FPS
// 101-150 pixels -> 60 FPS
// 151-200 pixels -> 75FPS
// ...

// Update these with values suitable for your network.
char mqtt_server[40] = "";
char mqtt_port[7]    = "1883";
char username[33]    = "";
char password[33]    = "";

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

//-----------------------------------------------------------
// main setup
void setup() {
  char str[EEPROM_SIZE+1];
  char tmp[20];

  Serial.begin(115200);
  delay(3000);

  String WiFiMAC = WiFi.macAddress();
  WiFiMAC.replace(":","");
  WiFiMAC.toCharArray(mac_address, 16);
  // Create client ID from MAC address
  sprintf(clientId, "led-%s", &mac_address[6]);
  WiFi.hostname(clientId);
  WiFi.mode(WIFI_STA);

  #if DEBUG
  Serial.println("");
  Serial.print("Host name: ");
  Serial.println(WiFi.hostname());
  Serial.print("MAC address: ");
  Serial.println(mac_address);
  #endif

  // request 20 bytes from EEPROM & write LED info
  EEPROM.begin(EEPROM_SIZE);

  #if DEBUG
  Serial.println("");
  Serial.print("EEPROM data: ");
  #endif
  for ( int i=0; i<EEPROM_SIZE; i++ ) {
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

  // read data from EEPROM
  if ( strncmp(str,"esp",3)==0 ) {
    strncpy(tmp, &str[3], 3);
    sprintf(c_idx, "%d", atoi(tmp)); 
    numZones = str[6] - '0';
  } else {
    strcpy(c_idx, "0");
    numZones=0;
  }

  #if DEBUG
  Serial.print("idx: ");
  Serial.println(c_idx);
  Serial.print("# of zones: ");
  Serial.println(numZones, DEC);
  #endif

  for ( int i=0; i<numZones; i++ ) {
    char tmp[10];
    strncpy(zoneLEDType[i], &str[7+(6+3+3*MAXSECTIONS)*i], 6);  // LED type for zone
    zoneLEDType[i][7] = '\0';
    strncpy(tmp, &str[7+(6+3+3*MAXSECTIONS)*i + 6], 3);
    tmp[4] = '\0';
    numLEDs[i] = atoi(tmp);       // number of LEDs in zone

    #if DEBUG
    Serial.print("Zone ");
    Serial.print(i, DEC);
    Serial.print(" LED type: ");
    Serial.print(zoneLEDType[i]);
    Serial.print(" and # of LEDs: ");
    Serial.println(numLEDs[i],DEC);
    #endif

    // get starting pixels for each section (0=empty section (except for first))
    numSections[i] = 0;
    for ( int j=0; j<MAXSECTIONS; j++ ) {
      strncpy(tmp, &str[7+(6+3+3*MAXSECTIONS)*i + 6 + 3 + 3*j], 3);
      tmp[4] = '\0';
      sectionStart[i][j] = atoi(tmp);
      sectionEnd[i][j] = numLEDs[i];  // will be overwritten later

      if ( sectionStart[i][j] != 0 || j==0 ) {
        numSections[i]++;
        if ( j>0 )
          sectionEnd[i][j-1] = sectionStart[i][j];
      }
    }
    #if DEBUG
    for ( int j=0; j<numSections[i]; j++ ) {
      Serial.print("Zone: ");
      Serial.print(i, DEC);
      Serial.print(" section: ");
      Serial.print(j, DEC);
      Serial.print(" start: ");
      Serial.print(sectionStart[i][j], DEC);
      Serial.print(" end: ");
      Serial.println(sectionEnd[i][j], DEC);
    }
    #endif
  }

//----------------------------------------------------------
  //clean FS, for testing
  //SPIFFS.format();

  #if DEBUG
  Serial.println("Starting WiFi manager");
  #endif

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
          Serial.println("");
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

  strcpy(c_idx, custom_idx.getValue());

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

    // write idx info to EEPROM
    sprintf(str, "esp%3s0", c_idx);
    for ( int i=0; i<strlen(str); i++ ) {
      EEPROM.write(i, str[i]);
    }
    EEPROM.commit();
    delay(250);

  }
//----------------------------------------------------------

  // done with EEPROM
  EEPROM.end();

  // OTA update setup
  //ArduinoOTA.setPort(8266);
  ArduinoOTA.setHostname(clientId);
  //ArduinoOTA.setPassword("ota_password");
  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
    #if DEBUG
    Serial.println("Start updating " + type);
    #endif
  });
  ArduinoOTA.onEnd([]() {
    #if DEBUG
    Serial.println("\nEnd");
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #if DEBUG
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #if DEBUG
    Serial.printf("Error[%u]: ", error);
    if      (error == OTA_AUTH_ERROR)    Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)   Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)     Serial.println("End Failed");
    #endif
  });
  ArduinoOTA.begin();

  // LED configuration set-up
  for ( int i=0; i<numZones; i++ ) {
    
    // allocate memory for LED array
    leds[i] = (CRGB*)calloc(numLEDs[i], sizeof(CRGB));

    // allocate Fire effect heat data
    heat[i] = (byte*)calloc(numLEDs[i], sizeof(byte));  // Fire effect static data buffer

    // Initialize FastLED library
    // Since FastLED uses C++ templates, we can't use variables for pin values:
    // const int dataPINs[] = {2, 4, 12, 15};  // D4, D2, D6, D8 (may also use TX for 5th zone)
    // const int clockPINs[] = {0, 5, 14, 13}; // D3, D1, D5, D7; for WS2801 type (may also use RX for 5th zone)
    // FastLED.addLeds<WS2812, dataPINs[i], GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
    
    if ( strcmp(zoneLEDType[i],"WS2801")==0 ) {
      #if DEBUG
      Serial.println("Adding WS2801 strip");
      #endif
      switch (i) {
        case 0:
          FastLED.addLeds<WS2801, 2, 0, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 1:
          FastLED.addLeds<WS2801, 4, 5, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 2:
          FastLED.addLeds<WS2801, 12, 14, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 3:
          FastLED.addLeds<WS2801, 15, 13, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
      }
    } else if ( strcmp(zoneLEDType[i],"WS2811")==0 ) {
      #if DEBUG
      Serial.println("Adding WS2811 strip");
      #endif
      switch (i) {
        case 0:
          FastLED.addLeds<WS2811, 0, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 1:
          FastLED.addLeds<WS2811, 4, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 2:
          FastLED.addLeds<WS2811, 12, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 3:
          FastLED.addLeds<WS2811, 15, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
/*
        // use the following for zones 5-8
        case 4:
          FastLED.addLeds<WS2811, 2, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 5:
          FastLED.addLeds<WS2811, 5, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 6:
          FastLED.addLeds<WS2811, 14, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 7:
          FastLED.addLeds<WS2811, 13, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
*/
      }
    } else if ( strcmp(zoneLEDType[i],"WS2812")==0 ) {
      #if DEBUG
      Serial.println("Adding WS2812 strip");
      #endif
      switch (i) {
        case 0:
          FastLED.addLeds<WS2812, 0, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 1:
          FastLED.addLeds<WS2812, 4, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 2:
          FastLED.addLeds<WS2812, 12, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 3:
          FastLED.addLeds<WS2812, 15, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
/*
        // use the following for zones 5-8
        case 4:
          FastLED.addLeds<WS2812, 2, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 5:
          FastLED.addLeds<WS2812, 5, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 6:
          FastLED.addLeds<WS2812, 14, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 7:
          FastLED.addLeds<WS2812, 13, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
*/
      }
    }
  }

  randomSeed(millis());

  // initialize MQTT connection & provide callback function
  sprintf(outTopic, "%s/%s", MQTTBASE, clientId);
      
  client.setServer(mqtt_server, atoi(mqtt_port));
  client.setCallback(mqtt_callback);

}

void loop() {
  CRGB rRGB;

  // get the pixel color from hue
  gHue &= 0xFF;
  rRGB = CHSV(gHue,255,255);

  // handle OTA updates
  ArduinoOTA.handle();

  // handle MQTT reconnects
  if (!client.connected()) {
    mqtt_reconnect();
  }
  // MQTT message processing
  client.loop();

  selectedEffect = max(min(selectedEffect,24),0); // sanity check
  switch ( selectedEffect ) {

    case 0 : {
              for ( int z=0; z<numZones; z++ ) {
                fadeToBlackBy(leds[z], numLEDs[z], 32);
              }
              showStrip();
              delay(100);
              break;
              }

    case 1  : {
              RGBLoop();
              break;
              }

    case 2  : {
              FadeInOut();
              break;
              }
              
    case 3  : {
              Strobe(CRGB::White);
              break;
              }

    case 4  : {
              HalloweenEyes(CRGB::Red, 1, 1, false);
              break;
              }
              
    case 5  : {
              CylonBounce(4, 10); // 100% / 1000 ms
              break;
              }
              
    case 6  : {
              NewKITT(6, 8);
              break;
              }
              
    case 7  : {
              Twinkle(100, false);
              break;
              }
              
    case 8  : { 
              TwinkleRandom(100, false);
              break;
              }
              
    case 9  : {
              Sparkle(50);
              break;
              }
               
    case 10 : {
              snowSparkle(50, random(50,200));
              break;
              }
              
    case 11 : {
              runningLights(200); /* produces a lot of flickering */
              break;
              }
              
    case 12 : {
              colorWipe(10);
              break;
              }

    case 13 : {
              rainbowCycle(200); /* produces a lot of flickering */
              break;
              }

    case 14 : {
              theaterChase(rRGB);
              gHue++;
              break;
              }

    case 15 : {
              rainbowChase();
              break;
              }

    case 16 : {
              // Fire - Cooling rate, Sparking rate, speed delay (1000/FPS)
              Fire(55, 120, 30);
              break;
              }

    case 17 : {
              // simple bouncingBalls not included, since BouncingColoredBalls can perform this as well as shown below
              bouncingColoredBalls(1, &rRGB);
              gHue += 8;
              break;
              }

    case 18 : {
              // multiple colored balls
              CRGB colors[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
              bouncingColoredBalls(3, colors);
              break;
              }

    case 19 : {
              meteorRain(4, 64, true, 20);
              break;
              }

    case 20 : {
              sinelon(10);
              break;
              }

    case 21 : {
              bpm(10);
              break;
              }

    case 22 : {
              juggle(10);
              break;
              }

    case 23 : {
              CRGB colors[3] = { rRGB, CHSV((gHue+128)&0xFF,255,255), CRGB::Black };
              colorChase(colors, 4, 200, true);
              gHue++;
              break;
              }

    case 24 : {
              christmasChase(4);
              break;
              }

  }
  breakEffect = false;

}

// Apply LED color changes & allow other tasks (MQTT callback, ...)
void showStrip() {
//  yield();    // allow other tasks
  if ( client.connected() )
    client.loop(); //check MQTT
  
  // FastLED
  FastLED.show();
}

//---------------------------------------------------
// MQTT callback function
void mqtt_callback(char* topic, byte* payload, unsigned int length) {
  char tmp[EEPROM_SIZE];
  
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
      selectedEffect = (actionId/10);
      breakEffect = true;
  
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
    
    if ( strstr(topic,"/command/idx") ) {
      
      sprintf(c_idx,"%3d",max(min((int)newPayload.toInt(),999),1));
      #if DEBUG
      Serial.print("New idx: ");
      Serial.println(c_idx);
      #endif
      
      // store configuration to EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for ( int i=0; i<3; i++ ) {
        EEPROM.write(i+3, c_idx[i]);
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);
  
    } else if ( strstr(topic,"/command/effect") ) {
      
      selectedEffect = (int)newPayload.toInt();
      breakEffect = true;
      
      #if DEBUG
      Serial.print("New effect: ");
      Serial.println(selectedEffect, DEC);
      #endif
  
    } else if ( strstr(topic,"/command/zones") ) {

      int lNumZones = max(min((int)newPayload.toInt(),8),1);
      #if DEBUG
      Serial.print("New # of zones: ");
      Serial.println(lNumZones, DEC);
      #endif
      
      // store configuration to EEPROM
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.write(6, lNumZones+'0');
      EEPROM.commit();
      EEPROM.end();
      delay(250);
      
    } else if ( strstr(topic,"/command/ledtype/") ) {

      int zone = (int)(topic[strlen(topic)-1] - '0');
      if ( zone < 0 || zone > MAXZONES-1 )
        return;

      newPayload.toCharArray(tmp, 7);

      #if DEBUG
      Serial.print("New LED type for zone ");
      Serial.print(zone, DEC);
      Serial.print(": ");
      Serial.println(tmp);
      #endif

      // store configuration to EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for ( int i=0; i<6; i++ ) {
        EEPROM.write(i + 7+(6+3+3*MAXSECTIONS)*zone, tmp[i]);
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);
  
    } else if ( strstr(topic,"/command/leds/") ) {

      int zone = (int)(topic[strlen(topic)-1] - '0');
      if ( zone < 0 || zone > numZones-1 )
        return;

      int lNumLEDs = max(min((int)newPayload.toInt(),999),1);
      #if DEBUG
      Serial.print("New # of LEDs for zone ");
      Serial.print(zone, DEC);
      Serial.print(": ");
      Serial.println(lNumLEDs, DEC);
      #endif
      
      // store configuration to EEPROM
      sprintf(tmp, "%3d", lNumLEDs);
      EEPROM.begin(EEPROM_SIZE);
      for ( int i=0; i<3; i++ ) {
        EEPROM.write(i + 6 + 7+(6+3+3*MAXSECTIONS)*zone, tmp[i]);
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);
      
    } else if ( strstr(topic,"/command/sections/") ) {

      int zone = (int)(topic[strlen(topic)-1] - '0');
      if ( zone < 0 || zone > MAXZONES-1 )
        return;

      int lSectionStart[MAXSECTIONS];
      int sections = 0;
      char *buf = strtok((char*)payload, ",;");
      while ( buf != NULL ) {
        lSectionStart[sections++] = atoi(buf);
        buf = strtok(NULL, ",;");

        #if DEBUG
        Serial.print("New section for zone ");
        Serial.print(zone, DEC);
        Serial.print(": ");
        Serial.println(lSectionStart[sections-1]);
        #endif
      }

      // store configuration to EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for ( int s=0; s<MAXSECTIONS; s++ ) {
        sprintf(tmp, "%3d", s>=sections ? 0 : lSectionStart[s]);
        for ( int i=0; i<3; i++ ) {
          EEPROM.write(i + 6 + 3 + 3*s + 7+(6+3+3*MAXSECTIONS)*zone, tmp[i]);
        }
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);
  
    } else if ( strstr(topic,"/command/restart") ) {

      #if DEBUG
      Serial.println("Restarting...");
      #endif
      
      // restart ESP
      ESP.reset();
      delay(2000);
      
    } else if ( strstr(topic,"/command/reset") ) {

      #if DEBUG
      Serial.println("Factory reset...");
      #endif
      
      // erase EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for ( int i=0; i<EEPROM_SIZE; i++ ) {
        EEPROM.write(i, '\0');
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);  // wait for write to complete
      #if DEBUG
      Serial.println("Erased EEPROM");
      #endif
  
      // clean FS
      SPIFFS.format();
      #if DEBUG
      Serial.println("SPIFFS formatted");
      #endif
  
      // clear WiFi data & disconnect
      WiFi.disconnect();
      #if DEBUG
      Serial.println("WiFi disconnected");
      #endif
      
      // restart ESP
      ESP.reset();
      delay(2000);

    }
  }
}

//----------------------------------------------------
// MQTT reconnect handling
void mqtt_reconnect() {
  char tmp[64];

  #if DEBUG
  Serial.print("Reconnecting.");
  #endif

  // Loop until we're reconnected
  while ( !client.connected() ) {
    // Attempt to connect
    if ( strlen(username)==0? client.connect(clientId): client.connect(clientId, username, password) ) {
      
      // Once connected, publish an announcement...
      DynamicJsonDocument doc(256);
      doc["mac"] = WiFi.macAddress(); //.toString().c_str();
      doc["ip"] = WiFi.localIP().toString();  //.c_str();
      doc["idx"] = c_idx;
      doc["zones"] = numZones;

      size_t n = serializeJson(doc, msg);
      sprintf(tmp, "%s/announce", outTopic);
      client.publish(tmp, msg, true);  // retain the announcement
      
      // ... and resubscribe
      sprintf(tmp, "%s/%s/command/#", MQTTBASE, clientId);
      client.subscribe(tmp);
      client.subscribe("domoticz/out");
      #if DEBUG
      Serial.println("\r\nConnected & subscribed.");
      #endif
    } else {
      #if DEBUG
      Serial.print(".");
      #endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// reverses a string 'str' of length 'len' 
void reverse(char *str, int len) 
{ 
    int i=0, j=len-1, temp; 
    while (i<j) {
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
    while (x) {
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
