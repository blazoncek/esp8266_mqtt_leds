/*
 ESP8266 MQTT client for driving WS28xx individually addressable LEDs

 Changing of effect is done via Domotiz MQTT API Slector call or via custom MQTT topic.

 (c) blaz@kristan-sp.si / 2019-11-16
*/

// global includes (libraries)

#define MQTT_MAX_PACKET_SIZE 1024 // does not work always!!! Please change library include (PubSubClient.h)

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <pgmspace.h>

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <ArduinoOTA.h>           // OTA updates
#include <EEPROM.h>
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <ESP8266HTTPUpdateServer.h>    // http OTA updates
#include <WiFiManager.h>          // https://github.com/tzapu/WiFiManager WiFi Configuration Magic

#include <PubSubClient.h>         // MQTT client

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#define FASTLED_INTERNAL //remove annoying pragma messages
//#define FASTLED_ALLOW_INTERRUPTS 0
#define FASTLED_INTERRUPT_RETRY_COUNT 0
#define FASTLED_ESP8266_RAW_PIN_ORDER
#include <FastLED.h>

// NOTE: FastLED is a very memory hungry library. Each call to FastLED.addLeds() consumes around 1k of
// IRAM memory which is limited to 32k. Also every static string gets stored to IRAM by default so we
// will use PROGMEM for static strings whenever possible to reduce IRAM pressure.
//
// Even though this sketch supports WS2801 pixels, they are limited only to zone 0, also since WS2811 are less
// frequently used (than WS2812) they are limited to zones 0 and 1.
// WS2812 pixels can use zones 0 to zone 5.
//
// WS2801 uses SPI pins on ESP (even though FastLED lacks HW support), GPIO12 (MISO) & GPIO14 (SCLK) and is so limited to ESP12E
// WS2811 uses pins GPIO2, 0, 4, 5; only zones 0 and 1 can be used on ESP01.
// WS2812 uses PINS GPIO2, 0, 4, 5; only zones 0 and 1 can be used on ESP01.

// NOTE: Be aware, that driving # of LEDs takes time. Due to ESP8266 serial nature (use ESP32 if you want paralell
// execution) only about 600 LED pixels can be driven with 30 FPS.
// If you go beyond 600 pixels you may notice some effects (fire,...) becoming slow. Using slow pixels (e.g. WS2801) doesn't
// help either.


// local includes

#include "eepromdata.h"
#include "effects.h"
#include "webpages.h"
#include "boblight.h"

// global variables

eeprom_data_t e;

uint8_t  numZones = 0;                        // number of zones (LED strips), each zone requires a GPIO pin (or two)
uint16_t numLEDs[MAXZONES];                   // number of pixels in each zone
uint8_t  numSections[MAXZONES];               // number of sections in each zone
uint16_t sectionStart[MAXZONES][MAXSECTIONS]; // start pixel of each section in each zone
uint16_t sectionEnd[MAXZONES][MAXSECTIONS];   // last pixel of each section in each zone
char     zoneLEDType[MAXZONES][7];            // LED type for each zone (WS2801, WS2811, WS2812, ...)

// This is an array of leds.  One item for each led in your strip.
CRGB *leds[MAXZONES];

uint8_t volatile gHue = 0;
uint8_t volatile gBrightness = 255;  // used for solid effect
CRGB gRGB = CRGB::White;    // used for solid effect

effects_t selectedEffect = OFF;

// Effect's speed should be between 30 FPS and 60 FPS, depending on length (density) of LED strip
// <51 pixels -> 30 FPS
// 51-100 pixels -> 45 FPS
// 101-150 pixels -> 60 FPS
// 151-200 pixels -> 75FPS
// ...

// Update these with values suitable for your network (or leave empty to use WiFiManager).
char mqtt_server[40] = "";
char mqtt_port[7]    = "1883";
char username[33]    = "";
char password[33]    = "";

char c_idx[4]        = "0";         // domoticz switch IDX (type Selector, values define type of LED effect)

// flag for saving data from WiFiManager
bool shouldSaveConfig = false;

char msg[256];
char outTopic[64];
char clientId[20];  // MQTT client ID
char MQTTBASE[16] = "LEDstrip";

