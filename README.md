# 🛡️ Sentinel-CV: Autonomous 2-DOF Targeting Sentry Turret

An autonomous, computer-vision-powered 2-DOF targeting sentry turret built with OpenCV, Python, and Arduino for the Science Exhibition.

---

## ✨ Features

* **Multi-Target Detection:** Real-time tracking for Red, Green, and Blue HSV color objects as well as Human Face Detection using OpenCV Haar Cascades.
* **Manual WASD Control Mode:** Keybind override allowing manual pan-tilt targeting.
* **Priority State Machine:** Manages state changes between Breach Alerts, Mode Select, Target Locks, and Auto-Patrol.
* **Continuous Auto-Patrol:** Automatically sweeps 30° to 150° side-to-side when no target is detected for over 3 seconds.
* **Non-Flickering 16x2 LCD Telemetry:** Displays live system modes, panning/tilting angles, and status updates cleanly.
* **Hardware & Power Safety:** Built-in software angle constraints (Pan 10°–170°, Tilt 40°–140°) and 1° step interpolation to prevent USB brownouts and motor gear stripping.
* **Audio/Visual Breach Alert:** Triggerable strobe laser light and piezo buzzer siren.

---

## 🔌 Hardware Wiring & Pinout

| Component | Arduino Pin | Description |
| :--- | :--- | :--- |
| **Pan Servo (SG90)** | Pin 9 | Horizontal rotation |
| **Tilt Servo (SG90)** | Pin 10 | Vertical elevation |
| **Targeting Laser** | Pin 13 | 5V Laser Diode |
| **Piezo Siren Buzzer** | Pin 11 | Audio alert speaker |
| **16x2 LCD (I2C)** | SDA (A4), SCL (A5) | I2C Telemetry Screen (Address `0x27`) |

---

## ⌨️ Live Demo Hotkey Controls

| Key | Function | LCD Display |
| :---: | :--- | :--- |
| **`1`** | Track Red Color Profile | `MODE:RED SCAN` |
| **`2`** | Track Green Color Profile | `MODE:GREEN SCAN` |
| **`3`** | Track Blue Color Profile | `MODE:BLUE SCAN` |
| **`F`** | Enable Human Face Lock Mode | `MODE:FACE LOCK` |
| **`M`** | Enable Manual WASD Control | `MODE:MANUAL CTRL` |
| **`B`** | Trigger Audio/Visual Breach Siren | `!! WARNING !!` |
| **`Q`** | Quit Vision System Safely | — |

---

## 🛠️ Project Setup Checklist

- [ ] Assemble 2-DOF pan-tilt bracket with 2x SG90 servos.
- [ ] Wire servos, laser, buzzer, and I2C LCD to Arduino.
- [ ] Connect Arduino via USB and flash `sentry_arduino.ino`.
- [ ] Install Python dependencies: `pip install opencv-python pyserial numpy`.
- [ ] Update `SERIAL_PORT` in `sentry_vision.py` to match your COM port.
- [ ] Run `sentry_vision.py` on your laptop and test live tracking!
