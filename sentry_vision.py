"""
============================================================================
PROJECT: Jarvis-CV / Sentinel-CV Master Autonomous Turret Vision Software
HARDWARE INTERFACE: Fully Compatible with Master Arduino Firmware
TRACKING ENGINE: MediaPipe Palm/Hand Landmark Engine & Color HSV Profiler
============================================================================
"""

import cv2
import numpy as np
import serial
import time
import mediapipe as mp

SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

# 1. SERIAL CONNECTION INITIALIZATION
try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)
    print(f"[SUCCESS] Connected to Arduino Sentry Turret on port {SERIAL_PORT}!")
except Exception as e:
    print(f"[WARNING] Serial Connection Error: {e}")
    print("[WARNING] Running in Simulation Mode.")
    arduino = None

# 2. CAMERA SETUP
cap = cv2.VideoCapture(0)
FRAME_WIDTH = 640
FRAME_HEIGHT = 480
cap.set(cv2.CAP_PROP_FRAME_WIDTH, FRAME_WIDTH)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, FRAME_HEIGHT)

# 3. MEDIAPIPE HAND/PALM DETECTOR SETUP
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(
    static_image_mode=False,
    max_num_hands=1,
    min_detection_confidence=0.7,
    min_tracking_confidence=0.7
)
mp_draw = mp.solutions.drawing_utils

# 4. COLOR PROFILES (HSV)
COLOR_PROFILES = {
    'RED': (np.array([0, 120, 70]), np.array([10, 255, 255])),
    'GREEN': (np.array([36, 100, 100]), np.array([86, 255, 255])),
    'BLUE': (np.array([94, 80, 2], dtype=np.uint8), np.array([126, 255, 255], dtype=np.uint8))
}

current_mode = "RED"  # Options: RED, GREEN, BLUE, PALM, MANUAL, CALIB

# Proximity limits based on contour/bounding area
MIN_AREA = 2000
MAX_AREA = 40000

def send_to_arduino(command_str):
    if arduino and arduino.is_open:
        arduino.write((command_str + "\n").encode('utf-8'))

print("=================================================================")
print("          JARVIS-CV / SENTINEL-CV MASTER TURRET HUD              ")
print("=================================================================")
print(" HOTKEY CONTROLS:")
print("   [1] RED Target Mode  | [2] GREEN Target Mode  | [3] BLUE Target Mode")
print("   [F] Palm Track Mode  | [M] Manual Override    | [C] Auto-Calibrate Mode")
print("   [P] Power System On/Off                       | [Q] Quit Program")
print("=================================================================\n")

