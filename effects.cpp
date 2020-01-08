#include "FastLED.h"

#include "eepromdata.h"
#include "effects.h"

boolean breakEffect = false;
byte *heat[MAXZONES]; // Fire effect static data (for each zone; max 8)

typedef void (*NewKITTPatternList[])(CRGB, int, int, boolean);
NewKITTPatternList gPatterns = { LeftToRight, RightToLeft, OutsideToCenter, CenterToOutside, RightToLeft, LeftToRight, OutsideToCenter, CenterToOutside };
int gCurrentPattern = 0;

// *************************
// ** LEDEffect Functions **
// *************************

//------------------------------------------------------//
void solidColor(CRGB c) {
  //showColor(&c)
  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(10);  
}

//------------------------------------------------------//
void RGBLoop() {
  static int count = 0;
  
  if ( count < 256 ) {  // 256 up, 256 down
    gHue = HUE_RED;
  } else if ( count < 512 ) {
    gHue = HUE_GREEN;
  } else {
    gHue = HUE_BLUE;
  }
  FadeInOut();

  if ( ++count == 768 ) count = 0;
}

//------------------------------------------------------//
void FadeInOut() {
  static unsigned int k = 0;

  unsigned int level = triwave8(k);
  CRGB c = CHSV(gHue, 255, level);
  
  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(10);

  ++k &= 0xFF;
  if ( k==0 ) gHue += 8;
}

//------------------------------------------------------//
void Strobe(CRGB c, int FlashDelay) {

  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(FlashDelay/2);
  
  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], CRGB::Black);
  }
  showStrip();
  FastLED.delay(FlashDelay/2);
}

//------------------------------------------------------//
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade) {
  int i;
  static unsigned long wait = 0;

  if ( wait > millis() ) {
    return;
  }
  wait = millis() + random16(500,2500);
  
  for ( int z=0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 196);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      int StartPoint = random16(sectionStart[z][s], sectionEnd[z][s] - (2*EyeWidth) - EyeSpace);
      int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

      // show eyes
      for ( i=0; i<EyeWidth; i++ ) {
        leds[z][StartPoint + i] = leds[z][Start2ndEye + i] = c;
      }
    }
  }
  showStrip();
  FastLED.delay(10);
}

//------------------------------------------------------//
void CylonBounce(int EyeSizePct, int SweepsPerMinute) {
  static boolean dir = false;   // start left-to-right
  static unsigned int pct = 0;  // starting position
  CRGB c = CRGB::Red;
  
  uint8_t beat = beat8(SweepsPerMinute)*100/255;
  if ( pct == beat ) {  // if there is no change in position just exit
    FastLED.delay(10);
    return;
  } else if ( pct > beat ) {  // roll-over, erase the section
    dir = !dir;
  }
  pct = beat; // percent fill

  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], CRGB::Black);
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int EyeSize = max(1,(int)(EyeSizePct * ledsPerSection / 100));
      unsigned int pos = sectionStart[z][s] + (((dir?100-pct:pct) * (ledsPerSection-(EyeSize+2))) / 100);
      leds[z][pos] = leds[z][pos+EyeSize+1] = c/8;
      for ( int j=1; j<=EyeSize; j++ ) {
        leds[z][pos+j] = c; 
      }
    }
  }
  showStrip();
  FastLED.delay(10);
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
//------------------------------------------------------//
void NewKITT(int EyeSizePct, int SpeedDelay){
  CRGB c = CHSV(gHue, 255, 255);

  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPattern](c, EyeSizePct, SpeedDelay, true);

}

void nextPattern() {
  gCurrentPattern = (gCurrentPattern + 1) % ARRAY_SIZE(gPatterns);
  if ( gCurrentPattern == 0 )
    gHue += 8;
}

// used by NewKITT
void CenterToOutside(CRGB c, int EyeSizePct, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 50;  // starting position
  unsigned int EyeSize;
  
  for ( int z=0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][sectionStart[z][s]+pos+j] = c; 
        leds[z][sectionEnd[z][s]-pos-j] = c; 
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);

  if ( pct-- == 0 ) {
    pct = 50;
    nextPattern();
  }
}

