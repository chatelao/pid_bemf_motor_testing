#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

/**
 * BEMF_Ramp_Test_Closed_Loop.ino (v3.0 - Virtual Commutator Encoder & Closed-Loop Suite)
 *
 * Deterministisches Closed-Loop Regelungs-, Diagnose-, Auto-Tuning- und Odometrie-Tool
 * für unbekannte Gleichstrom-/Märklin-Modellbahnmotoren auf Basis des Seeed XIAO RP2040 & BDR-6133.
 *
 * NEU in v3.0 - Virtueller Encoder (Kommutator-Abrissfunken- & Schlitz-Detektor):
 *  1. Orthogonaler High-Pass Glitch-Detektor in der Messlücke (erkennt Bürsten-Abrissfunken & Kommutierungslücken)
 *  2. Dynamische Hysterese & drehzahlabhängige Totzeit (Blanking Time gegen Mehrfach-Prellen einer Lamelle)
 *  3. High-Speed Shunt-Ripple-Abtastung während der aktiven PWM-Fahrt (erkennt Strom-Kommutierungseinbrüche)
 *  4. Phasen-gekoppelter Hybrid-Odometrie-Beobachter (fusioniert echte Funken-Impulse mit der bEMF-Kinematik,
 *     um die 93% Messlücken-Blindzeit verlustfrei zu überbrücken)
 *  5. Getriebespiel-Kompensation (Backlash-Deadband) bei Richtungswechseln für präzise Netto-Distanz
 *  6. Live-Distanz-Bilanzierung: Prüft nach jedem Vorwärts-/Rückwärts-Zyklus den absoluten Positions-Restfehler
 */

// =========================================================================
// 1. PIN DEFINITIONEN (XIAO RP2040 / DESIGN.md)
// =========================================================================
#define PIN_PWM_A         D7   // BDR-6133 InA
#define PIN_PWM_B         D8   // BDR-6133 InB
#define PIN_BEMF_A        A0   // bEMF Messeingang A
#define PIN_BEMF_B        A1   // bEMF Messeingang B
#define PIN_SHUNT         A2   // Motor Shunt / Strommessung
#define PIN_TRIG_F0F      D9   // Oszilloskop Trigger 1: HIGH = Exaktes bEMF-ADC-Messfenster
#define PIN_TRIG_F0B      D10  // Oszilloskop Trigger 2: Toggelt bei jedem erkannten Kommutator-Funken!
#define PIN_NEOPIXEL_PWR  11   // XIAO RP2040 NeoPixel Power Enable
#define PIN_NEOPIXEL_DATA 12   // XIAO RP2040 NeoPixel Data
#define PIN_LED1          15   // Status LED 1 (Drive Active)
#define PIN_LED2          16   // Status LED 2 (Gap Active)

Adafruit_NeoPixel pixels(1, PIN_NEOPIXEL_DATA, NEO_GRB + NEO_KHZ400);

// =========================================================================
// 2. KONFIGURATION & TUNING-STRUKTUR (Zur Laufzeit über CLI änderbar)
// =========================================================================
enum OperatingMode : uint8_t {
  MODE_AUTO_TUNE        = 0, // Automatische Identifikation (Stiction, Kv, Tau_m, PI & Funken-Kalibrierung)
  MODE_CLOSED_LOOP_RAMP = 1, // Geregelte bEMF-Rampe mit aktivem Virtuellem Encoder
  MODE_STEP_RESPONSE    = 2, // Sprungantwort-Test (25% -> 75% -> 25%)
  MODE_OPEN_LOOP_RAMP   = 3, // Klassische PWM-gesteuerte Rampe zum Vergleich
  MODE_POSITION_TARGET  = 4  // NEU: Fährt exakt auf eine Ziel-Distanz (in virtuellen Encoder-Schritten) und bremst ab!
};

struct MotorConfig {
  uint32_t pwm_freq_hz          = 20000;   // PWM-Frequenz (Hz)
  uint32_t ramp_duration_ms     = 2500;    // Reine aktive Fahrzeit pro Flanke (ms)
  uint32_t control_interval_ms  = 20;      // Regler-Abtastintervall (20ms = 50 Hz)
  uint32_t measure_gap_us       = 2000;    // Gesamtdauer der stromlosen Messlücke (us)
  uint32_t bemf_settle_us       = 650;     // Abklingzeit für Ankerinduktivität L*di/dt (us)
  
  float    max_expected_bemf    = 2400.0f; // 12-Bit ADC bEMF bei 100% Solldrehzahl
  float    kp                   = 0.10f;   // Proportional-Verstärkung (PWM / ADC-Count)
  float    ki                   = 1.50f;   // Integral-Verstärkung (PWM / (ADC-Count * s))
  float    kv_ff                = 0.075f;  // Lineare Vorsteuerung (PWM / ADC-Count)
  float    coulomb_ff_pwm       = 18.0f;   // Statische Reibungskompensation (PWM-Offset [0..255])
  float    ema_alpha            = 0.65f;   // Zeitlicher Tiefpass für bEMF
  
  // Parameter für den Virtuellen Kommutator-Encoder
  uint8_t  comm_slots_per_rev   = 6;       // Kommutator-Impulse pro Umdrehung (6 für 3-Pol, 10 für 5-Pol HLA)
  float    spark_threshold_adc  = 38.0f;   // Mindest-Spannungssprung (ADC-Counts) für Abrissfunken-Erkennung
  uint32_t spark_blanking_us    = 180;     // Totzeit nach erkanntem Funken gegen Bürsten-Prellen (us)
  float    bemf_to_slot_rate    = 0.42f;   // Kalibrierfaktor: Kommutator-Schlitze pro Sekunde pro bEMF-ADC-Count
  float    pll_spark_trust      = 0.35f;   // Gewichtung (0..1): Wie stark echte Funken die bEMF-Integration korrigieren
  int32_t  gear_backlash_steps  = 12;      // Getriebespiel (Schritte, die bei Richtungswechsel leer durchlaufen)
  int32_t  target_pos_steps     = 1200;    // Ziel-Distanz im Modus MODE_POSITION_TARGET (M4)
  
