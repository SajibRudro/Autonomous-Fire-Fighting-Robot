#include <Servo.h>
#include <SoftwareSerial.h>

// ===== SoftwareSerial =====
// ESP32 GPIO17 → Arduino Pin 10 (RX)
// Pin 11 খালি রাখো
SoftwareSerial espSerial(10, 11);

// ===== Flame Sensors =====
#define FLAME_LEFT   A0
#define FLAME_CENTER A1
#define FLAME_RIGHT  A2

// ===== Motors =====
#define MOTOR_L1 5
#define MOTOR_L2 6
#define MOTOR_R1 9
#define MOTOR_R2 11

// ===== Pump + Servo =====
#define RELAY_PIN 7
#define SERVO_PIN 3

Servo waterServo;

// ===== Speed =====
#define SPEED_MOVE  130
#define SPEED_TURN  110

// ===== Fire Threshold =====
#define FIRE_NEAR    400
#define FIRE_DETECT  900

// ===== Fire Confirmation =====
#define CONFIRM_COUNT 8
int fireConfirm = 0;

// ===== Turn timing =====
const int TURN_DURATION = 300;
bool turning = false;
unsigned long turnStartTime = 0;
String lastTurn = "";
unsigned long lastTurnTime = 0;
const int TURN_COOLDOWN = 500;

// ===== State =====
bool pumping = false;
bool manualPumping = false;
bool autoMode = true;
bool manualStopped = false;
String espCommand = "";

void setup() {
  // Battery connect এ চলা ঠেকাতে
  pinMode(MOTOR_L1, OUTPUT); digitalWrite(MOTOR_L1, LOW);
  pinMode(MOTOR_L2, OUTPUT); digitalWrite(MOTOR_L2, LOW);
  pinMode(MOTOR_R1, OUTPUT); digitalWrite(MOTOR_R1, LOW);
  pinMode(MOTOR_R2, OUTPUT); digitalWrite(MOTOR_R2, LOW);
  pinMode(RELAY_PIN, OUTPUT); digitalWrite(RELAY_PIN, HIGH);

  Serial.begin(9600);
  espSerial.begin(9600);

  waterServo.attach(SERVO_PIN);
  waterServo.write(90);
  delay(500);
  waterServo.detach();

  stopMotors();
  pumpOFF();

  delay(1000);
  Serial.println("FireBot Ready!");
}