// used by NewKITT
void OutsideToCenter(CRGB c, int EyeSizePct, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 0;  // starting position
  unsigned int EyeSize;
  
  for ( int z=0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=1; j<=EyeSize; j++ ) {
        leds[z][sectionStart[z][s]+pos+j] = c; 
        leds[z][sectionEnd[z][s]-pos-j] = c; 
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);

  if ( pct++ == 50 ) {
    pct = 0;
    nextPattern();
  }
}

// used by NewKITT
void LeftToRight(CRGB c, int EyeSizePct, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 0;  // starting position
  unsigned int EyeSize;
  
  for ( int z=0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = sectionStart[z][s] + ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][pos+j] = c; 
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);

  if ( pct++ == 100 ) {
    pct = 0;
    nextPattern();
  }
}

// used by NewKITT
void RightToLeft(CRGB c, int EyeSizePct, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 100;  // starting position
  unsigned int EyeSize;
  
  for ( int z=0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = sectionStart[z][s] + ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][pos+j] = c; 
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);

  if ( pct-- == 0 ) {
    pct = 100;
    nextPattern();
  }
}

//------------------------------------------------------//
void Twinkle(int SpeedDelay, boolean OnlyOne) {

  for ( int z=0; z<numZones; z++ ) {

    if ( !OnlyOne )
      fadeToBlackBy(leds[z], numLEDs[z], 32);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      int pos = random16(sectionStart[z][s], sectionEnd[z][s]);
      leds[z][pos] = CHSV(gHue++, 255, 255);
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}

//------------------------------------------------------//
void TwinkleRandom(int SpeedDelay, boolean OnlyOne) {
  gHue = random8();
  Twinkle(SpeedDelay, OnlyOne);
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void sinelon(int SpeedDelay) {

  // a colored dot sweeping back and forth, with fading trails
  for ( int z=0; z<numZones; z++ ) {
    fadeToBlackBy(leds[z], numLEDs[z], 16);
    for ( int s=0; s<numSections[z]; s++ ) {
      int pos = beatsin16(20, sectionStart[z][s], sectionEnd[z][s]-1);
      leds[z][pos] = CHSV(gHue, 255, 255);
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);

  gHue++;
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void bpm(int SpeedDelay) {

  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = RainbowColors_p; // PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 32, 255);
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) { //9948
        leds[z][i] = ColorFromPalette(palette, gHue+((i-sectionStart[z][s])*2), beat-gHue+((i-sectionStart[z][s])*10));
      }
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);

  gHue++;
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void juggle(int SpeedDelay) {

  // eight colored dots, weaving in and out of sync with each other
  for ( int z=0; z<numZones; z++ ) {
    fadeToBlackBy(leds[z], numLEDs[z], 64);
    for ( int s=0; s<numSections[z]; s++ ) {
      byte dothue = 0;
      for ( int i=0; i<8; i++ ) {
        leds[z][beatsin16(i+7, sectionStart[z][s], sectionEnd[z][s]-1)] |= CHSV(dothue, 255, 255);
        dothue += 32;
      }
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}


//------------------------------------------------------//
void Sparkle(int SpeedDelay) {

  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], CRGB::Black);
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      int pos = random16(sectionStart[z][s], sectionEnd[z][s]);
      leds[z][pos] = CRGB::White;
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}

//------------------------------------------------------//
void snowSparkle(int SparkleDelay, int SpeedDelay) {
  CRGB c = CRGB(16,16,16);
  
  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
    for ( int s=0; s<numSections[z]; s++ ) {
      addGlitter(z, s, 90);
    }
  }
  showStrip();
  FastLED.delay(SparkleDelay);
  
  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}

//------------------------------------------------------//
void runningLights(int WaveDelay) {
  static unsigned int offset = 0;
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      for ( int i=0; i<ledsPerSection; i++ ) {
        unsigned int level = sin8( (i*1280/ledsPerSection + offset) & 0xFF );  // 5 waves
        leds[z][sectionStart[z][s] + i] = CHSV(gHue, 255, level);
      }
    }
  }
  showStrip();
  FastLED.delay(WaveDelay);

  gHue++;
  offset = (offset + 21) & 0xFF;
}

