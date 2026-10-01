# Märklin Motor Test & Calibration Tool

## Overview
This project provides tool sketches for the Seeed Studio XIAO RP2040 to test and calibrate Märklin motors after refitting them with permanent magnets. It utilizes Back-EMF (BEMF) sensing and PID control for precise speed regulation.

## Documentation
- [CONCEPT.md](CONCEPT.md): High-level architecture and use cases.
- [DESIGN.md](DESIGN.md): Technical details and implementation choices.
- [ROADMAP.md](ROADMAP.md): Project status and planned steps.
- [TECHNICAL_DEBTS.md](TECHNICAL_DEBTS.md): Log of technical shortcuts and issues.

## Hardware Support
- **Seeed Studio XIAO RP2040**

## Getting Started
### Building Firmware and Example UF2 Assets
To build the main application firmware and package all example sketches as `.uf2` binaries for the Seeed Studio XIAO RP2040, execute the packaging script:

```bash
chmod +x scripts/package_uf2.sh
./scripts/package_uf2.sh dist
```

All compiled `.uf2` assets will be placed into the `dist/` directory (e.g. `Marklin_Motor_Control.uf2`, `BEMF_Ramp_Test.uf2`, `Blink.uf2`, etc.).

### Continuous Integration & Release Packaging
- **CI Workflow (`.github/workflows/build.yml`)**: Builds all examples and uploads UF2 binaries as CI build artifacts on every push or pull request.
- **Release Workflow (`.github/workflows/release.yml`)**: Automatically generates `.uf2` assets for all examples and main firmware when a tag (`v*`) or release is published on GitHub.

## License
MIT
