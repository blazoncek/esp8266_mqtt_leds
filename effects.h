/*
 * effects.h  - master include for LED effects
 */
 
// This is an array of leds.  One item for each led in your strip.
extern CRGB *leds[], gRGB;

extern boolean breakEffect;
extern uint8_t gHue, gBrightness;

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
  BOUNCINGBALL,
  BOUNCINGCOLOREDBALLS,
  METEORRAIN,
  SINELON,
  BPM,
  JUGGLE,
  COLORCHASE,
  CHRISTMASCHASE,
  LAST_EFFECT = CHRISTMASCHASE
};
typedef EFFECTS effects_t;

// Fire effect static data (byte[numLEDs]) allocated at init
extern byte *heat[];

// LED effects
void solidColor(CRGB c);
void RGBLoop();
void FadeInOut();
void Strobe(CRGB c, int FlashDelay=50);
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade=true);
void CylonBounce(int EyeSizePct, int SpeedDelay=10);
void NewKITT(int EyeSizePct, int SpeedDelay);
void CenterToOutside(CRGB c, int EyeSizePct, int SpeedDelay=10, boolean Fade=true);
void OutsideToCenter(CRGB c, int EyeSizePct, int SpeedDelay=10, boolean Fade=true);
void LeftToRight(CRGB c, int EyeSizePct, int SpeedDelay=10, boolean Fade=true);
void RightToLeft(CRGB c, int EyeSizePct, int SpeedDelay=10, boolean Fade=true);
void Twinkle(int SpeedDelay, boolean OnlyOne=false);
void TwinkleRandom(int SpeedDelay, boolean OnlyOne=false);
void sinelon(int SpeedDelay=10);
void bpm(int SpeedDelay=10);
void juggle(int SpeedDelay=10);
void Sparkle(int SpeedDelay);
void snowSparkle(int SparkleDelay, int SpeedDelay);
void runningLights(int WaveDelay);
void colorWipe(int SpeedDelay=10, boolean Reverse=false);
void colorChase(CRGB c[], int Size=1, int SpeedDelay=200, boolean Reverse=false);
void christmasChase(int Size, int SpeedDelay=200, boolean Reverse=false);
void theaterChase(CRGB c, int SpeedDelay=200);
void rainbowCycle(int SpeedDelay=10);
void rainbowChase(int SpeedDelay=200);
void Fire(int Cooling, int Sparking, int SpeedDelay);
void bouncingColoredBalls(int BallCount, CRGB colors[]);
void meteorRain(byte meteorSizePct, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay);

// Common Functions
void setPixel(int zone, int Pixel, CRGB c);
void setAll(int zone, int section, CRGB c);
void fadeToBlack(int zone, int ledNo, byte fadeValue);
void addGlitter(int zone, int section, fract8 chanceOfGlitter);

// private functions
void setPixelHeatColor(int zone, int Pixel, byte temperature);
