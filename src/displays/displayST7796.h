#ifndef displayST7796_h
#define displayST7796_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../libraries/Adafruit_ST7796S/Adafruit_ST7796S_kbv.h"

// ST7796 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  480
#define DSP_HEIGHT 320
#define DISPLAY_MODEL_NAME "ST7796S"

typedef GFXcanvas16 Canvas;
typedef Adafruit_ST7796S_kbv yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
