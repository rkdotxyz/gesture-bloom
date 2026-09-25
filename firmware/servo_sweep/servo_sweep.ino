/*
  servo_sweep.ino  —  Gesture Bloom, Phase 1
  ------------------------------------------------------------------
  What it is:   An interactive servo test bench, driven by commands
                you type into the Serial Monitor.
  Why it exists: Before a flower is attached, we need three facts about
                YOUR servo: that it's a 180° (positional) model, the
                pulse range where it moves freely without buzzing
                against its end stops, and that the power wiring holds
                up while it moves.
  How it fits:  It reuses both patterns from phase 0 (millis() timing,
                reading typed lines) and adds the third building block
                of the real firmware: moving at a limited speed toward
                a target, instead of jumping there at full speed.

  Commands (type, then press Enter; line ending = "New Line"):
    a90     go to 90 degrees (0–180, mapped onto PULSE_MIN..PULSE_MAX)
    u1500   go to a raw pulse width in microseconds (500–2500)
    s       start/stop a slow sweep between sweepLowUs and sweepHighUs
    v600    set the speed limit in µs per second (100–3000); higher = harsher
            starts = bigger current spikes (used in the power experiment)
    d       detach: stop sending pulses, the servo goes limp and quiet
    ?       print the current status
  ------------------------------------------------------------------
*/

#include <Servo.h>   // built into the Arduino IDE, nothing to install

// ---------- Settings ----------------------------------------------

const uint8_t SERVO_PIN = 9;          // orange servo wire goes to D9
const unsigned long BAUD_RATE = 115200;

// A hobby servo reads the WIDTH of a pulse sent every 20 ms:
// ~1500 µs = centre, shorter = one way, longer = the other way.
// 500–2500 µs is the widest range an MG996R might accept. Your servo
// may hit its internal end stop before these limits: that's what
// this phase finds out. Never command beyond these two numbers.
const int PULSE_MIN = 500;
const int PULSE_MAX = 2500;
const int PULSE_CENTER = 1500;

// The sweep starts deliberately narrow (roughly 45°–135°). Widen it
// only after checking the ends by hand with the u command.
int sweepLowUs = 1000;
int sweepHighUs = 2000;

// Speed limit: how many microseconds of pulse width the servo may
// change per second. 600 µs/s is about 54°/s, slow and easy to watch.
// The real firmware keeps a limit like this so the flower can't be yanked.
// It's a variable (not const) so the v command can change it while running.
const int SPEED_DEFAULT = 600;
const int SPEED_MIN = 100;
const int SPEED_MAX = 3000;
int speedUsPerSec = SPEED_DEFAULT;

// How often we nudge the servo toward its target. 20 ms matches the
// servo's own pulse rate, so every pulse carries a fresh position.
const unsigned long STEP_INTERVAL_MS = 20;


// ---------- State --------------------------------------------------

Servo servo;                      // the library object that makes the pulses
bool attached = false;            // are we currently sending pulses?
int currentUs = PULSE_CENTER;     // pulse width being sent right now
int targetUs = PULSE_CENTER;      // where we're heading
bool sweeping = false;            // is the slow sweep running?
unsigned long lastStepAt = 0;     // millis() of the last nudge

char lineBuffer[32];              // same line reader as phase 0
uint8_t lineLength = 0;

// ---------- Small helpers ------------------------------------------

// Keep any requested pulse inside the safe window.
int clampPulse(long us) {
  if (us < PULSE_MIN) return PULSE_MIN;
  if (us > PULSE_MAX) return PULSE_MAX;
  return (int)us;
}

// Degrees (0–180) → pulse width, using the same window.
int degreesToPulse(long deg) {
  if (deg < 0) deg = 0;
  if (deg > 180) deg = 180;
  return (int)map(deg, 0, 180, PULSE_MIN, PULSE_MAX);
}

// Pulse width → approximate degrees, just for printing.
int pulseToDegrees(int us) {
  return (int)map(us, PULSE_MIN, PULSE_MAX, 0, 180);
}

// Start sending pulses again after a 'd'. We write the current position
// BEFORE attaching, so the servo doesn't jump to the library's default
// (centre) for a moment.
void ensureAttached() {
  if (!attached) {
    servo.writeMicroseconds(currentUs);
    servo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
    attached = true;
    Serial.println(F("Attached (sending pulses)."));
  }
}

void printStatus() {
  Serial.print(F("pulse="));
  Serial.print(currentUs);
  Serial.print(F("us (~"));
  Serial.print(pulseToDegrees(currentUs));
  Serial.print(F(" deg)  target="));
  Serial.print(targetUs);
  Serial.print(F("us  speed="));
  Serial.print(speedUsPerSec);
  Serial.print(F("us/s  sweep="));
  Serial.print(sweeping ? F("on") : F("off"));
  Serial.print(F("  attached="));
  Serial.println(attached ? F("yes") : F("no"));
}

