#ifndef dspcolors_h
#define dspcolors_h
#pragma once

// Centralized display colour macros selected by active DSP model.
//
// Only four macros are consumed outside this file:
//   BOOT_PRG_COLOR / BOOT_TXT_COLOR - boot screen (core/display.cpp)
//   TFT_BG / TFT_FG                 - OLED theme init (tools/oledcolorfix.h)
//                                     and DspCore::clearDsp()
//
// Panel-specific palettes live with their own driver instead, so this file
// stays a minimal shared contract.  The grayscale OLED palette for SSD1327 is
// in displaySSD1327.cpp; SSD1322's lives in its driver plus the SSD1322 library.
#if DSP_MODEL==DSP_SH1106 || DSP_MODEL==DSP_SH1107
  #define BOOT_PRG_COLOR    SH110X_WHITE
  #define BOOT_TXT_COLOR    SH110X_WHITE
  #define TFT_BG            SH110X_BLACK
  #define TFT_FG            SH110X_WHITE

#elif DSP_MODEL==DSP_SSD1306 || DSP_MODEL==DSP_SSD1305 || DSP_MODEL==DSP_SSD1322
  #define BOOT_PRG_COLOR    WHITE
  #define BOOT_TXT_COLOR    WHITE
  #define TFT_BG            BLACK
  #define TFT_FG            WHITE
  // looking for other SSD1322 colors?  They are in display1322.cpp

#elif DSP_MODEL==DSP_SSD1327
  #define BOOT_PRG_COLOR    0x7F
  #define BOOT_TXT_COLOR    0x7F
  #define TFT_BG            0x00
  #define TFT_FG            0x7F

#else
  #define BOOT_PRG_COLOR    65048 // 0xE68B
  #define BOOT_TXT_COLOR    50719 // 0xFFFF
#endif

#ifndef TFT_BG
  #define TFT_BG            BLACK
#endif

#ifndef TFT_FG
  #define TFT_FG            WHITE
#endif

#endif
