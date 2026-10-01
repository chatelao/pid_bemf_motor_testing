# CONCEPT_CALIBRATION - Parameter Calibration & Exploration Software Concept

## Goal
Establish a structured calibration concept and software exploration framework based on `examples/Minimal_PWM_BDR6133_Fixed_Oszi` to experimentally determine optimal operating parameters for Märklin motors refitted with permanent magnets using the BDR-6133 motor driver.

The system explores four key parameters:
1. **PWM Frequency**: Finding the optimal switching frequency balancing acoustic noise, driver heating, current ripple quality, and motor torque.
2. **BEMF Readout Interruption**: Determining the minimum required motor drive interruption window (gap duration) and timing delay to capture inductive decay-free Back-EMF signals.
3. **ADC Readout Speed**: Optimizing ADC sampling clock rate, resolution, and sampling window speed to achieve clean bEMF/current telemetry without excessive quantization or conversion noise.
4. **Kickstart Values**: Calibrating initial PWM boost level and pulse duration to overcome static friction (stiction) during low-speed starting without aggressive velocity jumps.

The software systematically sweeps these parameter spaces while outputting high-speed bEMF readouts, motor current, time stamps, and oscilloscope trigger signals to the serial log for automated analysis.

---

## Business & Use Cases

### Business Cases
- **Automated Locomotive Profiling**: Reduce manual tuning labor when converting heritage Märklin train fleets by executing a standard calibration sweep that outputs optimal motor profiles.
- **Hardware Protection & Reliability**: Prevent motor stall heating and driver thermal shutdown by establishing accurate kickstart boundaries and minimum PWM frequencies.
- **Data-Driven Quality Assurance**: Provide standardized CSV telemetry logs that allow hobbyists and engineers to verify physical motor health and magnetic degradation over time.

### Use Cases
- **Sweep PWM Carrier Frequencies**: Evaluate motor responsiveness and inductive flyback decay across frequencies (e.g., 5 kHz to 32 kHz).
- **Sweep BEMF Interruption Windows**: Sweep measurement gap durations (e.g., 500 µs to 5000 µs) and sample timing relative to PWM disable transitions.
- **Sweep ADC Sampling Speeds**: Vary MCU ADC clock prescalers and sample counts to evaluate telemetry signal-to-noise ratio (SNR).
- **Sweep Kickstart Dynamics**: Step through kickstart PWM duty cycles (e.g., 20% to 80%) and burst durations (e.g., 10 ms to 100 ms) to establish seamless motion initiation from standstill.

---

## Foundation: Minimal_PWM_BDR6133_Fixed_Oszi

The existing `examples/Minimal_PWM_BDR6133_Fixed_Oszi` sketch provides the reference architecture:
- **Dual-Channel H-Bridge Control**: Drives BDR-6133 via `D7` and `D8` with configurable PWM frequency (`analogWriteFreq`).
- **High-Speed Measurement Window**: Disables motor outputs (`analogWrite(D7, 0); analogWrite(D8, 0);`) and triggers hardware oscilloscope outputs (`D9`, `D10`) during a fixed 2 ms measurement gap.
- **Burst ADC Sampling**: Captures high-frequency ADC arrays (`A0` for bEMF A, `A1` for bEMF B, `A2` for Shunt Current `bEMF_C`) into RAM buffers (`bemfA_buf`, `bemfB_buf`, `bemfC_buf`, `time_buf`).
- **Asynchronous Telemetry Logging**: Flushes captured burst buffers over USB Serial (`115200` or `921600` baud) during the active motor drive cycle to avoid blocking the high-speed measurement window.

```text
                  +-------------------------------------------------------+
                  |         Exploration Loop (Iterates Sweeps)           |
                  +-------------------------------------------------------+
                                              |
     +----------------------------------------+----------------------------------------+
     |                                        |                                        |
     v                                        v                                        v
[1. Config PWM Freq]              [2. Config Kickstart]                    [3. Config Interruption Window]
 (5 kHz - 32 kHz)                  (Duty % & Duration ms)                   (500 µs - 5000 µs)
     |                                        |                                        |
     +----------------------------------------+----------------------------------------+
                                              |
                                              v
                             +---------------------------------+
                             |     Active Drive State (PWM)    |
                             |   Apply Kickstart / PWM Drive   |
                             +---------------------------------+
                                              |
                                              v (Trigger Gap: D9/D10 LOW->HIGH)
                             +---------------------------------+
                             |    BEMF Interruption Window     |
                             | Motor OFF (D7=0, D8=0 Coasting) |
                             | Burst ADC Readout (A0, A1, A2)  |
                             +---------------------------------+
                                              |
                                              v
                             +---------------------------------+
                             |    Serial Telemetry Output      |
                             | Output CSV Buffers over Serial  |
                             +---------------------------------+
```

---

## Parameter Exploration Specifications

### 1. PWM Frequency Sweeping
- **Objective**: Identify the frequency range that minimizes motor humming (acoustic noise) without causing excess switching losses in the BDR-6133 or smothering current ripple pulses.
- **Exploration Range**: 5 kHz to 32 kHz (e.g., 8 kHz, 16 kHz, 20 kHz, 25 kHz, 32 kHz).
- **Evaluation Criteria**: Measured current ripple amplitude on `A2`, motor torque capability at low duty cycles, and BDR-6133 driver temperature.

### 2. BEMF Readout Interruption Sweeping
- **Objective**: Measure motor Back-EMF during PWM "off" gaps without interference from inductive discharge spikes (flyback current).
- **Exploration Range**:
  - **Interruption Duration**: 500 µs to 5000 µs in 250 µs increments.
  - **Sampling Delay (Settling Time)**: 50 µs to 500 µs after PWM cutoff before taking bEMF valid readings.