  uint8_t  num_speed_levels     = 16;      // Anzahl Geschwindigkeitsstufen
  bool     use_discrete_steps   = true;    // true = Stufenrampe, false = glatte Rampe
  bool     verbose_csv          = true;    // CSV-Streaming ein/aus
} cfg;

// Logging-Intervalle & Puffer
const uint32_t LOG_INTERVAL_GAP_US  = 50;   // 20 kHz während der Messlücke
const uint32_t LOG_INTERVAL_NORM_US = 2500; // 400 Hz während der aktiven PWM-Fahrt
const size_t   MAX_GAP_SAMPLES      = 64;   // Puffergröße für Trimmed-Mean Filter in der Lücke

// =========================================================================
// 3. ZUSTANDSMASCHINEN & GLOBALE VARIABLEN
// =========================================================================
enum RampPhase : uint8_t {
  RAMP_FORWARD_UP,
  RAMP_FORWARD_DOWN,
  RAMP_BACKWARD_UP,
  RAMP_BACKWARD_DOWN
};

enum GapState : uint8_t {
  GAP_STATE_IDLE      = 0, // PWM aktiv (Fahrt)
  GAP_STATE_SETTLING  = 1, // PWM = 0 (High-Z), Freilaufstrom klingt ab
  GAP_STATE_MEASURING = 2  // PWM = 0 (High-Z), Trimmed-Mean bEMF & Funken-Sampling
};

enum AutoTuneStage : uint8_t {
  TUNE_INIT = 0,
  TUNE_STICTION_UP,
  TUNE_HIGH_SPEED_SETTLE,
  TUNE_STEP_DYNAMICS,
  TUNE_COAST_DOWN,
  TUNE_COMPLETE
};

// Virtueller Encoder / Odometrie-Struktur
struct VirtualEncoder {
  // 1. Direkte physikalische Zähler (Roh-Impulse aus Funken & Stromrippel)
  uint32_t raw_gap_sparks_total    = 0;    // Summe aller in der Messlücke erkannten Abrissfunken
  uint32_t raw_shunt_ripples_total = 0;    // Summe aller während der Fahrt im Shunt erkannten Strom-Dips
  uint8_t  sparks_in_current_gap   = 0;    // Funken im aktuellen 1.4ms Messfenster
  uint8_t  ripples_in_current_drive= 0;    // Shunt-Impulse im aktuellen 20ms Drive-Fenster

  // 2. Hochpass-Zustand für die Flankenerkennung in der Lücke
  float    fast_baseline_bemf      = 0.0f; // Gleitender Kurzzeit-Mittelwert innerhalb der Lücke
  uint32_t last_spark_us           = 0;    // Zeitstempel des letzten gültigen Abrissfunkens
  bool     in_spark_glitch         = false;// Hysterese-Zustand (Schmitt-Trigger)
  bool     trig_pin_state          = false;// Zustand für Oszilloskop-Toggle an Pin D10

  // 3. Shunt-Ripple-Hochpass während der aktiven PWM-Fahrt
  float    shunt_fast_avg          = 0.0f;
  uint32_t last_shunt_pulse_us     = 0;
  bool     in_shunt_dip            = false;

  // 4. Phasen-gekoppelter Hybrid-Odometrie-Zustand (Absolute Distanz)
  float    sub_step_accumulator    = 0.0f; // Bruchteile eines Kommutator-Schlitzes
  int32_t  raw_motor_steps         = 0;    // Kumulierte Anker-Kommutierungsschritte (vor Getriebespiel)
  int32_t  net_track_distance      = 0;    // Absolute Schienen-Distanz (kompensiert um Getriebespiel)
  int32_t  backlash_remaining      = 0;    // Verbleibende Leerweg-Schritte nach Richtungswechsel
  bool     last_direction_fwd      = true;
  float    estimated_rpm           = 0.0f; // Geschätzte Motordrehzahl in U/min (RPM)
} enc;

OperatingMode op_mode          = MODE_AUTO_TUNE;
RampPhase     ramp_phase       = RAMP_FORWARD_UP;
GapState      gap_state        = GAP_STATE_IDLE;
AutoTuneStage tune_stage       = TUNE_INIT;

uint32_t last_tick_ms          = 0;
uint32_t active_ramp_ms        = 0;
uint32_t ms_since_last_gap     = 0;
uint32_t gap_start_us          = 0;
uint32_t last_log_us           = 0;
uint32_t last_shunt_poll_us    = 0;

// Mess-Puffer & Diagnose-Metriken während der Messlücke
uint16_t gap_samples[MAX_GAP_SAMPLES];
uint8_t  gap_sample_count      = 0;
int32_t  first_half_accum      = 0;
int32_t  second_half_accum     = 0;
uint8_t  first_half_count      = 0;
uint8_t  second_half_count     = 0;

float    settling_delta_bemf   = 0.0f;
float    gap_noise_spread      = 0.0f;
int      active_shunt_adc      = 0;

// Regelgrößen (Closed-Loop)
float    target_bemf           = 0.0f;
float    measured_bemf         = 0.0f;
float    pi_integral           = 0.0f;
int      current_pwm           = 0;
uint8_t  current_step_level    = 0;
bool     is_saturated          = false;
bool     pos_dir_forward       = true; // Aktuelle Richtung im Modus MODE_POSITION_TARGET

// Auto-Tune Messspeicher
uint32_t tune_timer_ms         = 0;
int      tune_pwm              = 0;
float    detected_stiction_pwm = 20.0f;
float    bemf_at_pwm100        = 0.0f;
float    bemf_at_pwm200        = 0.0f;
float    step_t63_ms           = 60.0f;
bool     step_t63_captured     = false;
uint32_t tune_spark_count      = 0;
uint32_t tune_measure_window_us= 0;

// CLI Ringbuffer
char     cli_buf[32];
uint8_t  cli_idx               = 0;

