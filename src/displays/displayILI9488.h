#ifndef displayILI9488_h
#define displayILI9488_h

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include "../libraries/ILI9488/ILI9486_SPI.h"

// ILI9488/ILI9486 — hardware-fixed resolution.
#undef DSP_WIDTH
#undef DSP_HEIGHT
#define DSP_WIDTH  480
#define DSP_HEIGHT 320
#if DSP_MODEL==DSP_ILI9488
  #define DISPLAY_MODEL_NAME "ILI9488"
#elif DSP_MODEL==DSP_ILI9486
  #define DISPLAY_MODEL_NAME "ILI9486"
#endif

typedef GFXcanvas16 Canvas;
typedef ILI9486_SPI yoDisplay;

#include "dspfont.h"

#include "tools/commongfx.h"

#include "dspconf.h"

#define ILI9488_SLPIN     0x10
#define ILI9488_SLPOUT    0x11
#define ILI9488_DISPOFF   0x28
#define ILI9488_DISPON    0x29

#endif
