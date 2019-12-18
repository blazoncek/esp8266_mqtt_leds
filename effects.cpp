#include "FastLED.h"
#include "effects.h"

boolean breakEffect = false;
byte *heat[MAXZONES]; // Fire effect static data (for each zone; max 8)


// *************************
// ** LEDEffect Functions **
// *************************

//------------------------------------------------------//
void RGBLoop() {
  FadeInOut(CRGB::Red);
  FadeInOut(CRGB::Green);
  FadeInOut(CRGB::Blue);
}

//------------------------------------------------------//
void FadeInOut(CRGB c){
  float r, g, b;

  for ( int k=0; k<256; k++ ) { 
    if ( breakEffect ) return;

    r = (k/255.0)*c.red;
    g = (k/255.0)*c.green;
    b = (k/255.0)*c.blue;

    for ( int z=0; z<numZones; z++ ) {
      for ( int s=0; s<numSections[z]; s++ ) {
        setAll(z, s, CRGB(r,g,b));
      }
    }
    showStrip();
  }
  delay(5);
     
  for ( int k=255; k>=0; k--) {
    if ( breakEffect ) return;

    r = (k/255.0)*c.red;
    g = (k/255.0)*c.green;
    b = (k/255.0)*c.blue;

    for ( int z=0; z<numZones; z++ ) {
      for ( int s=0; s<numSections[z]; s++ ) {
        setAll(z, s, CRGB(r,g,b));
      }
    }
    showStrip();
  }
  delay(5);
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
  LeftToRight(CRGB::Red, EyeSize, SpeedDelay, false);
  RightToLeft(CRGB::Red, EyeSize, SpeedDelay, false);
}

//------------------------------------------------------//
void NewKITT(CRGB c, int EyeSize, int SpeedDelay){
  RightToLeft(c, EyeSize, SpeedDelay);
  LeftToRight(c, EyeSize, SpeedDelay);
  OutsideToCenter(c, EyeSize, SpeedDelay);
  CenterToOutside(c, EyeSize, SpeedDelay);
  LeftToRight(c, EyeSize, SpeedDelay);
  RightToLeft(c, EyeSize, SpeedDelay);
  OutsideToCenter(c, EyeSize, SpeedDelay);
  CenterToOutside(c, EyeSize, SpeedDelay);
}

// used by NewKITT
void CenterToOutside(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      for ( int i=(ledsPerSection-EyeSize)/2; i>=0; i-- ) {
        if ( breakEffect ) return;
    
        if ( Fade==true ) {
          // fade brightness of all LEDs in one step by 25%
          fadeToBlack(z, sectionStart[z][s]+i, 64);
          fadeToBlack(z, sectionEnd[z][s]-i, 64);
        } else {
          setPixel(z, sectionStart[z][s]+i, CRGB::Black);
          setPixel(z, sectionEnd[z][s]-i, CRGB::Black);
        }
        
        setPixel(z, sectionStart[z][s]+i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, sectionStart[z][s]+i+j, c); 
        }
        setPixel(z, sectionStart[z][s]+i+EyeSize+1, c/4);
        
        setPixel(z, sectionEnd[z][s]-i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, sectionEnd[z][s]-i-j, c); 
        }
        setPixel(z, sectionEnd[z][s]-i-EyeSize-1, c/4);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

// used by NewKITT
void OutsideToCenter(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      for ( int i=0; i <= (ledsPerSection-EyeSize)/2; i++ ) {
        if ( breakEffect ) return;
    
        if ( Fade==true ) {
          // fade brightness of all LEDs in one step by 25%
          fadeToBlack(z, sectionStart[z][s]+i, 64);
          fadeToBlack(z, sectionEnd[z][s]-i, 64);
        } else {
          setPixel(z, sectionStart[z][s]+i, CRGB::Black);
          setPixel(z, sectionEnd[z][s]-i, CRGB::Black);
        }
        
        setPixel(z, sectionStart[z][s]+i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, sectionStart[z][s]+i+j, c); 
        }
        setPixel(z, sectionStart[z][s]+i+EyeSize+1, c/4);
        
        setPixel(z, sectionEnd[z][s]-i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, sectionEnd[z][s]-i-j, c); 
        }
        setPixel(z, sectionEnd[z][s]-i-EyeSize-1, c/4);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

