# 50Hz Single-Direction BEMF & Shunt ADC Test

This example sketch provides a simple single-direction 50 Hz PWM motor drive at full speed while continuously acquiring ADC readings from both Back-EMF (BEMF) sensing terminals and the current shunt resistor as fast as possible.

## Purpose

This simplified test sketch is designed to record raw BEMF and shunt current signals under low-frequency (50 Hz) single-direction PWM driving. By sampling continuously at maximum ADC speed without measurement gaps or frequency sweeps, it enables detailed inspection of high-speed current waveform behavior and terminal BEMF decay characteristics during standard 50 Hz operation.

## Features

- **50 Hz Fixed PWM Drive**: Operates the driver stage at a constant 50 Hz PWM frequency in forward direction at full speed (100% duty cycle).
- **Continuous Max-Speed ADC Sampling**: Directly reads 12-bit raw values from `PIN_BEMF_A` (A0), `PIN_BEMF_B` (A1), and `PIN_SHUNT` (A2) back-to-back in a tight loop.
- **High-Speed Telemetry Streaming**: Streams time-stamped CSV data over USB Serial at **921,600 baud** using non-blocking buffer checks (`Serial.availableForWrite() >= 32`).

## Pin Mapping

Standardized for Seeed Studio XIAO RP2040 interfacing with the BDR-6133 motor driver:

| Signal | Description | Seeed Studio XIAO RP2040 | Default Fallback |
| :--- | :--- | :--- | :--- |
| **PIN_PWM_A** | PWM Phase A Drive (Active 50Hz) | D7 / GPIO 7 | Pin 7 |
| **PIN_PWM_B** | PWM Phase B Drive (0) | D8 / GPIO 8 | Pin 8 |
| **PIN_BEMF_A** | BEMF Sense Terminal A | A0 / GPIO 26 | Pin A0 |
| **PIN_BEMF_B** | BEMF Sense Terminal B | A1 / GPIO 27 | Pin A1 |
| **PIN_SHUNT** | Current Sense Shunt | A2 / GPIO 28 | Pin A2 |
| **PIN_LED1** | Status LED 1 (Activity) | GPIO 15 (Red LED) | Pin 13 |
| **PIN_LED2** | Status LED 2 | GPIO 16 (Blue LED) | Pin 12 |

## Telemetry Format

Data is logged to the serial output as CSV at **921600 baud**:

```csv
<TIME_US>,<BEMF_A>,<BEMF_B>,<SHUNT>
```

- **TIME_US**: Microsecond timestamp (`micros()`).
- **BEMF_A**: Raw 12-bit ADC value (0 to 4095) read on Terminal A.
- **BEMF_B**: Raw 12-bit ADC value (0 to 4095) read on Terminal B.
- **SHUNT**: Raw 12-bit ADC value (0 to 4095) read on Current Sense Shunt (`PIN_SHUNT`).

## Compilation and Upload

```bash
# Navigate to the example directory
cd examples/BEMF_50Hz

# Build and upload for Seeed Studio XIAO RP2040
pio run -e seeed_xiao_rp2040 -t upload
```
