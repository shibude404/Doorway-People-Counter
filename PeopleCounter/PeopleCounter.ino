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
#define TRIGGER_DISTANCE_CM  60   // s1: ultrasonic: closer than this = person detected
#define TRIGGER_DISTANCE_S2  100   // S2: threshold in cm
#define PULSE_TIMEOUT        23200  // ~4m max range

const int debounceCount = 1;

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
int bothTriggeredCount = 0;        // consecutive reads where both ultrasonics fired
#define TWO_PERSON_DEBOUNCE 1      // require this many consecutive simultaneous triggers

// --- Two-Person Cooldown ---
unsigned long twoPeopleCooldownUntil = 0;
#define TWO_PERSON_COOLDOWN_MS 2000  // ignore crossings for this long after a 2-person event

// --- Ultrasonic Window ---
// Opens when ultrasonics detect someone (even before tripwire), stays open after tripwire clears
unsigned long ultrasonicWindowUntil = 0;
#define ULTRASONIC_POST_MS 1000  // keep window open this long after tripwire clears

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
  // --- 2. Ultrasonic polling — always read, open window when someone detected ---
  if (!twoPeopleMode) {
    float dist1 = measureDistance(TRIG1, ECHO1);
    delay(10);
    float dist2 = measureDistance(TRIG2, ECHO2);
    delay(10);
    bool windowActive = (millis() < ultrasonicWindowUntil);

    bool s1 = (dist1 > 0 && dist1 < TRIGGER_DISTANCE_CM) || (windowActive && dist1 < 0);
    bool s2 = (dist2 > 0 && dist2 < TRIGGER_DISTANCE_S2) || (windowActive && dist2 < 0);

    // Open/extend the window whenever either sensor or tripwire is active
    if (s1 || s2 || isInnerBroken || isOuterBroken)
      ultrasonicWindowUntil = millis() + ULTRASONIC_POST_MS;

    if (windowActive) {
      if (isInnerBroken || isOuterBroken) {
        Serial.printf("  [US] S1: %.1fcm (%s)  S2: %.1fcm (%s)  count=%d\n",
          dist1, s1 ? "HIT" : "---",
          dist2, s2 ? "HIT" : "---",
          bothTriggeredCount);
      }
      if (s1 && s2) {
        bothTriggeredCount++;
        if (bothTriggeredCount >= TWO_PERSON_DEBOUNCE) {
          twoPeopleMode = true;
          Serial.println("Two-person mode activated.");
        }
      }
    }
  }

  // Re-read tripwires after ultrasonic delays for faster response
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

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
    bool inCooldown = (millis() < twoPeopleCooldownUntil);

    if (inCooldown) {
      // Second person of a 2-person event — already counted, skip
      Serial.println("Crossing ignored (2-person cooldown).");
    } else {
      int delta = twoPeopleMode ? 2 : 1;

      if (firstTrigger == 2 && lastTrigger == 1) {
        peopleCount += delta;
        update_shared_count();
        Serial.printf("%s Entered. Total: %d\n", twoPeopleMode ? "2 People" : "Person", peopleCount);
      } else if (firstTrigger == 1 && lastTrigger == 2) {
        peopleCount -= delta;
        if (peopleCount < 0) peopleCount = 0;
        update_shared_count();
        Serial.printf("%s Exited. Total: %d\n", twoPeopleMode ? "2 People" : "Person", peopleCount);
      }

      if (twoPeopleMode) {
        twoPeopleCooldownUntil = millis() + TWO_PERSON_COOLDOWN_MS;
      }
    }

    firstTrigger       = 0;
    lastTrigger        = 0;
    twoPeopleMode      = false;
    bothTriggeredCount = 0;
  }
}
