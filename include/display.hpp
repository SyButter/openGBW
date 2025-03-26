#pragma once

#include "config.hpp"
#include <SPI.h>
#include <U8g2lib.h>

void setupDisplay();
void showCupWeightSetScreen(double cupWeight);
void showInfoMenu();
void wakeScreen();
void showDebugModeStatus(bool debugMode);
void showDebugMenu();
void handleDebugMenuAction();
void showIpAddress();
void CenterConfirmationPrintToScreen(char const *str, u8g2_uint_t y);
