/**
 * P5-1921 128x32 (2x 64x32) LED Matrix
 * AP + Captive Portal + WiFi STA + WebSocket nameplate JSON
 */

#include "config.h"
#include "matrix.h"
#include "wifi_web.h"
#include "status_led.h"
#include <WiFi.h>

/** IN1 (active low): status = AP or WIFI, nameplate = IP; 10 s then full clear. */
static void pollIn1ShowIp() {
  static bool prevHigh = true;
  static uint32_t lastEdgeMs = 0;
  bool pressed = digitalRead(PIN_IN1) == LOW;
  uint32_t now = millis();
  if (pressed && prevHigh) {
    if (now - lastEdgeMs >= 200) {
      lastEdgeMs = now;
      const bool apMode = captivePortalActive;
      String ip;
      if (apMode)
        ip = WiFi.softAPIP().toString();
      else if (WiFi.status() == WL_CONNECTED)
        ip = WiFi.localIP().toString();
      else
        ip = "NO WIFI";
      matrixShowIpTemporary(ip.c_str(), 10, apMode);
    }
  }
  prevHigh = !pressed;
}

/** IN2 (active low): clear full panel (same as status "clear"). */
static void pollIn2ClearScreen() {
  static bool prevHigh = true;
  static uint32_t lastEdgeMs = 0;
  bool pressed = digitalRead(PIN_IN2) == LOW;
  uint32_t now = millis();
  if (pressed && prevHigh) {
    if (now - lastEdgeMs >= 200) {
      lastEdgeMs = now;
      matrixClearScreen();
    }
  }
  prevHigh = !pressed;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("P5-1921-128x32 start");

  pinMode(RESET_BUTTON_PIN, INPUT_PULLUP);
  pinMode(PIN_IN1, INPUT_PULLUP);
  pinMode(PIN_IN2, INPUT_PULLUP);
  pinMode(PIN_IN3, INPUT_PULLUP);
  pinMode(PIN_OUT1, OUTPUT);
  pinMode(PIN_OUT2, OUTPUT);
  pinMode(PIN_OUT3, OUTPUT);
  digitalWrite(PIN_OUT1, LOW);
  digitalWrite(PIN_OUT2, LOW);
  digitalWrite(PIN_OUT3, LOW);

  initStatusLed();
  loadConfig();
  initMatrix();

  if (connectWiFi()) {
    startSTA();
  } else {
    startAPCaptive();
  }
}

void loop() {
  handleResetButtonRuntime();

  if (g_resetButtonPressed) {
    static uint32_t lastBlink = 0;
    static bool blinkOn = true;
    uint32_t now = millis();
    if (now - lastBlink >= BLINK_MS) {
      lastBlink = now;
      blinkOn = !blinkOn;
    }
    if (blinkOn) ledRed();
    else ledOff();
  } else {
    updateStatusLed();
  }

  pollIn1ShowIp();
  pollIn2ClearScreen();
  wifiWebLoop();

  delay(10);
}
