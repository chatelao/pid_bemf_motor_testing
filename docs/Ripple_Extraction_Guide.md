# Commutator Ripple Extraction Practical Implementation Guide

This guide details the practical realization of high-speed current ripple extraction for our Märklin motor calibration tool, mapping the theories from TI SLVA303 and Infineon AN-Motor-01 directly to our microcontroller architectures: **Seeed Studio XIAO RP2040** and **ST Nucleo-F446RE/G431RB**.

---

## 1. System Engineering Constraints & Parameters

To design the digital filters and peak detector, we establish the motor and sampling parameters:

| Parameter | Value / Range | Description |
|---|---|---|
| **Motor Pole Count ($k$)** | 3 or 5 | Typically 3-pole or 5-pole (Märklin stock/upgrade rotors) |
| **Operational Speed ($N$)** | 1,200 to 12,000 RPM | 20 to 200 Hz rotor mechanical frequency |
| **Ripple Frequency ($f_{\text{ripple}}$)**| 100 Hz to 1,000 Hz | Fundamental commutation ripple band |
| **PWM Carrier Frequency** | 20,000 Hz | High-frequency switching noise from the H-bridge |
| **ADC Sampling Frequency ($f_s$)** | 50,000 Hz (50 kHz) | Fulfills Nyquist and allows oversampling for filtering |

---

## 2. High-Speed ADC Sampling Architectures

Continuous polling of the ADC at 50 kHz blocks the main application thread. Therefore, platform-specific hardware features must be utilized to fill a buffer in the background.

```
+--------------------+      +-----------------------+      +--------------------+
| Hardware Trigger   | ---> | ADC Conversion (A2)   | ---> | DMA Circular Buffer|
| (Timer / PWM Sync) |      | (Shunt Current)       |      | (Memory Allocation)|
+--------------------+      +-----------------------+      +--------------------+
```

### 2.1 RP2040 (Seeed Studio XIAO RP2040) Implementation
*   **Hardware Setup**: Configured with the RP2040 ADC FIFO.
*   **DMA Transfer**: A dedicated DMA channel moves completed samples from the ADC FIFO into a circular buffer in SRAM.
*   **Interrupt Handling**: An interrupt is fired when the buffer is half-full (Ping-Pong buffering) or fully wrapped. Core 1 of the RP2040 handles the digital filtering, preventing jitter on Core 0 which executes the main speed control loop and CLI stream.

### 2.2 STM32 (Nucleo-F446RE / G431RB) Implementation
*   **Hardware Setup**: A hardware timer (e.g., `TIM2`) is configured to trigger ADC conversions at exactly 50 kHz.
*   **DMA Transfer**: The ADC is linked to a DMA channel (e.g., DMA2 Stream 0 on F446) operating in circular mode.
*   **Interrupt Handling**: The DMA half-complete (`HT`) and transfer-complete (`TC`) interrupts are used to process data blocks without stopping the sampling pipeline.

---

## 3. Digital Band-Pass Filter Design

To isolate the 100 Hz to 1,000 Hz ripple band, we design a 2nd-order Infinite Impulse Response (IIR) Band-Pass Filter.

### 3.1 Mathematical Form
A standard 2nd-order IIR biquad filter is represented in the time domain by:

$$y[n] = b_0 \cdot x[n] + b_1 \cdot x[n-1] + b_2 \cdot x[n-2] - a_1 \cdot y[n-1] - a_2 \cdot y[n-2]$$

### 3.2 Fixed-Point Optimization (Q15 / Q31 Arithmetic)
To avoid slow floating-point operations on low-end MCUs, we scale the filter coefficients by $2^{15}$ (or $2^{30}$) to perform integer math:

$$\text{Scaled\_Coef} = \text{round}(\text{Float\_Coef} \times 32768)$$

The output is then bit-shifted right to restore the scale:

$$y_{\text{fixed}}[n] = \left( b_{0,\text{int}} \cdot x[n] + b_{1,\text{int}} \cdot x[n-1] + b_{2,\text{int}} \cdot x[n-2] - a_{1,\text{int}} \cdot y[n-1] - a_{2,\text{int}} \cdot y[n-2] \right) \gg 15$$

*Note: On STM32, the **CMSIS-DSP** library provides optimized biquad functions (`arm_biquad_cascade_df1_fast_q15` or `arm_biquad_cascade_df1_f32`) utilizing the hardware FPU or SIMD instructions.*

