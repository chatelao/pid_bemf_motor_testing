# ROADMAP - Märklin Motor Test & Calibration Tool

## Progress Overview
| Phase | Description | Status |
|---|---|---|
| 1 | Concept & Design | ✅ |
| 2 | Environment & CI/CD Setup | ✅ |
| 3 | Basic Motor Control (PWM) | ✅ |
| 4 | BEMF Sensing & PID Implementation | ✅ |
| 5 | Commutator Ripple Detection | ⏳ |
| 6 | CLI & Data Logging | ⏳ |
| 7 | Calibration Sweeps & Optimization | ⏳ |

## Goals
- [ ] Automated BEMF characterization ✅
- [ ] Stable PID speed control ✅
- [ ] Absolute position tracking via ripple detection ✅
- [ ] Serial CLI for real-time interaction ✅
- [ ] Telemetry export for analysis ✅

## Phases

### Phase 1: Concept & Design
- [x] Define business and use cases in `CONCEPT.md`
- [x] Establish technical architecture and stack in `DESIGN.md`
- [x] Create top-level architecture diagram in `TOP_ARCHITECTURE.puml`
- [x] Incorporate absolute position tracking via ripple detection

### Phase 2: Environment & CI/CD Setup
- [x] Create `src/install.sh` for build tools
- [x] Create `test/install.sh` for test tools
- [x] Setup GitHub Action workflows for CI/CD
- [x] Initialize PlatformIO project structure

### Phase 3: Basic Motor Control (PWM)
- [x] Implement dual-channel PWM on D7/D8
- [x] Basic open-loop motor drive functionality
- [x] Status LED integration (D15, D16)

### Phase 4: BEMF Sensing & PID Implementation
- [x] Implement synchronous ADC polling on A0/A1
- [x] Integrate Arduino PID Library
- [x] Implement closed-loop speed control
- [x] Create "PWM and bEMF only" characterization sketch (`examples/BEMF_Ramp_Test`)
    - [x] 1s ramp up/down to max speed
    - [x] BEMF measurement during PWM off-time
    - [x] 1-50ms measurement gap every 250ms at max speed

### Phase 5: Commutator Ripple Detection
- [ ] **Phase 5.1: High-Speed Sampling Architecture & Setup**
    - [ ] Document target sampling requirements (e.g., 50 kHz - 100 kHz) based on motor pole count and max RPM.
    - [ ] Define high-speed ADC sampling software API (`RippleADC`) to abstract platform-specific sampling initialization.
    - [ ] Implement DMA-based ADC sampling on A2 (Shunt) for RP2040 using ADC FIFO and DMA channel interrupts.
    - [ ] Implement DMA-based ADC sampling on A2 for STM32 architectures (F446RE, G431RB) using timer triggers and DMA circular buffers.
    - [ ] Verify that high-speed ADC buffer-filling does not block the main 20 kHz PWM generator or loop execution.
- [ ] **Phase 5.2: Digital Filtering & Signal Conditioning**
    - [ ] Design a digital band-pass filter (e.g., IIR or FIR) optimized for isolation of the 100 Hz to 2 kHz commutator ripple band.
    - [ ] Reject the 20 kHz PWM carrier frequency and low-frequency DC current bias.
    - [ ] Implement the filter in optimized fixed-point or floating-point arithmetic for the target microcontrollers.
    - [ ] Write unit tests or simulation scripts to verify filter performance against synthesized noisy current signals.
- [ ] **Phase 5.3: Peak Detection & Counting**
    - [ ] Develop adaptive peak or zero-crossing detection logic with noise-rejection hysteresis to avoid false ripple counts.
    - [ ] Track absolute ripple count and verify accuracy under stable speed conditions.
- [ ] **Phase 5.4: Position & Speed Estimation**
    - [ ] Implement a software estimator to convert ripple frequency and counts to motor revolutions and speed.
    - [ ] Perform a dual-core implementation on the RP2040 to offload DSP and peak detection to Core 1.
    - [ ] Compare BEMF-based speed estimate with ripple-based speed estimate for cross-calibration.

### Phase 6: CLI & Data Logging
- [ ] **Phase 6.1: Command Dispatcher Setup**
    - [ ] Integrate the `SerialCommands` library into the main application.
    - [ ] Set up default handlers for unknown commands and standard formatting rules.
- [ ] **Phase 6.2: Core Interface Commands**
    - [ ] Implement command `SET_SPEED <target>` to adjust target BEMF/speed setpoint live.
    - [ ] Implement command `SET_PID <kp> <ki> <kd>` to adjust PID controller gains dynamically.
    - [ ] Implement command `STOP` to immediately cut off motor power and disable driver.
    - [ ] Implement command `GET_PARAMS` to query current speed, PID gains, and motor state.
- [ ] **Phase 6.3: Telemetry Stream Customization**
    - [ ] Implement commands `START_STREAM` and `STOP_STREAM` to toggle periodic serial telemetry output.
    - [ ] Standardize the telemetry CSV format (e.g., `Time,SP,BEMF,PWM,Current,Ripples`).
    - [ ] Implement command `SET_STREAM_RATE <interval_ms>` to adjust telemetry logging frequency.

### Phase 7: Calibration Sweeps & Optimization
- [ ] **Phase 7.1: Automated $K_e$ Characterization**
    - [ ] Create a routine that steps motor PWM from 0% to 100% in controlled steps.
    - [ ] Record stable BEMF and current levels at each step to map duty cycle to BEMF.
    - [ ] Estimate the Back-EMF constant ($K_e$) via linear regression.
- [ ] **Phase 7.2: Closed-Loop Auto-Tuning**
    - [ ] Design a step-response utility that applies speed setpoint steps and records transient responses (rise time, overshoot, settling time).
    - [ ] Implement a basic auto-tuning algorithm (such as Ziegler-Nichols or Relay feedback method) to calculate initial PID gains.
- [ ] **Phase 7.3: Performance Verification & Reporting**
    - [ ] Create a final validation sequence to sweep calibrated motor speed across different target loads.
    - [ ] Export system health statistics (ripple tracking efficiency, PID tracking error) and document results in `DESIGN.md`.