void loop() {

  if (espSerial.available()) {
    espCommand = espSerial.readStringUntil('\n');
    espCommand.trim();
    Serial.println("CMD: " + espCommand);
  }

  int L = analogRead(FLAME_LEFT);
  int C = analogRead(FLAME_CENTER);
  int R = analogRead(FLAME_RIGHT);

  Serial.print("L:"); Serial.print(L);
  Serial.print(" C:"); Serial.print(C);
  Serial.print(" R:"); Serial.println(R);

  // ===== STOP =====
  if (espCommand == "STOP") {
    manualStopped = true;
    autoMode = false;
    stopMotors();
    pumpOFF();
    pumping = false;
    manualPumping = false;
    fireConfirm = 0;
    waterServo.detach();
    espCommand = "";
    Serial.println("STOPPED");
    return;
  }

  // ===== AUTO =====
  if (espCommand == "AUTO") {
    autoMode = true;
    manualStopped = false;
    manualPumping = false;
    fireConfirm = 0;
    espCommand = "";
    Serial.println("AUTO ON");
    return;
  }

  // ===== Manual =====
  if (espCommand == "FORWARD") {
    autoMode = false; manualStopped = false;
    moveForward(); espCommand = ""; return;
  }
  if (espCommand == "BACKWARD") {
    autoMode = false; manualStopped = false;
    moveBackward(); espCommand = ""; return;
  }
  if (espCommand == "TURN_LEFT") {
    autoMode = false; manualStopped = false;
    manualLeft(); espCommand = ""; return;
  }
  if (espCommand == "TURN_RIGHT") {
    autoMode = false; manualStopped = false;
    manualRight(); espCommand = ""; return;
  }
  if (espCommand == "PUMP_ON") {
    autoMode = false; manualStopped = false;
    manualPumping = true; pumping = true;
    pumpON();
    waterServo.attach(SERVO_PIN);
    manualServoPump();
    espCommand = ""; return;
  }
  if (espCommand == "PUMP_OFF") {
    manualPumping = false; pumping = false;
    pumpOFF();
    waterServo.write(90); delay(100);
    waterServo.detach();
    espCommand = ""; return;
  }

  if (manualStopped) { stopMotors(); pumpOFF(); return; }
  if (!autoMode) return;

  // ===== AUTO MODE =====
  bool anyFire = (L < FIRE_DETECT) || (C < FIRE_DETECT) || (R < FIRE_DETECT);

  if (anyFire) {
    if (fireConfirm < CONFIRM_COUNT) fireConfirm++;
  } else {
    fireConfirm = 0;
  }

  if (fireConfirm < CONFIRM_COUNT) {
    stopMotors();
    stopPumpIfRunning();
    Serial.println("IDLE");
    delay(100);
    return;
  }

  bool fireL = (L < FIRE_DETECT);
  bool fireC = (C < FIRE_DETECT);
  bool fireR = (R < FIRE_DETECT);
  bool fireClose = (C < FIRE_NEAR);

  // আগুন কাছে → থামো + pump
  if (fireClose) {
    stopMotors();
    if (!pumping) {
      pumping = true;
      Serial.println("FIRE CLOSE → PUMP ON");
      autoPumpUntilOut();
    }
  }
  // Center আগুন → সামনে
  else if (fireC && !fireClose) {
    stopPumpIfRunning();
    lastTurn = ""; turning = false;
    moveForward();
    Serial.println("CENTER → FORWARD");
  }
  // বাঁয়ে আগুন → বাঁয়ে ঘোরো
  else if (fireL && !fireR && !fireC) {
    stopPumpIfRunning();
    if (!turning) {
      if (millis() - lastTurnTime > TURN_COOLDOWN || lastTurn != "LEFT") {
        startTurnLeft();
        lastTurn = "LEFT"; lastTurnTime = millis();
        Serial.println("LEFT → TURN LEFT");
      }
    }
  }
  // ডানে আগুন → ডানে ঘোরো
  else if (fireR && !fireL && !fireC) {
    stopPumpIfRunning();
    if (!turning) {
      if (millis() - lastTurnTime > TURN_COOLDOWN || lastTurn != "RIGHT") {
        startTurnRight();
        lastTurn = "RIGHT"; lastTurnTime = millis();
        Serial.println("RIGHT → TURN RIGHT");
      }
    }
  }
  // দুই দিকে আগুন → সামনে
  else if (fireL && fireR && !fireC) {
    stopPumpIfRunning();
    turning = false;
    moveForward();
    Serial.println("BOTH → FORWARD");
  }
  // আগুন নেই
  else {
    stopMotors();
    stopPumpIfRunning();
    lastTurn = ""; fireConfirm = 0;
    Serial.println("IDLE");
  }

  if (turning && millis() - turnStartTime >= TURN_DURATION) {
    turning = false;
    stopMotors();
    Serial.println("TURN DONE");
  }

  delay(100);
}

// =============================================
// MOTORS
// FORWARD কাজ না করলে নিচের note পড়ো:
//
// BACK ঠিকঠাক কাজ করছে মানে
// moveBackward() এর L1+R1 = HIGH সঠিক।
// তাই moveForward() এ L2+R2 = HIGH হওয়া উচিত।
// কিন্তু Right side চলছে না মানে R2 pin সমস্যা।
//
// Fix: MOTOR_R2 আর MOTOR_R1 swap করে দেখো
// অর্থাৎ L298N এর IN3/IN4 wire উল্টো লাগাও
// =============================================

