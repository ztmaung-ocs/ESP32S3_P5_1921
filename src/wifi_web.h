/**
 * WiFi, captive portal, web server, WebSocket
 */

#pragma once

#include <Arduino.h>

void loadConfig();
void saveConfig();
void clearConfig();
bool connectWiFi(uint32_t timeout_ms = 15000);
void startAPCaptive();
void startSTA();
void wifiWebLoop();

extern String wifi_ssid;
extern String wifi_pass;
extern String displayMessage;
extern bool captivePortalActive;
