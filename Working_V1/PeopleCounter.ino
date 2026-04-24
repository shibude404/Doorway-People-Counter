#include "WirelessCommunication.h"
#include "sharedVariable.h"

// --- Tripwire Pin Assignments ---
#define OUTER_PIN 32
#define INNER_PIN 33

// --- Ultrasonic Pin Assignments ---
#define TRIG1 22
#define ECHO1 26
#define TRIG2 21
#define ECHO2 25

// --- Thresholds ---
#define BREAK_THRESHOLD      1000   // analog < this = tripwire broken
#define CLEAR_THRESHOLD      1500   // analog > this = tripwire clear
#define TRIGGER_DISTANCE_CM  50     // ultrasonic: closer than this = person detected
#define PULSE_TIMEOUT        23200  // ~4m max range

const int debounceCount = 3;

// --- Global People Count ---
int peopleCount = 0;
volatile shared_uint32 x;

// --- Tripwire State ---
bool isOuterBroken = false;
bool isInnerBroken = false;
int outerLowCount = 0;
int innerLowCount = 0;

// --- Sequence Tracking ---
int firstTrigger = 0;   // 1=inner, 2=outer
int lastTrigger  = 0;

// --- Two-Person Mode ---
bool twoPeopleMode = false;
int bothTriggeredCount = 0;       // consecutive reads where both ultrasonics fired
#define TWO_PERSON_DEBOUNCE 4     // require this many consecutive simultaneous triggers

// -------------------------------------------------------

float measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH, PULSE_TIMEOUT);
  if (duration == 0) return -1.0;
  return duration * 0.034 / 2.0;
}

void readTripwire(int pin, bool &isBroken, int &lowCount) {
  int val = analogRead(pin);
  if (val < BREAK_THRESHOLD) {
    lowCount++;
    if (lowCount >= debounceCount) isBroken = true;
  } else if (val > CLEAR_THRESHOLD) {
    lowCount = 0;
    isBroken = false;
  }
}

void update_shared_count() {
  LOCK_SHARED_VARIABLE(x);
  x.value = peopleCount;
  UNLOCK_SHARED_VARIABLE(x);
}

// -------------------------------------------------------

void setup() {
  Serial.begin(115200);

  pinMode(OUTER_PIN, INPUT);
  pinMode(INNER_PIN, INPUT);
  pinMode(TRIG1, OUTPUT);
  pinMode(ECHO1, INPUT);
  pinMode(TRIG2, OUTPUT);
  pinMode(ECHO2, INPUT);

  init_wifi_task();
  INIT_SHARED_VARIABLE(x, peopleCount);

  Serial.println("People Counter initialized. Count: 0");
}

void loop() {
  // --- 1. Read tripwires ---
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

  // --- 2. Check ultrasonic for 2-person mode during active crossing ---
  if (!twoPeopleMode && (isInnerBroken || isOuterBroken)) {
    float dist1 = measureDistance(TRIG1, ECHO1);
    float dist2 = measureDistance(TRIG2, ECHO2);
    bool s1 = (dist1 > 0 && dist1 < TRIGGER_DISTANCE_CM);
    bool s2 = (dist2 > 0 && dist2 < TRIGGER_DISTANCE_CM);
    if (s1 && s2) {
      bothTriggeredCount++;
      if (bothTriggeredCount >= TWO_PERSON_DEBOUNCE) {
        twoPeopleMode = true;
        Serial.println("Two-person mode activated.");
      }
    } else {
      bothTriggeredCount = 0;  // reset if they stop being simultaneously triggered
    }
  }

  // --- 3. Track direction sequence ---
  if (isInnerBroken && !isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 1;
    lastTrigger = 1;
  } else if (!isInnerBroken && isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 2;
    lastTrigger = 2;
  }

  // --- 4. Crossing complete — both clear ---
  if (!isInnerBroken && !isOuterBroken && firstTrigger != 0) {
    int delta = twoPeopleMode ? 2 : 1;

    if (firstTrigger == 2 && lastTrigger == 1) {
      // Outer first, inner last → entering
      peopleCount += delta;
      update_shared_count();
      Serial.printf("%s Entered. Total: %d\n", twoPeopleMode ? "2 People" : "Person", peopleCount);
    } else if (firstTrigger == 1 && lastTrigger == 2) {
      // Inner first, outer last → exiting
      peopleCount -= delta;
      if (peopleCount < 0) peopleCount = 0;
      update_shared_count();
      Serial.printf("%s Exited. Total: %d\n", twoPeopleMode ? "2 People" : "Person", peopleCount);
    }

    firstTrigger      = 0;
    lastTrigger       = 0;
    twoPeopleMode     = false;
    bothTriggeredCount = 0;
  }
}