WiFiClient espClient;
WiFiServer bob(BOB_PORT);
WiFiClient bobClient;
PubSubClient client(espClient);

// web server object
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

// private functions
void mqtt_callback(char*, byte*, unsigned int);
void mqtt_reconnect();
char *ftoa(float,char*,int d=2);
void saveConfigCallback();

// strings (to reduce IRAM pressure)
const char _USERNAME[] = "username";
const char _PASSWORD[] = "password";
const char _MQTTSVR[]  = "mqtt_server";
const char _MQTTPRT[]  = "mqtt_port";
const char _CONFIG[] PROGMEM = "/config.json";
const char _WS2801[] PROGMEM = "WS2801";
const char _WS2811[] PROGMEM = "WS2811";
const char _WS2812[] PROGMEM = "WS2812";
/*
const char [] PROGMEM = "";
*/

//-----------------------------------------------------------
// main setup
void setup() {
  char mac_address[16];
  char tmp[32];

  Serial.begin(115200);
  delay(3000);

  String WiFiMAC = WiFi.macAddress();
  WiFiMAC.replace(F(":"),F(""));
  WiFiMAC.toCharArray(mac_address, 16);
  
  // Create client ID from MAC address
  sprintf_P(clientId, PSTR("led-%s"), &mac_address[6]);
  WiFi.hostname(clientId);
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleepMode(WIFI_NONE_SLEEP);

  #if DEBUG
  Serial.println();
  Serial.print(F("Host name: "));
  Serial.println(WiFi.hostname());
  Serial.print(F("MAC address: "));
  Serial.println(mac_address);

  Serial.print(F("# of effects : "));
  Serial.println(LAST_EFFECT, DEC);
  #endif

  // request 20 bytes from EEPROM & write LED info
  EEPROM.begin(EEPROM_SIZE);

  #if DEBUG
  Serial.println();
  Serial.print(F("EEPROM data: "));
  #endif
  // read data from EEPROM
  for ( int i=0; i<EEPROM_SIZE; i++ ) {
    ((char*)&e)[i] = EEPROM.read(i);
    #if DEBUG
    Serial.print(((char*)&e)[i], HEX);
    Serial.print(F(":"));
    #endif
  }
  #if DEBUG
  Serial.print(F(" ("));
  Serial.print(((char*)&e));
  Serial.println(F(")"));
  #endif

  // verify data
  if ( strncmp_P((char*)&e, PSTR("esp"), 3)==0 ) {
    memcpy(tmp, e.idx, 3);
    tmp[4] = '\0';
    sprintf_P(c_idx, PSTR("%d"), atoi(tmp)); 
    numZones = ((char*)&e)[6] - '0';
    gBrightness = e.iBrightness;
  } else {
    strcpy_P(c_idx, PSTR("0"));
    numZones = 0;
    gBrightness = 255;
  }

  #if DEBUG
  Serial.print(F("idx: "));
  Serial.println(c_idx);
  Serial.print(F("# of zones: "));
  Serial.println(numZones, DEC);
  #endif

  for ( int i=0; i<numZones; i++ ) {
    char tmp[10];
    memcpy(zoneLEDType[i], e.zoneData[i].ledType, 6);  // LED type for zone
    zoneLEDType[i][7] = '\0';
    memcpy(tmp, e.zoneData[i].zoneLEDs, 3);
    tmp[4] = '\0';
    numLEDs[i] = max(1,atoi(tmp));       // number of LEDs in zone (min 1, otherwise FastLED crashes)

    #if DEBUG
    Serial.print(F("Zone "));
    Serial.print(i, DEC);
    Serial.print(F(" LED type: "));
    Serial.print(zoneLEDType[i]);
    Serial.print(F(" and # of LEDs: "));
    Serial.println(numLEDs[i],DEC);
    #endif

    // get starting pixels for each section (0=empty section (except for first))
    numSections[i] = 0;
    for ( int j=0; j<MAXSECTIONS; j++ ) {
      memcpy(tmp, e.zoneData[i].sectionStart[j], 3);
      tmp[4] = '\0';
      sectionStart[i][j] = atoi(tmp);
      sectionEnd[i][j] = numLEDs[i]-1;  // will be overwritten later

      if ( sectionStart[i][j] != 0 || j==0 ) {
        numSections[i]++;
        if ( j>0 )
          sectionEnd[i][j-1] = sectionStart[i][j]-1;  // because 1st section starts with 0
      }
    }
    #if DEBUG
    for ( int j=0; j<numSections[i]; j++ ) {
      Serial.print(F("Zone: "));
      Serial.print(i, DEC);
      Serial.print(F(" section: "));
      Serial.print(j, DEC);
      Serial.print(F(" start: "));
      Serial.print(sectionStart[i][j], DEC);
      Serial.print(F(" end: "));
      Serial.println(sectionEnd[i][j], DEC);
    }
    #endif
  }

//----------------------------------------------------------
  //clean FS, for testing
  //SPIFFS.format();

  #if DEBUG
  Serial.println(F("Starting WiFi manager"));
  #endif

  //read configuration from FS json
  if ( SPIFFS.begin() ) {
    #if DEBUG
    Serial.println(F("SPIFFS begin."));
    #endif

    if ( SPIFFS.exists(FPSTR(_CONFIG)) ) {
      //file exists, reading and loading
      #if DEBUG
      Serial.println(F("File found."));
      #endif
      
      File configFile = SPIFFS.open(FPSTR(_CONFIG), "r");
      if ( configFile ) {
        size_t size = configFile.size();
        
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);
        configFile.readBytes(buf.get(), size);
        
        DynamicJsonDocument doc(size);
        DeserializationError error = deserializeJson(doc, buf.get());
        if ( !error ) {
          strcpy(mqtt_server, doc[_MQTTSVR]);
          strcpy(mqtt_port, doc[_MQTTPRT]);
          strcpy(username, doc[_USERNAME]);
          strcpy(password, doc[_PASSWORD]);
          #if DEBUG 
          serializeJson(doc, Serial);
          Serial.println();
          #endif
        } else {
          #if DEBUG
          Serial.println(F("Error reading JSON from config file."));
          #endif
        }
      } else {
        #if DEBUG
        Serial.println(F("Config file does not exist."));
        #endif
      }
    }
  } else {
    #if DEBUG
    Serial.println(F("SPIFFS failed."));
    #endif
  }
  //end read

  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_mqtt_server("server", _MQTTSVR, mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port",     _MQTTPRT, mqtt_port, 5);
  WiFiManagerParameter custom_username(_USERNAME,  _USERNAME, username, 32);
  WiFiManagerParameter custom_password(_PASSWORD,  _PASSWORD, password, 32);

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

  //save the custom parameters to FS
  if ( shouldSaveConfig ) {
    saveMQTTConfig();
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
      type = F("sketch");
    } else { // U_FS
      type = F("filesystem");
    }
    #if DEBUG
    Serial.print(F("Start updating "));
    Serial.println(type);
    #endif
  });
  ArduinoOTA.onEnd([]() {
    #if DEBUG
    Serial.println(F("\nEnd"));
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #if DEBUG
    Serial.printf_P(PSTR("Progress: %u%%\r"), (progress / (total / 100)));
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #if DEBUG
    Serial.printf_P(PSTR("Error[%u]: "), error);
    if      (error == OTA_AUTH_ERROR)    Serial.println(F("Auth Failed"));
    else if (error == OTA_BEGIN_ERROR)   Serial.println(F("Begin Failed"));
    else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
    else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
    else if (error == OTA_END_ERROR)     Serial.println(F("End Failed"));
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
    // Since FastLED uses C++ templates, we can't use variables for pin values.
    // Currently not enough IRAM for more than 4 zones. :(
    // Each FastLED.addLeds() takes 1140 bytes of IRAM
    
    if ( strcmp_P(zoneLEDType[i], _WS2801)==0 && i==0 ) {
      // only allow WS2801 strip on zone 0 (due to memory constrains)
      #if DEBUG
      Serial.println(F("Adding WS2801 strip (on GPIO12, GPIO14)."));
      #endif
      FastLED.addLeds<WS2801, 12/*D6*/, 14/*D5*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
    } else if ( strcmp_P(zoneLEDType[i], _WS2811)==0 && i<2 ) {
      // only allow WS2811 strip on zone 0 and 1 (due to memory constrains)
      #if DEBUG
      Serial.println(F("Adding WS2811 strip."));
      #endif
      switch (i) {
        case 0:
          FastLED.addLeds<WS2811, 2/*D4*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 1:
          FastLED.addLeds<WS2811, 0/*D3*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 2:
          FastLED.addLeds<WS2811, 4/*D2*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 3:
          FastLED.addLeds<WS2811, 5/*D1*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
//        case 4:
//          FastLED.addLeds<WS2811, 15/*D8*/, RGB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
//          break;
      }
    } else if ( strcmp_P(zoneLEDType[i], _WS2812)==0 ) {
      #if DEBUG
      Serial.println(F("Adding WS2812 strip."));
      #endif
      switch (i) {
        case 0:
          FastLED.addLeds<WS2812, 2/*D4*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 1:
          FastLED.addLeds<WS2812, 0/*D3*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 2:
          FastLED.addLeds<WS2812, 4/*D2*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
        case 3:
          FastLED.addLeds<WS2812, 5/*D1*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
          break;
//        case 4:
//          FastLED.addLeds<WS2812, 15/*D8*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
//          break;
//        case 5:
//          FastLED.addLeds<WS2812, 13/*D7*/, GRB>(leds[i], numLEDs[i]).setCorrection(TypicalLEDStrip);
//          break;
      }
    }
  }

  randomSeed(millis());

  // initialize MQTT connection & provide callback function
  sprintf_P(outTopic, PSTR("%s/%s"), MQTTBASE, clientId);

  if ( strlen(mqtt_server) > 0 ) {
    client.setServer(mqtt_server, atoi(mqtt_port));
    client.setCallback(mqtt_callback);
  }

  // web server setup
  if (MDNS.begin(clientId)) {
    MDNS.addService("http", "tcp", 80);
    #if DEBUG
    Serial.println(F("MDNS responder started."));
    #endif
  }

  httpUpdater.setup(&server);

  server.on("/", handleRoot);
  server.on("/set", handleSet);
  server.on("/set/", handleSet);
  server.on("/bob", handleBob);
  server.on("/bob/", handleBob);
  server.onNotFound(handleNotFound);
  server.begin();

  // read boblight configuration from FS json
  if ( !loadBobConfig() || numLights == 0 ) {
    fillBobLights(16,9,16,9,10);  // 50 lights == 1 WS2811 string
    saveBobConfig();
  }
  
  bob.begin();
  bob.setNoDelay(true);
}

