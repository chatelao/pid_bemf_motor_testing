# BEMF Ramp Characterization Test

This example sketch is designed to characterize and test a refitted Märklin motor using an open-loop PWM sweep. It is optimized to run on the Seeed Studio XIAO RP2040 and ST Nucleo-F446RE / Nucleo-G431RB development boards using the BDR-6133 motor driver stage.

## Purpose

When refitting Märklin AC/DC motors with permanent magnets, the Back-EMF (BEMF) response profile must be characterized to determine motor constants and choose appropriate closed-loop speed control (PID) parameters. This sketch automates that characterization process by sweeping the motor driver's PWM duty cycle up and down bidirectionally while logging high-frequency telemetry data.

## Features

- **Open-Loop Bidirectional PWM Ramp**: Sweeps the PWM from 0 to 255 (100% duty cycle) over 1 second, then back down to 0 over 1 second. It then repeats this cycle in the reverse direction.
- **High-Frequency Telemetry Logging**: Emits real-time BEMF and PWM telemetry at **40 kHz** (every 25 microseconds) over USB Serial at **921,600 baud**. The serial writing uses a best-effort, non-blocking check (`Serial.availableForWrite() >= 32`) to prevent high-frequency printing from choking the microprocessor's control loop.
- **Synchronous Measurement Gaps**: To reliably measure BEMF without the influence of active PWM driving (especially critical at high duty cycles), the sketch inserts a **25 ms "measurement gap" (PWM = 0) every 250 ms**. During this gap, the BEMF terminal voltage decays and stabilizes to represent true coasting speed.
- **Platform-Agnostic Pin Configuration**: Supports the Seeed Studio XIAO RP2040 and STM32 Nucleo boards seamlessly through preprocessor macro detection.

## Pin Mapping

The pinouts are standardized across platforms to interface with the BDR-6133 driver stage:

| Signal | Description | Seeed Studio XIAO RP2040 | ST Nucleo (F446RE / G431RB) | Default Fallback |
| :--- | :--- | :--- | :--- | :--- |
| **PIN_PWM_A** | PWM Phase A Drive | D7 / GPIO 7 | D7 / PA8 | Pin 7 |
| **PIN_PWM_B** | PWM Phase B Drive | D8 / GPIO 8 | D8 / PA9 | Pin 8 |
| **PIN_BEMF_A** | BEMF Sense Terminal A | A0 / GPIO 26 | A0 / PA0 | Pin A0 |
| **PIN_BEMF_B** | BEMF Sense Terminal B | A1 / GPIO 27 | A1 / PA1 | Pin A1 |
| **PIN_SHUNT** | Current Sense Shunt | A2 / GPIO 28 | A2 / PA4 | Pin A2 |
| **PIN_LED1** | Status LED 1 (Activity) | GPIO 15 (Red LED) | LED_BUILTIN / D13 | Pin 13 |
| **PIN_LED2** | Status LED 2 (Gap Indicator)| GPIO 16 (Blue LED) | D12 / PA6 | Pin 12 |

*Note: For the Seeed Studio XIAO RP2040, the onboard LEDs are active-low, and the sketch correctly handles this behavior.*

## Telemetry Format

Data is logged to the serial monitor as a comma-separated stream (`CSV`) at **921600 baud** in the following format:

```csv
<SIGNED_PWM>,<BEMF_A>,<BEMF_B>,<IS_GAP>
```

- **SIGNED_PWM**: Signed integer representation of the active PWM duty cycle. Runs from `-255` (backward max) to `255` (forward max). This allows plotting utilities (such as PlatformIO Teleplot, Arduino Serial Plotter, or Python scripts) to cleanly differentiate between directions.
- **BEMF_A**: Raw 12-bit ADC value (0 to 4095) read on Terminal A.
- **BEMF_B**: Raw 12-bit ADC value (0 to 4095) read on Terminal B.
- **IS_GAP**: A boolean flag (`1` or `0`) indicating whether the data point was captured during a measurement gap.

## Compilation and Upload

### Prerequisite: PlatformIO
Ensure you have PlatformIO Core installed. If you use VS Code, you can install the PlatformIO IDE extension.

To compile and upload the sketch from the command line:

```bash
# Navigate to the BEMF_Ramp_Test directory
cd examples/BEMF_Ramp_Test

# Build and upload for Seeed Studio XIAO RP2040
pio run -e seeed_xiao_rp2040 -t upload

# Build and upload for Nucleo F446RE
pio run -e nucleo_f446re -t upload

# Build and upload for Nucleo G431RB
pio run -e nucleo_g431rb -t upload
```

To monitor the high-speed telemetry:

```bash
pio device monitor -b 921600
```
