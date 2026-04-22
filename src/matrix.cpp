/**
 * Matrix display - HUB75 LED panel init and drawing
 */

#include "config.h"
#include "matrix.h"
#include "plate_font7x9.h"
#include "wifi_web.h"
#include <Arduino.h>
#include <WiFi.h>
#include <cstring>
#include <cstdio>

static String s_boardStatus(DEVICE_WS_STATUS);
static String s_boardVol(DEVICE_NAMEPLATE_VOL);
static String s_boardPlate(DEVICE_NAMEPLATE);
static uint32_t s_autoClearAtMs = 0;
static uint32_t s_ipShowUntilMs = 0;
/** Center 2×2 blue blocks from drawNonJsonWsIndicator — idle corner blink must not erase it. */
static bool s_nonJsonCenterIndicator = false;
/** Bumped on any full-panel draw outside matrixPollIdleWifiIndicator so corner blink resyncs. */
static uint32_t s_panelContentStamp = 0;

static void bumpPanelContentStamp() { ++s_panelContentStamp; }

namespace {

constexpr int kPlateCharW = 7;
constexpr int kPlateCharH = 9;
constexpr int kPlateCharGap = 1;
constexpr int kPlateLineGap = 1;

void drawPlate7x9Char(Adafruit_GFX *d, int x, int y, char ch, uint16_t color) {
  if (ch >= 'a' && ch <= 'z')
    ch = static_cast<char>(ch - ('a' - 'A'));
  const uint8_t *rows = getSquarePlateChar7x9(ch);
  for (int row = 0; row < kPlateCharH; row++) {
    uint8_t bits = rows[row];
    for (int col = 0; col < kPlateCharW; col++) {
      if ((bits >> (6 - col)) & 1)
        d->drawPixel(x + col, y + row, color);
    }
  }
}

static int plateTextPixelWidth(int nChars) {
  if (nChars <= 0) return 0;
  return nChars * kPlateCharW + (nChars - 1) * kPlateCharGap;
}

/** One line of plate font, horizontally centered (clips if wider than panel). */
static void drawPlateLineCentered(Adafruit_GFX *d, int y, const char *str, uint16_t color) {
  const int margin = 2;
  int n = static_cast<int>(strlen(str));
  int tw = plateTextPixelWidth(n);
  int x = (d->width() - tw) / 2;
  if (x < margin)
    x = margin;
  for (int i = 0; i < n; i++) {
    if (x + kPlateCharW > d->width() - margin)
      break;
    drawPlate7x9Char(d, x, y, str[i], color);
    x += kPlateCharW + kPlateCharGap;
  }
}

/** One plate line centered inside [rx,ry] .. [rx+rw, ry+rh]. */
static void drawPlateLineInRect(Adafruit_GFX *d, int rx, int ry, int rw, int rh, const char *str,
                                uint16_t color) {
  const int margin = 1;
  int n = static_cast<int>(strlen(str));
  int tw = plateTextPixelWidth(n);
  int x = rx + (rw - tw) / 2;
  if (x < rx + margin)
    x = rx + margin;
  int y = ry + (rh - kPlateCharH) / 2;
  if (y < ry)
    y = ry;
  for (int i = 0; i < n; i++) {
    if (x + kPlateCharW > rx + rw - margin)
      break;
    drawPlate7x9Char(d, x, y, str[i], color);
    x += kPlateCharW + kPlateCharGap;
  }
}

/** IP split across two lines when needed, inside [rx,ry,rw,rh]. */
static void drawIpLinesInRect(Adafruit_GFX *d, int rx, int ry, int rw, int rh, const char *ipStr,
                              uint16_t fg) {
  if (!ipStr)
    return;
  int len = static_cast<int>(strlen(ipStr));
  if (len <= 7) {
    drawPlateLineInRect(d, rx, ry, rw, rh, ipStr, fg);
    return;
  }
  char line1[20];
  char line2[20];
  int mid = len / 2;
  int dotIdx = -1;
  int bestDist = len;
  for (int i = 0; i < len; i++) {
    if (ipStr[i] != '.')
      continue;
    int dist = i - mid;
    if (dist < 0)
      dist = -dist;
    if (dist < bestDist) {
      bestDist = dist;
      dotIdx = i;
    }
  }
  if (dotIdx < 0) {
    int n1 = len < 7 ? len : 7;
    memcpy(line1, ipStr, n1);
    line1[n1] = '\0';
    int n2 = len - n1;
    if (n2 >= (int)sizeof(line2))
      n2 = (int)sizeof(line2) - 1;
    memcpy(line2, ipStr + n1, n2);
    line2[n2] = '\0';
  } else {
    int n1 = dotIdx;
    if (n1 >= (int)sizeof(line1))
      n1 = (int)sizeof(line1) - 1;
    memcpy(line1, ipStr, n1);
    line1[n1] = '\0';
    int n2 = len - dotIdx - 1;
    if (n2 >= (int)sizeof(line2))
      n2 = (int)sizeof(line2) - 1;
    memcpy(line2, ipStr + dotIdx + 1, n2);
    line2[n2] = '\0';
  }

  const int nh = rh / 2;
  drawPlateLineInRect(d, rx, ry, rw, nh, line1, fg);
  drawPlateLineInRect(d, rx, ry + nh, rw, rh - nh, line2, fg);
}

/** Left = "AP" or "WIFI"; right nameplate = IP (AP mode vs STA from caller). */
static void drawModeAndIpOnBoards(bool apMode, const char *ipStr) {
  if (!disp || !dma_display || !ipStr)
    return;
  uint16_t bg = dma_display->color565(0, 0, 0);
  uint16_t fg = dma_display->color565(0, 200, 255);
  const int h = disp->height();
  const int halfW = PANEL_RES_X;

  disp->fillRect(0, 0, halfW, h, bg);
  const char *modeLabel = apMode ? "AP" : "WIFI";
  drawPlateLineInRect(disp, 0, 0, halfW, h, modeLabel, fg);

  disp->fillRect(halfW, 0, halfW, h, bg);
  drawIpLinesInRect(disp, halfW, 0, halfW, h, ipStr, fg);
}

} // namespace

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel *virtualDisp = nullptr;
Adafruit_GFX *disp = nullptr;

