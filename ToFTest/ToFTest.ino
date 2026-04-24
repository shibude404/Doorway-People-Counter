#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_VL53L1X.h"

#define XSHUT1 25
#define XSHUT2 26
#define TOF_ADDR_1 0x30
#define TOF_ADDR_2 0x31
#define TOF_TRIGGER_MM 600

Adafruit_VL53L1X tof1 = Adafruit_VL53L1X(XSHUT1);
Adafruit_VL53L1X tof2 = Adafruit_VL53L1X(XSHUT2);
bool tof1Ok = false;
bool tof2Ok = false;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Booting...");

  Wire.begin(21, 22);

  // Boot sensor 1 first, reassign address
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  delay(100);

  digitalWrite(XSHUT1, HIGH);
  delay(100);
  if (tof1.begin(TOF_ADDR_1, &Wire)) {
    tof1.startRanging();
    tof1.setTimingBudget(50);
    tof1Ok = true;
    Serial.println("ToF Sensor 1 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 1 failed.");
  }

  // Boot sensor 2, assign different address
  digitalWrite(XSHUT2, HIGH);
  delay(100);
  if (tof2.begin(TOF_ADDR_2, &Wire)) {
    tof2.startRanging();
    tof2.setTimingBudget(50);
    tof2Ok = true;
    Serial.println("ToF Sensor 2 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 2 failed.");
  }

  Serial.println("ToF sensor test starting...");
}

int16_t lastD1 = 0;
int16_t lastD2 = 0;

void loop() {
  if (tof1Ok && tof1.dataReady()) {
    int16_t d = tof1.distance();
    tof1.clearInterrupt();
    if (d > 0) lastD1 = d;
  }
  if (tof2Ok && tof2.dataReady()) {
    int16_t d = tof2.distance();
    tof2.clearInterrupt();
    if (d > 0) lastD2 = d;
  }

  bool s1 = (lastD1 > 0 && lastD1 < TOF_TRIGGER_MM);
  bool s2 = (lastD2 > 0 && lastD2 < TOF_TRIGGER_MM);

  Serial.printf("S1: %4dmm (%s)  |  S2: %4dmm (%s)",
    lastD1, s1 ? "BLOCKED" : "clear",
    lastD2, s2 ? "BLOCKED" : "clear");

  if (s1 && s2) Serial.print("  >>> 2 PEOPLE <<<");
  else if (s1 || s2) Serial.print("  >>> 1 PERSON <<<");

  Serial.println();
}
