/**
 * WiFi, captive portal, web server, WebSocket
 */

#include "config.h"
#include "matrix.h"
#include "wifi_web.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <cstring>

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(WS_PORT);

String wifi_ssid = "";
String wifi_pass = "";
bool captivePortalActive = false;

static String buildConfigPage(const String &ssid) {
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

static void handleRoot() {
  server.send(200, "text/html", buildConfigPage(wifi_ssid));
}

static void handleSave() {
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

static void handleClear() {
  clearConfig();
  server.send(200, "text/html", "<html><body><h1>Config cleared.</h1><p>Restarting...</p></body></html>");
  delay(500);
  ESP.restart();
}

static void handleWsHelp() {
  server.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>P5 Matrix — WebSocket</title>
  <style>
    body{font-family:sans-serif;margin:20px;background:#1a1a1a;color:#eee;max-width:42rem;}
    h1{color:#0af;}
    pre{background:#222;padding:12px;border-radius:6px;overflow-x:auto;font-size:13px;}
    a{color:#0af;}
  </style>
</head>
<body>
  <h1>WebSocket (port 81)</h1>
  <p>Updates use JSON only. Example nameplate payload:</p>
  <pre>{"status":"entrance","nameplatevol":"47W","nameplate":"98765","displaytime":10}</pre>
  <p>Optional <code>displaytime</code> is seconds until auto blank (omit to leave on). <code>displaytime</code> 0 cancels a pending timer.</p>
  <p>Blank both panels:</p>
  <pre>{"status":"clear"}</pre>
  <p><a href="/">WiFi config</a></p>
</body>
</html>
)rawliteral");
}

static void sendDeviceStatusJson(uint8_t clientNum) {
  char buf[160];
  if (matrixBuildStatusJson(buf, sizeof(buf)))
    webSocket.sendTXT(clientNum, buf);
}

static bool isStatusRequest(const uint8_t *payload, size_t len) {
  if (len == 6 && strncmp((const char *)payload, "status", 6) == 0)
    return true;
  if (len == 9 && strncmp((const char *)payload, "getStatus", 9) == 0)
    return true;
  return false;
}

/** If message starts with '{', parse as JSON. Returns true if handled (caller should not treat as plain text). */
static bool handleWebSocketJson(uint8_t clientNum, const uint8_t *payload, size_t len) {
  if (len < 2 || payload[0] != '{')
    return false;

  char tmp[384];
  if (len >= sizeof(tmp))
    return false;
  memcpy(tmp, payload, len);
  tmp[len] = '\0';

  StaticJsonDocument<384> doc;
  if (deserializeJson(doc, tmp))
    return false;

  const char *action = doc["action"].as<const char *>();
  if (!action || !action[0])
    action = doc["cmd"].as<const char *>();

  if (action && action[0]) {
    if (!strcmp(action, "status") || !strcmp(action, "getStatus")) {
      sendDeviceStatusJson(clientNum);
      return true;
    }
  }

  // Same shape as device JSON: updates split board (not one-row scroll of raw JSON)
  if (doc.containsKey("status") || doc.containsKey("nameplatevol") || doc.containsKey("nameplate") ||
      doc.containsKey("displaytime")) {
    matrixApplyBoardFields(doc.containsKey("status"), doc["status"].as<const char *>(),
                           doc.containsKey("nameplatevol"), doc["nameplatevol"].as<const char *>(),
                           doc.containsKey("nameplate"), doc["nameplate"].as<const char *>());
    drawNameplateBoard(WiFi.localIP().toString());
    uint32_t displaySec = 0;
    if (doc.containsKey("displaytime")) {
      double d = doc["displaytime"].as<double>();
      if (d > 0 && d < 1e9)
        displaySec = static_cast<uint32_t>(d);
    }
    matrixConfigureAutoClear(doc.containsKey("displaytime"), displaySec);
    sendDeviceStatusJson(clientNum);
    return true;
  }

  return false;
}

static void handleNotFound() {
  if (captivePortalActive) {
    server.sendHeader("Location", "http://" + WiFi.softAPIP().toString(), true);
    server.send(302, "text/plain", "");
  } else {
    server.send(404, "text/plain", "Not found");
  }
}

static void onWebSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t len) {
  switch (type) {
    case WStype_CONNECTED:
      sendDeviceStatusJson(num);
      break;
    case WStype_TEXT:
      if (len > 0 && len < 384) {
        if (isStatusRequest(payload, len)) {
          sendDeviceStatusJson(num);
          break;
        }
        if (handleWebSocketJson(num, payload, len))
          break;
        drawNonJsonWsIndicator();
        if (len < 256) {
          char dbg[256];
          memcpy(dbg, payload, len);
          dbg[len] = '\0';
          Serial.printf("WS not JSON / unknown: %s\n", dbg);
        }
      }
      break;
    default:
      break;
  }
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

void clearConfig() {
  prefs.begin("config", false);
  prefs.clear();
  prefs.end();
  wifi_ssid = "";
  wifi_pass = "";
}

bool connectWiFi(uint32_t timeout_ms) {
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
  WiFi.setSleep(false);   // Disable power save for more stable 24/7 operation
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
}

void startSTA() {
  server.on("/", handleRoot);
  server.on("/msg", handleWsHelp);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/clear", HTTP_POST, handleClear);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

void wifiWebLoop() {
  matrixPollIpDisplay();
  matrixPollAutoClear();

  if (captivePortalActive) {
    dnsServer.processNextRequest();
  } else if (wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 30000) {  // Retry every 30 seconds
      lastReconnect = millis();
      Serial.println("WiFi disconnected, reconnecting...");
      WiFi.disconnect();
      WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    }
  }

  server.handleClient();
  if (!captivePortalActive) webSocket.loop();
}