// used by NewKITT
void LeftToRight(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      for ( int i=sectionStart[z][s]; i < sectionEnd[z][s]-EyeSize-2; i++ ) {
        if ( breakEffect ) return;
    
        if ( Fade==true ) {
          // fade brightness of all LEDs in one step by 25%
          fadeToBlack(z, i, 64);
        } else {
          setPixel(z, i, CRGB::Black);
        }
        
        setPixel(z, i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, i+j, c); 
        }
        setPixel(z, i+EyeSize+1, c/4);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

// used by NewKITT
void RightToLeft(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      for ( int i=sectionEnd[z][s]-EyeSize-2; i>sectionStart[z][s]; i-- ) {
        if ( breakEffect ) return;
    
        if ( Fade==true ) {
          // fade brightness of all LEDs in one step by 25%
          fadeToBlack(z, i, 64);
        } else {
          setPixel(z, i, CRGB::Black);
        }
        
        setPixel(z, i, c/4);
        for ( int j=1; j<=EyeSize; j++ ) {
          setPixel(z, i+j, c); 
        }
        setPixel(z, i+EyeSize+1, c/4);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void Twinkle(CRGB c, int SpeedDelay, boolean OnlyOne) {

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
      setPixel(z, pos, c);
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void TwinkleRandom(int Count, int SpeedDelay, boolean OnlyOne) {

  for ( int i=0; i<Count; i++ ) {
    Twinkle(CHSV(random(255),random(64,255),255), SpeedDelay, OnlyOne);
  }
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void sinelon(CRGB c, int SpeedDelay) {

  // a colored dot sweeping back and forth, with fading trails
  for ( int z=0; z<numZones; z++ ) {
    fadeToBlackBy(leds[z], numLEDs[z], 16);
    for ( int s=0; s<numSections[z]; s++ ) {
      int pos = beatsin16(13, sectionStart[z][s], sectionEnd[z][s]-1);
      leds[z][pos] = c;
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void bpm(int Hue, int SpeedDelay) {

  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8(BeatsPerMinute, 64, 255);
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) { //9948
        leds[z][i] = ColorFromPalette(palette, Hue+((i-sectionStart[z][s])*2), beat-Hue+((i-sectionStart[z][s])*10));
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
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
void runningLights(CRGB c, int WaveDelay) {
  static int Position = 0;
  
  for ( int z=0; z<numZones; z++ ) {
    Position = 0;
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      
      for ( int i=0; i<ledsPerSection; i++ ) {
        // sine wave, 3 offset waves make a rainbow!
        float level = (sin(i+Position) + 1.0) / 2.2;
        setPixel(z, sectionStart[z][s] + i, CRGB(level*c.r,level*c.g,level*c.b));
      }
      if ( Position++ >= ledsPerSection ) {
        Position = 0;
      }
    }
  }
  showStrip();
  delay(WaveDelay);
}

//------------------------------------------------------//
void colorWipe(CRGB c, boolean Reverse, int SpeedDelay) {
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];

      for ( int i=0; i<ledsPerSection; i++ ) {
        if ( breakEffect ) return;
        if ( Reverse ) {
          setPixel(z, sectionEnd[z][s] - i, c);
        } else {
          setPixel(z, sectionStart[z][s] + i, c);
        }
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void colorChase(CRGB c[], int Size, boolean Reverse, int SpeedDelay) {
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
              pos = sectionStart[z][s] + (window-q-1) + (Size-j-1);
              if ( (pos+Size)%window + i < ledsPerSection )
                setPixel(z, (pos+Size)%window + i, c[0]);
              if ( pos%window + i < ledsPerSection )
                setPixel(z, pos%window + i, c[1]);
            } else {
              pos = sectionStart[z][s] + q + j;
              if ( pos%window + i < ledsPerSection )
                setPixel(z, pos%window + i, c[0]);
              if ( (pos+Size)%window + i < ledsPerSection )
                setPixel(z, (pos+Size)%window + i, c[1]);
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
void christmasChase(int Size, boolean Reverse, int SpeedDelay) {
  CRGB c[3] = {CRGB::Red, CRGB::Green, CRGB::White};
  colorChase(c, Size, Reverse, SpeedDelay);
}

//------------------------------------------------------//
void theaterChase(CRGB cColor, int SpeedDelay) {
  CRGB c[3] = {cColor, CRGB::Black, CRGB::Black};
  colorChase(c, 1, false, SpeedDelay);
}

//------------------------------------------------------//
void rainbowChase(int SpeedDelay) {
  static int Hue = 0;
  CRGB c;

  //each call shifts the hue a bit
  ++Hue &= 0xFF;
  for ( int q=0; q<3; q++ ) {
    for ( int z=0; z<numZones; z++ ) {
      for ( int s=0; s<numSections[z]; s++ ) {
        setAll(z, s, CRGB::Black); // clear each zone
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
        
        for ( int i=0; i<ledsPerSection; i=i+3 ) {
          c = CHSV(((i * 256 / ledsPerSection) + Hue) & 255, 255, 255);
          setPixel(z, sectionStart[z][s]+i+q, c);        //turn every third pixel on
        }
      }
      showStrip();
      delay(SpeedDelay);
    }
  }
}

//------------------------------------------------------//
void rainbowCycle(int SpeedDelay) {
  static int Hue = 0;
  CRGB c;

  //each call shifts the hue a bit
  ++Hue &= 0xFF;
  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
    
      for ( int i=0; i<ledsPerSection; i++ ) {
        c = CHSV(((i * 256 / ledsPerSection) + Hue) & 255, 255, 255);
        setPixel(z, sectionStart[z][s] + i, c);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
// borrowed from FastLED demo
void Fire(int Cooling, int Sparking, int SpeedDelay, boolean SplitOnLong, int SplitPoint) {
  int cooldown;
  int nLeds, ledsPerSection;

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
    
      // if we have a long LED strip make fire at both ends
      if ( SplitOnLong == true && sectionEnd[z][s]-sectionStart[z][s] > SplitPoint ) {
        nLeds = sectionEnd[z][s]-sectionStart[z][s] / 2;
      } else {
        nLeds = sectionEnd[z][s]-sectionStart[z][s];
      }
      
      // Step 1.  Cool down every cell a little
      for ( int i=sectionStart[z][s]; i<sectionEnd[z][s]; i++ ) {
        cooldown = random(0, ((Cooling * 10) / nLeds) + 2);
        
        if ( cooldown>heat[z][i] ) {
          heat[z][i]=0;
        } else {
          heat[z][i]=heat[z][i]-cooldown;
        }
      }
      
      // Step 2.  Heat from each cell drifts 'up' and diffuses a little
      for ( int k=nLeds-1; k>=2; k-- ) {
        heat[z][sectionStart[z][s]+k] = (heat[z][sectionStart[z][s]+k - 1] + heat[z][sectionStart[z][s]+k - 2] + heat[z][sectionStart[z][s]+k - 2]) / 3;
        if ( k==2 )
          heat[z][sectionStart[z][s]+k-1] = heat[z][sectionStart[z][s]+k - 2] / 2;
        if ( SplitOnLong == true && ledsPerSection > SplitPoint ) {
          int j = ledsPerSection - k - 1;
          heat[z][j] = (heat[z][j+1] + heat[z][j+2] + heat[z][j+2]) / 3;
        }
      }
        
      // Step 3.  Randomly ignite new 'sparks' near the bottom
      if( random(255) < Sparking ) {
        int y = random(7);
        heat[z][y] = heat[z][y] + random(160,255);
        if ( SplitOnLong == true && ledsPerSection > SplitPoint ) {
          y = random(7);
          heat[z][ledsPerSection-y-1] = heat[z][ledsPerSection-y-1] + random(160,255);
        }
      }
    
      // Step 4.  Convert heat to LED colors
      for ( int j=sectionStart[z][s]; j<sectionEnd[z][s]; j++ ) {
        setPixelHeatColor(z, j, heat[z][j]);
      }
    }
  }
  showStrip();
  delay(SpeedDelay);
}

void setPixelHeatColor(int zone, int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    setPixel(zone, Pixel, CRGB(255, 255, heatramp));
  } else if( t192 > 0x40 ) {             // middle
    setPixel(zone, Pixel, CRGB(255, heatramp, 0));
  } else {                               // coolest
    setPixel(zone, Pixel, CRGB(heatramp, 0, 0));
  }
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
void meteorRain(CRGB c, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  

  for ( int z=0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s];
      
      for ( int i=0; i<ledsPerSection+2*meteorSize; i++ ) {
        if ( breakEffect ) return;
    
        // fade brightness all LEDs one step
        for ( int j=0; j<ledsPerSection; j++ ) {
          if ( (!meteorRandomDecay) || (random(10)>5) ) {
            fadeToBlack(z, sectionStart[z][s]+j, meteorTrailDecay );        
          }
        }
        
        // draw meteor
        for ( int j=0; j<meteorSize; j++ ) {
          if ( (i-j < ledsPerSection) && (i-j >= 0) ) {
            setPixel(z, sectionStart[z][s]+i-j, c);
          } 
        }
      }
    }
  }
  
  showStrip();
  delay(SpeedDelay);
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
