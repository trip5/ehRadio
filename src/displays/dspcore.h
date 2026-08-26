#ifndef dspcore_h
#define dspcore_h
#pragma once

#include "dspcolors.h"

// ==========================================================================
// dspcore.h — Display driver dispatcher
// ==========================================================================
// Selects the display driver header, sets feature flags (PSFBUFFER, DSP_OLED,
// DSP_LCD), then delegates font/bootlogo/TIME_SIZE to dspfont.h and conf file
// selection to dspconf.h.
//
// One #elif branch per DSP_MODEL (controller). Resolution variants and
// interface variants (I2C) are handled by downstream files.
// ==========================================================================

#if DSP_MODEL==DSP_DUMMY
  #define DSP_NOT_FLIPPED
  #define DISPLAY_MODEL_NAME "None"

#elif DSP_MODEL==DSP_1602 || DSP_MODEL==DSP_2004
  #define DSP_LCD
  #include "displayLC1602.h"

#elif DSP_MODEL==DSP_GC9A01A
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayGC9A01A.h"

#elif DSP_MODEL==DSP_GC9106
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayGC9106.h"

#elif DSP_MODEL==DSP_ILI9225
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayILI9225.h"

#elif DSP_MODEL==DSP_ILI9341
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayILI9341.h"

#elif DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayILI9488.h"

#elif DSP_MODEL==DSP_NOKIA5110
  #define DSP_OLED
  #include "displayN5110.h"

#elif DSP_MODEL==DSP_NV3007
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayNV3007.h"

#elif DSP_MODEL==DSP_SH1106 || DSP_MODEL==DSP_SH1107
  #define DSP_OLED
  #include "displaySH1106.h"

#elif DSP_MODEL==DSP_SSD1305
  #define DSP_OLED
  #include "displaySSD1305.h"

#elif DSP_MODEL==DSP_SSD1306
  #define DSP_OLED
  #include "displaySSD1306.h"

#elif DSP_MODEL==DSP_SSD1322
  #define DSP_OLED
  #include "displaySSD1322.h"

#elif DSP_MODEL==DSP_SSD1327
  #define DSP_OLED
  #include "displaySSD1327.h"

#elif DSP_MODEL==DSP_ST7735
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayST7735.h"

#elif DSP_MODEL==DSP_ST7789
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayST7789.h"

#elif DSP_MODEL==DSP_ST7796
  #define PSFBUFFER
  #define DSP_TFT
  #include "displayST7796.h"

#elif DSP_MODEL==DSP_ST7920
  #define DSP_OLED
  #include "displayST7920.h"

#endif

//extern DspCore dsp;

#endif
