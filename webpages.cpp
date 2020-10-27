// global includes (libraries)

#include <stdint.h>
#include <pgmspace.h>

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <EEPROM.h>
#include <FastLED.h>
#include <PubSubClient.h>         // MQTT client


#include "eepromdata.h"
#include "webpages.h"
#include "effects.h"
#include "boblight.h"


extern char c_idx[];
extern char zoneLEDType[][7];
extern uint16_t numLEDs[];

extern effects_t selectedEffect;
void changeEffect(effects_t effect);

static const char HTML_HEAD[] PROGMEM = "<html>\n<head>\n<title>Configure LED strips</title>\n<style>body {background-color:#cccccc;font-family:Arial,Helvetica,Sans-Serif;Color:#000088;}</style>\n</head>\n<body>\n";
static const char HTML_FOOT[] PROGMEM = "</body>\n</html>";
static const char _WS2801[] PROGMEM = "WS2801";
static const char _WS2811[] PROGMEM = "WS2811";
static const char _WS2812[] PROGMEM = "WS2812";
static const char _SELECTED[] PROGMEM = " selected";
static const char _LED_OPTION[] PROGMEM = "<option value=\"%S\"%S>%S</option>\n";      // %S for PROGMEM strings, %s for regular
static const char _EFFECT_OPTION[] PROGMEM = "<option value=\"%d\"%S>%s</option>\n";   // %S for PROGMEM strings, %s for regular

void handleRoot() {
  char tmp[64];
  String sections, postForm = FPSTR(HTML_HEAD);
  
  postForm += F("<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/\">\n<table>\n");
  if (server.method() == HTTP_POST) {
    if ( server.args() == 0 ) {
      postForm += F("<tr><td colspan=\"2\">BAD ARGUMENT</td></tr>\n");
    }

    changeEffect((effects_t) server.arg("effect").toInt());

  } else {
 
    if ( server.args() > 0 ) {
      if ( server.arg("e") == "" ) {     //Parameter not found
      } else {     //Parameter found

        changeEffect((effects_t) server.arg("e").toInt());

      }
    }

  }
  
  postForm += F("<tr><td>Effect:</td><td><select name=\"effect\" size=\"1\" onchange=\"this.form.submit();\">\n");
  
  for ( int i=0; i<=LAST_EFFECT; i++ ) {
    char buffer[64];
    snprintf_P(buffer, 64, _EFFECT_OPTION, i, (i==selectedEffect ? _SELECTED : PSTR("")), effects[i].name);
    postForm += buffer;
  }
  postForm += F("</select></td></tr>\n");
  postForm += F("<tr><td colspan=\"2\" align=\"center\"><input type=\"submit\" value=\"Submit\"></td></tr>\n</table>\n</form>\n");
  postForm += F("<a href=\"/set/\">Configure LED strips</a><br>\n");
  postForm += F("<a href=\"/bob/\">Configure Boblight</a><br>\n");
  postForm += FPSTR(HTML_FOOT);
  server.send(200, "text/html", postForm);
}

