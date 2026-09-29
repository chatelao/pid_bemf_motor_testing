# CONCEPT_SHUNT - Shunt Resistor Readout and Dynamic Motor Control

## Goal
Establish a standardized concept and tool sketch based on `examples/Minimal_PWM_BDR6133_Fixed_Oszi` for real-time monitoring of motor current via a shunt resistor (ADC input `A2`) and multi-meter / analog inputs (`A0`, `A1`) under configurable PWM drive conditions (defaulting to 100 Hz / 70% duty cycle), while supporting real-time serial control to stop or reconfigure motor drive parameters (`<freq>/<duty>`).

## Business Cases
- **Current & Load Characterization**: Allows precise monitoring of current spikes during commutation and varying motor loads across different operating frequencies.
- **Overcurrent & Stall Detection Diagnostic**: Provides the foundational signal acquisition pipeline for diagnostic monitoring and protecting power stage hardware (BDR-6133 driver) during motor stalls.
- **Commutator Ripple Analysis**: Provides clean time-series data from the shunt resistor to analyze high-frequency current fluctuations for sensorless position tracking.

## Use Cases
- **Default Baseline Monitoring**: Automatically initializes motor drive to 100 Hz frequency at 70% duty time and continuously streams shunt current (`A2`) and analog voltage (`A0`, `A1`) measurements over USB Serial.
- **Emergency / Quick Stop**: Immediately halts motor drive upon receiving the `'s'` command on the serial interface.
- **Dynamic Parameter Tuning**: Allows real-time dynamic adjustment of PWM frequency (Hz) and duty cycle (%) by issuing simple serial commands such as `123/45` (123 Hz frequency, 45% duty cycle).

## High-Level Architecture
```text
                 +--------------------+      +--------------------+         +---------------+
                 |        MCU         |      |     BDR-6133       |         |     Motor     |
                 | (RP2040 / STM32)   |      |    Motor Driver    |         | DC brushed    |
                 +--------------------+      +--------------------+         +---------------+
                 |                VCC |      |                    |         |               |
                 |                GND |      |                    |         |               |
                 |                    |      |                    |         |               |
                 |        (PWM B) D8  |----->| InB           OutB |=====+==>| B             |
                 |        (PWM A) D7  |----->| InA           OutA |==+==|==>| A             |
                 |                    |      +---------+----------+  |  |   +---------------+
                 |       (Shunt)  A2  |<.............../             |  |
                 |       (MM B)   A1  |<----------------------------/   |
                 |       (MM A)   A0  |<-------------------------------/
```

## Serial Control Interface Syntax
- **`s` or `S`**: Stops the motor (0% duty cycle).
- **`<freq>/<duty>`** (e.g. `123/45`): Sets the PWM drive frequency to `<freq>` Hz (e.g. 123 Hz) and duty cycle to `<duty>` % (e.g. 45%).

## Major Choices

### 1. Shunt Signal Acquisition Strategy
- **Alternative A: Non-blocking Continuous Stream with Buffer Checking (Selected)**: Periodically reads ADC pins `A2` (Shunt) and `A0`/`A1` (MM/Analog) in the main loop while verifying `Serial.availableForWrite()` space before emitting telemetry. Prevents serial buffer saturation from blocking motor control logic and allows continuous data visualization.
- **Alternative B: High-Speed Burst Sampling into RAM Buffer**: Collects fixed-size arrays of samples during a discrete window and dumps them sequentially to Serial. Suitable for high-speed oscilloscope-style captures, but halts continuous execution and command responsiveness during buffer transmission.
- **Alternative C: Polled On-Demand Sampling**: Takes ADC readings only when requested via serial commands. Minimizes serial traffic but provides poor time resolution for observing rapid current dynamics.

### 2. Interactive Serial Control Strategy
- **Alternative A: Non-Blocking Line Buffer Parser (Selected)**: Accumulates incoming serial characters into a buffer until a newline or carriage return is received, then parses commands (`s` or `<freq>/<duty>`). Ensures responsive command execution without delaying motor drive or telemetry acquisition.
- **Alternative B: Blocking `Serial.parseInt()` / `Serial.readString()`**: Simple to implement using built-in Arduino functions, but introduces severe execution timeouts when no input is pending, causing control loop stuttering.
- **Alternative C: Binary Protocol Frame Parser**: Uses fixed binary packet headers, commands, and checksums. Reduces bandwidth overhead but requires custom software on the host PC and lacks ease of manual CLI control via terminal.

### 3. Dynamic PWM Parameter Adjustment Strategy
- **Alternative A: Direct Hardware Core Frequency Reconfiguration (Selected)**: Uses platform hardware abstractions (`analogWriteFreq` / `analogWriteFrequency` and `analogWrite`) to dynamically update timer frequencies and duty registers at runtime. Provides instant transition without external hardware or software PWM overhead.
- **Alternative B: Fixed Hardware PWM Frequency with Software Bit-Banging**: Sets a high hardware timer rate and toggles pins manually in software for lower frequencies. Results in high CPU load and timing jitter.
- **Alternative C: Fixed Preset Step Table**: Pre-defines allowable frequency/duty combinations in an array and limits user selection to index lookups. Restricts flexibility for fine-grained motor parameter sweeps.

## Discarded Alternatives

### 1. Shunt Signal Acquisition Strategy
- **Alternative B (High-Speed Burst Sampling)** was discarded because it halts continuous serial telemetry and input handling during data transmission blocks.
- **Alternative C (Polled On-Demand Sampling)** was discarded because model train motor characterization requires continuous waveform monitoring rather than single-point samples.

### 2. Interactive Serial Control Strategy
- **Alternative B (Blocking `Serial.parseInt()`)** was discarded due to blocking timeouts that disrupt smooth motor operation and stream timing.
- **Alternative C (Binary Protocol Frame Parser)** was discarded because human-readable text commands (`s`, `123/45`) over standard serial terminals are significantly easier to debug and operate manually.

### 3. Dynamic PWM Parameter Adjustment Strategy
- **Alternative B (Fixed PWM with Software Bit-Banging)** was discarded due to unnecessary CPU overhead and potential signal jitter.
- **Alternative C (Fixed Preset Step Table)** was discarded because it prevents flexible input of arbitrary frequencies and duty cycles during testing.
