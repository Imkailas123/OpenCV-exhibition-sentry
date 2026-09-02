/*
 * ============================================================================
 * PROJECT: OpenCV-exhibition-sentry
 * HARDWARE: Arduino Uno/Nano, 2x SG90 Servos, 16x2 I2C LCD, 5V Laser, Buzzer
 * FEATURES INCLUDED:
 *  1. Priority State Machine (Breach Alert > Keypress Mode > Track Lock > Patrol)
 *  2. Non-Flickering 2-Line Telemetry (Padded string layout, formatted angles)
 *  3. Mechanical Safety Angle Limits (Pan: 10-170°, Tilt: 40-140°)
 *  4. USB Power-Safe Smooth Stepping (10ms step delay prevents brownout restarts)
 *  5. Visual/Audio Breach Alert (Strobe laser + 2.5kHz piezo siren on key 'B')
 *  6. Automatic Patrol Sweep (Sweeps side-to-side on 3s target loss)
 *  7. Full Profile Tracking Display (Target Red/Green/Blue, Face Lock, Manual)
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// ----------------------------------------------------------------------------
// 1. HARDWARE PIN DEFINITIONS
// ----------------------------------------------------------------------------
const int PAN_PIN = 9;       // Base horizontal servo pin
const int TILT_PIN = 10;     // Vertical elevation servo pin
const int LASER_PIN = 13;    // Target laser output pin
const int BUZZER_PIN = 11;   // Piezo siren output pin

// ----------------------------------------------------------------------------
// 2. HARDWARE OBJECT INITIALIZATION
// ----------------------------------------------------------------------------
Servo panServo;
Servo tiltServo;
LiquidCrystal_I2C lcd(0x27, 16, 2); // Address 0x27, 16 Columns, 2 Rows

// ----------------------------------------------------------------------------
// 3. FEATURE: MECHANICAL SAFETY ANGLE LIMITS & SERVO TRACKING
// Prevents servos from forcing against internal plastic stops (0° or 180°),
// avoiding stall current spikes (>600mA) that trip USB ports.
// ----------------------------------------------------------------------------
const int MIN_PAN = 10;
const int MAX_PAN = 170;
const int MIN_TILT = 40;
const int MAX_TILT = 140;

int currentPan = 90;   // Live pan position (Initialized at center)
int currentTilt = 90;  // Live tilt position (Initialized at center)

// ----------------------------------------------------------------------------
// 4. FEATURE: AUTOMATIC PATROL SWEEP & TIMEOUT CONTROL
// ----------------------------------------------------------------------------
int patrolPan = 90;
int patrolDirection = 1;         // 1 = Move Right, -1 = Move Left
unsigned long lastPatrolStep = 0;
const int PATROL_SPEED = 35;     // Step interval (ms) for smooth scan

unsigned long lastTargetTime = 0;
bool targetAcquired = false;
String activeProfileText = "RED"; // Tracks selected Python profile (RED/GREEN/BLUE/FACE/MANUAL)

// ----------------------------------------------------------------------------
// 5. SETUP FUNCTION
// ----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200); // High-speed serial link with Python OpenCV
  
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  
  pinMode(LASER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LASER_PIN, HIGH); // Laser ON by default
  
  // Center servos safely on startup
  panServo.write(currentPan);
  tiltServo.write(currentTilt);
  
  // Initialize LCD display
  lcd.init();
  lcd.backlight();
  
  // Boot screen telemetry initialization
  updateLCD("STATE: PATROL", "MODE:RED SCAN");
}

// ----------------------------------------------------------------------------
// 6. MAIN LOOP (PRIORITY STATE MACHINE EXECUTION)
// ----------------------------------------------------------------------------
void loop() {
  
  // ==========================================================================
  // PRIORITY LEVEL 1, 2 & 3: PARSE SERIAL COMMANDS FROM PYTHON
  // ==========================================================================
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    // --- PRIORITY 1: AUDIO/VISUAL BREACH ALERT (Key 'B' pressed in Python) ---
    if (data == "ALERT:BREACH") {
      triggerBreachAlert();
    }
    
    // --- PRIORITY 2: MODE & COLOR SWITCHING CONFIRMATION (Keys 1, 2, 3, F, M) ---
    else if (data.startsWith("MODE:")) {
      activeProfileText = data.substring(5); // Extracts "RED", "GREEN", "BLUE", "FACE LOCK", etc.
      updateLCD("MODE SWITCHED", activeProfileText);
      delay(500); // Brief hold so state confirmation is visible on LCD
    } 
    
    // --- PRIORITY 3: ACTIVE TARGET COORDINATE LOCK (Serial String format "P90T100") ---
    else {
      int pIndex = data.indexOf('P');
      int tIndex = data.indexOf('T');
      
      if (pIndex != -1 && tIndex != -1) {
        int targetPan = data.substring(pIndex + 1, tIndex).toInt();
        int targetTilt = data.substring(tIndex + 1).toInt();
        
        // ENFORCE FEATURE 3: Safety Angle Limits
        targetPan = constrain(targetPan, MIN_PAN, MAX_PAN);
        targetTilt = constrain(targetTilt, MIN_TILT, MAX_TILT);
        
        // ENFORCE FEATURE 4: USB-Safe Smooth Stepping (10ms step delay)
        smoothMove(targetPan, targetTilt, 10);
        
        lastTargetTime = millis();
        targetAcquired = true;
        
        // ENFORCE FEATURE 2 & 7: Telemetry LCD formatting with active profile
        updateLCD("LOCK: " + activeProfileText, "P:" + formatAngle(targetPan) + " T:" + formatAngle(targetTilt) + " TRK");
      }
    }
  }

  // ==========================================================================
  // PRIORITY LEVEL 4: AUTOMATIC CONTINUOUS PATROL SWEEP (WHEN NO TARGET LOCKED)
  // Executes side-to-side scanning when Python is not sending target coordinates.
  // ==========================================================================
  if (!targetAcquired) {
    if (millis() - lastPatrolStep >= PATROL_SPEED) {
      lastPatrolStep = millis();
      
      // Sweep bounds horizontal (30° to 150°)
      patrolPan += patrolDirection;
      if (patrolPan >= 150) {
        patrolPan = 150;
        patrolDirection = -1; // Reverse left
      } else if (patrolPan <= 30) {
        patrolPan = 30;
        patrolDirection = 1;  // Reverse right
      }
      
      // Smooth step to patrol position while keeping tilt level (90°)
      smoothMove(patrolPan, 90, 0);
      
      // ENFORCE FEATURE 2 & 6: Clean telemetry output during active patrol scan
      updateLCD("STATE: PATROL", "MODE:" + activeProfileText + " SCAN");
    }
  }

  // ==========================================================================
  // AUTO-PATROL TIMEOUT RECOVERY
  // Reverts turret state back to PATROL if target is lost for >3 seconds.
  // ==========================================================================
  if (targetAcquired && (millis() - lastTargetTime > 3000)) {
    targetAcquired = false;
    patrolPan = currentPan; // Seamlessly resume sweeping from current position
  }
}

// ----------------------------------------------------------------------------
// 7. FUNCTION MODULES & HELPERS
// ----------------------------------------------------------------------------

/**
 * FEATURE 4: USB-Safe Smooth Stepping
 * Breaks continuous moves into 1-degree steps with delay to eliminate peak current spikes.
 */
