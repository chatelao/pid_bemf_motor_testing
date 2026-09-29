# Sequential Blink Example

This example demonstrates sequential LED toggling across multiple status LEDs and digital outputs on the Seeed Studio XIAO RP2040 board.

## Purpose

The sketch serves as a hardware validation and pin-mapping check tool for the XIAO RP2040 platform. It verifies that both onboard RGB LEDs and auxiliary output pins (such as oscilloscope/function outputs `D9` and `D10`) are properly mapped and functioning.

## Key Features

- **Sequential Pin Toggling**: Iterates through an array of digital outputs, switching each pin `HIGH` for 500 ms and `LOW` for 250 ms.
- **Onboard RGB LED Verification**: Cycles through the individual red, green, and blue onboard LEDs.
- **Trigger/Function Line Check**: Cycles through auxiliary control lines `D9` (`F0f`) and `D10` (`F0r`).

## Pin Mapping

| Pin Name | Physical Pin / Macro | Signal / Function |
| :--- | :--- | :--- |
| **16** | GPIO 16 (`PIN_LED_R`) | Onboard Red LED |
| **17** | GPIO 17 (`PIN_LED_G`) | Onboard Green LED |
| **25** | GPIO 25 (`PIN_LED_B`) | Onboard Blue LED |
| **D9** | GPIO 9 (`D9`) | Function Output / Scope Trigger F0 Forward (`F0f`) |
| **D10** | GPIO 10 (`D10`) | Function Output / Scope Trigger F0 Reverse (`F0r`) |

*Note: Onboard LEDs on the XIAO RP2040 are active-low.*

## Code Structure

1. **Setup**: Configures each specified pin in `blinkPins[]` as `OUTPUT`.
2. **Loop**: Sequentially sets each pin `HIGH` for 500 ms, then `LOW` for 250 ms before advancing to the next pin.

## Compilation and Upload

Using PlatformIO Core or the Arduino IDE:

```bash
# Navigate to the project root or example folder
cd examples/Blink

# Using Arduino CLI (for XIAO RP2040)
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Blink.ino
```
