/*
  blink_test.ino  —  Gesture Bloom, Phase 0
  ------------------------------------------------------------------
  What it is:   The smallest possible "is everything alive?" sketch.
  Why it exists: Before any servo or browser is involved, prove three
                things: the IDE can upload to the Uno, the Uno runs
                code, and the Uno and the Mac can talk over serial.
  How it fits:  The two patterns in here are the backbone of the real
                firmware (phase 3 onward):
                  1. Non-blocking timing with millis() instead of
                     delay(), so the Uno can do several things at once.
                  2. Reading serial input one character at a time into
                     a buffer until a newline arrives (a "line").
                The real firmware will read lines like "B:57" exactly
                the way this sketch reads whatever you type.
  ------------------------------------------------------------------
*/

// ---------- Settings ----------------------------------------------

// LED_BUILTIN is pin 13 on the Uno: the small LED labelled "L".
const uint8_t LED_PIN = LED_BUILTIN;

// Serial speed in bits per second. The Serial Monitor must be set to the
// same number, or you'll see garbage characters. 115200 is the speed the
// browser will use later, so we use it from day one.
const unsigned long BAUD_RATE = 115200;

// A "heartbeat" rhythm: durations in milliseconds, alternating
// ON, OFF, ON, OFF ... starting with ON.
// {80, 120, 80, 720} = two quick blinks, then a long pause (1 s total).
// Change these numbers to make your own rhythm; keep an even count so the
// pattern always ends on an OFF step before it repeats.
const unsigned int PATTERN[] = {80, 120, 80, 720};

// How many steps the pattern has. sizeof(whole array) / sizeof(one item)
// counts the items for us, so the code keeps working if you add steps.
const uint8_t PATTERN_LEN = sizeof(PATTERN) / sizeof(PATTERN[0]);

// ---------- State (values that change while running) --------------

uint8_t patternStep = 0;           // which step of PATTERN we're in
unsigned long stepStartedAt = 0;   // millis() value when that step began

// Incoming text is collected here until Enter (newline) is received.
// A fixed-size char array instead of Arduino's String class: the Uno has
// only 2 KB of RAM, and String can fragment it over long run times.
char lineBuffer[32];               // up to 31 characters + the end marker
uint8_t lineLength = 0;            // how many characters we have so far

// ---------- Setup: runs once at power-up or reset ------------------

void setup() {
  pinMode(LED_PIN, OUTPUT);        // we drive the LED, we don't read it
  digitalWrite(LED_PIN, HIGH);     // step 0 of the pattern is ON

  Serial.begin(BAUD_RATE);

  // "READY" is the first thing the real firmware will print too. The
  // browser waits for it, because opening the serial port resets the Uno.
  // F("...") keeps the text in flash memory instead of precious RAM.
  Serial.println(F("READY"));
  Serial.println(F("Gesture Bloom phase 0. Type something and press Enter."));

  stepStartedAt = millis();
}

// ---------- Heartbeat LED, without delay() --------------------------

// Called on every pass through loop(). It never waits: it only checks
// whether the current step has lasted long enough, and if so moves on.
void updateHeartbeat() {
  unsigned long now = millis();    // milliseconds since the Uno started

  // "now - start >= duration" (rather than "now >= start + duration")
  // stays correct even when millis() wraps back to 0 after ~49 days.
  if (now - stepStartedAt >= PATTERN[patternStep]) {
    stepStartedAt = now;
    patternStep = (patternStep + 1) % PATTERN_LEN;   // % wraps to 0 at the end

    // Even steps (0, 2, ...) are ON, odd steps (1, 3, ...) are OFF.
    digitalWrite(LED_PIN, (patternStep % 2 == 0) ? HIGH : LOW);
  }
}

// ---------- What to do with one complete line ----------------------

void handleLine(const char *line) {
  Serial.print(F("You said: "));
  Serial.println(line);
  Serial.print(F("Uptime (ms): "));
  Serial.println(millis());
}

// ---------- Collect typed characters into lines --------------------

// Serial data arrives one byte at a time, and not all at once. We take
// whatever has arrived so far, add it to the buffer, and only act when
// the newline character ('\n') shows the line is complete.
void readSerialLines() {
  while (Serial.available() > 0) {         // bytes waiting to be read?
    char c = Serial.read();

    if (c == '\r') {
      continue;                            // ignore carriage returns (some
                                           // line-ending settings send \r\n)
    }

    if (c == '\n') {                       // end of line: act on it
      lineBuffer[lineLength] = '\0';       // '\0' marks the end of a C string
      handleLine(lineBuffer);
      lineLength = 0;                      // start a fresh line
    } else if (lineLength < sizeof(lineBuffer) - 1) {
      lineBuffer[lineLength] = c;          // store it; -1 leaves room for '\0'
      lineLength++;
    }
    // If the line is longer than the buffer, extra characters are dropped
    // rather than written past the end of the array (which would corrupt
    // other variables in memory).
  }
}

// ---------- Loop: runs over and over, thousands of times a second ---

void loop() {
  updateHeartbeat();   // each of these returns immediately,
  readSerialLines();   // so both "happen at the same time"
}