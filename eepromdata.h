#define DEBUG 1

#define MAXZONES 6              // could go up to 8, if not using WS2801 pixels
#define MAXSECTIONS 9           // don't go wild; 9 sections per zone/LED strip is enough

extern uint8_t numZones;        // number of zones (LED strips), each zone requires a GPIO pin (or two)
extern uint16_t numLEDs[];      // number of pixels in each zone
extern uint8_t numSections[];   // number of sections in each zone
extern uint16_t sectionStart[][MAXSECTIONS];  // start pixel of each section in each zone
extern uint16_t sectionEnd[][MAXSECTIONS];    // last pixel of each section in each zone
extern char zoneLEDType[][7];    // LED type for each zone (WS2801, WS2811, WS2812, ...)

// EEPROM layout:
// [0-2]   esp ................ verification string
// [3-5]   999 ................ Domoticz idx
// [6]     9   ................ number of used zones
// [7-12]  XXXXXX ............. zone 1 LED type (WS2801 (only on zone 0), WS2811, WS2812)
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

typedef struct ZONE_DATA_T {
  char ledType[6];
  char zoneLEDs[3];
  char sectionStart[MAXSECTIONS][3];
} zone_data_t;

typedef struct EEPROM_DATA_T {
  char esp[3];  // contains "esp"
  char idx[3];
  char nZones;
  zone_data_t zoneData[MAXZONES];
} eeprom_data_t;

extern eeprom_data_t e;

#define EEPROM_SIZE sizeof(eeprom_data_t)   // limit is 512 bytes
