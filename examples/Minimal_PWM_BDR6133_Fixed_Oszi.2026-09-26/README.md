# Minimal PWM BDR6133 Fixed Oszi (Snapshot 2026-09-26)

This directory contains a standalone archive/snapshot of the `Minimal_PWM_BDR6133_Fixed_Oszi` oscilloscope test sketch.

## Purpose

Provides a preserved snapshot of the high-speed BEMF sampling and oscilloscope trigger sketch designed for analyzing motor coasting decays and driver switching characteristics on the Seeed Studio XIAO RP2040 and BDR-6133 motor driver stage.

## Key Features

- **20 kHz Ultrasonic Motor Drive**: Drives BDR-6133 PWM pins `D7` and `D8` above human hearing limits.
- **Oscilloscope Hardware Triggers**: Dedicated trigger output lines on `D9` and `D10` that reflect active driving versus BEMF measurement gaps.
- **In-Memory High-Speed Sampling**: Stores raw ADC samples (`A0`, `A1`, `A2`) into array buffers during 2 ms PWM-off measurement gaps.
- **Asynchronous Telemetry Output**: Outputs microsecond-timestamped BEMF samples over USB Serial during active motor driving intervals.

## Hardware & Pin Mapping

| Signal | Pin | Description |
| :--- | :--- | :--- |
| **PIN_PWM_A** | `D7` / GPIO 1 | BDR-6133 Phase A Drive |
| **PIN_PWM_B** | `D8` / GPIO 2 | BDR-6133 Phase B Drive |
| **PIN_BEMF_A** | `A0` / GPIO 26 | BEMF Terminal A ADC Input |
| **PIN_BEMF_B** | `A1` / GPIO 27 | BEMF Terminal B ADC Input |
| **PIN_SHUNT** | `A2` / GPIO 28 | Current Sense Shunt ADC Input |
| **PIN_TRIG_1** | `D9` / GPIO 9 | Oscilloscope Trigger 1 |
| **PIN_TRIG_2** | `D10` / GPIO 10 | Oscilloscope Trigger 2 |

## Directory Structure

```text
Minimal_PWM_BDR6133_Fixed_Oszi.2026-09-26/
└── Minimal_PWM_BDR6133_Fixed_Oszi/
    └── Minimal_PWM_BDR6133_Fixed_Oszi.ino
```

## Compilation

```bash
arduino-cli compile --fqbn rp2040:rp2040:seeed_xiao_rp2040 Minimal_PWM_BDR6133_Fixed_Oszi/Minimal_PWM_BDR6133_Fixed_Oszi.ino
```