// =========================================================================
// 4. HARDWARE-ABSTRAKTION & VIRTUELLER ENCODER (SIGNALVERARBEITUNG)
// =========================================================================
inline bool isActiveForward() {
  if (op_mode == MODE_AUTO_TUNE) return true;
  if (op_mode == MODE_POSITION_TARGET) return pos_dir_forward;
  return (ramp_phase == RAMP_FORWARD_UP || ramp_phase == RAMP_FORWARD_DOWN);
}

void updateNeoPixel(bool forward, int pwm, bool saturated, OperatingMode mode) {
  if (mode == MODE_AUTO_TUNE) {
    pixels.setPixelColor(0, pixels.Color(180, 120, 0)); // Gelb/Orange = Auto-Tuning
  } else if (mode == MODE_POSITION_TARGET && pwm == 0) {
    pixels.setPixelColor(0, pixels.Color(0, 220, 220)); // Cyan = Zielposition exakt erreicht!
  } else if (saturated) {
    pixels.setPixelColor(0, pixels.Color(255, 0, 255)); // Magenta = Sättigung
  } else if (pwm == 0) {
    pixels.setPixelColor(0, pixels.Color(0, 0, 120));   // Blau = Stillstand
  } else if (forward) {
    pixels.setPixelColor(0, pixels.Color(pwm, 0, 0));   // Rot = Vorwärts
  } else {
    pixels.setPixelColor(0, pixels.Color(0, pwm, 0));   // Grün = Rückwärts
  }
  pixels.show();
}

void applyMotorDrive(int pwm, bool forward, GapState g_state) {
  if (g_state != GAP_STATE_IDLE || pwm <= 0) {
    analogWrite(PIN_PWM_A, 0);
    analogWrite(PIN_PWM_B, 0);
  } else {
    int clamped_pwm = constrain(pwm, 0, 255);
    if (forward) {
      analogWrite(PIN_PWM_A, clamped_pwm);
      analogWrite(PIN_PWM_B, 0);
    } else {
      analogWrite(PIN_PWM_A, 0);
      analogWrite(PIN_PWM_B, clamped_pwm);
    }
  }
  // Oszilloskop Trigger 1 (D9): HIGH während GAP_STATE_MEASURING
  digitalWrite(PIN_TRIG_F0F, (g_state == GAP_STATE_MEASURING) ? HIGH : LOW);
}

/**
 * Fügt dem virtuellen Encoder diskrete Schritte hinzu und berücksichtigt das
 * mechanische Getriebespiel (Backlash) bei Richtungswechseln.
 */
void advanceVirtualEncoderSteps(int32_t delta_steps, bool forward) {
  if (delta_steps <= 0) return;

  // Richtungswechsel-Detektion -> Getriebespiel-Zähler laden
  if (forward != enc.last_direction_fwd) {
    enc.last_direction_fwd = forward;
    enc.backlash_remaining = cfg.gear_backlash_steps;
  }

  int32_t sign = forward ? 1 : -1;
  enc.raw_motor_steps += (sign * delta_steps);

  // Getriebespiel (Schnecke/Stirnrad) abziehen, bevor sich das Rad auf der Schiene bewegt
  if (enc.backlash_remaining > 0) {
    if (delta_steps <= enc.backlash_remaining) {
      enc.backlash_remaining -= delta_steps;
      delta_steps = 0;
    } else {
      delta_steps -= enc.backlash_remaining;
      enc.backlash_remaining = 0;
    }
  }

  enc.net_track_distance += (sign * delta_steps);
}

/**
 * Hochpass-Flankendetektor für Bürsten-Abrissfunken / Kommutierungslücken in der Messlücke.
 * Arbeitet orthogonal zum Trimmed-Mean-Filter auf den Einzel-Samples.
 */
void detectGapCommutatorSpark(int diff_bemf, uint32_t now_us) {
  if (gap_sample_count == 1) {
    // Initialisiere lokale Baseline mit dem ersten Sample nach dem Settling
    enc.fast_baseline_bemf = (float)diff_bemf;
    enc.in_spark_glitch    = false;
    return;
  }

  // Hochpass: Differenz zwischen aktuellem Sample und gleitender lokaler Baseline
  // Ein Bürsten-Abrissfunke oder Kommutator-Kurzschluss erzeugt einen steilen Einbruch/Sprung
  float highpass_glitch = fabsf((float)diff_bemf - enc.fast_baseline_bemf);

  // Adaptive Schwelle: Skaliert leicht mit der Grunddrehzahl, mindestens aber cfg.spark_threshold_adc
  float dyn_threshold = max(cfg.spark_threshold_adc, enc.fast_baseline_bemf * 0.045f);

  if (!enc.in_spark_glitch) {
    // Prüfe auf Überschreiten der oberen Schmitt-Trigger-Schwelle + Blanking-Zeit
    if (highpass_glitch >= dyn_threshold && (now_us - enc.last_spark_us >= cfg.spark_blanking_us)) {
      enc.in_spark_glitch = true;
      enc.last_spark_us   = now_us;
      enc.sparks_in_current_gap++;
      enc.raw_gap_sparks_total++;

      // Toggle Oszilloskop Pin D10 bei jedem erkannten Kommutator-Ereignis zur Verifikation!
      enc.trig_pin_state = !enc.trig_pin_state;
      digitalWrite(PIN_TRIG_F0B, enc.trig_pin_state ? HIGH : LOW);
    } else {
      // Baseline nur nachführen, wenn wir uns NICHT mitten in einem Abrissfunken befinden
      enc.fast_baseline_bemf = (0.25f * (float)diff_bemf) + (0.75f * enc.fast_baseline_bemf);
    }
  } else {
    // Untere Schmitt-Trigger-Hysterese (50% der Schwelle) zum Scharfschalten für die nächste Lamelle
    if (highpass_glitch < (dyn_threshold * 0.50f)) {
      enc.in_spark_glitch = false;
    }
  }
}

/**
 * Abtastung des Shunt-Stromrippels während der aktiven PWM-Fahrt.
 * Wenn die Bürste zwei Kommutatorlamellen kurzschließt, sinkt kurzzeitig die Gegen-EMK
 * und verursacht eine charakteristische Stromspitze am Shunt.
 */
