#include <stdint.h>
#include <math.h>
#include <Arduino.h>
#include <FS.h>                   //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson
#include <pgmspace.h>
#include <FastLED.h>


#include "eepromdata.h"
#include "boblight.h"


extern CRGB *leds[];
extern uint16_t numLEDs[];

uint8_t bobStrip = 0;             // Default: first zone
light_t lights[MAX_LEDS];
uint16_t numLights = 0;

// strings (to reduce IRAM pressure)
static const char _CONFIG[] PROGMEM = "/config.bob";

void fillBobLights(int bottom, int left, int top, int right, float pct_scan)
{
/*
# boblight
# Copyright (C) Bob  2009 
#
# makeboblight.sh created by Adam Boeglin <adamrb@gmail.com>
#
# boblight is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# boblight is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
# See the GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License along
# with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

  int lightcount = 0;
  int total = top+left+right+bottom;
  int bcount;

  if ( total > MAX_LEDS ) {
    #if DEBUG
    Serial.println(F("Too many lights."));
    #endif
    return;
  }
  
  if ( bottom > 0 )
  {
    bcount = 1;
    float brange = 100.0/bottom;
    float bcurrent = 50.0;
    if ( bottom < top )
    {
      int diff = top - bottom;
      brange = 100.0/top;
      bcurrent -= (diff/2)*brange;
    }
    while ( bcount <= bottom/2 ) 
    {
      float btop = bcurrent - brange;
      String name = "b"+String(bcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = btop;
      lights[lightcount].hscan[1] = bcurrent;
      lights[lightcount].vscan[0] = 100 - pct_scan;
      lights[lightcount].vscan[1] = 100;
      lightcount+=1;
      bcurrent = btop;
      bcount+=1;
    }
  }

  if ( left > 0 )
  {
    int lcount = 1;
    float lrange = 100.0/left;
    float lcurrent = 100.0;
    while (lcount <= left )
    {
      float ltop = lcurrent - lrange;
      String name = "l"+String(lcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = 0;
      lights[lightcount].hscan[1] = pct_scan;
      lights[lightcount].vscan[0] = ltop;
      lights[lightcount].vscan[1] = lcurrent;
      lightcount+=1;
      lcurrent = ltop;
      lcount+=1;
    }
  }

  if ( top > 0 )
  {
    int tcount = 1;
    float trange = 100.0/top;
    float tcurrent = 0;
    while ( tcount <= top )
    {
      float ttop = tcurrent + trange;
      String name = "t"+String(tcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = tcurrent;
      lights[lightcount].hscan[1] = ttop;
      lights[lightcount].vscan[0] = 0;
      lights[lightcount].vscan[1] = pct_scan;
      lightcount+=1;
      tcurrent = ttop;
      tcount+=1;
    }
  }

  if ( right > 0 )
  {
    int rcount = 1;
    float rrange = 100.0/right;
    float rcurrent = 0;
    while ( rcount <= right )
    {
      float rtop = rcurrent + rrange;
      String name = "r"+String(rcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = 100-pct_scan;
      lights[lightcount].hscan[1] = 100;
      lights[lightcount].vscan[0] = rcurrent;
      lights[lightcount].vscan[1] = rtop;
      lightcount+=1;
      rcurrent = rtop;
      rcount+=1;
    }
  }
  
        
  if ( bottom > 0 )
  {
    float brange = 100.0/bottom;
    float bcurrent = 100;
    if ( bottom < top )
    {
      brange = 100.0/top;
    }
    while ( bcount <= bottom )
    {
      float btop = bcurrent - brange;
      String name = "b"+String(bcount);
      strncpy(lights[lightcount].lightname, name.c_str(), 4);
      lights[lightcount].hscan[0] = btop;
      lights[lightcount].hscan[1] = bcurrent;
      lights[lightcount].vscan[0] = 100 - pct_scan;
      lights[lightcount].vscan[1] = 100;
      lightcount+=1;
      bcurrent = btop;
      bcount+=1;
    }
  }

  numLights = lightcount;

  #if DEBUG
  Serial.println(F("Fill light data: "));
  char tmp[64];
  sprintf(tmp, " lights %d\n", numLights);
  Serial.print(tmp);
  for (int i=0; i<numLights; i++)
  {
    sprintf(tmp, " light %s scan %2.1f %2.1f %2.1f %2.1f\n", lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
    Serial.print(tmp);
  }
  #endif

}

bool saveBobConfig()
{
  //read configuration from FS json
  if ( SPIFFS.begin() )
  {
    String conf_str = FPSTR(_CONFIG);
    String tmp_str;
    
    //file exists, reading and loading
    File configFile = SPIFFS.open(conf_str, "w");
    if ( configFile )
    {
      int b=0,r=0,t=0,l=0;
      float pct=0;
      DynamicJsonDocument doc(256);

      doc["lights"] = numLights;
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
      doc["top"]    = t;
      doc["bottom"] = b;
      doc["left"]   = l;
      doc["right"]  = r;
      doc["pct"]    = pct;

      serializeJson(doc, configFile);
      configFile.close();

      #if DEBUG
      Serial.println(F("Save config: "));
      serializeJson(doc, Serial);
      Serial.println();
      #endif

      return true;
    }
    else
    {
      #if DEBUG
      Serial.println(F("Failed to create config file."));
      #endif
    }
  }
  else
  {
    #if DEBUG
    Serial.println(F("Failed to initialize SPIFFS."));
    #endif
  }
  //end save

  return false;
}

bool loadBobConfig()
{
  //read configuration from FS json
  if ( SPIFFS.begin() )
  {
    String conf_str = FPSTR(_CONFIG);
    String tmp_str;
    
    if ( SPIFFS.exists(conf_str) )
    {
      #if DEBUG
      Serial.println(F("Reading SPIFFS data."));
      #endif

      //file exists, reading and loading
      File configFile = SPIFFS.open(conf_str, "r");
      if ( configFile )
      {
        size_t size = configFile.size();

        // Allocate a buffer to store contents of the file.
        char *buf = (char*)malloc(size+1);
        configFile.readBytes(buf, size);
        buf[size] = '\0';
        
        #if DEBUG
        Serial.println(size,DEC);
        Serial.println(buf);
        #endif

        DynamicJsonDocument doc(256);
        DeserializationError error = deserializeJson(doc, buf);

        if ( !error && doc["lights"] != nullptr )
        {
          numLights = doc["lights"];
          fillBobLights(doc["bottom"], doc["left"], doc["top"], doc["right"], doc["pct"]);
        }
        else
        {
          #if DEBUG
          Serial.println(F("Error in config file."));
          Serial.println(error.c_str());
          #endif
        }
        free(buf);
        return true;
      }
    }
  }
  //end read
  return false;
}

void BobSync()
{
  yield();    // allow other tasks
  FastLED[bobStrip].showLeds(255);
}

void BobClear()
{
  fill_solid(leds[bobStrip], numLights, CRGB::Black);
  BobSync();
}

void setBobColor(uint16_t led, uint8_t red, uint8_t green, uint8_t blue)
{
  CRGB lcolor;
  lcolor.r = red;
  lcolor.g = green;
  lcolor.b = blue;
  
  leds[bobStrip][led] = lcolor;
}

// main boblight handling
void pollBob() {
  
  //check if there are any new clients
  if (bob.hasClient())
  {
    //find free/disconnected spot
    if (!bobClient || !bobClient.connected())
    {
      if (bobClient) bobClient.stop();
      bobClient = bob.available();
    }
    //no free/disconnected spot so reject
    WiFiClient bobClient = bob.available();
    bobClient.stop();
    BobClear();
  }
  
  //check clients for data
  if (bobClient && bobClient.connected())
  {
    if (bobClient.available())
    {
      //get data from the client
      while (bobClient.available())
      {
        String input = bobClient.readStringUntil('\n');
        //Serial.print("Client: "); Serial.println(input);
        if (input.startsWith("hello"))
        {
          #if DEBUG
          Serial.println(F("hello"));
          #endif
          bobClient.print("hello\n");
        }
        else if (input.startsWith("ping"))
        {
          #if DEBUG
          Serial.println(F("ping 1"));
          #endif
          bobClient.print("ping 1\n");
        }
        else if (input.startsWith("get version"))
        {
          #if DEBUG
          Serial.println(F("version 5"));
          #endif
          bobClient.print("version 5\n");
        }
        else if (input.startsWith("get lights"))
        {
          char tmp[64];
          String answer = "";
          sprintf(tmp, "lights %d\n", numLights);
          #if DEBUG
          Serial.println(tmp);
          #endif
          answer.concat(tmp);
          for (int i=0; i<numLights; i++)
          {
            sprintf_P(tmp, PSTR("light %s scan %2.1f %2.1f %2.1f %2.1f\n"), lights[i].lightname, lights[i].vscan[0], lights[i].vscan[1], lights[i].hscan[0], lights[i].hscan[1]);
            #if DEBUG
            Serial.print(tmp);
            #endif
            answer.concat(tmp);
          }
          bobClient.print(answer);
        }
        else if (input.startsWith("set priority"))
        {
          #if DEBUG
          Serial.println(F("set priority not implemented"));
          #endif
          // not implemented
        }
        else if (input.startsWith("set light "))
        { // <id> <cmd in rgb, speed, interpolation> <value> ...
          input.remove(0,10);
          String tmp = input.substring(0,input.indexOf(' '));
          
          int light_id = -1;
          for ( uint16_t i=0; i<numLights; i++ )
          {
            if ( strncmp(lights[i].lightname, tmp.c_str(), 4) == 0 )
            {
              light_id = i;
              break;
            }
          }
          if ( light_id == -1 )
            return;
/*
          #if DEBUG
          Serial.print(F("light found "));
          Serial.println(light_id,DEC);
          #endif
*/
          input.remove(0,input.indexOf(' ')+1);
          if ( input.startsWith("rgb ") )
          {
            input.remove(0,4);
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t red = (uint8_t)(255.0f*tmp.toFloat());
            input.remove(0,input.indexOf(' ')+1);        // remove first float value
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t green = (uint8_t)(255.0f*tmp.toFloat());
            input.remove(0,input.indexOf(' ')+1);        // remove second float value
            tmp = input.substring(0,input.indexOf(' '));
            uint8_t blue = (uint8_t)(255.0f*tmp.toFloat());
/*
            #if DEBUG
            Serial.println(F("color set "));
            Serial.print(red,HEX);
            Serial.print(green,HEX);
            Serial.print(blue,HEX);
            Serial.println();
            #endif
*/
            setBobColor(light_id, red, green, blue);
          } // currently no support for interpolation or speed, we just ignore this
        }
        else if (input.startsWith("sync"))
        {
/*
          #if DEBUG
          Serial.println(F("sync"));
          #endif
*/
          BobSync();
        }        
        else
        {
          // Client sent gibberish
          #if DEBUG
          Serial.println(F("Client sent gibberish."));
          #endif
          bobClient.stop();
          bobClient = bob.available();
          BobClear();
        }
      }
    }
  }
}
