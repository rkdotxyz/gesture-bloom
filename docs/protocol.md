# Gesture Bloom serial protocol

Plain text over USB serial at **115200 baud**. One message per line, each
ending in a newline (`\n`). Carriage returns (`\r`) are ignored.

## Browser → Uno

| Message      | Meaning                                                        |
|--------------|----------------------------------------------------------------|
| `B:<0-100>`  | Target bloom percent. Digits only; values above 100 are clamped. |
| `?`          | Print a status line (for humans in the Serial Monitor).        |

The browser sends `B:` whenever the value changes, and **at least every
500 ms** as a keep-alive while a hand is visible.

## Uno → Browser

| Message        | Meaning                                                   |
|----------------|-----------------------------------------------------------|
| `READY`        | Firmware booted. Send nothing before this.                |
| `A:<degrees>`  | Current servo angle (0–180), sent when it changes, ≤ 5/s. |
| `ERR:<text>`   | A line that couldn't be understood; nothing moved.        |
| `# <text>`     | Human-readable note (state changes, status). Ignore.      |

## Behaviour

- Opening the serial port resets the Uno, so wait for `READY`.
- The Uno maps bloom % onto `CLOSED_US`..`OPEN_US` (config.h) and never
  leaves `PULSE_MIN`..`PULSE_MAX`.
- Movement is speed-limited (`SPEED_US_PER_SEC`).
- Watchdog: no valid `B:` for `WATCHDOG_MS` (1500 ms) → the flower eases
  closed. States: WAITING → FOLLOWING → CLOSING → WAITING.