void smoothMove(int targetPan, int targetTilt, int stepDelay) {
  if (currentPan != targetPan) {
    if (currentPan < targetPan) currentPan++;
    else if (currentPan > targetPan) currentPan--;
    panServo.write(currentPan);
  }
  
  if (currentTilt != targetTilt) {
    if (currentTilt < targetTilt) currentTilt++;
    else if (currentTilt > targetTilt) currentTilt--;
    tiltServo.write(currentTilt);
  }
  
  if (stepDelay > 0) {
    delay(stepDelay);
  }
}

/**
 * FEATURE 2: Clean 2-Line LCD Telemetry Formatting
 * Pads line strings with empty spaces to overwrite previous text without running clear screen.
 */
void updateLCD(String line1, String line2) {
  while (line1.length() < 16) line1 += " ";
  while (line2.length() < 16) line2 += " ";
  
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

/**
 * FEATURE 2 HELPER: Formats 1-3 digit numbers to fixed 3-digit strings ("009", "090").
 */
String formatAngle(int angle) {
  if (angle < 10) return "00" + String(angle);
  if (angle < 100) return "0" + String(angle);
  return String(angle);
}

/**
 * FEATURE 5: Visual/Audio Breach Alert Routine
 * Strobes the laser pin and sounds a 2.5kHz piezo buzzer tone.
 */
void triggerBreachAlert() {
  updateLCD("!! WARNING !!", "SYSTEM BREACH");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LASER_PIN, LOW);    // Laser OFF
    tone(BUZZER_PIN, 2500);          // Sound Piezo Siren
    delay(100);
    digitalWrite(LASER_PIN, HIGH);   // Laser ON
    noTone(BUZZER_PIN);              // Silent
    delay(100);
  }
}