void loop() {

  // get the pixel color from hue
  //EVERY_N_MILLISECONDS( 50 ) { gHue++; }  // Slowly cycle through the rainbow
  gHue &= 0xFF;

  // handle OTA updates
  ArduinoOTA.handle();

  if ( strlen(mqtt_server) > 0 ) {
    // handle MQTT reconnects
    if (!client.connected()) {
      mqtt_reconnect();
    }
    // MQTT message processing
    client.loop();
  }

  // handle web server request
  server.handleClient();
  MDNS.update();


  pollBob();


  selectedEffect = (effects_t)max(min((int)selectedEffect,(int)LAST_EFFECT),(int)OFF); // sanity check
  switch ( selectedEffect ) {

    case OFF :
              for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
                fadeToBlackBy(leds[z], numLEDs[z], 32);
                FastLED[z].showLeds(gBrightness);
              }
              delay(50);
              break;

    case SOLID :
              for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
                FastLED[z].showColor(gRGB, gBrightness);
              }
              delay(50);
              break;

    case FADEINOUT :
              FadeInOut();
              break;
              
    case STROBE :
              Strobe(CRGB::White);
              break;

    case HALLOWEENEYES :
              HalloweenEyes(CRGB::Red, 1, 1, false);
              break;
              
    case CYLONBOUNCE :
              eyeBounce(CRGB::Red, 10, 30); // 10% size, 30/min
              break;
              
    case NEWKITT :
              NewKITT(5, 45); // 5% size, 45/min
              break;
              
    case TWINKLE :
              Twinkle(25, false); // 25ms delay, fade pixels
              gHue++;
              break;
              
    case TWINKLERANDOM :
              TwinkleRandom(25, false); // 25ms delay, fade pixels
              break;
              
    case SPARKLE :
              Sparkle(50);  // 50ms delay
              break;
               
    case SNOWSPARKLE :
              snowSparkle(50); // 50ms delay
              break;
              
    case RUNNINGLIGHTS :
              runningLights(30); // 30ms delay
              break;
              
    case COLORWIPE :
              colorWipe(30);  // 30/min
              break;

    case RAINBOWCYCLE :
              rainbowCycle(50);
              break;

    case THEATRECHASE :
              theaterChase(CHSV(gHue,255,255),100); // 100ms delay
              gHue++;
              break;

    case RAINBOWCHASE :
              rainbowChase(100);  // 100ms delay
              break;

    case FIRE :
              // Fire - Cooling rate, Sparking rate, speed delay (1000/FPS)
              Fire(50, 128, 15);
              break;

    case GRADIENTCYCLE :
              gradientCycle(50);  // 50ms delay
              break;

    case BOUNCINGCOLOREDBALLS :
              {
              // multiple colored balls
              CRGB colors[3] = { CRGB::Red, CRGB::Green, CRGB::Blue };
              bouncingColoredBalls(3, colors);
              break;
              }

    case METEORRAIN :
              meteorRain(8, 64, true, 10);  // 8% size, 64 decay, random decay, 10ms delay
              break;

    case SINELON :
              sinelon(15);
              break;

    case BPM :
              bpm(15);
              break;

    case JUGGLE :
              juggle(15);
              break;

    case COLORCHASE :
              {
              CRGB colors[3] = { CHSV(gHue,255,255), CHSV((gHue+128)&0xFF,255,255), CRGB::Black };
              colorChase(colors, 4, 25, true);  // 4 pixel size, 50ms delay, rotating if possible
              gHue++;
              break;
              }

    case CHRISTMASCHASE :
              christmasChase(4);  // 4 pixel size, default delay 50ms, not rotating
              break;

    case RAINBOWBOUNCE :
              eyeBounce(CRGB::Black, 25, 30);  // 25% size, 30/min
              break;
              
  }
  breakEffect = false;

  yield();
}

