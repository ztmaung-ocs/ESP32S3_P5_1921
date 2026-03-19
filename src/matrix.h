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

#endif
