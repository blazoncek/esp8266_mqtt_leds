// global includes (libraries)

#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ESP8266mDNS.h>          // multicast DNS
#include <WiFiUdp.h>              // UDP handling
#include <DNSServer.h>            // Local DNS Server used for redirecting all requests to the configuration portal
#include <ESP8266WebServer.h>     // Local WebServer used to serve the configuration portal


//#include <LittleFS.h>
FS* filesystem = &SPIFFS;
//FS* filesystem = &LittleFS;

File fsUploadFile;

extern ESP8266WebServer server;

const String postForms = "<html>\
  <head>\
    <title>Configure LED strips</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <form method=\"post\" enctype=\"application/x-www-form-urlencoded\" action=\"/set/\">\
    <table>\
    <tr><td>Domoticz IDX:</td><td><input type=\"text\" name=\'idx\' value=\'0\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 0:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds0\' value=\'50\'></td></tr>\
    <tr><td>LED type (WS28xx):</td><td><select name=\'ledtype0\' size=\'1\'>\
      <option value=\'WS2812\'>WS2812</option>\
      <option value=\'WS2811\'>WS2811</option>\
      <option value=\'WS2801\'>WS2801</option>\
      </select></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections0\' value=\'0,25\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 1:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds1\' value=\'0\'></td></tr>\
    <tr><td>LED type (WS28xx):</td><td><select name=\'ledtype1\' size=\'1\'>\
      <option value=\'WS2812\'>WS2812</option>\
      <option value=\'WS2811\'>WS2811</option>\
      </select></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections1\' value=\'\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 2:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds2\' value=\'0\'></td></tr>\
    <tr><td>LED type:</td><td><input type=\"text\" name=\'ledtype2\' value=\'WS2812\' readonly></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections2\' value=\'\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 3:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds3\' value=\'0\'></td></tr>\
    <tr><td>LED type:</td><td><input type=\"text\" name=\'ledtype3\' value=\'WS2812\' readonly></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections3\' value=\'\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 4:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds4\' value=\'0\'></td></tr>\
    <tr><td>LED type:</td><td><input type=\"text\" name=\'ledtype4\' value=\'WS2812\' readonly></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections4\' value=\'\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'>Zone 5:</td></tr>\
    <tr><td># of LEDs:</td><td><input type=\"text\" name=\'leds5\' value=\'0\'></td></tr>\
    <tr><td>LED type:</td><td><input type=\"text\" name=\'ledtype5\' value=\'WS2812\' readonly></td></tr>\
    <tr><td>LED sections:</td><td><input type=\"text\" name=\'sections5\' value=\'\'></td></tr>\
    <tr><td colspan=\'2\' align=\'center\'><input type=\"submit\" value=\"Submit\"></td></tr>\
    </table>\
    </form>\
  </body>\
</html>";

void handleRoot() {
  server.send(200, "text/html", postForms);
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
  
    String message = "POST form was:\n";
    for (uint8_t i = 0; i < server.args(); i++) {
      message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }
    server.send(200, "text/plain", message);

    for ( int i=0; i<server.args(); i++ ) {
      String path = server.arg(i);
      String var = server.argName(i);
      Serial.print(var);
      Serial.print("=");
      Serial.println(path);
    }
  }
}
