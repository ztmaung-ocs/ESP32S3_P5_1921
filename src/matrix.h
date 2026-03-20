/**
 * Matrix display - HUB75 LED panel init and drawing
 */

#ifndef MATRIX_H
#define MATRIX_H

#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

extern MatrixPanel_I2S_DMA *dma_display;
extern VirtualMatrixPanel *virtualDisp;
extern Adafruit_GFX *disp;

void initMatrix();
void drawMatrixMessage(const String &msg);
/**
 * Split 128×32 into two 64×32 boards: left = status, right = nameplate
 * (vol top / nameplate bottom). Text comes from config defaults until updated
 * via WebSocket JSON or matrixApplyBoardFields.
 */
void drawNameplateBoard(const String &ip);

/** Only keys with setX true are applied; empty/null skips that field. */
void matrixApplyBoardFields(bool setStatus, const char *status, bool setVol, const char *nameplatevol,
                            bool setPlate, const char *nameplate);
/** Writes same shape as WS: {"status":"...","nameplatevol":"...","nameplate":"..."} */
bool matrixBuildStatusJson(char *buf, size_t bufLen);

#endif
