# 🛡️ JARVIS-CV v1: Autonomous 2-DOF Targeting Sentry Turret

An autonomous, computer-vision-powered 2-DOF targeting sentry turret built with MediaPipe, OpenCV, Python 3.12, and Arduino Uno. Designed for real-time tracking, manual override, and physical telemetry feedback.

---

## ✨ Features

* **MediaPipe Palm Landmark Engine:** High-accuracy 21-point hand skeleton detection superseding legacy Haar Cascades for dynamic gesture tracking and distance mapping.
* **Multi-Profile CV Tracking:** Real-time targeting for Red, Green, and Blue HSV color profiles alongside MediaPipe Palm Detection.
* **Unified Single-String Protocol:** Optimized serial data structure transmitting smooth coordinate commands (`PAN,TILT,PROXIMITY`) alongside lightweight 1-byte state toggles (`1`-`3`, `F`, `M`, `C`, `P`, `W`, `A`, `S`, `D`).
* **Interactive Manual Override:** Toggleable manual mode (`M`) enabling smooth WASD pan-tilt keybind controls without interrupting the tracking engine state.
* **Proximity & Breach State Machine:** Dynamic PWM LED brightness and pitch-shifting buzzer audio mapped directly to target proximity (0–100%), with automatic full breach alerts at $\ge 85\%$.
* **Non-Blocking Auto-Patrol:** Sweeps smoothly between 30° and 150° pan when no active targets are detected for over 3 seconds.
* **Hardware Protection:** Software-enforced mechanical constraints (Pan 10°–170°, Tilt 40°–140°) and non-blocking degree-by-degree step interpolation (`smoothMove`) to prevent gear stripping and USB brownouts.
* **Dual-Mode Python Environment:** Features an automatic fallback to **Simulation Mode** (HUD telemetry preview on PC) when no physical Arduino microcontroller is detected on the serial bus.

---

## 🔌 Hardware Wiring & Pinout

| Component | Arduino Pin | Description |
| :--- | :--- | :--- |
| **Pan Servo (SG90)** | Pin 9 | Horizontal rotation servo |
| **Tilt Servo (SG90)** | Pin 10 | Vertical elevation servo |
| **PWM Proximity LED** | Pin 6 | PWM brightness indicator (0–255) |
| **PWM Piezo Siren** | Pin 5 | Dynamic pitch speaker (400Hz–2500Hz) |
| **16x2 LCD (I2C)** | SDA (A4), SCL (A5) | I2C Telemetry Screen (Address `0x27`) |

---

## ⌨️ Master Keybind Directory

| Key | System Action | Arduino Display |
| :---: | :--- | :--- |
| **`1`** | Switch to RED Scan Profile | `MODE: RED SCAN` |
| **`2`** | Switch to GREEN Scan Profile | `MODE: GREEN SCAN` |
| **`3`** | Switch to BLUE Scan Profile | `MODE: BLUE SCAN` |
| **`F` / `f`** | Enable Palm Tracking Mode | `MODE: PALM TRACK` |
| **`M` / `m`** | Toggle Manual WASD Mode On/Off | `STATUS:Manual` |
| **`C` / `c`** | Enter Auto-Calibration Mode | `MODE: CALIBRATING` |
| **`P` / `p`** | Power System On / Standby Shutdown | `SYS: OFFLINE` |
| **`W` / `S`** | Manual Tilt Control (Up / Down +5°) | `P:XXX T:YYY D:ZZ%` |
| **`A` / `D`** | Manual Pan Control (Left / Right ±5°) | `P:XXX T:YYY D:ZZ%` |
| **`Q`** | Terminate Vision HUD safely | — |

---

## 🛠️ Project Setup Checklist

- [ ] Assemble 2-DOF pan-tilt mount with 2x SG90 micro-servos.
- [ ] Connect servos, PWM LED, Piezo speaker, and I2C LCD to the Arduino Uno per pinout.
- [ ] Flash the updated firmware `sentry_arduino.ino` via Arduino IDE (Baud Rate: `115200`).
- [ ] Install Python dependencies using the Python Launcher:
  ```cmd
  py -3.12 -m pip install --user opencv-python mediapipe pyserial numpy
