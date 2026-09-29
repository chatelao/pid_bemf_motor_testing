/*
 * Sequential Blink for non-contiguous pins: 16, 17, 25, 9, 10
 * Turns LEDs on for 500ms, then off for 250ms sequentially.
 */

// Array holds the specific pin sequence
const int blinkPins[] = {  16  // Red
                        ,  17  // Green
                        ,  25  // Blue
                        
                        ,  D9  // F0f
                        , D10  // F0r
                        };
const int numPins = sizeof(blinkPins) / sizeof(blinkPins[0]);

void setup() {
  // Initialize mapped pins as OUTPUT
  for (int i = 0; i < numPins; i++) {
    pinMode(blinkPins[i], OUTPUT);
  }
}

void loop() {
  // Iterate through the array indices
  for (int i = 0; i < numPins; i++) {
    digitalWrite(blinkPins[i], HIGH);
    delay(500);
    digitalWrite(blinkPins[i], LOW);
    delay(250);
  }
}
