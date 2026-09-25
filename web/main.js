/*
  main.js  —  Gesture Bloom, phase 4: slider → Uno
  ------------------------------------------------------------------
  What it is:   Wires the page's controls to BloomSerial.
  Why it exists: To prove the browser can drive the flower before any
                camera is involved, and to give you a precise manual
                control for tuning the mechanism in phase 5.
  How it fits:  The sending rules below (only on change, capped rate,
                keep-alive every 500 ms) are exactly what the hand
                tracker will use in phase 7; only the source of the
                number changes.
  ------------------------------------------------------------------
*/

import { BloomSerial, STATUS } from './serial.js';

// ---------- Sending rules (match docs/protocol.md) -----------------
const TICK_MS = 50;          // check for something to send 20× per second
const KEEPALIVE_MS = 500;    // resend at least this often, or the Uno closes
const LOG_LIMIT = 80;        // lines kept in the on-page log

// ---------- Page elements ------------------------------------------
const $ = (id) => document.getElementById(id);
const connectBtn = $('connect');
const statusPill = $('status');
const statusDetail = $('status-detail');
const slider = $('bloom');
const bloomValue = $('bloom-value');
const angleValue = $('angle-value');
const keepAliveBox = $('keepalive');
const showSentBox = $('show-sent');
const logBox = $('log');
const presetButtons = document.querySelectorAll('[data-preset]');

// ---------- State --------------------------------------------------
let lastSentValue = null;   // last bloom value actually sent
let lastSentAt = 0;         // performance.now() of that send
let resetCount = 0;         // unexpected READYs (brownouts)

// ---------- Log ----------------------------------------------------
function log(text, kind = 'in') {
  const row = document.createElement('div');
  row.className = `log-row ${kind}`;
  const time = new Date().toLocaleTimeString([], { hour12: false });
  row.textContent = `${time}  ${kind === 'out' ? '→' : '←'} ${text}`;
  logBox.prepend(row);                       // newest on top
  while (logBox.children.length > LOG_LIMIT) logBox.lastChild.remove();
}

// ---------- Serial connection --------------------------------------
const serial = new BloomSerial({
  onLine(line) {
    if (line.startsWith('A:')) {
      angleValue.textContent = `${line.slice(2)}°`;
    }
    if (line.startsWith('ERR:')) {
      log(line, 'err');
      return;
    }
    log(line, 'in');
  },

  onStatus(status, detail) {
    statusPill.dataset.status = status;
    statusPill.textContent = {
      [STATUS.UNSUPPORTED]: 'Unsupported browser',
      [STATUS.IDLE]: 'Not connected',
      [STATUS.OPENING]: 'Connecting…',
      [STATUS.READY]: 'Connected',
      [STATUS.ERROR]: 'Error',
    }[status];
    statusDetail.textContent = detail;

    if (detail.startsWith('The Uno reset')) {
      resetCount += 1;
      log(`# unexpected reset #${resetCount}`, 'err');
    }

    const connected = status === STATUS.OPENING || status === STATUS.READY;
    connectBtn.textContent = connected ? 'Disconnect' : 'Connect to Uno';
    slider.disabled = status !== STATUS.READY;
    presetButtons.forEach((b) => { b.disabled = status !== STATUS.READY; });

    // After a (re)connect, force the current slider value to go out.
    if (status === STATUS.READY) lastSentValue = null;
  },
});

connectBtn.addEventListener('click', async () => {
  if (serial.status === STATUS.OPENING || serial.status === STATUS.READY) {
    await serial.disconnect();
  } else {
    await serial.connect();
  }
});

// ---------- Slider -------------------------------------------------
function showBloom() {
  bloomValue.textContent = `${slider.value}%`;
}
slider.addEventListener('input', showBloom);

presetButtons.forEach((button) => {
  button.addEventListener('click', () => {
    slider.value = button.dataset.preset;
    showBloom();
  });
});

// ---------- The send loop ------------------------------------------
// The slider can fire dozens of events per second. Instead of sending on
// every event, a steady tick sends at most 20 messages a second: only
// when the value changed, or when a keep-alive is due.
setInterval(async () => {
  if (!serial.isReady) return;

  const value = Number(slider.value);
  const now = performance.now();
  const changed = value !== lastSentValue;
  const keepAliveDue = keepAliveBox.checked && now - lastSentAt >= KEEPALIVE_MS;

  if (!changed && !keepAliveDue) return;

  const message = `B:${value}`;
  const sent = await serial.send(message);
  if (sent) {
    lastSentValue = value;
    lastSentAt = now;
    // Keep-alives would flood the log, so only changes are shown
    // (unless "show every message" is ticked).
    if (changed || showSentBox.checked) log(message, 'out');
  }
}, TICK_MS);

// ---------- Start ---------------------------------------------------
showBloom();
if (!BloomSerial.isSupported()) {
  serial.setStatus(STATUS.UNSUPPORTED, 'Web Serial needs Chrome or Edge, and the page must come from localhost.');
  connectBtn.disabled = true;
} else {
  serial.setStatus(STATUS.IDLE, 'Close the Arduino Serial Monitor first, then connect.');
}