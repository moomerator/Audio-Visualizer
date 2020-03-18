// LED Audio Spectrum Analyzer Display
// 
// Creates an impressive LED light show to music input
//   using Teensy 3.1 with the OctoWS2811 adaptor board
//   http://www.pjrc.com/store/teensy31.html
//   http://www.pjrc.com/store/octo28_adaptor.html
//
// Line Level Audio Input connects to analog pin A3
//   Recommended input circuit:
//   http://www.pjrc.com/teensy/gui/?info=AudioInputAnalog
//
// This example code is in the public domain.


#include <OctoWS2811.h>
#include <Audio.h>
#include <Wire.h>
#include <SD.h>
#include <SPI.h>
#include <SerialFlash.h>

// The display size and color to use
const unsigned int matrix_width = 8;
const unsigned int matrix_height = 15;
const unsigned int myColor = 0x000016;

// These parameters adjust the vertical thresholds
const float maxLevel = 0.3;      // 1.0 = max, lower is more "sensitive"
const float dynamicRange = 40.0; // total range to display, in decibels
const float linearBlend = 0.3;   // useful range is 0 to 0.7

// OctoWS2811 objects
const int ledsPerPin = matrix_width * matrix_height / 8;
DMAMEM int displayMemory[ledsPerPin*6];
int drawingMemory[ledsPerPin*6];
const int config = WS2811_GRB | WS2811_800kHz;
OctoWS2811 leds(ledsPerPin, displayMemory, drawingMemory, config);

// Audio library objects

AudioInputUSB            usb1;
AudioFilterBiquad        biquad1;
AudioAnalyzeFFT1024      fft;
AudioOutputI2S           i2s1;
AudioOutputUSB           usb2;

AudioConnection          patchCord1(usb1, 0, usb2, 0);
AudioConnection          patchCord2(usb1, 0, i2s1, 0);
AudioConnection          patchCord4(usb1, 1, fft, 0);

// This array holds the volume level (0 to 1.0) for each
// vertical pixel to turn on.  Computed in setup() using
// the 3 parameters above.
float thresholdVertical[matrix_height];

// This array specifies how many of the FFT frequency bin
// to use for each horizontal pixel.  Because humans hear
// in octaves and FFT bins are linear, the low frequencies
// use a small number of bins, higher frequencies use more.
int frequencyBinsHorizontal[matrix_width] = 
{ 2,  4,  8,  16,  32,  64,  128,  256};


void setup() {
  AudioMemory(20);
  
  computeVerticalLevels();

  leds.begin();
  leds.show();
}

// Converts matrix display coordinates to
// index numbers OctoWS2811 requires. 
unsigned int xy(unsigned int x, unsigned int y) {
  if ((y & 1) == 0) {
    // even numbered rows (0, 2, 4...) are left to right
    return y * matrix_width + x;
  } else {
    // odd numbered rows (1, 3, 5...) are right to left
    return y * matrix_width + matrix_width - 1 - x;
  }
}

  int yCurr[ matrix_width ];
  int yTarg[ matrix_width ];
  float level[ matrix_width ];
  int Iter = 0;
  
void loop() {
  unsigned long currentMillis = millis();
  unsigned int x, y, freqBin;

  if (fft.available()) {
    freqBin = 0;

// Determine a new destination height
    for (x=0; x < matrix_width; x++) {
      yCurr[x] = yTarg[x];
      yTarg[x] = 0;
      level[x] = fft.read(freqBin, freqBin + frequencyBinsHorizontal[x] - 1);
      for (y=0; y < matrix_height; y++) {
        if (level[x] >= thresholdVertical[y]) {
          yTarg[x] = yTarg[x] + 1;}}
      freqBin = freqBin + frequencyBinsHorizontal[x];
      if (abs(yTarg[x] - yCurr[x]) > Iter) {
        Iter = abs(yTarg[x] - yCurr[x]); }
    }

// Slide until yCurr = yTarg
    for (int i=0; i < Iter; i++) {
      for (x=0; x < matrix_width; x++) {     
        if (yCurr[x] <= yTarg[x]) {
          leds.setPixel(xy(x, yCurr[x]), myColor);
          yCurr[x] = yCurr[x] + 1;}
        else if (yCurr[x] > yTarg[x]) {
          leds.setPixel(xy(x, yCurr[x]), 0x000000); 
          yCurr[x] = yCurr[x] - 1;}
      }
    leds.show();
    xxxxx;
    }
  }
}

// Run once from setup, the compute the vertical levels
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
