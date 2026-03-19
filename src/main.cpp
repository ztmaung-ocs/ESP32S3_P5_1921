#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

#define PANEL_RES_X 64
#define PANEL_RES_Y 32
#define PANEL_CHAIN 1

#define USE_8_SCAN 1

// Your earlier wiring
#define R1_PIN 42
#define G1_PIN 41
#define B1_PIN 40
#define R2_PIN 38
#define G2_PIN 39
#define B2_PIN 37
#define A_PIN  45
#define B_PIN  36
#define C_PIN  48
#define D_PIN  35
#define E_PIN  -1   // not used for 64x32 1/8 scan
#define LAT_PIN 47
#define OE_PIN  14
#define CLK_PIN 2

MatrixPanel_I2S_DMA *dma_display = nullptr;
VirtualMatrixPanel *virtualDisp = nullptr;
Adafruit_GFX *disp = nullptr;

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("P5-1921-64x32-8S start");

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
  // mxconfig.driver = HUB75_I2S_CFG::SHIFTREG; // leave default first

  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  if (!dma_display->begin()) {
    Serial.println("Matrix init FAILED!");
    while (true) delay(1000);
  }

  dma_display->setBrightness8(128);
  dma_display->clearScreen();

#if USE_8_SCAN
  virtualDisp = new VirtualMatrixPanel(*dma_display, 1, 1, PANEL_RES_X, PANEL_RES_Y, CHAIN_NONE);
  virtualDisp->setPhysicalPanelScanRate(FOUR_SCAN_32PX_HIGH);
  disp = virtualDisp;
#else
  disp = dma_display;
#endif

  uint16_t BLACK  = dma_display->color565(0, 0, 0);
  uint16_t WHITE  = dma_display->color565(255, 255, 255);
  uint16_t RED    = dma_display->color565(255, 0, 0);
  uint16_t GREEN  = dma_display->color565(0, 255, 0);
  uint16_t BLUE   = dma_display->color565(0, 0, 255);
  uint16_t YELLOW = dma_display->color565(255, 255, 0);
  uint16_t CYAN   = dma_display->color565(0, 255, 255);
  uint16_t ORANGE = dma_display->color565(255, 128, 0);

  disp->fillScreen(BLACK);
  disp->drawRect(0, 0, PANEL_RES_X, PANEL_RES_Y, WHITE);

  disp->drawPixel(0, 0, RED);
  disp->drawPixel(PANEL_RES_X - 1, 0, GREEN);
  disp->drawPixel(0, PANEL_RES_Y - 1, BLUE);
  disp->drawPixel(PANEL_RES_X - 1, PANEL_RES_Y - 1, YELLOW);

  disp->drawLine(0, 0, PANEL_RES_X - 1, PANEL_RES_Y - 1, ORANGE);

  disp->setTextColor(CYAN);
  disp->setTextSize(1);
  disp->setCursor(2, 8);
  disp->print("P5-1921");
  disp->setCursor(2, 20);
  disp->print("64x32 8S");
}

void loop() {
  delay(100);
}