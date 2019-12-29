/*
 * effects.h  - master include for LED effects
 */
 
#define MAXZONES 4                          // can be 5 if also using RX/TX pins, if not using WS2801 pixels can be up to 8
#define MAXSECTIONS 9                       // don't go wild; 9 is enough

extern int numLEDs[];
extern int numZones;
extern int numSections[];
extern int sectionStart[][MAXSECTIONS];
extern int sectionEnd[][MAXSECTIONS];

// This is an array of leds.  One item for each led in your strip.
extern CRGB *leds[], gRGB;

extern boolean breakEffect;
extern int gHue, gBrightness;

// Fire effect static data (byte[numLEDs]) allocated at init
extern byte *heat[];

// from master file
void showStrip();

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
