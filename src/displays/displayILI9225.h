#ifndef displayILI9225_h
#define displayILI9225_h
#include "../core/options.h"
//==================================================
#include <Arduino.h>
#include "../libraries/Adafruit_ILI9225/Adafruit_ILI9225.h"

// ILI9225 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  220
#define DSP_HEIGHT 176
#define DISPLAY_MODEL_NAME "ILI9225"

typedef GFXcanvas16 Canvas;
typedef Adafruit_ILI9225 yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#endif
