/**
 * Matrix display - HUB75 LED panel init and drawing
 */

#include "config.h"
#include "matrix.h"
#include <Arduino.h>

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
  disp->setTextColor(GREEN);
  disp->setTextSize(1);
  disp->setTextWrap(true);
  disp->setCursor(2, 2);
  disp->print(msg);
}
