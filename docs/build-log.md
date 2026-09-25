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