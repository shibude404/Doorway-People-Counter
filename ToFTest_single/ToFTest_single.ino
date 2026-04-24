#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_VL53L1X.h"

// Change to 25 for Sensor 1, 26 for Sensor 2
#define XSHUT_PIN 25

#define TOF_TRIGGER_MM 400

Adafruit_VL53L1X tof = Adafruit_VL53L1X(XSHUT_PIN);
bool tofOk = false;

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Booting...");

  Wire.begin(21, 22);

  if (tof.begin(0x29, &Wire)) {
    tofOk = true;
    tof.startRanging();
    tof.setTimingBudget(50);
    Serial.println("ToF sensor initialized.");
  } else {
    Serial.println("WARNING: ToF sensor failed to initialize. Check wiring.");
  }

  Serial.println("Single ToF sensor test starting...");
}

void loop() {
  if (!tofOk) {
    Serial.println("Sensor not initialized.");
    delay(1000);
    return;
  }

  if (tof.dataReady()) {
    int16_t dist = tof.distance();
    tof.clearInterrupt();

    if (dist == -1) {
      Serial.println("Distance: out of range");
    } else {
      bool blocked = dist < TOF_TRIGGER_MM;
      Serial.printf("Distance: %4dmm (%s)\n", dist, blocked ? "BLOCKED" : "clear");
    }
  }

  delay(50);
}
