#include <Arduino.h>

/**
 * BEMF_Ramp_Test_v2.ino
 *
 * Deterministisches Charakterisierungs-Tool für Gleichstrom-/Märklin-Motoren.
 * Behebt UART-Buffer-Starvation, Rampen-Phasendrift und induktive Einschwingfehler.
 */

// Pin Definitions based on DESIGN.md
#define PIN_PWM_A   D7
#define PIN_PWM_B   D8
#define PIN_BEMF_A  A0
#define PIN_BEMF_B  A1
#define PIN_SHUNT   A2
#define PIN_LED1    15
#define PIN_LED2    16

// Timing & Ramp Parameters
const uint32_t RAMP_DURATION_MS     = 1000;  // Reine aktive Fahrzeit pro Flanke
const uint32_t MEASURE_INTERVAL_MS  =  250;  // Alle 250ms aktiver Rampenzeit messen
const uint32_t MEASURE_GAP_MS       =   25;  // Gesamtdauer der stromlosen Lücke
const uint32_t BEMF_SETTLE_US       =  800;  // 800us Abklingzeit für Ankerinduktivität (L * di/dt)

// Logging-Intervalle (verhindert UART/USB-Pufferüberlauf vor der wichtigen Messlücke)
const uint32_t LOG_INTERVAL_GAP_US  =   25;  // 40 kHz (25us) während der Messlücke
const uint32_t LOG_INTERVAL_NORM_US = 1000;  // 1 kHz (1000us) während normaler PWM-Fahrt

// E3 Frequency Series from 47 Hz to 100 kHz
const uint32_t PWM_FREQUENCIES[]    = {47, 100, 220, 470, 1000, 2200, 4700, 10000, 22000, 47000, 100000};
const size_t   NUM_FREQUENCIES      = sizeof(PWM_FREQUENCIES) / sizeof(PWM_FREQUENCIES[0]);

enum RampPhase : uint8_t {
  RAMP_FORWARD_UP,
  RAMP_FORWARD_DOWN,
  RAMP_BACKWARD_UP,
  RAMP_BACKWARD_DOWN
};

enum GapState : uint8_t {
  GAP_STATE_IDLE      = 0, // PWM aktiv
  GAP_STATE_SETTLING  = 1, // PWM = 0, Freilaufstrom klingt ab (ungültige BEMF)
  GAP_STATE_MEASURING = 2  // PWM = 0, eingeschwungene reine BEMF-Messung
};

// State Variables
uint32_t  last_tick_ms        = 0;
uint32_t  active_ramp_ms      = 0;  // Kumulierte aktive Rampenzeit (pausiert in Lücke)
uint32_t  ms_since_last_gap   = 0;  // Aktive Fahrzeit seit letzter Lücke
uint32_t  gap_start_us        = 0;
uint32_t  last_log_us         = 0;

int       current_pwm         = 0;
size_t    current_freq_index  = 0;
RampPhase ramp_phase          = RAMP_FORWARD_UP;
GapState  gap_state           = GAP_STATE_IDLE;

inline bool isForward(RampPhase phase) {
  return (phase == RAMP_FORWARD_UP || phase == RAMP_FORWARD_DOWN);
}

void applyMotorDrive(int pwm, bool forward, GapState g_state) {
  if (g_state != GAP_STATE_IDLE || pwm <= 0) {
    // High-Z / Coasting (Voraussetzung: H-Brücke schaltet bei 0/0 hochohmig!)
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);
  } else if (forward) {
    analogWrite(PIN_PWM_A, pwm);
    analogWrite(PIN_PWM_B, 0);
  } else {
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, pwm);
  }
}

void updatePwmFrequency(uint32_t freq) {
  analogWriteFreq(freq);
  // Nach Frequenzwechsel Duty-Cycle sofort neu verankern
  applyMotorDrive(current_pwm, isForward(ramp_phase), gap_state);
}

void setup() {
  Serial.begin(921600);
  while (!Serial && millis() < 2000);

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1,  OUTPUT);
  pinMode(PIN_LED2,  OUTPUT);

  analogReadResolution(12);
  analogWriteRange(255); // Stellt sicher, dass 8-Bit PWM-Skalierung (0..255) erhalten bleibt

  updatePwmFrequency(PWM_FREQUENCIES[current_freq_index]);

  Serial.println("--- Märklin Motor BEMF Characterization Tool v2 ---");
  Serial.println("Ramp: 1s UP, 1s DOWN (Active Drive Time, Paused During Gaps)");
  Serial.println("GAP_STATE: 0 = PWM_ACTIVE, 1 = INDUCTIVE_SETTLING, 2 = VALID_BEMF");
  Serial.println("Format: TIME_US,PWM_FREQ,PWM_DUTY,GAP_STATE,BEMF_A,BEMF_B,SHUNT");

  last_tick_ms = millis();
  last_log_us  = micros();
}

