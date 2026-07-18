# Jackw01 Ripplecounter Repository Technical Summary & Analysis

## 1. Executive Summary
The `jackw01/ripplecounter` repository provides a complete open-source hardware and software solution for sensorless position and speed control of brushed DC motors using commutator current ripple counting. By analyzing and filtering the high-frequency current ripples produced during commutation, the system can determine a motor's rotation, speed, and position without needing physical encoders. The design is optimized for low-cost, flexible implementations and is demonstrated on a custom Raspberry Pi Pico-based development board.

---

## 2. Hardware Signal Conditioning Circuit Design

The signal conditioning circuit filters and amplifies the high-frequency commutator ripple current while rejecting the DC motor load, low-frequency transients, and high-frequency PWM carrier noise. It is based on Texas Instruments TIDA-01421 and Microchip AN3049.

```
                  +-------------------------+
                  | INA181 Current-Sense    |
                  | Amplifier (U202)        |
                  +-------------------------+
                               |
                               v
                  +-------------------------+
                  | LM324 Active Band-Pass  |
                  | Filter (U203B)          |
                  +-------------------------+
                               |
                               v
                  +-------------------------+
                  | LM324 Baseline          |
                  | Subtractor (U203C)      |
                  +-------------------------+
                               |
                               v
                  +-------------------------+
                  | LM324 Comparator with   |
                  | Hysteresis (U203D)      |
                  +-------------------------+
                               |
                               v (TP215)
                    Clean Square Wave Output
```

### 2.1 First-Stage: Current Sense Amplification (U202)
*   **Component**: Texas Instruments INA181 bidirectional current-sense amplifier.
*   **Function**: Measures the voltage drop across a low-value series shunt resistor (R201/R202). The INA181 provides a fixed gain (typically 20, 50, 100, or 200 V/V) and low offset, producing an amplified version of the raw current waveform including AC ripples.

### 2.2 Second-Stage: Active Band-Pass Filtering (U203B)
*   **Component**: LM324 general-purpose quad op-amp (U203B).
*   **Function**: Configured as an active band-pass filter to isolate the fundamental current ripple frequencies (typically 100 Hz to 2 kHz) corresponding to the motor's operating speed. It works in conjunction with passive RC low-pass filters to eliminate high-frequency PWM carrier noise (e.g., 20 kHz) and low-frequency load oscillations.

### 2.3 Third-Stage: Baseline / Slow-Change Subtraction (U203C)
*   **Component**: LM324 op-amp (U203C).
*   **Function**: Subtracts slow, large-amplitude baseline variations from the signal, such as the motor's inrush current spike during startup or deceleration transients. This ensures that the AC ripple remains centered and doesn't saturate or drift away from the comparator thresholds.

### 2.4 Fourth-Stage: Comparator with Hysteresis (U203D)
*   **Component**: LM324 op-amp (U203D).
*   **Function**: Acts as a hardware Schmitt trigger (comparator with hysteresis) to transform the filtered sinusoidal-like ripple signal into a clean square wave at TP215. The hysteresis is critical to prevent false double-counts on residual noise or ripple irregularities.

---

## 3. Design Modification & Customization Guide

To adapt this circuit for motors of different physical sizes or electrical characteristics (such as the refitted Märklin locomotive motors), the following component-level guidelines must be applied:

1.  **Current Sense Resistor (R201/R202)**:
    *   Must be selected based on power dissipation rating: $P = I_{\text{rms}}^2 \cdot R_{\text{sense}}$.
    *   For larger motors or motors with high inrush and stall currents, choose a resistor with high pulse-load capability.
    *   To prevent clipping at the output of the current sense amplifier, select the resistor and INA181 gain such that:

    $$I_{\text{motor\_max}} \times R_{\text{sense}} \times \text{Gain} < \frac{V_{\text{cc}}}{2}$$

2.  **Filter Gain & Cutoff Frequencies**:
    *   **Band-pass Filter Gain**: Configured via resistors R213 and R214. It should be tuned experimentally to prevent signal clipping at TP211 under maximum ripple amplitude.
    *   **Differential Amplifier Gain**: Configured via resistors R216 to R219 to produce a clean output at TP215. If R217/R218 are changed by more than an order of magnitude, capacitor C214 should be updated proportionally.
    *   **Cutoff Frequencies**: Must be tuned according to the motor's operating speed range. It is recommended that the cutoff frequency of the RC low-pass filter (R215/C213) is at least the motor's ripple frequency at maximum speed, and the active band-pass filter's cutoff frequency (R214/C210) is at least twice that frequency.

