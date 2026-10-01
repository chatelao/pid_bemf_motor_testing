# DESIGN_CALIBRATION - Parameter Calibration & Exploration Software Design

## Introduction
This document details the technical design and software architecture for the Parameter Calibration & Exploration framework, derived from [CONCEPT_CALIBRATION.md](CONCEPT_CALIBRATION.md). The software framework is built upon the foundation of `examples/Minimal_PWM_BDR6133_Fixed_Oszi` to systematically sweep, measure, and analyze physical motor behavior for Märklin locomotives refitted with permanent magnets and driven by the BDR-6133 H-bridge IC.

---

## Technical Architecture & State Machine

The calibration engine operates as a non-blocking state machine running on the MCU, executing multidimensional parameter sweeps while outputting high-speed burst-sampled telemetry over USB Serial.

```text
       +-----------------------------------------------------------------------+
       |                         STATE_IDLE / INIT                             |
       |  Initialize Hardware (PWM, ADC, GPIOs, Oscilloscope Triggers, Serial) |
       +-----------------------------------------------------------------------+
                                           |
                                           v
       +-----------------------------------------------------------------------+
       |                         STATE_LOAD_NEXT_SWEEP                         |
       | Fetch next parameter tuple (Freq, Gap, ADC_Speed, KickPWM, KickDur)    |
       +-----------------------------------------------------------------------+
                                           |
                                           v
       +-----------------------------------------------------------------------+
       |                         STATE_KICKSTART                               |
       | Apply Kickstart PWM Duty Cycle (InA/InB) for Kickstart Duration        |
       +-----------------------------------------------------------------------+
                                           |
                                           v
       +-----------------------------------------------------------------------+
       |                         STATE_ACTIVE_DRIVE                            |
       | Drive Motor at Target PWM Duty Cycle until Interruption Interval      |
       +-----------------------------------------------------------------------+
                                           |
                                           v
       +-----------------------------------------------------------------------+
       |                         STATE_MEASUREMENT_GAP                         |
       | 1. Assert Oszi Triggers (D9/D10 LOW)                                  |
       | 2. Float Motor H-Bridge (D7=0, D8=0)                                  |
       | 3. High-Speed Burst ADC Capture (A0, A1, A2 -> Active RAM Buffer)     |
       | 4. De-assert Oszi Triggers (D9/D10 HIGH)                              |
       +-----------------------------------------------------------------------+
                                           |
                                           v
       +-----------------------------------------------------------------------+
       |                         STATE_FLUSH_TELEMETRY                         |
       | Stream RAM Buffer contents over USB Serial (115200 / 921600 Baud)     |
       | during regular motor PWM drive phase                                  |
       +-----------------------------------------------------------------------+
                                           |
                                           v
                        [More Sweep Steps Remaining?]
                         /                         \
                       YES                          NO
                       /                              \
                      v                                v
       [STATE_LOAD_NEXT_SWEEP]                   [STATE_SWEEP_COMPLETE]
```

---

## Target Hardware Pin Mapping & Peripheral Allocation

To maintain consistency, the calibration tool utilizes standard Arduino pin assignments and dedicated hardware trigger outputs for oscilloscope synchronization.

| Function | XIAO RP2040 | Description / Logic Level |
|---|---|---|
| **PWM InA** | `D7` (GPIO 3) | Motor Terminal A Drive Control |
| **PWM InB** | `D8` (GPIO 4) | Motor Terminal B Drive Control |
| **bEMF Sense A** | `A0` (GPIO 26) | Voltage feedback from Motor Terminal A |
| **bEMF Sense B** | `A1` (GPIO 27) | Voltage feedback from Motor Terminal B |
| **Shunt Current (`bEMF_C`)** | `A2` (GPIO 28) | Voltage drop across low-side shunt resistor |
| **Oszi Trigger 1** | `D9` (GPIO 1) | Active LOW measurement gap trigger |
| **Oszi Trigger 2** | `D10` (GPIO 2) | Active LOW measurement gap sync |
| **Status LED 1** | `PIN_LED_R` | System Status / Sweep Active Indicator |

---

## Detailed Calibration Parameters & Sweep Specifications

### 1. PWM Carrier Frequency Matrix
- **Configurable Range**: 5 kHz to 32 kHz (e.g., 5 kHz, 8 kHz, 16 kHz, 20 kHz, 25 kHz, 32 kHz).
- **MCU Realization**:
  - **RP2040**: Utilizes `analogWriteFreq(freq)` or native RP2040 hardware PWM slice registers.
