#include "FastLED.h"
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
    for ( int s=0; s<numSections[z]; s++ ) {
      setAll(z, s, c);
    }
  }
  showStrip();
  delay(10);

  ++k &= 0xFF;
  if ( k==0 ) gHue += 8;
}

//------------------------------------------------------//
void Strobe(CRGB c, int FlashDelay) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      setAll(z, s, c);
    }
  }
  showStrip();
  delay(FlashDelay/2);
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      setAll(z, s, CRGB::Black);
    }
  }
  showStrip();
  delay(FlashDelay/2);
}

//------------------------------------------------------//
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade) {
  int i;

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      if ( Fade )
        fadeToBlack(z, s, 196);
      else
        setAll(z, s, CRGB::Black); // Set all black
        
      int StartPoint = random16(sectionStart[z][s], sectionEnd[z][s] - (2*EyeWidth) - EyeSpace);
      int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

      // show eyes
      for ( i=0; i<EyeWidth; i++ ) {
        setPixel(z, StartPoint + i, c);
        setPixel(z, Start2ndEye + i, c);
      }
/*
      if ( !Fade ) {
        // blink
        delay(random(250,750));
        for ( i=0; i<EyeWidth; i++ ) {
          setPixel(z, StartPoint + i, CRGB::Black);
          setPixel(z, Start2ndEye + i, CRGB::Black);
        }
        showStrip();
        delay(random(50,150));
      
        // show eyes
        for(i = 0; i < EyeWidth; i++) {
          setPixel(z, StartPoint + i, c);
          setPixel(z, Start2ndEye + i, c);
        }
        showStrip();
        delay(random(500,2500));
      }
*/
    }
  }
  showStrip();
  delay(random(500,2500));
}

//------------------------------------------------------//
void CylonBounce(int EyeSize, int SpeedDelay) {
  CRGB c = CRGB::Red;
  static boolean dir = false; // start left-to-right
  static unsigned int pct = 0;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = sectionStart[z][s] + ((pct * (ledsPerSection-EyeSize-2)) / 100); // get position using 'sawtooth' BPM function

      setAll(z, s, CRGB::Black);

      setPixel(z, pos, c/8);
      for ( int j=1; j<=EyeSize; j++ ) {
        setPixel(z, pos+j, c); 
      }
      setPixel(z, pos+EyeSize+1, c/8);
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( dir ) {
    pct--;
  } else {
    pct++;
  }

  if ( pct == 100 || pct == 0) {
    dir = !dir;
  }
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
//------------------------------------------------------//
void NewKITT(int EyeSize, int SpeedDelay){
  CRGB c = CHSV(gHue, 255, 255);

  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPattern](c, EyeSize, SpeedDelay, true);

}

void nextPattern() {
  gCurrentPattern = (gCurrentPattern + 1) % ARRAY_SIZE(gPatterns);
  if ( gCurrentPattern == 0 )
    gHue += 8;
}

