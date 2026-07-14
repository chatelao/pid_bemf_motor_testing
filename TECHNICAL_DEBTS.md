# TECHNICAL DEBTS - Märklin Motor Test & Calibration Tool

## Identified Debts

### Documentation
- [ ] **Initial Setup**: No `README.md` in the root directory yet.
- [ ] **Scripts missing**: `src/install.sh` and `test/install.sh` mentioned in `GEMINI.md` are not yet created.

### Architecture
- [ ] **Dual-Core Utilization**: While mentioned as a possibility for ripple detection, the exact inter-core communication mechanism is not yet defined.
- [ ] **Sensing Accuracy**: Current shunt value and amplification (if any) are not yet specified, which affects the ADC resolution for ripple detection.

### Environment
- [ ] **CI/CD**: GitHub Actions workflows are not yet implemented.
- [ ] **Project Structure**: PlatformIO configuration (`platformio.ini`) and source directories (`src/`, `include/`, `lib/`, `test/`) are not yet initialized.