void pollActiveShuntRipple(uint32_t now_us) {
  if (current_pwm < 25 || gap_state != GAP_STATE_IDLE) return;
  if (now_us - last_shunt_poll_us < 100) return; // 10 kHz Abtastung während der Fahrt
  last_shunt_poll_us = now_us;

  int shunt_raw = analogRead(PIN_SHUNT);
  active_shunt_adc = shunt_raw;

  float delta_shunt = fabsf((float)shunt_raw - enc.shunt_fast_avg);
  enc.shunt_fast_avg = (0.15f * (float)shunt_raw) + (0.85f * enc.shunt_fast_avg);

  // Dynamische Totzeit abhängig von der aktuellen Drehzahl (verhindert PWM-Schaltrippel-Aliasing)
  uint32_t min_period_us = (measured_bemf > 100.0f) ?
                           constrain((uint32_t)(600000.0f / measured_bemf), 250UL, 8000UL) : 2000UL;

  if (!enc.in_shunt_dip) {
    if (delta_shunt > 28.0f && (now_us - enc.last_shunt_pulse_us >= min_period_us)) {
      enc.in_shunt_dip         = true;
      enc.last_shunt_pulse_us  = now_us;
      enc.ripples_in_current_drive++;
      enc.raw_shunt_ripples_total++;
    }
  } else if (delta_shunt < 12.0f) {
    enc.in_shunt_dip = false;
  }
}

/**
 * Phasen-gekoppeltes Update des Virtuellen Encoders am Ende jeder Messlücke.
 * Verbindet die hohe zeitliche Auflösung der bEMF-Integration (Prädiktor) mit den
 * tatsächlich gezählten Abrissfunken in der Lücke und den Shunt-Rippeln (Korrektor).
 */
void updateHybridVirtualEncoder(float dt_sec, uint32_t valid_gap_window_us, bool forward) {
  if (measured_bemf < 15.0f && current_pwm == 0) {
    enc.estimated_rpm = 0.0f;
    return;
  }

  // 1. Kinematischer Prädiktor über bEMF (überbrückt die 93% Zeit außerhalb der Messlücke)
  float predicted_slot_rate_hz = measured_bemf * cfg.bemf_to_slot_rate;
  float predicted_steps        = predicted_slot_rate_hz * dt_sec;

  // 2. Direkte Funken-Messung hochgerechnet aus dem aktuellen Messfenster + Shunt-Rippel
  float measured_steps_corr = predicted_steps;
  if (enc.sparks_in_current_gap > 0 && valid_gap_window_us > 200) {
    // Extrapoliere die im kurzen Messfenster beobachtete Funkendichte, begrenzt auf plausible Physik
    float instant_gap_rate_hz = ((float)enc.sparks_in_current_gap * 1000000.0f) / (float)valid_gap_window_us;
    float max_plausible_hz    = predicted_slot_rate_hz * 2.2f + 30.0f;
    instant_gap_rate_hz       = min(instant_gap_rate_hz, max_plausible_hz);
    measured_steps_corr       = instant_gap_rate_hz * dt_sec;
  } else if (enc.ripples_in_current_drive > 0) {
    // Falls kein Funke in die 1.4ms Lücke fiel, ziehe die gezählten Shunt-Rippel der Drive-Phase heran
    measured_steps_corr = (float)enc.ripples_in_current_drive;
  }

  // 3. Komplementär-Fusion (PLL-Gewichtung zwischen Funken-Ereignis und bEMF-Integral)
  float fused_steps = ((1.0f - cfg.pll_spark_trust) * predicted_steps) +
                      (cfg.pll_spark_trust * measured_steps_corr);

  enc.sub_step_accumulator += fused_steps;
  int32_t whole_steps = (int32_t)enc.sub_step_accumulator;
  if (whole_steps > 0) {
    enc.sub_step_accumulator -= (float)whole_steps;
    advanceVirtualEncoderSteps(whole_steps, forward);
  }

  // Drehzahlberechnung in RPM: (Schritte/Sekunde * 60) / Kommutator-Schlitze pro Umdrehung
  float effective_rate_hz = fused_steps / max(0.001f, dt_sec);
  enc.estimated_rpm = (effective_rate_hz * 60.0f) / (float)max((uint8_t)1, cfg.comm_slots_per_rev);
}

/**
 * Interquartil-Trimmed-Mean Filter für den PI-Drehzahlregler (unverändert robust gegen Funken).
 */
