# DESIGN_SHUNT - Technical Design for Shunt Resistor Readout and Dynamic Motor Control

## Introduction
This document details the technical implementation of the Shunt Resistor Readout and Dynamic Motor Control tool, derived from [CONCEPT_SHUNT.md](CONCEPT_SHUNT.md) and implemented in `examples/CONCEPT_SHUNT/CONCEPT_SHUNT.ino`.

The tool enables real-time monitoring of motor current across a low-side shunt resistor (ADC channel `A2`) and motor terminal / multimeter feedback signals (`A0`, `A1`) under user-configurable PWM drive conditions (defaulting to 100 Hz frequency at 70% duty cycle). It features interactive non-blocking serial command control for quick stopping (`s`) or real-time drive reconfiguration (`<freq>/<duty>`), streaming CSV telemetry over USB Serial.

## Detailed Architecture & System Overview

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

### Hardware Pin Mapping Across Platforms
To ensure cross-platform compatibility across Seeed Studio XIAO RP2040 and ST Nucleo evaluation boards, hardware signals are mapped to standard Arduino pin macros:

| Function | XIAO RP2040 | Nucleo F446RE | Nucleo G431RB | Description |
|---|---|---|---|---|
| **PWM A** | D7 | D7 (PA8) | D7 (PA8) | Motor Output Terminal A Drive |
| **PWM B** | D8 | D8 (PA9) | D8 (PA9) | Motor Output Terminal B Drive |
| **ADC MM A / BEMF A** | A0 | A0 (PA0) | A0 (PA0) | Analog Feedback Terminal A |
| **ADC MM B / BEMF B** | A1 | A1 (PA1) | A1 (PA1) | Analog Feedback Terminal B |
| **ADC Shunt** | A2 | A2 (PA4) | A2 (PA4) | Current Measurement across Shunt |
| **LED 1** | D15 | D13 (PA5) | D13 (PA5) | Drive Status LED (Active when PWM > 0) |
| **LED 2** | D16 | D12 (PA6) | D12 (PA6) | Secondary Status LED |
| **RGB NeoPixel** | GPIO 12 (PWR D11) | N/A | N/A | Visual Status Indicator (Green=Run, Red=Stop) |

## Software Specifications & Interfaces

### 1. Serial Control Interface
- **Baud Rate**: 115200 baud over USB Serial.
- **Command Syntax**:
  - **`s` / `S`**: Stops the motor immediately (0% duty cycle, preserves last set frequency).
  - **`<freq>/<duty>`** (e.g. `123/45`): Configures PWM drive frequency to `<freq>` Hz and duty cycle to `<duty>` % (valid duty range: 0–100%).

### 2. Telemetry Output Format
- **Streaming Interval**: Non-blocking ~1 kHz sampling rate (1000 µs interval).
- **Format**: CSV text format with field headers:
  `Time_us,Freq_Hz,Duty_pct,Shunt_A2,MM_A0,MM_A1`
- **Output Sample**:
  ```csv
  Time_us,Freq_Hz,Duty_pct,Shunt_A2,MM_A0,MM_A1
  1000234,100,70,1842,2048,120
  1001234,100,70,1850,2045,118
  ```

### 3. Execution Control Loop
The main loop executes non-blocking command processing and telemetry sampling:
1. `handleSerialInput()` reads characters into an internal 32-byte line buffer without blocking execution.
2. Upon receiving `\n` or `\r`, `processCommand()` parses the line and invokes `setMotorDrive()`.
3. A timing loop checks `micros()` every 1000 µs and verifies `Serial.availableForWrite() >= 32` before reading ADC channels (`PIN_SHUNT`, `PIN_BEMF_A`, `PIN_BEMF_B`) and emitting CSV data.

## Major Choices

### 1. Shunt Signal Acquisition Strategy
- **Alternative A: Non-blocking Continuous Stream with Buffer Checking (Selected)**:
  - *Implementation*: Periodically checks `micros()` for a 1000 µs interval and evaluates `Serial.availableForWrite() >= 32` prior to writing telemetry bytes.
  - *Justification*: Guarantees that time-critical motor drive timers and serial input parsing are never stalled by blocking serial write operations, enabling smooth dynamic control and continuous data acquisition.
- **Alternative B: High-Speed Burst Sampling into RAM Buffer**:
  - *Concept*: Collects fixed-length arrays of ADC samples in memory during a sampling window and dumps them sequentially to Serial.
- **Alternative C: Polled On-Demand Sampling**:
  - *Concept*: Takes ADC readings only when explicitly requested by incoming serial query commands.

### 2. Interactive Serial Control Strategy
- **Alternative A: Non-Blocking Line Buffer Parser (Selected)**:
  - *Implementation*: Accumulates individual incoming serial bytes into a lightweight 32-byte `inputBuffer` String variable, triggering parsing only when newline or carriage return delimiters are detected.
  - *Justification*: Eliminates blocking execution timeouts, allowing commands to be parsed instantly in the main loop without causing stutters in motor drive or gaps in telemetry timestamps.
- **Alternative B: Blocking `Serial.parseInt()` / `Serial.readString()`**:
  - *Concept*: Uses standard Arduino blocking helper functions to parse integers and strings directly from the serial stream.
- **Alternative C: Binary Protocol Frame Parser**:
  - *Concept*: Uses fixed binary packet headers, length fields, command bytes, and CRC checksums.

### 3. Dynamic PWM Parameter Adjustment Strategy
- **Alternative A: Direct Hardware Core Frequency Reconfiguration (Selected)**:
  - *Implementation*: Uses platform core abstraction functions (`analogWriteFreq` / `analogWriteFrequency` on RP2040 and STM32) and updates hardware PWM registers on-the-fly when new frequency and duty parameters are parsed.
  - *Justification*: Provides instant parameter updates directly in hardware timers with zero software CPU overhead during PWM period generation.
- **Alternative B: Fixed Hardware PWM Frequency with Software Bit-Banging**:
  - *Concept*: Configures a fixed high-rate hardware timer interrupt and manually toggles GPIO pins in software to synthesize variable lower frequencies.
- **Alternative C: Fixed Preset Step Table**:
  - *Concept*: Restricts user choices to a predefined lookup table of discrete frequency and duty cycle steps.

## Discarded Technical Alternatives

### 1. Shunt Signal Acquisition Strategy
- **Alternative B (High-Speed Burst Sampling)** was discarded because dumping large memory arrays halts real-time serial parsing and motor drive adjustments during transmission blocks.
- **Alternative C (Polled On-Demand Sampling)** was discarded because characterization of motor current spikes and commutation ripples requires continuous time-series data rather than isolated single-point queries.

### 2. Interactive Serial Control Strategy
- **Alternative B (Blocking `Serial.parseInt()`)** was discarded due to default execution timeouts (1000 ms) that stall the main loop whenever incomplete string data is present on the serial port.
- **Alternative C (Binary Protocol Frame Parser)** was discarded because human-readable ASCII commands (`s`, `123/45`) allow seamless testing directly from standard serial terminal tools without specialized host software.

### 3. Dynamic PWM Parameter Adjustment Strategy
- **Alternative B (Fixed PWM with Software Bit-Banging)** was discarded due to significant CPU overhead and high timing jitter caused by software interrupts during high-speed sampling.
- **Alternative C (Fixed Preset Step Table)** was discarded because it prevents arbitrary parameter tuning required when identifying optimal PWM driving conditions across different motor models.
