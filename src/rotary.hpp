#ifndef ROTARY_HPP
#define ROTARY_HPP
#include "config.hpp"

#include <AiEsp32RotaryEncoder.h>

// Declare global rotary encoder object
extern AiEsp32RotaryEncoder rotaryEncoder;

// Declare rotary methods
void setupRotary();
void handleRotary();
bool isDisplayOn();
void wakeDisplay();

#endif
