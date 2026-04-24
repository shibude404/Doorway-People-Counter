#include <Arduino.h>
#include <Wire.h>

#define XSHUT_PIN 25  // change to 26 for sensor 2

void setup() {
  Serial.begin(115200);
  delay(3000);
  Serial.println("Booting...");

  // Boot the sensor
  pinMode(XSHUT_PIN, OUTPUT);
  digitalWrite(XSHUT_PIN, LOW);
  delay(100);
  digitalWrite(XSHUT_PIN, HIGH);
  delay(100);

  Wire.begin(21, 22);  // SDA, SCL
  Serial.println("Scanning I2C bus...");

  int found = 0;
  for (byte addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    byte err = Wire.endTransmission();
    if (err == 0) {
      Serial.printf("Device found at address 0x%02X\n", addr);
      found++;
    }
  }

  if (found == 0)
    Serial.println("No I2C devices found. Check SDA/SCL wiring.");
  else
    Serial.printf("%d device(s) found.\n", found);
}

void loop() {}
