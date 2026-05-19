/*************************************************************************************
    ILI9341 320x240 displays configuration file.
    Copy this file to src/displays/conf/displayILI9341conf_custom.h
    and modify it
    More info on https://github.com/e2002/yoradio/wiki/Widgets#widgets-description
*************************************************************************************/

#ifndef displayILI9341conf_h
#define displayILI9341conf_h

#define DSP_WIDTH       320
#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#if BITRATE_FULL
  #define TITLE_FIX 44
#else
  #define TITLE_FIX 0
#endif
#define bootLogoTop     68

/* SCROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf       PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 50, 2, WA_LEFT }, 140, true, MAX_WIDTH-TITLE_FIX, 5000, 4, 30 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 70, 2, WA_LEFT }, 140, true, MAX_WIDTH-TITLE_FIX, 5000, 4, 30 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 };
const ScrollConfig weatherConf    PROGMEM = {{ 8, 87, 2, WA_LEFT }, 140, true, MAX_WIDTH, 0, 4, 30 };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 38, 0, WA_LEFT }, DSP_WIDTH, 1, false };
const FillConfig   volbarConf     PROGMEM = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH, 6, true };
const FillConfig  playlBGConf     PROGMEM = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false };
const FillConfig  heapbarConf     PROGMEM = {{ 0, 239, 0, WA_LEFT }, DSP_WIDTH, 1, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
const WidgetConfig bootstrConf    PROGMEM = { 0, 182, 1, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { 70, 191, 1, WA_LEFT };
const WidgetConfig voltxtConf     PROGMEM = { 0, 214, 1, WA_CENTER };
const WidgetConfig batteryConf    PROGMEM = { 215, 214, 1, WA_LEFT };
const WidgetConfig  iptxtConf     PROGMEM = { TFT_FRAMEWDT, 214, 1, WA_LEFT };
const WidgetConfig   rssiConf     PROGMEM = { TFT_FRAMEWDT, 214-6, 2, WA_RIGHT };
const WidgetConfig numConf        PROGMEM = { 0, 120+30, 0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { TFT_FRAMEWDT, 66, 2, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { TFT_FRAMEWDT, 90, 2, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { TFT_FRAMEWDT, 130, 2, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { TFT_FRAMEWDT, 154, 2, WA_CENTER };
const WidgetConfig  clockConf     PROGMEM = { TFT_FRAMEWDT*2, 176, 0, WA_RIGHT };
const WidgetConfig vuConf         PROGMEM = { TFT_FRAMEWDT, 100, 1, WA_LEFT };

const WidgetConfig bootWdtConf    PROGMEM = { 0, 162, 1, WA_CENTER };
const ProgressConfig bootPrgConf  PROGMEM = { 90, 14, 4 };
const BitrateConfig fullbitrateConf PROGMEM = {{DSP_WIDTH-TFT_FRAMEWDT-34, 43, 2, WA_LEFT}, 42 };

/* BANDS  */                             /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
const VUBandsConfig bandsConf     PROGMEM = { 24, 100, 4, 2, 10, 2 };

/* STRINGS  */
const char * const         numtxtFmt    PROGMEM = "%d";
const char * const           rssiFmt    PROGMEM = "WiFi %d";
const char * const          iptxtFmt    PROGMEM = "\010 %s";
const char * const         voltxtFmt    PROGMEM = "\023\025%d";
const char * const     batteryLowFmt    PROGMEM = " %d%%";       // Low battery format (marker inserted by display code)
const char * const   batteryPlainFmt    PROGMEM = " %d%%";       // Plain percent
/* Battery range formats (0-25%, 25-75%, 75-100%) - customizable per display */
// Single-byte custom glyphs for battery ranges (font must include these codes)
// - These are single-byte codes placed in your font: dec251..253 (octal \373..\375).
// - `printFix` is configured to pass bytes 0xFB..0xFD unchanged so these codes survive UTF fixes.
// - Display code composes the final string from the chosen range glyph and numeric percentage
//   and will append " !" (low) or " !!" (critical) if needed; see `src/core/display.cpp`.
// - Test: use serial debug (##BATTERY# txt HEX:) to confirm bytes (FB/FC/FD) are present at runtime.
// Mapping: 0-25% => dec11 (octal \013)
//          25-75% => dec12 (octal \014)
//          75-100% => dec15 (octal \017)
const char * const  batteryRangeLowFmt  PROGMEM = "\013 %d%%"; // 0-25%
const char * const  batteryRangeMidFmt  PROGMEM = "\014 %d%%"; // 25-75%
const char * const  batteryRangeHighFmt PROGMEM = "\017 %d%%"; // 75-100%

const char * const        bitrateFmt    PROGMEM = "%d kBs";

/* MOVES  */                             /* { left, top, width } */
const MoveConfig    clockMove     PROGMEM = { 0, 176, -1 };
const MoveConfig   weatherMove    PROGMEM = { 8, 97, MAX_WIDTH };
const MoveConfig  weatherMoveVU   PROGMEM = { 70, 97, MAX_WIDTH-70+TFT_FRAMEWDT };

#endif