void handleSet() {
  char buffer[64];
  
  if (server.method() != HTTP_POST) {

    String sections, postForm = FPSTR(HTML_HEAD);
    postForm += F("<body>\n"
        "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/set/\">\n"
        "<table>\n"
        "<tr><td>Domoticz IDX:</td><td><input type=\"text\" name=\"idx\" value=\"");
    postForm += String(atoi(c_idx));
    postForm += F("\"></td></tr>\n");
      
    for ( int i=0; i<MAXZONES; i++ ) {
      sections = "0";
      for ( int j=1; j<numSections[i]; j++ ) {
        sections += "," + String(sectionStart[i][j]);
      }
      postForm += F("<tr><td colspan=\"2\" align=\"center\">Zone ");
      postForm += String(i);
      postForm += F(":</td></tr>\n");
      postForm += F("<tr><td># of LEDs:</td><td><input type=\"text\" name=\"leds");
      postForm += String(i);
      postForm += F("\" value=\"");
      postForm += String(numLEDs[i]);
      postForm += F("\"></td></tr>\n");
      if ( i == 0 ) {
        postForm += F("<tr><td>LED type (WS28xx):</td><td><select name=\"ledtype");
        postForm += String(i);
        postForm += F("\" size=\"1\">\n");
        snprintf_P(buffer, 64, _LED_OPTION, _WS2812, (strncmp_P(zoneLEDType[i], _WS2812, 6)==0 ? _SELECTED : PSTR("")), _WS2812);
        postForm += buffer;
        snprintf_P(buffer, 64, _LED_OPTION, _WS2811, (strncmp_P(zoneLEDType[i], _WS2811, 6)==0 ? _SELECTED : PSTR("")), _WS2811);
        postForm += buffer;
        snprintf_P(buffer, 64, _LED_OPTION, _WS2801, (strncmp_P(zoneLEDType[i], _WS2801, 6)==0 ? _SELECTED : PSTR("")), _WS2801);
        postForm += buffer;
        postForm += F("</select></td></tr>\n");
      } else if ( i == 1 ) {
        postForm += F("<tr><td>LED type (WS28xx):</td><td><select name=\"ledtype");
        postForm += String(i);
        postForm += F("\" size=\"1\">\n");
        snprintf_P(buffer, 64, _LED_OPTION, _WS2812, (strncmp_P(zoneLEDType[i], _WS2812, 6)==0 ? _SELECTED : PSTR("")), _WS2812);
        postForm += buffer;
        snprintf_P(buffer, 64, _LED_OPTION, _WS2811, (strncmp_P(zoneLEDType[i], _WS2811, 6)==0 ? _SELECTED : PSTR("")), _WS2811);
        postForm += buffer;
        postForm += F("</select></td></tr>\n");
      } else {
        postForm += F("<tr><td>LED type:</td><td><input type=\"text\" name=\"ledtype");
        postForm += String(i);
        postForm += F("\" value=\"WS2812\" readonly></td></tr>\n");
      }
      postForm += F("<tr><td>LED sections:</td><td><input type=\"text\" name=\"sections");
      postForm += String(i);
      postForm += F("\" value=\"");
      postForm += sections;
      postForm += F("\"></td></tr>\n");
    }
  
    postForm += F("<tr><td colspan=\"2\" align=\"center\"><input type=\"submit\" value=\"Submit\"></td></tr>\n</table>\n</form>\n</body>\n");
    postForm += FPSTR(HTML_FOOT);
    server.send(200, "text/html", postForm);

  } else {
    if (server.args() == 0) {
      return server.send(500, "text/plain", "BAD ARGS");
    }

    int zones;
    char tnum[4];
    memset(&e, 0, sizeof(e));
    memcpy_P(e.esp, PSTR("esp"), 3);
  
    String message = F("<html>\n<head>\n<meta http-equiv='refresh' content='15; url=/' />\n<title>ESP8266 settings applied</title>\n</head>\n");
    message += F("<body>\nSettings applied.<br>\n");

    for ( uint8_t i = 0; i < server.args(); i++ ) {
      String argN = server.argName(i);
      String argV = server.arg(i);

      if ( argN != "plain" ) {
        message += " " + argN + ": " + argV + "<br>\n";
      } else {
        continue;
      }
      
      if ( argN == "idx" ) {
        int idx = max(min((int)argV.toInt(),999),0);
        sprintf_P(c_idx, PSTR("%3d"), idx);
        memcpy(e.idx, c_idx, 3);
        #if DEBUG
        Serial.print("idx: ");
        Serial.println(c_idx);
        #endif
      }
      
      if ( argN.substring(0,4) == "leds" ) {
        int z = atoi(argN.substring(4).c_str());
        int l = max(min((int)argV.toInt(),999),0);
        sprintf_P(tnum, PSTR("%3d"),l);
        memcpy(e.zoneData[z].zoneLEDs, tnum, 3);
        #if DEBUG
        Serial.print(z, DEC);
        Serial.print("leds: ");
        Serial.println(tnum);
        #endif
      }
      
      if ( argN.substring(0,7) == "ledtype" ) {
        int z = atoi(argN.substring(7).c_str());
        memcpy(e.zoneData[z].ledType, argV.c_str(), 6);
        #if DEBUG
        Serial.print("LED type: ");
        Serial.println(argV);
        #endif
      }
      
      if ( argN.substring(0,8) == "sections" ) {
        int z = atoi(argN.substring(8).c_str());
        char tmp[64];
        argV.toCharArray(tmp, 64);
        char *buf = strtok(tmp, ",;");
        int sections=0;
        while ( buf != NULL && sections<MAXSECTIONS ) {
          sprintf_P(tnum, PSTR("%3d"), max(min(atoi(buf),999),0));
          memcpy(e.zoneData[z].sectionStart[sections++], tnum, 3);
          buf = strtok(NULL, ",;");
        }
        #if DEBUG
        Serial.print("Section: ");
        Serial.println(argV);
        #endif
      }
    }
    message += F("<br>ESP is restarting, please wait.<br>\n");
    message += F("</body>\n</html>");
    server.send(200, "text/html", message);

    for ( zones=1; zones<MAXZONES; zones++ ) {
      memcpy(tnum, e.zoneData[zones].zoneLEDs, 3);
      tnum[4] = '\0';
      if ( atoi(tnum) == 0 )
        break;
    }
    e.nZones = (char)zones + '0';
  
    #if DEBUG
    Serial.print("Zones: ");
    Serial.println(zones, DEC);
    
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

    // restart ESP
    ESP.reset();
    delay(2000);

  }
}

