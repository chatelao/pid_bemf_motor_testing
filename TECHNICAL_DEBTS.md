# TECHNICAL DEBTS - Märklin Motor Test & Calibration Tool

## Identified Debts

### Documentation
- [x] **Initial Setup**: Root `README.md` created.
- [x] **Scripts**: `src/install.sh` and `test/install.sh` created.

### Architecture
- [ ] **Dual-Core Utilization**: While mentioned as a possibility for ripple detection, the exact inter-core communication mechanism is not yet defined.
- [ ] **Sensing Accuracy**: Current shunt value and amplification (if any) are not yet specified, which affects the ADC resolution for ripple detection.

### Environment
- [x] **CI/CD**: GitHub Actions workflows implemented.
- [x] **Project Structure**: PlatformIO configuration (`platformio.ini`) and source directories (`src/`, `include/`, `lib/`, `test/`) initialized.
