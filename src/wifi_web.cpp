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

Preferences prefs;
WebServer server(80);
DNSServer dnsServer;
WebSocketsServer webSocket(WS_PORT);

String wifi_ssid = "";
String wifi_pass = "";
String displayMessage = "";
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

static void handleMsg() {
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
    case WStype_TEXT:
      if (len > 0 && len < 256) {
        displayMessage = "";
        for (size_t i = 0; i < len; i++) displayMessage += (char)payload[i];
        Serial.printf("WS message: %s\n", displayMessage.c_str());
        drawMatrixMessage(displayMessage);
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
  drawMatrixMessage("Setup " + apIP.toString());
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

  drawMatrixMessage("IP: " + WiFi.localIP().toString());
}

void wifiWebLoop() {
  if (captivePortalActive) {
    dnsServer.processNextRequest();
  } else {
    // WiFi reconnect: when STA mode, has config, but disconnected
    if (wifi_ssid.length() > 0 && WiFi.status() != WL_CONNECTED) {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 30000) {  // Retry every 30 seconds
        lastReconnect = millis();
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.disconnect();
        WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
        drawMatrixMessage("Reconnecting...");
      }
    } else if (WiFi.status() == WL_CONNECTED) {
      static bool wasConnected = false;
      if (!wasConnected) {
        wasConnected = true;
        drawMatrixMessage("IP: " + WiFi.localIP().toString());
      }
    } else {
      static bool wasConnected = true;
      wasConnected = false;
    }
  }

  server.handleClient();
  if (!captivePortalActive) webSocket.loop();
}
