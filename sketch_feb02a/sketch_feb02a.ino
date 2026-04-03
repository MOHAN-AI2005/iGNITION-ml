#include <Wire.h>
#include <LiquidCrystal_PCF8574.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define BUZZER 25

// Buzzer modes:
// 0 = passive piezo (tone only)
// 1 = active buzzer, active HIGH
// 2 = active buzzer, active LOW
// 3 = dual drive (tone + HIGH) for unknown buzzer type/wiring tests
const uint8_t BUZZER_MODE = 3;
const uint16_t BUZZER_TONE_HZ = 2200;

LiquidCrystal_PCF8574 lcd(0x27);
bool lcdReady = false;

// -------- VARIABLES --------
int occupancyCount = 0;

bool serverAlertActive = false;
unsigned long serverAlertUntil = 0;
const unsigned long serverAlertHoldMs = 3000;

bool buzzerState = false;

long history[5] = {0};
int histIndex = 0;
bool bufferFilled = false;

unsigned long lastTriggerTime = 0;
const int cooldown = 2000;

// -------- DISTANCE --------
long getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  if (duration == 0) return -1;

  long d = duration * 0.034 / 2;
  if (d < 2 || d > 400) return -1;

  return d;
}

// -------- FILTER --------
long getFilteredDistance() {
  long readings[5];
  int count = 0;

  for (int i = 0; i < 5; i++) {
    long d = getDistance();
    if (d != -1) readings[count++] = d;
    delay(5);
  }

  if (count == 0) return -1;

  for (int i = 0; i < count - 1; i++) {
    for (int j = i + 1; j < count; j++) {
      if (readings[i] > readings[j]) {
        long t = readings[i];
        readings[i] = readings[j];
        readings[j] = t;
      }
    }
  }

  return readings[count / 2];
}

// -------- TREND --------
void updateHistory(long d) {
  history[histIndex] = d;
  histIndex = (histIndex + 1) % 5;
  if (histIndex == 0) bufferFilled = true;
}

bool isIncreasing() {
  for (int i = 0; i < 4; i++)
    if (history[i] >= history[i + 1]) return false;
  return true;
}

bool isDecreasing() {
  for (int i = 0; i < 4; i++)
    if (history[i] <= history[i + 1]) return false;
  return true;
}

void setBuzzer(bool on) {
  if (buzzerState == on) return;
  buzzerState = on;

  if (BUZZER_MODE == 0) {
    if (on) tone(BUZZER, BUZZER_TONE_HZ);
    else noTone(BUZZER);
  } else if (BUZZER_MODE == 1) {
    noTone(BUZZER);
    digitalWrite(BUZZER, on ? HIGH : LOW);
  } else if (BUZZER_MODE == 2) {
    noTone(BUZZER);
    digitalWrite(BUZZER, on ? LOW : HIGH);
  } else {
    if (on) {
      tone(BUZZER, BUZZER_TONE_HZ);
      digitalWrite(BUZZER, HIGH);
    } else {
      noTone(BUZZER);
      digitalWrite(BUZZER, LOW);
    }
  }

  Serial.print("BUZZER:");
  Serial.print(on ? "ON" : "OFF");
  Serial.print(" MODE:");
  Serial.println(BUZZER_MODE);
}

void buzzerSelfTest() {
  // Audible startup check so wiring/type problems are obvious immediately.
  setBuzzer(true);
  delay(350);
  setBuzzer(false);
  delay(150);
  setBuzzer(true);
  delay(350);
  setBuzzer(false);
}

bool i2cDevicePresent(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  
  pinMode(BUZZER, OUTPUT);
  digitalWrite(BUZZER, LOW);
  noTone(BUZZER);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  Wire.begin();
  if (i2cDevicePresent(0x27)) {
    lcd.begin(16, 2);
    lcd.setBacklight(255);
    lcd.setCursor(0, 0);
    lcd.print("People Counter");
    delay(1000);
    lcd.clear();
    lcdReady = true;
    Serial.println("LCD:READY");
  } else {
    lcdReady = false;
    Serial.println("LCD:NOT_FOUND (running without LCD)");
  }

  Serial.print("READY BUZZER_PIN=");
  Serial.print(BUZZER);
  Serial.print(" MODE=");
  Serial.println(BUZZER_MODE);

  buzzerSelfTest();
}

// -------- LOOP --------
void loop() {

  long d = getFilteredDistance();

  // 🔥 SERIAL OUTPUT
  Serial.print("Distance: ");
  Serial.print(d);
  Serial.println(" cm");

  if (Serial.available()) {
    String msg = Serial.readStringUntil('\n');
    msg.trim();
  
    if (msg.startsWith("ALERT")) {
      Serial.print("CMD:");
      Serial.println(msg);
      serverAlertActive = true;
      serverAlertUntil = millis() + serverAlertHoldMs;
    } else if (msg == "BUZZER_TEST") {
      Serial.println("CMD:BUZZER_TEST");
      buzzerSelfTest();
    } else if (msg == "BUZZER_ON") {
      Serial.println("CMD:BUZZER_ON");
      setBuzzer(true);
      delay(1200);
      setBuzzer(false);
    } else if (msg == "BUZZER_PULSE") {
      Serial.println("CMD:BUZZER_PULSE");
      for (int i = 0; i < 8; i++) {
        setBuzzer(true);
        delay(120);
        setBuzzer(false);
        delay(120);
      }
    } else if (msg == "OK") {
      Serial.println("CMD:OK");
      serverAlertActive = false;
      serverAlertUntil = 0;
    }
  }

  if (serverAlertActive && millis() > serverAlertUntil) {
    serverAlertActive = false;
  }

  if (serverAlertActive) {
    setBuzzer(true);
  } else {
    setBuzzer(false);
  }
  
  if (d != -1) {
    updateHistory(d);

    if (bufferFilled && millis() - lastTriggerTime > cooldown) {
      // ENTER
      if (isIncreasing()) {
        occupancyCount++;

        Serial.println("ENTER detected");
        Serial.print("COUNT:");
        Serial.print(occupancyCount);
        Serial.println(",DOOR:MAIN,EVENT:ENTER");

        if (lcdReady) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print(">> ENTER >>");
        }
        delay(500);

        lastTriggerTime = millis();
      }

      // EXIT
      else if (isDecreasing()) {
        occupancyCount--;

        if (occupancyCount < 0) occupancyCount = 0;

        Serial.println("EXIT detected");
        Serial.print("COUNT:");
        Serial.print(occupancyCount);
        Serial.println(",DOOR:MAIN,EVENT:EXIT");

        if (lcdReady) {
          lcd.clear();
          lcd.setCursor(0, 0);
          lcd.print("<< EXIT <<");
        }
        delay(500);

        lastTriggerTime = millis();
      }
    }
  }

  // LCD DISPLAY
  if (lcdReady) {
    lcd.setCursor(0, 0);
    lcd.print("People: ");
    lcd.print(occupancyCount);
    lcd.print("   ");

    lcd.setCursor(0, 1);
    lcd.print("Dist:");
    lcd.print(d);
    lcd.print("   ");
  }
  
  Serial.print("DIST:");
  Serial.print(d);
  
  Serial.print("|COUNT:");
  Serial.println(occupancyCount);
  delay(200);
}
