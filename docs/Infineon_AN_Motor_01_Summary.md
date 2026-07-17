# Infineon Application Note AN-Motor-01 Summary

## Title
**DC Motor Control using Ripple Counting**

---

## 1. Executive Summary
Infineon AN-Motor-01 focuses on the implementation of sensorless position tracking and speed control of brushed DC motors via ripple counting, with a deep dive into the practical non-idealities encountered in real-world environments. While the theoretical model of ripple counting is simple, dynamic operating conditions—specifically rapid load transients, starting/stopping phases, and voltage spikes—introduce severe signal distortions. The application note details these problem areas and presents robust mitigation strategies in both hardware and software to combat "missed pulses" (false negatives) and "double counts" (false positives).

---

## 2. Dynamic Transients and the "Missed Pulses" Problem

In an ideal, constant-speed, constant-load environment, the current ripple is a clean, uniform wave. However, real-world motor control (such as train locomotives climbing grades, starting, or braking) is highly dynamic.

```
Normal Operation:    ~~~~\/~~~~\/~~~~\/~~~~ (Uniform ripples)
Rapid Load Change:   __/\_/\__________/\___ (Suppressed or distorted ripples -> "Missed Pulses")
```

### 2.1 Physics of Load Changes
When the mechanical load on the motor changes abruptly:
1.  **Slew Rate of Current**: The average motor current ($I_{\text{motor}}$) rises or falls rapidly. This steep DC incline/decline superimposed on the ripple signal can overwhelm simple high-pass filters, shifting the signal completely out of the comparator's hysteresis window.
2.  **Ripple Amplitude Damping**: At very high loads, the armature magnetic circuit may approach saturation, and the physical brush contact duration changes. This can significantly damp the high-frequency ripple amplitude, making the ripples too small to cross fixed analog comparator thresholds.
3.  **Slip and Sparking**: Under heavy loads, micro-slipping of the brushes or minor sparking (commutation arcing) creates high-amplitude, high-frequency spikes. These spikes mimic ripples, leading to "double-counting" if not filtered appropriately.

### 2.2 Starting and Stopping Phases
*   **Startup**: When a motor starts from a standstill, there is no Back-EMF. The starting current is extremely high ($I_{\text{start}} = \frac{V_{\text{in}}}{R_{\text{armature}}}$), and the rotor speed accelerates from 0 RPM. The initial ripples have extremely low frequencies and massive amplitudes, which can saturate the amplification stages of the AFE.
*   **Stopping/Coasting**: When power is cut, the current drops to zero, but the motor continues to spin due to momentum, generating a Back-EMF voltage. If the driver is placed in coasting (high-impedance) mode, no current flows, and thus ripple counting based on current sensing becomes impossible.

---

## 3. Hardware and Software Mitigation Strategies

To handle these dynamic anomalies, AN-Motor-01 suggests a combined hardware and software approach.

### 3.1 Hardware Mitigation
*   **Adaptive Hysteresis (Schmitt Trigger with Programmable Thresholds)**: Instead of a fixed hysteresis band, the comparator's reference levels are adapted. This can be achieved by using digital-to-analog converters (DACs) on the MCU to adjust the comparison thresholds dynamically based on the current average current.
*   **Active Damping and AGC**: An Automatic Gain Control (AGC) amplifier in the AFE ensures that when the ripple amplitude drops due to heavy load, the signal is amplified more, and when the amplitude is huge (at startup), the gain is reduced to prevent saturation.

### 3.2 Software Mitigation & Algorithms
Where hardware fails, intelligent software must reconstruct the true motion.

1.  **Plausibility Filters**: The software maintains a history of the last $N$ ripple intervals. If a detected ripple pulse arrives significantly earlier than expected (e.g., $< 50\%$ of the running average period), it is discarded as noise (double count prevention).
2.  **Extrapolation / "Ghost Pulse" Insertion**: If a ripple pulse does not arrive within an expected window (e.g., $> 150\%$ of the running average period), the software assumes a "missed pulse" occurred due to a transient. It artificially inserts a "ghost pulse" to keep the position tracking and speed estimation accurate.
3.  **State Machine Integration**: The software tracks the overall motor state (Standstill, Acceleration, Steady-State, Deceleration, Coasting). During phases where ripple detection is known to be unreliable (like the first 50ms of startup or during coasting), the estimator falls back to a motor-model-based speed estimate (derived from applied PWM duty cycle and voltage).

---

## 4. Architectural Alternatives for Software Filtering & Mitigation (Evaluating 3 Options)

We evaluate exactly three software structures to implement these mitigation algorithms on modern microcontrollers:

### Alternative A: ISR-Based Simple Hysteresis Counting
*   **Description**: The output of a basic hardware comparator is tied to an external interrupt pin. Every rising edge triggers an Interrupt Service Routine (ISR) that increments the ripple counter. No software filtering is performed, other than a simple blanking time (debounce) inside the ISR.
*   **Advantages**: Minimal CPU footprint, extremely simple to write.
*   **Disadvantages**: Discarded. Highly vulnerable to missed pulses and double counts during load transients. Offers no way to insert ghost pulses or apply advanced statistical plausibility filters.

### Alternative B: Direct ADC Sampling + Dual-Core/Interrupt DSP Pipeline [SELECTED]
*   **Description**: High-speed ADC samples are processed in a periodic DSP block. A digital band-pass filter isolates the ripple, and a software-defined comparator with **dynamic, adaptive hysteresis** detects peaks. A tracking state machine applies timing plausibility checks (comparing the new interval to a rolling median of past intervals) and dynamically inserts ghost pulses if an expected peak is missing.
*   **Advantages**: Extremely robust. By having full access to the digitized current waveform, the software can adapt its thresholds in real-time based on the signal's root-mean-square (RMS) value. Plausibility and extrapolation algorithms are easily implemented.
*   **Disadvantages**: Higher RAM and CPU consumption (well within the capabilities of the RP2040 and STM32 Cortex-M4).

### Alternative C: Frequency-Domain (FFT) Analysis
*   **Description**: Collect a window of ADC samples (e.g., 256 or 512 points) and compute a Fast Fourier Transform (FFT) in software. Identify the speed by finding the dominant peak in the frequency spectrum corresponding to the commutator frequency.
*   **Advantages**: Extremely robust against random noise and individual missed pulses, as it analyzes the signal globally in the frequency domain.
*   **Disadvantages**: Discarded. Extremely high computational complexity and high latency. Cannot track absolute position (individual ripple counting) in real-time, only average speed. Unsuitable for fast closed-loop control or step responses.
