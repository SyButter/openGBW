#pragma once

#include "scale.hpp"
#include <SPI.h>
#include <U8g2lib.h>
#ifndef DISPLAY_HPP
#define DISPLAY_HPP

// Declare the global `u8g2` object
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

// Other function declarations...
void setupDisplay();
bool isDisplayOn();
void wakeDisplay();

#endif

void setupDisplay();
