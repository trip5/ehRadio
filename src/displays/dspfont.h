#ifndef dspfont_h
#define dspfont_h
#pragma once

// ==========================================================================
// dspfont.h — Centralized font, bootlogo, and TIME_SIZE selection
// ==========================================================================
// Selects bootlogo, clock font, and TIME_SIZE based on DSP_HEIGHT.
// DSP_WIDTH only used to differentiate 160x128 (wide) from 128x128 (narrow).
// DSP_OLED/DSP_TFT macros from dspcore.h used where OLED needs smaller fonts.
// ==========================================================================

/* --- DISPLAY FONT (Unicode GFXfont) --- */
// Select the main UI font (converted from BDF via bdf2adafruit3.py).
// Options: MATRIXLIGHT (default), MATRIXCHUNKY, X11.
#if DISPLAYFONT == MATRIXLIGHT
  #include "fonts/MatrixLight8x6.h"
  #define DisplayFont MatrixLight8x6
#elif DISPLAYFONT == MATRIXCHUNKY
  #include "fonts/MatrixChunky8x6.h"
  #define DisplayFont MatrixChunky8x6
#elif DISPLAYFONT == X11
  #include "fonts/UnixX11_6x9.h"
  #define DisplayFont Fixed
#else
  #warning "DISPLAYFONT value not recognized, defaulting to MatrixChunky8x6"
  #include "fonts/MatrixChunky8x6.h"
  #define DisplayFont MatrixChunky8x6
#endif

/* --- TIME_SIZE --- */
// Outlier overrides (checked before height-based rules)
#if DSP_MODEL==DSP_DUMMY
  #define TIME_SIZE 0
#elif DSP_MODEL==DSP_ST7920
  #define TIME_SIZE 2

// Height-based defaults
#elif DSP_HEIGHT >= 320        // 480x320
  #define TIME_SIZE 70
#elif DSP_HEIGHT >= 240        // 320x240, 240x240
  #define TIME_SIZE 52
#elif DSP_HEIGHT >= 128        // 220x176, 160x128, 160x80, 428x142, 128x128, 284x76
  #define TIME_SIZE 35
#elif DSP_HEIGHT == 64 && defined(DSP_OLED) && DSP_WIDTH >= 256  // 256x64 OLED — wider, fits larger font
  #define TIME_SIZE 35
#elif DSP_HEIGHT == 64 && defined(DSP_OLED)  // 128x64 OLED
  #define TIME_SIZE 15
#elif DSP_HEIGHT == 64         // non-OLED 64px-high (fallback)
  #define TIME_SIZE 35
#elif DSP_HEIGHT == 48         // 84x48 Nokia
  #define TIME_SIZE 15
#elif defined(DSP_LCD)         // character LCDs
  #define TIME_SIZE 1
#else                          // 128x32
  #define TIME_SIZE 1
#endif

// Chunky-font override — when using YO_MONO on small OLEDs,
// reverts to display font size (moved from dspcore.h)
#if TIME_SIZE == 15 && CLOCKFONT == YO_MONO
  #undef TIME_SIZE
  #define TIME_SIZE 2
#endif

/*--- BOOTLOGO ---*/
#if DSP_HEIGHT >= 320 && defined(BIG_BOOT_LOGO) && BIG_BOOT_LOGO
  #include "bootlogo/198x128.h"
#elif DSP_HEIGHT >= 176           // 480x320, 320x240, 240x240, 220x176
  #include "bootlogo/99x64.h"
#elif DSP_WIDTH >= 160 && DSP_HEIGHT >= 128   // 160x128 (wider than 128x128)
  #include "bootlogo/99x64.h"
#elif DSP_HEIGHT >= 76            // 128x128, 284x76, 160x80
  #include "bootlogo/62x40.h"
#elif DSP_HEIGHT >= 64            // 128x64, 256x64
  #include "bootlogo/110x32mono.h"
#elif DSP_HEIGHT >= 48            // 84x48 Nokia
  #include "bootlogo/36x32mono.h"
// 128x32, character LCDs: no bootlogo
#endif

/* ---  CLOCK FONT --- */
#if DSP_HEIGHT >= 320             // 480x320
  #include "clockfonts/font70.h"
#elif DSP_HEIGHT >= 240           // 320x240, 240x240
  #include "clockfonts/font52.h"
#elif DSP_HEIGHT >= 76            // 220x176, 160x128, 160x80, 284x76, 428x142, 128x128
  #include "clockfonts/font35.h"
#elif DSP_HEIGHT == 64 && defined(DSP_OLED) && DSP_WIDTH >= 256  // 256x64 OLED
  #include "clockfonts/font35.h"
#elif DSP_HEIGHT == 64 && defined(DSP_OLED) && DSP_MODEL!=DSP_ST7920  // 128x64 OLED
  #include "clockfonts/font15.h"
#elif DSP_HEIGHT == 48            // 84x48 Nokia
  #include "clockfonts/TinyFont5.h"
  #include "clockfonts/TinyFont6.h"
// 128x32, character LCDs: use GLCD font (no include)
#endif

#endif // dspfont_h