float computeTrimmedMeanAndSpread(uint16_t* buf, uint8_t count, float &out_iqr_spread) {
  if (count == 0) {
    out_iqr_spread = 0.0f;
    return 0.0f;
  }
  for (uint8_t i = 1; i < count; i++) {
    uint16_t key = buf[i];
    int8_t j = i - 1;
    while (j >= 0 && buf[j] > key) {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }

  uint8_t trim = count / 4;
  uint8_t start_idx = trim;
  uint8_t end_idx   = count - trim;
  if (end_idx <= start_idx) {
    start_idx = 0;
    end_idx   = count;
  }

  out_iqr_spread = (float)(buf[end_idx - 1] - buf[start_idx]);

  uint32_t sum = 0;
  for (uint8_t i = start_idx; i < end_idx; i++) {
    sum += buf[i];
  }
  return (float)sum / (float)(end_idx - start_idx);
}

// =========================================================================
// 5. SOLLWERT-GENERATOR, POSITIONS-REGLER & PI-REGLER
// =========================================================================
void updateTargetSpeedLevel() {
  if (op_mode == MODE_POSITION_TARGET) {
    // Kaskadierter Positionsregler (P-Glied mit Bremsrampe auf den virtuellen Encoder-Schritten)
    int32_t pos_error = cfg.target_pos_steps - enc.net_track_distance;
    if (abs(pos_error) <= 3) {
      // Zielfenster erreicht -> Motor präzise anhalten!
      target_bemf        = 0.0f;
      current_step_level = 0;
      return;
    }
    pos_dir_forward = (pos_error > 0);
    // Quadratwurzel-Bremsprofil für sanftes Einlaufen in den Zielpunkt
    float dist_rem = (float)abs(pos_error);
    float desired_norm = constrain(sqrtf(dist_rem / 180.0f), 0.12f, 0.85f);
    current_step_level = (uint8_t)(desired_norm * cfg.num_speed_levels);
    target_bemf        = desired_norm * cfg.max_expected_bemf;
    return;
  }

  if (op_mode == MODE_STEP_RESPONSE) {
    bool high_step = ((active_ramp_ms / 1000) % 2) == 1;
    current_step_level = high_step ? 12 : 4;
    target_bemf = (high_step ? 0.75f : 0.25f) * cfg.max_expected_bemf;
    return;
  }

  float progress = (float)active_ramp_ms / (float)cfg.ramp_duration_ms;
  if (progress > 1.0f) progress = 1.0f;

  bool ramp_up = (ramp_phase == RAMP_FORWARD_UP || ramp_phase == RAMP_BACKWARD_UP);
  float norm_level = ramp_up ? progress : (1.0f - progress);

  if (cfg.use_discrete_steps) {
    current_step_level = (uint8_t)(norm_level * cfg.num_speed_levels + 0.5f);
    target_bemf = ((float)current_step_level / (float)cfg.num_speed_levels) * cfg.max_expected_bemf;
  } else {
    current_step_level = (uint8_t)(norm_level * cfg.num_speed_levels);
    target_bemf = norm_level * cfg.max_expected_bemf;
  }
}

void executeClosedLoopControl(float dt_sec) {
  if (op_mode == MODE_OPEN_LOOP_RAMP) {
    current_pwm  = (int)((target_bemf / cfg.max_expected_bemf) * 255.0f + 0.5f);
    pi_integral  = 0.0f;
    is_saturated = false;
    return;
  }

  if (target_bemf <= 12.0f) {
    pi_integral  = 0.0f;
    current_pwm  = 0;
    is_saturated = false;
    return;
  }

  float error   = target_bemf - measured_bemf;
  float ff_term = cfg.coulomb_ff_pwm + (target_bemf * cfg.kv_ff);
  float p_term  = cfg.kp * error;

  float new_integral = pi_integral + (cfg.ki * error * dt_sec);
  float raw_pwm      = ff_term + p_term + new_integral;

  if (raw_pwm >= 255.0f) {
    raw_pwm      = 255.0f;
    is_saturated = true;
    if (error < 0.0f) pi_integral = new_integral;
  } else if (raw_pwm <= 0.0f) {
    raw_pwm      = 0.0f;
    is_saturated = true;
    if (error > 0.0f) pi_integral = new_integral;
  } else {
    pi_integral  = new_integral;
    is_saturated = false;
  }

  current_pwm = (int)(raw_pwm + 0.5f);
}

// =========================================================================
// 6. AUTOMATISCHE MOTOR- & ENCODER-CHARAKTERISIERUNG
// =========================================================================
void resetVirtualEncoder() {
  enc.raw_gap_sparks_total     = 0;
  enc.raw_shunt_ripples_total  = 0;
  enc.sub_step_accumulator     = 0.0f;
  enc.raw_motor_steps          = 0;
  enc.net_track_distance       = 0;
  enc.backlash_remaining       = 0;
}

void startAutoTune() {
  op_mode                = MODE_AUTO_TUNE;
  tune_stage             = TUNE_INIT;
  tune_timer_ms          = 0;
  tune_pwm               = 0;
  measured_bemf          = 0.0f;
  step_t63_captured      = false;
  tune_spark_count       = 0;
  tune_measure_window_us = 0;
  resetVirtualEncoder();
  Serial.println(F("\n# === AUTO-TUNE STARTED: IDENTIFYING MOTOR & VIRTUAL ENCODER ==="));
}

void runAutoTuneStep(uint32_t dt_ms) {
  tune_timer_ms += dt_ms;

  switch (tune_stage) {
    case TUNE_INIT:
      tune_pwm      = 5;
      tune_timer_ms = 0;
      tune_stage    = TUNE_STICTION_UP;
      break;

    case TUNE_STICTION_UP:
      if (tune_timer_ms >= 40) {
        tune_timer_ms = 0;
        if (measured_bemf > 45.0f) {
          detected_stiction_pwm = (float)tune_pwm;
          cfg.coulomb_ff_pwm    = detected_stiction_pwm * 0.82f;
          Serial.print(F("# [1/4] Breakaway PWM (Stiction) = "));
          Serial.println(detected_stiction_pwm, 1);
          tune_pwm   = 100;
          tune_stage = TUNE_HIGH_SPEED_SETTLE;
        } else {
          tune_pwm += 2;
          if (tune_pwm >= 180) {
            Serial.println(F("# [WARN] Motor stall or disconnected! Using fallback stiction."));
            detected_stiction_pwm = 25.0f;
            cfg.coulomb_ff_pwm    = 20.0f;
            tune_pwm              = 100;
            tune_stage            = TUNE_HIGH_SPEED_SETTLE;
          }
        }
      }
      current_pwm = tune_pwm;
      break;

    case TUNE_HIGH_SPEED_SETTLE:
      current_pwm = 100;
      if (tune_timer_ms >= 600) {
        bemf_at_pwm100         = measured_bemf;
        tune_timer_ms          = 0;
        step_t63_captured      = false;
        tune_spark_count       = enc.raw_gap_sparks_total;
        tune_measure_window_us = 0;
        tune_stage             = TUNE_STEP_DYNAMICS;
      }
      break;

    case TUNE_STEP_DYNAMICS:
      current_pwm = 200;
      if (!step_t63_captured && bemf_at_pwm100 > 10.0f) {
        float expected_target = bemf_at_pwm100 * 2.1f;
        float thresh_63 = bemf_at_pwm100 + 0.632f * (expected_target - bemf_at_pwm100);
        if (measured_bemf >= thresh_63) {
          step_t63_ms       = (float)tune_timer_ms;
          step_t63_captured = true;
        }
      }

      if (tune_timer_ms >= 800) {
        bemf_at_pwm200 = measured_bemf;
        if (fabsf(settling_delta_bemf) > 35.0f && cfg.bemf_settle_us < 1100) {
          cfg.bemf_settle_us += 250;
        }
        // Kalibriere die Funken-zu-bEMF-Rate für den Virtuellen Encoder
        uint32_t sparks_observed = enc.raw_gap_sparks_total - tune_spark_count;
        if (sparks_observed >= 3 && tune_measure_window_us > 5000 && bemf_at_pwm200 > 150.0f) {
          float obs_spark_hz = ((float)sparks_observed * 1000000.0f) / (float)tune_measure_window_us;
          cfg.bemf_to_slot_rate = constrain(obs_spark_hz / bemf_at_pwm200, 0.08f, 2.50f);
        }
        // Passe Funken-Schwellenwert automatisch an das gemessene Interquartils-Rauschen an
        if (gap_noise_spread > 10.0f) {
          cfg.spark_threshold_adc = constrain(gap_noise_spread * 0.85f, 18.0f, 140.0f);
        }
        tune_timer_ms = 0;
        tune_stage    = TUNE_COAST_DOWN;
      }
      break;

    case TUNE_COAST_DOWN:
      current_pwm = 0;
      if (tune_timer_ms >= 400) {
        tune_stage = TUNE_COMPLETE;
      }
      break;

    case TUNE_COMPLETE:
      {
        float delta_bemf = bemf_at_pwm200 - bemf_at_pwm100;
        if (delta_bemf < 50.0f) delta_bemf = 800.0f;

        float plant_gain_k = delta_bemf / 100.0f;
        cfg.kv_ff = (1.0f / plant_gain_k) * 0.85f;
        cfg.max_expected_bemf = constrain(bemf_at_pwm200 * 1.15f, 400.0f, 3600.0f);

        float tau_m_sec = constrain(step_t63_ms * 0.001f, 0.025f, 0.500f);
        float tau_c_sec = max(0.035f, tau_m_sec * 0.6f);

        cfg.kp = (tau_m_sec / (plant_gain_k * tau_c_sec));
        cfg.ki = cfg.kp / tau_m_sec;

        Serial.println(F("# === AUTO-TUNE & VIRTUAL ENCODER REPORT ==="));
        Serial.print(F("# Plant Gain K (BEMF/PWM) : ")); Serial.println(plant_gain_k, 2);
        Serial.print(F("# Mech. Time Const Tau_m  : ")); Serial.print(tau_m_sec * 1000.0f, 1); Serial.println(F(" ms"));
        Serial.print(F("# Spark Threshold ADC     : ")); Serial.println(cfg.spark_threshold_adc, 1);
        Serial.print(F("# Calib. Slot/BEMF Ratio  : ")); Serial.print(cfg.bemf_to_slot_rate, 3); Serial.println(F(" Hz/ADC"));
        Serial.print(F("# Tuned KP / KI / KV_FF   : "));
        Serial.print(cfg.kp, 4); Serial.print(F(" / "));
        Serial.print(cfg.ki, 4); Serial.print(F(" / "));
        Serial.println(cfg.kv_ff, 4);
        Serial.println(F("# Resetting Virtual Encoder to 0 and starting MODE_CLOSED_LOOP_RAMP..."));
        Serial.println(F("# Format: TIME_US,MODE,STEP_LVL,TARGET_BEMF,MEAS_BEMF,PWM_DUTY,SPARKS_GAP,DIST_STEPS,EST_RPM,SHUNT_ACT"));

        resetVirtualEncoder();
        active_ramp_ms    = 0;
        ms_since_last_gap = 0;
        pi_integral       = 0.0f;
        ramp_phase        = RAMP_FORWARD_UP;
        op_mode           = MODE_CLOSED_LOOP_RAMP;
      }
      break;
  }
}

// =========================================================================
// 7. INTERAKTIVES NON-BLOCKING SERIAL CLI (Inkl. Encoder- & Distanz-Steuerung)
// =========================================================================
void printHelpAndStatus() {
  Serial.println(F("\n# --- INTERACTIVE MOTOR & VIRTUAL ENCODER CLI (v3.0) ---"));
  Serial.println(F("# Commands (send with newline):"));
  Serial.println(F("#   M0..M4   : Mode (0=AutoTune, 1=ClosedLoopRamp, 2=StepTest, 3=OpenLoop, 4=GoToPosition)"));
  Serial.println(F("#   T<steps> : Set Target Distance Steps & run M4 (e.g. T2000 or T-1500)"));
  Serial.println(F("#   Z        : Zero / Reset Virtual Encoder Distance Counter"));
  Serial.println(F("#   N<slots> : Set Commutator Slots/Rev (e.g. N6 for 3-pole, N10 for 5-pole HLA)"));
  Serial.println(F("#   E<adc>   : Set Spark Glitch Threshold ADC (e.g. E35.0)"));
  Serial.println(F("#   L<steps> : Set Gear Backlash Steps on Reversal (e.g. L12)"));
  Serial.println(F("#   P/I/F/C  : Set KP / KI / KV_FF / Coulomb_PWM"));
  Serial.println(F("#   S/G/Q/V  : Set Settle_us / Gap_us / PWM_Hz / Toggle CSV (V0/V1)"));
  Serial.print(F("# Status: Mode=")); Serial.print((uint8_t)op_mode);
  Serial.print(F(" DistSteps=")); Serial.print(enc.net_track_distance);
  Serial.print(F(" RawSparks=")); Serial.print(enc.raw_gap_sparks_total);
  Serial.print(F(" ShuntRipples=")); Serial.print(enc.raw_shunt_ripples_total);
  Serial.print(F(" SparkTh=")); Serial.print(cfg.spark_threshold_adc, 1);
  Serial.print(F(" Backlash=")); Serial.println(cfg.gear_backlash_steps);
}

void processCliCommand(const char* cmd) {
  if (cmd[0] == '\0') return;
  char c = toupper(cmd[0]);
  float val = atof(&cmd[1]);

  switch (c) {
    case 'M':
      if ((int)val == 0) startAutoTune();
      else {
        op_mode = (OperatingMode)constrain((int)val, 1, 4);
        active_ramp_ms = 0;
        pi_integral = 0.0f;
      }
      break;
    case 'T':
      cfg.target_pos_steps = (int32_t)val;
      op_mode              = MODE_POSITION_TARGET;
      pi_integral          = 0.0f;
      break;
    case 'Z': resetVirtualEncoder(); break;
    case 'N': cfg.comm_slots_per_rev = (uint8_t)constrain((int)val, 2, 32); break;
    case 'E': cfg.spark_threshold_adc= constrain(val, 5.0f, 500.0f); break;
    case 'L': cfg.gear_backlash_steps= constrain((int32_t)val, 0L, 500L); break;
    case 'P': cfg.kp                 = max(0.0f, val); break;
    case 'I': cfg.ki                 = max(0.0f, val); break;
    case 'F': cfg.kv_ff              = max(0.0f, val); break;
    case 'C': cfg.coulomb_ff_pwm     = constrain(val, 0.0f, 120.0f); break;
    case 'B': cfg.max_expected_bemf  = constrain(val, 200.0f, 4000.0f); break;
    case 'S': cfg.bemf_settle_us     = constrain((uint32_t)val, 100UL, cfg.measure_gap_us - 200UL); break;
    case 'G': cfg.measure_gap_us     = constrain((uint32_t)val, 500UL, 10000UL); break;
    case 'Q':
      cfg.pwm_freq_hz = constrain((uint32_t)val, 47UL, 100000UL);
      analogWriteFreq(cfg.pwm_freq_hz);
      applyMotorDrive(current_pwm, isActiveForward(), gap_state);
      break;
    case 'D': cfg.use_discrete_steps = ((int)val != 0); break;
    case 'V': cfg.verbose_csv        = ((int)val != 0); break;
    case '?':
    case 'H': printHelpAndStatus(); return;
    default:  break;
  }
  printHelpAndStatus();
}

void pollSerialCli() {
  while (Serial.available() > 0) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      if (cli_idx > 0) {
        cli_buf[cli_idx] = '\0';
        processCliCommand(cli_buf);
        cli_idx = 0;
      }
    } else if (cli_idx < sizeof(cli_buf) - 1) {
      cli_buf[cli_idx++] = ch;
    }
  }
}

