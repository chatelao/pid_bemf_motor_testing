#include <Arduino.h>

/**
 * BEMF_Ramp_Test.ino
 *
 * This sketch implements a simple open-loop PWM ramp to characterize motor BEMF.
 * It performs a 1s ramp up and 1s ramp down.
 * Every 250ms, it inserts a measurement gap (PWM = 0) to measure BEMF
 * without PWM interference, which is particularly useful at high duty cycles.
 */

// Pin Definitions based on DESIGN.md
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #define PIN_PWM_A 7
  #define PIN_PWM_B 8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   15
  #define PIN_LED2   16
#elif defined(ARDUINO_ARCH_STM32)
  #define PIN_PWM_A 7
  #define PIN_PWM_B 8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   13 // Onboard LED for Nucleo
  #define PIN_LED2   12
#else
  // Default fallback to standard pins
  #define PIN_PWM_A 7
  #define PIN_PWM_B 8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   13
  #define PIN_LED2   12
#endif

// Parameters
const uint32_t RAMP_DURATION_MS = 1000;
const uint32_t MEASURE_GAP_MS = 25;      // Gap duration (1-50ms)
const uint32_t MEASURE_INTERVAL_MS = 250; // Every 250ms

// State variables
uint32_t last_ramp_update = 0;
uint32_t last_gap_time = 0;
int current_pwm = 0;
bool ramping_up = true;
bool in_gap = false;
uint32_t gap_start_ms = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for Serial on USB boards

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Set ADC to 12-bit resolution as per DESIGN.md
  analogReadResolution(12);

  // Set PWM frequency to 20kHz (ultrasonic)
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  analogWriteFreq(20000);
#elif defined(ARDUINO_ARCH_STM32)
  // analogWriteFrequency is supported by STM32 Arduino core
  // analogWriteFrequency(20000);
#endif

  Serial.println("--- Märklin Motor BEMF Characterization Tool ---");
  Serial.println("Ramp: 1s UP, 1s DOWN");
  Serial.print("Measurement Gap: "); Serial.print(MEASURE_GAP_MS); Serial.println("ms");
  Serial.println("Format: PWM, BEMF_A, BEMF_B, IS_GAP");
}

void loop() {
  uint32_t now = millis();

  // 1. Measurement Gap Logic
  if (!in_gap && (now - last_gap_time >= MEASURE_INTERVAL_MS)) {
    in_gap = true;
    gap_start_ms = now;
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);
    digitalWrite(PIN_LED2, HIGH); // LED2 indicates gap
  }

  if (in_gap) {
    if (now - gap_start_ms >= MEASURE_GAP_MS) {
      in_gap = false;
      last_gap_time = now;
      digitalWrite(PIN_LED2, LOW);
      // Resume current PWM
      analogWrite(PIN_PWM_A, current_pwm);
    } else {
      // During gap, measure and log BEMF
      int bemfA = analogRead(PIN_BEMF_A);
      int bemfB = analogRead(PIN_BEMF_B);
      Serial.print(current_pwm);
      Serial.print(", ");
      Serial.print(bemfA);
      Serial.print(", ");
      Serial.print(bemfB);
      Serial.println(", 1");
      delay(5); // Don't flood too much, but get a few samples per gap
      return;
    }
  }

  // 2. Ramp Logic
  // Update every 10ms for a smooth ramp
  if (now - last_ramp_update >= 10) {
    last_ramp_update = now;

    // Calculate increment: 255 steps / (1000ms / 10ms) = 2.55 steps per update
    // We'll use a slightly higher increment to ensure we hit 255
    float increment = 255.0 / (RAMP_DURATION_MS / 10.0);

    static float pwm_float = 0;

    if (ramping_up) {
      pwm_float += increment;
      if (pwm_float >= 255.0) {
        pwm_float = 255.0;
        ramping_up = false;
      }
    } else {
      pwm_float -= increment;
      if (pwm_float <= 0.0) {
        pwm_float = 0.0;
        ramping_up = true;
      }
    }

    current_pwm = (int)pwm_float;

    // Drive Motor in one direction
    analogWrite(PIN_PWM_A, current_pwm);
    analogWrite(PIN_PWM_B, 0);

    digitalWrite(PIN_LED1, current_pwm > 0 ? HIGH : LOW);

    // Log data
    Serial.print(current_pwm);
    Serial.print(", ");
    Serial.print(analogRead(PIN_BEMF_A));
    Serial.print(", ");
    Serial.print(analogRead(PIN_BEMF_B));
    Serial.println(", 0");
  }
}
