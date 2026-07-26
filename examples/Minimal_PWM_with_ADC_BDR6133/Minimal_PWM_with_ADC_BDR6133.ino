/*
 * XIAO RP2040 analogWrite() test
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// XIAO RP2040 internal NeoPixel pins
Adafruit_NeoPixel pixels( 1, 12, NEO_GRB + NEO_KHZ400 );

void setup() {

  analogWriteFreq ( 400 );
  analogWriteRange( 255 );

  // Neopixel enable power
       pinMode( 11, OUTPUT );
  digitalWrite( 11, HIGH );
  // InisNeopixel power
  pixels.begin();
  pixels.setBrightness(50);

  // Enable BDR6133 PWM pins
      pinMode( D7, OUTPUT);
  analogWrite( D7, 0);

      pinMode( D8, OUTPUT);
  analogWrite( D8, 0);

  // Enable Oscilloscope trigger
      pinMode( D2, OUTPUT);
 digitalWrite(D2, LOW);  // Sets D2 to logic LOW (GND)
}



void loop() {

  // Kickstart
  analogWrite( D8, 255);
        delay( 10 );

  for(int i = 0; i < 256; i++ ) {
    analogWrite( D8, i);
    pixels.setPixelColor(0, pixels.Color(i, 0, 0));
    pixels.show();
          delay( 10 );

    // Messlücke
   digitalWrite( D2, HIGH ); // Sets D2 to logic HIGH (VCC)
          delay( 1 );
    analogWrite( D8, 0);
          delay( 2 );
   digitalWrite( D2, LOW ); // Sets D2 to logic HIGH (VCC)
  }

  analogWrite( D8, 255);
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();
  delay( 2000 );

  for(int i = 255; i >= 0; i-- ) {
    analogWrite( D8, i);
    pixels.setPixelColor(0, pixels.Color(0, i, 0));
    pixels.show();
          delay( 15 );

    // Messlücke
    analogWrite( D8, 0);
          delay( 2 );
  }

  delay( 500 );
}

void setup1() {

  Serial.begin(921600);
  while (!Serial && millis() < 2000); // Wait for Serial on USB boards

  analogReadResolution(12);
}

void loop1() {

         uint32_t now    = millis();
  static uint32_t log_us = micros();

              int bemfA  = analogRead( A0 );
              int bemfB  = analogRead( A1 );

    Serial.print( now );    Serial.print( ',' );
    Serial.print( bemfA );  Serial.print( ',' );
    Serial.println( bemfB );

}

