# Texas Instruments Application Report SLVA303 Summary

## Title
**Sensorless Speed Control of Brushed DC Motors Using Current Ripple**

---

## 1. Executive Summary
TI SLVA303 explores the theory and implementation of sensorless speed measurement and control for brushed DC motors by detecting and analyzing current ripples. In brushed DC motors, commutator actions cause periodic fluctuations (ripples) in the armature current. The frequency of these current ripples is directly proportional to the motor's rotational speed. By isolating and counting these fluctuations, developers can achieve closed-loop speed control and position tracking without external encoders, thereby saving space, reducing mechanical complexity, and lowering total system bill-of-materials (BOM) costs.

---

## 2. Mathematical Fundamentals

### 2.1 Motor Speed and Current Ripple Frequency
For a brushed DC motor, the relationship between the mechanical speed $N$ (in revolutions per minute, RPM) and the electrical current ripple frequency $f_{\text{ripple}}$ (in Hz) is given by:

$$f_{\text{ripple}} = N \times \frac{k}{60}$$

Where:
*   $f_{\text{ripple}}$ is the fundamental ripple frequency (Hz).
*   $N$ is the motor speed in RPM.
*   $k$ is the number of commutator segments (bars) on the armature.

Alternatively, expressing mechanical angular velocity $\omega$ in radians per second (rad/s):

$$\omega = \frac{2\pi \cdot f_{\text{ripple}}}{k}$$

This linear relationship means that if the commutator has $k$ segments, there are exactly $k$ current ripples per physical motor revolution (or $2k$ half-waves if both rising and falling slopes are evaluated).

### 2.2 Mechanism of Current Ripple Generation
The current ripple is primarily a result of the commutation process:
1.  **Inductance Changes**: As the rotor turns, the brushes slide from one commutator segment to the next. During this transition, a coil winding is temporarily short-circuited by the brush. This action changes the equivalent resistance and inductance of the armature.
2.  **Back-EMF Variations**: The induced back-electromotive force (Back-EMF) is not perfectly constant but varies slightly depending on the rotor angle. When brushes switch windings, a small dip or spike in the Back-EMF occurs, causing a corresponding variation in the motor current ($I_{\text{motor}} = \frac{V_{\text{in}} - V_{\text{BEMF}}}{R_{\text{armature}}}$).

---

## 3. Filter Topologies for Ripple Extraction

The raw current signal through the motor shunt resistor contains multiple unwanted components:
*   **DC Component**: The average current drawing load (can vary slowly from milliamps to several amperes).
*   **PWM Carrier Noise**: High-frequency switching noise (typically 15 kHz to 25 kHz) stemming from the H-bridge driver.
*   **Random Noise and Brush Arcing**: High-frequency spikes due to mechanical brush contact discontinuity.

To isolate the fundamental current ripple (typically in the range of 100 Hz to 2 kHz), SLVA303 outlines specific analog and digital filtering topologies.

```
[Shunt Resistor] ---> [High-Pass Filter] ---> [Band-Pass Filter] ---> [Amplification] ---> [Comparator/ADC]
```

### 3.1 First-Stage: High-Pass Filtering (DC Block)
To remove the massive DC offset of the average motor current, a passive or active high-pass filter (HPF) is utilized. This stage retains only the AC fluctuations.
*   **Cut-off frequency ($f_{c,\text{low}}$)**: Positioned well below the lowest expected ripple frequency (e.g., 30 Hz to 100 Hz).

### 3.2 Second-Stage: Band-Pass Filtering
An active band-pass filter (BPF) is used to isolate the target frequency band where the commutator ripples reside, while simultaneously rejecting low-frequency load transients and high-frequency PWM switching harmonics.
*   **Butterworth or Chebyshev Config**: Typically implemented as a 2nd-order or 4th-order active filter to provide a steep roll-off.
*   **Variable/Adaptive Band-Pass**: Since the ripple frequency shifts dynamically with motor speed, a fixed band-pass filter must be wide enough to encompass the entire operational range, or an adaptive filter (e.g., switched-capacitor filter or software-defined digital filter) must be employed to track the center frequency.

### 3.3 Third-Stage: Amplification and Schmitt Trigger
*   **Preamplifier**: Amplifies the filtered AC ripple (often only a few millivolts across a shunt) to a voltage level suitable for MCU analog-to-digital converters (ADCs) or comparator inputs.
*   **Schmitt Trigger/Comparator**: Converts the sinusoidal-like ripple signal into a clean square wave. The hysteresis of the Schmitt trigger is crucial to prevent false triggering from residual high-frequency noise.

---

## 4. Analog Front-End (AFE) Design Considerations

### 4.1 Current Sensing Shunt
*   Placed in series with the low-side of the H-bridge or directly in the motor return path.
*   Must have low resistance ($10\text{ m}\Omega$ to $500\text{ m}\Omega$) to minimize power dissipation while producing a detectable voltage drop ($V_{\text{shunt}} = I_{\text{motor}} \times R_{\text{shunt}}$).

### 4.2 Dynamic Range & Hysteresis
*   As motor load increases, the amplitude of the ripple current often increases as well. Conversely, at light loads, the ripple amplitude can shrink significantly.
*   **Automatic Gain Control (AGC)**: Recommended for wide-speed or wide-load range systems to dynamically scale the gain of the AFE.
*   **Adaptive Hysteresis**: Prevents noise triggering at high loads while ensuring low-amplitude ripples are still counted at light loads.

---

## 5. Architectural Implementation Alternatives (Evaluating 3 Options)

Per our project standards, we analyze exactly three alternative approaches to realizing the AFE and filtering for our Märklin motor calibration tool:

### Alternative A: Pure Analog Hardware Processing (Discrete Op-Amps + Hardware Comparator)
*   **Description**: This approach uses a multi-stage active analog filter chain (high-pass, band-pass, gain) followed by an integrated hardware Schmitt trigger. The output is fed directly to an MCU digital input pin configured as an external interrupt or timer input capture pin.
*   **Advantages**: Zero MCU processing overhead; the microcontroller simply counts pulses using a hardware counter. Very fast reaction time.
*   **Disadvantages**: Discarded. Rigid and cannot adapt to different motors or varying speed ranges without physically swapping resistors and capacitors. Highly sensitive to components tolerance.

### Alternative B: Mixed-Signal Software/Hardware Processing (Passive Analog Filter + High-Speed ADC + Digital IIR BPF) [SELECTED]
*   **Description**: The current shunt voltage is routed through a simple passive high-pass/antialiasing filter and directly sampled by a high-speed ADC (using DMA to avoid blocking). The filtering (band-pass), amplification, and hysteresis-based peak detection are done entirely in software (DSP).
*   **Advantages**: Maximum flexibility. Filter parameters (e.g., center frequency, Q-factor, hysteresis thresholds) can be dynamically adjusted in software based on the estimated motor speed. No complex hardware tuning is required.
*   **Disadvantages**: Requires higher MCU processing power (mitigated on the RP2040 by offloading DSP processing to Core 1 or using STM32's fast hardware FPUs).

### Alternative C: Integrated SoC/Dedicated Ripple Counting ICs
*   **Description**: Utilize a dedicated commercial motor diagnostics or ripple counting IC (such as specialized automotive bridge-drivers with integrated ripple counters).
*   **Advantages**: Offloads all sensing and processing tasks to a specialized, highly robust external chip.
*   **Disadvantages**: Discarded. Greatly increases BOM cost and hardware footprint; reduces portability and violates our constraint of using standard, readily available breakout boards (like the BDR-6133).
