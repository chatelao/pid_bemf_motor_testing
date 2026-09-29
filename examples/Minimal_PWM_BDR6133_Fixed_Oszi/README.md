# Minimal PWM BDR6133 Fixed Oszi Example

This example provides a specialized test setup for measuring Back-EMF (BEMF) decays and motor terminal voltages using an external oscilloscope, with synchronized high-speed sampling and serial data logging on the Seeed Studio XIAO RP2040.

## Purpose

To accurately analyze BEMF terminal voltages without interference from active PWM driving, the motor driver must be periodically turned off (creating a "measurement gap"). This sketch generates precise measurement gaps during motor operation and outputs inverted active-low oscilloscope trigger signals on digital pins `D9` and `D10`. During each 2 ms measurement gap, the sketch performs high-speed ADC sampling (reading `A0`, `A1`, and `A2`) into RAM buffers and streams the captured data asynchronously over Serial during active PWM cycles.

## Key Features

- **Ultrasonic 20 kHz PWM Driving**: Configures `analogWriteFreq(20000)` and 8-bit duty cycle range (`0–255`) on `D7` and `D8`.
- **Oscilloscope Trigger Output**: Generates active-low hardware trigger signals on `D9` and `D10` that switch `HIGH` specifically during the 2 ms BEMF measurement gap.
- **High-Speed BEMF Sampling Window**: Disables motor drive and captures up to 1,000 raw ADC samples across channels `A0` (Terminal A), `A1` (Terminal B), and `A2` (Current Shunt) over a 2 ms window (~4 µs per sample).
- **Asynchronous Data Streaming**: Streams accumulated telemetry (microsecond timestamps, speed setpoint, BEMF_A, BEMF_B) over USB Serial at **115,200 baud** during the 20 ms active motor driving interval to avoid blocking the sampling window.

## Hardware & Pin Mapping

| Signal | Seeed Studio XIAO RP2040 Pin | Function Description |
| :--- | :--- | :--- |
| **PIN_PWM_A** | `D7` / GPIO 1 | BDR-6133 Phase A PWM Drive |
| **PIN_PWM_B** | `D8` / GPIO 2 | BDR-6133 Phase B PWM Drive |
| **PIN_BEMF_A** | `A0` / GPIO 26 | BEMF Sense Terminal A |
| **PIN_BEMF_B** | `A1` / GPIO 27 | BEMF Sense Terminal B |
| **PIN_SHUNT** | `A2` / GPIO 28 | Current Sense Shunt |
| **PIN_TRIG_1** | `D9` / GPIO 9 | Oscilloscope Trigger Line 1 (Inverted / Active LOW during PWM, HIGH during measurement gap) |
| **PIN_TRIG_2** | `D10` / GPIO 10 | Oscilloscope Trigger Line 2 (Inverted / Active LOW during PWM, HIGH during measurement gap) |
| **PIN_NEO_PWR**| GPIO 11 | Onboard NeoPixel Power Enable |
| **PIN_NEO_DAT**| GPIO 12 | Onboard NeoPixel Data Line |

## Serial Output Format

During active motor driving cycles, buffered BEMF measurements are outputted as CSV:

```csv
Time_us,Speed_Step,bEMF_A0,bEMF_A1
```

- **Time_us**: Microsecond timestamp (`micros()`) recorded during the 2 ms measurement window.
- **Speed_Step**: Direction and speed step index (positive for forward, negative for reverse).
- **bEMF_A0**: Raw 12-bit ADC reading on channel `A0`.
- **bEMF_A1**: Raw 12-bit ADC reading on channel `A1`.

## Compilation and Upload

```bash
# Using Arduino CLI for Seeed Studio XIAO RP2040
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_BDR6133_Fixed_Oszi.ino
```
