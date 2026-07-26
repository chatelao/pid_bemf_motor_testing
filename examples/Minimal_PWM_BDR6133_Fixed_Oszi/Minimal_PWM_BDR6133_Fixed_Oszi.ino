/*
 * XIAO RP2040 analogWrite() test
 *
 * - D7 / D8   - DC-Motor (H-Bridge Signal)
 * - A0 / A1   - DC-Motor (bEMF Messeingang)
 * - D9 (F0f)  - Oszilloskop Trigger

                      +--------------------+      +--------------------+         +---------------+
                     |      RP2040        |      |     BDR-6133       |         |     Motor     |
                     |    (Top View)      |      |    Motor Driver    |         | DC brushed    |
                     +--------------------+      +--------------------+         +---------------+
                     |                5v  |      |                    |         |               |
          ---``|<----| D15 (LED)      GND |      |                    |         |               |
          ---``|<----| D16 (LED)      3v3 |      |                    |         |               |
                     |                    |      |                    |         |               |
                     |        (PWM B) D8  |----->| InB           OutB |=====+==>| B             |
                     |        (PWM A) D7  |----->| InA           OutA |==+==|==>| A             |
                     |                    |      +---------+----------+  |  |   +---------------+
                     |       (Shut)   A2  |<.............../             |  |
                     |       (bEMF B) A1  |<----------------------------/   |
                     |       (bEMF A) A0  |<-------------------------------/
                     |                    |
                     |                    |      +--------------------+
                     |                    |      | Functions          |
                     |                    |      +--------------------+
                     |         F0f    D9  |----->| F0f                |
                     |         F0b    D10 |----->| F0b                |
                     |                    |      +---------+----------+
                     |       (Shut)   A3  |<.............../

                     |                    |      +--------------------+
                     |                    |      | DCC / Railcom      |
                     |                    |      +--------------------+
                     |     DCC-ACK    D4  |----->| DCC-ACK            |
                     |     DCC/MM/SX  D5  |<-----| DCC/MM/SX          |
                     |     RailCom    D6  |----->| RailCom            |
                     |                    |      +---------+----------+
                     |       (Shut)   A3  |<.............../
                     +--------------------+

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

  // Oscilloscope Trigger
       pinMode( D9, OUTPUT);
  digitalWrite( D9, LOW);
}

int i = 0;
  
void loop() {

  // Enable BDR6133
   analogWrite( D8, 200 );
  digitalWrite( D9, HIGH);   // Oscilloscope Trigger

   if((i = (i + 1) % 2) == 0)  // Toggle LED
     pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
   else
     pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
   pixels.show();
         delay( 20 );

  // Disable BDR6133 + Wait bEMF for settling (ready to read)
   analogWrite( D8,   0 );
  digitalWrite( D9, LOW );  // Oscilloscope Trigger
         delay( 2 );
}