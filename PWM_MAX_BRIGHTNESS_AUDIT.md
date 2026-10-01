# PWM Max Brightness Audit: Root Causes & Solutions for BDR6133 Output Attenuation

## 1. Overview & Problem Description

During execution of the BEMF characterization ramp test (`examples/BEMF_Ramp_Test/BEMF_Ramp_Test.ino`), an open-loop PWM sweep is applied across an E3 frequency series:
$$\text{Frequencies: } 47\text{ Hz}, 100\text{ Hz}, 220\text{ Hz}, 470\text{ Hz}, 1\text{ kHz}, 2.2\text{ kHz}, 4.7\text{ kHz}, 10\text{ kHz}, 22\text{ kHz}, 47\text{ kHz}, 100\text{ kHz}$$

At higher frequencies (particularly $22\text{ kHz}$, $47\text{ kHz}$, and $100\text{ kHz}$), indicator LEDs connected across the motor output terminals of the BDR6133 H-bridge IC exhibit significantly reduced brightness compared to lower frequencies ($47\text{ Hz}$ to $1\text{ kHz}$), even when driven at maximum commanded PWM duty cycle (`current_pwm = 255`).

This document provides a technical audit analyzing the root causes of this attenuation and details actionable firmware and hardware solutions.

---

## 2. Categorized Root Causes

### A. BDR6133 H-Bridge Driver Switching Delays & Dead-Time Attenuation

1. **Propagation Delays & Finite Slew Rates ($t_{d(on)}, t_{d(off)}, t_r, t_f$)**:
   - The MOSFET power switches inside the BDR6133 driver require a non-zero time to transition between fully OFF ($R_{DS(off)} \approx \infty$) and fully ON ($R_{DS(on)}$). Typical driver gate turn-on and turn-off propagation delays range from $100\text{ ns}$ to $500\text{ ns}$.
   - At $47\text{ Hz}$, the total PWM period $T$ is $\approx 21.27\text{ ms}$. A $500\text{ ns}$ delay represents only $0.0023\%$ of the cycle.
   - At $100\text{ kHz}$, the total PWM period $T$ is $10\text{ }\mu\text{s}$. A $500\text{ ns}$ turn-on delay represents $5\%$ of the total period, directly truncating the output pulse width.

2. **MOSFET Gate Charge ($Q_g$) Dynamics**:
   - The driver's internal gate charge circuit must supply current to charge the MOSFET gate capacitance ($C_{iss}$). At high switching speeds, finite internal gate drive current prevents instantaneous state changes, transforming ideal rectangular PWM pulses into trapezoidal or rounded waveforms with lower RMS voltage.

3. **Anti-Shoot-Through Dead Time**:
   - Integrated H-bridge drivers introduce hardware dead time ($t_{\text{dead}}$) to prevent shoot-through currents when switching bridge legs. This dead time delays the conduction onset of the active output on every edge transition. As switching frequency increases, the cumulative dead time per second increases linearly, reducing the net average output voltage delivered to the load.

---

### B. Microcontroller PWM Hardware & Framework Abstraction (RP2040 / `earlephilhower`)

1. **PWM Counter Wrap ($TOP$) Resolution Scaling**:
   - The RP2040 PWM hardware operates via a system clock ($125\text{ MHz}$) divided by a prescaler (`DIV`) and wrapped at a counter top value (`TOP`).
   - The PWM frequency is determined by:
     $$f_{\text{PWM}} = \frac{f_{\text{sys}}}{\text{DIV} \times (\text{TOP} + 1)}$$
   - At $100\text{ kHz}$ with $\text{DIV} = 1.0$, $\text{TOP} + 1 = \frac{125,000,000}{100,000} = 1250$ counts.
   - Arduino's `analogWrite(pin, val)` expects `val` in the range $0\text{--}255$.

2. **Arduino Framework Mapping & Saturation Issues**:
   - In some core versions, `analogWrite(pin, 255)` maps $255$ to the counter wrap value. However, if `analogWriteFreq()` updates the slice frequency without scaling the input range correctly, or if `analogWrite` assumes an 8-bit wrap ($255$), severe scaling mismatch occurs:
     - If `TOP = 1250` and `analogWrite` sets compare counter to $255$, the actual duty cycle becomes $\frac{255}{1250} = 20.4\%$!
     - Even when mapped properly, many Arduino core implementations set the maximum compare value to `TOP - 1` rather than forcing a static HIGH level. At high frequencies, this leaves a $1$-clock-cycle OFF pulse at the end of every counter period (e.g., $8\text{ ns}$ pulse every $10\text{ }\mu\text{s}$ at $100\text{ kHz}$), triggering repeated gate discharging cycles in the BDR6133.

---

### C. Output Load, Parasitic Capacitance & Filtering Effects

1. **Parasitic RC Low-Pass Filtering**:
   - High-side and low-side PCB traces, motor wiring inductance, and parasitic output capacitance ($C_{oss}$ of MOSFETs + line capacitance + snubber capacitors) act as a low-pass filter.
   - At high frequencies, the fundamental frequency and higher harmonics of the PWM square wave are heavily attenuated. The output pulse amplitude fails to reach the full rail voltage ($V_{CC}$) before the PWM off-cycle begins.

2. **Inductive Flyback & Snubber Discharging**:
   - Inductive motor loads and parallel snubber networks recirculate current through free-wheeling diodes during off-states. At $100\text{ kHz}$, the interval between pulses ($10\text{ }\mu\text{s}$) is comparable to the $L/R$ time constant of the motor winding, altering the voltage profile across the output terminals.

---

