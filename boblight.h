#define MAX_LEDS     150         // Max number of LEDs don't go above 150 due to ArduinoJson memory problems (150 pixels are on 5m strip spaced at 30 pixels/m)
#define BOB_PORT     19333       // Default boblightd port

typedef struct _LIGHT {
  char lightname[5];
  float hscan[2];
  float vscan[2];
} light_t;

extern light_t lights[];
extern uint16_t numLights;
extern uint8_t bobStrip;

void pollBob();

bool loadBobConfig();
bool saveBobConfig();
void fillBobLights(int,int,int,int,float);
