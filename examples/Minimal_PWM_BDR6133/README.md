# Minimal PWM BDR6133 Example

This example demonstrates open-loop PWM motor control using the Arduino framework on the Seeed Studio XIAO RP2040 connected to a BDR-6133 motor driver stage.

## Purpose

This minimal test sketch verifies basic H-bridge PWM driving capabilities at ultrasonic frequencies (**20 kHz**), while driving the onboard NeoPixel RGB LED to visually mirror the active PWM duty cycle.

## Key Features

- **20 kHz Ultrasonic PWM**: Configures `analogWriteFreq(20000)` to drive the motor silently above the audible frequency range.
- **8-Bit PWM Resolution**: Uses `analogWriteRange(255)` for standard 0–255 duty cycle control.
- **Visual Duty Cycle Indicator**: Drives the onboard WS2812 NeoPixel (connected via GPIO 12 with power enabled on GPIO 11) using the `Adafruit_NeoPixel` library.
- **PWM Ramp Cycle**: Smoothly ramps the PWM output on `D8` up from 0 to 255 (Red LED) and back down to 0 (Green LED), with a blue status indication in between.

## Hardware & Pin Mapping

| Signal | Seeed Studio XIAO RP2040 Pin | Function Description |
| :--- | :--- | :--- |
| **PIN_PWM_A** | `D7` / GPIO 1 | BDR-6133 Phase A PWM Drive |
| **PIN_PWM_B** | `D8` / GPIO 2 | BDR-6133 Phase B PWM Drive |
| **PIN_NEO_PWR**| GPIO 11 | Onboard NeoPixel Power Enable (Active HIGH) |
| **PIN_NEO_DAT**| GPIO 12 | Onboard NeoPixel Data Line |

## Code Structure

1. **Setup**:
   - Sets PWM frequency to 20 kHz and resolution to 8 bits (0–255).
   - Enables power to the onboard NeoPixel on GPIO 11 and initializes the `Adafruit_NeoPixel` instance.
   - Sets `D7` and `D8` as outputs with initial 0% duty cycle.
2. **Loop**:
   - Sweeps `D8` PWM duty cycle from 0 to 255 in 10 ms steps, updating the NeoPixel red channel.
   - Illuminates the NeoPixel blue for 500 ms at peak PWM.
   - Sweeps `D8` PWM duty cycle back down from 255 to 0 in 10 ms steps, updating the NeoPixel green channel.

## Compilation and Upload

```bash
# Using Arduino CLI for Seeed Studio XIAO RP2040
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_BDR6133.ino
```
