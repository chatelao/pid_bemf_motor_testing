# CONCEPT - Märklin Motor Test & Calibration Tool

## Goal
Create tool sketches for the XIAO 2040, Nucleo STM32F446RE and STM32G431 using BDR6133 to test Märklin Motor behaviour after refit with permanent magnets to analyze and calibrate a PID driver using BEMF.

## Business Cases
- **Modernization of Heritage Locomotives**: Enable collectors to upgrade vintage Märklin locomotives with modern control electronics while maintaining smooth performance.
- **Precision Performance Analysis**: Provide a systematic way to measure motor characteristics (BEMF constant) to ensure optimal speed-to-voltage mapping.
- **Cost-Effective Diagnostics**: Offer a low-cost, open-source hardware solution for hobbyists to diagnose motor health and tune PID parameters without expensive commercial equipment.

## Use Cases
- **BEMF Characterization**: Automatically measure the Back-Electromotive Force at various PWM duty cycles to determine the motor's speed constant ($K_e$).
- **PID Parameter Calibration**: Perform automated sweeps of Proportional, Integral, and Derivative gains to find the most stable and responsive control loop settings.
- **Real-time Performance Logging**: Stream motor speed, current consumption, and control error to a PC for visualization and analysis.
- **Safety Monitoring**: Detect motor stalls or overcurrent conditions via the shunt resistor to protect the driver and the motor.

## High-Level Architecture
The system is composed of four main functional modules:

1.  **Control Logic (MCU)**: Manages the high-frequency PWM generation, executes the PID control algorithm, and handles user communication. Supports RP2040 and STM32 platforms.
2.  **Power Stage (BDR-6133)**: An H-bridge driver that translates low-voltage PWM signals into high-current motor drive signals.
3.  **Sensing Unit**:
    - **BEMF Sensing**: High-impedance analog inputs to measure motor voltage during "off" PWM cycles.
    - **Current Sensing (Shunt)**: Measures voltage drop across a shunt resistor to determine motor load.
4.  **Interface**:
    - **Visual Feedback**: LEDs for status (e.g., Run/Stop, Error).
    - **Digital Interface**: Serial CLI over USB for command entry and data logging.

### Business Interfaces
- **User Command Interface**: Accepts speed setpoints and PID tuning parameters.
- **Data Export Interface**: Provides telemetry data in a format suitable for external plotting tools (e.g., CSV, Serial Plotter).
- **Motor Power Interface**: Standard two-wire connection to DC brushed motors.

## Wiring Diagram (Generic)
```text
                 +--------------------+      +--------------------+         +---------------+
                 |        MCU         |      |     BDR-6133       |         |     Motor     |
                 | (RP2040 / STM32)   |      |    Motor Driver    |         | DC brushed    |
                 +--------------------+      +--------------------+         +---------------+
                 |                VCC |      |                    |         |               |
      ---``|<----| Status LED 1   GND |      |                    |         |               |
      ---``|<----| Status LED 2   3V3 |      |                    |         |               |
                 |                    |      |                    |         |               |
                 |        (PWM B)     |----->| InB           OutB |=====+==>| B             |
                 |        (PWM A)     |----->| InA           OutA |==+==|==>| A             |
                 |                    |      +---------+----------+  |  |   +---------------+
                 |       (Shunt)  ADC |<.............../             |  |
                 |       (bEMF B) ADC |<----------------------------/   |
                 |       (bEMF A) ADC |<-------------------------------/
```

## Major Choices

### 1. Feedback Method
- **Alternative A: BEMF Sensing (Selected)**: Uses the motor itself as a generator during PWM off-times. Requires no extra mechanical parts and is cost-effective for model trains.
- **Alternative B: Optical Encoder**: Requires mounting a disk and sensor on the motor shaft. Highly accurate but mechanically difficult to fit inside small locomotives.
- **Alternative C: Hall Effect Sensor**: Requires mounting magnets on the rotor. Less accurate than encoders and also presents mechanical integration challenges.

### 2. Control Strategy
- **Alternative A: PID Control (Selected)**: Standard closed-loop control that balances responsiveness and stability. Well-supported by modern MCU processing power.
- **Alternative B: Open-Loop PWM**: Simple to implement but cannot maintain constant speed under varying loads.
- **Alternative C: Bang-Bang Control**: Simple on/off control. High efficiency but results in jerky motion.

### 3. User Interface
- **Alternative A: Serial CLI over USB (Selected)**: Leveraging the native USB capabilities of the MCUs. Requires no extra hardware and is ideal for developers.
- **Alternative B: Physical Potentiometer and Buttons**: Intuitive for manual control but lacks precision for calibration.
- **Alternative C: Web/Mobile App via Wi-Fi/Bluetooth**: User-friendly but requires more expensive hardware and increases complexity.

### 4. Target Platform Support
- **Alternative A: Multi-Platform Arduino (Selected)**: Use the Arduino framework to support XIAO RP2040 and STM32 Nucleo boards. Provides high portability and a vast library ecosystem.
- **Alternative B: Specific MCU (Single Platform)**: Focus only on XIAO RP2040. Simplifies development but limits the tool's reach and hardware flexibility.
- **Alternative C: Real-Time OS (RTOS)**: Use FreeRTOS or Zephyr. Offers better task management but increases overhead and learning curve for simple tool sketches.

## Discarded Alternatives

### 1. Feedback Method
- **Alternative B: Optical Encoder**: Highly accurate but mechanically difficult to fit inside small locomotives.
- **Alternative C: Hall Effect Sensor**: Less accurate than encoders and also presents mechanical integration challenges.

### 2. Control Strategy
- **Alternative B: Open-Loop PWM**: Cannot maintain constant speed under varying loads (e.g., climbing hills).
- **Alternative C: Bang-Bang Control**: Results in jerky motion and poor low-speed performance.

### 3. User Interface
- **Alternative B: Physical Potentiometer and Buttons**: Lacks the precision needed for PID calibration and data logging.
- **Alternative C: Web/Mobile App via Wi-Fi/Bluetooth**: Increases hardware cost and software complexity significantly.

### 4. Target Platform Support
- **Alternative B: Specific MCU**: Limits the utility of the tool to a single hardware platform.
- **Alternative C: Real-Time OS (RTOS)**: Overly complex for the requirements of a motor calibration tool.
