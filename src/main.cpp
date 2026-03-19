/**
 * P5-1921 128x32 (2x 64x32) LED Matrix
 * AP + Captive Portal + WiFi STA + WebSocket Text Display
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebSocketsServer.h>

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

// -------------------- Reset button & Status LED --------------------
#define RESET_BUTTON_PIN  0   // Boot button on ESP32-S3 DevKit
#define RGB_LED_PIN      48  // Built-in NeoPixel (DevKit v1.0). C_PIN moved to 21
#define RESET_HOLD_MS    5000
#define BLINK_MS         200

#if RGB_LED_PIN >= 0
#include <Adafruit_NeoPixel.h>
#define NEO_LED_ORDER NEO_GRB  // NEO_GRB = if colors wrong; MatrixPortal S3 usually GRB
Adafruit_NeoPixel rgb(1, RGB_LED_PIN, NEO_LED_ORDER + NEO_KHZ800);
#endif

bool g_resetButtonHeld = false;
bool g_resetButtonPressed = false;  // true while holding (blink during countdown)

// -------------------- Matrix config --------------------
// 2 panels side by side: 64x32 x 2 = 128x32
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
#define AP_SSID "OCS-IoT-WiFi"
#define AP_PASS "12345678"
#define DNS_PORT 53
#define WS_PORT 81

// -------------------- 3 Inputs & 3 Outputs --------------------
#define PIN_IN1  5   // INPUT_PULLUP
#define PIN_IN2  6
#define PIN_IN3  7
#define PIN_OUT1 8
#define PIN_OUT2 9
#define PIN_OUT3 10

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(WS_PORT);
bool captivePortalActive = false;

String wifi_ssid = "";
String wifi_pass = "";

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel *virtualDisp = nullptr;
Adafruit_GFX *disp = nullptr;

String displayMessage = "";

void drawMatrixMessage(const String &msg);  // forward decl
void clearConfig();

// -------------------- LED helpers (optional, when RGB_LED_PIN >= 0) --------------------
#if RGB_LED_PIN >= 0
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
void ledOff() {}
void ledRed() {}
void ledGreen() {}
void ledBlue() {}
void ledYellow() {}
void updateStatusLed() {}
#endif

// -------------------- Reset button --------------------
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

// -------------------- Helpers --------------------
void clearConfig() {
  prefs.begin("config", false);
  prefs.clear();
  prefs.end();
  wifi_ssid = "";
  wifi_pass = "";
}

void loadConfig() {
  prefs.begin("config", true);
  wifi_ssid = prefs.getString("ssid", "");
  wifi_pass = prefs.getString("pass", "");
  prefs.end();
}

void saveConfig() {
  prefs.begin("config", false);
  prefs.putString("ssid", wifi_ssid);
  prefs.putString("pass", wifi_pass);
  prefs.end();
}

String buildConfigPage(const String &ssid) {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>P5 Matrix Config</title>
  <style>
    body{font-family:sans-serif;margin:20px;background:#1a1a1a;color:#eee;}
    h1{color:#0af;}
    input{display:block;width:100%;padding:10px;margin:8px 0;box-sizing:border-box;border:1px solid #444;border-radius:6px;background:#222;color:#fff;}
    button{width:100%;padding:12px;margin:8px 0;background:#0af;border:none;border-radius:6px;color:#fff;font-size:16px;cursor:pointer;}
    button.secondary{background:#444;}
    label{display:block;margin-top:12px;}
  </style>
</head>
<body>
  <h1>P5 Matrix WiFi</h1>
  <form method="post" action="/save">
    <label>SSID</label>
    <input type="text" name="ssid" value=")rawliteral" + ssid + R"rawliteral(" placeholder="Network name">
    <label>Password</label>
    <input type="password" name="pass" placeholder="(leave blank to keep)">
    <button type="submit">Save & Connect</button>
  </form>
  <form method="post" action="/clear">
    <button type="submit" class="secondary">Clear Config</button>
  </form>
</body>
</html>
)rawliteral";
}

// -------------------- Web handlers --------------------
void handleRoot() {
  server.send(200, "text/html", buildConfigPage(wifi_ssid));
}

void handleSave() {
  if (server.hasArg("ssid")) {
    wifi_ssid = server.arg("ssid");
    if (server.hasArg("pass") && server.arg("pass").length() > 0) {
      wifi_pass = server.arg("pass");
    }
    saveConfig();
    server.send(200, "text/html", "<html><body><h1>Saved. Connecting...</h1><p>Device will restart.</p></body></html>");
    delay(500);
    ESP.restart();
  } else {
    server.send(400, "text/plain", "Bad request");
  }
}

void handleClear() {
  clearConfig();
  server.send(200, "text/html", "<html><body><h1>Config cleared.</h1><p>Restarting...</p></body></html>");
  delay(500);
  ESP.restart();
}

void handleMsg() {
  // Served by ESP32 - same origin, no CSP block. Uses location.hostname so it works on any IP.
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>P5 Matrix - Send Message</title>
  <style>
    body{font-family:sans-serif;margin:20px;background:#1a1a1a;color:#eee;}
    h1{color:#0af;}
    input{display:block;width:100%;padding:12px;margin:8px 0;box-sizing:border-box;border:1px solid #444;border-radius:6px;background:#222;color:#fff;font-size:16px;}
    button{padding:12px 24px;background:#0af;border:none;border-radius:6px;color:#fff;font-size:16px;cursor:pointer;}
    button:disabled{opacity:0.5;cursor:not-allowed;}
    #status{margin-top:12px;font-size:14px;color:#8f8;}
    a{color:#0af;}
  </style>
</head>
<body>
  <h1>Send to Matrix</h1>
  <input type="text" id="msg" placeholder="Type message..." maxlength="255" autofocus>
  <button id="send">Send</button>
  <p id="status">Connecting...</p>
  <p><a href="/">WiFi config</a></p>
  <script>
    const ws = new WebSocket('ws://' + location.hostname + ':81');
    const inp = document.getElementById('msg');
    const btn = document.getElementById('send');
    const st = document.getElementById('status');
    ws.onopen = function(){ st.textContent = 'Connected'; btn.disabled = false; };
    ws.onclose = function(){ st.textContent = 'Disconnected'; btn.disabled = true; };
    ws.onerror = function(){ st.textContent = 'Error'; };
    function send(){ var t = inp.value.trim(); if(t && ws.readyState===1){ ws.send(t); st.textContent = 'Sent: ' + t; inp.value=''; }}
    btn.onclick = send;
    inp.onkeydown = function(e){ if(e.key==='Enter') send(); };
  </script>
</body>
</html>
)rawliteral");
}

void handleNotFound() {
  if (captivePortalActive) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

// -------------------- WebSocket --------------------
void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_TEXT:
      if (len > 0 && len < 256) {
        displayMessage = "";
        for (size_t i = 0; i < len; i++) displayMessage += (char)payload[i];
        Serial.printf("WS message: %s\n", displayMessage.c_str());
        if (disp) drawMatrixMessage(displayMessage);
      }
      break;
    default:
      break;
  }
}

// -------------------- Matrix init --------------------
void initMatrix() {
  HUB75_I2S_CFG::i2s_pins pins = {
    R1_PIN, G1_PIN, B1_PIN,
    R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };

#if USE_8_SCAN
  // 1/8 scan: (width per panel)*2 for scan format, height/2, chain. Try 128 or 256 if one fails.
  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2, PANEL_CHAIN, pins);
#else
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
#endif

  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false;
  mxconfig.latch_blanking = 4;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("Matrix init FAILED!");
    return;
  }

  dma_display->setBrightness8(200);
  dma_display->clearScreen();

#if USE_8_SCAN
  virtualDisp = new VirtualMatrixPanel(*dma_display, 1, PANEL_CHAIN, PANEL_RES_X, PANEL_RES_Y, CHAIN_NONE);
  virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
  disp = virtualDisp;
#else
  disp = dma_display;
#endif

  // Boot test: red left, green right. If both panels work you see both colors.
  disp->fillRect(0, 0, 64, 32, dma_display->color565(120, 0, 0));
  disp->fillRect(64, 0, 64, 32, dma_display->color565(0, 120, 0));
  delay(2000);
  disp->fillScreen(dma_display->color565(0, 0, 0));
}

void drawMatrixMessage(const String &msg) {
  if (!disp) return;
  uint16_t BLACK = dma_display->color565(0, 0, 0);
  uint16_t GREEN = dma_display->color565(0, 255, 0);

  disp->fillScreen(BLACK);
  disp->setTextColor(GREEN);
  disp->setTextSize(1);
  disp->setTextWrap(true);   // Long text wraps instead of going off-screen
  disp->setCursor(2, 2);
  disp->print(msg);
}

// -------------------- WiFi --------------------
bool connectWiFi(uint32_t timeout_ms = 15000) {
  if (wifi_ssid.length() == 0) return false;
  Serial.println("Connecting WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > timeout_ms) {
      Serial.println("WiFi timeout");
      return false;
    }
    delay(200);
  }
  Serial.println("WiFi OK: " + WiFi.localIP().toString());
  return true;
}

void startAPCaptive() {
  WiFi.mode(WIFI_AP);
  IPAddress apIP(192, 168, 4, 1);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, AP_PASS);
  dnsServer.start(DNS_PORT, "*", apIP);
  captivePortalActive = true;

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println("Config AP started (captive portal)");
  Serial.println("Connect WiFi: " AP_SSID);
  if (disp) drawMatrixMessage("Setup " + apIP.toString());
}

void startSTA() {
  server.on("/", handleRoot);
  server.on("/msg", handleMsg);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);

  if (disp) drawMatrixMessage("IP: " + WiFi.localIP().toString());
}

// -------------------- Main --------------------
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("P5-1921-64x32-8S start");

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
#if RGB_LED_PIN >= 0
  rgb.begin();
  rgb.setBrightness(80);
  rgb.clear();
  rgb.show();
  delay(50);
  // Boot test: cycle R,G,B to verify pin/color order
  ledRed();
  delay(300);
  ledGreen();
  delay(300);
  ledBlue();
  delay(300);
  rgb.clear();
  rgb.show();
#endif
  loadConfig();
  initMatrix();

  if (connectWiFi()) {
    startSTA();
    displayMessage = "Ready";
  } else {
    startAPCaptive();
    displayMessage = "";
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

  if (captivePortalActive) dnsServer.processNextRequest();
  server.handleClient();
  if (!captivePortalActive) webSocket.loop();

  delay(10);
}
