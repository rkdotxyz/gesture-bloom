/*
  serial.js  —  Gesture Bloom, Web Serial connection
  ------------------------------------------------------------------
  What it is:   A small wrapper around Chrome's Web Serial API that
                speaks our protocol (docs/protocol.md): it opens the
                Uno's port at 115200 baud, splits incoming text into
                lines, waits for READY, and sends lines like "B:57".
  Why it exists: Web Serial works with streams of raw bytes, which is
                powerful but fiddly. main.js (and later the hand
                tracker) should only ever need: connect(), send(),
                disconnect(), and callbacks for lines and status.
  How it fits:  Phase 4 drives it from a slider; phase 7 from your hand.
                Nothing in here knows about sliders or hands.
  ------------------------------------------------------------------
*/

// Status values reported through onStatus(). Plain strings, easy to show.
export const STATUS = {
  UNSUPPORTED: 'unsupported',   // browser has no Web Serial (not Chrome/Edge)
  IDLE: 'idle',                 // not connected
  OPENING: 'opening',           // port chosen, waiting for READY
  READY: 'ready',               // Uno said READY: safe to send
  ERROR: 'error',               // something went wrong (see message)
};

/*
  Split streamed text into complete lines.
  Serial data arrives in arbitrary chunks: "REA", "DY\nA:4", "5\n".
  We keep the unfinished tail in `buffer` until its newline arrives.
  Returns { lines, buffer } where buffer is the leftover partial line.
  Exported on its own so it can be tested without any hardware.
*/
export function splitLines(buffer, chunk) {
  const parts = (buffer + chunk).split('\n');
  const rest = parts.pop();                 // after the last \n: incomplete
  const lines = parts
    .map((line) => line.replace(/\r$/, '')) // tolerate \r\n endings
    .filter((line) => line.length > 0);     // ignore blank lines
  return { lines, buffer: rest };
}

export class BloomSerial {
  /*
    onLine(line)             called for every complete line from the Uno
    onStatus(status, detail) called whenever the connection state changes
  */
  constructor({ onLine = () => {}, onStatus = () => {} } = {}) {
    this.onLine = onLine;
    this.onStatus = onStatus;
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.readableClosed = null;   // promise: resolves when reading stops
    this.writableClosed = null;   // promise: resolves when writing stops
    this.status = STATUS.IDLE;
    this.readyTimer = null;
  }

  static isSupported() {
    return typeof navigator !== 'undefined' && 'serial' in navigator;
  }

  setStatus(status, detail = '') {
    this.status = status;
    this.onStatus(status, detail);
  }

  get isReady() {
    return this.status === STATUS.READY;
  }

  /*
    Ask the user to pick a port, open it, and start reading.
    Must be called from a user action (a button click): Chrome only
    shows the port picker in response to a click, for privacy.
    `portOverride` lets tests pass a fake port instead of the picker.
  */
  async connect(portOverride = null) {
    if (!portOverride && !BloomSerial.isSupported()) {
      this.setStatus(STATUS.UNSUPPORTED, 'Web Serial needs Chrome or Edge, on localhost.');
      return;
    }
    try {
      this.port = portOverride || (await navigator.serial.requestPort());
      await this.port.open({ baudRate: 115200 });
    } catch (err) {
      // NotFoundError = the user closed the picker without choosing.
      if (err.name === 'NotFoundError') {
        this.setStatus(STATUS.IDLE);
      } else {
        // Most common cause: the Arduino IDE's Serial Monitor holds the port.
        this.setStatus(STATUS.ERROR, `Couldn't open the port (${err.message}). Is the Arduino Serial Monitor still open?`);
      }
      this.port = null;
      return;
    }

    // Opening the port resets the Uno, so it isn't listening yet.
    this.setStatus(STATUS.OPENING, 'Port open, waiting for READY…');
    this.readyTimer = setTimeout(() => {
      if (this.status === STATUS.OPENING) {
        this.setStatus(STATUS.OPENING, 'Still no READY after 4 s. Is bloom.ino uploaded, at 115200 baud?');
      }
    }, 4000);

    // Writing: text → bytes → port. We write strings to `this.writer`.
    const encoder = new TextEncoderStream();
    this.writableClosed = encoder.readable.pipeTo(this.port.writable);
    this.writer = encoder.writable.getWriter();

    // Reading: port → bytes → text. We read strings from `this.reader`.
    const decoder = new TextDecoderStream();
    this.readableClosed = this.port.readable.pipeTo(decoder.writable);
    this.reader = decoder.readable.getReader();

    this.readLoop();   // runs in the background until disconnect
  }

  async readLoop() {
    let buffer = '';
    try {
      while (true) {
        const { value, done } = await this.reader.read();
        if (done) break;                       // reader was cancelled
        const result = splitLines(buffer, value);
        buffer = result.buffer;
        for (const line of result.lines) this.handleLine(line);
      }
    } catch (err) {
      // Reading fails when the cable is pulled or the Uno resets hard.
      this.setStatus(STATUS.ERROR, `Connection lost (${err.message}).`);
      await this.cleanup();
    }
  }

  handleLine(line) {
    if (line === 'READY') {
      clearTimeout(this.readyTimer);
      // A READY while already connected means the Uno reset on its own:
      // almost always a brownout from the servo. Worth shouting about.
      const detail = this.status === STATUS.READY
        ? 'The Uno reset unexpectedly (a brownout?). Reconnected.'
        : 'Connected.';
      this.setStatus(STATUS.READY, detail);
    }
    this.onLine(line);
  }

  // Send one protocol line. Returns false if not ready (nothing is sent).
  async send(line) {
    if (!this.isReady || !this.writer) return false;
    try {
      await this.writer.write(line + '\n');
      return true;
    } catch (err) {
      this.setStatus(STATUS.ERROR, `Send failed (${err.message}).`);
      return false;
    }
  }

  // Close everything in the right order: stop reading, stop writing, close.
  async cleanup() {
    clearTimeout(this.readyTimer);
    try { if (this.reader) await this.reader.cancel(); } catch (_) { /* already closed */ }
    try { if (this.readableClosed) await this.readableClosed; } catch (_) { /* expected after cancel */ }
    try { if (this.writer) await this.writer.close(); } catch (_) { /* already closed */ }
    try { if (this.writableClosed) await this.writableClosed; } catch (_) { /* already closed */ }
    try { if (this.port) await this.port.close(); } catch (_) { /* already closed */ }
    this.reader = this.writer = this.readableClosed = this.writableClosed = this.port = null;
  }

  async disconnect() {
    await this.cleanup();
    this.setStatus(STATUS.IDLE, 'Disconnected. The Uno will close the flower in 1.5 s.');
  }
}