void initMatrix() {
  HUB75_I2S_CFG::i2s_pins pins = {
    R1_PIN, G1_PIN, B1_PIN,
    R2_PIN, G2_PIN, B2_PIN,
    A_PIN, B_PIN, C_PIN, D_PIN, E_PIN,
    LAT_PIN, OE_PIN, CLK_PIN
  };

#if USE_8_SCAN
  HUB75_I2S_CFG mxconfig(PANEL_RES_X * 2, PANEL_RES_Y / 2, PANEL_CHAIN, pins);
#else
  HUB75_I2S_CFG mxconfig(PANEL_RES_X, PANEL_RES_Y, PANEL_CHAIN, pins);
#endif

  // HZ_5M not in library - use HZ_10M. WIFI_FRIENDLY_MODE reduces brightness only.
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;
  mxconfig.clkphase = false;
  mxconfig.latch_blanking = 4;

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  if (!dma_display->begin()) {
    Serial.println("Matrix init FAILED!");
    return;
  }

#if defined(WIFI_FRIENDLY_MODE) && WIFI_FRIENDLY_MODE
  dma_display->setBrightness8(120);   // Lower = less EMI, better WiFi
#else
  dma_display->setBrightness8(200);
#endif
  dma_display->clearScreen();

#if USE_8_SCAN
  virtualDisp = new VirtualMatrixPanel(*dma_display, 1, PANEL_CHAIN, PANEL_RES_X, PANEL_RES_Y, CHAIN_NONE);
  virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
  disp = virtualDisp;
#else
  disp = dma_display;
#endif

  disp->fillScreen(dma_display->color565(0, 0, 0));
}

void matrixDrawBootSplash(bool apMode) {
  if (!disp || !dma_display)
    return;
  bumpPanelContentStamp();
  uint16_t c = apMode ? dma_display->color565(0, 0, 255) : dma_display->color565(0, 255, 0);
  disp->fillScreen(c);
}

void drawNonJsonWsIndicator() {
  if (!disp || !dma_display)
    return;
  bumpPanelContentStamp();
  s_nonJsonCenterIndicator = true;
  uint16_t black = dma_display->color565(0, 0, 0);
  uint16_t blue = dma_display->color565(0, 140, 255);
  disp->fillScreen(black);
  const int dot = 4;
  const int gap = 6;
  const int blockW = dot * 2 + gap;
  const int blockH = dot * 2 + gap;
  int x0 = (disp->width() - blockW) / 2;
  int y0 = (disp->height() - blockH) / 2;
  for (int row = 0; row < 2; row++) {
    for (int col = 0; col < 2; col++) {
      int x = x0 + col * (dot + gap);
      int y = y0 + row * (dot + gap);
      disp->fillRect(x, y, dot, dot, blue);
    }
  }
}