---

## 4. Firmware Implementation and Software Debouncing

The `jackw01/ripplecounter` firmware is written using the Raspberry Pi Pico C SDK.

### 4.1 Timer and Interrupt Setup
*   **PWM Generation**: Drives the motor via high-frequency PWM on PinMotorA and PinMotorB.
*   **Interrupt Input**: The comparator output from the hardware AFE (TP215) is connected to a digital GPIO pin (PinCounterIn / GPIO 16) configured to trigger an interrupt on the falling edge (`GPIO_IRQ_EDGE_FALL`).

### 4.2 Software Debouncing Logic
To mitigate false triggers caused by mechanical brush sparking and ripple irregularities, the ISR (`encoder_gpio_callback`) implements a timing-based debouncing filter:

*   **Inactivity Threshold (`InactivityThresholdUs` = 20,000 µs)**: If the elapsed time since the last pulse is greater than this threshold, it is treated as the first pulse after a period of standstill or inactivity and is counted immediately.
*   **Debounce Threshold (`DebounceThresholdPercent` = 60%)**: If the elapsed time since the last pulse (`dt`) is less than 60% of the previous valid pulse interval (`ripplecounter_last_dt`), it is discarded as a noise spike (double count). Otherwise, it is counted as a valid commutation pulse.

```cpp
void encoder_gpio_callback(uint gpio, uint32_t events) {
  if (gpio == PinCounterIn) {
    uint32_t time = time_us_32();
    uint32_t dt = time - ripplecounter_last_time;

    if (!EnableDebouncing ||
        ripplecounter_last_dt > InactivityThresholdUs ||
        dt * 100 > ripplecounter_last_dt * DebounceThresholdPercent) {

      if (ripplecounter_direction) {
        ripplecounter_position--;
      } else {
        ripplecounter_position++;
      }
    }
    ripplecounter_last_time = time;
    ripplecounter_last_dt = dt;
  }
}
```

---

## 5. Architectural Evaluation (Evaluating 3 Alternatives)

Per project design standards, we evaluate exactly three alternative architectures for the integration of the signal conditioning and ripple counting firmware:

### Alternative A: Hardware-First Filter with Interrupt Counting [SELECTED by jackw01]
*   **Description**: This architecture relies on a highly tuned multi-stage analog active filter chain and baseline subtraction to output a clean square wave. The microcontroller's role is restricted to a simple edge-triggered ISR with timing debouncing.
*   **Advantages**: Very low CPU utilization on the microcontroller. It does not require continuous high-frequency sampling or complex DSP calculations in software.
*   **Disadvantages**: Requires a large analog PCB footprint with multiple op-amps, resistors, and capacitors. It is difficult to adjust dynamically for different motor models or speed bands without physical hardware changes.

### Alternative B: Direct ADC Sampling with Software DSP (IIR + Software Peak Detection)
*   **Description**: The motor shunt current is sampled directly via a high-speed ADC (e.g., at 50 kHz) utilizing DMA. A software 2nd-order IIR band-pass filter and a software-defined comparator with adaptive hysteresis process the samples to count ripples.
*   **Advantages**: Highly flexible. Filter parameters and hysteresis bands can be adjusted dynamically in software to track the motor's actual speed or adapt to a completely different motor size.
*   **Disadvantages**: Discarded. Imposes significant computational overhead on the microcontroller's CPU. For dual-motor systems or low-end MCUs, this could consume too many clock cycles and introduce jitter into the closed-loop motor control.

### Alternative C: Frequency-Domain Spectral Analysis (FFT)
*   **Description**: Collects windows of current samples and executes a Fast Fourier Transform (FFT) on the microcontroller to determine the dominant frequency component corresponding to the motor's rotation.
*   **Advantages**: Highly immune to individual missed pulses or brush spark noise spikes, since it determines the average frequency of the entire signal window rather than counting discrete edges.
*   **Disadvantages**: Discarded. Introduces significant latency, making it unsuitable for real-time closed-loop position control. It cannot track individual commutation steps (absolute position tracking is impossible), and the computational requirements are excessively high.
