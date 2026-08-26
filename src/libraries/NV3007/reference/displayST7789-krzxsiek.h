#ifndef displayST7789_h
#define displayST7789_h

#include "Arduino.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#if DSP_MODEL==DSP_ST7789_76
  //#include "fonts/bootlogo62x40.h"
  #include "fonts/bootlogo160x40.h" /* nowe logo ~ kc-dev (10.02.26) */
  #include "fonts/dsfont35.h"
#elif DSP_MODEL==DSP_NV3007_142
  //#include "fonts/bootlogo62x40.h"
  #include "fonts/bootlogo270x80.h" /* nowe logo ~ kc-dev (10.02.26) */
  #include "fonts/dsfont52.h"
#else
  #include "fonts/bootlogo99x64.h"
  #include "fonts/dsfont52.h"
#endif

typedef GFXcanvas16 Canvas;
typedef Adafruit_ST7789 yoDisplay;

#include "tools/commongfx.h"

#if __has_include("conf/displayST7789conf_custom.h")
  #include "conf/displayST7789conf_custom.h"
#else
  #if DSP_MODEL==DSP_ST7789
    #include "conf/displayST7789conf.h"
  #elif DSP_MODEL==DSP_ST7789_76
    #include "conf/displayST7789_76conf.h"
  #elif DSP_MODEL==DSP_NV3007_142
    #include "conf/displayNV3007_142conf.h"
  #endif
#endif

#endif
