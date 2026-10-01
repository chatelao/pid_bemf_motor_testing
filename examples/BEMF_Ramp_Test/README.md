# BEMF Ramp Characterization Test

This example sketch is designed to characterize and test a refitted Märklin motor using an open-loop PWM sweep across an E3-like range of frequencies (47 Hz to 100 kHz). It is optimized to run on the Seeed Studio XIAO RP2040 development board using the BDR-6133 motor driver stage.

## Purpose

When refitting Märklin AC/DC motors with permanent magnets, the Back-EMF (BEMF) response profile must be characterized across different PWM carrier frequencies to determine motor constants, identify frequency-dependent motor behavior, and choose appropriate closed-loop speed control (PID) parameters. This sketch automates that characterization process by sweeping the motor driver's PWM duty cycle up and down bidirectionally across 11 logarithmically spaced frequency steps in the E3 series.

## Features

- **E3 Frequency Range Sweep**: Sequentially steps through an E3 preferred numbers series of 11 frequencies: **47 Hz, 100 Hz, 220 Hz, 470 Hz, 1.0 kHz, 2.2 kHz, 4.7 kHz, 10 kHz, 22 kHz, 47 kHz, and 100 kHz**. After completing a bidirectional ramp cycle at one frequency, it advances to the next frequency step in the series.
- **Open-Loop Bidirectional PWM Ramp**: For each frequency step, it sweeps the PWM from 0 to 255 (100% duty cycle) over 1 second, then back down to 0 over 1 second in the forward direction, followed by the same 1s ramp up and 1s ramp down in the backward direction.
- **High-Frequency Telemetry Logging**: Emits real-time BEMF, PWM, and frequency telemetry at **40 kHz** (every 25 microseconds) over USB Serial at **921,600 baud**. The serial writing uses a best-effort, non-blocking check (`Serial.availableForWrite() >= 32`) to prevent high-frequency printing from choking the microprocessor's control loop.
- **Synchronous Measurement Gaps**: To reliably measure BEMF without the influence of active PWM driving (especially critical at high duty cycles), the sketch inserts a **25 ms "measurement gap" (PWM = 0) every 250 ms**. During this gap, the BEMF terminal voltage decays and stabilizes to represent true coasting speed.
- **Platform Pin Configuration**: Preconfigured for Seeed Studio XIAO RP2040 with standard pin fallbacks.

## Pin Mapping

The pinouts are standardized to interface with the BDR-6133 driver stage:

| Signal | Description | Seeed Studio XIAO RP2040 | Default Fallback |
| :--- | :--- | :--- | :--- |
| **PIN_PWM_A** | PWM Phase A Drive | D7 / GPIO 7 | Pin 7 |
| **PIN_PWM_B** | PWM Phase B Drive | D8 / GPIO 8 | Pin 8 |
| **PIN_BEMF_A** | BEMF Sense Terminal A | A0 / GPIO 26 | Pin A0 |
| **PIN_BEMF_B** | BEMF Sense Terminal B | A1 / GPIO 27 | Pin A1 |
| **PIN_SHUNT** | Current Sense Shunt | A2 / GPIO 28 | Pin A2 |
| **PIN_LED1** | Status LED 1 (Activity) | GPIO 15 (Red LED) | Pin 13 |
| **PIN_LED2** | Status LED 2 (Gap Indicator)| GPIO 16 (Blue LED) | Pin 12 |

*Note: For the Seeed Studio XIAO RP2040, the onboard LEDs are active-low, and the sketch correctly handles this behavior.*

## Telemetry Format

Data is logged to the serial monitor as a comma-separated stream (`CSV`) at **921600 baud** in the following format:

```csv
<TIME_US>,<PWM_FREQ>,<PWM_DUTY>,<IS_GAP>,<BEMF_A>,<BEMF_B>,<SHUNT>
```

- **TIME_US**: Microsecond timestamp (`micros()`).
- **PWM_FREQ**: Current PWM frequency in Hz (e.g. `47`, `100`, `220`, `470`, `1000`, `2200`, `4700`, `10000`, `22000`, `47000`, `100000`).
- **PWM_DUTY**: Signed integer representation of the active PWM duty cycle. Runs from `-255` (backward max) to `255` (forward max). This allows plotting utilities (such as PlatformIO Teleplot, Arduino Serial Plotter, or Python scripts) to cleanly differentiate between directions.
- **IS_GAP**: A boolean flag (`1` or `0`) indicating whether the data point was captured during a measurement gap.
- **BEMF_A**: Raw 12-bit ADC value (0 to 4095) read on Terminal A.
- **BEMF_B**: Raw 12-bit ADC value (0 to 4095) read on Terminal B.
- **SHUNT**: Raw 12-bit ADC value (0 to 4095) read on Current Sense Shunt (`PIN_SHUNT`).

## Compilation and Upload

### Prerequisite: PlatformIO
Ensure you have PlatformIO Core installed. If you use VS Code, you can install the PlatformIO IDE extension.

To compile and upload the sketch from the command line:

```bash
# Navigate to the BEMF_Ramp_Test directory
cd examples/BEMF_Ramp_Test

# Build and upload for Seeed Studio XIAO RP2040
pio run -e seeed_xiao_rp2040 -t upload
```

To monitor the high-speed telemetry:

```bash
pio device monitor -b 921600
```
