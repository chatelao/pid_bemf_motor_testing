#include <Arduino.h>

/**
 * BEMF_50Hz.ino
 *
 * Simple example providing single-direction 50 Hz PWM motor drive at max speed
 * while continuously reading ADC values on both BEMF channels and current shunt
 * at maximum sampling speed.
 */

// Pin Definitions based on DESIGN.md
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #define PIN_PWM_A  D7
  #define PIN_PWM_B  D8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   15
  #define PIN_LED2   16
#else
  // Default fallback to standard pins
  #define PIN_PWM_A   7
  #define PIN_PWM_B   8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   13
  #define PIN_LED2   12
#endif

// Parameters
const uint32_t PWM_FREQ = 50;   // 50 Hz PWM frequency
const int PWM_DUTY      = 255;  // Max speed (100% duty cycle)

void setup() {
  Serial.begin(921600);
  while (!Serial && millis() < 2000); // Wait for Serial on USB boards

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1,  OUTPUT);
  pinMode(PIN_LED2,  OUTPUT);

  // Set ADC to 12-bit resolution as per DESIGN.md
  analogReadResolution(12);

  // Set 50 Hz PWM frequency for RP2040
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  analogWriteFreq(PWM_FREQ);
#endif

  // Drive single direction (Forward) at max speed
  analogWrite(PIN_PWM_A, PWM_DUTY);
  analogWrite(PIN_PWM_B, 0);

  digitalWrite(PIN_LED1, HIGH); // Activity LED on
  digitalWrite(PIN_LED2, LOW);

  Serial.println("--- Märklin Motor 50Hz BEMF & Shunt ADC Test ---");
  Serial.println("PWM: 50 Hz Single-Direction Max Speed");
  Serial.println("Format: TIME_US, BEMF_A, BEMF_B, SHUNT");
}

void loop() {
  // Continuous max speed ADC reading on both BEMF terminals and current shunt
  uint32_t now_us = micros();
  int bemfA = analogRead(PIN_BEMF_A);
  int bemfB = analogRead(PIN_BEMF_B);
  int shunt = analogRead(PIN_SHUNT);

  // Non-blocking serial logging when buffer is available
  if (Serial.availableForWrite() >= 32) {
    Serial.print(now_us);
    Serial.print(',');
    Serial.print(bemfA);
    Serial.print(',');
    Serial.print(bemfB);
    Serial.print(',');
    Serial.println(shunt);
  }
}
