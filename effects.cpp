#include "FastLED.h"
#include "effects.h"


byte *heat = NULL; // Fire effect static data


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
    r = (k/256.0)*c.red;
    g = (k/256.0)*c.green;
    b = (k/256.0)*c.blue;
    setAll(CRGB(r,g,b));
    showStrip();
  }
  delay(5);
     
  for ( int k=255; k>=0; k--) {
    r = (k/256.0)*c.red;
    g = (k/256.0)*c.green;
    b = (k/256.0)*c.blue;
    setAll(CRGB(r,g,b));
    showStrip();
  }
}

//------------------------------------------------------//
void Strobe(CRGB c, int FlashDelay) {
  setAll(c);
  showStrip();
  delay(FlashDelay/2);
  setAll(CRGB::Black);
  showStrip();
  delay(FlashDelay/2);
}

//------------------------------------------------------//
void HalloweenEyes(CRGB c, int EyeWidth, int EyeSpace, boolean Fade) {
  int i;
  int StartPoint = random( 0, numLEDs - (2*EyeWidth) - EyeSpace );
  int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

  if ( Fade == true )
    fadeToBlackBy(leds, numLEDs, 196);
  else
    setAll(CRGB::Black); // Set all black

  // show eyes
  for ( i=0; i<EyeWidth; i++ ) {
    setPixel(StartPoint + i, c);
    setPixel(Start2ndEye + i, c);
  }
  showStrip();
  
  if ( Fade != true ) {
    // blink
    delay(random(250,750));
    for ( i=0; i<EyeWidth; i++ ) {
      setPixel(StartPoint + i, CRGB::Black);
      setPixel(Start2ndEye + i, CRGB::Black);
    }
    showStrip();
    delay(random(50,150));
  
    // show eyes
    for(i = 0; i < EyeWidth; i++) {
      setPixel(StartPoint + i, c);
      setPixel(Start2ndEye + i, c);
    }
    showStrip();
    delay(random(500,2500));
  }
}