void loop() {
  uint32_t now_ms = millis();
  uint32_t now_us = micros();

  // =========================================================================
  // 1. MESSLÜCKEN-STATE-MACHINE (Mikrosekunden-Präzision für Settling)
  // =========================================================================
  switch (gap_state) {
    case GAP_STATE_IDLE:
      if (ms_since_last_gap >= MEASURE_INTERVAL_MS) {
        gap_state    = GAP_STATE_SETTLING;
        gap_start_us = now_us;
        applyMotorDrive(0, true, gap_state);
        digitalWrite(PIN_LED2, HIGH);
      }
      break;

    case GAP_STATE_SETTLING:
      if (now_us - gap_start_us >= BEMF_SETTLE_US) {
        gap_state = GAP_STATE_MEASURING;
      }
      break;

    case GAP_STATE_MEASURING:
      if (now_us - gap_start_us >= (MEASURE_GAP_MS * 1000UL)) {
        gap_state         = GAP_STATE_IDLE;
        ms_since_last_gap = 0; // Exaktes Zurücksetzen des aktiven Fahrzählers
        digitalWrite(PIN_LED2, LOW);
        applyMotorDrive(current_pwm, isForward(ramp_phase), gap_state);
      }
      break;
  }

  // =========================================================================
  // 2. RAMPEN-GENERATOR (Pausiert während der Messlücke -> Kein Phasendrift!)
  // =========================================================================
  uint32_t dt_ms = now_ms - last_tick_ms;
  if (dt_ms >= 5) { // 200 Hz Rampen-Update
    last_tick_ms = now_ms;

    if (gap_state == GAP_STATE_IDLE) {
      active_ramp_ms    += dt_ms;
      ms_since_last_gap += dt_ms;

      if (active_ramp_ms >= RAMP_DURATION_MS) {
        active_ramp_ms = 0;
        ms_since_last_gap = 0; // Synchronisiere Lücken-Timer an Phasengrenzen

        switch (ramp_phase) {
          case RAMP_FORWARD_UP:
            ramp_phase = RAMP_FORWARD_DOWN;
            break;
          case RAMP_FORWARD_DOWN:
            ramp_phase = RAMP_BACKWARD_UP;
            break;
          case RAMP_BACKWARD_UP:
            ramp_phase = RAMP_BACKWARD_DOWN;
            break;
          case RAMP_BACKWARD_DOWN:
            ramp_phase = RAMP_FORWARD_UP;
            current_freq_index = (current_freq_index + 1) % NUM_FREQUENCIES;
            updatePwmFrequency(PWM_FREQUENCIES[current_freq_index]);
            break;
        }
      }

      // Deterministische Integer-Berechnung ohne Float-Akkumulationsfehler
      int scaled_pwm = (active_ramp_ms * 255UL) / RAMP_DURATION_MS;
      if (ramp_phase == RAMP_FORWARD_UP || ramp_phase == RAMP_BACKWARD_UP) {
        current_pwm = scaled_pwm;
      } else {
        current_pwm = 255 - scaled_pwm;
      }

      applyMotorDrive(current_pwm, isForward(ramp_phase), gap_state);
      digitalWrite(PIN_LED1, current_pwm > 0 ? HIGH : LOW);
    }
  }

  // =========================================================================
  // 3. ADAPTIVES LOGGING (Verhindert Buffer-Starvation vor der Messlücke)
  // =========================================================================
  uint32_t target_log_interval = (gap_state == GAP_STATE_IDLE) ? LOG_INTERVAL_NORM_US : LOG_INTERVAL_GAP_US;

  if (now_us - last_log_us >= target_log_interval) {
    // Verhindert Burst-Nachlaufen (Windup), falls die Schleife einmal länger gedauert hat
    if (now_us - last_log_us > target_log_interval * 2) {
      last_log_us = now_us;
    } else {
      last_log_us += target_log_interval;
    }

    int bemfA = analogRead(PIN_BEMF_A);
    int bemfB = analogRead(PIN_BEMF_B);
    int shunt = analogRead(PIN_SHUNT);

    char log_buf[64];
    int signed_pwm = isForward(ramp_phase) ? current_pwm : -current_pwm;
    int len = snprintf(log_buf, sizeof(log_buf), "%lu,%lu,%d,%u,%d,%d,%d\n",
                       (unsigned long)now_us,
                       (unsigned long)PWM_FREQUENCIES[current_freq_index],
                       signed_pwm,
                       (uint8_t)gap_state,
                       bemfA, bemfB, shunt);

    if (len > 0 && Serial.availableForWrite() >= len) {
      Serial.write(log_buf, len);
    }
  }
}
