# Build log

## Phase 0: Set up and blink (v0.1) · 2026-09-25

**Built:** repo skeleton (`firmware/`, `web/`, `mechanism/`, `docs/`) and
`firmware/blink_test/blink_test.ino`: a heartbeat on the built-in LED using
`millis()` (no `delay()`), plus a serial line reader that echoes typed lines
at 115200 baud.

**Result:** compiles to 2380 bytes (7% of flash), 234 bytes of RAM (11%).
Heartbeat runs steadily while typing; typed lines echo back.
- READY printed again when the Serial Monitor opened (auto-reset): yes / no

**Problem:** upload failed with
`cannot open port /dev/cu.usbserial-0001: No such file or directory`.
**Cause:** the IDE was still pointed at the ESP32's port from an earlier project.
**Fix:** board dropdown → "Select other board and port" → the Uno's port.

**Learned:** `setup()` runs once and `loop()` forever; non-blocking timing
with `millis()` lets several jobs share the board; serial arrives one
character at a time and is assembled into lines on `\n`.

**Next:** Phase 1: breadboard basics, then the Uno-power experiment.


## Phase 1: Power the servo safely (v0.2) · 2026-09-25

**Built:** dual-rail breadboard (top = servo rail, bottom = Uno rail, one
shared ground), LED power indicators on both, capacitor across the servo
rail, and `firmware/servo_sweep/servo_sweep.ino`: an interactive servo
bench (a/u/s/v/d/? commands) with speed-limited movement.

**Experiment: servo powered from the Uno's 5V (laptop USB) via one jumper**
- 180° positional servo confirmed
- Usable range: u500 to u2500, ~180° (no end-stop buzzing)
- Test 1 (gentle sweep): 0 resets, very slight LED dimming
- Test 2 (v2500 sweep): 0 resets, noticeable LED flicker
- Test 3 (no capacitor): not run
- Noise: only normal motor sound while moving, silent at rest

**Verdict:** Uno power copes unloaded. The LED flicker in test 2 shows the 5V
sagging, so keep speeds moderate and retest under load in phase 5. No
breakout needed for now.

**Problem:** servo body jerked on fast moves.
**Cause:** reaction torque; the servo wasn't held down.
**Fix:** tape or screw it down (phase 2).

**Next:** Phase 2: trimmer controls the servo.


## Phase 2: Trimmer drives the servo (v0.3) · 2026-09-25

**Built:** trimmer on the Uno rail (cols 6–8), wiper to A0, and
`firmware/pot_servo/pot_servo.ino`: reads A0 100×/s, smooths it
(exponential moving average, SMOOTHING = 8), ignores changes under
8 µs (deadband), moves at 900 µs/s, and prints raw/smooth/servo
lines for the Serial Plotter. The servo is now held down.

**Result:** the servo follows the trimmer across its range and is still
when untouched. Smoothing ON feels clearly better; OFF feels jerky.
Kept the defaults (SMOOTHING 8, DEADBAND_US 8, SPEED 900).

**Learned:** a trimmer is a voltage divider; analogRead maps 0–5 V to
0–1023; the reading is ratiometric, so Uno 5V sag cancels out;
smoothing plus a deadband is what keeps a servo quiet on noisy input.

**Next:** Phase 3: serial protocol firmware.