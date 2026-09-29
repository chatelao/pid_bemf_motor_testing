# Minimal PWM BDR6133 Bare-Metal RP2040 SDK Example

This example demonstrates bare-metal hardware PWM and WS2812 RGB LED driving on the Seeed Studio XIAO RP2040 using the Raspberry Pi Pico C/C++ SDK directly within an Arduino environment.

## Purpose

This sketch illustrates low-level hardware control without relying on standard Arduino HAL abstractions like `analogWrite()` or third-party WS2812 libraries. By interacting directly with the RP2040's hardware PWM slices and programmable I/O (PIO) state machines, it provides deterministic control and minimal execution overhead.

## Key Features

- **RP2040 Bare-Metal Hardware PWM**:
  - Uses `hardware/pwm.h` to directly configure hardware PWM slices and channels for GPIO 1 (`D7`) and GPIO 2 (`D8`).
  - Sets a 1 MHz timer counter clock (125 MHz system clock divided by 125) and a counter wrap of 2,499 ticks, resulting in a precise **400 Hz PWM frequency**.
  - Provides a custom `set_pwm_8bit()` function to scale 8-bit duty cycles (`0–255`) to the underlying counter range.
- **Bare-Metal PIO WS2812 Driver**:
  - Employs a custom 4-instruction PIO assembly program (`hardware/pio.h`) compiled as `ws2812_program` to generate 800 kHz GRB bitstreams for the onboard NeoPixel on GPIO 12.
  - Implements direct blocking FIFO writes (`pio_sm_put_blocking()`) and scaling logic to match custom brightness settings.

## Hardware & Pin Mapping

| Signal | Physical GPIO | Board Label | Function |
| :--- | :--- | :--- | :--- |
| **PIN_D7** | GPIO 1 | `D7` | BDR-6133 PWM Phase A |
| **PIN_D8** | GPIO 2 | `D8` | BDR-6133 PWM Phase B |
| **PIN_NEO_PWR** | GPIO 11 | — | Onboard NeoPixel Power Enable |
| **PIN_NEO_DAT** | GPIO 12 | — | Onboard NeoPixel PIO Data Line |

## Code Architecture

1. **PIO State Machine Assembly**:
   - `ws2812_program_instructions[]`: Encodes the WS2812 bit timing state machine.
   - `ws2812_program_init()`: Configures PIO0 State Machine 0, clock divider, and side-set pins for 800 kHz GRB transmission.
2. **Hardware PWM Setup**:
   - Maps GPIO 1 (`D7`) and GPIO 2 (`D8`) to their respective hardware PWM slices.
   - Initializes counter limits and initializes duty cycles to 0.
3. **Control Loop**:
   - Ramps `D8` PWM duty cycle up and down while updating NeoPixel color channels via direct PIO FIFO writes.

## Compilation and Upload

```bash
# Using Arduino CLI with RP2040 board core
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_BDR6133_SDK.ino
```
