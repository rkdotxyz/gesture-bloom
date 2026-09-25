/*
  pot_servo.ino  —  Gesture Bloom, Phase 2
  ------------------------------------------------------------------
  What it is:   Turn the trimmer, and the servo follows.
  Why it exists: Your first analog input. The trimmer produces a
                continuously varying voltage; the Uno measures it and
                turns it into a servo position. That is exactly the
                shape of the final project, with "how far the trimmer
                is turned" standing in for "how open your hand is".
  How it fits:  It keeps phase 1's speed-limited movement and adds the
                two tricks that stop a servo buzzing when its input is
                noisy: smoothing and a deadband. The browser will do
                the same with hand data in phase 7.

  Watch it:     Tools → Serial Plotter (115200 baud) draws the raw and
                smoothed readings as live lines.
  Commands:     s = smoothing on/off (compare the jitter)
                d = detach / re-attach the servo (limp and quiet)
  ------------------------------------------------------------------
*/

#include <Servo.h>

// ---------- Settings ----------------------------------------------

const uint8_t SERVO_PIN = 9;       // servo orange
const uint8_t POT_PIN = A0;        // trimmer middle leg (wiper)
const unsigned long BAUD_RATE = 115200;

// Your phase 1 results: the servo moves freely across 500–2500 µs.
const int PULSE_MIN = 500;
const int PULSE_MAX = 2500;

// Smoothing: each reading only moves the smoothed value 1/SMOOTHING of
// the way toward it (an "exponential moving average"). Higher = calmer
// but laggier. 8 is a good start.
const int SMOOTHING = 8;

// Deadband: ignore target changes smaller than this many µs (8 µs is
// under 1°). This stops the servo "hunting" back and forth over tiny
// noise, which is what makes a still servo buzz.
const int DEADBAND_US = 8;

// Speed limit, as in phase 1. Faster than phase 1's 600 so the servo
// keeps up with your hand, still gentle enough to spare the Uno's 5V.
const int SPEED_US_PER_SEC = 900;
const unsigned long STEP_INTERVAL_MS = 20;   // one step per servo pulse

// How often to read the trimmer and print for the plotter.
const unsigned long READ_INTERVAL_MS = 10;   // 100 readings per second
const unsigned long PRINT_INTERVAL_MS = 50;  // 20 lines per second

// ---------- State --------------------------------------------------

Servo servo;
bool attached = false;
bool smoothingOn = true;

int rawReading = 0;             // latest analogRead, 0–1023
long smoothedX16 = 0;           // smoothed reading × 16 (see below)
int currentUs = 1500;           // pulse being sent now
int targetUs = 1500;            // where the trimmer says to go

unsigned long lastReadAt = 0;
unsigned long lastStepAt = 0;
unsigned long lastPrintAt = 0;

// Why "× 16"? Dividing whole numbers throws away the remainder, so a
// plain integer average gets stuck a few steps short of the real value.
// Keeping 16× the value keeps 4 extra bits of precision, without using
// slow floating-point maths on the Uno.

// ---------- Reading the trimmer ------------------------------------

void readTrimmer() {
  unsigned long now = millis();
  if (now - lastReadAt < READ_INTERVAL_MS) return;
  lastReadAt = now;

  // analogRead measures the wiper voltage against the Uno's 5V:
  // 0 V → 0, 5 V → 1023. Because the trimmer is also fed from that
  // same 5V, a sag (like the LED dimming you saw) shifts both together,
  // so the reading barely changes. This is called "ratiometric".
  rawReading = analogRead(POT_PIN);

  if (smoothingOn) {
    // Move 1/SMOOTHING of the way from the smoothed value to the new one.
    smoothedX16 += ((long)rawReading * 16 - smoothedX16) / SMOOTHING;
  } else {
    smoothedX16 = (long)rawReading * 16;   // no smoothing: follow raw
  }

  int smoothed = (int)(smoothedX16 / 16);
  int newTarget = (int)map(smoothed, 0, 1023, PULSE_MIN, PULSE_MAX);

  // Deadband: only accept the new target if it moved far enough.
  if (abs(newTarget - targetUs) >= DEADBAND_US) {
    targetUs = newTarget;
  }
}

// ---------- Speed-limited movement (from phase 1) ------------------

void updateServo() {
  unsigned long now = millis();
  if (now - lastStepAt < STEP_INTERVAL_MS) return;
  lastStepAt = now;
  if (!attached) return;

  int stepUs = (int)((long)SPEED_US_PER_SEC * STEP_INTERVAL_MS / 1000);
  int difference = targetUs - currentUs;
  if (difference > stepUs) difference = stepUs;
  if (difference < -stepUs) difference = -stepUs;

  if (difference != 0) {
    currentUs += difference;
    servo.writeMicroseconds(currentUs);
  }
}

// ---------- Printing for the Serial Plotter ------------------------

// "label:value" pairs separated by spaces. The Serial Plotter draws each
// label as its own coloured line; the Serial Monitor shows the text.
void printForPlotter() {
  unsigned long now = millis();
  if (now - lastPrintAt < PRINT_INTERVAL_MS) return;
  lastPrintAt = now;

  Serial.print(F("raw:"));
  Serial.print(rawReading);
  Serial.print(F(" smooth:"));
  Serial.print((int)(smoothedX16 / 16));
  // Pulse scaled to the same 0–1023 range so all three lines fit one plot.
  Serial.print(F(" servo:"));
  Serial.println((int)map(currentUs, PULSE_MIN, PULSE_MAX, 0, 1023));
}

// ---------- Single-key commands ------------------------------------

void readCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 's') {
      smoothingOn = !smoothingOn;
      Serial.println(smoothingOn ? F("# smoothing ON") : F("# smoothing OFF"));
    } else if (c == 'd') {
      if (attached) {
        servo.detach();
        attached = false;
        Serial.println(F("# servo detached (limp)"));
      } else {
        servo.writeMicroseconds(currentUs);   // no jump on re-attach
        servo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
        attached = true;
        Serial.println(F("# servo attached"));
      }
    }
    // Newlines and anything else are ignored.
  }
}

// ---------- Setup and loop -----------------------------------------

void setup() {
  Serial.begin(BAUD_RATE);
  Serial.println(F("READY"));
  Serial.println(F("# pot_servo: turn the trimmer. s = smoothing on/off, d = detach"));

  // Start the smoothed value AT the trimmer's current position, so the
  // average doesn't have to climb up from 0 at power-on.
  rawReading = analogRead(POT_PIN);
  smoothedX16 = (long)rawReading * 16;
  targetUs = (int)map(rawReading, 0, 1023, PULSE_MIN, PULSE_MAX);
  currentUs = targetUs;

  // The servo goes straight to where the trimmer points: one quick jump.
  servo.writeMicroseconds(currentUs);
  servo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
  attached = true;
}

void loop() {
  readCommands();
  readTrimmer();
  updateServo();
  printForPlotter();
}