### D. Non-Linear LED Characteristics ($V_f$ Threshold)

1. **Forward Voltage ($V_f$) Cut-off Effect**:
   - LEDs are non-linear, exponential devices governed by the Shockley diode equation:
     $$I_D = I_S \left( e^{\frac{V_D}{n V_T}} - 1 \right)$$
   - Standard red/green indicator LEDs require $V_D > V_f \approx 1.8\text{V} \text{--} 2.2\text{V}$ to conduct noticeable current.
   - When switching losses or RC rise-time rounding cause the output peak voltage to drop from $5.0\text{V}$ to near $V_f$, diode current drops exponentially rather than linearly, leading to a dramatic drop in optical output power.

2. **Talbot-Plateau Law & Human Visual Perception**:
   - While the human visual system integrates light pulses above $\sim 60\text{ Hz}$ according to the Talbot-Plateau law, it responds to the *mean optical flux*. Because LED current drops exponentially when pulse shapes degrade into trapezoids, the mean optical flux plummets far faster than the nominal voltage duty cycle.

---

## 3. Recommended Solutions & Mitigation Strategies

```
+-----------------------------------------------------------------------------------+
|                                Mitigation Matrix                                  |
+--------------------------------------------------+--------------------------------+
| Firmware Optimizations                           | Hardware & Circuit Design      |
+--------------------------------------------------+--------------------------------+
| 1. Direct Pico-SDK PWM Register Control          | 1. Separate Status LEDs        |
| 2. Dynamic Frequency Duty Cycle Compensation     | 2. Active LED Gate Drivers     |
| 3. Upper Frequency Boundary Capping (<=20-25 kHz)| 3. Low-ESR Decoupling Capacitors|
+--------------------------------------------------+--------------------------------+
```

### Solution A: Firmware & Software Enhancements

1. **Direct Hardware PWM Register Control (RP2040 Pico-SDK)**:
   - Replace standard `analogWrite` / `analogWriteFreq` with explicit RP2040 hardware PWM calls (`hardware/pwm.h`).
   - Guarantee true 100% duty cycle saturation by disabling the PWM slice or setting the compare counter to `TOP + 1` when duty cycle is 255:
     ```cpp
     #include "hardware/pwm.h"

     void setPwmDutyDirect(uint pin, uint16_t duty_8bit, uint32_t freq_hz) {
       uint slice_num = pwm_gpio_to_slice_num(pin);
       uint channel   = pwm_gpio_to_channel(pin);

       if (duty_8bit == 255) {
         // Force continuous HIGH output without switching pulses
         pwm_set_chan_level(slice_num, channel, 65535);
         return;
       }

       uint32_t clock_freq = 125000000;
       uint32_t top = (clock_freq / freq_hz) - 1;
       pwm_set_wrap(slice_num, top);
       uint32_t level = (duty_8bit * (top + 1)) / 255;
       pwm_set_chan_level(slice_num, channel, level);
     }
     ```

2. **Empirical Frequency-Dependent Duty Compensation Curve**:
   - Implement a compensation lookup table or polynomial function that inflates the commanded duty cycle at higher frequencies to offset BDR6133 switching delays:
     $$D_{\text{compensated}} = D_{\text{commanded}} + \Delta D(f_{\text{PWM}})$$

3. **Cap Maximum PWM Frequency for Motor Operation**:
   - Restrict the operational PWM frequency range for BDR6133 motor control to $20\text{ kHz} \text{--} 25\text{ kHz}$. Frequencies above $25\text{ kHz}$ yield diminishing acoustic benefits while drastically increasing switching losses, driver thermal dissipation, and pulse distortion.

---

### Solution B: Hardware & Circuit Design Improvements

1. **Isolate Status LEDs from Motor Power Output Lines**:
   - Drive status indicator LEDs directly from microcontroller GPIO pins (e.g. `PIN_LED1` / `PIN_LED2`) using 3.3V logic signals, rather than tapping directly into the BDR6133 motor power outputs (`OutA` / `OutB`).

2. **Active Buffer / Transistor Drive for Output Diagnostics**:
   - If monitoring output terminals is required, use a high-speed small-signal NPN/PNP transistor or MOSFET buffer stage (e.g., 2BS/2AS) with low input capacitance to drive the indicator LEDs without loading the BDR6133 or suffering from $V_f$ threshold attenuation.

3. **Power Driver Decoupling & Rail Stabilization**:
   - Place a low-ESR ceramic decoupling capacitor ($100\text{ nF}$ in parallel with $10\text{ }\mu\text{F}$ tantalum) immediately adjacent to the BDR6133 $V_{CC}$ and $V_{MOTOR}$ supply pins to prevent supply voltage drop during high-frequency gate charging spikes.

---

## 4. Verification & Oscilloscope Diagnostic Plan

To confirm the root cause empirically on physical hardware:

1. **Oscilloscope Waveform Capture**:
   - Connect oscilloscope channel 1 to `PIN_PWM_A` (MCU output) and channel 2 to `BDR6133_OutA` (Motor terminal).
   - Sweep frequency from $47\text{ Hz}$ to $100\text{ kHz}$ at $100\%$ duty cycle (`255`).
   - Measure:
     - Rise time ($t_r$) and fall time ($t_f$) on `OutA`.
     - Output voltage amplitude ($V_{\text{peak-to-peak}}$).
     - Actual pulse high time vs commanded period ($T$).

2. **Optical Intensity Validation**:
   - Measure LED light output using a photodiode or optical sensor to establish the $f_{\text{PWM}}$ vs Optical Output curve before and after firmware/hardware mitigation.