//------------------------------------------------------//
void colorWipe(int WipesPerMinute, boolean Reverse) {
  static unsigned int pct = 0;  // starting position
  static boolean blank = false;
  CRGB c;

  uint8_t beat = beat8(WipesPerMinute*2)*100/255; // double the speed (colored & black wipes)
  if ( pct == beat ) {  // if there is no change in position just exit
    FastLED.delay(5);
    return;
  } else if ( pct > beat ) {  // roll-over, erase the section
    blank = !blank;
    if ( blank ) gHue += 8;
  }
  pct = beat; // percent fill
  
  if ( blank ) {
    c = CRGB::Black;
  } else {
    c = CHSV(gHue, 255, 255);
  }
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int nLeds = (pct * ledsPerSection) / 100;
      if ( Reverse ) {
        fill_solid(&leds[z][sectionEnd[z][s]-nLeds-1], nLeds, c);
      } else {
        fill_solid(&leds[z][sectionStart[z][s]], nLeds, c);
      }
    }
  }
  showStrip();
  FastLED.delay(5);
}

//------------------------------------------------------//
void colorChase(CRGB c[], int Size, int SpeedDelay, boolean Reverse) {
  int window = 3*Size;  // c[] has 3 elements
  int pos;
  
  // move by 1 pixel within window
  for ( int q=0; q<window; q++ ) {

    for ( int z=0; z<numZones; z++ ) {
      fill_solid(leds[z], numLEDs[z], c[2]);
      for ( int s=0; s<numSections[z]; s++ ) {
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
        for ( int i=0; i<ledsPerSection; i+=window ) {
          // turn LEDs on: ..++++####....++++####..   (size=4, . = black, + = c1, # = c2)
          // turn LEDs on: ..####++++....####++++..   (size=4, . = black, + = c1, # = c2, Reverse)
          for ( int j=0; j<Size; j++ ) {
            if ( Reverse ) {
              pos = (window-q-1) + (Size-j-1);
              if ( (pos+Size)%window + i < ledsPerSection )
                leds[z][sectionStart[z][s] + (pos+Size)%window + i] = c[0];
              if ( pos%window + i < ledsPerSection )
                leds[z][sectionStart[z][s] + pos%window + i] = c[1];
            } else {
              pos = q + j;
              if ( pos%window + i < ledsPerSection )
                leds[z][sectionStart[z][s] + pos%window + i] = c[0];
              if ( (pos+Size)%window + i < ledsPerSection )
                leds[z][sectionStart[z][s] + (pos+Size)%window + i] = c[1];
            }
          }
        }
      }
    }
    showStrip();
    FastLED.delay(SpeedDelay/Size);
  }
}

//------------------------------------------------------//
void christmasChase(int Size, int SpeedDelay, boolean Reverse) {
  CRGB c[3] = {CRGB::Red, CRGB::Green, CRGB::White};
  colorChase(c, Size, SpeedDelay, Reverse);
}

//------------------------------------------------------//
void theaterChase(CRGB cColor, int SpeedDelay) {
  CRGB c[3] = {cColor, CRGB::Black, CRGB::Black};
  colorChase(c, 1, SpeedDelay, false);
}

//------------------------------------------------------//
void rainbowChase(int SpeedDelay) {
  static int q = 0;

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      fill_rainbow(&leds[z][sectionStart[z][s]], ledsPerSection, gHue, max(1,255/ledsPerSection));
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) {
        if ( i%3 != q ) {
          leds[z][i] = CRGB::Black;
        }
      }
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);

  ++q %= 3;
  gHue++;
}

//------------------------------------------------------//
void rainbowCycle(int SpeedDelay) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      fill_rainbow(&leds[z][sectionStart[z][s]], ledsPerSection, gHue, max(1,255/ledsPerSection));
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
  gHue++;
}