//------------------------------------------------------//
void CylonBounce(int EyeSize, int SpeedDelay) {
  LeftToRight(CRGB::Red, EyeSize, SpeedDelay, false);
  RightToLeft(CRGB::Red, EyeSize, SpeedDelay, false);
/*
  setAll(CRGB::Black);
  for ( int i=0; i < numLEDs-EyeSize-2; i++ ) {
    for ( int j=1; j<=EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    if ( i>0 )
      setPixel(i-1, CRGB::Black);
    fadeToBlack(i, 32);
    fadeToBlack(i+EyeSize+1, 32);
    showStrip();
    delay(SpeedDelay);
  }

  setAll(CRGB::Black);
  for ( int i=numLEDs-EyeSize-2; i>0; i-- ) {
    for ( int j=1; j<=EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    fadeToBlack(i, 32);
    fadeToBlack(i+EyeSize+1, 32);
    if ( i<numLEDs-EyeSize-2 )
      setPixel(i+EyeSize+2, CRGB::Black);
    showStrip();
    delay(SpeedDelay);
  }
*/
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

  setAll(CRGB::Black);
  for ( int i =((numLEDs-EyeSize)/2); i>=0; i-- ) {

    if ( Fade==true ) {
      // fade brightness of all LEDs in one step by 25%
      fadeToBlackBy(leds, numLEDs, 64);
    } else {
      setAll(CRGB::Black);
    }
    
    setPixel(i, c/10);
    for ( int j = 1; j <= EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    setPixel(i+EyeSize+1, c/10);
    
    setPixel(numLEDs-i, c/10);
    for ( int j = 1; j <= EyeSize; j++ ) {
      setPixel(numLEDs-i-j, c); 
    }
    setPixel(numLEDs-i-EyeSize-1, c/10);
    
    showStrip();
    delay(SpeedDelay);
  }
}

// used by NewKITT
void OutsideToCenter(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  setAll(CRGB::Black);
  for ( int i = 0; i<=((numLEDs-EyeSize)/2); i++ ) {

    if ( Fade==true ) {
      // fade brightness of all LEDs in one step by 25%
      fadeToBlackBy(leds, numLEDs, 64);
    } else {
      setAll(CRGB::Black);
    }
    
    setPixel(i, c/4);
    for ( int j = 1; j <= EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    setPixel(i+EyeSize+1, c/4);
    
    setPixel(numLEDs-i, c/4);
    for ( int j = 1; j <= EyeSize; j++ ) {
      setPixel(numLEDs-i-j, c); 
    }
    setPixel(numLEDs-i-EyeSize-1, c/4);
    
    showStrip();
    delay(SpeedDelay);
  }
}

// used by NewKITT
void LeftToRight(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  setAll(CRGB::Black);
  for ( int i=0; i < numLEDs-EyeSize-2; i++ ) {

    if ( Fade==true ) {
      // fade brightness of all LEDs in one step by 25%
      fadeToBlackBy(leds, numLEDs, 64);
    } else {
      setAll(CRGB::Black);
    }
    
    setPixel(i, c/4);
    for ( int j=1; j <= EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    setPixel(i+EyeSize+1, c/4);

    showStrip();
    delay(SpeedDelay);
  }
}

// used by NewKITT
void RightToLeft(CRGB c, int EyeSize, int SpeedDelay, boolean Fade) {

  setAll(CRGB::Black);
  for ( int i=numLEDs-EyeSize-2; i>0; i-- ) {

    if ( Fade==true ) {
      // fade brightness of all LEDs in one step by 25%
      fadeToBlackBy(leds, numLEDs, 64);
    } else {
      setAll(CRGB::Black);
    }
    
    setPixel(i, c/4);
    for ( int j=1; j<=EyeSize; j++ ) {
      setPixel(i+j, c); 
    }
    setPixel(i+EyeSize+1, c/4);

    showStrip();
    delay(SpeedDelay);
  }
}

//------------------------------------------------------//
void Twinkle(CRGB c, int SpeedDelay, boolean OnlyOne) {
  fadeToBlackBy(leds, numLEDs, 32);  // 12.5% fade
  int pos = random(numLEDs-1);
  setPixel(pos,c);
  showStrip();
  delay(SpeedDelay);
  if ( OnlyOne ) { 
    setAll(CRGB::Black); 
  }
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
  fadeToBlackBy(leds, numLEDs, 20);
  int pos = beatsin16(13, 0, numLEDs-1);
  leds[pos] = c;
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
  for ( int i=0; i<numLEDs; i++ ) { //9948
    leds[i] = ColorFromPalette(palette, Hue+(i*2), beat-Hue+(i*10));
  }
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
// borrowed from FastLED's DemoReel
void juggle(int SpeedDelay) {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy(leds, numLEDs, 64);
  byte dothue = 0;
  for ( int i=0; i<8; i++ ) {
    leds[beatsin16(i+7, 0, numLEDs-1)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
  showStrip();
  delay(SpeedDelay);
}


//------------------------------------------------------//
void snowSparkle(int SparkleDelay, int SpeedDelay) {
  CRGB c = CRGB(32,32,32);
  setAll(c);
  addGlitter(90);
  showStrip();
  delay(SparkleDelay);
  setAll(c);
  showStrip();
  delay(SpeedDelay);
}

//------------------------------------------------------//
void runningLights(CRGB c, int WaveDelay) {
  static int Position=0;
  
//  for ( int j=0; j<numLEDs*2; j++ ) {
//    Position++; // = 0; //Position + Rate;
    for ( int i=0; i<numLEDs; i++ ) {
      // sine wave, 3 offset waves make a rainbow!
      float level = (sin(i+Position) * 127 + 128) / 255;
      setPixel(i,CRGB(level*c.r,level*c.g,level*c.b));
    }
    showStrip();
    if ( Position++ == numLEDs ) {
      Position = 0;
    }
    delay(WaveDelay);
//  }
}

//------------------------------------------------------//
void colorWipe(CRGB c, boolean Reverse, int SpeedDelay) {
  for ( int i=0; i<numLEDs; i++ ) {
    if ( Reverse ) {
      setPixel(numLEDs-i-1, c);
    } else {
      setPixel(i, c);
    }
    showStrip();
    delay(SpeedDelay);
  }
}

//------------------------------------------------------//
void colorChase(CRGB c[], int Size, boolean Reverse, int SpeedDelay) {
  int window = 3*Size;
  int pos;
  
  // move by 1 pixel within window
  for ( int q=0; q<window; q++ ) {
    setAll(c[2]);
    for ( int i=0; i<numLEDs; i+=window ) {
      // turn LEDs on: ..++++####....++++####..   (size=4, . = black, + = c1, # = c2)
      // turn LEDs on: ..####++++....####++++..   (size=4, . = black, + = c1, # = c2, Reverse)
      for ( int j=0; j<Size; j++ ) {
        if ( Reverse ) {
          pos = (window-q-1) + (Size-j-1);
          if ( (pos+Size)%window + i < numLEDs )
            setPixel((pos+Size)%window + i, c[0]);
          if ( pos%window + i < numLEDs )
            setPixel(pos%window + i, c[1]);
        } else {
          pos = q + j;
          if ( pos%window + i < numLEDs )
            setPixel(pos%window + i, c[0]);
          if ( (pos+Size)%window + i < numLEDs )
            setPixel((pos+Size)%window + i, c[1]);
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
void rainbowCycle(int Hue, int SpeedDelay) {
  CRGB c;

  // depend on caller to do the hue shift
//  for ( int Hue=0; Hue<256; Hue++ ) { // cycle of all colors on wheel
    for ( int i=0; i<numLEDs; i++ ) {
      c = CHSV(((i * 256 / numLEDs) + Hue) & 255, 255, 255);
      setPixel(i, c);
    }
    showStrip();
    delay(SpeedDelay);
//  }
}

//------------------------------------------------------//
void theaterChase(CRGB c, int SpeedDelay) {
  for ( int q=0; q < 3; q++ ) {
    setAll(CRGB::Black);
    for ( int i=0; i<numLEDs; i=i+3 ) {
      setPixel(i+q, c);        //turn every third pixel on
    }
    showStrip();
    delay(SpeedDelay);
  }
}

//------------------------------------------------------//
void theaterChaseRainbow(int Hue, int SpeedDelay) {
  CRGB c;

  //depend on calller to do the hue shift
//  for ( int Hue=0; Hue<256; Hue++ ) {     // cycle all 256 colors in the wheel
    for ( int q=0; q<3; q++ ) {
      setAll(CRGB::Black);
      for ( int i=0; i<numLEDs; i=i+3 ) {
        c = CHSV(((i * 256 / numLEDs) + Hue) & 255, 255, 255);
        setPixel(i+q, c);        //turn every third pixel on
      }
      showStrip();
      delay(SpeedDelay);
    }
//  }
}

//------------------------------------------------------//
void Fire(int Cooling, int Sparking, int SpeedDelay, boolean SplitOnLong, int SplitPoint) {
  //static byte heat[NUM_LEDS]; // moved to setup()
  int cooldown;
  int nLeds;

  // if we have a long LED strip make fire at both ends
  if ( SplitOnLong == true && numLEDs > SplitPoint ) {
    nLeds = numLEDs / 2;
  } else {
    nLeds = numLEDs;
  }
  
  // Step 1.  Cool down every cell a little
  for ( int i=0; i<numLEDs; i++ ) {
    cooldown = random(0, ((Cooling * 10) / nLeds) + 2);
    
    if ( cooldown>heat[i] ) {
      heat[i]=0;
    } else {
      heat[i]=heat[i]-cooldown;
    }
  }
  
  // Step 2.  Heat from each cell drifts 'up' and diffuses a little
  for ( int k=nLeds-1; k>=2; k-- ) {
    heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2]) / 3;
    if ( k==2 )
      heat[k-1] = heat[k - 2] / 2;
    if ( SplitOnLong == true && numLEDs > SplitPoint ) {
      int j = numLEDs - k - 1;
      heat[j] = (heat[j+1] + heat[j+2] + heat[j+2]) / 3;
    }
  }
    
  // Step 3.  Randomly ignite new 'sparks' near the bottom
  if( random(255) < Sparking ) {
    int y = random(7);
    heat[y] = heat[y] + random(160,255);
    //heat[y] = random(160,255);
    if ( SplitOnLong == true && numLEDs > SplitPoint ) {
      y = random(7);
      heat[numLEDs-y-1] = heat[numLEDs-y-1] + random(160,255);
    }
  }

  // Step 4.  Convert heat to LED colors
  for ( int j=0; j<numLEDs; j++ ) {
    setPixelHeatColor(j, heat[j]);
  }

  showStrip();
  delay(SpeedDelay);
}

void setPixelHeatColor(int Pixel, byte temperature) {
  // Scale 'heat' down from 0-255 to 0-191
  byte t192 = round((temperature/255.0)*191);
 
  // calculate ramp up from
  byte heatramp = t192 & 0x3F; // 0..63
  heatramp <<= 2; // scale up to 0..252
 
  // figure out which third of the spectrum we're in:
  if( t192 > 0x80) {                     // hottest
    setPixel(Pixel, CRGB(255, 255, heatramp));
  } else if( t192 > 0x40 ) {             // middle
    setPixel(Pixel, CRGB(255, heatramp, 0));
  } else {                               // coolest
    setPixel(Pixel, CRGB(heatramp, 0, 0));
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
      Position[i] = round( Height[i] * (numLEDs - 1) / StartHeight);
    }

    ballsStillBouncing = false; // assume no balls bouncing
    for ( int i=0; i<BallCount; i++ ) {
      setPixel(Position[i],colors[i]);
      if ( ballBouncing[i] ) {
        ballsStillBouncing = true;
      }
    }
    
    showStrip();
    setAll(CRGB::Black);  // reset all pixels
  }
}

//------------------------------------------------------//
void meteorRain(CRGB c, byte meteorSize, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  

  setAll(CRGB::Black);  // reset all pixels
  
  for ( int i=0; i<2*numLEDs; i++ ) {

    // fade brightness all LEDs one step
    for ( int j=0; j<numLEDs; j++ ) {
      if ( (!meteorRandomDecay) || (random(10)>5) ) {
        fadeToBlack(j, meteorTrailDecay );        
      }
    }
    
    // draw meteor
    for ( int j=0; j<meteorSize; j++ ) {
      if ( ( i-j <numLEDs) && (i-j>=0) ) {
        setPixel(i-j, c);
      } 
    }
    
    showStrip();
    delay(SpeedDelay);
  }
}

// ***************************************
// ** Common Functions **
// ***************************************

// Set a LED color
void setPixel(int Pixel, CRGB c) {
   leds[Pixel] = c;
}

// Set all LEDs to a given color
void setAll(CRGB c) {
  for ( int i=0; i<numLEDs; i++ ) {
    setPixel(i, c); 
  }
}

// used by meteorrain
void fadeToBlack(int ledNo, byte fadeValue) {
   // FastLED
   leds[ledNo].fadeToBlackBy( fadeValue );  // 64=25%
}

void addGlitter(fract8 chanceOfGlitter) {
  if ( random8() < chanceOfGlitter ) {
    leds[ random16(numLEDs) ] += CRGB::White;
  }
}
