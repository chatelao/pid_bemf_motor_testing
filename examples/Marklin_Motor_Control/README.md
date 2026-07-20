# Märklin Closed-Loop Motor Control

This example sketch implements Phase 4 speed regulation of a refitted Märklin AC/DC motor using a custom inline Proportional-Integral-Derivative (PID) controller, synchronous BEMF sensing, and a robust Command Line Interface (CLI) over USB Serial.

It supports the **Seeed Studio XIAO RP2040**, **ST Nucleo-F446RE**, and **ST Nucleo-G431RB** development boards and controls the motor via the BDR-6133 motor driver stage.

## Purpose

Once a motor's BEMF characteristics are understood (e.g., using the `BEMF_Ramp_Test` tool), this closed-loop controller regulates the motor's actual speed (BEMF feedback) to match desired setpoints. It demonstrates stable bidirectional setpoint tracking under varying load conditions, making it an excellent platform for tuning Kp, Ki, and Kd coefficients on custom model railroad locomotives.

## Key Features

- **Custom Inline PID Controller**: Implements a minimal, lightweight PID class mimicking standard Arduino PID behaviors. This avoids external library dependencies, ensures deterministic floating-point execution, and protects against integral windup when the motor is stopped or heavily overloaded.
- **Synchronous BEMF Sensing**: The BEMF sensing takes place during a highly periodic 50 ms loop. Inside this control cycle, the motor's PWM is temporarily disabled (measurement gap), and the microcontroller pauses for **1.5 ms** to let the inductive flyback current decay. The raw BEMF voltage is then cleanly read on the coasting terminal.
- **Multistep Setpoint Profile**: Features a pre-programmed, bidirectional, multi-step profile that steps through various BEMF targets (500, 1500, 2500, 1000) in both forward and backward directions to demonstrate closed-loop tracking capabilities.
- **Sanitized Serial CLI**: Houses an integrated command interpreter powered by the `SerialCommands` library. To prevent common parsing failures across different operating systems (such as Windows `\r\n` vs Unix `\n`), a custom `SanitizedSerialStream` class is used to filter out carriage returns, making terminal command input exceptionally robust.
- **Ultrasonic PWM**: Drive signals are generated at an ultrasonic frequency of **20 kHz** to eliminate audible motor buzzing and coil whine.

## CLI Commands

The CLI operates at **115,200 baud** and supports the following clean, shorthand command characters:

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `f` | *None* | Sets the allowable direction to **Forward Only** (forces direction variable forward). |
| `b` | *None* | Sets the allowable direction to **Backward Only** (forces direction variable backward). |
| `a` | *None* | Sets the allowable direction to **Bidirectional (Both)** (automatically alternates direction periodically). |
| `s` | `<speed>` | Switches to **Constant Speed Control** and sets the target BEMF setpoint (0 to 4095). |
| `p` | `<pwm>` | Switches to **Constant Open-Loop PWM Control** (0 to 255) for manual profiling or calibration. |
| `prof` | *None* | Resets and runs the pre-programmed bidirectional multi-step **Setpoint Profile**. |
| `h` or `help` | *None* | Displays the CLI help menu and the current system status (Modes, Targets, and Direction). |

### Error Handling

If an invalid or unrecognized command is sent, the controller returns a clean error:
```text
>>> Error: Unrecognized command '<input>'. Type 'h' or 'help' for assistance.
```

## Pin Mapping

The pin assignments match standard project guidelines:

| Signal | Description | Seeed Studio XIAO RP2040 | ST Nucleo (F446RE / G431RB) | Default Fallback |
| :--- | :--- | :--- | :--- | :--- |
| **PIN_PWM_A** | PWM Phase A Drive | D7 / GPIO 7 | D7 / PA8 | Pin 7 |
| **PIN_PWM_B** | PWM Phase B Drive | D8 / GPIO 8 | D8 / PA9 | Pin 8 |
| **PIN_BEMF_A** | BEMF Sense Terminal A | A0 / GPIO 26 | A0 / PA0 | Pin A0 |
| **PIN_BEMF_B** | BEMF Sense Terminal B | A1 / GPIO 27 | A1 / PA1 | Pin A1 |
| **PIN_SHUNT** | Current Sense Shunt | A2 / GPIO 28 | A2 / PA4 | Pin A2 |
| **PIN_LED1** | Status LED 1 (Activity) | PIN_LED_R (Red Onboard) | LED_BUILTIN / D13 | Pin 13 |
| **PIN_LED2** | Status LED 2 (Gap Indicator)| PIN_LED_B (Blue Onboard) | D12 / PA6 | Pin 12 |

*Note: For the Seeed Studio XIAO RP2040, the onboard LEDs are active-low, and the sketch correctly handles this behavior.*

## Telemetry Format

During operation, real-time control metrics are outputted to the serial port every 50 ms:

```text
SP:<setpoint>, BEMF:<measured>, PWM:<output>, DIR:<dir>, MODE:<mode>
```

- **SP**: The active PID BEMF target (0–4095) or `N/A` if in raw PWM mode.
- **BEMF**: The actual 12-bit raw ADC voltage measured on the coasting terminal.
- **PWM**: The active 8-bit PWM command (0–255) calculated by the PID loop.
- **DIR**: Active driving direction (`FWD` or `REV`).
- **MODE**: Active control loop mode (`PROFILE`, `SPEED`, or `PWM`).

## Compilation and Upload

### Dependencies
The sketch relies on the **SerialCommands** library (v2.2.0), which PlatformIO will automatically install as defined in the local `platformio.ini` dependencies list.

### Build & Upload Commands

Using PlatformIO Core:

```bash
# Navigate to the example directory
cd examples/Marklin_Motor_Control

# Build and upload for Seeed Studio XIAO RP2040
pio run -e seeed_xiao_rp2040 -t upload

# Build and upload for Nucleo F446RE
pio run -e nucleo_f446re -t upload

# Build and upload for Nucleo G431RB
pio run -e nucleo_g431rb -t upload
```

To connect to the CLI interpreter:

```bash
pio device monitor -b 115200
```