- **Data Capture Focus**: Evaluates motor current ripple amplitude on shunt channel `A2` and acoustic humming frequency spectrum.

### 2. BEMF Interruption Window & Settling Delay
- **Configurable Range**:
  - **Gap Duration ($T_{\text{gap}}$)**: 500 µs to 5000 µs in 250 µs steps.
  - **Settling Delay ($T_{\text{settle}}$)**: 50 µs to 500 µs post-cutoff before storing valid bEMF ADC samples.
- **Microsecond Timing Mechanics**:
  ```text
  PWM Output:    |---|---|---|___|_____________________________|---|---|---|
                 ^ PWM Active    ^ Cutoff (D7=0, D8=0)         ^ Resume PWM
  Oszi Trigger:  ================\____________________________/===========
                                 |<------- Gap Duration (T_gap) ------->|
                                 |<-- T_settle -->|<-- Valid bEMF Capture -->|
  ADC Sampling:  .................................x...x...x...x...x.........
  ```

### 3. ADC Readout Clock & Sampling Speed
- **Configurable Range**:
  - **ADC Sampling Rate**: 50 kHz to 250 kHz per channel.
  - **Sample Depth ($N_{\text{samples}}$)**: 250 to 1000 samples per measurement window.
- **Hardware Optimization**: Configures MCU ADC prescalers (RP2040 ADC clock divider) to maximize acquisition bandwidth while keeping quantization noise within 12-bit bounds.

### 4. Kickstart Boost Parameters
- **Configurable Range**:
  - **Kickstart Duty Cycle**: 20% to 80% (PWM 51 to 204 out of 255).
  - **Kickstart Duration**: 10 ms to 150 ms in 10 ms steps.
- **Execution Strategy**: Applied at motion initiation from zero speed to overcome static friction ($T_{\text{stiction}}$) prior to dropping to target steady-state PWM.

---

## Telemetry Data Format & RAM Dual-Buffering Architecture

To maintain microsecond-level timing accuracy during measurement gaps without serial transmission bottlenecks, the calibration engine uses a **RAM Dual-Buffering Strategy**.

### RAM Buffer Allocation
Two static ping-pong buffers (`Buffer_A` and `Buffer_B`) store burst ADC readings in SRAM:
- `uint16_t bemfA_buf[MAX_SAMPLES]`
- `uint16_t bemfB_buf[MAX_SAMPLES]`
- `uint16_t bemfC_buf[MAX_SAMPLES]` (Shunt Current)
- `uint32_t time_buf[MAX_SAMPLES]` (Microsecond timestamps relative to gap start)

While `Buffer_A` is populated inside `STATE_MEASUREMENT_GAP`, `Buffer_B` is transmitted over USB Serial in `STATE_FLUSH_TELEMETRY` during active PWM drive.

### CSV Serial Telemetry Schema
Data is streamed at 115200 or 921600 baud using structured CSV records.

#### Sweep Header Record
```csv
# START_SWEEP: SweepID=12, Freq_Hz=20000, Gap_us=2000, ADC_Rate_kHz=100, KickPWM=180, KickDur_ms=30
```

#### Sample Data Record
```csv
SWEEP,SweepID,SampleIdx,Time_us,PWM_Dir_Speed,bEMF_A0,bEMF_A1,Current_Shunt_A2
```

#### Example Output Trace
```csv
# START_SWEEP: SweepID=1, Freq_Hz=20000, Gap_us=2000, ADC_Rate_kHz=100, KickPWM=180, KickDur_ms=30
SWEEP,1,0,50,128,2048,42,310
SWEEP,1,1,60,128,1980,42,285
SWEEP,1,2,70,128,1850,42,240
...
# END_SWEEP: SweepID=1, TotalSamples=150
```

---

## Major Technical Design Choices

In accordance with project architectural guidelines, every major technical choice evaluates exactly three alternatives (Selected vs Discarded).

### 1. Sweep Execution Engine Architecture
- **Alternative A: Automated Non-Blocking Finite State Machine (Selected)**: An autonomous software state machine iterates through sweep configurations, triggering measurement gaps, capturing RAM buffers, and streaming CSV logs without blocking system timers. Ensures high reproducibility and precise execution.
- **Alternative B: Delay-Based Synchronous Loop**: Executes sweeps using blocking calls like `delayMicroseconds()` and blocking serial output. Easy to implement, but serial blocking degrades measurement timing and introduces unmeasurable loop jitter.
- **Alternative C: Task-Based FreeRTOS Execution Engine**: Uses an RTOS with separate tasks for sweep control, ADC acquisition, and telemetry streaming. Highly modular, but adds significant runtime overhead and reduces platform portability across bare-metal Arduino cores.

