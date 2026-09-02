"""
============================================================================
PROJECT: Sentinel-CV Master Autonomous Turret Vision Software
DEPENDENCIES: OpenCV (cv2), PySerial, NumPy
HARDWARE: Laptop Camera / USB Webcam -> USB Serial -> Arduino Uno/Nano
FEATURES INCLUDED:
 1. Multi-Target Mode (Red, Green, Blue Color HSV Tracking + Face Lock)
 2. Live GUI HUD Overlay (Central crosshairs, target bounding boxes, telemetry)
 3. Keybind State Controller (1/2/3: Colors, F: Face, M: Manual WASD, B: Breach)
 4. Dynamic Angle Mapping (640x480 webcam pixels mapped to 10°-170° / 40°-140°)
 5. Auto Serial Reconnection & Synchronization with LCD Display
============================================================================
"""

import cv2
import numpy as np
import serial
import time

# ----------------------------------------------------------------------------
# 1. SERIAL COMMUNICATION INITIALIZATION
# Connects to Arduino over USB Serial at 115200 Baud Rate.
# ----------------------------------------------------------------------------
# NOTE FOR SCHOOL: Update 'COM3' to match your laptop's Device Manager port 
# (e.g., 'COM4' on Windows or '/dev/ttyUSB0' on Linux/Mac)
SERIAL_PORT = 'COM3'
BAUD_RATE = 115200

try:
    arduino = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.1)
    time.sleep(2)  # 2-second delay allowing Arduino to complete reboot sequence
    print(f"[SUCCESS] Connected to Arduino Sentry Turret on port {SERIAL_PORT}!")
except Exception as e:
    print(f"[WARNING] Serial Connection Error: {e}")
    print("[WARNING] Running in Simulation Mode (No physical hardware connected).")
    arduino = None

# ----------------------------------------------------------------------------
# 2. CAMERA SETUP & OPENCV INITIALIZATION
# ----------------------------------------------------------------------------
cap = cv2.VideoCapture(0)  # Standard internal laptop webcam (Index 0)

FRAME_WIDTH = 640
FRAME_HEIGHT = 480
cap.set(3, FRAME_WIDTH)
cap.set(4, FRAME_HEIGHT)

# Load OpenCV's pre-trained Haar Cascade Classifier for Human Face Detection
face_cascade = cv2.CascadeClassifier(cv2.data.haarcascades + 'haarcascade_frontalface_default.xml')

# ----------------------------------------------------------------------------
# 3. COLOR HSV TRACKING PROFILES
# Defines lower and upper boundaries for color thresholding in HSV space.
# ----------------------------------------------------------------------------
COLOR_PROFILES = {
    'RED': (np.array([0, 120, 70]), np.array([10, 255, 255])),
    'GREEN': (np.array([36, 100, 100]), np.array([86, 255, 255])),
    'BLUE': (np.array([94, 80, 2], dtype=np.uint8), np.array([126, 255, 255], dtype=np.uint8))
}

# ----------------------------------------------------------------------------
# 4. SYSTEM STATE & MANUAL CONTROL INITIALIZATION
# ----------------------------------------------------------------------------
current_mode = "RED"  # Initial Active Mode Options: RED, GREEN, BLUE, FACE, MANUAL
manual_pan = 90
manual_tilt = 90

def send_to_arduino(command_str):
    """
    Encodes and transmits string packets over Serial link to Arduino.
    Adds a newline char '\n' required by Serial.readStringUntil('\n').
    """
    if arduino and arduino.is_open:
        arduino.write((command_str + "\n").encode())

# ----------------------------------------------------------------------------
# 5. EXHIBITION USER INTERFACE DEMO INSTRUCTIONS
# ----------------------------------------------------------------------------
print("=================================================================")
print("           SENTINEL-CV TURRET MASTER CONTROLLER                  ")
print("=================================================================")
print(" DEMO HOTKEY CONTROLS:")
print("   [1] Switch to RED Object Tracking Mode")
print("   [2] Switch to GREEN Object Tracking Mode")
print("   [3] Switch to BLUE Object Tracking Mode")
print("   [F] Switch to FACE LOCK Mode (Haar Cascade Detection)")
print("   [M] Switch to MANUAL OVERRIDE (WASD Servo Keys)")
print("   [B] Trigger VISUAL/AUDIO BREACH ALERT (Strobe + Buzzer)")
print("   [Q] Quit System Safely")
print("=================================================================\n")