// =========================================================================
// 8. SETUP & MAIN LOOP
// =========================================================================
void setup() {
  Serial.begin(921600);
  while (!Serial && millis() < 2000);

  pinMode(PIN_PWM_A,        OUTPUT);
  pinMode(PIN_PWM_B,        OUTPUT);
  pinMode(PIN_TRIG_F0F,     OUTPUT);
  pinMode(PIN_TRIG_F0B,     OUTPUT);
  pinMode(PIN_LED1,         OUTPUT);
  pinMode(PIN_LED2,         OUTPUT);
  pinMode(PIN_NEOPIXEL_PWR, OUTPUT);

  digitalWrite(PIN_NEOPIXEL_PWR, HIGH);
  pixels.begin();
  pixels.setBrightness(40);
  pixels.setPixelColor(0, pixels.Color(0, 0, 255));
  pixels.show();

  analogReadResolution(12);
  analogWriteRange(255);
  analogWriteFreq(cfg.pwm_freq_hz);

  applyMotorDrive(0, true, GAP_STATE_IDLE);

  printHelpAndStatus();
  startAutoTune();

  last_tick_ms = millis();
  last_log_us  = micros();
}

void loop() {
  pollSerialCli();

  uint32_t now_ms = millis();
  uint32_t now_us = micros();

  // Während der aktiven Fahrt kontinuierlich den Shunt auf Kommutator-Stromrippel abtasten
  pollActiveShuntRipple(now_us);

  // -------------------------------------------------------------------------
  // A. MESSLÜCKEN-, FILTER- & FUNKEN-ENCODER-STATE-MACHINE
  // -------------------------------------------------------------------------
  switch (gap_state) {
    case GAP_STATE_IDLE:
      if (ms_since_last_gap >= cfg.control_interval_ms) {
        active_shunt_adc          = analogRead(PIN_SHUNT);
        gap_state                 = GAP_STATE_SETTLING;
        gap_start_us              = now_us;
        gap_sample_count          = 0;
        first_half_accum          = 0;
        second_half_accum         = 0;
        first_half_count          = 0;
        second_half_count         = 0;
        enc.sparks_in_current_gap = 0;

        applyMotorDrive(0, isActiveForward(), gap_state);
        digitalWrite(PIN_LED2, HIGH);
      }
      break;

    case GAP_STATE_SETTLING:
      if (now_us - gap_start_us >= cfg.bemf_settle_us) {
        gap_state = GAP_STATE_MEASURING;
        applyMotorDrive(0, isActiveForward(), gap_state);
      }
      break;

    case GAP_STATE_MEASURING:
      {
        uint32_t elapsed_us = now_us - gap_start_us;
        if (elapsed_us < cfg.measure_gap_us) {
          int rawA = analogRead(PIN_BEMF_A);
          int rawB = analogRead(PIN_BEMF_B);
          bool fwd = isActiveForward();
          int diff = fwd ? (rawA - rawB) : (rawB - rawA);
          if (diff < 0) diff = 0;

          if (gap_sample_count < MAX_GAP_SAMPLES) {
            gap_samples[gap_sample_count++] = (uint16_t)diff;
          }

          // PFAD 2: Ungefiltertes Einzel-Sample auf Bürsten-Abrissfunken / Kommutierungslücke prüfen!
          detectGapCommutatorSpark(diff, now_us);

          uint32_t mid_window_us = cfg.bemf_settle_us + ((cfg.measure_gap_us - cfg.bemf_settle_us) / 2);
          if (elapsed_us < mid_window_us) {
            first_half_accum += diff;
            first_half_count++;
          } else {
            second_half_accum += diff;
            second_half_count++;
          }
        } else {
          // Ende der Messlücke -> 1. Trimmed-Mean bEMF für Regler, 2. Hybrid-Encoder Update
          float raw_filtered = computeTrimmedMeanAndSpread(gap_samples, gap_sample_count, gap_noise_spread);
          measured_bemf = (cfg.ema_alpha * raw_filtered) + ((1.0f - cfg.ema_alpha) * measured_bemf);

          if (first_half_count > 0 && second_half_count > 0) {
            float avg1 = (float)first_half_accum / (float)first_half_count;
            float avg2 = (float)second_half_accum / (float)second_half_count;
            settling_delta_bemf = avg1 - avg2;
          }

          uint32_t valid_window_us = cfg.measure_gap_us - cfg.bemf_settle_us;
          if (op_mode == MODE_AUTO_TUNE && tune_stage == TUNE_STEP_DYNAMICS) {
            tune_measure_window_us += valid_window_us;
          }

          float dt_sec = (float)(ms_since_last_gap + (cfg.measure_gap_us / 1000.0f)) * 0.001f;
          bool fwd     = isActiveForward();

          // Virtuellen Encoder aktualisieren (Fusion aus Funken-Zähler, Shunt-Rippel & bEMF)
          updateHybridVirtualEncoder(dt_sec, valid_window_us, fwd);
          enc.ripples_in_current_drive = 0;

          if (op_mode != MODE_AUTO_TUNE) {
            updateTargetSpeedLevel();
            executeClosedLoopControl(dt_sec);
          }

          gap_state         = GAP_STATE_IDLE;
          ms_since_last_gap = 0;
          digitalWrite(PIN_LED2, LOW);

          applyMotorDrive(current_pwm, isActiveForward(), gap_state);
          updateNeoPixel(isActiveForward(), current_pwm, is_saturated, op_mode);
        }
      }
      break;
  }

  // -------------------------------------------------------------------------
  // B. ZEITBASIS FÜR RAMPEN & AUTO-TUNER (Pausiert während der Messlücke)
  // -------------------------------------------------------------------------
  uint32_t dt_ms = now_ms - last_tick_ms;
  if (dt_ms >= 2) {
    last_tick_ms = now_ms;

    if (gap_state == GAP_STATE_IDLE) {
      ms_since_last_gap += dt_ms;

      if (op_mode == MODE_AUTO_TUNE) {
        runAutoTuneStep(dt_ms);
        applyMotorDrive(current_pwm, true, gap_state);
      } else if (op_mode == MODE_POSITION_TARGET) {
        updateTargetSpeedLevel();
      } else {
        active_ramp_ms += dt_ms;

        if (active_ramp_ms >= cfg.ramp_duration_ms) {
          active_ramp_ms    = 0;
          ms_since_last_gap = 0;

          switch (ramp_phase) {
            case RAMP_FORWARD_UP:
              ramp_phase = RAMP_FORWARD_DOWN;
              break;
            case RAMP_FORWARD_DOWN:
              ramp_phase    = RAMP_BACKWARD_UP;
              pi_integral   = 0.0f;
              measured_bemf = 0.0f;
              Serial.print(F("# [ODOMETRY] Peak Forward Distance reached: +"));
              Serial.print(enc.net_track_distance);
              Serial.println(F(" steps (Reversing now...)"));
              break;
            case RAMP_BACKWARD_UP:
              ramp_phase = RAMP_BACKWARD_DOWN;
              break;
            case RAMP_BACKWARD_DOWN:
              ramp_phase    = RAMP_FORWARD_UP;
              pi_integral   = 0.0f;
              measured_bemf = 0.0f;
              // Perfekter Odometrie-Selbsttest: Nach symmetrischer Vorwärts- und Rückwärtsrampe
              // sollte net_track_distance wieder nahe 0 liegen!
              Serial.print(F("# [ODOMETRY] Full Cycle Complete! Net Zero-Return Drift = "));
              Serial.print(enc.net_track_distance);
              Serial.print(F(" steps | Total Raw Sparks = "));
              Serial.println(enc.raw_gap_sparks_total);
              break;
          }
        }
        updateTargetSpeedLevel();
      }
      digitalWrite(PIN_LED1, current_pwm > 0 ? HIGH : LOW);
    }
  }

  // -------------------------------------------------------------------------
  // C. ADAPTIVES TELEMETRIE-LOGGING (Inkl. Encoder-Steps & RPM)
  // -------------------------------------------------------------------------
  if (!cfg.verbose_csv) return;

  uint32_t target_log_interval = (gap_state == GAP_STATE_IDLE) ? LOG_INTERVAL_NORM_US : LOG_INTERVAL_GAP_US;

  if (now_us - last_log_us >= target_log_interval) {
    if (now_us - last_log_us > target_log_interval * 2) {
      last_log_us = now_us;
    } else {
      last_log_us += target_log_interval;
    }

    bool fwd = isActiveForward();
    int signed_step   = fwd ? (int)current_step_level : -(int)current_step_level;
    int signed_target = fwd ? (int)target_bemf        : -(int)target_bemf;
    int signed_meas   = fwd ? (int)measured_bemf      : -(int)measured_bemf;
    int signed_pwm    = fwd ? current_pwm             : -current_pwm;
    int signed_rpm    = fwd ? (int)enc.estimated_rpm  : -(int)enc.estimated_rpm;

    char log_buf[128];
    int len = snprintf(log_buf, sizeof(log_buf), "%lu,%u,%d,%d,%d,%d,%u,%ld,%d,%d\n",
                       (unsigned long)now_us,
                       (uint8_t)op_mode,
                       signed_step,
                       signed_target,
                       signed_meas,
                       signed_pwm,
                       enc.sparks_in_current_gap,
                       (long)enc.net_track_distance,
                       signed_rpm,
                       active_shunt_adc);

    if (len > 0 && Serial.availableForWrite() >= len) {
      Serial.write(log_buf, len);
    }
  }
}
