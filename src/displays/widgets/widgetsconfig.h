#ifndef widgetsconfig_h
#define widgetsconfig_h

#include <stdint.h>

enum WidgetAlign { WA_LEFT, WA_CENTER, WA_RIGHT };
enum BitrateFormat { BF_UNKNOWN, BF_MP3, BF_AAC, BF_FLAC, BF_WAV, BF_VOR, BF_OPU };

struct WidgetConfig {
  uint16_t left; 
  uint16_t top; 
  uint16_t textsize;
  WidgetAlign align;
};

struct ScrollConfig {
  WidgetConfig widget;
  uint16_t buffsize;
  bool uppercase;
  uint16_t width;
  uint16_t startscrolldelay;
  uint8_t scrolldelta;
  uint16_t scrolltime;
};

struct FillConfig {
  WidgetConfig widget;
  uint16_t width;
  uint16_t height;
  bool outlined;
};

struct ProgressConfig {
  uint16_t speed;
  uint16_t width;
  uint16_t barwidth;
};

struct VUBandsConfig {
  uint16_t width;
  uint16_t height;
  uint8_t  space;
  uint8_t  vspace;
  uint8_t  perheight;
  // There is no fade speed here on purpose: the decay rate is derived from the band
  // length and VU_FADE_MS in options.h, so every bar falls in the same time no matter
  // how long it is or which panel it is on.
};

struct MoveConfig {
  uint16_t x;
  uint16_t y;
  int16_t width;
};

// Ids are persisted in config.store.vustyle and are the keys in /visuals.json, so they must never be renumbered: a new
// style is only ever appended, before VU_STYLE_COUNT. The label a style shows the user lives beside the code that
// draws it, in the /visuals.json table in netserver.cpp.
enum vuStyle_e : uint8_t {
  VU_STYLE_BARS = 0,          // the segmented original bar VU inherited from yoRadio - "Bars"
  VU_STYLE_DIGITAL_LED,       // the same painter, lit boundary snapped to whole segments - "Digital LED"
  VU_STYLE_HISTORY,           // level over time, both channels
  VU_STYLE_SPECTRUM_REFLECT,  // bands across the width, L above the baseline and R below it
  VU_STYLE_SPECTRUM_MIRROR,   // half the width each, full height, L mirrored on the left and R on the right
  VU_STYLE_WAVE,              // waveform, I2S only
  VU_STYLE_LISSAJOUS,         // vectorscope, I2S only
  VU_STYLE_COUNT
};

struct BitrateConfig {
  WidgetConfig widget;
  uint16_t dimension;
};

struct BootData {
    /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
    ScrollConfig   apTitleConf;
    ScrollConfig   apSettConf;
    /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
    FillConfig     apTitleBGConf;   // The AP and SD-manager band.  Boot screens must not borrow a layout's
                                    // metaBGConf: a layout is selectable, the boot screens are not.  Invert
                                    // title does not apply to them either.  The band on TFT, the hairline on
                                    // OLED (conf_tool.py derives both from the layout's pair).
    /* WIDGETS             { left, top, fontsize, align } */
    WidgetConfig   bootstrConf;
    WidgetConfig   apNameConf;
    WidgetConfig   apName2Conf;
    WidgetConfig   apPassConf;
    WidgetConfig   apPass2Conf;
    WidgetConfig   bootWdtConf;
    /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
    ProgressConfig bootPrgConf;
};

struct LayoutData {
    /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
    ScrollConfig metaConf;
    ScrollConfig title1Conf;
    ScrollConfig title2Conf;
    ScrollConfig playlistConf;
    ScrollConfig weatherConf;
    /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
    FillConfig   volbarConf;
    FillConfig   bufferbarConf;
    /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
    FillConfig   metaBGConf;
    FillConfig   metaBGConfInv;
    FillConfig   underLineConf;
    FillConfig   overLineConf;
    FillConfig   playlBGConf;
    /* WIDGETS             { left, top, fontsize, align } */
    WidgetConfig bitrateConf;
    WidgetConfig voltxtConf;
    WidgetConfig batteryConf;
    WidgetConfig iptxtConf;
    WidgetConfig rssiConf;
    WidgetConfig numConf;
    WidgetConfig clockConf;
    WidgetConfig vuConf;
    /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
    BitrateConfig fullbitrateConf;
    /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
    VUBandsConfig bandsConf;
    /* MOVES               { left, top, width (-1 keeps Conf position) */
    MoveConfig   clockMove;
    MoveConfig   weatherMove;
    MoveConfig   weatherMoveVU;
    /* TRANSFORMS          boolean */
    bool         boomboxVU; // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
    bool         rotateVU; // VU rotated 90 degrees
    bool         shareWeatherIP; // IP and weather share one row 
    bool         shareBattRSSI; // RSSI and battery share one row
    bool         rssiDigit; // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
};

// Layout switching — extern pointer declarations, defined in display.cpp
extern const ScrollConfig* metaConf_ptr;
extern const ScrollConfig* title1Conf_ptr;
extern const ScrollConfig* title2Conf_ptr;
extern const ScrollConfig* playlistConf_ptr;
extern const ScrollConfig* weatherConf_ptr;
extern const FillConfig*   metaBGConf_ptr;
extern const FillConfig*   metaBGConfInv_ptr;
extern const FillConfig*   volbarConf_ptr;
extern const FillConfig*   playlBGConf_ptr;
extern const FillConfig*   bufferbarConf_ptr;
extern const WidgetConfig* bitrateConf_ptr;
extern const WidgetConfig* voltxtConf_ptr;
extern const WidgetConfig* batteryConf_ptr;
extern const WidgetConfig* iptxtConf_ptr;
extern const WidgetConfig* rssiConf_ptr;
extern const WidgetConfig* numConf_ptr;
extern const WidgetConfig* clockConf_ptr;
extern const WidgetConfig* vuConf_ptr;
extern const BitrateConfig*  fullbitrateConf_ptr;
extern const VUBandsConfig*   bandsConf_ptr;
extern const MoveConfig*    clockMove_ptr;
extern const MoveConfig*    weatherMove_ptr;
extern const MoveConfig*    weatherMoveVU_ptr;
extern const bool*          boomboxVU_ptr;
extern const bool*          rotateVU_ptr;
extern const bool*          shareWeatherIP_ptr;
extern const bool*          shareBattRSSI_ptr;
extern const bool*          rssiDigit_ptr;
extern const FillConfig*    underLineConf_ptr;
extern const FillConfig*    overLineConf_ptr;

extern LayoutData activeLayout;
extern uint8_t layoutCount;

#endif
