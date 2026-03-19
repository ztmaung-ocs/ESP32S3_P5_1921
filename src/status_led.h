/**
 * Status LED and Reset button handling
 */

#ifndef STATUS_LED_H
#define STATUS_LED_H

extern bool g_resetButtonHeld;
extern bool g_resetButtonPressed;

void initStatusLed();
void ledOff();
void ledRed();
void ledGreen();
void ledBlue();
void ledYellow();
void updateStatusLed();
void handleResetButtonRuntime();

#endif
