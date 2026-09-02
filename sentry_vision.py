"""
============================================================================
PROJECT: Sentinel-CV Master Autonomous Turret Vision Software
AUTOMATIC BREACH TRIGGER: Automatically fires ALERT:BREACH when a target
crosses the critical proximity threshold (TOO_CLOSE_AREA).
============================================================================
"""

import cv2
import numpy as np
import serial
import time

SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)
    print(f"[SUCCESS] Connected to Arduino Sentry Turret on port {SERIAL_PORT}!")
except Exception as e:
    print(f"[WARNING] Serial Connection Error: {e}")
    print("[WARNING] Running in Simulation Mode.")
    arduino = None

cap = cv2.VideoCapture(0)

FRAME_WIDTH = 640
FRAME_HEIGHT = 480
cap.set(3, FRAME_WIDTH)
cap.set(4, FRAME_HEIGHT)

face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

COLOR_PROFILES = {
    'RED': (np.array([0, 120, 70]), np.array([10, 255, 255])),
    'GREEN': (np.array([36, 100, 100]), np.array([86, 255, 255])),
    'BLUE': (np.array([94, 80, 2], dtype=np.uint8), np.array([126, 255, 255], dtype=np.uint8))
}

current_mode = "RED"
manual_pan = 90
manual_tilt = 90

MIN_AREA = 1000
MAX_AREA = 35000
TOO_CLOSE_AREA = 40000  # Threshold to trigger automatic breach alert

def send_to_arduino(command_str):
    if arduino and arduino.is_open:
        arduino.write((command_str + "\n").encode())

print("=================================================================")
print("           SENTINEL-CV TURRET MASTER CONTROLLER                  ")
print("=================================================================")
print(" HOTKEY CONTROLS:")
print("   [1] RED Target Mode  | [2] GREEN Target Mode  | [3] BLUE Target Mode")
print("   [F] Face Lock Mode   | [M] Manual Override    | [B] Manual Breach Test")
print("   [P] POWER DOWN SYSTEM                         | [Q] Quit Program")
print("=================================================================\n")

while True:
    ret, frame = cap.read()
    if not ret:
        print("[ERROR] Camera frame capture failure.")
        break

    frame = cv2.flip(frame, 1)
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    center_x, center_y = FRAME_WIDTH // 2, FRAME_HEIGHT // 2

    # HUD Crosshairs
    cv2.line(frame, (center_x - 15, center_y), (center_x + 15, center_y), (255, 255, 255), 1)
    cv2.line(frame, (center_x, center_y - 15), (center_x, center_y + 15), (255, 255, 255), 1)

    target_found = False
    target_x, target_y = center_x, center_y
    target_area = 0

    # MODE A: FACE LOCK
    if current_mode == "FACE":
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
        
        if len(faces) > 0:
            fx, fy, fw, fh = max(faces, key=lambda rect: rect[2] * rect[3])
            target_x = fx + (fw // 2)
            target_y = fy + (fh // 2)
            target_area = fw * fh
            
            cv2.rectangle(frame, (fx, fy), (fx + fw, fy + fh), (255, 255, 0), 2)
            cv2.putText(frame, "HUMAN FACE DETECTED", (fx, fy - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)
            target_found = True

    # MODE B: MANUAL OVERRIDE
    elif current_mode == "MANUAL":
        target_found = False
        cv2.putText(frame, f"MANUAL CTRL: P:{manual_pan} T:{manual_tilt} (WASD)", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

    # MODE C: COLOR TRACKING
    else:
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
                target_area = area
                
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                target_found = True

    # PROXIMITY EVALUATION & AUTOMATIC BREACH TRIGGER
    if current_mode == "MANUAL":
        send_to_arduino(f"P{manual_pan}T{manual_tilt}")
    elif target_found:
        cv2.circle(frame, (target_x, target_y), 5, (0, 0, 255), -1)
        
        pan_angle = int(np.interp(target_x, [0, FRAME_WIDTH], [170, 10]))
        tilt_angle = int(np.interp(target_y, [0, FRAME_HEIGHT], [40, 140]))
        
        send_to_arduino(f"P{pan_angle}T{tilt_angle}")
        
        # AUTOMATIC BREACH CONDITION
        if target_area >= TOO_CLOSE_AREA:
            send_to_arduino("ALERT:BREACH")  # Automatically trigger siren & laser strobe
            cv2.putText(frame, "!! AUTOMATIC BREACH TRIGGERED !!", (10, 60),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)
        else:
            brightness = int(np.interp(target_area, [MIN_AREA, MAX_AREA], [10, 255]))
            send_to_arduino(f"DIST:{brightness}")
        
        cv2.putText(frame, f"LOCK [{current_mode}]: P:{pan_angle} T:{tilt_angle}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    else:
        cv2.putText(frame, f"MODE: {current_mode} - SEARCHING (PATROL ACTIVE)...", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

    cv2.imshow("Sentinel-CV Sentry Turret HUD", frame)

    # KEYBOARD CONTROLS
    key = cv2.waitKey(1) & 0xFF
    
    if key == ord('q'):
        print("[INFO] Shutting down Sentinel-CV system...")
        break
    elif key == ord('p'):
        print("[INFO] Powering down Sentry Turret...")
        send_to_arduino("SYS:POWERDOWN")
        time.sleep(1.5)
        break
    elif key == ord('1'):
        current_mode = "RED"
        send_to_arduino("MODE:TARGET RED")
    elif key == ord('2'):
        current_mode = "GREEN"
        send_to_arduino("MODE:TARGET GREEN")
    elif key == ord('3'):
        current_mode = "BLUE"
        send_to_arduino("MODE:TARGET BLUE")
    elif key == ord('f'):
        current_mode = "FACE"
        send_to_arduino("MODE:FACE LOCK")
    elif key == ord('m'):
        current_mode = "MANUAL"
        send_to_arduino("MODE:MANUAL CTRL")
    elif key == ord('b'):
        print("[ALERT] Manual Breach Command Triggered!")
        send_to_arduino("ALERT:BREACH")
    
    if current_mode == "MANUAL":
        if key == ord('w'): manual_tilt = min(140, manual_tilt + 5)
        elif key == ord('s'): manual_tilt = max(40, manual_tilt - 5)
        elif key == ord('a'): manual_pan = min(170, manual_pan + 5)
        elif key == ord('d'): manual_pan = max(10, manual_pan - 5)

cap.release()
cv2.destroyAllWindows()
if arduino and arduino.is_open:
    arduino.close()
print("[INFO] Serial connection closed. System terminated successfully.")
