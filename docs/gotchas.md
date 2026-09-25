# Gotchas

## Arduino IDE remembers the last port
If upload fails with "cannot open port … No such file or directory", the IDE
is pointed at a port that isn't there (e.g. `usbserial-0001` = the ESP32).
Pick the Uno's port via the board dropdown → "Select other board and port".
Unplug/replug the Uno while the list is open to see which port is its.

## LEDs on the power rails are voltage-sag detectors
A flicker when the servo starts = the 5V line dipping. No resets yet, but
it's the early warning. Keep servo speed moderate on Uno power.

## An unmounted servo jerks
The body twists against the horn's motion. Tape or screw it down.