 * ============================================================================
 * PROJECT: Jarvis-CV v1 Master Controller
 * HARDWARE: Arduino Uno, 2x SG90 Servos, 16x2 I2C LCD, Buzzer, LED
 * ============================================================================
 *
 * MASTER KEYBIND DIRECTORY (Sent via Serial / Python CV Script):
 * ----------------------------------------------------------------------------
 *  [P] / [p]   : Toggle System Power (Runs Startup / Shutdown Sequences)
 *  [1]         : Switch to RED COLOR TRACKING Profile ("MODE: RED SCAN")
 *  [2]         : Switch to GREEN COLOR TRACKING Profile ("MODE: GREEN SCAN")
 *  [3]         : Switch to BLUE COLOR TRACKING Profile ("MODE: BLUE SCAN")
 *  [F] / [f]   : Switch to FACE TRACKING Mode ("MODE: FACE RECOG")
 *  [M] / [m]   : TOGGLE MANUAL OVERRIDE Mode (Pressing 'm' toggles On/Off)
 *  [C] / [c]   : Switch to AUTO-CALIBRATION Mode ("MODE: CALIBRATING")
 *
 * MANUAL OVERRIDE CONTROLS (Active only when Manual Mode is ON):
 * ----------------------------------------------------------------------------
 *  [W] / [w]   : Tilt UP   (+5 degrees)
 *  [S] / [s]   : Tilt DOWN (-5 degrees)
 *  [A] / [a]   : Pan LEFT  (-5 degrees)
 *  [D] / [d]   : Pan RIGHT (+5 degrees)
 *
 * INCOMING TRACKING DATA FORMAT (From OpenCV Script):
 * ----------------------------------------------------------------------------
 *  Format      : "PAN,TILT,PROXIMITY" (e.g., "90,100,45")
 *  Target Lost : "LOST" (Reverts system to Patrol/Scan state)
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// 1. HARDWARE PINS
const int BUZZER_PIN = 5;   // PWM Pin for pitch control
const int PROX_LED_PIN = 6; // PWM Pin for brightness control
const int PAN_PIN = 9;
const int TILT_PIN = 10;

// 2. OBJECTS & LCD
Servo panServo;
Servo tiltServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 3. MECHANICAL LIMITS
const int MIN_PAN = 10;
const int MAX_PAN = 170;
const int MIN_TILT = 40;
const int MAX_TILT = 140;

int currentPan = 90;
int currentTilt = 90;
int currentProximity = 0; // Tracks live distance percentage in manual mode

// 4. PATROL & TIMING
int patrolPan = 90;
int patrolDirection = 1;
unsigned long lastPatrolStep = 0;
const int PATROL_SPEED = 30;

unsigned long lastTargetTime = 0;
bool targetAcquired = false;
bool systemPoweredDown = true; // Starts OFFLINE on plug-in
bool isManualMode = false;

// 5. PROFILE MODE TRACKING TEXT
String activeProfileText = "MODE: RED SCAN"; 

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PROX_LED_PIN, OUTPUT);

  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  panServo.write(currentPan);
  tiltServo.write(MIN_TILT); // Stowed rest position

  lcd.init();
  lcd.backlight();

  // STANDBY UPON PLUG-IN
  updateLCD("Jarvis-CV v1", "SYS: OFFLINE");
}

