#include <Arduino.h>
#include <PID_v1.h>

/**
 * Märklin Motor Test & Calibration Tool
 *
 * Phase 4: BEMF Sensing & PID Implementation
 * - Implement synchronous ADC polling on A0/A1
 * - Integrate Arduino PID Library
 * - Implement closed-loop speed control
 */

// Pin Definitions based on DESIGN.md and board specific variants
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #define PIN_PWM_A D7
  #define PIN_PWM_B D8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   PIN_LED_R // Status LED (Red)
  #define PIN_LED2   PIN_LED_B // Status LED (Blue)
#elif defined(ARDUINO_ARCH_STM32)
  #define PIN_PWM_A D7
  #define PIN_PWM_B D8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   LED_BUILTIN // Onboard LED for Nucleo
  #define PIN_LED2   D12         // External Status LED
#else
  #define PIN_PWM_A 7
  #define PIN_PWM_B 8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   13
  #define PIN_LED2   12
#endif

// Parameters
const uint32_t CONTROL_INTERVAL_MS = 50;

// State variables
uint32_t last_control_time = 0;
bool forward = true;

// PID Variables
double pid_input = 0;
double pid_output = 0;
double pid_setpoint = 0;

// PID Tuning Parameters
// feedback range: 0-4095 (12-bit ADC), output range: 0-255 (8-bit PWM)
// Kp, Ki, Kd values chosen to balance speed and stability
double kp = 0.15;
double ki = 0.8;
double kd = 0.01;

PID myPID(&pid_input, &pid_output, &pid_setpoint, kp, ki, kd, DIRECT);

// Bidirectional Multi-step Setpoint Profile to demonstrate closed-loop tracking
struct ProfileStep {
  double target_bemf;
  bool direction_forward;
  uint32_t duration_ms;
};

const ProfileStep profile[] = {
  { 500.0,  true,  4000 },  // Speed 500 FWD
  { 1500.0, true,  4000 },  // Speed 1500 FWD
  { 2500.0, true,  4000 },  // Speed 2500 FWD
  { 1000.0, true,  4000 },  // Speed 1000 FWD
  { 0.0,    true,  2000 },  // Stop
  { 500.0,  false, 4000 },  // Speed 500 REV
  { 1500.0, false, 4000 },  // Speed 1500 REV
  { 2500.0, false, 4000 },  // Speed 2500 REV
  { 1000.0, false, 4000 },  // Speed 1000 REV
  { 0.0,    false, 2000 }   // Stop
};
const size_t PROFILE_STEPS_COUNT = sizeof(profile) / sizeof(profile[0]);

size_t current_step_index = 0;
uint32_t step_start_time = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_BEMF_A, INPUT);
  pinMode(PIN_BEMF_B, INPUT);
  pinMode(PIN_SHUNT, INPUT);
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

  // Initialize PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetSampleTime(CONTROL_INTERVAL_MS);

  last_control_time = millis();
  step_start_time = millis();

  // Set initial setpoint and direction from the profile
  pid_setpoint = profile[current_step_index].target_bemf;
  forward = profile[current_step_index].direction_forward;

  Serial.println("--- Märklin Motor Control Tool (Phase 4) ---");
  Serial.println("Closed-loop PID Speed Control with Synchronous BEMF Feedback");
  Serial.println("Format: SP:<setpoint>, BEMF:<measured>, PWM:<output>, DIR:<dir>");
}

void loop() {
  uint32_t now = millis();

  // 1. Update Profile Setpoint and Direction over time
  if (now - step_start_time >= profile[current_step_index].duration_ms) {
    step_start_time += profile[current_step_index].duration_ms; // Prevent drift in step timing
    current_step_index = (current_step_index + 1) % PROFILE_STEPS_COUNT;

    pid_setpoint = profile[current_step_index].target_bemf;
    forward = profile[current_step_index].direction_forward;

    Serial.print(">>> Step ");
    Serial.print(current_step_index);
    Serial.print(": Target BEMF = ");
    Serial.print(pid_setpoint);
    Serial.print(", Dir = ");
    Serial.println(forward ? "FWD" : "REV");
  }

  // 2. Closed-Loop control loop running at periodic intervals
  if (now - last_control_time >= CONTROL_INTERVAL_MS) {
    last_control_time += CONTROL_INTERVAL_MS; // Prevent cumulative timing drift

    // Synchronous ADC Polling:
    // Briefly turn off PWM to measure BEMF during off-time (coasting)
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);

    // LED2 is active during measurement gap
    digitalWrite(PIN_LED2, HIGH);

    // Wait 1.5ms for flyback / inductive current decay
    delayMicroseconds(1500);

    // Read BEMF on the coasting terminal
    int bemf_raw = 0;
    if (forward) {
      bemf_raw = analogRead(PIN_BEMF_A);
    } else {
      bemf_raw = analogRead(PIN_BEMF_B);
    }

    digitalWrite(PIN_LED2, LOW);

    // Apply PID Control
    if (pid_setpoint == 0.0) {
      // Force zero output and reset PID controller state to prevent crawling / integral windup
      pid_output = 0;
      myPID.SetMode(MANUAL);
      pid_output = 0;
      myPID.SetMode(AUTOMATIC);
    } else {
      pid_input = (double)bemf_raw;
      myPID.Compute();
    }

    // Apply computed PID PWM Output
    int current_pwm = (int)pid_output;
    if (forward) {
      analogWrite(PIN_PWM_A, current_pwm);
      analogWrite(PIN_PWM_B, 0);
    } else {
      analogWrite(PIN_PWM_A, 0);
      analogWrite(PIN_PWM_B, current_pwm);
    }

    // Status LED1 reflects motor activity
#if defined(ARDUINO_SEEED_XIAO_RP2040)
    digitalWrite(PIN_LED1, current_pwm > 0 ? LOW : HIGH); // Low is ON for XIAO onboard LEDs
#else
    digitalWrite(PIN_LED1, current_pwm > 0 ? HIGH : LOW);
#endif

    // Print Telemetry to Serial
    if (Serial) {
      Serial.print("SP:");
      Serial.print((int)pid_setpoint);
      Serial.print(", BEMF:");
      Serial.print(bemf_raw);
      Serial.print(", PWM:");
      Serial.print(current_pwm);
      Serial.print(", DIR:");
      Serial.println(forward ? "FWD" : "REV");
    }
  }
}