// ---------- Reading a number safely --------------------------------

// Turns the text after the command letter into a number.
// Returns false if it isn't a clean whole number (e.g. "a9x" or "a").
bool parseNumber(const char *text, long &out) {
  if (*text == '\0') return false;          // nothing after the letter
  char *end;
  out = strtol(text, &end, 10);             // base-10 conversion
  return *end == '\0';                      // must have used every character
}

// ---------- Commands -----------------------------------------------

void handleLine(const char *line) {
  char command = line[0];
  long value = 0;

  if (command == 'a') {
    if (!parseNumber(line + 1, value)) {
      Serial.println(F("ERR: use a0 to a180, e.g. a90"));
      return;
    }
    sweeping = false;
    ensureAttached();
    targetUs = degreesToPulse(value);
    Serial.print(F("Moving to ~"));
    Serial.print(value);
    Serial.print(F(" deg = "));
    Serial.print(targetUs);
    Serial.println(F("us"));

  } else if (command == 'u') {
    if (!parseNumber(line + 1, value)) {
      Serial.println(F("ERR: use u500 to u2500, e.g. u1500"));
      return;
    }
    sweeping = false;
    ensureAttached();
    targetUs = clampPulse(value);
    if (targetUs != value) {
      Serial.println(F("(clamped to the 500-2500 safe window)"));
    }
    Serial.print(F("Moving to "));
    Serial.print(targetUs);
    Serial.println(F("us"));

  } else if (command == 's') {
    sweeping = !sweeping;
    if (sweeping) {
      ensureAttached();
      targetUs = sweepHighUs;
      Serial.println(F("Sweep ON. Type s again to stop."));
    } else {
      targetUs = currentUs;                 // stop where we are
      Serial.println(F("Sweep OFF."));
    }

  } else if (command == 'v') {
    if (!parseNumber(line + 1, value) || value < SPEED_MIN || value > SPEED_MAX) {
      Serial.println(F("ERR: use v100 to v3000, e.g. v600"));
      return;
    }
    speedUsPerSec = (int)value;
    Serial.print(F("Speed limit now "));
    Serial.print(speedUsPerSec);
    Serial.println(F(" us/s"));

  } else if (command == 'd') {
    sweeping = false;
    servo.detach();                         // stops the pulses
    attached = false;
    Serial.println(F("Detached: servo is limp and silent."));

  } else if (command == '?') {
    printStatus();

  } else if (command != '\0') {             // ignore empty lines
    Serial.print(F("ERR: unknown command '"));
    Serial.print(line);
    Serial.println(F("'. Try a90, u1500, s, v600, d or ?"));
  }
}

// ---------- Line reader (from phase 0) -----------------------------

void readSerialLines() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuffer[lineLength] = '\0';
      handleLine(lineBuffer);
      lineLength = 0;
    } else if (lineLength < sizeof(lineBuffer) - 1) {
      lineBuffer[lineLength] = c;
      lineLength++;
    }
  }
}

// ---------- Speed-limited movement ---------------------------------

// Every STEP_INTERVAL_MS, move currentUs a limited step toward targetUs.
void updateServo() {
  unsigned long now = millis();
  if (now - lastStepAt < STEP_INTERVAL_MS) return;   // not time yet
  lastStepAt = now;

  if (!attached) return;

  // How far one nudge may go, e.g. 600 µs/s * 0.02 s = 12 µs per step.
  int stepUs = (int)((long)speedUsPerSec * STEP_INTERVAL_MS / 1000);

  if (currentUs != targetUs) {
    int difference = targetUs - currentUs;
    if (difference > stepUs) difference = stepUs;        // cap each nudge
    if (difference < -stepUs) difference = -stepUs;
    currentUs += difference;
    servo.writeMicroseconds(currentUs);

    if (currentUs == targetUs && !sweeping) {
      Serial.print(F("Arrived: "));
      printStatus();
    }
  } else if (sweeping) {
    // Reached one end of the sweep: turn around.
    targetUs = (targetUs == sweepHighUs) ? sweepLowUs : sweepHighUs;
  }
}

// ---------- Setup and loop -----------------------------------------

void setup() {
  Serial.begin(BAUD_RATE);
  Serial.println(F("READY"));
  Serial.println(F("Servo bench. Commands: a90, u1500, s, v600, d, ?"));

  // Start at the centre. With no horn load this is always safe.
  servo.writeMicroseconds(PULSE_CENTER);
  servo.attach(SERVO_PIN, PULSE_MIN, PULSE_MAX);
  attached = true;
  printStatus();
}

void loop() {
  readSerialLines();
  updateServo();
}