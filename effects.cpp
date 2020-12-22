#include <stdint.h>
#include <math.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>          // Base ESP8266 includes
#include <FastLED.h>


#include "eepromdata.h"
#include "effects.h"
#include "boblight.h"


// This is an array of leds.  One item for each led in your strip.
extern CRGB *leds[], gRGB;
extern uint8_t gHue, gBrightness;

boolean volatile breakEffect = false;
byte *heat[MAXZONES]; // Fire effect static data (for each zone; max 8); allocated in setup()

typedef void (*NewKITTPatternList[])(CRGB, int, uint8_t, boolean);
static NewKITTPatternList gPatterns = {
  LeftToRight,
  RightToLeft,
  OutsideToCenter,
  CenterToOutside,
  RightToLeft,
  LeftToRight,
  OutsideToCenter,
  CenterToOutside
  };
static uint8_t gCurrentPattern = 0;
static uint8_t gBeat = 0;


effect_name_t effects[] = {
  {OFF,                  "Off"},
  {SOLID,                "Solid Color"},
  {FADEINOUT,            "Fade In&Out"},
  {STROBE,               "Strobe"},
  {HALLOWEENEYES,        "Haloween Eyes"},
  {CYLONBOUNCE,          "Cylon Bounce"},
  {NEWKITT,              "New KITT"},
  {TWINKLE,              "Twinkle"},
  {TWINKLERANDOM,        "Twinkle random"},
  {SPARKLE,              "Sparkle"},
  {SNOWSPARKLE,          "Snow Sparkle"},
  {RUNNINGLIGHTS,        "Running lights"},
  {COLORWIPE,            "Color Wipe"},
  {RAINBOWCYCLE,         "Rainbow Cycle"},
  {THEATRECHASE,         "Theatre Chase"},
  {RAINBOWCHASE,         "Rainbow Chase"},
  {FIRE,                 "Fire"},
  {GRADIENTCYCLE,        "Gradient Cycle"},
  {BOUNCINGCOLOREDBALLS, "3 bouncing balls"},
  {METEORRAIN,           "Meteor"},
  {SINELON,              "SineLon"},
  {BPM,                  "Pusling Colors"},
  {JUGGLE,               "8 juggling dots"},
  {COLORCHASE,           "Color Chase"},
  {CHRISTMASCHASE,       "Christmass Chase"},
  {RAINBOWBOUNCE,        "Rainbow Bounce"}
};


// *************************
// ** LEDEffect Functions **
// *************************

//------------------------------------------------------//
void solidColor(CRGB c) {
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
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
  int v = triwave8(beat8(10));
  CRGB c = CHSV(gHue, 255, v);
  if ( v<2 ) gHue += 8;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(15);
}

//------------------------------------------------------//
void Strobe(CRGB c, int FlashDelay) {

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
  }
  showStrip();
  FastLED.delay(FlashDelay/2);
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
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
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 196);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      int StartPoint = random16(sectionStart[z][s], sectionEnd[z][s] - (2*EyeWidth) - EyeSpace);
      int Start2ndEye = StartPoint + EyeWidth + EyeSpace;

      // show eyes
      for ( i=0; i<EyeWidth; i++ ) {
        leds[z][StartPoint + i] = leds[z][Start2ndEye + i] = c;
      }
    }
  }
  showStrip();
  FastLED.delay(15);
}

//------------------------------------------------------//
void eyeBounce(CRGB c, int EyeSizePct, int SweepsPerMinute) {
  static boolean dir = false;   // start left-to-right
  static unsigned int pct = 0;  // starting position
  
  uint8_t beat = round((float)beat8(SweepsPerMinute)*100/255);
  if ( pct == beat ) {  // if there is no change in position just exit
    FastLED.delay(15);
    return;
  } else if ( pct > beat ) {  // roll-over, erase the section
    dir = !dir;
  }
  pct = beat; // percent fill

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], CRGB::Black);
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      unsigned int EyeSize = max(1,(int)(EyeSizePct * ledsPerSection / 100));
      unsigned int pos = sectionStart[z][s] + getPosFromPct(pct, ledsPerSection, EyeSize, (s%2 ? !dir : dir));

      // if the color is black fill rainbow
      if ( c.r==0 && c.g==0 && c.b==0 ) {
        int delta = (s%2?-1:1) * max(1,255/(int)EyeSize);
        uint8_t hue = (s%2?delta:0) /*+ gHue*/;
        fill_rainbow(&leds[z][pos], EyeSize, hue, delta);
      } else {
        fill_solid(&leds[z][pos], EyeSize, c);
        leds[z][pos] = leds[z][pos+EyeSize-1] = c/8;
      }
    }
  }
  showStrip();
  FastLED.delay(15);
}

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
void nextPattern() {
  gCurrentPattern = (gCurrentPattern + 1) % ARRAY_SIZE(gPatterns);
  gHue += 4;
}

