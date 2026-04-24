#include <Arduino.h>

// --- Sensor 1 Pins ---
const int trigPin1 = 22;
const int echoPin1 = 26;

// --- Sensor 2 Pins (Change these to match your wiring!) ---
const int trigPin2 = 21; 
const int echoPin2 = 25; 

void setup() {
  Serial.begin(115200); 
  
  pinMode(trigPin1, OUTPUT);
  pinMode(echoPin1, INPUT);
  
  pinMode(trigPin2, OUTPUT);
  pinMode(echoPin2, INPUT);
}

// Reusable function to handle the math and the ping
float readSensor(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // A 30ms timeout is still required! 
  // If Sensor 1 points into the open sky and misses its echo, 
  // this timeout prevents the code from freezing so Sensor 2 can still fire.
  long duration = pulseIn(echoPin, HIGH, 30000);

  if (duration == 0) {
    return 0.0; // Return 0 if no echo was found
  }

  return (duration * 0.0343) / 2.0;
}

void loop() {
  // Fire sensors one at a time with a gap to prevent crosstalk
  float distance1 = readSensor(trigPin1, echoPin1);
  delay(30);  // wait for sensor 1's echo to fully die out

  float distance2 = readSensor(trigPin2, echoPin2);
  delay(30);  // wait for sensor 2's echo to fully die out

  Serial.print("Front: ");
  Serial.print(distance1);
  Serial.print(" cm   |   Back: ");
  Serial.print(distance2);
  Serial.println(" cm");
}


// #include "WirelessCommunication.h"
// #include "sharedVariable.h"

// // --- Sensor 1 Pins ---
// const int trigPin1 = 22;
// const int echoPin1 = 26;

// // --- Sensor 2 Pins ---
// const int trigPin2 = 21;
// const int echoPin2 = 25; 

// // --- Shared Variable for Server ---
// // This MUST be named 'x' to match the extern definition in WirelessCommunication.cpp
// volatile shared_uint32 x;

// void setup() {
//   Serial.begin(115200); 
  
//   pinMode(trigPin1, OUTPUT);
//   pinMode(echoPin1, INPUT);
  
//   pinMode(trigPin2, OUTPUT);
//   pinMode(echoPin2, INPUT);

//   // Initialize the WiFi core task and our thread-safe shared variable
//   init_wifi_task();
//   INIT_SHARED_VARIABLE(x, 0);

//   Serial.println("System Initialized. Ultrasonic reporting started.");
// }

// // Reusable function to handle the math and the ping
// float readSensor(int trigPin, int echoPin) {
//   digitalWrite(trigPin, LOW);
//   delayMicroseconds(2);
  
//   digitalWrite(trigPin, HIGH);
//   delayMicroseconds(10);
//   digitalWrite(trigPin, LOW);

//   // 30ms timeout prevents freezing
//   long duration = pulseIn(echoPin, HIGH, 30000);

//   if (duration == 0) {
//     return 0.0; // Return 0 if no echo was found
//   }

//   return (duration * 0.0343) / 2.0;
// }

// void update_shared_distance(uint32_t distance) {
//   // Quickly lock the variable, update it, and unlock it so Core 0 can send it
//   LOCK_SHARED_VARIABLE(x);
//   x.value = distance;
//   UNLOCK_SHARED_VARIABLE(x);
// }

// void loop() {
//   // 1. Read the sensors
//   float distance1 = readSensor(trigPin1, echoPin1);
//   float distance2 = readSensor(trigPin2, echoPin2);
  
//   // 2. Print the results to Serial
//   Serial.print("Front: ");
//   Serial.print(distance1);
//   Serial.print(" cm   |   Back: ");
//   Serial.print(distance2);
//   Serial.println(" cm");

//   // 3. Publish to the server
//   // Since WirelessCommunication expects an integer (uint32_t), we cast the float.
//   // Currently sending Sensor 1 (Front). Change to distance2 if you want the other sensor.
//   uint32_t distanceToSend = (uint32_t)distance1; 
//   update_shared_distance(distanceToSend);

//   // A tiny 10ms delay just to keep the Serial Monitor from crashing
//   delay(10);
// }