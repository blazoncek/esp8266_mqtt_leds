# ESP8266/ESP01 firmware to control WS28xx LED pixels used for holiday lights
Simple holiday lights ESP8266 firmware with 20+ effects directly controllable from Domoticz. Number of LEDs per strip is configurable via MQTT message as is the type of LEDs used.

### Features
- 20+ LED effects (
- Set-up on first use
- Uses WiFi Manager for initial configuration
- Change LED type & number of LEDs via MQTT message
- Restart and/or reset ESP to initial state via MQTT message
- Supports OTA updates
- Supports: WS2801, WS2811 and WS2812B LED strips
- Supports native [Domoticz](https://www.domoticz.com) control (virtual selector switch) via MQTT

### Instructions
- Use GPIO0 (clock) and GPIO2 (data)
- Use 512k or greater SPIFFS if possible (for OTA updates)
- Configure via MQTT message:
  - LEDstrip/led-xxxxxx/command/ledtype `LED strip type`
  - LEDstrip/led-xxxxxx/command/leds `# of LEDs on strip`
  - LEDstrip/led-xxxxxx/command/effect `# of effect`
  - LEDstrip/led-xxxxxx/command/idx `Domoticz virtual switch idx`
  - LEDstrip/led-xxxxxx/command/restart
  - LEDstrip/led-xxxxxx/command/reset
- In Domoticz create a virtual Selector Switch, with options for each effect
