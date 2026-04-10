#ifndef rgbled_h
#define rgbled_h
#pragma once
#include "options.h"
#include <Arduino.h>

void rgbled_init();
bool rgbled_is_initialized();
void rgbled_set(uint8_t r, uint8_t g, uint8_t b);
void rgbled_playing();
void rgbled_stopped();
void rgbled_trackchange();
void rgbled_loop();

#endif // rgbled_h