- **Evaluation Criteria**: Signal stability after flyback collapse; minimal speed loss during the interruption gap.

### 3. ADC Readout Speed Sweeping
- **Objective**: Maximize sampling frequency on channels `A0`, `A1`, and `A2` to capture transient spikes while staying within MCU ADC clock constraints.
- **Exploration Range**:
  - **ADC Clock Prescaler / Rate**: Standard Arduino default vs maximum hardware sampling rate (e.g., 50 kHz to 250 kHz conversion rate per channel).
  - **Buffer Depth**: 250 to 1000 samples per measurement window.
- **Evaluation Criteria**: SNR of bEMF voltage, current spike resolution, and RAM buffer consumption.

### 4. Kickstart Calibration (PWM Level & Duration)
- **Objective**: Ensure dependable starting from standstill for heavy or high-stiction 3-pole / 5-pole Märklin motors.
- **Exploration Range**:
  - **Kickstart Duty Cycle**: 20% to 80% duty cycle (Open-loop PWM).
  - **Kickstart Duration**: 10 ms to 150 ms burst prior to dropping to target steady-state PWM.
- **Evaluation Criteria**: Minimal startup lag time (time to first bEMF detection) without causing wheel slip or high velocity overshoot.

---

## Logging & Serial Telemetry Output Format

To support automated plotting (e.g., Python scripts, Serial Plotter, or CSV analysis tools), the software outputs structured CSV streams over Serial.

### Log Header
```csv
Param_Sweep_ID,PWM_Freq_Hz,Gap_Duration_us,Kickstart_PWM,Kickstart_Dur_ms,Sample_Idx,Time_us,PWM_Dir_Speed,bEMF_A0,bEMF_A1,Current_Shunt_A2
```

### Example Log Output
```csv
# START_SWEEP: Freq=20000, Gap=2000, KickPWM=180, KickDur=30
SWEEP,20000,2000,180,30,0,104,128,2048,45,312
SWEEP,20000,2000,180,30,1,112,128,1980,45,290
SWEEP,20000,2000,180,30,2,120,128,1850,45,250
...
# END_SWEEP
```

---

## Major Design Choices

In accordance with project architecture guidelines, each major design decision evaluates exactly three alternatives (Selected vs Discarded).

### 1. Parameter Sweep Control Strategy
- **Alternative A: Software Automated Grid Search (Selected)**: The microcontroller autonomously steps through a matrix of PWM frequencies, gap durations, ADC speeds, and kickstart values, emitting structured CSV logs for each combination. Provides repeatable, automated, and hands-free characterization.
- **Alternative B: Interactive CLI Manual Tuning**: The user adjusts parameters individually via Serial CLI commands. Allows quick manual testing but makes exhaustive multidimensional parameter mapping time-consuming and prone to human recording errors.
- **Alternative C: Fixed Hardcoded Preset Switching**: The software toggles between 3 hardcoded parameter profiles triggered by a physical push-button. Extremely simple but fails to explore the continuous parameter space needed for optimal motor calibration.

### 2. High-Speed Telemetry Buffering Strategy
- **Alternative A: Dual-Buffered Burst Capture with Asynchronous Inter-Gap Flushing (Selected)**: Samples are written directly to a RAM buffer during the high-speed interruption window, then transmitted asynchronously over USB Serial during the active PWM drive phase. Guarantees zero serial output overhead during critical bEMF sampling.
- **Alternative B: Real-Time Blocking Serial Print**: Sampling and `Serial.print` calls are executed synchronously inside the interruption loop. Simple to write, but serial baud rate limits drastically restrict sampling frequency and introduce severe timing jitter.
- **Alternative C: Ring-Buffer DMA Direct Streaming**: Continuously streams ADC data to USB using background DMA transfers. Offers maximum bandwidth, but DMA ring-buffer implementations significantly increase software complexity for parameter sweep sketches.

### 3. Oscilloscope Verification & Hardware Triggering
- **Alternative A: Inverted Dual-Pin Logic Gate Triggering (Selected)**: Pins `D9` and `D10` are kept HIGH during active drive, and driven LOW during the measurement window to provide clean active-low oscilloscope triggers matching `Minimal_PWM_BDR6133_Fixed_Oszi`.
- **Alternative B: Software-Only Microsecond Timestamping**: Relying exclusively on internal `micros()` software logs without dedicated hardware trigger pins. Reduces pin usage, but prevents precise hardware verification of ADC conversion timing using physical oscilloscopes or logic analyzers.
- **Alternative C: Dedicated SPI/I2C DAC Output Triggering**: Outputting internal digital state variables as analog voltages via an external DAC for oscilloscope monitoring. Highly flexible, but requires extra hardware components not present on the standard calibration board setup.

---

## Discarded Alternatives Summary

1. **Parameter Sweep Control Strategy**:
   - *Alternative B (Interactive CLI)*: Discarded due to difficulty in systematically mapping complex 4-parameter search spaces manually.
   - *Alternative C (Fixed Hardcoded Presets)*: Discarded because hardcoded presets cannot discover optimal settings across diverse Märklin motor types.

2. **High-Speed Telemetry Buffering Strategy**:
   - *Alternative B (Real-Time Blocking Serial)*: Discarded due to severe timing jitter and low sampling rates caused by blocking serial calls.
   - *Alternative C (Ring-Buffer DMA Direct Streaming)*: Discarded due to software abstraction overhead for tool sketches.

3. **Oscilloscope Verification & Hardware Triggering**:
   - *Alternative B (Software-Only Timestamping)*: Discarded because hardware oscilloscope verification is mandatory for accurate timing analysis of flyback decay.
   - *Alternative C (Dedicated DAC Output)*: Discarded because it adds unnecessary external hardware dependencies to the calibration setup.