// used by NewKITT
void CenterToOutside(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 50;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      if ( Fade ) {
        for ( int i=sectionStart[z][s]; i < sectionEnd[z][s]; i++ )
          fadeToBlack(z, i, 64); // fade brightness of all LEDs by 25%
      } else {
        setAll(z, s, CRGB::Black);
      }

      for ( int j=0; j<EyeSize; j++ ) {
        setPixel(z, sectionStart[z][s]+pos+j, c); 
        setPixel(z, sectionEnd[z][s]-pos-j, c); 
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( pct-- == 0 ) {
    pct = 50;
    nextPattern();
  }
}

// used by NewKITT
void OutsideToCenter(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 0;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      if ( Fade ) {
        for ( int i=sectionStart[z][s]; i < sectionEnd[z][s]; i++ )
          fadeToBlack(z, i, 64); // fade brightness of all LEDs in one step by 25%
      } else {
        setAll(z, s, CRGB::Black);
      }

      for ( int j=1; j<=EyeSize; j++ ) {
        setPixel(z, sectionStart[z][s]+pos+j, c); 
        setPixel(z, sectionEnd[z][s]-pos-j, c); 
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( pct++ == 50 ) {
    pct = 0;
    nextPattern();
  }
}

// used by NewKITT & Cylon Bounce
void LeftToRight(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 0;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = sectionStart[z][s] + ((pct * (ledsPerSection-EyeSize)) / 100);

      if ( Fade ) {
        for ( int i=sectionStart[z][s]; i < sectionEnd[z][s]; i++ )
          fadeToBlack(z, i, 64); // fade brightness of all LEDs in one step by 25%
      } else {
        setAll(z, s, CRGB::Black);
      }

      for ( int j=0; j<EyeSize; j++ ) {
        setPixel(z, pos+j, c); 
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( pct++ == 100 ) {
    pct = 0;
    nextPattern();
  }
}

// used by NewKITT & Cylon Bounce
void RightToLeft(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {
  static unsigned int pct = 100;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = sectionStart[z][s] + ((pct * (ledsPerSection-EyeSize)) / 100);

      if ( Fade ) {
        for ( int i=sectionStart[z][s]; i < sectionEnd[z][s]; i++ )
          fadeToBlack(z, i, 64); // fade brightness of all LEDs in one step by 25%
      } else {
        setAll(z, s, CRGB::Black);
      }

      for ( int j=0; j<EyeSize; j++ ) {
        setPixel(z, pos+j, c); 
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( pct-- == 0 ) {
    pct = 100;
    nextPattern();
  }
}

//------------------------------------------------------//
void Twinkle(int SpeedDelay, boolean OnlyOne) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      if ( OnlyOne ) { 
        for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) {
          setPixel(z, i, CRGB::Black);
        }
      } else {
        for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) {
          fadeToBlack(z, i, 32);  // 12.5% fade
        }
      }
      
      int pos = random16(sectionStart[z][s], sectionEnd[z][s]);
      setPixel(z, pos, CHSV(gHue++, 255, 255));
    }
  }
  showStrip();
  delay(SpeedDelay);
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
      int pos = beatsin16(13, sectionStart[z][s], sectionEnd[z][s]-1);
      leds[z][pos] = CHSV(gHue, 255, 255);
    }
  }
  showStrip();
  delay(SpeedDelay);

  gHue++;
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void bpm(int SpeedDelay) {

  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 32, 255);
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) { //9948
        leds[z][i] = ColorFromPalette(palette, gHue+((i-sectionStart[z][s])*2), beat-gHue+((i-sectionStart[z][s])*10));
      }
    }
  }
  showStrip();
  delay(SpeedDelay);

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
        leds[z][beatsin16(i+7, sectionStart[z][s], sectionEnd[z][s]-1)] |= CHSV(dothue, 200, 255);
        dothue += 32;
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}


//------------------------------------------------------//
void Sparkle(int SpeedDelay) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      setAll(z, s, CRGB::Black);
      
      int pos = random16(sectionStart[z][s], sectionEnd[z][s]);
      setPixel(z, pos, CRGB::White);
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void snowSparkle(int SparkleDelay, int SpeedDelay) {
  CRGB c = CRGB(16,16,16);
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      setAll(z, s, c);
      addGlitter(z, s, 90);
    }
  }
  showStrip();
  delay(SparkleDelay);
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      setAll(z, s, c);
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void runningLights(int WaveDelay) {
  static unsigned int offset = 0;
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      
      for ( int i=0; i<ledsPerSection; i++ ) {
        unsigned int level = sin8( (i*1280/ledsPerSection + offset) & 0xFF );  // 5 waves
        setPixel(z, sectionStart[z][s] + i, CHSV(gHue, 255, level));
      }
    }
  }
  
  showStrip();
  delay(WaveDelay);

  gHue++;
  offset = (offset + 21) & 0xFF;
}

//------------------------------------------------------//
void colorWipe(int SpeedDelay, boolean Reverse) {
  static unsigned int pct = 0;  // starting position
  static boolean blank = false;
  CRGB c = CHSV(gHue, 255, 255);
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = sectionStart[z][s] + ((pct * ledsPerSection) / 100);
      if ( pct > 0 && ledsPerSection > 100 ) {
        for ( int i=sectionStart[z][s] + (((pct-1) * ledsPerSection) / 100); i<pos; i++ ) {
          setPixel(z, pos, blank ? CRGB::Black : c);
        }
      } else {
        if ( pos < sectionEnd[z][s] )
          setPixel(z, pos, blank ? CRGB::Black : c);
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( Reverse ) {
    if ( pct-- == 0 ) {
      pct = 100;
      blank = !blank;
      if ( blank )
        gHue += 8;
    }
  } else {
    if ( pct++ == 100 ) {
      pct = 0;
      blank = !blank;
      if ( blank )
        gHue += 8;
    }
  }
}

//------------------------------------------------------//
void colorChase(CRGB c[], int Size, int SpeedDelay, boolean Reverse) {
  int window = 3*Size;  // c[] has 3 elements
  int pos;
  
  // move by 1 pixel within window
  for ( int q=0; q<window; q++ ) {

    for ( int z=0; z<numZones; z++ ) {
      for ( int s=0; s<numSections[z]; s++ ) {
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

        setAll(z, s, c[2]); // clear each zone

        for ( int i=0; i<ledsPerSection; i+=window ) {
          // turn LEDs on: ..++++####....++++####..   (size=4, . = black, + = c1, # = c2)
          // turn LEDs on: ..####++++....####++++..   (size=4, . = black, + = c1, # = c2, Reverse)
          for ( int j=0; j<Size; j++ ) {
            if ( Reverse ) {
              pos = (window-q-1) + (Size-j-1);
              if ( (pos+Size)%window + i < ledsPerSection )
                setPixel(z, sectionStart[z][s] + (pos+Size)%window + i, c[0]);
              if ( pos%window + i < ledsPerSection )
                setPixel(z, sectionStart[z][s] + pos%window + i, c[1]);
            } else {
              pos = q + j;
              if ( pos%window + i < ledsPerSection )
                setPixel(z, sectionStart[z][s] + pos%window + i, c[0]);
              if ( (pos+Size)%window + i < ledsPerSection )
                setPixel(z, sectionStart[z][s] + (pos+Size)%window + i, c[1]);
            }
          }
        }
      }
    }
    showStrip();
    delay(SpeedDelay/Size);
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
      setAll(z, s, CRGB::Black); // clear each zone
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      
      for ( int i=0; i<ledsPerSection; i=i+3 ) {
        setPixel(z, sectionStart[z][s]+i+q, CHSV(((i * 256 / ledsPerSection) + gHue) & 255, 255, 255));
      }
    }
  }
  showStrip();
  delay(SpeedDelay);

  ++q %= 3;
  gHue++;
}

