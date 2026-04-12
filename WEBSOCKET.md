# WebSocket API — P5 Matrix

The firmware exposes a **WebSocket** server on **port 81** (`WS_PORT` in `config.h`) when the device is in **STA mode** (connected WiFi). The WebSocket is **not** available while the **captive portal** AP is active.

- **URL**: `ws://<device-ip>:81`
- **Stack**: [links2004/WebSockets](https://github.com/Links2004/arduinoWebSockets) (`WebSocketsServer`)
- **Human-readable help** (HTTP): `http://<device-ip>/msg`

---

## Behavior summary

| Event | What happens |
|--------|----------------|
| Client connects | Server sends one **device status** JSON message (same shape as below). |
| Plain text `status` or `getStatus` | Server replies with **device status** JSON. |
| JSON with `action` or `cmd` = `status` / `getStatus` | Server replies with **device status** JSON. |
| JSON with `status` / `nameplatevol` / `nameplate` / `displaytime` | Updates the stored fields, redraws the split nameplate layout, applies optional auto-clear, then sends **device status** JSON. |
| Any other text (non-JSON, or JSON that does not match the handlers) | Matrix shows **four blue corner markers** on black; payload is logged on serial if under 256 bytes. |

**Payload limit**: incoming messages must be **under 384 bytes** (buffer size in firmware).

---

## Device status JSON (outgoing)

Sent on connect, after a successful display update, and in response to status requests.

```json
{"status":"<string>","nameplatevol":"<string>","nameplate":"<string>"}
```

Default values before any client update are defined in `config.h` (`DEVICE_WS_STATUS`, `DEVICE_NAMEPLATE_VOL`, `DEVICE_NAMEPLATE`).

Strings are trimmed; each field is capped at **32 characters** when applied from incoming JSON.

---

## Display update JSON (incoming)

Send a JSON object that includes **at least one** of: `status`, `nameplatevol`, `nameplate`, `displaytime`.

### Fields

| Field | Role |
|--------|------|
| `status` | Left half of the 128×32 layout (status line). Special values below. |
| `nameplatevol` | Top-right region (e.g. weight or zone). |
| `nameplate` | Bottom-right region (e.g. ID or plate). |
| `displaytime` | Optional. Seconds until the panel **auto-clears** (full black, internal status becomes `"clear"`). |

**Omitting a key** means that field is **not** changed on this message (only present keys are applied).

### `status` special cases (case-insensitive)

- **`clear`** — Entire panel goes black; no nameplate artwork.
- **`entrance`** or **`allowed`** — Left line shows **WELCOME** (green styling).
- **`exit`** — Left line shows **BYE BYE** (amber styling).
- **`not-allowed`**, **`notallowed`**, or **`not_allowed`** — Left line shows **CHECK** (red styling).
- Any other non-empty string — Used as the left status text as-is (after trim).

### `displaytime`

- **Key omitted** — Cancels a pending auto-clear timer (display stays until changed).
- **`0`** — Cancels a pending auto-clear timer.
- **Positive number** — After that many seconds, the panel clears (black). Ignored if `status` is already `clear`. Capped at **7 days** maximum.

Example:

```json
{"status":"entrance","nameplatevol":"47W","nameplate":"98765","displaytime":10}
```

Clear both panels immediately:

```json
{"status":"clear"}
```

---

## Layout

The display is **128×32** (two 64×32 panels): **left** = status line; **right** = volume on top half, nameplate on bottom half. The device’s STA IP is used internally when redrawing after updates (see `drawNameplateBoard` in the source).

---

## Plain-text shortcuts

Instead of JSON, you may send exactly:

- `status` (6 characters), or  
- `getStatus` (9 characters)  

to receive the current **device status** JSON with no display change.

---

## Troubleshooting

- **Cannot connect** — Confirm STA IP and port **81**; ensure you are not in AP / captive portal mode.
- **Unexpected blue squares** — Message was not parsed as handled JSON; check syntax, size (under 384 bytes), and that your update includes at least one of the recognized keys.
- **Serial** — Unknown or non-JSON payloads may print: `WS not JSON / unknown: ...`
