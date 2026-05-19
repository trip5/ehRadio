#ifndef dspcore_h
#define dspcore_h
#pragma once

#if DSP_MODEL==DSP_DUMMY
  #define DUMMYDISPLAY
  #define DSP_NOT_FLIPPED

#elif DSP_MODEL==DSP_ST7735
  #define TIME_SIZE           35
  #define PSFBUFFER
  #include "displayST7735.h"

#elif DSP_MODEL==DSP_SSD1306
  #define TIME_SIZE           2
  #define DSP_OLED
  #include "displaySSD1306.h"

#elif DSP_MODEL==DSP_SSD1306x32
  #define TIME_SIZE           1
  #define DSP_OLED
  #include "displaySSD1306.h"

#elif DSP_MODEL==DSP_NOKIA5110
  #define TIME_SIZE           19
  #define DSP_OLED
  #include "displayN5110.h"

#elif DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7789_240
  #define TIME_SIZE           52
  #define PSFBUFFER
  #include "displayST7789.h"

#elif DSP_MODEL==DSP_ST7789_76
  #define TIME_SIZE           35
  #define PSFBUFFER
  #include "displayST7789.h"

#elif DSP_MODEL==DSP_SH1106 || DSP_MODEL==DSP_SH1107
  #define TIME_SIZE           2
  #define DSP_OLED
  #include "displaySH1106.h"

#elif DSP_MODEL==DSP_1602I2C || DSP_MODEL==DSP_2004I2C || DSP_MODEL==DSP_1602 || DSP_MODEL==DSP_2004
  #define TIME_SIZE           1
  #define DSP_LCD
  #include "displayLC1602.h"

#elif DSP_MODEL==DSP_SSD1327
  #define TIME_SIZE           35
  #define DSP_OLED
  #include "displaySSD1327.h"

#elif DSP_MODEL==DSP_ILI9341
  #define TIME_SIZE           52
  #define PSFBUFFER
  #include "displayILI9341.h"

#elif DSP_MODEL==DSP_SSD1305 || DSP_MODEL==DSP_SSD1305I2C
  #define TIME_SIZE           2
  #define DSP_OLED
  #include "displaySSD1305.h"

#elif DSP_MODEL==DSP_GC9106
  #define TIME_SIZE           35
  #define PSFBUFFER
  #include "displayGC9106.h"

#elif DSP_MODEL==DSP_CUSTOM
  #define TIME_SIZE           0
  #include "displayCustom.h"

#elif DSP_MODEL==DSP_ILI9225
  #define TIME_SIZE           35
  #include "displayILI9225.h"

#elif DSP_MODEL==DSP_ST7796
  #define TIME_SIZE           70
  #define PSFBUFFER
  #include "displayST7796.h"

#elif DSP_MODEL==DSP_GC9A01A
  #define TIME_SIZE           52
  #define PSFBUFFER
  #include "displayGC9A01A.h"

#elif DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486
  #define TIME_SIZE           70
  #define PSFBUFFER
  #include "displayILI9488.h"

#elif DSP_MODEL==DSP_SSD1322
  #define TIME_SIZE           35
  #define DSP_OLED
  #include "displaySSD1322.h"

#elif DSP_MODEL==DSP_ST7920
  #define TIME_SIZE           2
  #define DSP_OLED
  #include "displayST7920.h"

#endif

//extern DspCore dsp;

#endif