//------------------------------------------------------//
void rainbowCycle(int SpeedDelay) {

  for ( int z=0; z<numZones; z++ ) {
    fill_rainbow(leds[z], numLEDs[z], gHue);
/*
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      for ( int i=0; i<ledsPerSection; i++ ) {
        setPixel(z, sectionStart[z][s] + i, CHSV(((i * 256 / ledsPerSection) + (gHue & 0xFF)) & 255, 255, 255));
      }
    }
*/
  }
  showStrip();
  delay(SpeedDelay);
  gHue++;
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
        cooldown = random(0, ((Cooling * 10) / ledsPerSection) + 2);
        
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
      if( random(255) < Sparking ) {
        int y = random(7);
        if ( s%2 == 1 ) {
          heat[z][sectionStart[z][s]+ledsPerSection-y-1] = heat[z][sectionStart[z][s]+ledsPerSection-y-1] + random(160,255);
        } else {
          heat[z][sectionStart[z][s]+y] = heat[z][sectionStart[z][s]+y] + random(160,255);
        }
      }
    
      // Step 4.  Convert heat to LED colors
      for ( int j=sectionStart[z][s]; j<sectionEnd[z][s]; j++ ) {
        setPixel(z, j, HeatColor(heat[z][j]));  // from FastLED library
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
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
      for ( int s=0; s<numSections[z]; s++ ) {
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
        setAll(z, s, CRGB::Black);  // reset all pixels in zone
       
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
          setPixel(z, sectionStart[z][s]+Position[i],colors[i]);
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
void meteorRain(byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  static unsigned int pct = 0;  // starting position
  
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      unsigned int pos = sectionStart[z][s] + ((pct * ledsPerSection+2*meteorSize) / 100);

      // fade brightness all LEDs one step
      for ( int j=0; j<ledsPerSection; j++ ) {
        if ( (!meteorRandomDecay) || (random(10)>5) ) {
          fadeToBlack(z, sectionStart[z][s]+j, meteorTrailDecay);
        }
      }

      for ( int j=0; j<meteorSize; j++ ) {
        if ( (pos-j < sectionEnd[z][s]) && (pos-j >= sectionStart[z][s]) ) {
          setPixel(z, pos-j, CRGB::White);
        } 
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);

  if ( pct++ == 100 ) {
    pct = 0;
    delay(100); // add a bit of delay between meteors
  }
}

// ***************************************
// ** Common Functions **
// ***************************************

// Set a LED color
void setPixel(int zone, int pixel, CRGB c) {
   leds[zone][pixel] = c;
}

// Set all LEDs to a given color
void setAll(int zone, int section, CRGB c) {
  for ( int p=sectionStart[zone][section]; p<sectionEnd[zone][section]; p++ ) {
    setPixel(zone, p, c); 
  }
}

// used by meteorrain
void fadeToBlack(int zone, int ledNo, byte fadeValue) {
   // FastLED
   leds[zone][ledNo].fadeToBlackBy( fadeValue );  // 64=25%
}

void addGlitter(int zone, int section, fract8 chanceOfGlitter) {
  if ( random8() < chanceOfGlitter ) {
    leds[zone][ random16(sectionStart[zone][section],sectionEnd[zone][section]) ] += CRGB::White;
  }
}
