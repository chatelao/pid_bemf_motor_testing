#include <Arduino.h>

/**
 * BEMF_Ramp_Test.ino
 *
 * This sketch implements an open-loop PWM ramp to characterize motor BEMF
 * across an E3-like range of frequencies (47 Hz to 100 kHz).
 * It performs a 1s ramp up and 1s ramp down, running both forward and backward
 * for each frequency step before advancing to the next.
 * Every 250ms, it inserts a measurement gap (PWM = 0) to measure BEMF
 * without PWM interference.
 */

// Pin Definitions based on DESIGN.md
#define PIN_PWM_A  D7
#define PIN_PWM_B  D8
#define PIN_BEMF_A A0
#define PIN_BEMF_B A1
#define PIN_SHUNT  A2
#define PIN_LED1   15
#define PIN_LED2   16

// Parameters
const uint32_t RAMP_DURATION_MS    = 1000 ;
const uint32_t MEASURE_GAP_MS      =   25 ;  // Gap duration (1-50ms)
const uint32_t MEASURE_INTERVAL_MS =  250 ;  // Every 250ms

// E3 Frequency Series from 47 Hz to 100 kHz
const uint32_t PWM_FREQUENCIES[]   = {47, 100, 220, 470, 1000, 2200, 4700, 10000, 22000, 47000, 100000};
const size_t NUM_FREQUENCIES       = sizeof(PWM_FREQUENCIES) / sizeof(PWM_FREQUENCIES[0]);

// Enums for state machine readability
enum RampPhase {
  RAMP_FORWARD_UP,
  RAMP_FORWARD_DOWN,
  RAMP_BACKWARD_UP,
  RAMP_BACKWARD_DOWN
};

enum GapState {
  GAP_STATE_IDLE,
  GAP_STATE_ACTIVE
};

// State variables
uint32_t last_ramp_update   =     0 ;
uint32_t last_gap_time      =     0 ;
     int current_pwm        =     0 ;
  size_t current_freq_index =     0 ;
RampPhase ramp_phase        = RAMP_FORWARD_UP;
 GapState gap_state         = GAP_STATE_IDLE;
    bool in_gap             = false ;
uint32_t gap_start_ms       =     0 ;

void updatePwmFrequency(uint32_t freq) {
  analogWriteFreq(freq);
}

void setup() {
  Serial.begin(921600);
  while (!Serial && millis() < 2000); // Wait for Serial on USB boards

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1,  OUTPUT);
  pinMode(PIN_LED2,  OUTPUT);

  // Set ADC to 12-bit resolution as per DESIGN.md
  analogReadResolution(12);

  // Set initial PWM frequency
  updatePwmFrequency(PWM_FREQUENCIES[current_freq_index]);

  Serial.println("--- Märklin Motor BEMF Characterization Tool ---");
  Serial.println("Ramp: 1s UP, 1s DOWN (Bidirectional)");
  Serial.println("Frequency Sweep: E3 Series (47 Hz to 100 kHz)");
  Serial.print("Measurement Gap: "); Serial.print(MEASURE_GAP_MS); Serial.println("ms");
  Serial.println("Format: TIME_US, PWM_FREQ, PWM_DUTY, IS_GAP, BEMF_A, BEMF_B, SHUNT");
}

void loop() {
         uint32_t now         = millis();
  static uint32_t last_log_us = micros();

  bool forward = (ramp_phase == RAMP_FORWARD_UP || ramp_phase == RAMP_FORWARD_DOWN);

  // 40kHz Logging (every 25us) - Best effort non-blocking
  if (micros() - last_log_us >= 25) {
    last_log_us += 25;
    // Check if buffer has enough space for ~32 chars to avoid blocking
    if (Serial.availableForWrite() >= 32) {
      uint32_t now_us = micros();
      int bemfA = analogRead(PIN_BEMF_A);
      int bemfB = analogRead(PIN_BEMF_B);
      int shunt = analogRead(PIN_SHUNT);
      // Format: TIME_US, PWM_FREQ, PWM_DUTY, IS_GAP, BEMF_A, BEMF_B, SHUNT
      Serial.print(now_us);
      Serial.print(',');
      Serial.print(PWM_FREQUENCIES[current_freq_index]);
      Serial.print(',');
      Serial.print(forward ? current_pwm : -current_pwm);
      Serial.print(',');
      Serial.print(in_gap ? 1 : 0);
      Serial.print(',');
      Serial.print(bemfA);
      Serial.print(',');
      Serial.print(bemfB);
      Serial.print(',');
      Serial.println(shunt);
    }
  }

  // 1. Measurement Gap State Machine
  switch (gap_state) {
    case GAP_STATE_IDLE:
      if (now - last_gap_time >= MEASURE_INTERVAL_MS) {
        gap_state = GAP_STATE_ACTIVE;
        in_gap = true;
        gap_start_ms = now;
        analogWrite(PIN_PWM_A, 0);
        analogWrite(PIN_PWM_B, 0);
        digitalWrite(PIN_LED2, HIGH); // LED2 indicates gap
      }
      break;

    case GAP_STATE_ACTIVE:
      if (now - gap_start_ms >= MEASURE_GAP_MS) {
        gap_state = GAP_STATE_IDLE;
        in_gap = false;
        last_gap_time = now;
        digitalWrite(PIN_LED2, LOW);
        // Resume current PWM in correct direction
        if (forward) {
          analogWrite(PIN_PWM_A, current_pwm);
          analogWrite(PIN_PWM_B, 0);
        } else {
          analogWrite(PIN_PWM_A, 0);
          analogWrite(PIN_PWM_B, current_pwm);
        }
      }
      break;
  }

  // 2. Ramp Logic State Machine
  // Update every 10ms for a smooth ramp
  if (now - last_ramp_update >= 10) {
    last_ramp_update = now;

    // Calculate increment: 255 steps / (1000ms / 10ms) = 2.55 steps per update
    float increment = 255.0 / (RAMP_DURATION_MS / 10.0);
    static float pwm_float = 0;

    switch (ramp_phase) {
      case RAMP_FORWARD_UP:
        pwm_float += increment;
        if (pwm_float >= 255.0) {
          pwm_float = 255.0;
          ramp_phase = RAMP_FORWARD_DOWN;
        }
        break;

      case RAMP_FORWARD_DOWN:
        pwm_float -= increment;
        if (pwm_float <= 0.0) {
          pwm_float = 0.0;
          ramp_phase = RAMP_BACKWARD_UP;
        }
        break;

      case RAMP_BACKWARD_UP:
        pwm_float += increment;
        if (pwm_float >= 255.0) {
          pwm_float = 255.0;
          ramp_phase = RAMP_BACKWARD_DOWN;
        }
        break;

      case RAMP_BACKWARD_DOWN:
        pwm_float -= increment;
        if (pwm_float <= 0.0) {
          pwm_float = 0.0;
          ramp_phase = RAMP_FORWARD_UP;
          // Advance to next PWM frequency in E3 series
          current_freq_index = (current_freq_index + 1) % NUM_FREQUENCIES;
          updatePwmFrequency(PWM_FREQUENCIES[current_freq_index]);
        }
        break;
    }

    current_pwm = (int)pwm_float;

    // Only update motor drive if we are not in a measurement gap
    if (!in_gap) {
      if (forward) {
        analogWrite(PIN_PWM_A, current_pwm);
        analogWrite(PIN_PWM_B, 0);
      } else {
        analogWrite(PIN_PWM_A, 0);
        analogWrite(PIN_PWM_B, current_pwm);
      }
    }

    digitalWrite(PIN_LED1, current_pwm > 0 ? HIGH : LOW);
  }
}
