#include <Wire.h>
#include <MPU6050.h>

#define TRIG 5
#define ECHO 18

long history[5] = {0};
int index = 0;

int occupancyCount = 0;
MPU6050 mpu;

long getDistance() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duration = pulseIn(ECHO, HIGH);
  long distance = duration * 0.034 / 2;

  return distance;
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

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);  // SDA, SCL

  mpu.initialize();

  if (mpu.testConnection()) {
    Serial.println("MPU6050 Connected");
  } else {
    Serial.println("MPU6050 Connection Failed");
  }
}

void loop() {
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // Read distance
  long distance = getDistance();
  
  // Store in history (sliding window)
  history[index] = distance;
  index++;
  
  if (index == 5) {
    index = 0;
  
    // Check motion trend
    if (isIncreasing()) {
      occupancyCount++;
    } 
    else if (isDecreasing()) {
      occupancyCount--;
    }
  
    if (occupancyCount < 0) occupancyCount = 0;
  }

  mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);

  // Send as CSV (easy for Python)
  Serial.print("IMU:");
  Serial.print(ax); Serial.print(",");
  Serial.print(ay); Serial.print(",");
  Serial.print(az); Serial.print(",");
  Serial.print(gx); Serial.print(",");
  Serial.print(gy); Serial.print(",");
  Serial.print(gz);
  
  Serial.print("|COUNT:");
  Serial.println(occupancyCount);
  
  delay(200);
}