void handleBob() {
  char buffer[128];
  if (server.method() != HTTP_POST)
  {

    int b=0,r=0,t=0,l=0;
    float pct=0.0;
    String postForm = F(
      "<html>\n"
      "<head>\n"
        "<title>Configure Boblight</title>\n"
        "<style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n"
      "</head>\n"
      "<body>\n"
      "<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/bob/\">\n"
      "<table>\n"
      );
  
    for ( int i=0; i<numLights; i++ )
    {
      switch ( lights[i].lightname[0] )
      {
        case 'b' : b++; if (!pct) pct = lights[i].vscan[1] - lights[i].vscan[0]; break;
        case 'l' : l++; if (!pct) pct = lights[i].hscan[1] - lights[i].hscan[0]; break;
        case 't' : t++; if (!pct) pct = lights[i].vscan[1] - lights[i].vscan[0]; break;
        case 'r' : r++; if (!pct) pct = lights[i].hscan[1] - lights[i].hscan[0]; break;
      }
    }
    snprintf_P(buffer, 127, PSTR("<tr><td>Bottom LEDs:</td><td><input type=\"text\" name=\"bottom\" value=\"%d\"></td></tr>\n"), b);
    postForm += buffer;
    snprintf_P(buffer, 127, PSTR("<tr><td>Left LEDs:</td><td><input type=\"text\" name=\"left\" value=\"%d\"></td></tr>\n"), l);
    postForm += buffer;
    snprintf_P(buffer, 127, PSTR("<tr><td>Top LEDs:</td><td><input type=\"text\" name=\"top\" value=\"%d\"></td></tr>\n"), t);
    postForm += buffer;
    snprintf_P(buffer, 127, PSTR("<tr><td>Right LEDs:</td><td><input type=\"text\" name=\"right\" value=\"%d\"></td></tr>\n"), r);
    postForm += buffer;
    snprintf_P(buffer, 127, PSTR("<tr><td>Depth of scan (%%):</td><td><input type=\"text\" name=\"pct\" value=\"%.1f\"></td></tr>\n"), pct);
    postForm += buffer;
  
    postForm += F("<tr><td colspan=\"2\" align=\"center\"><input type=\"submit\" value=\"Submit\"></td></tr>\n</table>\n</form>\n</body>\n</html>");
      
    server.send(200, "text/html", postForm);

  }
  else
  {
    if (server.args() == 0)
    {
      return server.send(500, "text/plain", "BAD ARGS");
    }
  
    String message = F(
      "<html>\n"
      "<head>\n"
        "<meta http-equiv='refresh' content='10; url=/' />\n"
        "<title>Configure Boblight</title>\n"
      "</head>\n"
      "<body>\n"
      );

    for ( uint8_t i = 0; i < server.args(); i++ )
    {
      String argN = server.argName(i);
      String argV = server.arg(i);

      if ( argN != "plain" )
      {
        message += " " + argN + ": " + argV + "<br>\n";
      }
      else
      {
        continue;
      }
    }

    int bottom = server.arg("bottom").toInt();
    int left   = server.arg("left").toInt();
    int top    = server.arg("top").toInt();
    int right  = server.arg("right").toInt();
    float pct  = server.arg("pct").toFloat();

    if ( bottom+left+top+right > MAX_LEDS || bottom+left+top+right > numLEDs[bobStrip] )
    {
      message += F("Too many LEDs specified. Try lower values.<br>\n");
      message += F("</body>\n</html>");
      server.send(200, "text/html", message);
    }
    else
    {
      message += F("Settings applied.<br>\n");
      //message += F("<br>ESP is restarting, please wait.<br>\n");
      message += F("</body>\n</html>");
      server.send(200, "text/html", message);

      fillBobLights(bottom, left, top, right, pct);
      saveBobConfig();
/*
      // restart ESP
      ESP.reset();
      delay(2000);
*/
    }

  }
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}
