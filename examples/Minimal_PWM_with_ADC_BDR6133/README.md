# Minimal PWM with BEMF ADC Sampling Example

This example demonstrates simultaneous motor driving, synchronous BEMF measurement gap insertion, and dual-core multi-threading on the Seeed Studio XIAO RP2040.

## Purpose

This sketch tests the RP2040 dual-core architecture (`setup()` / `loop()` on Core 0 and `setup1()` / `loop1()` on Core 1) for motor control tasks:
- **Core 0** manages 400 Hz PWM motor driving, NeoPixel indicator lighting, and periodic 2 ms measurement gap insertion (using oscilloscope trigger pin `D2`).
- **Core 1** handles continuous 12-bit ADC sampling of BEMF channels `A0` and `A1`, streaming timestamped telemetry over USB Serial at **921,600 baud**.

## Key Features

- **Dual-Core Execution**:
  - **Core 0**: Executes the main PWM drive cycle and inserts 2 ms measurement gaps where motor drive is disabled (`PWM = 0`) and oscilloscope trigger pin `D2` goes `HIGH`.
  - **Core 1**: Independently measures ADC channels `A0` and `A1` at 12-bit resolution (`0–4095`) and outputs CSV data continuously over USB Serial at **921,600 baud**.
- **Synchronous BEMF Measurement Gaps**:
  - Toggles `D2` `HIGH` during 2 ms PWM-off windows to trigger external test equipment (oscilloscopes or logic analyzers).
- **Visual Duty Cycle Indicator**:
  - Updates onboard NeoPixel RGB LED color according to motor speed and direction.

## Hardware & Pin Mapping

| Signal | Seeed Studio XIAO RP2040 Pin | Function Description |
| :--- | :--- | :--- |
| **PIN_PWM_A** | `D7` / GPIO 1 | BDR-6133 Phase A Drive |
| **PIN_PWM_B** | `D8` / GPIO 2 | BDR-6133 Phase B Drive |
| **PIN_TRIG** | `D2` / GPIO 2 | Oscilloscope Measurement Gap Trigger (Active HIGH) |
| **PIN_BEMF_A** | `A0` / GPIO 26 | BEMF Sense Terminal A Input |
| **PIN_BEMF_B** | `A1` / GPIO 27 | BEMF Sense Terminal B Input |
| **PIN_NEO_PWR**| GPIO 11 | Onboard NeoPixel Power Enable |
| **PIN_NEO_DAT**| GPIO 12 | Onboard NeoPixel Data Line |

## Serial Telemetry Format

Core 1 continuously prints comma-separated values at **921,600 baud**:

```csv
<TIMESTAMP_MS>,<BEMF_A0_RAW>,<BEMF_A1_RAW>
```

- **TIMESTAMP_MS**: Millisecond timestamp (`millis()`).
- **BEMF_A0_RAW**: 12-bit ADC raw reading on terminal A0 (`0–4095`).
- **BEMF_A1_RAW**: 12-bit ADC raw reading on terminal A1 (`0–4095`).

## Compilation and Upload

```bash
# Using Arduino CLI for Seeed Studio XIAO RP2040
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_with_ADC_BDR6133.ino
```
