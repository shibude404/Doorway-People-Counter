#include "WirelessCommunication.h"
#include "sharedVariable.h"
#include <Wire.h>
#include <Adafruit_VL53L0X.h>

// --- Tripwire Pin Assignments ---
const int outerTripwirePin = 32;
const int innerTripwirePin = 33;

// --- ToF Pin Assignments ---
const int xshutPin1 = 25;
const int xshutPin2 = 26;
#define TOF_ADDR_1 0x30
#define TOF_ADDR_2 0x31
#define TOF_TRIGGER_DIST_MM 400  // distance threshold: below = blocked

Adafruit_VL53L0X tof1;
Adafruit_VL53L0X tof2;
bool tof1Ok = false;
bool tof2Ok = false;

// --- Tunable Debounce Thresholds ---
const int outerDebounceCount = 3;
const int innerDebounceCount = 3;

// --- Global Counters ---
int peopleCount = 0;
volatile shared_uint32 x;

// --- Tripwire State ---
bool isOuterBroken = false;
bool prevOuterBroken = false;
int outerLowCount = 0;

bool isInnerBroken = false;
bool prevInnerBroken = false;
int innerLowCount = 0;

// --- Sequence Tracking ---
int firstTrigger = 0;
int lastTrigger = 0;

// --- Two-Person Mode ---
bool twoPeopleMode = false;  // set true if both ToF sensors blocked simultaneously during a crossing

void setup() {
  Serial.begin(115200);
  pinMode(outerTripwirePin, INPUT);
  pinMode(innerTripwirePin, INPUT);

  // --- Initialize ToF sensors with address reassignment ---
  pinMode(xshutPin1, OUTPUT);
  pinMode(xshutPin2, OUTPUT);
  digitalWrite(xshutPin1, LOW);
  digitalWrite(xshutPin2, LOW);
  delay(10);

  // Bring up sensor 1 and assign new address
  digitalWrite(xshutPin1, HIGH);
  delay(10);
  if (tof1.begin(TOF_ADDR_1)) {
    tof1Ok = true;
    Serial.println("ToF Sensor 1 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 1 failed to initialize.");
  }

  // Bring up sensor 2 and assign new address
  digitalWrite(xshutPin2, HIGH);
  delay(10);
  if (tof2.begin(TOF_ADDR_2)) {
    tof2Ok = true;
    Serial.println("ToF Sensor 2 initialized.");
  } else {
    Serial.println("WARNING: ToF Sensor 2 failed to initialize.");
  }

  init_wifi_task();
  INIT_SHARED_VARIABLE(x, peopleCount);

  Serial.println("System Initialized. Room count: 0");
}

void update_shared_count() {
  LOCK_SHARED_VARIABLE(x);
  x.value = peopleCount;
  UNLOCK_SHARED_VARIABLE(x);
}

bool isTofBlocked(Adafruit_VL53L0X &sensor) {
  VL53L0X_RangingMeasurementData_t measure;
  sensor.rangingTest(&measure, false);
  if (measure.RangeStatus == 4) return false;  // phase failure = no object
  return measure.RangeMilliMeter < TOF_TRIGGER_DIST_MM;
}

void loop() {
  // --- 1. Read Inner Sensor ---
  if (analogRead(innerTripwirePin) < 1000) {
    innerLowCount++;
    if (innerLowCount >= innerDebounceCount && !isInnerBroken) {
      isInnerBroken = true;
    }
  } else if (analogRead(innerTripwirePin) > 1500) {
    innerLowCount = 0;
    if (isInnerBroken) isInnerBroken = false;
  }

  // --- 2. Read Outer Sensor ---
  if (analogRead(outerTripwirePin) < 1000) {
    outerLowCount++;
    if (outerLowCount >= outerDebounceCount && !isOuterBroken) {
      isOuterBroken = true;
    }
  } else if (analogRead(outerTripwirePin) > 1500) {
    outerLowCount = 0;
    if (isOuterBroken) isOuterBroken = false;
  }

  // --- 3. Check ToF sensors during an active crossing ---
  // Once twoPeopleMode is set for this crossing it stays until crossing resolves
  if (!twoPeopleMode && (isInnerBroken || isOuterBroken)) {
    bool b1 = tof1Ok && isTofBlocked(tof1);
    bool b2 = tof2Ok && isTofBlocked(tof2);
    if (b1 && b2) {
      twoPeopleMode = true;
      Serial.println("Two-person mode activated.");
    }
  }

  // --- 4. Print Live Tripwire Status on Change ---
  if (isOuterBroken != prevOuterBroken || isInnerBroken != prevInnerBroken) {
    Serial.print("Live Status: ");
    if (!isOuterBroken && !isInnerBroken)      Serial.println("CLEAR");
    else if (isOuterBroken && isInnerBroken)   Serial.println("BOTH BROKEN");
    else if (isOuterBroken)                    Serial.println("OUTER BROKEN");
    else                                       Serial.println("INNER BROKEN");

    prevOuterBroken = isOuterBroken;
    prevInnerBroken = isInnerBroken;
  }

  // --- 5. Process Movement Sequence ---
  if (!isInnerBroken && !isOuterBroken) {
    int delta = twoPeopleMode ? 2 : 1;

    if (firstTrigger == 1 && lastTrigger == 2) {
      peopleCount -= delta;
      if (peopleCount < 0) peopleCount = 0;
      update_shared_count();
      Serial.print(twoPeopleMode ? "2 People Exited." : "Person Exited.");
      Serial.print(" \t Total People: ");
      Serial.println(peopleCount);
    } else if (firstTrigger == 2 && lastTrigger == 1) {
      peopleCount += delta;
      update_shared_count();
      Serial.print(twoPeopleMode ? "2 People Entered." : "Person Entered.");
      Serial.print(" \t Total People: ");
      Serial.println(peopleCount);
    }

    firstTrigger = 0;
    lastTrigger = 0;
    twoPeopleMode = false;

  } else {
    if (isInnerBroken && !isOuterBroken) {
      if (firstTrigger == 0) firstTrigger = 1;
      lastTrigger = 1;
    } else if (!isInnerBroken && isOuterBroken) {
      if (firstTrigger == 0) firstTrigger = 2;
      lastTrigger = 2;
    }
  }
}
