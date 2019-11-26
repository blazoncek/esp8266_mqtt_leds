// This is an array of leds.  One item for each led in your strip.
extern CRGB *leds;
extern int numLEDs;

// Fire effect static data (byte[numLEDs]) allocated at init
extern byte *heat;

// from master file
void showStrip();

// LED effects
void RGBLoop();
void FadeInOut(CRGB c);
void Strobe(CRGB c, int FlashDelay=100);
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade=true);
void CylonBounce(int EyeSize, int SpeedDelay);
void NewKITT(CRGB c, int EyeSize, int SpeedDelay);
void CenterToOutside(CRGB c, int EyeSize, int SpeedDelay, boolean Fade=true);
void OutsideToCenter(CRGB c, int EyeSize, int SpeedDelay, boolean Fade=true);
void LeftToRight(CRGB c, int EyeSize, int SpeedDelay, boolean Fade=true);
void RightToLeft(CRGB c, int EyeSize, int SpeedDelay, boolean Fade=true);
void Twinkle(CRGB c, int SpeedDelay, boolean OnlyOne=false);
void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne=false);
void sinelon(CRGB c);
void snowSparkle(int SparkleDelay, int SpeedDelay);
void runningLights(CRGB c, int WaveDelay);
void colorWipe(CRGB c, int SpeedDelay);
void colorWipeReverse(CRGB c, int SpeedDelay);
void rainbowCycle(int SpeedDelay);
void theaterChase(CRGB c, int SpeedDelay);
void theaterChaseRainbow(int SpeedDelay);
void Fire(int Cooling, int Sparking, int SpeedDelay, boolean SplitOnLong=false, int SplitPoint=119);
void bouncingColoredBalls(int BallCount, CRGB colors[]);
void meteorRain(CRGB c, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay);

// Common Functions
void setPixel(int Pixel, CRGB c);
void setAll(CRGB c);
void fadeToBlack(int ledNo, byte fadeValue);
void addGlitter(fract8 chanceOfGlitter);

// private functions
void setPixelHeatColor(int Pixel, byte temperature);
