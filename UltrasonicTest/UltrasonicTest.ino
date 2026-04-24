// HC-SR04 dual ultrasonic sensor test for ESP32
#include <Arduino.h>
// Sensor 1: Trig=22, Echo=26
// Sensor 2: Trig=21, Echo=25

#define TRIG1 22
#define ECHO1 26
#define TRIG2 21
#define ECHO2 25

// Distance threshold in cm — object closer than this counts as "triggered"
#define TRIGGER_DISTANCE_CM 50

// Timeout for pulseIn in microseconds (matches ~4m max range)
#define PULSE_TIMEOUT 23200

float measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, PULSE_TIMEOUT);
  if (duration == 0) return -1.0; // timeout = no echo = out of range
  return duration * 0.034 / 2.0;
}

void setup() {
  Serial.begin(115200);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);
  Serial.println("Ultrasonic sensor test starting...");
}

void loop() {
  float dist1 = measureDistance(TRIG1, ECHO1);
  float dist2 = measureDistance(TRIG2, ECHO2);

  bool sensor1Triggered = (dist1 > 0 && dist1 < TRIGGER_DISTANCE_CM);
  bool sensor2Triggered = (dist2 > 0 && dist2 < TRIGGER_DISTANCE_CM);

  Serial.printf("Sensor1: %.1f cm (%s)  |  Sensor2: %.1f cm (%s)\n",
    dist1, sensor1Triggered ? "TRIGGERED" : "clear",
    dist2, sensor2Triggered ? "TRIGGERED" : "clear");

  if (sensor1Triggered && sensor2Triggered) {
    Serial.println(">>> MODE: 2 PEOPLE <<<");
  } else {
    Serial.println(">>> MODE: 1 PERSON (or none) <<<");
  }

  delay(200);
}
