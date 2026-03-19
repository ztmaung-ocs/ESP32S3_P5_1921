/**
 * Status LED and Reset button handling
 */

#include "status_led.h"
#include "config.h"
#include "wifi_web.h"
#include <WiFi.h>

#if RGB_LED_PIN >= 0
#include <Adafruit_NeoPixel.h>
#define NEO_LED_ORDER NEO_GRB
Adafruit_NeoPixel rgb(1, RGB_LED_PIN, NEO_LED_ORDER + NEO_KHZ800);
#endif

bool g_resetButtonHeld = false;
bool g_resetButtonPressed = false;

#if RGB_LED_PIN >= 0
void initStatusLed() {
  rgb.begin();
  rgb.setBrightness(80);
  rgb.clear();
  rgb.show();
  delay(50);
  ledRed();
  delay(300);
  ledGreen();
  delay(300);
  ledBlue();
  delay(300);
  rgb.clear();
  rgb.show();
}

void ledOff()    { rgb.setPixelColor(0, rgb.Color(0,0,0)); rgb.show(); }
void ledRed()    { rgb.setPixelColor(0, rgb.Color(255,0,0)); rgb.show(); }
void ledGreen()  { rgb.setPixelColor(0, rgb.Color(0,255,0)); rgb.show(); }
void ledBlue()   { rgb.setPixelColor(0, rgb.Color(0,0,255)); rgb.show(); }
void ledYellow() { rgb.setPixelColor(0, rgb.Color(255,255,0)); rgb.show(); }
void updateStatusLed() {
  if (captivePortalActive) { ledBlue(); return; }
  if (WiFi.status() == WL_CONNECTED) { ledGreen(); return; }
  ledRed();
}
#else
void initStatusLed() {}
void ledOff() {}
void ledRed() {}
void ledGreen() {}
void ledBlue() {}
void ledYellow() {}
void updateStatusLed() {}
#endif

void handleResetButtonRuntime() {
  static uint32_t pressStart = 0;
  static bool wasPressed = false;
  bool pressed = (digitalRead(RESET_BUTTON_PIN) == LOW);
  if (pressed) {
    if (!wasPressed) {
      pressStart = millis();
      wasPressed = true;
    }
    g_resetButtonHeld = (millis() - pressStart >= RESET_HOLD_MS);
    if (g_resetButtonHeld) {
      Serial.println("Reset: clearing WiFi config, restarting...");
      clearConfig();
      delay(100);
      ESP.restart();
    }
  } else {
    wasPressed = false;
    g_resetButtonHeld = false;
  }
  g_resetButtonPressed = pressed;
}