//------------------------------------------------------//
void NewKITT(int EyeSizePct, int SweepsPerMinute) {
  CRGB c = CHSV(gHue, 255, 255);
  uint8_t newB;
  
  if ( gPatterns[gCurrentPattern]==CenterToOutside || gPatterns[gCurrentPattern]==OutsideToCenter ) {
    newB = beat8(2*SweepsPerMinute);
  } else {
    newB = beat8(SweepsPerMinute);
  }

  if ( newB <= gBeat ) {
    nextPattern();
  }
  // Call the current pattern function once, updating the 'leds' array
  gPatterns[gCurrentPattern](c, EyeSizePct, newB, true);
  gBeat = newB;
  FastLED.delay(15);
}

// used by NewKITT
void CenterToOutside(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade) {
  unsigned int EyeSize;
  unsigned int pct = 50 - (beat*50)/255;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][sectionStart[z][s]+pos+j] = leds[z][sectionEnd[z][s]-pos-j] = c; 
      }
    }
  }
  showStrip();
}

// used by NewKITT
void OutsideToCenter(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade) {
  unsigned int EyeSize;
  unsigned int pct = (beat*50)/255;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = ((pct * (ledsPerSection-EyeSize)) / 100);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][sectionStart[z][s]+pos+j] = leds[z][sectionEnd[z][s]-pos-j+1] = c; 
      }
    }
  }
  showStrip();
}

// used by NewKITT
void LeftToRight(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade) {
  unsigned int EyeSize;
  unsigned int pct = (beat*100)/255;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = sectionStart[z][s] + getPosFromPct(pct, ledsPerSection, EyeSize, s%2);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][pos+j] = c; 
      }
    }
  }
  showStrip();
}

// used by NewKITT
void RightToLeft(CRGB c, int EyeSizePct, uint8_t beat, boolean Fade) {
  unsigned int EyeSize;
  unsigned int pct = 100 - (beat*100)/255;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( Fade )
      fadeToBlackBy(leds[z], numLEDs[z], 64);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      EyeSize = EyeSizePct * ledsPerSection / 100;
      unsigned int pos = sectionStart[z][s] + getPosFromPct(pct, ledsPerSection, EyeSize, s%2);

      for ( int j=0; j<EyeSize; j++ ) {
        leds[z][pos+j] = c; 
      }
    }
  }
  showStrip();
}