### 2. High-Speed Burst ADC Capture Realization
- **Alternative A: Direct Register/Polling Burst Acquisition into SRAM (Selected)**: The MCU directly polls hardware ADC flags or conversion registers into pre-allocated SRAM arrays during the measurement gap. Guarantees deterministic microsecond sampling timing without complex DMA setup.
- **Alternative B: Real-Time Direct Serial Transmit**: Transmits each ADC sample immediately over Serial as soon as conversion completes. Discarded because serial baud rate limitations (even at 921600 baud) clamp maximum sampling rate to ~10 kHz and distort gap duration timing.
- **Alternative C: Continuous Background DMA Ring Buffer**: Uses continuous DMA sampling into a circular buffer and extracts gap slices post-hoc. Discarded because synchronizing DMA ring buffer pointers with high-speed PWM off-transitions introduces severe HAL complexity.

### 3. Oscilloscope & Hardware Verification Interface
- **Alternative A: Dual Active-LOW Dedicated Trigger Pins (D9/D10) (Selected)**: Drives pins `D9` and `D10` LOW strictly during the active measurement gap, matching `examples/Minimal_PWM_BDR6133_Fixed_Oszi`. Provides physical logic analyzer and oscilloscope synchronization for flyback decay inspection.
- **Alternative B: Single Pulse-Toggle Pin**: Toggles a single pin HIGH/LOW at the start and end of the gap. Discarded because dual active-low outputs allow separate oscilloscope trigger and scope channel gating without signal ambiguity.
- **Alternative C: No Hardware Triggering (Software Logging Only)**: Relies purely on microsecond timestamps logged in CSV. Discarded because software timestamps cannot capture physical inductive flyback voltage spikes on oscilloscope probes.

### 4. BEMF Inductive Decay Filter Strategy
- **Alternative A: Software Settling Time Guard Window ($T_{\text{settle}}$) (Selected)**: Discards ADC samples taken during the initial $T_{\text{settle}}$ microsecond window immediately following PWM cutoff to ignore high-voltage inductive flyback spikes. Simple, reliable, and configurable.
- **Alternative B: Analog Hardware Low-Pass RC Filtering**: Adds physical RC low-pass filters to `A0` and `A1` inputs. Discarded because fixed hardware filters attenuate high-frequency commutator current ripple required for position tracking.
- **Alternative C: Real-Time Digital Exponential Smoothing (IIR Filter)**: Applies an IIR low-pass filter to raw bEMF ADC values during the gap. Discarded because non-linear inductive spikes skew initial IIR filter state memory, leading to biased bEMF estimates.

---

## Discarded Technical Alternatives Summary

1. **Sweep Execution Engine Architecture**:
   - *Alternative B (Delay-Based Synchronous Loop)*: Discarded due to timing jitter and non-deterministic serial blocking during measurement gaps.
   - *Alternative C (Task-Based FreeRTOS Engine)*: Discarded due to increased binary footprint, scheduler overhead, and reduced core portability across target MCUs.

2. **High-Speed Burst ADC Capture Realization**:
   - *Alternative B (Real-Time Direct Serial Transmit)*: Discarded due to baud rate throughput bottlenecks restricting ADC sampling bandwidth.
   - *Alternative C (Continuous Background DMA Ring Buffer)*: Discarded due to DMA abstraction complexity.

3. **Oscilloscope & Hardware Verification Interface**:
   - *Alternative B (Single Pulse-Toggle Pin)*: Discarded because dual active-low triggering provides superior oscilloscope hardware gating capabilities.
   - *Alternative C (No Hardware Triggering)*: Discarded because physical probe verification of inductive flyback decay requires accurate hardware trigger outputs.

4. **BEMF Inductive Decay Filter Strategy**:
   - *Alternative B (Analog Hardware Low-Pass RC Filtering)*: Discarded because physical hardware filtering distorts current ripple signals needed for commutator counting.
   - *Alternative C (Real-Time Digital Exponential Smoothing)*: Discarded because initial flyback voltage spikes corrupt the recursive state of IIR digital filters.
