
#include <OctoWS2811.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VISUAL INITIATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// The display size
const unsigned int matrix_width = 15;
const unsigned int matrix_height = 8;

// These parameters adjust the vertical thresholds
const float maxLevel = 0.1;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 40.0; // total range to display, in decibels
const float linearBlend = 0.3;   // useful range is 0 to 0.7

// OctoWS2811 objects
const int ledsPerPin = matrix_width * matrix_height / 8;
DMAMEM int displayMemory[ledsPerPin*6];
int drawingMemory[ledsPerPin*6];
const int config = WS2811_GRB | WS2811_800kHz;
//const int config = WS2811_GRB;
OctoWS2811 leds(ledsPerPin, displayMemory, drawingMemory, config);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// AUDIO INITIATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Audio library objects
AudioInputUSB            usb1;
AudioAnalyzeFFT1024      fft;
AudioOutputI2S           i2s1;

AudioConnection          patchCord1(usb1, 0, i2s1, 0);
AudioConnection          patchCord2(usb1, 1, i2s1, 1);
AudioConnection          patchCord3(usb1, 1, fft, 0);
AudioControlSGTL5000     sgtl5000_1;     //xy=302,184

// This array holds the volume level (0 to 1.0) for each
// vertical pixel to turn on.  Computed in setup() using
// the 3 parameters above.
float thresholdVertical[matrix_height];

// This array specifies how many of the FFT frequency bin
// to use for each horizontal pixel.  Because humans hear
// in octaves and FFT bins are linear, the low frequencies
// use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[matrix_width] = 
//{1,1,2,3,4,6,9,12,18,26,37,53,76,108,155};
{1,1,1,1,1,1,2,2,3,3,4,5,7,9,12};

// SUB BASS: 0-50
// BASS:     50-225
// MIDRANGE: 225-1600
// HIGHMIDS: 1800-5000
// HIGHFREQ: 5000-20000
// EACH BIN: 300

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void setup() {
  AudioMemory(32);

  sgtl5000_1.enable();
  sgtl5000_1.volume(0.6);
  
  computeVerticalLevels();
  leds.begin();
  leds.show();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SECONDARY INITIALIZATIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Converts matrix display coordinates to
// index numbers OctoWS2811 requires. 
unsigned int xy(unsigned int x, unsigned int y) {
    return y * matrix_width + x;}

  int yCurr[ matrix_width ];
  int yTarg[ matrix_width ];
  float level[ matrix_width ];
  int Iter = 0;

  byte r;
  byte g;
  byte b;
  int COLOR[matrix_height] = {0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF,0xFFFFFF};
  int COLOR_CHANGE = 1;
  int HUE = 0;
  int BLANK = 0x000000;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// MAIN LOOP
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  unsigned long currentMillis = millis();
  unsigned int x, y, freqBin;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Set Output Volume
  float vol = usb1.volume();
  if (vol > 0) {
    vol = 0.3 + vol * 0.5;}
  sgtl5000_1.volume(vol);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycle color if button pushed
  if (COLOR_CHANGE == 1) {
    //HUE = HUE + 60;
    for (int i=0; i < matrix_height; i++) {
      HSV_to_RGB(HUE+10*i, 100, 30, &r,&g,&b);
      COLOR[i] = r*pow(16,4)+g*pow(16,2)+b;}
      COLOR_CHANGE = 0;}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// FFT Read and Process
  if (fft.available()) {
    freqBin = 0;
    
// Determine a new destination height
    for (x=0; x < matrix_width; x++) {
      yCurr[x] = yTarg[x];
      yTarg[x] = 0;
      level[x] = fft.read(freqBin, freqBin + frequencyBinsHorizontal[x] - 1);
      for (y=0; y < matrix_height; y++) {
        if (level[x] > thresholdVertical[y]) {
          yTarg[x] = yTarg[x] + 1;}
      freqBin = freqBin + frequencyBinsHorizontal[x];
        if (abs(yTarg[x] - yCurr[x]) > Iter) {
          Iter = abs(yTarg[x] - yCurr[x]); }
      }
    }

// Slide until yCurr = yTarg
    for (int i=0; i < Iter; i++) {
      for (x=0; x < matrix_width; x++) {     
        if (yCurr[x] < yTarg[x]) {
          yCurr[x] = yCurr[x] + 1;
          leds.setPixel(xy(x, yCurr[x]-1), COLOR[yCurr[x]-1]);}
        else if (yCurr[x] > yTarg[x]) {
          leds.setPixel(xy(x, yCurr[x]-1), BLANK);
          yCurr[x] = yCurr[x] - 1;}
      }
    //leds.show();
    //delay(10);
    }    
  } 
  leds.show();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SUB FUNCTIONS
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////

// Vertical Level Computation Function ////////////////////////////////////////////////////////////////////////
void computeVerticalLevels() {
  unsigned int y;
  float n, logLevel, linearLevel;

  for (y=0; y < matrix_height; y++) {
    n = (float)y / (float)(matrix_height - 1);
    logLevel = pow10f(n * -1.0 * (dynamicRange / 20.0));
    linearLevel = 1.0 - n;
    linearLevel = linearLevel * linearBlend;
    logLevel = logLevel * (1.0 - linearBlend);
    thresholdVertical[y] = (logLevel + linearLevel) * maxLevel;
  }
}


// HSV Function ////////////////////////////////////////////////////////////////////////////////////////////////
void HSV_to_RGB(float h, float s, float v, byte *r, byte *g, byte *b){
  int i;
  float f,p,q,t;
  
  h = max(0.0, min(360.0, h));
  s = max(0.0, min(100.0, s));
  v = max(0.0, min(100.0, v));
  
  s /= 100;
  v /= 100;
  
  if(s == 0) {
    // Achromatic (grey)
    *r = *g = *b = round(v*255);
    return;}

  h /= 60; // sector 0 to 5
  i = floor(h);
  f = h - i; // factorial part of h
  p = v * (1 - s);
  q = v * (1 - s * f);
  t = v * (1 - s * (1 - f));
  switch(i) {
    case 0:
      *r = round(255*v);
      *g = round(255*t);
      *b = round(255*p);
      break;
    case 1:
      *r = round(255*q);
      *g = round(255*v);
      *b = round(255*p);
      break;
    case 2:
      *r = round(255*p);
      *g = round(255*v);
      *b = round(255*t);
      break;
    case 3:
      *r = round(255*p);
      *g = round(255*q);
      *b = round(255*v);
      break;
    case 4:
      *r = round(255*t);
      *g = round(255*p);
      *b = round(255*v);
      break;
    default: // case 5:
      *r = round(255*v);
      *g = round(255*p);
      *b = round(255*q);
    }
}
