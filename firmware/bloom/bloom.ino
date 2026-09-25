/*
  bloom.ino  —  Gesture Bloom firmware (Phase 3 onward)
  ------------------------------------------------------------------
  What it is:   The real firmware. The Uno listens for text lines like
                "B:57" (bloom 57%) and moves the flower there smoothly.
  Why it exists: Anything can send those lines: you in the Serial
                Monitor today, the browser slider in phase 4, your hand
                in phase 7. The Uno doesn't care where they come from.
  How it fits:  The Uno owns everything physical: the safe pulse
                window, the flower's closed/open range, the speed
                limit, and what happens when messages stop (the
                watchdog closes the flower). The browser only ever says
                "how open", never "which angle", so no bug in the page
                can push the servo past the flower's limits.

  Protocol (details in docs/protocol.md), one line each, ending in \n:
    in:   B:<0-100>     target bloom percent (out of range → clamped)
          ?             print status (for humans)
    out:  READY         once at boot; the browser waits for this
          A:<degrees>   current servo angle, when it changes
          ERR:<text>    a line that couldn't be understood
          # <text>      human-readable notes; the browser ignores them

  All tunable numbers live in config.h (the second tab).
  ------------------------------------------------------------------
*/

#include <Servo.h>
#include "config.h"

// ---------- States -------------------------------------------------
// Plain numbers rather than a custom enum type: the Arduino IDE's
// auto-generated function prototypes break when a function signature
// uses a custom type (see docs/gotchas.md), and enterState() takes one.
const uint8_t STATE_WAITING = 0;    // no one talking; flower closed
const uint8_t STATE_FOLLOWING = 1;  // receiving B messages
const uint8_t STATE_CLOSING = 2;    // messages stopped; easing closed

uint8_t state = STATE_WAITING;

// ---------- Other state --------------------------------------------
Servo servo;
int currentUs = CLOSED_US;          // pulse being sent now
int targetUs = CLOSED_US;           // where we're heading
int bloomPercent = 0;               // last accepted B value

unsigned long lastMessageAt = 0;    // millis() of the last valid B line
unsigned long lastStepAt = 0;
unsigned long lastReportAt = 0;
int lastReportedUs = -1;            // -1 = nothing reported yet

char lineBuffer[LINE_MAX];
uint8_t lineLength = 0;
bool lineTooLong = false;           // did the current line overflow?

// ---------- Helpers ------------------------------------------------

// Bloom percent (0–100) → pulse width, inside the flower's range and
// always inside the hard safety window.
int bloomToPulse(int percent) {
  long us = map(percent, 0, 100, CLOSED_US, OPEN_US);
  if (us < PULSE_MIN) us = PULSE_MIN;
  if (us > PULSE_MAX) us = PULSE_MAX;
  return (int)us;
}

// Pulse width → degrees (0–180), for reporting.
int pulseToDegrees(int us) {
  return (int)map(us, PULSE_MIN, PULSE_MAX, 0, 180);
}

const __FlashStringHelper *stateName(uint8_t s) {
  if (s == STATE_FOLLOWING) return F("FOLLOWING");
  if (s == STATE_CLOSING) return F("CLOSING");
  return F("WAITING");
}

// Every state change goes through here, so it's always announced.
void enterState(uint8_t newState) {
  if (newState == state) return;
  state = newState;
  Serial.print(F("# state "));
  Serial.println(stateName(state));
}

void printStatus() {
  Serial.print(F("# state="));
  Serial.print(stateName(state));
  Serial.print(F(" bloom="));
  Serial.print(bloomPercent);
  Serial.print(F("% pulse="));
  Serial.print(currentUs);
  Serial.print(F("us target="));
  Serial.print(targetUs);
  Serial.print(F("us angle="));
  Serial.println(pulseToDegrees(currentUs));
}

// ---------- Handling one complete line -----------------------------

void handleLine(const char *line) {
  if (line[0] == '\0') return;               // ignore empty lines

  if (line[0] == '?' && line[1] == '\0') {
    printStatus();
    return;
  }

  // Expect exactly "B:" followed by digits.
  if (line[0] != 'B' || line[1] != ':') {
    Serial.print(F("ERR:unknown "));
    Serial.println(line);
    return;
  }

  const char *digits = line + 2;
  if (*digits == '\0') {
    Serial.println(F("ERR:missing number"));
    return;
  }
  for (const char *c = digits; *c != '\0'; c++) {
    if (*c < '0' || *c > '9') {              // no signs, spaces or letters
      Serial.print(F("ERR:bad number "));
      Serial.println(line);
      return;
    }
  }

  long value = strtol(digits, NULL, 10);
  if (value > 100) {
    value = 100;
    Serial.println(F("# clamped to 100"));
  }

  // A valid message: follow it and reset the watchdog.
  bloomPercent = (int)value;
  targetUs = bloomToPulse(bloomPercent);
  lastMessageAt = millis();
  enterState(STATE_FOLLOWING);
}

// ---------- Reading serial lines (from phase 0) --------------------

void readSerialLines() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (lineTooLong) {
        Serial.println(F("ERR:line too long"));
      } else {
        lineBuffer[lineLength] = '\0';
        handleLine(lineBuffer);
      }
      lineLength = 0;
      lineTooLong = false;
    } else if (lineLength < LINE_MAX - 1) {
      lineBuffer[lineLength] = c;
      lineLength++;
    } else {
      lineTooLong = true;   // reject the whole line rather than act on half of it
    }
  }
}

// ---------- Watchdog -----------------------------------------------

void checkWatchdog() {
  if (state != STATE_FOLLOWING) return;
  if (millis() - lastMessageAt >= WATCHDOG_MS) {
    bloomPercent = 0;
    targetUs = CLOSED_US;
    enterState(STATE_CLOSING);
  }
}

// ---------- Speed-limited movement (from phases 1 and 2) -----------

void updateServo() {
  unsigned long now = millis();
  if (now - lastStepAt < STEP_INTERVAL_MS) return;
  lastStepAt = now;

  int stepUs = (int)((long)SPEED_US_PER_SEC * STEP_INTERVAL_MS / 1000);
  int difference = targetUs - currentUs;
  if (difference > stepUs) difference = stepUs;
  if (difference < -stepUs) difference = -stepUs;

  if (difference != 0) {
    currentUs += difference;
    servo.writeMicroseconds(currentUs);
  } else if (state == STATE_CLOSING) {
    enterState(STATE_WAITING);               // fully closed: rest
  }
}

// ---------- Reporting the angle ------------------------------------

void reportAngle() {
  unsigned long now = millis();
  if (now - lastReportAt < REPORT_INTERVAL_MS) return;
  if (currentUs == lastReportedUs) return;   // nothing new to say
  lastReportAt = now;
  lastReportedUs = currentUs;
  Serial.print(F("A:"));
  Serial.println(pulseToDegrees(currentUs));
}

// ---------- Setup and loop -----------------------------------------

void setup() {
  Serial.begin(BAUD_RATE);

  // Start closed. The servo's real position is unknown after a reset,
  // so this first move is a quick jump to closed, the flower's resting shape.
  servo.writeMicroseconds(CLOSED_US);
  servo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);

  Serial.println(F("READY"));
  Serial.println(F("# bloom firmware: send B:0 to B:100, or ? for status"));
}

void loop() {
  readSerialLines();
  checkWatchdog();
  updateServo();
  reportAngle();
}