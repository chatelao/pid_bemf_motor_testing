#include <Arduino.h>

/**
 * Märklin Motor Test & Calibration Tool
 *
 * Phase 3: Basic Motor Control (PWM)
 * - Implement dual-channel PWM on D7/D8
 * - Basic open-loop motor drive functionality
 * - Status LED integration
 */

// Pin Definitions based on DESIGN.md and board specific variants
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #define PIN_PWM_A D7
  #define PIN_PWM_B D8
  #define PIN_LED1   PIN_LED_R // Status LED (Red)
  #define PIN_LED2   PIN_LED_B // Status LED (Blue)
#elif defined(ARDUINO_ARCH_STM32)
  #define PIN_PWM_A D7
  #define PIN_PWM_B D8
  #define PIN_LED1   LED_BUILTIN // Onboard LED for Nucleo
  #define PIN_LED2   D12         // External Status LED
#else
  #define PIN_PWM_A 7
  #define PIN_PWM_B 8
  #define PIN_LED1   13
  #define PIN_LED2   12
#endif

// Parameters
const uint32_t RAMP_DURATION_MS = 2000;
const uint32_t UPDATE_INTERVAL_MS = 20;

// State variables
uint32_t last_update = 0;
int current_pwm = 0;
bool ramping_up = true;
bool forward = true;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Set ADC to 12-bit resolution as per DESIGN.md
  analogReadResolution(12);

  // Set PWM frequency to 20kHz (ultrasonic) as per DESIGN.md
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  analogWriteFreq(20000);
#elif defined(ARDUINO_ARCH_STM32)
  analogWriteFrequency(20000);
#endif

  Serial.println("--- Märklin Motor Control Tool (Phase 3) ---");
  Serial.println("Open-loop PWM Ramp Test (Bidirectional)");
}

void loop() {
  uint32_t now = millis();

  if (now - last_update >= UPDATE_INTERVAL_MS) {
    last_update = now;

    // Calculate increment for a 2-second ramp (0-255)
    float increment = 255.0 * UPDATE_INTERVAL_MS / RAMP_DURATION_MS;

    static float pwm_f = 0;

    if (ramping_up) {
      pwm_f += increment;
      if (pwm_f >= 255.0) {
        pwm_f = 255.0;
        ramping_up = false;
        digitalWrite(PIN_LED2, HIGH);
      }
    } else {
      pwm_f -= increment;
      if (pwm_f <= 0.0) {
        pwm_f = 0.0;
        ramping_up = true;
        forward = !forward; // Switch direction
        digitalWrite(PIN_LED2, LOW);
      }
    }

    current_pwm = (int)pwm_f;

    // Dual-channel drive: PWM on one channel, 0 on the other
    if (forward) {
      analogWrite(PIN_PWM_A, current_pwm);
      analogWrite(PIN_PWM_B, 0);
    } else {
      analogWrite(PIN_PWM_A, 0);
      analogWrite(PIN_PWM_B, current_pwm);
    }

    // Status LED reflects motor activity
    digitalWrite(PIN_LED1, current_pwm > 0 ? LOW : HIGH); // Low is ON for XIAO onboard LEDs

    // Logging for telemetry
    if (current_pwm % 50 == 0) {
       Serial.print(forward ? "FWD " : "REV ");
       Serial.print("PWM: ");
       Serial.println(current_pwm);
    }
  }
}
