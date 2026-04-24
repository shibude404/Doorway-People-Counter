#include <Arduino.h>

// Outer = outside the room (pin 32), Inner = inside the room (pin 33)
#define OUTER_PIN 32
#define INNER_PIN 33

#define BREAK_THRESHOLD  1000
#define CLEAR_THRESHOLD  1500

const int debounceCount = 3;

bool isOuterBroken = false;
bool isInnerBroken = false;
int outerLowCount = 0;
int innerLowCount = 0;

int firstTrigger = 0;  // 1=inner, 2=outer
int lastTrigger  = 0;

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

void setup() {
  Serial.begin(115200);
  pinMode(OUTER_PIN, INPUT);
  pinMode(INNER_PIN, INPUT);
  Serial.println("Tripwire test starting...");
}

void loop() {
  readTripwire(OUTER_PIN, isOuterBroken, outerLowCount);
  readTripwire(INNER_PIN, isInnerBroken, innerLowCount);

  Serial.printf("Outer: %4d (%s)  |  Inner: %4d (%s)",
    analogRead(OUTER_PIN), isOuterBroken ? "BROKEN" : "clear",
    analogRead(INNER_PIN), isInnerBroken ? "BROKEN" : "clear");

  // Direction tracking
  if (isInnerBroken && !isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 1;
    lastTrigger = 1;
  } else if (!isInnerBroken && isOuterBroken) {
    if (firstTrigger == 0) firstTrigger = 2;
    lastTrigger = 2;
  }

  // Crossing complete — both clear
  if (!isInnerBroken && !isOuterBroken && firstTrigger != 0) {
    if (firstTrigger == 2 && lastTrigger == 1)
      Serial.print("  >>> ENTERING <<<");
    else if (firstTrigger == 1 && lastTrigger == 2)
      Serial.print("  >>> EXITING <<<");
    else
      Serial.print("  >>> UNKNOWN direction <<<");

    firstTrigger = 0;
    lastTrigger  = 0;
  }

  Serial.println();
  delay(100);
}
