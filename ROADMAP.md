# ROADMAP - Märklin Motor Test & Calibration Tool

## Progress Overview
| Phase | Description | Status |
|---|---|---|
| Phase 1 | Foundation & Multi-Platform Documentation | ✅ |
| Phase 2 | Environment Setup (CI/CD) | ⏳ |
| Phase 3 | Core Implementation (BEMF & PID) | ⏳ |
| Phase 4 | CLI & Telemetry | ⏳ |
| Phase 5 | Validation & Calibration Sweeps | ⏳ |

## Goals
- [ ] Support XIAO RP2040, Nucleo STM32F446RE, and Nucleo G431RB. ⏳
- [ ] Reliable BEMF sensing synchronized with PWM. ⏳
- [ ] Stable PID speed control for permanent magnet refitted Märklin motors. ⏳
- [ ] Extensible CLI for calibration and real-time logging. ⏳

## Phases

### Phase 1: Foundation & Multi-Platform Documentation
- [x] Define Goal and Structure in `GEMINI.md`.
- [x] Update `CONCEPT.md` for multi-platform support.
- [x] Update `DESIGN.md` with technical choices and pin mappings.
- [x] Generalize `TOP_ARCHITECTURE.puml`.

### Phase 2: Environment Setup (CI/CD)
- [ ] Create `src/install.sh` for build tools. ⏳
- [ ] Create `test/install.sh` for test tools. ⏳
- [ ] Setup GitHub Action Workflow for PlatformIO builds. ⏳
- [ ] Initialize `platformio.ini` with environments for all three boards. ⏳

### Phase 3: Core Implementation (BEMF & PID)
- [ ] Implement synchronous BEMF sampling. ⏳
- [ ] Integrate Arduino PID library. ⏳
- [ ] Implement basic PWM motor drive. ⏳

### Phase 4: CLI & Telemetry
- [ ] Integrate SerialCommands library. ⏳
- [ ] Implement speed setpoint and PID tuning commands. ⏳
- [ ] Implement real-time telemetry streaming (CSV/Serial Plotter format). ⏳

### Phase 5: Validation & Calibration Sweeps
- [ ] Implement automated PID tuning sweep scripts. ⏳
- [ ] Validate motor performance on all three hardware platforms. ⏳
- [ ] Finalize documentation and usage examples. ⏳
