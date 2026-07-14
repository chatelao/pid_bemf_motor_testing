# ROADMAP - Märklin Motor Test & Calibration Tool

## Progress Overview
| Phase | Description | Status |
|---|---|---|
| 1 | Concept & Design | 🚧 |
| 2 | Environment & CI/CD Setup | ⏳ |
| 3 | Basic Motor Control (PWM) | ⏳ |
| 4 | BEMF Sensing & PID Implementation | ⏳ |
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
- [ ] Setup GitHub Action workflows for CI/CD
- [ ] Initialize PlatformIO project structure

### Phase 3: Basic Motor Control (PWM)
- [ ] Implement dual-channel PWM on D7/D8
- [ ] Basic open-loop motor drive functionality
- [ ] Status LED integration (D15, D16)

### Phase 4: BEMF Sensing & PID Implementation
- [ ] Implement synchronous ADC polling on A0/A1
- [ ] Integrate Arduino PID Library
- [ ] Implement closed-loop speed control
- [ ] Create "PWM and bEMF only" characterization sketch (`examples/BEMF_Ramp_Test`)
    - [ ] 1s ramp up/down to max speed
    - [ ] BEMF measurement during PWM off-time
    - [ ] 1-50ms measurement gap every 250ms at max speed

### Phase 5: Commutator Ripple Detection
- [ ] Implement DMA-based ADC sampling on A2
- [ ] Develop digital filtering and peak detection logic
- [ ] Implement absolute position/revolution counter

### Phase 6: CLI & Data Logging
- [ ] Integrate SerialCommands library
- [ ] Implement commands for speed, PID tuning, and telemetry
- [ ] Standardize telemetry output format

### Phase 7: Calibration Sweeps & Optimization
- [ ] Automated $K_e$ measurement sweep
- [ ] PID gain auto-tuning routine
- [ ] Final performance verification and documentation