void drawNameplateBoard(const String &ip) {
  if (!disp) return;
  (void)ip;
  bumpPanelContentStamp();

  uint16_t BLACK = dma_display->color565(0, 0, 0);

  String key = s_boardStatus;
  key.trim();
  key.toLowerCase();
  if (key == "clear") {
    s_nonJsonCenterIndicator = false;
    disp->fillScreen(BLACK);
    return;
  }

  s_nonJsonCenterIndicator = false;
  const int h = disp->height();
  const int halfW = PANEL_RES_X;

  uint16_t GREEN = dma_display->color565(0, 255, 0);
  uint16_t RED = dma_display->color565(255, 0, 0);
  uint16_t EXIT_COLOR = dma_display->color565(255, 200, 0);
  uint16_t STATUS_COLOR = EXIT_COLOR;

  const char *leftText = s_boardStatus.c_str();
  uint16_t statusLineColor = STATUS_COLOR;
  uint16_t plateColor = GREEN;

  if (key == "entrance" || key == "allowed") {
    leftText = "WELCOME";
    statusLineColor = GREEN;
    plateColor = GREEN;
  } else if (key == "exit") {
    leftText = "BYE BYE";
    statusLineColor = EXIT_COLOR;
    plateColor = EXIT_COLOR;
  } else if (key == "not-allowed" || key == "notallowed" || key == "not_allowed") {
    leftText = "CHECK";
    statusLineColor = RED;
    plateColor = RED;
  }

  disp->fillScreen(BLACK);

  // 64×32 | 64×32 — left status, right nameplate (no tinted BG, no divider lines)
  drawPlateLineInRect(disp, 0, 0, halfW, h, leftText, statusLineColor);

  const int nh = h / 2;
  drawPlateLineInRect(disp, halfW, 0, halfW, nh, s_boardVol.c_str(), plateColor);
  drawPlateLineInRect(disp, halfW, nh, halfW, h - nh, s_boardPlate.c_str(), plateColor);
}

void matrixApplyBoardFields(bool setStatus, const char *status, bool setVol, const char *nameplatevol,
                            bool setPlate, const char *nameplate) {
  auto apply = [](String &dst, bool set, const char *v) {
    if (!set || !v)
      return;
    String s(v);
    s.trim();
    if (s.length() == 0)
      return;
    if (s.length() > 32)
      s = s.substring(0, 32);
    dst = s;
  };
  apply(s_boardStatus, setStatus, status);
  apply(s_boardVol, setVol, nameplatevol);
  apply(s_boardPlate, setPlate, nameplate);
}

bool matrixBuildStatusJson(char *buf, size_t bufLen) {
  if (!buf || bufLen == 0)
    return false;
  int n = snprintf(buf, bufLen, "{\"status\":\"%s\",\"nameplatevol\":\"%s\",\"nameplate\":\"%s\"}",
                   s_boardStatus.c_str(), s_boardVol.c_str(), s_boardPlate.c_str());
  return n > 0 && (size_t)n < bufLen;
}

static bool boardStatusIsClear() {
  String k = s_boardStatus;
  k.trim();
  k.toLowerCase();
  return k == "clear";
}

void matrixConfigureAutoClear(bool hasDisplaytimeKey, uint32_t seconds) {
  if (!hasDisplaytimeKey) {
    s_autoClearAtMs = 0;
    return;
  }
  if (seconds == 0) {
    s_autoClearAtMs = 0;
    return;
  }
  if (boardStatusIsClear()) {
    s_autoClearAtMs = 0;
    return;
  }
  constexpr uint32_t kMaxSec = 86400u * 7u;
  if (seconds > kMaxSec)
    seconds = kMaxSec;
  s_autoClearAtMs = millis() + seconds * 1000UL;
}

void matrixShowIpTemporary(const char *ipStr, uint32_t seconds, bool apMode) {
  if (!ipStr || !ipStr[0])
    return;
  bumpPanelContentStamp();
  s_nonJsonCenterIndicator = false;
  uint32_t dur = seconds ? seconds * 1000UL : 10000UL;
  s_ipShowUntilMs = millis() + dur;
  drawModeAndIpOnBoards(apMode, ipStr);
}

void matrixClearScreen() {
  bumpPanelContentStamp();
  s_nonJsonCenterIndicator = false;
  s_ipShowUntilMs = 0;
  s_autoClearAtMs = 0;
  s_boardStatus = "clear";
  if (!disp || !dma_display)
    return;
  disp->fillScreen(dma_display->color565(0, 0, 0));
}

