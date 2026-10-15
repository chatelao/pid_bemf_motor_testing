# Argon Regler - Virtual Commutator Encoder & Closed-Loop Control Suite

This example sketch implements the **Argon Regler** (v3.0) control and diagnostic suite for DC and refitted Märklin model railway locomotives. It combines deterministic closed-loop speed regulation, target position control, automated motor parameter identification (auto-tuning), and a phase-locked **Virtual Commutator Encoder** using the **Seeed Studio XIAO RP2040** board and **BDR-6133** H-bridge driver.

## Purpose

When controlling DC and refitted Märklin motors, precise speed control and position tracking normally require physical hardware optical or magnetic encoders. The Argon Regler software suite eliminates physical encoders by using high-speed Back-EMF (BEMF) sensing, current shunt ripple detection, and brush-arcing spark analysis to synthesize a **Virtual Commutator Encoder**.

This tool automatically identifies motor physical parameters (stiction/breakaway friction, plant gain $K$, mechanical time constant $\tau_m$), tunes PI control loops, and tracks exact wheel distance and RPM.

## Key Features

- **Automated Motor Identification (Auto-Tuning)**:
  - Measures breakaway PWM duty cycle (stiction/Coulomb friction).
  - Determines open-loop speed gain and estimates the mechanical time constant $\tau_m$.
  - Automatically calculates Proportional ($K_p$), Integral ($K_i$), Feed-Forward ($K_{v,ff}$), and Coulomb friction compensation parameters.
- **Virtual Commutator Encoder & Odometry**:
  - **Measurement Gap Spark Detector**: High-pass glitch detector samples BEMF in the coasting window to detect commutator brush arcing sparks and slot passages.
  - **Active Shunt Ripple Detector**: Polls motor current on `PIN_SHUNT` (A2) during PWM drive to detect commutation current dips.
  - **Hybrid Observer (Phase-Locked Loop)**: Fuses real spark counts with BEMF kinematic integration to seamlessly bridge the 93% non-gap driving time.
  - **Gear Backlash Compensation**: Deducts configurable gear play (`L<steps>`) on direction reversals before incrementing net track distance.
- **5 Versatile Operating Modes (`M0`–`M4`)**:
  - `M0` (**MODE_AUTO_TUNE**): Automatic motor parameter identification and virtual encoder calibration.
  - `M1` (**MODE_CLOSED_LOOP_RAMP**): BEMF speed-regulated ramp sweep with active virtual encoder.
  - `M2` (**MODE_STEP_RESPONSE**): Dynamic step-response test ($25\% \rightarrow 75\% \rightarrow 25\%$).
  - `M3` (**MODE_OPEN_LOOP_RAMP**): Unregulated PWM ramp for baseline comparisons.
  - `M4` (**MODE_POSITION_TARGET**): Closed-loop position controller that drives to a target step count and decelerates smoothly into place.
- **Hardware Oscilloscope Triggering**:
  - `PIN_TRIG_F0F` (`D9`): Set HIGH during exact BEMF ADC sampling window.
  - `PIN_TRIG_F0B` (`D10`): Toggles state on every validated commutator spark event for real-time oscilloscope verification.
- **Visual NeoPixel RGB Status**:
  - **Yellow/Orange**: Auto-Tuning active (`M0`).
  - **Cyan**: Target position reached (`M4`).
  - **Magenta**: Control output saturation ($100\%$ duty cycle).
  - **Blue**: Stopped / Idle.
  - **Red**: Forward drive active.
  - **Green**: Reverse drive active.

## Hardware Pin Mapping

The pin allocations are standardized for the Seeed Studio XIAO RP2040 and BDR-6133 motor driver stage:

