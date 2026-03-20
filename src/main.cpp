/**
 * P5-1921 128x32 (2x 64x32) LED Matrix
 * AP + Captive Portal + WiFi STA + WebSocket nameplate JSON
 */

#include "config.h"
#include "matrix.h"
#include "wifi_web.h"
#include "status_led.h"

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

  wifiWebLoop();

  delay(10);
}