# ----------------------------------------------------------------------------
# 6. MAIN OPENCV COMPUTER VISION LOOP
# ----------------------------------------------------------------------------
while True:
    ret, frame = cap.read()
    if not ret:
        print("[ERROR] Camera frame capture failure. Check webcam connection.")
        break

    # Mirror video feed horizontally so movement feels natural
    frame = cv2.flip(frame, 1)
    hsv = cv2.cvtColor(frame, cv2.COLOR_BGR2HSV)
    center_x, center_y = FRAME_WIDTH // 2, FRAME_HEIGHT // 2

    # Draw Central Crosshair HUD
    cv2.line(frame, (center_x - 15, center_y), (center_x + 15, center_y), (255, 255, 255), 1)
    cv2.line(frame, (center_x, center_y - 15), (center_x, center_y + 15), (255, 255, 255), 1)

    target_found = False
    target_x, target_y = center_x, center_y

    # ========================================================================
    # MODE A: FACE TRACKING (HAAR CASCADE FACE DETECTION)
    # Executes when user presses key 'F'.
    # ========================================================================
    if current_mode == "FACE":
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
        faces = face_cascade.detectMultiScale(gray, scaleFactor=1.1, minNeighbors=5, minSize=(30, 30))
        
        if len(faces) > 0:
            # Select the largest face detected in view
            fx, fy, fw, fh = max(faces, key=lambda rect: rect[2] * rect[3])
            target_x = fx + (fw // 2)
            target_y = fy + (fh // 2)
            
            # Draw Face Box & Target Dot
            cv2.rectangle(frame, (fx, fy), (fx + fw, fy + fh), (255, 255, 0), 2)
            cv2.putText(frame, "HUMAN FACE DETECTED", (fx, fy - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 2)
            target_found = True

    # ========================================================================
    # MODE B: MANUAL WASD OVERRIDE
    # Executes when user presses key 'M'. Allows direct servo control.
    # ========================================================================
    elif current_mode == "MANUAL":
        target_found = False
        cv2.putText(frame, f"MANUAL CTRL: P:{manual_pan} T:{manual_tilt} (Use W/A/S/D)", (10, 60),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 0), 2)

    # ========================================================================
    # MODE C: COLOR TRACKING (RED / GREEN / BLUE)
    # Executes when user presses keys '1', '2', or '3'.
    # ========================================================================
    else:
        lower_bound, upper_bound = COLOR_PROFILES[current_mode]
        mask = cv2.inRange(hsv, lower_bound, upper_bound)
        contours, _ = cv2.findContours(mask, cv2.RETR_TREE, cv2.CHAIN_APPROX_SIMPLE)

        if contours:
            c = max(contours, key=cv2.contourArea)
            if cv2.contourArea(c) > 500:  # Filter out tiny pixel noise
                x, y, w, h = cv2.boundingRect(c)
                target_x = x + (w // 2)
                target_y = y + (h // 2)
                
                # Draw Color Bounding Box
                cv2.rectangle(frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                target_found = True

    # ========================================================================
    # COORDINATE MAPPING & SERIAL TRANSMISSION TO ARDUINO
    # Maps screen pixel targets (640x480) to physical servo angles (0°-180°).
    # ========================================================================
    if current_mode == "MANUAL":
        # Send direct manual angles
        send_to_arduino(f"P{manual_pan}T{manual_tilt}")
    elif target_found:
        cv2.circle(frame, (target_x, target_y), 5, (0, 0, 255), -1)
        
        # Linear Interpolation: 
        # Map X Pixel [0 -> 640] to Pan Angle [170° -> 10°] (Inverted X so tracking mirrors motion)
        # Map Y Pixel [0 -> 480] to Tilt Angle [40° -> 140°]
        pan_angle = int(np.interp(target_x, [0, FRAME_WIDTH], [170, 10]))
        tilt_angle = int(np.interp(target_y, [0, FRAME_HEIGHT], [40, 140]))
        
        # Transmit target command string (e.g., "P090T100")
        send_to_arduino(f"P{pan_angle}T{tilt_angle}")
        
        # Display On-Screen Target Telemetry
        cv2.putText(frame, f"LOCK [{current_mode}]: P:{pan_angle} T:{tilt_angle}", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
    else:
        # Inform user that system is in auto-patrol search mode
        cv2.putText(frame, f"MODE: {current_mode} - SEARCHING (PATROL ACTIVE)...", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 2)

    # Display HUD Window
    cv2.imshow("Sentinel-CV Sentry Turret HUD", frame)

    # ========================================================================
    # KEYBOARD INPUT INTERCEPTOR (LIVE DEMO CONTROLS)
    # ========================================================================
    key = cv2.waitKey(1) & 0xFF
    
    if key == ord('q'):
        print("[INFO] Shutting down Sentinel-CV system...")
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
        print("[ALERT] Breach Command Triggered!")
        send_to_arduino("ALERT:BREACH")
    
    # WASD Controls for Manual Mode Adjustment (5-degree step increments)
    if current_mode == "MANUAL":
        if key == ord('w'): manual_tilt = min(140, manual_tilt + 5)
        elif key == ord('s'): manual_tilt = max(40, manual_tilt - 5)
        elif key == ord('a'): manual_pan = min(170, manual_pan + 5)
        elif key == ord('d'): manual_pan = max(10, manual_pan - 5)

# ----------------------------------------------------------------------------
# 7. CLEANUP & SHUTDOWN
# ----------------------------------------------------------------------------
cap.release()
cv2.destroyAllWindows()
if arduino and arduino.is_open:
    arduino.close()
print("[INFO] Serial connection closed. System terminated successfully.")
