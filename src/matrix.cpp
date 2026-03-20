/**
 * Matrix display - HUB75 LED panel init and drawing
 */

#include "config.h"
#include "matrix.h"
#include "plate_font7x9.h"
#include <Arduino.h>
#include <cstring>
#include <cstdio>

static String s_boardStatus(DEVICE_WS_STATUS);
static String s_boardVol(DEVICE_NAMEPLATE_VOL);
static String s_boardPlate(DEVICE_NAMEPLATE);

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

  // Boot test: red left, green right
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

  const int margin = 2;
  const int stepX = kPlateCharW + kPlateCharGap;
  const int stepY = kPlateCharH + kPlateLineGap;
  int x = margin;
  int y = margin;
  const int maxX = disp->width() - margin;
  const int maxY = disp->height() - margin;

  for (size_t i = 0; i < msg.length(); i++) {
    char c = msg[i];
    if (c == '\n' || c == '\r') {
      x = margin;
      y += stepY;
      if (y + kPlateCharH > maxY)
        break;
      if (c == '\r' && i + 1 < msg.length() && msg[i + 1] == '\n')
        i++;
      continue;
    }

    if (x + kPlateCharW > maxX) {
      x = margin;
      y += stepY;
      if (y + kPlateCharH > maxY)
        break;
    }

    drawPlate7x9Char(disp, x, y, c, GREEN);
    x += stepX;
  }
}

void drawNameplateBoard(const String &ip) {
  if (!disp) return;
  (void)ip;

  const int h = disp->height();
  const int halfW = PANEL_RES_X;

  uint16_t BLACK = dma_display->color565(0, 0, 0);
  uint16_t GREEN = dma_display->color565(0, 255, 0);
  uint16_t STATUS_COLOR = dma_display->color565(255, 200, 0);

  disp->fillScreen(BLACK);

  // 64×32 | 64×32 — left status, right nameplate (no tinted BG, no divider lines)
  drawPlateLineInRect(disp, 0, 0, halfW, h, s_boardStatus.c_str(), STATUS_COLOR);

  const int nh = h / 2;
  drawPlateLineInRect(disp, halfW, 0, halfW, nh, s_boardVol.c_str(), GREEN);
  drawPlateLineInRect(disp, halfW, nh, halfW, h - nh, s_boardPlate.c_str(), GREEN);
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