// Apply LED color changes & allow other tasks (MQTT callback, ...)
void showStrip() {
  yield();    // allow other tasks
//  if ( client.connected() )
//    client.loop(); //check MQTT
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    FastLED[z].showLeds(gBrightness);
  }
  //FastLED.show();
}

//---------------------------------------------------
// MQTT callback function
//------
// Possible MQTT topics with message are:
//   - domoticz/out                      [{idx=ID,svalue1="effect*10"}] - JSON struct
//   - MQTTBASE/clientID/command/restart []          - restart ESP
//   - MQTTBASE/clientID/command/reset   [1]         - factory reset (clear all settings)
//   - MQTTBASE/clientID/command/effect  [effectID]  - change effect
//   - MQTTBASE/clientID/set/effect      [effectID]  - change effect
//   - MQTTBASE/clientID/set/idx         [ID]        - Domoticz IDX
//   - MQTTBASE/clientID/set/zones       [n]         - # of zones (applied after restart)
//   - MQTTBASE/clientID/set/ledtype/X   [WS2801|WS2811|WS2812] - attached LED type to zone X (applied after restart)
//   - MQTTBASE/clientID/set/leds/X      [n]         - attached # of LEDs to zone X (applied after restart)
//   - MQTTBASE/clientID/set/sections/X  [a,b,c,...] - a,b,c,... = start # of 1st LED in each section of zone X (applied after restart)
//------
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

  // client
  sprintf_P(tmp, PSTR("%s/%s"), MQTTBASE, clientId);

  if ( strcmp_P(topic, PSTR("domoticz/out")) == 0 ) {

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    // if we have idx field equal to our idx
    if ( doc["idx"] == atoi(c_idx) ) {
      
      #if DEBUG
      Serial.print("JSON: ");
      serializeJson(doc, Serial);
      Serial.println();
      #endif

      // proper IDX found
      String action = doc["svalue1"];
      int actionId = action.toInt();  // global brightness or selected effect

      // do we have RGBW switch?
      if ( doc["Color"] != nullptr ) {  // can use doc["switchType"]=="Dimmer" && doc["dtype"]=="Color Switch"
        int r,g,b,w;
        
        #if DEBUG
        Serial.println(F("RGB(W) switch found."));
        #endif

        if ( doc["Color"]["m"] == 3 ) { // colour mode = RGB(W)
          
          r = doc["Color"]["r"];
          g = doc["Color"]["g"];
          b = doc["Color"]["b"];
          w = doc["Color"]["ww"]; // reserver for future
          //w = doc["Color"]["cw"];

          gRGB = CRGB(r,g,b);
          
          #if DEBUG
          Serial.println(F("Color found."));
          Serial.print(F("R: "));
          Serial.println(r,DEC);
          Serial.print(F("G: "));
          Serial.println(g,DEC);
          Serial.print(F("B: "));
          Serial.println(b,DEC);
          #endif

        } else if ( doc["Color"]["m"] == 1 ) { // colour mode = monochrome
          
          w = doc["Color"]["ww"];
          //w = doc["Color"]["cw"];

          gRGB = CRGB(w,w,w);
          
          #if DEBUG
          Serial.println(F("Monochrome found."));
          Serial.print(F("WW: "));
          Serial.println(w,DEC);
          #endif
        }

        gBrightness = doc["Level"];
        gBrightness = min(max((int)gBrightness,1),255);
        FastLED.setBrightness(gBrightness);

        #if DEBUG
        Serial.print(F("Brightness: "));
        Serial.println(gBrightness, DEC);
        #endif

        if ( gBrightness && doc["nvalue"]) { // doc["nvalue"]==0 -> OFF
          changeEffect(SOLID);  // solid color effect
        } else {
          changeEffect(OFF);    // off
        }
        
      } else {  // doc["switchType"]=="Selector" && doc["dtype"]=="Light/Switch"
        
        // selector switch -> apply new effect
        changeEffect((effects_t)(actionId/10));
        FastLED.setBrightness(gBrightness);

      }

    }

  } else if ( strstr_P(topic, PSTR("shellies/")) && strstr(topic, clientId) && strstr_P(topic, PSTR("/color/0/command")) ) {

    if ( newPayload == "on" ) {
      changeEffect(SOLID);  // solid color effect
    } else {
      changeEffect(OFF);    // off
    }
    
  } else if ( strstr_P(topic, PSTR("shellies/")) && strstr(topic, clientId) && strstr_P(topic, PSTR("/color/0/set")) ) {

    DynamicJsonDocument doc(2048);
    deserializeJson(doc, payload);

    if ( doc["turn"] == "on" ) {
      int r = doc["red"];
      int g = doc["green"];
      int b = doc["blue"];
      int w = doc["white"]; // reserver for future
  
      gRGB = CRGB(r,g,b);
  
      int gain = doc["gain"];
      gBrightness = (int) (gain/100.0 * 255);
      
      #if DEBUG
      Serial.println(F("Color found."));
      Serial.print(F("R: "));
      Serial.println(r,DEC);
      Serial.print(F("G: "));
      Serial.println(g,DEC);
      Serial.print(F("B: "));
      Serial.println(b,DEC);
      #endif

      changeEffect(SOLID);  // solid color effect
    }

    if ( doc["effect"] != 0 ) {
      int effect = doc["effect"];
      switch ( effect ) {
        case 1: selectedEffect = METEORRAIN;     break; // Meteor
        case 2: selectedEffect = RAINBOWCYCLE;   break; // rainbowCycle
        case 3: selectedEffect = FADEINOUT;      break; // fadeInOut
        case 4: selectedEffect = SPARKLE;        break; // sparkle
        case 5: selectedEffect = COLORWIPE;      break; // colorWipe
        case 6: selectedEffect = CHRISTMASCHASE; break; // christmasChase
      }
      //selectedEffect = max(min(doc["effect"]+1,2,24);
    }

    if ( doc["turn"] == "off" ) {
      changeEffect(OFF);
    }
    
  } else if ( strstr(topic, MQTTBASE) && strstr(topic, clientId) ) {
    
    if ( strstr_P(topic, PSTR("/set/idx")) ) {
      
      sprintf_P(c_idx, PSTR("%3d"), max(min((int)newPayload.toInt(),999),1));
      #if DEBUG
      Serial.print(F("New idx: "));
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
  
    } else if ( strstr_P(topic, PSTR("/set/effect")) || strstr_P(topic, PSTR("/command/effect")) ) {
      
      changeEffect((effects_t)newPayload.toInt());
  
    } else if ( strstr_P(topic, PSTR("/set/hue")) ) {
      
      gHue = min(max((int)newPayload.toInt(),0),255);
      
      #if DEBUG
      Serial.print(F("New hue: "));
      Serial.println(gHue, DEC);
      #endif
  
    } else if ( strstr_P(topic, PSTR("/set/brightness")) ) {
      
      gBrightness = min(max((int)newPayload.toInt(),0),255);
      
      #if DEBUG
      Serial.print(F("New brightness: "));
      Serial.println(gBrightness, DEC);
      #endif
  
    } else if ( strstr_P(topic, PSTR("/set/zones")) ) {

      String t, s;
      int l, zones = 0, sections = 0;
      char tnum[4];
      memset(&e, 0, sizeof(e));
      
      DynamicJsonDocument doc(2048);
      deserializeJson(doc, payload);

      #if DEBUG
      Serial.print("JSON: ");
      serializeJson(doc, Serial);
      Serial.println();
      #endif

      if ( doc["zones"] == nullptr )
        return;

      JsonArray arr = doc["zones"];
      for (JsonVariant a : arr) {

        if (  a["ledtype"] == nullptr || a["leds"] == nullptr || a["sections"] == nullptr )
          return;

        l = a["leds"];
        t = a["ledtype"].as<char*>();
        s = a["sections"].as<char*>();

        if ( (zones != 0 && t == "WS2801") || zones > 1 && t == "WS2811" ) {
          #if DEBUG
          Serial.print(F("Wrong LED type for zone "));
          Serial.println(zones, DEC);
          #endif
          return;
        }
        memcpy(e.zoneData[zones].ledType, t.c_str(), 6);
        #if DEBUG
        Serial.print(F("New LED type for zone "));
        Serial.print(zones, DEC);
        Serial.print(F(": "));
        Serial.println(t.c_str());
        #endif
  
        l = max(min(l,999),1);
        sprintf_P(tnum, PSTR("%3d"),l);
        memcpy(e.zoneData[zones].zoneLEDs, tnum, 3);
        #if DEBUG
        Serial.print(F("New # of LEDs for zone "));
        Serial.print(zones, DEC);
        Serial.print(F(": "));
        Serial.println(l, DEC);
        #endif
  
        char *buf = strtok((char*)s.c_str(), ",;");
        while ( buf != NULL && sections<9 ) {
          sprintf_P(tnum, PSTR("%3d"), atoi(buf));
          memcpy(e.zoneData[zones].sectionStart[sections++], tnum, 3);
  
          #if DEBUG
          Serial.print(F("New section for zone "));
          Serial.print(zones, DEC);
          Serial.print(F(": "));
          Serial.println(atoi(buf),DEC);
          #endif
          
          buf = strtok(NULL, ",;");
        }
        // memcpy_P(e.zoneData[zones].sectionStart[sections], PSTR("  0"), 3);
        sections = 0; // reset sections for next zone

        zones++;
      }
      
      e.nZones = (char)zones + '0';
      memcpy_P(e.esp, PSTR("esp"), 3);
      sprintf_P(tnum, PSTR("%3d"), atoi(c_idx));
      memcpy(e.idx, tnum, 3);

      #if DEBUG
      Serial.print(F("EEPROM data: "));
      for ( int i=0; i<sizeof(e); i++ ) {
        Serial.print(((char*)&e)[i], HEX);
        Serial.print(":");
      }
      Serial.println();
      #endif
      
      // store configuration to EEPROM
      EEPROM.begin(EEPROM_SIZE);
      for ( int i=0; i<sizeof(e); i++ ) {
        EEPROM.write(i, ((char*)&e)[i]);
      }
      EEPROM.commit();
      EEPROM.end();
      delay(250);

    } else if ( strstr_P(topic, PSTR("/command/restart")) ) {

      #if DEBUG
      Serial.println(F("Restarting..."));
      #endif
      
      // restart ESP
      ESP.reset();
      delay(2000);
      
    } else if ( strstr_P(topic, PSTR("/command/reset")) && (int)(payload[strlen((char*)payload)-1] - '0')==1 ) {

      #if DEBUG
      Serial.println(F("Factory reset..."));
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
      Serial.println(F("Erased EEPROM"));
      #endif
  
      // clean FS
      SPIFFS.format();
      #if DEBUG
      Serial.println(F("SPIFFS formatted"));
      #endif
  
      // clear WiFi data & disconnect
      WiFi.disconnect();
      #if DEBUG
      Serial.println(F("WiFi disconnected"));
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
  unsigned int deadCounter=0;

  #if DEBUG
  Serial.print(F("Reconnecting."));
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
      sprintf_P(tmp, PSTR("%s/announce"), outTopic);
      client.publish(tmp, msg, true);  // retain the announcement
      
      // ... and resubscribe
      sprintf_P(tmp, PSTR("%s/%s/command/#"), MQTTBASE, clientId);
      client.subscribe(tmp);
      sprintf_P(tmp, PSTR("%s/%s/set/#"), MQTTBASE, clientId);
      client.subscribe(tmp);
      client.subscribe("domoticz/out");
      #if DEBUG
      Serial.println(F("\r\nConnected & subscribed."));
      #endif
    } else {
      #if DEBUG
      Serial.print(".");
      #endif
      // Wait 5 seconds before retrying
      delay(5000);
      
      // try reconnecting for 10 minutes then reset WiFi credentials
      if ( deadCounter++ > 120 ) {
        // clear WiFi data & disconnect
        WiFi.disconnect();
        #if DEBUG
        Serial.println(F("WiFi disconnected"));
        #endif
        
        // restart ESP
        ESP.reset();
        delay(2000);
      }
    }
  }
}