void moveForward() {
  analogWrite(MOTOR_L1, SPEED_MOVE); digitalWrite(MOTOR_L2, LOW);
  analogWrite(MOTOR_R1, SPEED_MOVE); digitalWrite(MOTOR_R2, LOW);
}

void moveBackward() {
  digitalWrite(MOTOR_L1, LOW); analogWrite(MOTOR_L2, SPEED_MOVE);
  digitalWrite(MOTOR_R1, LOW); analogWrite(MOTOR_R2, SPEED_MOVE);
}

void stopMotors() {
  digitalWrite(MOTOR_L1, LOW); digitalWrite(MOTOR_L2, LOW);
  digitalWrite(MOTOR_R1, LOW); digitalWrite(MOTOR_R2, LOW);
}

void startTurnLeft() {
  turning = true; turnStartTime = millis();
  digitalWrite(MOTOR_L1, LOW); analogWrite(MOTOR_L2, SPEED_TURN);
  analogWrite(MOTOR_R1, SPEED_TURN); digitalWrite(MOTOR_R2, LOW);
}

void startTurnRight() {
  turning = true; turnStartTime = millis();
  analogWrite(MOTOR_L1, SPEED_TURN); digitalWrite(MOTOR_L2, LOW);
  digitalWrite(MOTOR_R1, LOW); analogWrite(MOTOR_R2, SPEED_TURN);
}

void manualLeft() {
  digitalWrite(MOTOR_L1, LOW); analogWrite(MOTOR_L2, SPEED_TURN);
  analogWrite(MOTOR_R1, SPEED_TURN); digitalWrite(MOTOR_R2, LOW);
}

void manualRight() {
  analogWrite(MOTOR_L1, SPEED_TURN); digitalWrite(MOTOR_L2, LOW);
  digitalWrite(MOTOR_R1, LOW); analogWrite(MOTOR_R2, SPEED_TURN);
}

// ================= PUMP =================

void autoPumpUntilOut() {
  waterServo.attach(SERVO_PIN);
  pumpON();
  while (analogRead(FLAME_CENTER) < FIRE_NEAR) {
    for (int pos = 70; pos <= 110; pos += 5) {
      waterServo.write(pos); delay(50);
      if (analogRead(FLAME_CENTER) >= FIRE_NEAR) break;
    }
    for (int pos = 110; pos >= 70; pos -= 5) {
      waterServo.write(pos); delay(50);
      if (analogRead(FLAME_CENTER) >= FIRE_NEAR) break;
    }
  }
  pumpOFF();
  waterServo.write(90); delay(300);
  waterServo.detach();
  pumping = false;
  fireConfirm = 0;
  Serial.println("FIRE OUT → PUMP OFF");
}

void manualServoPump() {
  for (int pos = 70; pos <= 110; pos += 5) {
    waterServo.write(pos); delay(50);
    if (espSerial.available()) {
      String cmd = espSerial.readStringUntil('\n'); cmd.trim();
      if (cmd == "PUMP_OFF" || cmd == "STOP") { espCommand = cmd; return; }
    }
  }
  for (int pos = 110; pos >= 70; pos -= 5) {
    waterServo.write(pos); delay(50);
    if (espSerial.available()) {
      String cmd = espSerial.readStringUntil('\n'); cmd.trim();
      if (cmd == "PUMP_OFF" || cmd == "STOP") { espCommand = cmd; return; }
    }
  }
  if (manualPumping && espCommand != "PUMP_OFF" && espCommand != "STOP") {
    manualServoPump();
  }
}

void stopPump() {
  pumpOFF();
  waterServo.write(90); delay(150);
  waterServo.detach();
  pumping = false; manualPumping = false;
}

void stopPumpIfRunning() {
  if (pumping) stopPump();
}

void pumpON()  { digitalWrite(RELAY_PIN, LOW); }
void pumpOFF() { digitalWrite(RELAY_PIN, HIGH); }
