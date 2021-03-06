#include <TimerOne.h>
#include <math.h>
#include <Adafruit_NeoPixel.h>
#include <avr/pgmspace.h>

#define NEOPIXEL_PIN 4 // neopixel data pin
#define HEART_PIN A5 // heart rate monitor pin
#define PULSE_PIN  A4 // Pulse Sensor purple wire
#define TOGGLE 3  // pin to read the mode toggle switch
#define DIAL 2  // pin to read potentiomiter for brightness 

#define X 1 // just to make the patterns a little clearer to read. Dont make any variables named X though...

#define LED_COUNT 49

const byte  beatPattern[4][LED_COUNT]  = {
  {
     0,0,0,  0,0,0,
   0,0,0,0,0,0,0,0,0,
   0,0,0,X,0,X,0,0,0,
   0,0,0,X,X,X,0,0,0,
     0,0,0,X,0,0,0,
       0,0,0,0,0,
         0,0,0,
           0
  },
  {
     0,0,0,  0,0,0,
   0,0,0,0,0,0,0,0,0,
   0,0,X,0,0,0,X,0,0,
   0,0,X,0,0,0,X,0,0,
     0,0,X,0,X,0,0,
       0,0,X,0,0,
         0,0,0,
           0
  },
  {
     0,0,0,  0,0,0,
   0,0,X,X,0,X,X,0,0,
   0,X,0,0,X,0,0,X,0,
   0,X,0,0,0,0,0,X,0,
     0,X,0,0,0,X,0,
       0,X,0,X,0,
         0,X,0,
           0
  },
  {
     X,X,X,  X,X,X,
   X,X,0,0,X,0,0,X,X,
   X,0,0,0,0,0,0,0,X,
   X,0,0,0,0,0,0,0,X,
     X,0,0,0,0,0,X,
       X,0,0,0,X,
         X,0,X,
           X
  }
};

const byte idlePattern[4][LED_COUNT] PROGMEM = {
   {
     X,X,X,  X,X,X,
   X,X,0,0,X,0,0,X,X,
   X,0,0,0,0,0,0,0,X,
   X,0,0,0,0,0,0,0,X,
     X,0,0,0,0,0,X,
       X,0,0,0,X,
         X,0,X,
           X
  },
   {
     0,0,0,  0,0,0,
   0,0,X,X,0,X,X,0,0,
   0,X,0,0,X,0,0,X,0,
   0,X,0,0,0,0,0,X,0,
     0,X,0,0,0,X,0,
       0,X,0,X,0,
         0,X,0,
           0
  },
   {
     0,0,0,  0,0,0,
   0,0,0,0,0,0,0,0,0,
   0,0,X,X,0,X,X,0,0,
   0,0,X,0,X,0,X,0,0,
     0,0,X,0,X,0,0,
       0,0,X,0,0,
         0,0,0,
           0
  },
  {
     0,0,0,  0,0,0,
   0,0,0,0,0,0,0,0,0,
   0,0,0,X,0,X,0,0,0,
   0,0,0,X,X,X,0,0,0,
     0,0,0,X,0,0,0,
       0,0,0,0,0,
         0,0,0,
           0
  }
};
int brightness = 100;              
boolean idleMode = false;
unsigned long prevSettingCheck = 0;     

// Volatile Variables, used in the interrupt service routine!
volatile int BPM;                   // int that holds raw Analog in 0. updated every 2mS
volatile int Signal;                // holds the incoming raw data
volatile int IBI = 600;             // int that holds the time interval between beats! Must be seeded!
volatile boolean Pulse = false;     // "True" when User's live heartbeat is detected. "False" when not a "live beat".
volatile boolean QS = true;        // becomes true when Arduoino finds a beat.
volatile int pulseVal;  

volatile int P =512;                      // used to find peak in pulse wave, seeded
volatile int T = 512;                     // used to find trough in pulse wave, seeded
volatile int thresh = 525;                // used to find instant moment of heart beat, seeded



