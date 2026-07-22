#include <Arduino.h>
#include <SerialCommands.h>

// Minimal custom PID Class matching standard library behavior/scaling
class PID {
public:
  #define AUTOMATIC 1
  #define MANUAL 0
  #define DIRECT 0
  #define REVERSE 1

  PID(double* Input, double* Output, double* Setpoint,
      double Kp, double Ki, double Kd, int ControllerDirection) {
    myInput = Input;
    myOutput = Output;
    mySetpoint = Setpoint;
    inAuto = false;

    SampleTime = 100; // default to 100ms
    outMin     =   0;
    outMax     = 255;

    controllerDirection = ControllerDirection;
    SetTunings(Kp, Ki, Kd);

    lastTime = millis() - SampleTime;
  }

  void SetMode(int Mode) {
    bool newAuto = (Mode == AUTOMATIC);
    if (newAuto && !inAuto) {
      // Initialize to ensure a bumpless transfer
      outputSum = *myOutput;
      lastInput = *myInput;
      if (outputSum > outMax) outputSum = outMax;
      else if (outputSum < outMin) outputSum = outMin;
    }
    inAuto = newAuto;
  }

  void SetOutputLimits(double Min, double Max) {
    if (Min >= Max) return;
    outMin = Min;
    outMax = Max;
    if (inAuto) {
      if (*myOutput > outMax) *myOutput = outMax;
      else if (*myOutput < outMin) *myOutput = outMin;
      if (outputSum > outMax) outputSum = outMax;
      else if (outputSum < outMin) outputSum = outMin;
    }
  }

  void SetSampleTime(int NewSampleTime) {
    if (NewSampleTime > 0) {
      double ratio = (double)NewSampleTime / (double)SampleTime;
      ki *= ratio;
      kd /= ratio;
      SampleTime = (unsigned long)NewSampleTime;
    }
  }

  void SetTunings(double Kp, double Ki, double Kd) {
    if (Kp < 0 || Ki < 0 || Kd < 0) return;
    dispKp = Kp; dispKi = Ki; dispKd = Kd;
    double SampleTimeInSec = ((double)SampleTime) / 1000.0;
    kp = Kp;
    ki = Ki * SampleTimeInSec;
    kd = Kd / SampleTimeInSec;
    if (controllerDirection == REVERSE) {
      kp = 0.0 - kp;
      ki = 0.0 - ki;
      kd = 0.0 - kd;
    }
  }

  bool Compute() {
    if (!inAuto) return false;
    unsigned long now = millis();
    unsigned long timeChange = (now - lastTime);
    if (timeChange >= SampleTime) {
      double input = *myInput;
      double error = *mySetpoint - input;
      double dInput = (input - lastInput);
      outputSum += (ki * error);

      if (outputSum > outMax) outputSum = outMax;
      else if (outputSum < outMin) outputSum = outMin;

      double output = kp * error + outputSum - kd * dInput;
      if (output > outMax) output = outMax;
      else if (output < outMin) output = outMin;

      *myOutput = output;
      lastInput = input;
      lastTime = now;
      return true;
    }
    return false;
  }

  double GetKp() { return dispKp; }
  double GetKi() { return dispKi; }
  double GetKd() { return dispKd; }
  int GetMode() { return inAuto ? AUTOMATIC : MANUAL; }
  int GetDirection() { return controllerDirection; }

private:
  double kp, ki, kd;
  double dispKp, dispKi, dispKd;
  int controllerDirection;
  double* myInput;
  double* myOutput;
  double* mySetpoint;
  unsigned long lastTime;
  double outputSum, lastInput;
  unsigned long SampleTime;
  double outMin, outMax;
  bool inAuto;
};

