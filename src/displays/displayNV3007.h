#ifndef displayNV3007_h
#define displayNV3007_h

#include <Arduino.h>
#include "../libraries/NV3007/Adafruit_NV3007.h"

// NV3007 2.79" bar display — hardware-fixed 142x428 portrait
// (428x142 landscape once rotated via flip()).
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  428
#define DSP_HEIGHT 142
#define DISPLAY_MODEL_NAME "NV3007"

typedef GFXcanvas16 Canvas;
typedef Adafruit_NV3007 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
