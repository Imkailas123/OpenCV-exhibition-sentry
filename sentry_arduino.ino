/*
 * ============================================================================
 * PROJECT: OpenCV-exhibition-sentry
 * HARDWARE: Arduino Uno/Nano, 2x SG90 Servos, 16x2 I2C LCD, 5V Laser, Buzzer, LED
 * FEATURES INCLUDED:
 *  1. Priority State Machine (Breach Alert > Shutdown > Proximity > Mode > Lock > Patrol)
 *  2. AUTOMATIC BREACH ALERT with non-blocking 5-second cooldown
 *  3. Automatic Patrol Sweep on target loss (3-second timeout)
 *  4. Non-Flickering 2-Line Telemetry
 *  5. Mechanical Safety Angle Limits (Pan: 10-170°, Tilt: 40-140°)
 *  6. USB Power-Safe Smooth Stepping (10ms step delay)
 *  7. Graceful Power-Down / Shutdown Routine (Key 'P')
 * ============================================================================
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Servo.h>

// 1. HARDWARE PIN DEFINITIONS
const int PAN_PIN = 9;
const int TILT_PIN = 10;
const int LASER_PIN = 13;
const int BUZZER_PIN = 11;
const int PROX_LED_PIN = 6;

// 2. HARDWARE OBJECT INITIALIZATION
Servo panServo;
Servo tiltServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 3. MECHANICAL SAFETY ANGLE LIMITS
const int MIN_PAN = 10;
const int MAX_PAN = 170;
const int MIN_TILT = 40;
const int MAX_TILT = 140;

int currentPan = 90;
int currentTilt = 90;

// 4. PATROL & TIMING VARIABLES
int patrolPan = 90;
int patrolDirection = 1;
unsigned long lastPatrolStep = 0;
const int PATROL_SPEED = 35;

unsigned long lastTargetTime = 0;
bool targetAcquired = false;
bool systemPoweredDown = false;
String activeProfileText = "RED";

// Proximity & Automatic Breach Cooldown
unsigned long lastBreachTime = 0;
const unsigned long BREACH_COOLDOWN = 5000; // 5 seconds between auto-sirens
unsigned long lastProxFlashTime = 0;
bool proxLedState = false;
bool isTooClose = false;
int currentBrightness = 10;

void setup() {
  Serial.begin(115200);
  
  panServo.attach(PAN_PIN);
  tiltServo.attach(TILT_PIN);
  
  pinMode(LASER_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PROX_LED_PIN, OUTPUT);
  
  digitalWrite(LASER_PIN, HIGH);
  analogWrite(PROX_LED_PIN, currentBrightness);
  
  panServo.write(currentPan);
  tiltServo.write(currentTilt);
  
  lcd.init();
  lcd.backlight();
  
  updateLCD("STATE: PATROL", "MODE:RED SCAN");
}

void loop() {
  
  // ==========================================================================
  // SERIAL COMMAND PARSER
  // ==========================================================================
  if (Serial.available() > 0) {
    String data = Serial.readStringUntil('\n');
    data.trim();

    // --- PRIORITY 1: SHUTDOWN (Key 'P') ---
    if (data == "SYS:POWERDOWN") {
      executeShutdownSequence();
      return;
    }

    systemPoweredDown = false;

    // --- PRIORITY 2: BREACH ALERT (Auto-triggered or Key 'B') ---
    if (data == "ALERT:BREACH") {
      if (millis() - lastBreachTime > BREACH_COOLDOWN) {
        lastBreachTime = millis();
        triggerBreachAlert();
      }
    }
    
    // --- PRIORITY 3: PROXIMITY WARNING ---
    else if (data == "ALERT:CLOSE") {
      isTooClose = true;
      updateLCD("WARNING: CLOSE!", "PROXIMITY BREACH");
    }
    
    // --- PRIORITY 4: PROXIMITY DISTANCE VALUE ---
    else if (data.startsWith("DIST:")) {
      isTooClose = false;
      int distValue = data.substring(5).toInt();
      currentBrightness = constrain(distValue, 10, 255);
      analogWrite(PROX_LED_PIN, currentBrightness);
    }
    
    // --- PRIORITY 5: MODE SWITCHING ---
    else if (data.startsWith("MODE:")) {
      isTooClose = false;
      currentBrightness = 10;
      analogWrite(PROX_LED_PIN, currentBrightness);
      activeProfileText = data.substring(5);
      updateLCD("MODE SWITCHED", activeProfileText);
      delay(500);
    } 
    
    // --- PRIORITY 6: TARGET LOCK ("P90T100") ---
    else {
      int pIndex = data.indexOf('P');
      int tIndex = data.indexOf('T');
      
      if (pIndex != -1 && tIndex != -1) {
        int targetPan = data.substring(pIndex + 1, tIndex).toInt();
        int targetTilt = data.substring(tIndex + 1).toInt();
        
        targetPan = constrain(targetPan, MIN_PAN, MAX_PAN);
        targetTilt = constrain(targetTilt, MIN_TILT, MAX_TILT);
        
        smoothMove(targetPan, targetTilt, 10);
        
        lastTargetTime = millis();
        targetAcquired = true;
        
        if (!isTooClose) {
          updateLCD("LOCK: " + activeProfileText, "P:" + formatAngle(targetPan) + " T:" + formatAngle(targetTilt) + " TRK");
        }
      }
    }
  }

  if (systemPoweredDown) return;

  // ==========================================================================
  // NON-BLOCKING PROXIMITY LED BLINKING
  // ==========================================================================
  if (isTooClose) {
    if (millis() - lastProxFlashTime >= 100) {
      lastProxFlashTime = millis();
      proxLedState = !proxLedState;
      analogWrite(PROX_LED_PIN, proxLedState ? 255 : 0);
    }
  }

  // ==========================================================================
  // AUTOMATIC PATROL SWEEP
  // ==========================================================================
  if (!targetAcquired) {
    if (millis() - lastPatrolStep >= PATROL_SPEED) {
      lastPatrolStep = millis();
      
      patrolPan += patrolDirection;
      if (patrolPan >= 150) {
        patrolPan = 150;
        patrolDirection = -1;
      } else if (patrolPan <= 30) {
        patrolPan = 30;
        patrolDirection = 1;
      }
      
      smoothMove(patrolPan, 90, 0);
      
      if (!isTooClose) {
        updateLCD("STATE: PATROL", "MODE:" + activeProfileText + " SCAN");
      }
    }
  }

  // ==========================================================================
  // AUTO-PATROL TIMEOUT RECOVERY (3 Seconds)
  // ==========================================================================
  if (targetAcquired && (millis() - lastTargetTime > 3000)) {
    targetAcquired = false;
    isTooClose = false;
    currentBrightness = 10;
    analogWrite(PROX_LED_PIN, currentBrightness);
    patrolPan = currentPan;
  }
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

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

void triggerBreachAlert() {
  updateLCD("!! WARNING !!", "SYSTEM BREACH");
  for (int i = 0; i < 5; i++) {
    digitalWrite(LASER_PIN, LOW);
    digitalWrite(PROX_LED_PIN, LOW);
    tone(BUZZER_PIN, 2500);
    delay(100);
    digitalWrite(LASER_PIN, HIGH);
    digitalWrite(PROX_LED_PIN, HIGH);
    noTone(BUZZER_PIN);
    delay(100);
  }
  analogWrite(PROX_LED_PIN, currentBrightness);
}

void executeShutdownSequence() {
  systemPoweredDown = true;
  updateLCD("POWERING DOWN...", "STANDBY MODE");
  
  digitalWrite(LASER_PIN, LOW);
  analogWrite(PROX_LED_PIN, 0);
  
  while (currentPan != 90 || currentTilt != MIN_TILT) {
    smoothMove(90, MIN_TILT, 15);
  }
  
  updateLCD("SYS: OFFLINE", "CONN TERMINATED");
}