---

## 4. Peak Detection and Hysteresis (Mitigating Non-Idealities)

Once filtered, the ripple signal must be evaluated to count individual commutation pulses.

```
       Signal Amplitude
             ^
             |       _--_            _--_
  Threshold  |------/----\----------/----\--------- (High Peak Threshold)
             |     /      \        /      \
       Zero  |----+--------+------+--------+-------> Time
             |   /          \    /          \
  Threshold  |--/------------\--/------------\----- (Low Valley Threshold)
             |                --              --
```

### 4.1 Adaptive Hysteresis Algorithm
To prevent false counts due to high-frequency noise or residual PWM carrier:
1.  **State Machine**: Maintain a state variable `RIPPLE_STATE` (either `VALLEY` or `PEAK`).
2.  **Adaptive Thresholds**: Calculate the moving peak-to-peak amplitude of the filtered signal:

    $$V_{\text{pp}} = V_{\text{peak}} - V_{\text{valley}}$$

    Set the detection thresholds dynamically:

    $$\text{Threshold}_{\text{High}} = V_{\text{mean}} + \beta \cdot V_{\text{pp}}$$

    $$\text{Threshold}_{\text{Low}} = V_{\text{mean}} - \beta \cdot V_{\text{pp}}$$

    Where $\beta \approx 0.25$ and $V_{\text{mean}}$ is the running DC average.
3.  **State Transitions**:
    *   Transition from `VALLEY` to `PEAK` occurs only when the signal crosses above $\text{Threshold}_{\text{High}}$. Increment ripple counter.
    *   Transition from `PEAK` to `VALLEY` occurs only when the signal drops below $\text{Threshold}_{\text{Low}}$.

### 4.2 Software Plausibility & Ghost Pulses (AN-Motor-01 Integration)
To handle severe load transients, we implement the following logic in the timer loop or Core 1:

```cpp
uint32_t current_time = micros();
uint32_t expected_interval = running_average_interval;

// 1. Double Count Prevention
if (detected_pulse_interval < (expected_interval * 0.6)) {
    // Too fast! Discard as noise spike (double count)
    return;
}

// 2. Missed Pulse Detection (Ghost Pulse Insertion)
if (current_time - last_pulse_time > (expected_interval * 1.6)) {
    // We missed a commutator ripple! Insert a ghost pulse
    ripple_count++;
    last_pulse_time = last_pulse_time + expected_interval; // Extrapolate timing
}
```

---

## 5. Architectural Comparison (Evaluating 3 Implementations)

We evaluate exactly three firmware options for integrating the digital filter, peak detector, and plausibility engine into the system:

### Alternative A: Polling on the Main Loop Thread
*   **Description**: Run the ADC conversion synchronously inside the main Arduino `loop()`, apply the IIR filter, and check thresholds.
*   **Advantages**: Very easy to write; no interrupts or multicore synchronization needed.
*   **Disadvantages**: Discarded. The main loop also handles the slow Serial command parser, LED state blinks, and other tasks. Any delay in printing to the serial terminal will cause sampling gaps, leading to severe missed pulses.

### Alternative B: Timer Interrupt Service Routine (ISR) processing [SELECTED for STM32]
*   **Description**: The 50 kHz timer interrupt triggers the ADC conversion. The resulting sample is processed directly within the high-priority ISR. The filtering and state transition are done in less than 5 microseconds, and the global `ripple_count` is updated.
*   **Advantages**: Guaranteed deterministic sampling. Low latency. Highly portable across Single-Core Cortex-M4 (STM32) architectures.
*   **Disadvantages**: Consumes a significant fraction of CPU time if the ISR math is not highly optimized (requires fixed-point or hardware FPU).

### Alternative C: Asynchronous Multi-Core Processing [SELECTED for RP2040]
*   **Description**: Core 0 of the RP2040 handles the closed-loop PID control and Serial CLI stream. Core 1 runs an independent, non-blocking execution block that continuously reads blocks from the DMA circular buffer, applies the IIR filtering, and performs peak detection.
*   **Advantages**: The absolute best approach for dual-core MCUs. Isolates time-critical signal processing from slow serial communications and user interface interaction.
*   **Disadvantages**: Requires thread-safe sharing of variables (such as `ripple_count`) between Core 0 and Core 1 using hardware mutexes (`spin_lock`).
