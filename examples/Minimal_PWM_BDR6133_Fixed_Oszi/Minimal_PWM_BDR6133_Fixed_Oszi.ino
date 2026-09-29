/*
 * XIAO RP2040 analogWrite() test
 *
 * - D7 / D8   - DC-Motor (H-Bridge Signal)
 * - A0 / A1   - DC-Motor (bEMF Messeingang)
 * - D9 (F0f)  - Oszilloskop Trigger
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// XIAO RP2040 internal NeoPixel pins
Adafruit_NeoPixel pixels( 1, 12, NEO_GRB + NEO_KHZ400 );

// Puffer für High-Speed Sampling (500 Samples reichen für 2ms bei ~4µs pro analogRead)
const int MAX_SAMPLES = 1000;

uint16_t bemfA_buf[MAX_SAMPLES];
uint16_t bemfB_buf[MAX_SAMPLES];
uint16_t bemfC_buf[MAX_SAMPLES];
     int speed_buf[MAX_SAMPLES];
uint32_t time_buf[ MAX_SAMPLES];
int lastSampleCount = 0;

void setupMotor() {
  // Setup PWM for BDR6133
  analogWriteFreq ( 20000 );
  analogWriteRange( 255 );

  // Enable BDR6133
       pinMode( D7, OUTPUT);
   analogWrite( D7, 0);
       pinMode( D8, OUTPUT);
   analogWrite( D8, 0);
}

void setup() {
  // Initialize USB Serial for bEMF output
  Serial.begin( 115200 );

  // Neopixel enable power
       pinMode( 11, OUTPUT );
  digitalWrite( 11, HIGH );

  // Neopixel power
  pixels.begin();
  pixels.setBrightness(50);

  // Oscilloscope Trigger - INVERTED (Initial state HIGH)
       pinMode(  D9, OUTPUT);
  digitalWrite(  D9, HIGH);
       pinMode( D10, OUTPUT);
  digitalWrite( D10, HIGH);

  pixels.setPixelColor(0, pixels.Color(0, 0, 255)); // Blue
  pixels.show();

  Serial.println( "Time_us,bEMF_A0,bEMF_A1" );

  setupMotor();
}

unsigned long cycleStartTime = 0;
  
void loop() {
  for(int d = 0; d < 2; d++)  {      // Direction
    for(int i = 0; i <= 16; i++)  {  // Speed * 16    
      for(int k = 0; k < 30; k++)  {  // Durations / Repetitions à 20ms (100ms / 16 Stufen)

        // Start 20ms window
        cycleStartTime = millis();

        // Configure BDR6133 (Fixed broken syntax)
        analogWrite(  D7, (d == 0 ? (i == 0 ? 0 : i * 16 - 1) : 0)); // forward  test (0..255)
        analogWrite(  D8, (d == 0 ? 0 : (i == 0 ? 0 : i * 16 - 1))); // backward test (0..255)
        
        // Oscilloscope Trigger (F0/F1) - INVERTED (Active LOW)
        digitalWrite(  D9, LOW );
        digitalWrite( D10, LOW );

        if((d % 2) == 0)  // Toggle LED Color
          pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red
        else
          pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green
        pixels.show();

        // Die gesammelten Daten des vorherigen 2ms-Fensters asynchron über USB ausgeben
        for (int j = 0; j < lastSampleCount; j++) {
                             Serial.print(    time_buf[j] );
          Serial.print(","); Serial.print(   speed_buf[j] );
          Serial.print(","); Serial.print(   bemfA_buf[j] );
          Serial.print(","); Serial.println( bemfB_buf[j] );
        }

        // Den Rest der 20ms "Motor Ein"-Zeit abwarten, kompensiert die Serial.print Dauer
        while (millis() - cycleStartTime < 20) {
          // Wait
        }

        // 2. MOTOR AUS & HIGH-SPEED SAMPLING (2ms bEMF Messfenster)
        analogWrite(   D7,   0 );
        analogWrite(   D8,   0 );
        
        // Oscilloscope Trigger - INVERTED (Measurement window HIGH)
        digitalWrite(  D9, HIGH );
        digitalWrite( D10, HIGH );

        int sampleCount = 0;
        unsigned long startMicros = micros();
        
        // Array füllen, solange wir im 2000µs Fenster sind und der Puffer nicht voll ist
        while ((micros() - startMicros < 2000) && (sampleCount < MAX_SAMPLES)) {
          bemfA_buf[sampleCount] = analogRead( A0 );
          bemfB_buf[sampleCount] = analogRead( A1 );
          bemfC_buf[sampleCount] = analogRead( A2 );
          time_buf[sampleCount]  = micros();
          speed_buf[sampleCount] = (d == 0 ? i : -i);
          sampleCount++;
        }
        
        lastSampleCount = sampleCount;
      }
    }
  }
}