//-------------------------------------------------
// private functions

// change current effect
void changeEffect(effects_t effect) {
  char tmp[64];

  selectedEffect = effect;
  gHue = 0;           // reset hue
  breakEffect = true; // interrupt current effect

  if ( client.connected() ) {
    // Publish effect state
    sprintf_P(tmp, PSTR("%s/effect"), outTopic);
    sprintf_P(msg, PSTR("%d"), (int)selectedEffect);
    client.publish(tmp, msg);
  }

  #if DEBUG
  Serial.print("New effect: ");
  Serial.println(selectedEffect, DEC);
  #endif
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

// MQTT save
void saveMQTTConfig()
{
  DynamicJsonDocument doc(1024);
  doc[_MQTTSVR] = mqtt_server;
  doc[_MQTTPRT] = mqtt_port;
  doc[_USERNAME] = username;
  doc[_PASSWORD] = password;

  File configFile = SPIFFS.open(FPSTR(_CONFIG), "w");
  if ( !configFile ) {
    // failed to open config file for writing
    #if DEBUG
    Serial.println(F("Error opening config file fo writing."));
    #endif
  } else {
    serializeJson(doc, configFile);
    configFile.close();
    #if DEBUG
    serializeJson(doc, Serial);
    #endif
  }
}

//callback notifying us of the need to save config
void saveConfigCallback ()
{
  shouldSaveConfig = true;
}

// calculate hue from RGB
CHSV *getHSVfromRGB(int r, int g, int b) {
  static CHSV hsv;

  float Cmax = max(r,max(g,b)) / 255.0;
  float Cmin = min(r,min(g,b)) / 255.0;
  float d = Cmax - Cmin;

  float sat = Cmax>0.0 ? d / Cmax : 0.0;  // saturation: 0..1
  float val = Cmax;                       // value: 0..1

  float hue;
  if ( d == 0.0 ) {
    hue = 0;
  } else if ( Cmax == r/255.0 ) {
    hue = 60 * ((int)((g-b)/d/255.0) % 6);
  } else if ( Cmax == g/255.0 ) {
    hue = 60 * (((b-r)/d/255.0) + 2);
  } else if ( Cmax == b/255.0 ) {
    hue = 60 * (((r-g)/d/255.0) + 4);
  } else {
    hue = 0;
  }

  int h = (hue / 360.0) * 255;
  int s = sat * 255;
  int v = val * 255;

  hsv = CHSV(h,s,v);
  return &hsv;
}
