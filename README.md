# ESP8266/ESP01 firmware to control WS28xx LED pixels used for holiday lights/boblight
Simple holiday lights ESP8266 firmware with 20+ effects directly controllable from Domoticz. Number of LEDs per strip is configurable via MQTT message as is the type of LEDs used.
Supports multiple zones (on multiple pins; max: 6) and up to 9 sections in each zone.
Basic Boblight support on zone 0.
Also supports Domoticz RGBW Switch type natively.

### Features
- 20+ LED effects (
- Set-up on first use / configure via MQTT/web server
- Uses WiFi Manager for initial configuration
- Change LED type & number of LEDs via MQTT message or embedded web interface
- basic Boblight support (works with Kodi)
- Restart and/or reset ESP to initial state via MQTT message
- Supports OTA updates
- Supports: WS2801, WS2811 and WS2812B LED strips
- Supports native [Domoticz](https://www.domoticz.com) control (virtual selector switch) via MQTT

### Instructions
- Use GPIO14 (clock) and GPIO12 (data) `only for zone 0 & SPI based chip`
- Use GPIO 0,4,2,5,15,13 pins for non-SPI based chips (3-wire pixels WS2812/WS2811)
- Use only zones 0 and 1 for WS2811 pixels
- Use 64k SPIFFS on ESP01 if possible (for OTA updates via Arduino IDE)
- Configure via embedded web server
- Configure via MQTT message:
  - LEDstrip/led-xxxxxx/set/zones [{JSON}] `zone 0 configuration in JSON format`
    - {JSON} = { "zones":[{"ledtype":"WSxxxx", "leds":50, "sections":"0,25"},{"ledtype":...}] }
    - "ledtype" : [WS2801|WS2811|WS2812] `LED strip type`
    - "leds" : [#] `# of LEDs in zone 0`
    - "sections" : [0,#,...] `section's start delimited by comma`
  - LEDstrip/led-xxxxxx/set/effect [#] `# of effect`
  - LEDstrip/led-xxxxxx/set/idx [#] `# of Domoticz virtual switch idx`
  - LEDstrip/led-xxxxxx/set/hue [#] `set Hue`
  - LEDstrip/led-xxxxxx/set/brightness [#] `global brightness control`
  - LEDstrip/led-xxxxxx/command/restart
  - LEDstrip/led-xxxxxx/command/reset [1] `factory reset`
- In Domoticz create a virtual Selector Switch, with options for each effect
- Due to ESP8266 HW limitations and speed of LED updates it is only possible to drive about 400-450 LEDs