//------------------------------------------------------//
void rainbowBounce(int EyeSizePct, int SweepsPerMinute) {
  static boolean dir = false;   // start left-to-right
  static unsigned int pct = 0;  // starting position
  
  uint8_t beat = beat8(SweepsPerMinute)*100/255;
  if ( pct == beat ) {  // if there is no change in position just exit
    FastLED.delay(10);
    return;
  } else if ( pct > beat ) {  // roll-over, erase the section
    dir = !dir;
  }
  pct = beat; // percent fill

  for ( int z=0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], CRGB::Black);
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int EyeSize = max(1,(int)(EyeSizePct * ledsPerSection / 100));
      unsigned int pos = sectionStart[z][s] + (((dir?100-pct:pct) * (ledsPerSection-EyeSize)) / 100);
      fill_rainbow(&leds[z][pos], EyeSize, 0, max(1,(int)(255/EyeSize)));
    }
  }
  
  showStrip();
  FastLED.delay(10);
}

//------------------------------------------------------//
// borrowed from FastLED demo
void Fire(int Cooling, int Sparking, int SpeedDelay) {
  int cooldown;

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      // Odd an even sections are 'burnng' from the oposite ends.
      
      // Step 1.  Cool down every cell a little
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) {
        cooldown = random16(0, ((Cooling * 10) / ledsPerSection) + 2);
        
        if ( cooldown>heat[z][i] ) {
          heat[z][i] = ((s%2==0 && i<sectionStart[z][s]+7) || (s%2==1 && i>sectionEnd[z][s]-8)) ? 2 : 0;
        } else {
          heat[z][i] = heat[z][i]-cooldown;
        }
      }
      
      // Step 2.  Heat from each cell drifts 'up' and diffuses a little
      for ( int k=ledsPerSection-1; k>=2; k-- ) {
        if ( s%2 == 1 ) {
          heat[z][sectionStart[z][s]+ledsPerSection-k] = (heat[z][sectionStart[z][s]+ledsPerSection-k + 1] + heat[z][sectionStart[z][s]+ledsPerSection-k + 2] + heat[z][sectionStart[z][s]+ledsPerSection-k + 2]) / 3;
        } else {
          heat[z][sectionStart[z][s]+k] = (heat[z][sectionStart[z][s]+k - 1] + heat[z][sectionStart[z][s]+k - 2] + heat[z][sectionStart[z][s]+k - 2]) / 3;
        }
        if ( k==2 )
          heat[z][sectionStart[z][s]+k-1] = heat[z][sectionStart[z][s]+k - 2] / 2;
      }
        
      // Step 3.  Randomly ignite new 'sparks' near the bottom
      if( random8() < Sparking ) {
        int y = random8(7);
        if ( s%2 == 1 ) {
          heat[z][sectionStart[z][s]+ledsPerSection-y-1] = heat[z][sectionStart[z][s]+ledsPerSection-y-1] + random8(160,255);
        } else {
          heat[z][sectionStart[z][s]+y] = heat[z][sectionStart[z][s]+y] + random8(160,255);
        }
      }
    
      // Step 4.  Convert heat to LED colors
      for ( int j=sectionStart[z][s]; j<sectionEnd[z][s]; j++ ) {
        leds[z][j] = HeatColor(heat[z][j]);  // from FastLED library
      }
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}

