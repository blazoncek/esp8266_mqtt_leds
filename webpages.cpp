// global includes (libraries)

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal
#include <EEPROM.h>

#include "eepromdata.h"
#include "webpages.h"

extern char c_idx[];

void handleRoot() {
  String sections, postForm = "<html>\n\<head>\n\
<title>Configure LED strips</title>\n\
<style>body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>\n\
</head>\n\
<body>\n\
<form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/set/\">\n\
<table>\n\
<tr><td>Domoticz IDX:</td><td><input type=\"text\" name=\"idx\" value=\"" + String(atoi(c_idx)) + "\"></td></tr>\n";
    
  for ( int i=0; i<MAXZONES; i++ ) {
    sections = "0";
    for ( int j=1; j<numSections[i]; j++ ) {
      sections += "," + String(sectionStart[i][j]);
    }
    postForm += "<tr><td colspan=\"2\" align=\"center\">Zone " + String(i) + ":</td></tr>\n";
    postForm += "<tr><td># of LEDs:</td><td><input type=\"text\" name=\"leds" + String(i) + "\" value=\""+ String(numLEDs[i]) +"\"></td></tr>\n";
    if ( i == 0 ) {
      postForm += "<tr><td>LED type (WS28xx):</td><td><select name=\"ledtype"+ String(i) +"\" size=\"1\">\n";
      postForm += "<option value=\"WS2812\"" + (strncmp_P(zoneLEDType[i], PSTR("WS2812"), 6)==0 ? String(" selected") : String("")) + ">WS2812</option>\n";
      postForm += "<option value=\"WS2811\"" + (strncmp_P(zoneLEDType[i], PSTR("WS2811"), 6)==0 ? String(" selected") : String("")) + ">WS2811</option>\n";
      postForm += "<option value=\"WS2801\"" + (strncmp_P(zoneLEDType[i], PSTR("WS2801"), 6)==0 ? String(" selected") : String("")) + ">WS2801</option>\n";
      postForm += "</select></td></tr>\n";
    } else if ( i == 1 ) {
      postForm += "<tr><td>LED type (WS28xx):</td><td><select name=\"ledtype"+ String(i) +"\" size=\"1\">\n";
      postForm += "<option value=\"WS2812\"" + (strncmp_P(zoneLEDType[i], PSTR("WS2812"), 6)==0 ? String(" selected") : String("")) + ">WS2812</option>\n";
      postForm += "<option value=\"WS2811\"" + (strncmp_P(zoneLEDType[i], PSTR("WS2811"), 6)==0 ? String(" selected") : String("")) + ">WS2811</option>\n";
      postForm += "</select></td></tr>\n";
    } else {
      postForm += "<tr><td>LED type:</td><td><input type=\"text\" name=\"ledtype" + String(i) + "\" value=\"WS2812\" readonly></td></tr>\n";
    }
    postForm += "<tr><td>LED sections:</td><td><input type=\"text\" name=\"sections" + String(i) + "\" value=\""+ sections +"\"></td></tr>\n";
  }

  postForm += "<tr><td colspan=\"2\" align=\"center\"><input type=\"submit\" value=\"Submit\"></td></tr>\n</table>\n</form>\n</body>\n</html>";
    
  server.send(200, "text/html", postForm);
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

void handleSet() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
  } else {
    if (server.args() == 0) {
      return server.send(500, "text/plain", "BAD ARGS");
    }

    int zones;
    char tnum[4];
    memset(&e, 0, sizeof(e));
    memcpy_P(e.esp, PSTR("esp"), 3);
  
    String message = "<html>\n\
  <head>\n\
    <meta http-equiv='refresh' content='15; url=/' />\n\
    <title>ESP8266 settings applied</title>\n\
  </head>\n\
  <body>\n\
    Settings applied.<br>\n";

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
        char tmp[32];
        argV.toCharArray(tmp, 32);
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
    message += "<br>ESP is restarting, please wait.<br>\n";
    message += "</body>\n</html>";
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