void loop() {
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    // --- 1. POWER TOGGLE (KEY: P) ---
    if (data == "p" || data == "P" || data == "SYS:POWERDOWN") {
      if (systemPoweredDown) {
        executeStartupSequence();
      } else {
        executeShutdownSequence();
      }
      return;
    }

    if (systemPoweredDown) return; // Ignore inputs if system is powered off

    // --- 2. COLOR TRACKING PROFILES (KEYS: 1, 2, 3) ---
    if (data == "1") {
      isManualMode = false;
      activeProfileText = "MODE: RED SCAN";
      confirmProfileChange();
      return;
    }
    else if (data == "2") {
      isManualMode = false;
      activeProfileText = "MODE: GREEN SCAN";
      confirmProfileChange();
      return;
    }
    else if (data == "3") {
      isManualMode = false;
      activeProfileText = "MODE: BLUE SCAN";
      confirmProfileChange();
      return;
    }

    // --- 3. FACE RECOGNITION MODE (KEY: F) ---
    else if (data == "f" || data == "F") {
      isManualMode = false;
      activeProfileText = "MODE: FACE RECOG";
      confirmProfileChange();
      return;
    }

    // --- 4. AUTO-CALIBRATION MODE (KEY: C) ---
    else if (data == "c" || data == "C") {
      isManualMode = false;
      activeProfileText = "MODE: CALIBRATING";
      confirmProfileChange();
      return;
    }

    // --- 5. MANUAL OVERRIDE TOGGLE (KEY: M) ---
    else if (data == "m" || data == "M") {
      isManualMode = !isManualMode; // Toggle Manual Mode ON/OFF
      
      if (isManualMode) {
        activeProfileText = "STATUS:Manual";
      } else {
        activeProfileText = "MODE: RED SCAN"; // Returns to Auto Patrol Mode
      }
      confirmProfileChange();
      return;
    }

    // --- 6. MANUAL DIRECTION CONTROLS (KEYS: W, A, S, D) ---
    if (isManualMode) {
      if (data == "w" || data == "W") { // Tilt UP
        currentTilt = constrain(currentTilt + 5, MIN_TILT, MAX_TILT);
        tiltServo.write(currentTilt);
      }
      else if (data == "s" || data == "S") { // Tilt DOWN
        currentTilt = constrain(currentTilt - 5, MIN_TILT, MAX_TILT);
        tiltServo.write(currentTilt);
      }
      else if (data == "a" || data == "A") { // Pan LEFT
        currentPan = constrain(currentPan - 5, MIN_PAN, MAX_PAN);
        panServo.write(currentPan);
      }
      else if (data == "d" || data == "D") { // Pan RIGHT
        currentPan = constrain(currentPan + 5, MIN_PAN, MAX_PAN);
        panServo.write(currentPan);
      }

      // Update screen in manual mode matching tracking telemetry layout
      updateLCD("STATUS:Manual", "P:" + formatAngle(currentPan) + " T:" + formatAngle(currentTilt) + " D:" + String(currentProximity) + "%");
      return;
    }

    // --- 7. COMPUTER VISION TRACKING PARSER ---
    if (data == "LOST") {
      targetAcquired = false;
      currentProximity = 0;
      analogWrite(PROX_LED_PIN, 0);
      noTone(BUZZER_PIN);
    } 
    else {
      // Parse "PAN,TILT,PROXIMITY"
      int firstComma = data.indexOf(',');
      int secondComma = data.indexOf(',', firstComma + 1);

      if (firstComma > 0 && secondComma > firstComma) {
        int targetPan = data.substring(0, firstComma).toInt();
        int targetTilt = data.substring(firstComma + 1, secondComma).toInt();
        int proximity = data.substring(secondComma + 1).toInt();

        targetPan = constrain(targetPan, MIN_PAN, MAX_PAN);
        targetTilt = constrain(targetTilt, MIN_TILT, MAX_TILT);
        proximity = constrain(proximity, 0, 100);
        currentProximity = proximity; // Save distance value

        if (!isManualMode) {
          smoothMove(targetPan, targetTilt);
        }

        lastTargetTime = millis();
        targetAcquired = true;

        // BREACH ALERT STATE (>= 85%)
        if (proximity >= 85) {
          digitalWrite(PROX_LED_PIN, HIGH);
          tone(BUZZER_PIN, 2500);
          updateLCD("STATUS: Breach", "Proximity Alert");
        } 
        // TRACKING STATE
        else if (!isManualMode) {
          int brightness = map(proximity, 0, 84, 10, 255);
          int tonePitch = map(proximity, 0, 84, 400, 1800);

          analogWrite(PROX_LED_PIN, brightness);
          tone(BUZZER_PIN, tonePitch);

          updateLCD("STATUS: Tracking", "P:" + formatAngle(targetPan) + " T:" + formatAngle(targetTilt) + " D:" + String(proximity) + "%");
        }
      }
    }
  }

  if (systemPoweredDown || isManualMode) return;

  // --- AUTOMATIC PATROL STATE ---
  if (!targetAcquired) {
    if (millis() - lastPatrolStep >= PATROL_SPEED) {
      lastPatrolStep = millis();

      patrolPan += patrolDirection;
      if (patrolPan >= MAX_PAN - 20) patrolDirection = -1;
      else if (patrolPan <= MIN_PAN + 20) patrolDirection = 1;

      smoothMove(patrolPan, 90);
      updateLCD("STATUS: Patrol", activeProfileText);
    }
  }

  // Target loss timeout (3 Seconds)
  if (targetAcquired && (millis() - lastTargetTime > 3000)) {
    targetAcquired = false;
    currentProximity = 0;
    analogWrite(PROX_LED_PIN, 0);
    noTone(BUZZER_PIN);
  }
}

// --- HELPER FUNCTIONS ---

void confirmProfileChange() {
  if (systemPoweredDown) return;
  lcd.clear();
  updateLCD("PROFILE UPDATED", activeProfileText);
  tone(BUZZER_PIN, 1200, 100);
  delay(500);
  lcd.clear();
}

void smoothMove(int targetPan, int targetTilt) {
  if (currentPan < targetPan) currentPan++;
  else if (currentPan > targetPan) currentPan--;
  panServo.write(currentPan);

  if (currentTilt < targetTilt) currentTilt++;
  else if (currentTilt > targetTilt) currentTilt--;
  tiltServo.write(currentTilt);
}

void updateLCD(String line1, String line2) {
  while (line1.length() < 16) line1 += " ";
  while (line2.length() < 16) line2 += " ";
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

String formatAngle(int angle) {
  if (angle < 10) return "00" + String(angle);
  if (angle < 100) return "0" + String(angle);
  return String(angle);
}

// STARTUP SEQUENCE
void executeStartupSequence() {
  lcd.clear();
  updateLCD("Jarvis-CV v1", "Booting System");
  tone(BUZZER_PIN, 1000, 100);
  delay(1000);

  lcd.clear();
  updateLCD("Diagnostics...", "Servo Check");

  while (currentPan != 90 || currentTilt != 90) {
    smoothMove(90, 90);
    delay(10);
  }
  tone(BUZZER_PIN, 1500, 150);
  delay(1000);

  lcd.clear();
  updateLCD("Powering Up", "Standby Mode");
  tone(BUZZER_PIN, 2000, 300);
  delay(1000);

  systemPoweredDown = false;
}

// SHUTDOWN SEQUENCE
void executeShutdownSequence() {
  systemPoweredDown = true;
  isManualMode = false;
  analogWrite(PROX_LED_PIN, 0);
  noTone(BUZZER_PIN);

  lcd.clear();
  updateLCD("Powering Down", "Standby Mode");

  while (currentPan != 90 || currentTilt != MIN_TILT) {
    smoothMove(90, MIN_TILT);
    delay(10);
  }
  tone(BUZZER_PIN, 500, 400);
  delay(1000);

  lcd.clear();
  updateLCD("Jarvis-CV v1", "SYS: OFFLINE");
}
