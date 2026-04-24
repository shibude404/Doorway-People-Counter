#include "WirelessCommunication.h"
#include "sharedVariable.h"
#include <Wire.h>
#include "Adafruit_VL53L1X.h"

// --- Tripwire Pin Assignments ---
#define OUTER_PIN 32
#define INNER_PIN 33

// --- ToF Pin Assignments ---
#define XSHUT1 25
#define XSHUT2 26
#define TOF_ADDR_1 0x30
#define TOF_ADDR_2 0x31
#define TOF_TRIGGER_MM 700

// --- Tripwire Thresholds ---
#define BREAK_THRESHOLD  1000
#define CLEAR_THRESHOLD  1500

const int debounceCount = 1;

// --- Global People Count ---
int peopleCount = 0;
volatile shared_uint32 x;

// --- ToF Sensors ---
Adafruit_VL53L1X tof1 = Adafruit_VL53L1X(XSHUT1);
Adafruit_VL53L1X tof2 = Adafruit_VL53L1X(XSHUT2);
bool tof1Ok = false;
bool tof2Ok = false;
int16_t lastD1 = 0;
int16_t lastD2 = 0;

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
#define TWO_PERSON_WINDOW_MS 500  // both sensors must trigger within this window
unsigned long s1TriggeredAt = 0;
unsigned long s2TriggeredAt = 0;

// --- Two-Person Cooldown ---
unsigned long twoPeopleCooldownUntil = 0;
#define TWO_PERSON_COOLDOWN_MS 2000

// --- Sensing Window ---
unsigned long sensingWindowUntil = 0;
#define SENSING_POST_MS 1000

// --- ToF Poll Rate ---
unsigned long lastTofPoll = 0;
#define TOF_POLL_MS 100  // poll ToF sensors at most once every 100ms

// -------------------------------------------------------

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
  delay(3000);
  Serial.println("Booting...");

  Wire.begin(21, 22);

  pinMode(OUTER_PIN, INPUT);
  pinMode(INNER_PIN, INPUT);

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

  init_wifi_task();
  INIT_SHARED_VARIABLE(x, peopleCount);

  Serial.println("People Counter (ToF v2) initialized. Count: 0");
}

void loop() {
  // --- 1. Read tripwires ---
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

  // --- 2. Update ToF readings (rate-limited, keep last good value) ---
  if (millis() - lastTofPoll >= TOF_POLL_MS) {
    lastTofPoll = millis();
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
  }

  bool s1 = (lastD1 > 0 && lastD1 < TOF_TRIGGER_MM);
  bool s2 = (lastD2 > 0 && lastD2 < TOF_TRIGGER_MM);

  // --- 3. Two-person detection within sensing window ---
  if (!twoPeopleMode) {
    if (s1 || s2 || isInnerBroken || isOuterBroken)
      sensingWindowUntil = millis() + SENSING_POST_MS;

    bool windowActive = (millis() < sensingWindowUntil);

    if (windowActive) {
      if (isInnerBroken || isOuterBroken) {
        Serial.printf("  [ToF] S1: %4dmm (%s)  S2: %4dmm (%s)\n",
          lastD1, s1 ? "BLOCKED" : "clear",
          lastD2, s2 ? "BLOCKED" : "clear");
      }
      if (s1) s1TriggeredAt = millis();
      if (s2) s2TriggeredAt = millis();

      bool bothTriggeredNearby = (s1TriggeredAt > 0 && s2TriggeredAt > 0 &&
        abs((long)(s1TriggeredAt - s2TriggeredAt)) <= TWO_PERSON_WINDOW_MS);

      if (bothTriggeredNearby) {
        twoPeopleMode = true;
        Serial.println("Two-person mode activated.");
      }
    }
  }

  // Re-read tripwires for faster response
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

  // --- 4. Track direction sequence ---
  if (isInnerBroken && !isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 1;
    lastTrigger = 1;
  } else if (!isInnerBroken && isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 2;
    lastTrigger = 2;
  }

  // --- 5. Crossing complete — both clear ---
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

    firstTrigger  = 0;
    lastTrigger   = 0;
    twoPeopleMode = false;
    s1TriggeredAt = 0;
    s2TriggeredAt = 0;
  }
}
