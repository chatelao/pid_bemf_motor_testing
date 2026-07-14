# CONCEPT - Märklin Motor Test & Calibration Tool

## Goal
Create tool sketches for the XIAO 2040 using BDR6133 to test Märklin Motor behaviour after refit with permanent magnets to analyze and calibrate a PID driver using BEMF.

## Business Cases
- **Modernization of Heritage Locomotives**: Enable collectors to upgrade vintage Märklin locomotives with modern control electronics while maintaining smooth performance.
- **Precision Performance Analysis**: Provide a systematic way to measure motor characteristics (BEMF constant) to ensure optimal speed-to-voltage mapping.
- **Cost-Effective Diagnostics**: Offer a low-cost, open-source hardware solution for hobbyists to diagnose motor health and tune PID parameters without expensive commercial equipment.

## Use Cases
- **BEMF Characterization**: Automatically measure the Back-Electromotive Force at various PWM duty cycles to determine the motor's speed constant ($K_e$).
- **PID Parameter Calibration**: Perform automated sweeps of Proportional, Integral, and Derivative gains to find the most stable and responsive control loop settings.
- **Real-time Performance Logging**: Stream motor speed, current consumption, and control error to a PC for visualization and analysis.
- **Safety Monitoring**: Detect motor stalls or overcurrent conditions via the shunt resistor to protect the driver and the motor.
- **Absolute Position Tracking**: Detect commutator ripples in the motor current to achieve "encoder-like" absolute position knowledge without physical encoders.

## High-Level Architecture
The system is composed of four main functional modules:

1.  **Control Logic (RP2040)**: Manages the high-frequency PWM generation, executes the PID control algorithm, and handles user communication.
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

## Wiring Diagram
```text
                 +--------------------+      +--------------------+         +---------------+
                 |      RP2040        |      |     BDR-6133       |         |     Motor     |
                 |    (Top View)      |      |    Motor Driver    |         | DC brushed    |
                 +--------------------+      +--------------------+         +---------------+
                 |                5v  |      |                    |         |               |
      ---``|<----| D15 (LED)      GND |      |                    |         |               |
      ---``|<----| D16 (LED)      3v3 |      |                    |         |               |
                 |                    |      |                    |         |               |
                 |        (PWM B) D8  |----->| InB           OutB |=====+==>| B             |
                 |        (PWM A) D7  |----->| InA           OutA |==+==|==>| A             |
                 |                    |      +---------+----------+  |  |   +---------------+
                 |       (Shunt)  A2  |<.............../             |  |
                 |       (bEMF B) A1  |<----------------------------/   |
                 |       (bEMF A) A0  |<-------------------------------/
```

## Major Choices

### 1. Feedback Method
- **Alternative A: BEMF Sensing (Selected)**: Uses the motor itself as a generator during PWM off-times. Requires no extra mechanical parts and is cost-effective for model trains.

### 2. Control Strategy
- **Alternative A: PID Control (Selected)**: Standard closed-loop control that balances responsiveness and stability. Well-supported by RP2040's processing power.

### 3. User Interface
- **Alternative A: Serial CLI over USB (Selected)**: Leveraging the native USB capabilities of the RP2040. Requires no extra hardware and is ideal for developers.

### 4. Position Tracking Method
- **Alternative A: Commutator Ripple Detection (Selected)**: Analyzes the high-frequency current fluctuations caused by the commutator switching. Enables position tracking on stock motors without mechanical modifications.

## Discarded Alternatives

### 1. Feedback Method
- **Alternative B: Optical Encoder**: Requires mounting a disk and sensor on the motor shaft. Highly accurate but mechanically difficult to fit inside small locomotives.
- **Alternative C: Hall Effect Sensor**: Requires mounting magnets on the rotor. Less accurate than encoders and also presents mechanical integration challenges.

### 2. Control Strategy
- **Alternative B: Open-Loop PWM**: Simple to implement but cannot maintain constant speed under varying loads (e.g., climbing hills).
- **Alternative C: Bang-Bang Control**: Simple on/off control. High efficiency but results in jerky motion and poor low-speed performance.

### 3. User Interface
- **Alternative B: Physical Potentiometer and Buttons**: Intuitive for manual control but lacks the precision needed for PID calibration and data logging.
- **Alternative C: Web/Mobile App via Wi-Fi**: User-friendly but requires more expensive hardware (e.g., ESP32) and increases software complexity significantly.

### 4. Position Tracking Method
- **Alternative B: Physical Encoders**: Use optical or magnetic encoders. Very precise but requires mechanical modification of the locomotive, which is often impossible in small scales.
- **Alternative C: Time-Speed Integration**: Estimate position by integrating speed over time. Error accumulates quickly due to load variations and wheel slip, making it unsuitable for absolute positioning.
