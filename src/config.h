/**
 * P5-1921 128x32 (2x 64x32) - Configuration
 */

#ifndef CONFIG_H
#define CONFIG_H

// -------------------- Reset button & Status LED --------------------
#define RESET_BUTTON_PIN  0   // Boot button on ESP32-S3 DevKit
#define RGB_LED_PIN      48  // Built-in NeoPixel (DevKit v1.0)
#define RESET_HOLD_MS    5000
#define BLINK_MS         200

// -------------------- Matrix (2 panels side by side = 128x32) --------------------
#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 2   // horizontal daisy chain
#define USE_8_SCAN 1

#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37
#define A_PIN 45
#define B_PIN 36
#define C_PIN 21   // was 48; freed for built-in NeoPixel
#define D_PIN 35
#define E_PIN -1
#define LAT_PIN 47
#define OE_PIN 14
#define CLK_PIN 2

// -------------------- AP / WiFi --------------------
// AP / hostname: prefix + last N hex digits of station MAC (right-hand side of 12-hex; use 4 or 5)
#define DEVICE_MAC_SUFFIX_HEX 4
#define AP_SSID_PREFIX "OCS-IoT"
#define DEVICE_HOSTNAME_PREFIX "ocs-iot"
#define AP_PASS "12345678"
#define DNS_PORT 53
#define WS_PORT 81

// JSON fields sent on WebSocket connect and on "status" / "getStatus" request
#define DEVICE_WS_STATUS       "entrance"
#define DEVICE_NAMEPLATE_VOL   "47W"
#define DEVICE_NAMEPLATE       "987654"

// -------------------- WiFi stability (EMI from HUB75) --------------------
// Set to 1 to reduce matrix speed/brightness for better WiFi ping stability
#define WIFI_FRIENDLY_MODE 1

// -------------------- Digital I/O --------------------
#define PIN_IN1  5   // INPUT_PULLUP
#define PIN_IN2  6
#define PIN_IN3  7
#define PIN_OUT1 8
#define PIN_OUT2 9
#define PIN_OUT3 10

#endif // CONFIG_H
