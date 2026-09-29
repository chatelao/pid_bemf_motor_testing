# Minimal PWM BDR6133 SDK with Adafruit NeoPixel Example

This example combines low-level RP2040 SDK hardware PWM configuration with the high-level `Adafruit_NeoPixel` library for visual indicator control on the Seeed Studio XIAO RP2040.

## Purpose

Demonstrates how low-level RP2040 C/C++ SDK calls (`hardware/pwm.h`) for ultrasonic 20 kHz PWM generation on motor driver pins can coexist seamlessly with standard Arduino libraries (`Adafruit_NeoPixel`) for onboard status lighting.

## Key Features

- **20 kHz Ultrasonic Hardware PWM (RP2040 SDK)**:
  - Uses `hardware/pwm.h` to configure hardware PWM slices for GPIO 1 (`D7`) and GPIO 2 (`D8`).
  - System clock (125 MHz) divided by 125 yields a 1 MHz counter clock. Set with a `PWM_WRAP` of 50, this produces an ultrasonic **20 kHz PWM frequency** to prevent motor coil whine.
- **Adafruit NeoPixel Integration**:
  - Uses `Adafruit_NeoPixel` for single-pixel RGB status indications on GPIO 12 with power enabled on GPIO 11.
- **Duty Cycle Ramping**:
  - Smoothly ramps duty cycle on `D8` while driving status colors (Red during ramp-up, Blue at full speed, Green during ramp-down).

## Hardware & Pin Mapping

| Signal | Physical GPIO | Board Label | Function |
| :--- | :--- | :--- | :--- |
| **PIN_D7** | GPIO 1 | `D7` | BDR-6133 PWM Phase A |
| **PIN_D8** | GPIO 2 | `D8` | BDR-6133 PWM Phase B |
| **PIN_NEO_PWR** | GPIO 11 | — | Onboard NeoPixel Power Enable |
| **PIN_NEO_DAT** | GPIO 12 | — | Onboard NeoPixel Data Line |

## Code Architecture

1. **Setup**:
   - Initializes `PIN_NEO_PWR` (GPIO 11) `HIGH` and initializes `Adafruit_NeoPixel`.
   - Configures GPIO 1 (`D7`) and GPIO 2 (`D8`) as `GPIO_FUNC_PWM`.
   - Configures PWM clock divider (`125.0f`) and wrap count (`50`) to set the frequency to 20 kHz.
2. **Loop**:
   - Converts 8-bit values (`0–255`) to internal wrap limits using `set_pwm_8bit()`.
   - Sweeps motor duty cycle while updating NeoPixel color output.

## Compilation and Upload

```bash
# Using Arduino CLI for Seeed Studio XIAO RP2040
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_BDR6133_Semdi-SDK.ino
```
