# Märklin Motor Calibration Project Documentation Index

Welcome to the documentation directory of the **Märklin Motor Test & Calibration Tool** project. This folder holds theoretical papers, application notes summaries, and practical implementation guides regarding sensorless motor control.

---

## Document Index

1.  **[TI SLVA303 Summary](TI_SLVA303_Summary.md)**
    *   **Topic**: *Sensorless Speed Control of Brushed DC Motors Using Current Ripple*
    *   **Focus**: Mathematical foundations of ripple frequency, physical origin of the ripple signal, and analog/digital band-pass filter topology designs to isolate the ripple from DC and PWM components.
2.  **[Infineon AN-Motor-01 Summary](Infineon_AN_Motor_01_Summary.md)**
    *   **Topic**: *DC Motor Control using Ripple Counting*
    *   **Focus**: Problems encountered under dynamic real-world environments (rapid load changes, starting/stopping phase, brush sparking), and mitigation strategies (adaptive software hysteresis, plausibility checks, ghost-pulse extrapolation).
3.  **[Commutator Ripple Extraction Practical Guide](Ripple_Extraction_Guide.md)**
    *   **Topic**: *Practical Implementation Guide on Target MCUs*
    *   **Focus**: Concrete engineering decisions for the Seeed Studio XIAO RP2040 and ST Nucleo-F446RE/G431RB microcontrollers. Contains code examples for DMA-based high-speed sampling, 2nd-order fixed-point IIR filter implementation, dynamic hysteresis peak detection, and multi-core processing architecture.

---

## Architectural Choices & Quality Standards
Per our project's rigorous documentation standards, every design alternative and architectural choice inside these documents is evaluated using **exactly three options**:
*   A chosen optimal option (detailed with design specifics and hardware mapping).
*   Two discarded options (clearly summarized with technical explanations of why they are unsuitable for our specific Märklin motor calibration context).
