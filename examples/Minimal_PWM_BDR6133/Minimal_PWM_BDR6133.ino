/*
 * XIAO RP2040 analogWrite() test
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// XIAO RP2040 internal NeoPixel pins
Adafruit_NeoPixel pixels( 1, 12, NEO_GRB + NEO_KHZ400 );

void setup() {

  analogWriteFreq ( 20000 );
  analogWriteRange( 255 );

  // Neopixel enable power
       pinMode( 11, OUTPUT );
  digitalWrite( 11, HIGH );
  // InisNeopixel power
  pixels.begin();
  pixels.setBrightness(50);

  // Enable BDR6133
      pinMode( D7, OUTPUT);
  analogWrite( D7, 0);

      pinMode( D8, OUTPUT);
  analogWrite( D8, 0);
}



void loop() {

  analogWrite( D8, 255);
        delay( 10 );

  for(int i = 0; i < 256; i++ ) {
    analogWrite( D8, i);
    pixels.setPixelColor(0, pixels.Color(i, 0, 0));
    pixels.show();
          delay( 10 );
  }

  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
  delay( 500 );

  for(int i = 255; i >= 0; i-- ) {
    analogWrite( D8, i);
    pixels.setPixelColor(0, pixels.Color(0, i, 0));
    pixels.show();
          delay( 10 );
  }

  delay( 500 );
}