//------------------------------------------------------//
void Twinkle(int SpeedDelay, boolean OnlyOne) {
  CRGB c = CHSV(gHue, 255, 255);
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    if ( !OnlyOne )
      fadeToBlackBy(leds[z], numLEDs[z], 16);
    else
      fill_solid(leds[z], numLEDs[z], CRGB::Black);

    for ( int s=0; s<numSections[z]; s++ ) {
      int pos = random16(sectionStart[z][s], sectionEnd[z][s]+1);
      leds[z][pos] = c;
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
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fadeToBlackBy(leds[z], numLEDs[z], 16);
    for ( int s=0; s<numSections[z]; s++ ) {
      int pos = beatsin16(12, sectionStart[z][s], sectionEnd[z][s]);
      if ( s%2 )
        pos = (sectionEnd[z][s]-pos) + sectionStart[z][s];
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
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      for ( int i=sectionStart[z][s]; i<=sectionEnd[z][s]; i++ ) { //9948
        int hue = s%2 ? ((sectionEnd[z][s]-i)*2) : ((i-sectionStart[z][s])*2);
        int val = s%2 ? ((sectionEnd[z][s]-i)*10) : ((i-sectionStart[z][s])*10);
        leds[z][i] = ColorFromPalette(palette, gHue+hue, beat-gHue+val);
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
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fadeToBlackBy(leds[z], numLEDs[z], 64);
    for ( int s=0; s<numSections[z]; s++ ) {
      byte dothue = 0;
      for ( int i=0; i<8; i++ ) {
        int pos = beatsin16(i+7, sectionStart[z][s], sectionEnd[z][s]);
        if ( s%2 )
          pos = (sectionEnd[z][s]-pos) + sectionStart[z][s];
        leds[z][pos] |= CHSV(dothue, 255, 255);
        dothue += 32;
      }
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}


//------------------------------------------------------//
void Sparkle(int SpeedDelay, CRGB c) {

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    fill_solid(leds[z], numLEDs[z], c);
    for ( int s=0; s<numSections[z]; s++ ) {
      addGlitter(z, s, 75);
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
}

//------------------------------------------------------//
void snowSparkle(int SpeedDelay) {
  Sparkle(SpeedDelay, CRGB(16,16,16));
}

//------------------------------------------------------//
void runningLights(int WaveDelay) {
  static unsigned int offset = 0;
  
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      for ( int i=0; i<ledsPerSection; i++ ) {
        unsigned int level = sin8( (i*1280/ledsPerSection + offset) & 0xFF );  // 5 waves
        if ( s%2 ) {
          leds[z][sectionEnd[z][s] - i] = CHSV(gHue, 255, level);
        } else {
          leds[z][sectionStart[z][s] + i] = CHSV(gHue, 255, level);
        }
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

  uint8_t beat = (beat8(WipesPerMinute*2)+2)*100/255; // double the speed (colored & black wipes)
  if ( pct == beat ) {  // if there is no change in position just exit
    FastLED.delay(5);
    return;
  } else if ( pct > beat && pct > 99 ) {  // roll-over, erase the section
    blank = !blank;
    if ( blank ) gHue += 8; // slightly change the hue
  } else if ( pct > beat && pct < 100 ) {
    beat = 100;
  }
  pct = beat; // percent fill
  
  if ( blank ) {
    c = CRGB::Black;
  } else {
    c = CHSV(gHue, 255, 255);
  }
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      unsigned int nLeds = (pct * ledsPerSection) / 100;
      if ( !Reverse != !(bool)(s%2) ) { // Reverse XOR s==odd
        fill_solid(&leds[z][sectionEnd[z][s]-nLeds+1], nLeds, c);
      } else {
        fill_solid(&leds[z][sectionStart[z][s]], nLeds, c);
      }
    }
  }
  showStrip();
  FastLED.delay(30);
}

//------------------------------------------------------//
void colorChase(CRGB c[], int Size, int SpeedDelay, boolean Reverse) {
  uint8_t window = 3*Size;  // c[] has 3 elements
  static uint16_t q = 0;    // we assume we will never have window size > 256

  // move by 1 pixel on each run
  ++q;  // we will have occassional hick-up when the counter rolls over
  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {

    // we will repurpose Reverse for Rotating chase (xmas tree with multiple zig-zag sections)
    if ( Reverse ) {
      // fail-safes first
      if ( numSections[z] < 2 ) {
        // number of sections is smaller than minimum window size
        // we will not do a rotating chase
        Reverse = false;
      } else if ( numSections[z] % (3*Size) ) {
        // number of sections is not multiple of window size
        // we need to change size
        if ( numSections[z] % 3 ) {
          // number of sections is not multiple of 3
          // we will not do a rotating chase
          Reverse = false;
        } else {
          // change Size and adjust window
          Size = numSections[z] / 3;
          window = 3*Size;
        }
      }
    }

    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      int delta = (s%2?-1:1) * max(1,255/ledsPerSection);
      uint8_t hue = (s%2 ? (ledsPerSection-1)*abs(delta) : 0) + gHue;

      if ( c[0].getLuma()==0 && c[1].getLuma()==0 && c[2].getLuma()==0 ) {  // all black so we do a rainbow chase
        fill_rainbow(&leds[z][sectionStart[z][s]], ledsPerSection, hue, delta);
      } else {
        fill_solid(&leds[z][sectionStart[z][s]], ledsPerSection, c[2]);
      }

      for ( int i=0; i<ledsPerSection; i+=window ) {
        // turn LEDs on: ..++++####....++++####..   (size=4, . = c0, + = c1, # = c2)
        for ( int j=0; j<Size; j++ ) {
          uint16_t pos = (q%window) + j + (Reverse?s:0); // if rotating add offset by current section#

          // Every odd section is reversed (zig-zag mounted strips, xmas tree)
          if ( s%2 ) {
            if ( pos%window + i < ledsPerSection )
              leds[z][sectionEnd[z][s] - pos%window - i] = c[0];
            if ( (pos+Size)%window + i < ledsPerSection )
              leds[z][sectionEnd[z][s] - (pos+Size)%window - i] = c[1];
          } else {
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
  FastLED.delay(SpeedDelay);
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
  CRGB c[3] = {CRGB::Black, CRGB::Black, CRGB::Black};
  colorChase(c, 1, SpeedDelay, false);
  gHue++;
}

//------------------------------------------------------//
void gradientCycle(int SpeedDelay) {

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      CHSV sHue = (s%2) ? CHSV(gHue+64,255,255): CHSV(gHue,255,255);
      CHSV eHue = (s%2) ? CHSV(gHue,255,255): CHSV(gHue+64,255,255);
      fill_gradient<CRGB>(&leds[z][sectionStart[z][s]], ledsPerSection, sHue, eHue);
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
  gHue++;
}

//------------------------------------------------------//
void rainbowCycle(int SpeedDelay) {

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      int delta = (s%2?-1:1) * max(1,255/ledsPerSection);
      uint8_t hue = (s%2 ? (ledsPerSection-1)*abs(delta) : 0) + gHue;
      fill_rainbow(&leds[z][sectionStart[z][s]], ledsPerSection, hue, delta);
    }
  }
  showStrip();
  FastLED.delay(SpeedDelay);
  gHue++;
}

//------------------------------------------------------//
// borrowed from FastLED demo
void Fire(int Cooling, int Sparking, int SpeedDelay) {

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      uint16_t pos;
      uint8_t cooldown;

      // Odd an even sections are 'burnng' from the oposite ends.
      
      // Step 1.  Cool down every cell a little
      for ( int i=0; i<ledsPerSection; i++ ) {
        cooldown = random8(0, ((Cooling * 10) / ledsPerSection) + 2);
        pos = s%2 ? sectionEnd[z][s]-i : sectionStart[z][s]+i;
        // prevent bottommost pixels becoming black
        if ( cooldown>heat[z][pos] ) {
          heat[z][pos] = (i<7) ? 2 : 0;
        } else {
          heat[z][pos] -= cooldown;
        }
      }
      
      // Step 2.  Heat from each cell drifts 'up' and diffuses a little
      for ( int k=ledsPerSection-1; k>=2; k-- ) {
        pos = s%2 ? sectionEnd[z][s]-k : sectionStart[z][s]+k;
        if ( s%2 ) {
          heat[z][pos] = (heat[z][pos+1] + ((heat[z][pos+2])<<1)) / 3;
        } else {
          heat[z][pos] = (heat[z][pos-1] + ((heat[z][pos-2])<<1)) / 3;
        }
      }
        
      // Step 3.  Randomly ignite new 'spark' near the bottom
      if( random8() < Sparking ) {
        int y = random8(max(5,ledsPerSection/10));
        pos = s%2 ? sectionEnd[z][s]-y : sectionStart[z][s]+y;
        heat[z][pos] += random8(160,255);
      }
    
      // Step 4.  Convert heat to LED colors
      for ( int j=sectionStart[z][s]; j<=sectionEnd[z][s]; j++ ) {
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
    
    for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
      fill_solid(leds[z], numLEDs[z], CRGB::Black);
      for ( int s=0; s<numSections[z]; s++ ) {
        int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
       
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
    FastLED.delay(15);
  }
}

//------------------------------------------------------//
void meteorRain(byte meteorSizePct, byte meteorTrailDecay, boolean meteorRandomDecay, int SpeedDelay) {  
  unsigned int meteorSize, pos;
  byte pct = (beat8(20)*100)/255; // 20 meteors/min

  for ( int z=(bobClient && bobClient.connected())?1:0; z<numZones; z++ ) {
    for ( int s=0; s<numSections[z]; s++ ) {
      unsigned int ledsPerSection = sectionEnd[z][s]-sectionStart[z][s]+1;
      meteorSize = ledsPerSection * meteorSizePct / 100;

      // fade brightness all LEDs one step
      for ( int j=0; j<ledsPerSection; j++ ) {
        if ( (!meteorRandomDecay) || (random8(10)>5) ) {
          leds[z][sectionStart[z][s]+j].fadeToBlackBy(meteorTrailDecay);
        }
      }

      // draw the meteor only when pct is between 0 & 50
      if ( pct <= 50 ) {
        int newpct = pct * 2;
        pos = sectionStart[z][s] + getPosFromPct(newpct, ledsPerSection, meteorSize, s%2);
  
        for ( int j=0; j<meteorSize; j++ ) {
          leds[z][pos+j] = CRGB::White;
        }
      }
    }
  }
  
  showStrip();
  FastLED.delay(SpeedDelay);
}

// ***************************************
// ** Common Functions **
// ***************************************

unsigned int getPosFromPct(unsigned int pct, unsigned int ledsPerSection, byte width, bool Reverse) {
  if ( width<1 || width>=ledsPerSection ) return 0;
  return (unsigned int) round(((float)((Reverse ? 100-pct : pct) * (ledsPerSection-width)) / 100));
}

void addGlitter(int zone, int section, fract8 chanceOfGlitter) {
  if ( (random8()*100)/255 < chanceOfGlitter ) {
    for ( int i=0; i<random8(1,5); i++ )  // at least one dot per 50 pixels
      leds[zone][ random16(sectionStart[zone][section],sectionEnd[zone][section]) ] = CRGB::White; // 100% white
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
