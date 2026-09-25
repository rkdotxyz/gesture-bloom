# Gesture Bloom

Open your hand, and a paper flower opens with it.

A webcam and MediaPipe in Chrome measure how open your hand is. The browser
sends that value over USB (Web Serial) to an Arduino Uno, which drives an
MG996R servo. The servo winds thread that pulls the petals of a paper flower
closed; unwinding lets them fall open.

## Status

Phase 0: repo set up, board alive, serial echo working.

## Roadmap

- [x] Phase 0: set up and blink
- [ ] Phase 1: power the servo safely
- [ ] Phase 2: the potentiometer drives the servo
- [ ] Phase 3: talk to the Uno (serial protocol)
- [ ] Phase 4: a browser slider moves the servo
- [ ] Phase 5: build the flower
- [ ] Phase 6: track your hand
- [ ] Phase 7: your hand blooms the flower (v1.0)
- [ ] Phase 8: polish
- [ ] Phase 9: document and publish

## Hardware

Arduino Uno R3, MG996R servo (180°), servo powered from the Uno's 5V as a
tested experiment (a USB-C breakout takes over if that proves unreliable), 470–1000 µF capacitor, 10 kΩ trimmer
potentiometer, breadboard, LEDs + resistors as rail testers, webcam.

## Structure

- `firmware/` Arduino sketches (each in a folder with the same name)
- `web/` the browser page (hand tracking + Web Serial)
- `mechanism/` petal templates, photos and notes
- `docs/` build log and gotchas