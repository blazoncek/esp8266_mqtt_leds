# ESP8266/ESP01 firmware to control WS28xx LED pixels used for holiday lights
Simple holiday lights ESP8266 firmware with 20+ effects directly controllable from Domoticz. Number of LEDs per strip is configurable via MQTT message as is the type of LEDs used.
Supports multiple zones (on multiple pins; max: 8) and up to 9 sections in each zone.
Also supports Domoticz RGBW Switch type natively.

### Features
- 20+ LED effects (
- Set-up on first use / configure via MQTT
- Uses WiFi Manager for initial configuration
- Change LED type & number of LEDs via MQTT message
- Restart and/or reset ESP to initial state via MQTT message
- Supports OTA updates
- Supports: WS2801, WS2811 and WS2812B LED strips
- Supports native [Domoticz](https://www.domoticz.com) control (virtual selector switch) via MQTT

### Instructions
- Use GPIO0 (clock) and GPIO2 (data) `for zone 0, SPI based chip`
- GPIO 4,12,15 pins for data pin zones 1,2,3; GPIO 5,14,13 clock pin zones 1,2,3
- Use GPIO 0,4,12,15,2,5,14,13 pins for non-SPI based chips (3-wire pixels)
- Use 512k or greater SPIFFS if possible (for OTA updates)
- Configure via MQTT message:
  - LEDstrip/led-xxxxxx/set/zones [#] `# of zones (max 4 with SPI based pixels or 8 otherwise)`
  - LEDstrip/led-xxxxxx/set/ledtype/0 [WS2801|WS2811|WS2812] `LED strip type for zone 0`
  - LEDstrip/led-xxxxxx/set/leds/0 [#] `# of LEDs in zone 0`
  - LEDstrip/led-xxxxxx/set/sections/0 [0,#,...] `section's start delimited by comma`
  - LEDstrip/led-xxxxxx/set/effect [#] `# of effect`
  - LEDstrip/led-xxxxxx/set/idx [#] `# of Domoticz virtual switch idx`
  - LEDstrip/led-xxxxxx/set/hue [#] `set Hue`
  - LEDstrip/led-xxxxxx/set/brightness [#] `global brightness control`
  - LEDstrip/led-xxxxxx/command/restart
  - LEDstrip/led-xxxxxx/command/reset [1] `factory reset`
- In Domoticz create a virtual Selector Switch, with options for each effect
