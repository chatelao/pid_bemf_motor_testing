# Simulation & Analysis Notebooks

This directory contains Jupyter notebooks used for modeling, simulating, and analyzing Märklin DC motor dynamics, PWM motor drive behavior, Back-EMF (BEMF) sensing gaps, and commutator current ripple.

## Available Notebooks

### 1. `marklin_dc_motor_pwm_simulation.ipynb`
An interactive electromechanical simulation engine for DC motors (e.g. refitted Märklin 3-pole and 5-pole Drum Collector Motors / DCM, coreless Glockenanker motors, and Gauge 1 / LGB motors) driven by PWM signals with high-impedance BEMF readout gaps.

#### Key Features & Technical Capabilities:
- **Electromechanical Physics Engine**: Solves the coupled differential equations governing armature current ($I_a$) and rotational angular speed ($\omega$):
  $$\frac{dI_a}{dt} = \frac{V_{applied} - R_a I_a - k_e \omega}{L_a}$$
  $$\frac{d\omega}{dt} = \frac{k_t I_a - T_{load} - b \omega}{J}$$
- **High-Frequency Waveform Modeling**:
  - Commutator current ripple based on pole count ($N_{poles}$).
  - Physical commutation spikes featuring current-dependent inductive surges (`comm_spike_amplitude`, `comm_spike_width`).
  - Brush arcing noise (`brush_noise_amplitude`).
- **BEMF Sensing Gap Diagnostics**:
  - Simulates periodic high-impedance PWM-off windows ($T_{gap}$) for sampling motor generator voltage.
  - Models inductive freewheeling tail decay saturation to determine optimal ADC sampling delays and prevent measurement distortion.
- **Interactive Visualization Dashboard**:
  - Built with `ipywidgets` and `matplotlib`.
  - Features real-time parameter tuning sliders for PWM frequency, duty cycle, gap period/duration, load torque, and motor physical constants.
  - Interactive BEMF gap selector (`w_gap_index`) for zoomed inspection of individual BEMF measurement windows and edge cases.
- **Pre-configured Motor Presets**:
  - **Alte Märklinmotoren (3-polig / 5-polig DCM)**: High inductance ($L_a \approx 10\text{--}25\text{ mH}$), significant mechanical inertia.
  - **Glockenankermotor (Coreless / Faulhaber / Maxon)**: Extremely low inductance ($L_a < 1\text{ mH}$), rapid electrical response.
  - **Moderne Motoren**: Standard 5-pole DC motor characteristics.
  - **Grosse Gartenbahnmotoren (Spur 1 / LGB)**: Higher torque and current capacities.

---

## Prerequisites & Installation

To run the simulation notebooks, ensure you have Python 3.8+ installed along with the required numerical and visualization packages:

```bash
pip install numpy scipy matplotlib ipywidgets jupyterlab
```

If using JupyterLab, ensure the `ipywidgets` extension is enabled:

```bash
jupyter lab extension list
```

---

## Usage

Launch JupyterLab or Jupyter Notebook from the project root directory:

```bash
jupyter lab notebooks/marklin_dc_motor_pwm_simulation.ipynb
```

or

```bash
jupyter notebook notebooks/marklin_dc_motor_pwm_simulation.ipynb
```

Once opened, run all cells to display the interactive dashboard controls and plot outputs.
