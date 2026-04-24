#include "WirelessCommunication.h"
#include "sharedVariable.h"
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

// --- Tripwire Pin Assignments ---
#define OUTER_PIN 32
#define INNER_PIN 33

// --- ToF Pin Assignments ---
#define XSHUT1 25
#define XSHUT2 26
#define TOF_ADDR_1 0x30
#define TOF_ADDR_2 0x31
#define TOF_TRIGGER_MM 400  // object closer than this = person detected

// --- Tripwire Thresholds ---
#define BREAK_THRESHOLD  1000
#define CLEAR_THRESHOLD  1500

const int debounceCount = 1;

// --- Global People Count ---
int peopleCount = 0;
volatile shared_uint32 x;

// --- ToF Sensors ---
Adafruit_VL53L0X tof1;
Adafruit_VL53L0X tof2;
bool tof1Ok = false;
bool tof2Ok = false;

// --- Tripwire State ---
bool isOuterBroken = false;
bool isInnerBroken = false;
int outerLowCount = 0;
int innerLowCount = 0;

// --- Sequence Tracking ---
int firstTrigger = 0;  // 1=inner, 2=outer
int lastTrigger  = 0;

// --- Two-Person Mode ---
bool twoPeopleMode = false;
int bothTriggeredCount = 0;
#define TWO_PERSON_DEBOUNCE 1

// --- Two-Person Cooldown ---
unsigned long twoPeopleCooldownUntil = 0;
#define TWO_PERSON_COOLDOWN_MS 2000

// --- Sensing Window ---
unsigned long sensingWindowUntil = 0;
#define SENSING_POST_MS 1000

// -------------------------------------------------------

bool isTofBlocked(Adafruit_VL53L0X &sensor) {
  VL53L0X_RangingMeasurementData_t measure;
  sensor.rangingTest(&measure, false);
  if (measure.RangeStatus == 4) return false;
  return measure.RangeMilliMeter < TOF_TRIGGER_MM;
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

  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  delay(10);

  digitalWrite(XSHUT1, HIGH);
  delay(10);
  if (tof1.begin(TOF_ADDR_1)) {
    tof1Ok = true;
    Serial.println("ToF Sensor 1 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 1 failed.");
  }

  digitalWrite(XSHUT2, HIGH);
  delay(10);
  if (tof2.begin(TOF_ADDR_2)) {
    tof2Ok = true;
    Serial.println("ToF Sensor 2 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 2 failed.");
  }

  init_wifi_task();
  INIT_SHARED_VARIABLE(x, peopleCount);

  Serial.println("People Counter (ToF) initialized. Count: 0");
}

void loop() {
  // --- 1. Read tripwires ---
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

  // --- 2. ToF polling ---
  if (!twoPeopleMode) {
    bool windowActive = (millis() < sensingWindowUntil);

    bool s1 = tof1Ok && isTofBlocked(tof1);
    bool s2 = tof2Ok && isTofBlocked(tof2);

    if (s1 || s2 || isInnerBroken || isOuterBroken)
      sensingWindowUntil = millis() + SENSING_POST_MS;

    if (windowActive) {
      if (isInnerBroken || isOuterBroken) {
        Serial.printf("  [ToF] S1: %s  S2: %s  count=%d\n",
          s1 ? "BLOCKED" : "clear",
          s2 ? "BLOCKED" : "clear",
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

  // Re-read tripwires after ToF polling for faster response
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

      if (twoPeopleMode)
        twoPeopleCooldownUntil = millis() + TWO_PERSON_COOLDOWN_MS;
    }

    firstTrigger       = 0;
    lastTrigger        = 0;
    twoPeopleMode      = false;
    bothTriggeredCount = 0;
  }
}