void matrixPollIpDisplay() {
  if (s_ipShowUntilMs == 0)
    return;
  if ((int32_t)(millis() - s_ipShowUntilMs) < 0)
    return;
  matrixClearScreen();
}

void matrixPollAutoClear() {
  if (s_ipShowUntilMs != 0 && (int32_t)(millis() - s_ipShowUntilMs) < 0)
    return;
  if (s_autoClearAtMs == 0)
    return;
  if ((int32_t)(millis() - s_autoClearAtMs) < 0)
    return;
  s_autoClearAtMs = 0;
  s_boardStatus = "clear";
  s_nonJsonCenterIndicator = false;
  bumpPanelContentStamp();
  if (!disp || !dma_display)
    return;
  disp->fillScreen(dma_display->color565(0, 0, 0));
}

void matrixPollIdleWifiIndicator() {
  if (!disp || !dma_display)
    return;

  const bool ipShowing =
      s_ipShowUntilMs != 0 && (int32_t)(millis() - s_ipShowUntilMs) < 0;
  const bool idle = boardStatusIsClear() && !ipShowing && !s_nonJsonCenterIndicator;
  const bool showCorners = idle;

  constexpr uint32_t kIdleDotsOnMs = 1000;
  constexpr uint32_t kIdleDotsOffMs = 6000;
  static bool phaseHigh = true;
  static uint32_t phaseStartMs = 0;
  static bool wasShowing = false;
  static uint32_t lastContentStamp = 0;
  static uint16_t lastPaintedIdleDot565 = 0xFFFF;

  if (s_panelContentStamp != lastContentStamp) {
    lastContentStamp = s_panelContentStamp;
    wasShowing = false;
    phaseStartMs = 0;
    lastPaintedIdleDot565 = 0xFFFF;
  }

  uint16_t black = dma_display->color565(0, 0, 0);
  // Match status_led NeoPixel: AP=blue, STA connected=green, else red
  auto idleStatusColor565 = []() -> uint16_t {
    if (captivePortalActive)
      return dma_display->color565(0, 0, 255);
    if (WiFi.status() == WL_CONNECTED)
      return dma_display->color565(0, 255, 0);
    return dma_display->color565(255, 0, 0);
  };
  auto drawIdleWifiDots = [&](uint16_t fg) {
    const int halfW = PANEL_RES_X; // left = status, right = nameplate (same as drawNameplateBoard)
    const int ymax = disp->height() - 1;
    const int xmax = disp->width() - 1;
    disp->drawPixel(0, 0, fg);              // status board top-left
    disp->drawPixel(halfW - 1, 0, fg);      // status board top-right
    disp->drawPixel(halfW, ymax, fg);       // nameplate board bottom-left
    disp->drawPixel(xmax, ymax, fg);        // nameplate board bottom-right
  };

  if (!showCorners) {
    if (wasShowing) {
      // Do not blank if nameplate / IP / non-JSON overlay took over the panel.
      if (idle)
        disp->fillScreen(black);
      wasShowing = false;
    }
    phaseStartMs = 0;
    lastPaintedIdleDot565 = 0xFFFF;
    return;
  }

  uint32_t now = millis();
  if (!wasShowing) {
    wasShowing = true;
    phaseStartMs = now;
    phaseHigh = true;
    uint16_t c = idleStatusColor565();
    disp->fillScreen(black);
    drawIdleWifiDots(c);
    lastPaintedIdleDot565 = c;
    return;
  }

  const uint32_t phaseLenMs = phaseHigh ? kIdleDotsOnMs : kIdleDotsOffMs;
  if ((int32_t)(now - phaseStartMs) >= (int32_t)phaseLenMs) {
    phaseStartMs = now;
    phaseHigh = !phaseHigh;
    if (phaseHigh) {
      uint16_t c = idleStatusColor565();
      disp->fillScreen(black);
      drawIdleWifiDots(c);
      lastPaintedIdleDot565 = c;
    } else {
      disp->fillScreen(black);
      lastPaintedIdleDot565 = 0xFFFF;
    }
  }

  if (phaseHigh) {
    uint16_t c = idleStatusColor565();
    if (c != lastPaintedIdleDot565) {
      lastPaintedIdleDot565 = c;
      disp->fillScreen(black);
      drawIdleWifiDots(c);
    }
  }
}