/**
 * Märklin Motor Test & Calibration Tool
 *
 * Phase 4: BEMF Sensing & PID Implementation with CLI Support
 * - Implement synchronous ADC polling on A0/A1
 * - Integrate Arduino PID Library
 * - Implement closed-loop speed control
 * - Serial CLI interface using SerialCommands for manual/testing modes
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

// Direction Mode
enum DirectionMode {
  DIR_FWD_ONLY,
  DIR_REV_ONLY,
  DIR_BOTH
};
DirectionMode current_dir_mode = DIR_BOTH;

// Control Mode
enum ControlMode {
  CTRL_PROFILE,
  CTRL_CONST_SPEED,
  CTRL_CONST_PWM
};
ControlMode current_ctrl_mode = CTRL_PROFILE;

// Constant speed / PWM settings
double const_speed_target = 1000.0;
int const_pwm_target = 128;
uint32_t last_dir_toggle_time = 0;
const uint32_t DIR_TOGGLE_INTERVAL_MS = 4000;

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

// Stream Wrapper to remove carriage return '\r' and make CLI highly robust across platforms
class SanitizedSerialStream : public Stream {
private:
  Stream* source_;
public:
  SanitizedSerialStream(Stream* source) : source_(source) {}

  int available() override {
    while (source_->available() > 0) {
      int ch = source_->peek();
      if (ch == '\r') {
        source_->read(); // Discard '\r'
      } else {
        break;
      }
    }
    return source_->available();
  }

  int read() override {
    while (true) {
      int ch = source_->read();
      if (ch == -1) return -1;
      if (ch != '\r') {
        return ch;
      }
    }
  }

  int peek() override {
    while (true) {
      int ch = source_->peek();
      if (ch == -1) return -1;
      if (ch == '\r') {
        source_->read(); // Discard '\r'
      } else {
        return ch;
      }
    }
  }

  void flush() override {
    source_->flush();
  }

  size_t write(uint8_t val) override {
    return source_->write(val);
  }

  size_t write(const uint8_t *buf, size_t size) override {
    return source_->write(buf, size);
  }
};

SanitizedSerialStream sanitized_serial(&Serial);
char serial_command_buffer[32];
SerialCommands serial_commands(&sanitized_serial, serial_command_buffer, sizeof(serial_command_buffer), "\n", " ");

// CLI Command Handlers
void cmd_fwd_only(SerialCommands* sender) {
  current_dir_mode = DIR_FWD_ONLY;
  forward = true;
  sender->GetSerial()->println(">>> Direction Mode: FORWARD ONLY");
}

void cmd_rev_only(SerialCommands* sender) {
  current_dir_mode = DIR_REV_ONLY;
  forward = false;
  sender->GetSerial()->println(">>> Direction Mode: BACKWARD ONLY");
}

void cmd_both_dir(SerialCommands* sender) {
  current_dir_mode = DIR_BOTH;
  last_dir_toggle_time = millis();
  sender->GetSerial()->println(">>> Direction Mode: BIDIRECTIONAL (BOTH)");
}

void cmd_const_speed(SerialCommands* sender) {
  char* arg = sender->Next();
  if (arg == NULL) {
    sender->GetSerial()->println(">>> Error: Speed target argument missing (0-4095)");
    return;
  }
  double target = atof(arg);
  if (target < 0.0 || target > 4095.0) {
    sender->GetSerial()->println(">>> Error: Speed must be between 0 and 4095");
    return;
  }
  const_speed_target = target;
  current_ctrl_mode = CTRL_CONST_SPEED;
  last_dir_toggle_time = millis();

  // Reset PID state to avoid sudden jumps
  pid_output = 0;
  myPID.SetMode(MANUAL);
  pid_output = 0;
  myPID.SetMode(AUTOMATIC);

  sender->GetSerial()->print(">>> Control Mode: CONSTANT SPEED, Target = ");
  sender->GetSerial()->println(const_speed_target);
}

void cmd_const_pwm(SerialCommands* sender) {
  char* arg = sender->Next();
  if (arg == NULL) {
    sender->GetSerial()->println(">>> Error: PWM target argument missing (0-255)");
    return;
  }
  int target = atoi(arg);
  if (target < 0 || target > 255) {
    sender->GetSerial()->println(">>> Error: PWM must be between 0 and 255");
    return;
  }
  const_pwm_target = target;
  current_ctrl_mode = CTRL_CONST_PWM;
  last_dir_toggle_time = millis();

  sender->GetSerial()->print(">>> Control Mode: CONSTANT PWM, Target = ");
  sender->GetSerial()->println(const_pwm_target);
}

void cmd_profile(SerialCommands* sender) {
  current_ctrl_mode = CTRL_PROFILE;
  current_step_index = 0;
  step_start_time = millis();
  pid_setpoint = profile[current_step_index].target_bemf;
  if (current_dir_mode == DIR_FWD_ONLY) {
    forward = true;
  } else if (current_dir_mode == DIR_REV_ONLY) {
    forward = false;
  } else {
    forward = profile[current_step_index].direction_forward;
  }
  sender->GetSerial()->println(">>> Control Mode: MULTI-STEP PROFILE RESET & RUN");
}

void cmd_help(SerialCommands* sender) {
  Stream* s = sender->GetSerial();
  s->println("--- Märklin Motor Control CLI Help ---");
  s->println("Commands:");
  s->println("  f          : Set Direction to Forward Only");
  s->println("  b          : Set Direction to Backward Only");
  s->println("  a          : Set Direction to Forward & Backward (Both)");
  s->println("  s <speed>  : Set Closed-Loop Constant Speed (BEMF Target 0-4095)");
  s->println("  p <pwm>    : Set Open-Loop Constant PWM (Target 0-255)");
  s->println("  prof       : Set Mode to Pre-programmed Multi-step Profile");
  s->println("  h or help  : Show this help menu and current system status");
  s->println("Current Status:");
  s->print("  Direction Mode: ");
  switch (current_dir_mode) {
    case DIR_FWD_ONLY:
      s->println("FORWARD ONLY");
      break;
    case DIR_REV_ONLY:
      s->println("BACKWARD ONLY");
      break;
    case DIR_BOTH:
    default:
      s->println("BIDIRECTIONAL (BOTH)");
      break;
  }
  s->print("  Control Mode  : ");
  switch (current_ctrl_mode) {
    case CTRL_PROFILE:
      s->println("PROFILE");
      break;
    case CTRL_CONST_SPEED:
      s->println("CONSTANT SPEED");
      break;
    case CTRL_CONST_PWM:
    default:
      s->println("CONSTANT PWM");
      break;
  }
  s->print("  Target Speed  : "); s->println(const_speed_target);
  s->print("  Target PWM    : "); s->println(const_pwm_target);
  s->print("  Active Dir    : "); s->println(forward ? "FWD" : "REV");
}

void cmd_unrecognized(SerialCommands* sender, const char* cmd) {
  sender->GetSerial()->print(">>> Error: Unrecognized command '");
  sender->GetSerial()->print(cmd);
  sender->GetSerial()->println("'. Type 'h' or 'help' for assistance.");
}

// Commands registration
SerialCommand cmd_f_("f", cmd_fwd_only);
SerialCommand cmd_b_("b", cmd_rev_only);
SerialCommand cmd_a_("a", cmd_both_dir);
SerialCommand cmd_s_("s", cmd_const_speed);
SerialCommand cmd_p_("p", cmd_const_pwm);
SerialCommand cmd_prof_("prof", cmd_profile);
SerialCommand cmd_h_("h", cmd_help);
SerialCommand cmd_help_("help", cmd_help);

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000);

  // Set ADC to 12-bit resolution as per DESIGN.md
  analogReadResolution(12);
  pinMode(PIN_BEMF_A, INPUT);
  pinMode(PIN_BEMF_B, INPUT);

  pinMode(PIN_SHUNT, INPUT);

  pinMode(PIN_LED1, OUTPUT);
  pinMode(PIN_LED2, OUTPUT);

  // Set PWM frequency to 20kHz (ultrasonic) as per DESIGN.md
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  analogWriteFreq(20000);
#elif defined(ARDUINO_ARCH_STM32)
  analogWriteFrequency(20000);
#endif
  analogWriteRange( 255 );

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);

  // Initialize PID
  myPID.SetMode(AUTOMATIC);
  myPID.SetOutputLimits(0, 255);
  myPID.SetSampleTime(CONTROL_INTERVAL_MS);

  last_control_time    = millis();
  step_start_time      = millis();
  last_dir_toggle_time = millis();

  // Set initial setpoint and direction from the profile
  pid_setpoint = profile[current_step_index].target_bemf;
  forward = profile[current_step_index].direction_forward;

  // Add CLI commands
  serial_commands.AddCommand(&cmd_f_);
  serial_commands.AddCommand(&cmd_b_);
  serial_commands.AddCommand(&cmd_a_);
  serial_commands.AddCommand(&cmd_s_);
  serial_commands.AddCommand(&cmd_p_);
  serial_commands.AddCommand(&cmd_prof_);
  serial_commands.AddCommand(&cmd_h_);
  serial_commands.AddCommand(&cmd_help_);
  serial_commands.SetDefaultHandler(cmd_unrecognized);

  Serial.println("--- Märklin Motor Control Tool (Phase 4) ---");
  Serial.println("Closed-loop PID Speed Control with Synchronous BEMF Feedback and CLI");
  Serial.println("Type 'h' or 'help' for command options.");
  Serial.println("Format: SP:<setpoint>, BEMF:<measured>, PWM:<output>, DIR:<dir>, MODE:<mode>");
}

void loop() {
  uint32_t now = millis();

  // 0. Handle CLI Commands
  serial_commands.ReadSerial();

  // 1. Update Profile Setpoint and Direction over time
  if (current_ctrl_mode == CTRL_PROFILE) {
    if (now - step_start_time >= profile[current_step_index].duration_ms) {
      step_start_time += profile[current_step_index].duration_ms; // Prevent drift in step timing
      current_step_index = (current_step_index + 1) % PROFILE_STEPS_COUNT;

      pid_setpoint = profile[current_step_index].target_bemf;
      if (current_dir_mode == DIR_FWD_ONLY) {
        forward = true;
      } else if (current_dir_mode == DIR_REV_ONLY) {
        forward = false;
      } else {
        forward = profile[current_step_index].direction_forward;
      }

      Serial.print(">>> Step ");
      Serial.print(current_step_index);
      Serial.print(": Target BEMF = ");
      Serial.print(pid_setpoint);
      Serial.print(", Dir = ");
      Serial.println(forward ? "FWD" : "REV");
    }
  } else {
    // For non-profile modes, check direction overrides
    if (current_dir_mode == DIR_FWD_ONLY) {
      forward = true;
    } else if (current_dir_mode == DIR_REV_ONLY) {
      forward = false;
    } else if (current_dir_mode == DIR_BOTH) {
      if (now - last_dir_toggle_time >= DIR_TOGGLE_INTERVAL_MS) {
        last_dir_toggle_time += DIR_TOGGLE_INTERVAL_MS;
        forward = !forward;
        Serial.print(">>> Bidirectional Toggle: Dir = ");
        Serial.println(forward ? "FWD" : "REV");
      }
    }

    if (current_ctrl_mode == CTRL_CONST_SPEED) {
      pid_setpoint = const_speed_target;
    } else {
      pid_setpoint = 0.0;
    }
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

    int current_pwm = 0;
    if (current_ctrl_mode == CTRL_CONST_PWM) {
      current_pwm = const_pwm_target;
      pid_output  = current_pwm;
    } else {
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
      current_pwm = (int)pid_output;
    }

    // Apply computed PID PWM Output
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
      if (current_ctrl_mode == CTRL_CONST_PWM) {
        Serial.print("N/A");
      } else {
        Serial.print((int)pid_setpoint);
      }
      Serial.print(", BEMF:");
      Serial.print(bemf_raw);
      Serial.print(", PWM:");
      Serial.print(current_pwm);
      Serial.print(", DIR:");
      Serial.print(forward ? "FWD" : "REV");
      Serial.print(", MODE:");
      if (current_ctrl_mode == CTRL_PROFILE) {
        Serial.println("PROFILE");
      } else if (current_ctrl_mode == CTRL_CONST_SPEED) {
        Serial.println("SPEED");
      } else {
        Serial.println("PWM");
      }
    }
  }
}
