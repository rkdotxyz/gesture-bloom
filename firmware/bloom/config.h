/*
  config.h  —  Gesture Bloom firmware settings
  ------------------------------------------------------------------
  Every number you might want to tune lives here, so bloom.ino never
  needs editing just to adjust the flower. This file sits in the same
  folder as bloom.ino and shows up as a second tab in the Arduino IDE.
  ------------------------------------------------------------------
*/

#pragma once   // include this file only once, even if referenced twice

// ---------- Pins and serial ----------------------------------------
const uint8_t SERVO_PIN = 9;             // servo orange wire
const unsigned long BAUD_RATE = 115200;  // must match the browser later

// ---------- Hard safety window (from your phase 1 results) ---------
// The servo is NEVER sent a pulse outside this range, whatever arrives.
const int PULSE_MIN = 500;
const int PULSE_MAX = 2500;

// ---------- The flower's range -------------------------------------
// Pulse widths for a fully closed and a fully open flower.
// PLACEHOLDERS: a safe middle chunk of the servo's range. In phase 5
// you'll find the real values with the browser slider and put them here.
// CLOSED_US may be larger than OPEN_US if your mechanism winds the
// other way; the maths handles either direction.
const int CLOSED_US = 1000;
const int OPEN_US = 2000;

// ---------- Motion -------------------------------------------------
// Maximum change in pulse width per second. 900 µs/s ≈ 81°/s.
// Your phase 1 test showed faster moves make the Uno's 5V sag,
// so stay moderate while the servo runs on Uno power.
const int SPEED_US_PER_SEC = 900;
const unsigned long STEP_INTERVAL_MS = 20;   // one step per servo pulse

// ---------- Timing -------------------------------------------------
// If no valid message arrives for this long, the flower eases closed.
// The browser sends a keep-alive at least every 500 ms, so 1500 ms
// means three missed messages in a row.
const unsigned long WATCHDOG_MS = 1500;

// How often the current angle is reported back (only when it changed).
const unsigned long REPORT_INTERVAL_MS = 200;

// Longest line we accept. "B:100" is 5 characters; 16 leaves headroom.
const uint8_t LINE_MAX = 16;