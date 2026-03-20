# ESP32-S3 P5-1921 128×32 LED Matrix (2× 64×32 side by side)

HUB75 LED matrix display: **2 panels side by side** (64×32 × 2 = 128×32), with WiFi (AP + STA), captive portal, WebSocket JSON nameplate updates, and digital I/O. No E pin required.

## Pin Layout

### HUB75 Matrix (P5-1921-64x32-8S × 2, 1/8 scan, horizontal chain)

| HUB75 Signal | GPIO | Description |
|--------------|------|-------------|
| R1 | 42 | Red data (top half) |
| G1 | 41 | Green data (top) |
| B1 | 40 | Blue data (top) |
| R2 | 38 | Red data (bottom) |
| G2 | 39 | Green data (bottom) |
| B2 | 37 | Blue data (bottom) |
| A | 45 | Row address bit 0 |
| B | 36 | Row address bit 1 |
| C | 21 | Row address bit 2 *(was 48; freed for NeoPixel)* |
| D | 35 | Row address bit 3 |
| E | - | Not used (32px height) |
| LAT | 47 | Latch / Strobe |
| OE | 14 | Output Enable |
| CLK | 2 | Clock |

### System

| Function | GPIO | Mode |
|----------|------|------|
| Reset button | 0 | INPUT_PULLUP (hold 5s → WiFi reset) |
| Status NeoPixel | 48 | Built-in LED (DevKit v1.0) |

### Digital I/O

| Pin | GPIO | Mode |
|-----|------|------|
| IN1 | 5 | INPUT_PULLUP |
| IN2 | 6 | INPUT_PULLUP |
| IN3 | 7 | INPUT_PULLUP |
| OUT1 | 8 | OUTPUT |
| OUT2 | 9 | OUTPUT |
| OUT3 | 10 | OUTPUT |

---

## Quick Reference

```
GPIO Usage Summary:
  0   Reset button
  2   HUB75 CLK
  5   IN1
  6   IN2
  7   IN3
  8   OUT1
  9   OUT2
  10  OUT3
  14  HUB75 OE
  21  HUB75 C (address)
  35  HUB75 D (address)
  36  HUB75 B (address)
  37  HUB75 B2
  38  HUB75 R2
  39  HUB75 G2
  40  HUB75 B1
  41  HUB75 G1
  42  HUB75 R1
  45  HUB75 A (address)
  47  HUB75 LAT
  48  NeoPixel status LED
```

---

## Panel Wiring (128×32)

For the two panels to show different content (left vs right), they must be **daisy-chained**:

```
ESP32 HUB75 output  →  Panel 1 INPUT
Panel 1 OUTPUT     →  Panel 2 INPUT
```

Only connect the ESP32 to Panel 1. If both panels are wired in parallel to the ESP32, they will show the same image.

---

## Features

- **WiFi**: AP mode (captive portal) for setup, STA mode when configured
- **Web**: Config at `http://<IP>/`, WebSocket help at `http://<IP>/msg`
- **WebSocket**: Port 81; matrix stays blank until a JSON update with `status` / `nameplatevol` / `nameplate` (non-JSON shows four blue squares)
- **Reset**: Hold Boot button 5s to clear WiFi and return to AP mode
- **Status LED**: Green=connected, Blue=AP mode, Red blink=holding reset

## Build & Upload

```bash
pio run -t upload
```
