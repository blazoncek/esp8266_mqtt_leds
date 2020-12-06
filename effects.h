/*
 * effects.h  - master include for LED effects
 */
 
extern boolean volatile breakEffect;

// from master file
void showStrip();

// Effect enums
enum EFFECTS {
  OFF = 0,
  SOLID,
  FADEINOUT,
  STROBE,
  HALLOWEENEYES,
  CYLONBOUNCE,
  NEWKITT,
  TWINKLE,
  TWINKLERANDOM,
  SPARKLE,
  SNOWSPARKLE,
  RUNNINGLIGHTS,
  COLORWIPE,
  RAINBOWCYCLE,
  THEATRECHASE,
  RAINBOWCHASE,
  FIRE,
  GRADIENTCYCLE,
  BOUNCINGCOLOREDBALLS,
  METEORRAIN,
  SINELON,
  BPM,
  JUGGLE,
  COLORCHASE,
  CHRISTMASCHASE,
  RAINBOWBOUNCE,
  ROTATECHASE,
  LAST_EFFECT = ROTATECHASE
};
typedef EFFECTS effects_t;

typedef struct EFFECT_NAME {
  effects_t id;
  char name[20];
} effect_name_t;

extern effect_name_t effects[];
extern const char *effect_names[];

// Fire effect static data (byte[numLEDs]) allocated at init
extern byte *heat[];

// LED effects
void solidColor(CRGB c);
void RGBLoop();
void FadeInOut();
void Strobe(CRGB c, int FlashDelay=50);
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade=true);
void eyeBounce(CRGB c, int EyeSizePct, int SweepsPerMinute=60);
void NewKITT(int EyeSizePct, int SweepsPerMinute=60);
void CenterToOutside(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade=true);
void OutsideToCenter(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade=true);
void LeftToRight(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade=true);
void RightToLeft(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade=true);
void Twinkle(int SpeedDelay, boolean OnlyOne=false);
void TwinkleRandom(int SpeedDelay, boolean OnlyOne=false);
void sinelon(int SpeedDelay=10);
void bpm(int SpeedDelay=10);
void juggle(int SpeedDelay=10);
void Sparkle(int SpeedDelay, CRGB c=CRGB::Black);
void snowSparkle(int SpeedDelay);
void runningLights(int WaveDelay);
void colorWipe(int WipesPerMinute=30, boolean Reverse=false);
void colorChase(CRGB c[], int Size=1, int SpeedDelay=50, boolean Reverse=false);
void christmasChase(int Size, int SpeedDelay=50, boolean Reverse=false);
void theaterChase(CRGB c, int SpeedDelay=50);
void gradientCycle(int SpeedDelay=10);
void rainbowCycle(int SpeedDelay=10);
void rainbowChase(int SpeedDelay=200);
void Fire(int Cooling, int Sparking, int SpeedDelay);
void bouncingColoredBalls(int BallCount, CRGB colors[]);
void meteorRain(byte meteorSizePct, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay);
void rotateChase(CRGB c, int speedDelay);

// Common Functions
void addGlitter(int zone, int section, fract8 chanceOfGlitter);

// private functions
unsigned int getPosFromPct(unsigned int pct, unsigned int ledsPerSection, byte width=1, bool Reverse=false);
void setPixelHeatColor(int zone, int Pixel, byte temperature);
CRGB * getRGBfromHue(byte hue);
CRGB * getRGBfromHeat(byte temp);