// Regards Serial OutPut  -- Set This Up to your needs
static boolean serialVisual = false;   // Set to 'false' by Default.  Re-set to 'true' to see Arduino Serial Monitor ASCII Visual Pulse
Adafruit_NeoPixel strip = Adafruit_NeoPixel(LED_COUNT, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  Serial.begin(115200);             // we agree to talk fast!
  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  pinMode(TOGGLE, INPUT);
  digitalWrite(TOGGLE, HIGH);

  interruptSetup();                 // sets up to read Pulse Sensor signal every 2mS
}


void loop() {
  getSettings();
 
  if ( idleMode) {
    playIdlePattern();
  } else {
    if (QS == true) {    // A Heartbeat Was Found
      // BPM and IBI have been Determined
      // Quantified Self "QS" true when arduino finds a heartbeat
     // serialOutputWhenBeatHappens();   // A Beat Happened, Output that to serial.
      QS = false;                      // reset the Quantified Self flag for next time
      heartBeat();
    }

    // resting pattern;
    for ( uint8_t ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
      if (((beatPattern[0][ledIndex]))  == 1) {
        strip.setPixelColor(ledIndex, beat(40) );
      }
    }
    strip.show();
  }
}


void getSettings() {
  unsigned long currentMillis = millis();
  boolean newMode = (digitalRead(TOGGLE) == HIGH);
  if(newMode != idleMode) {
    idleMode = newMode;
    clearLEDs();
  }
  
  if (currentMillis - prevSettingCheck > 1000) {
    prevSettingCheck = currentMillis;    
    int newBrightness = map(analogRead(DIAL), 0, 1024, 0, 100);
    
    if(round(newBrightness * 1.05) > brightness || round(newBrightness * .95) < brightness ){
      brightness = newBrightness;
      strip.setBrightness(brightness); // updating the strip disables interrups, so do this as infrequently as possible
    }
  }
  
}


void setBeatPattern(uint8_t val){
  uint8_t ledIndex,  frameIndex;
  for (frameIndex = 0; frameIndex < 4; frameIndex++) {
    for (ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
      if ( ((beatPattern[frameIndex][ledIndex])) == 1) {
        strip.setPixelColor(ledIndex, beat((val) * (4 - frameIndex)) );
      }
    }
  }
}

void clearLEDs() {
  uint8_t ledIndex;
  for (ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
    strip.setPixelColor(ledIndex, strip.Color(0, 0, 0));
  }
}

void playIdlePattern() {
  uint8_t ledIndex, colorVal, frameIndex;
  for (colorVal = 0; colorVal < 256; colorVal++) {
    getSettings();
    if (!idleMode) {
      break;
    }
    for (frameIndex = 0; frameIndex < 4; frameIndex++) {
      for (ledIndex = 0; ledIndex < LED_COUNT; ledIndex++) {
        if (pgm_read_byte(&(idlePattern[frameIndex][ledIndex])) == 1) {
          strip.setPixelColor(ledIndex, Wheel((frameIndex * 32 + colorVal) & 255));
        }
      }
    }
    strip.show();
    delay(5);
  }
}

  void heartBeat() {
    uint8_t beatVal;
    for (beatVal = 10; beatVal < 64; beatVal++) {
      setBeatPattern(beatVal);
      strip.show();
    }
   
    for (beatVal = 64; beatVal > 10; beatVal--) {
      setBeatPattern(beatVal);
      strip.show();
    }
  
  }


uint32_t beat(byte beatIntensity) {
  if (beatIntensity < 40) {
    return strip.Color(0, 0, 0);
  }
  byte yellow = round(beatIntensity / 5);
  return strip.Color( beatIntensity, (yellow < 25) ? 0 : yellow, 0);
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if (WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3);
  } else if (WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3);
  } else {
    WheelPos -= 170;
    return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0);
  }
}




