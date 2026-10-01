/*
 * CONCEPT_SHUNT - Shunt Resistor Readout and Dynamic Motor Control
 *
 * Based on examples/Minimal_PWM_BDR6133_Fixed_Oszi
 *
 * Requirements:
 * - Default motor configuration: 100 Hz frequency, 70% duty cycle.
 * - Continuously read values on Shunt (A2) and MM / Analog inputs (A0, A1).
 * - Output telemetry CSV stream to Serial port.
 * - If 's' or 'S' is entered, stop the motor.
 * - If '<freq>/<duty>' is entered (e.g. '123/45'), run motor at <freq> Hz and <duty>% duty cycle.
 */

#include <Arduino.h>

#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #include <Adafruit_NeoPixel.h>
  Adafruit_NeoPixel pixels(1, 12, NEO_GRB + NEO_KHZ400);
#endif

// Pin Definitions based on DESIGN.md
#if defined(ARDUINO_SEEED_XIAO_RP2040)
  #define PIN_PWM_A   7
  #define PIN_PWM_B   8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   15
  #define PIN_LED2   16
#else
  #define PIN_PWM_A   7
  #define PIN_PWM_B   8
  #define PIN_BEMF_A A0
  #define PIN_BEMF_B A1
  #define PIN_SHUNT  A2
  #define PIN_LED1   13
  #define PIN_LED2   12
#endif

// State variables
int current_freq = 100; // Default 100 Hz
int current_duty = 70;  // Default 70%

String inputBuffer = "";

void setMotorDrive(int freq_hz, int duty_pct) {
  current_freq = freq_hz;
  current_duty = duty_pct;

  if (freq_hz > 0) {
#if defined(ARDUINO_SEEED_XIAO_RP2040)
    analogWriteFreq(freq_hz);
    analogWriteRange(255);
#endif
  }

  int pwm_val = (current_duty * 255) / 100;
  if (current_duty <= 0) {
    pwm_val = 0;
  } else if (pwm_val > 255) {
    pwm_val = 255;
  }

  // Drive motor forward on PIN_PWM_A, PIN_PWM_B set to 0
  analogWrite(PIN_PWM_A, pwm_val);
  analogWrite(PIN_PWM_B, 0);

  // Status LED update
  digitalWrite(PIN_LED1, pwm_val > 0 ? HIGH : LOW);

#if defined(ARDUINO_SEEED_XIAO_RP2040)
  if (pwm_val > 0) {
    pixels.setPixelColor(0, pixels.Color(0, 255, 0)); // Green when running
  } else {
    pixels.setPixelColor(0, pixels.Color(255, 0, 0)); // Red when stopped
  }
  pixels.show();
#endif
}

void processCommand(String cmd) {
  cmd.trim();
  if (cmd.length() == 0) return;

  if (cmd.equalsIgnoreCase("s")) {
    setMotorDrive(current_freq, 0);
    Serial.println("# Motor STOPPED");
  } else {
    int slashIndex = cmd.indexOf('/');
    if (slashIndex > 0) {
      String freqStr = cmd.substring(0, slashIndex);
      String dutyStr = cmd.substring(slashIndex + 1);

      int freq = freqStr.toInt();
      int duty = dutyStr.toInt();

      if (freq > 0 && duty >= 0 && duty <= 100) {
        setMotorDrive(freq, duty);
        Serial.print("# Motor set to ");
        Serial.print(freq);
        Serial.print(" Hz, ");
        Serial.print(duty);
        Serial.println("% duty cycle");
      } else {
        Serial.println("# ERROR: Invalid parameters. Usage: <freq>/<duty> (e.g. 123/45) or 's'");
      }
    } else {
      Serial.println("# ERROR: Unknown command. Usage: <freq>/<duty> (e.g. 123/45) or 's'");
    }
  }
}

void handleSerialInput() {
  while (Serial.available() > 0) {
    char c = (char)Serial.read();
    if (c == '\n' || c == '\r') {
      if (inputBuffer.length() > 0) {
        processCommand(inputBuffer);
        inputBuffer = "";
      }
    } else {
      if (inputBuffer.length() < 32) {
        inputBuffer += c;
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 2000); // Wait for Serial connection on USB boards

  pinMode(PIN_PWM_A, OUTPUT);
  pinMode(PIN_PWM_B, OUTPUT);
  pinMode(PIN_LED1,  OUTPUT);
  pinMode(PIN_LED2,  OUTPUT);

  analogReadResolution(12);

#if defined(ARDUINO_SEEED_XIAO_RP2040)
  pinMode(11, OUTPUT);
  digitalWrite(11, HIGH); // Power NeoPixel
  pixels.begin();
  pixels.setBrightness(50);
#endif

  // Set motor to initial state: 100 Hz / 70% duty cycle
  setMotorDrive(100, 70);

  Serial.println("--- CONCEPT_SHUNT Telemetry & Control ---");
  Serial.println("# Commands: 's' to stop, '<freq>/<duty>' (e.g. 123/45) to configure PWM");
  Serial.println("Time_us,Freq_Hz,Duty_pct,Shunt_A2,MM_A0,MM_A1");
}

void loop() {
  // Check for interactive commands from Serial
  handleSerialInput();

  // Non-blocking telemetry output at ~1 kHz (every 1000 us)
  static uint32_t last_log_us = micros();
  uint32_t now_us = micros();

  if (now_us - last_log_us >= 1000) {
    last_log_us += 1000;

    if (Serial.availableForWrite() >= 32) {
      int shunt_val = analogRead(PIN_SHUNT);
      int mm_a0     = analogRead(PIN_BEMF_A);
      int mm_a1     = analogRead(PIN_BEMF_B);

      Serial.print(now_us);       Serial.print(',');
      Serial.print(current_freq); Serial.print(',');
      Serial.print(current_duty); Serial.print(',');
      Serial.print(shunt_val);   Serial.print(',');
      Serial.print(mm_a0);       Serial.print(',');
      Serial.println(mm_a1);
    }
  }
}