| Signal | Function Description | Seeed Studio XIAO RP2040 Pin |
| :--- | :--- | :--- |
| **PIN_PWM_A** | BDR-6133 Phase A Drive | `D7` / GPIO 7 |
| **PIN_PWM_B** | BDR-6133 Phase B Drive | `D8` / GPIO 8 |
| **PIN_BEMF_A** | BEMF Terminal A Sensing | `A0` / GPIO 26 |
| **PIN_BEMF_B** | BEMF Terminal B Sensing | `A1` / GPIO 27 |
| **PIN_SHUNT** | Motor Shunt Current Sense | `A2` / GPIO 28 |
| **PIN_TRIG_F0F** | Oscilloscope Trigger 1 (BEMF Gap Window) | `D9` / GPIO 9 |
| **PIN_TRIG_F0B** | Oscilloscope Trigger 2 (Spark Detect Toggle)| `D10` / GPIO 10 |
| **PIN_NEOPIXEL_PWR**| Onboard WS2812 NeoPixel Power Enable | GPIO 11 |
| **PIN_NEOPIXEL_DATA**| Onboard WS2812 NeoPixel Data Line | GPIO 12 |
| **PIN_LED1** | Activity Status LED | GPIO 15 (Red Onboard LED) |
| **PIN_LED2** | Gap Indicator Status LED | GPIO 16 (Blue Onboard LED) |

## Interactive Serial CLI

The sketch features a non-blocking command interpreter running at **921,600 baud**. Commands are case-insensitive and processed on newline.

| Command | Argument | Description |
| :--- | :--- | :--- |
| `M` | `0`..`4` | Switch Operating Mode (`0`=AutoTune, `1`=ClosedLoopRamp, `2`=StepTest, `3`=OpenLoop, `4`=PositionTarget) |
| `T` | `<steps>` | Set Target Distance Steps and execute Mode 4 (e.g., `T2000` or `T-1500`) |
| `Z` | *None* | Zero / reset Virtual Encoder distance counter |
| `N` | `<slots>` | Set Commutator slots per revolution (e.g., `N6` for 3-pole, `N10` for 5-pole HLA) |
| `E` | `<adc>` | Set Spark Glitch Threshold in ADC counts (e.g., `E35.0`) |
| `L` | `<steps>` | Set Gear Backlash step hysteresis on reversal (e.g., `L12`) |
| `P` | `<val>` | Set Proportional Gain $K_p$ |
| `I` | `<val>` | Set Integral Gain $K_i$ |
| `F` | `<val>` | Set Linear Feed-Forward $K_{v,ff}$ |
| `C` | `<val>` | Set Static Friction Feed-Forward offset (PWM 0–255) |
| `B` | `<val>` | Set Max expected BEMF ADC value |
| `S` | `<us>` | Set BEMF settling time ($\mu\text{s}$) |
| `G` | `<us>` | Set Measurement gap duration ($\mu\text{s}$) |
| `Q` | `<hz>` | Set PWM carrier frequency (Hz) |
| `D` | `0` / `1` | Toggle discrete speed steps vs continuous ramp |
| `V` | `0` / `1` | Toggle verbose CSV stream (`1`=On, `0`=Off) |
| `H` or `?` | *None* | Display help menu and current system status |

## Telemetry Format

When CSV logging is enabled (`V1`), real-time stream data is output at **921,600 baud** in the following comma-separated format:

```csv
<TIME_US>,<MODE>,<STEP_LVL>,<TARGET_BEMF>,<MEAS_BEMF>,<PWM_DUTY>,<SPARKS_GAP>,<DIST_STEPS>,<EST_RPM>,<SHUNT_ACT>
```

- **TIME_US**: Microsecond timestamp (`micros()`).
- **MODE**: Active operating mode integer (`0`–`4`).
- **STEP_LVL**: Signed discrete speed step level.
- **TARGET_BEMF**: Signed target BEMF setpoint (ADC counts).
- **MEAS_BEMF**: Signed measured BEMF feedback (ADC counts).
- **PWM_DUTY**: Signed active PWM duty cycle ($-255$ to $+255$).
- **SPARKS_GAP**: Number of spark events detected in the most recent measurement gap.
- **DIST_STEPS**: Net track distance in virtual encoder steps (backlash-compensated).
- **EST_RPM**: Estimated motor rotational speed in RPM.
- **SHUNT_ACT**: Raw 12-bit ADC current reading from `PIN_SHUNT`.

## Compilation and Upload

### Build with PlatformIO

Navigate to the `examples/Argon_Regler` directory and run PlatformIO commands:

```bash
# Navigate to the Argon_Regler directory
cd examples/Argon_Regler

# Build firmware for Seeed Studio XIAO RP2040
pio run -e seeed_xiao_rp2040

# Upload firmware to board
pio run -e seeed_xiao_rp2040 -t upload
```

### Serial Monitor & CLI Interface

To open the CLI serial interface at 921,600 baud:

```bash
pio device monitor -b 921600
```