//------------------------------------------------------//
void bouncingColoredBalls(int BallCount, CRGB colors[]) {
  float Gravity = -9.81;
  int StartHeight = 1;
  
  float Height[BallCount];
  float ImpactVelocityStart = sqrt( -2 * Gravity * StartHeight );
  float ImpactVelocity[BallCount];
  float TimeSinceLastBounce[BallCount];
  int   Position[BallCount];
  long  ClockTimeSinceLastBounce[BallCount];
  float Dampening[BallCount];
  boolean ballBouncing[BallCount];
  boolean ballsStillBouncing = true;
  
  for ( int i=0; i<BallCount; i++ ) {   
    ClockTimeSinceLastBounce[i] = millis();
    Height[i] = StartHeight;
    Position[i] = 0; 
    ImpactVelocity[i] = ImpactVelocityStart;
    TimeSinceLastBounce[i] = 0;
    Dampening[i] = 0.90 - float(i)/pow(BallCount,2);
    ballBouncing[i]=true; 
  }

  while ( ballsStillBouncing ) {
    if ( breakEffect ) return;
    
    for ( int z=0; z<numZones; z++ ) {
      fill_solid(leds[z], numLEDs[z], CRGB::Black);
      for ( int s=0; s<numSections[z]; s++ ) {
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
       
        for ( int i=0; i<BallCount; i++ ) {
          TimeSinceLastBounce[i] =  millis() - ClockTimeSinceLastBounce[i];
          Height[i] = 0.5 * Gravity * pow( TimeSinceLastBounce[i]/1000 , 2.0 ) + ImpactVelocity[i] * TimeSinceLastBounce[i]/1000;
      
          if ( Height[i] < 0 ) {                      
            Height[i] = 0;
            ImpactVelocity[i] = Dampening[i] * ImpactVelocity[i];
            ClockTimeSinceLastBounce[i] = millis();
      
            if ( ImpactVelocity[i] < 0.01 ) {
              ballBouncing[i]=false;
            }
          }
          Position[i] = round( Height[i] * (ledsPerSection - 1) / StartHeight);
        }
    
        ballsStillBouncing = false; // assume no balls bouncing
        for ( int i=0; i<BallCount; i++ ) {
          leds[z][sectionStart[z][s]+Position[i]] = colors[i];
          if ( ballBouncing[i] ) {
            ballsStillBouncing = true;
          }
        }
      }
    }    
    showStrip();
  }
}

//------------------------------------------------------//
void meteorRain(byte meteorSizePct, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  static unsigned int pct = 0;  // starting position
  unsigned int meteorSize;
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      meteorSize = ledsPerSection * meteorSizePct / 100;
      unsigned int pos = sectionStart[z][s] + ((pct * ledsPerSection+2*meteorSize) / 100);

      // fade brightness all LEDs one step
      for ( int j=0; j<ledsPerSection; j++ ) {
        if ( (!meteorRandomDecay) || (random8(10)>5) ) {
          leds[z][sectionStart[z][s]+j].fadeToBlackBy(meteorTrailDecay);
        }
      }

      for ( int j=0; j<meteorSize; j++ ) {
        if ( (pos-j < sectionEnd[z][s]) && (pos-j >= sectionStart[z][s]) ) {
          leds[z][pos-j] = CRGB::White;
        } 
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);

  if ( pct++ == 100 ) {
    pct = 0;
  }
}

// ***************************************
// ** Common Functions **
// ***************************************

void addGlitter(int zone, int section, fract8 chanceOfGlitter) {
  if ( random8() < chanceOfGlitter ) {
    leds[zone][ random16(sectionStart[zone][section],sectionEnd[zone][section]) ] += CRGB::White; // 100% white
  }
}

// Color wheel (get RGB from hue)
CRGB * getRGBfromHue(byte hue) {
  static CRGB c;
  
  if ( hue < 85 ) {
   c.r = hue * 3;
   c.g = 255 - hue * 3;
   c.b = 0;
  } else if ( hue < 170 ) {
   hue -= 85;
   c.r = 255 - hue * 3;
   c.g = 0;
   c.b = hue * 3;
  } else {
   hue -= 170;
   c.r = 0;
   c.g = hue * 3;
   c.b = 255 - hue * 3;
  }

  return &c;
}

CRGB * getRGBfromHeat(byte temp) {
  static CRGB c;

  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = (byte)(((int)temp*191)/255);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    c = CRGB(255, 255, heatramp);
  } else if( t192 > 0x40 ) {             // middle
    c = CRGB(255, heatramp, 0);
  } else {                               // coolest
    c = CRGB(heatramp, 0, 0);
  }
  return &c;
}
