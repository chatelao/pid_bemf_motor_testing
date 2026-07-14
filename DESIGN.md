# DESIGN - Märklin Motor Test & Calibration Tool

## Introduction
This document details the technical implementation of the Märklin Motor Test & Calibration Tool, derived from the [CONCEPT.md](CONCEPT.md).

## Detailed Architecture
The system follows a modular architecture as shown in the top-level diagram:

![Top Architecture](TOP_ARCHITECTURE.puml)

### Technical Interfaces
- **Motor Control Interface**: Two PWM outputs driving the BDR-6133 H-bridge.
- **Analog Sensing Interface**:
  - BEMF: Two ADC channels (connected to Motor Out A/B).
  - Current: One ADC channel (connected to the shunt resistor).
- **Serial Communication**: 115200 baud over USB CDC.

## Technological Stack
- **Microcontrollers**:
  - Seeed Studio XIAO RP2040 (Dual-core ARM Cortex M0+)
  - ST Nucleo-F446RE (ARM Cortex M4)
  - ST Nucleo-G431RB (ARM Cortex M4)
- **Motor Driver**: BDR-6133 H-bridge.
- **Framework**: Arduino Framework with PlatformIO for build management and testing.
- **Language**: C++ (Arduino flavor).

## Technical Implementation Choices

### 1. Hardware Pin Mapping
To maintain portability, the tool uses standard Arduino pin labels where possible.

| Function | XIAO RP2040 | Nucleo F446RE | Nucleo G431RB | Description |
|---|---|---|---|---|
| **PWM A** | D7 | D7 | D7 | Motor Output A Control |
| **PWM B** | D8 | D8 | D8 | Motor Output B Control |
| **ADC BEMF A** | A0 | A0 | A0 | Feedback from Motor Terminal A |
| **ADC BEMF B** | A1 | A1 | A1 | Feedback from Motor Terminal B |
| **ADC Shunt** | A2 | A2 | A2 | Current measurement across shunt |
| **LED 1** | D15 | D13 (Onboard) | D13 (Onboard) | Status LED (Red/Green) |
| **LED 2** | D16 | D12 | D12 | Status LED (Blue) |

### 2. PWM & ADC Parameters
- **PWM Frequency**: 20 kHz (to ensure ultrasonic operation and minimize torque ripple).
- **ADC Resolution**: 12-bit (Native resolution for all target MCUs) for precise BEMF and current measurements.
- **Sampling Rate**: Synchronized with the PWM "off" cycle for BEMF.

### 3. BEMF Sampling Implementation
The chosen implementation for BEMF sensing is **Synchronous ADC Polling during PWM-off time**.

- **Implementation**: The control loop waits for the PWM signal to go low (H-bridge in high-impedance or coasting mode) and then triggers the ADC conversion.
- **Justification**: This is the most straightforward method that guarantees sampling during the correct window without complex DMA or interrupt management across different MCU architectures.

### 4. PID Control Implementation
The chosen implementation for the control strategy is the **Arduino PID Library**.

- **Implementation**: Utilize the standard `PID_v1` library or a compatible variant.
- **Justification**: This library is well-tested, platform-independent within the Arduino ecosystem, and provides essential features like bumpless transfer and anti-windup.

### 5. Serial Interface Realization
The chosen implementation for the User Interface is the **SerialCommands Library**.

- **Implementation**: A command-dispatcher library that maps serial strings to internal C++ functions.
- **Justification**: Provides a clean way to handle multiple commands and parameters across all supported boards.

### 6. Hardware Abstraction Layer
The chosen implementation for the HAL is the **Arduino Core**.

- **Alternative A: Arduino Core (Selected)**: Provides a unified API for GPIO, PWM, and ADC across RP2040 and STM32. High portability and library support.
- **Alternative B: Native Vendor SDKs (HAL/LL/pico-sdk)**: Offers maximum performance and control. However, it requires separate codebases for RP2040 and STM32, increasing maintenance overhead.
- **Alternative C: MBED OS**: Standard on some STM32 boards. Provides RTOS features but is heavier than necessary and support for RP2040 in Arduino varies.

## Discarded Technical Alternatives

### 1. BEMF Sampling
- **Alternative B: DMA Circular Buffer**: Sampling ADC continuously into a DMA buffer.
  - *Reason for discarding*: Overly complex; difficult to synchronize with PWM duty cycle across different DMA controllers (RP2040 vs STM32).
- **Alternative C: Timer-Triggered ADC Interrupt**: Using a hardware timer to trigger ADC conversion.
  - *Reason for discarding*: Requires platform-specific timer configuration (TIM for STM32, PWM/Timer for RP2040).

### 2. PID Control
- **Alternative B: Custom Floating-Point PID**: Manual implementation of the PID equation.
  - *Reason for discarding*: Standard libraries are already robust and platform-independent.
- **Alternative C: Custom Fixed-Point PID**: Implementation using integer math.
  - *Reason for discarding*: Modern MCUs (RP2040/STM32) have enough floating-point performance (Hardware FPU in STM32, ROM functions in RP2040).

### 3. Serial Interface
- **Alternative B: Simple `Serial.readString()`**: Basic string reading.
  - *Reason for discarding*: Scalability and maintenance issues.
- **Alternative C: Microrl**: A full-featured interactive shell.
  - *Reason for discarding*: Too heavy for simple sketches.

### 4. Hardware Abstraction Layer
- **Alternative B: Native Vendor SDKs**: Too much platform-specific code needed for a "tool sketch".
- **Alternative C: MBED OS**: Increases binary size and complexity without significant benefit for this application.