while True:
    ret, frame = cap.read()
    if not ret:
        print("[ERROR] Camera frame capture failure.")
        break

    frame = cv2.flip(frame, 1) # Mirror camera frame
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    center_x, center_y = FRAME_WIDTH // 2, FRAME_HEIGHT // 2

    # HUD Target Crosshairs
    cv2.line(frame, (center_x - 15, center_y), (center_x + 15, center_y), (255, 255, 255), 1)
    cv2.line(frame, (center_x, center_y - 15), (center_x, center_y + 15), (255, 255, 255), 1)

    target_found = False
    target_x, target_y = center_x, center_y
    proximity_pct = 0

    # MODE A: PALM / HAND TRACKING (MediaPipe)
    if current_mode == "PALM":
        rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
        results = hands.process(rgb_frame)

        if results.multi_hand_landmarks:
            for hand_landmarks in results.multi_hand_landmarks:
                mp_draw.draw_landmarks(frame, hand_landmarks, mp_hands.HAND_CONNECTIONS)

                # Calculate hand bounding box
                x_max, y_max = 0, 0
                x_min, y_min = FRAME_WIDTH, FRAME_HEIGHT
                for lm in hand_landmarks.landmark:
                    cx, cy = int(lm.x * FRAME_WIDTH), int(lm.y * FRAME_HEIGHT)
                    x_min, x_max = min(x_min, cx), max(x_max, cx)
                    y_min, y_max = min(y_min, cy), max(y_max, cy)

                target_x = (x_min + x_max) // 2
                target_y = (y_min + y_max) // 2
                
                # Area estimation based on bounding box
                box_area = (x_max - x_min) * (y_max - y_min)
                proximity_pct = int(np.interp(box_area, [MIN_AREA, MAX_AREA], [0, 100]))

                cv2.rectangle(frame, (x_min, y_min), (x_max, y_max), (255, 255, 0), 2)
                cv2.putText(frame, "PALM LOCK ENGAGED", (x_min, y_min - 10),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)
                target_found = True
                break

    # MODE B: MANUAL OVERRIDE MODE
    elif current_mode == "MANUAL":
        target_found = False
        cv2.putText(frame, "MANUAL OVERRIDE: CONTROL ACTIVE (WASD)", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

    # MODE C: COLOR TRACKING PROFILES (RED / GREEN / BLUE)
    elif current_mode in COLOR_PROFILES:
        lower_bound, upper_bound = COLOR_PROFILES[current_mode]
        mask = cv2.inRange(hsv, lower_bound, upper_bound)
        contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

        if contours:
            c = max(contours, key=cv2.contourArea)
            area = cv2.contourArea(c)
            if area > 500:
                x, y, w, h = cv2.boundingRect(c)
                target_x = x + (w // 2)
                target_y = y + (h // 2)
                
                proximity_pct = int(np.interp(area, [MIN_AREA, MAX_AREA], [0, 100]))

                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                target_found = True

    # SERIAL TELEMETRY DISPATCH (Format: "PAN,TILT,PROXIMITY")
    if current_mode != "MANUAL":
        if target_found:
            cv2.circle(frame, (target_x, target_y), 5, (0, 0, 255), -1)

            # Map camera frame pixels to Arduino Servo Angle Limits
            pan_angle = int(np.interp(target_x, [0, FRAME_WIDTH], [170, 10]))
            tilt_angle = int(np.interp(target_y, [0, FRAME_HEIGHT], [40, 140]))

            send_to_arduino(f"{pan_angle},{tilt_angle},{proximity_pct}")

            cv2.putText(frame, f"LOCK [{current_mode}]: P:{pan_angle} T:{tilt_angle} D:{proximity_pct}%", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        else:
            send_to_arduino("LOST")
            cv2.putText(frame, f"MODE: {current_mode} - SEARCHING...", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

    cv2.imshow("Jarvis-CV Master HUD", frame)

    # KEYBOARD INPUT INTERFACE
    key = cv2.waitKey(1) & 0xFF

    if key == ord('q'):
        print("[INFO] Shutting down Python Vision Software...")
        break
    elif key == ord('p'):
        print("[COMMAND] System Power Toggle Sent.")
        send_to_arduino("P")
    elif key == ord('1'):
        current_mode = "RED"
        send_to_arduino("1")
    elif key == ord('2'):
        current_mode = "GREEN"
        send_to_arduino("2")
    elif key == ord('3'):
        current_mode = "BLUE"
        send_to_arduino("3")
    elif key == ord('f'):
        current_mode = "PALM"
        send_to_arduino("F")
    elif key == ord('c'):
        current_mode = "CALIB"
        send_to_arduino("C")
    elif key == ord('m'):
        if current_mode == "MANUAL":
            current_mode = "RED"
        else:
            current_mode = "MANUAL"
        send_to_arduino("M")

    # MANUAL WASD DIRECTIONAL CONTROL PASS-THROUGH
    if current_mode == "MANUAL":
        if key == ord('w'): send_to_arduino("W")
        elif key == ord('s'): send_to_arduino("S")
        elif key == ord('a'): send_to_arduino("A")
        elif key == ord('d'): send_to_arduino("D")

# SHUTDOWN CLEANUP
cap.release()
cv2.destroyAllWindows()
if arduino and arduino.is_open:
    arduino.close()
print("[INFO] Serial connection closed. Vision System terminated.")
