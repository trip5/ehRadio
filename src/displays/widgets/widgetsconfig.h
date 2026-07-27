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
  uint8_t  fadespeed;
};

struct MoveConfig {
  uint16_t x;
  uint16_t y;
  int16_t width;
};

struct BitrateConfig {
  WidgetConfig widget;
  uint16_t dimension;
};

struct LayoutData {
    ScrollConfig metaConf;
    ScrollConfig title1Conf;
    ScrollConfig title2Conf;
    ScrollConfig playlistConf;
    ScrollConfig apTitleConf;
    ScrollConfig apSettConf;
    ScrollConfig weatherConf;
    FillConfig   metaBGConf;
    FillConfig   metaBGConfInv;
    FillConfig   volbarConf;
    FillConfig   playlBGConf;
    FillConfig   bufferbarConf;
    WidgetConfig bootstrConf;
    WidgetConfig bitrateConf;
    WidgetConfig voltxtConf;
    WidgetConfig batteryConf;
    WidgetConfig iptxtConf;
    WidgetConfig rssiConf;
    WidgetConfig numConf;
    WidgetConfig apNameConf;
    WidgetConfig apName2Conf;
    WidgetConfig apPassConf;
    WidgetConfig apPass2Conf;
    WidgetConfig clockConf;
    WidgetConfig vuConf;
    WidgetConfig bootWdtConf;
    ProgressConfig bootPrgConf;
    BitrateConfig fullbitrateConf;
    VUBandsConfig bandsConf;
    MoveConfig   clockMove;
    MoveConfig   weatherMove;
    MoveConfig   weatherMoveVU;
    bool         boomboxStyle;
};

// Layout switching — extern pointer declarations, defined in display.cpp
extern const ScrollConfig* metaConf_ptr;
extern const ScrollConfig* title1Conf_ptr;
extern const ScrollConfig* title2Conf_ptr;
extern const ScrollConfig* playlistConf_ptr;
extern const ScrollConfig* apTitleConf_ptr;
extern const ScrollConfig* apSettConf_ptr;
extern const ScrollConfig* weatherConf_ptr;
extern const FillConfig*   metaBGConf_ptr;
extern const FillConfig*   metaBGConfInv_ptr;
extern const FillConfig*   volbarConf_ptr;
extern const FillConfig*   playlBGConf_ptr;
extern const FillConfig*   bufferbarConf_ptr;
extern const WidgetConfig* bootstrConf_ptr;
extern const WidgetConfig* bitrateConf_ptr;
extern const WidgetConfig* voltxtConf_ptr;
extern const WidgetConfig* batteryConf_ptr;
extern const WidgetConfig* iptxtConf_ptr;
extern const WidgetConfig* rssiConf_ptr;
extern const WidgetConfig* numConf_ptr;
extern const WidgetConfig* apNameConf_ptr;
extern const WidgetConfig* apName2Conf_ptr;
extern const WidgetConfig* apPassConf_ptr;
extern const WidgetConfig* apPass2Conf_ptr;
extern const WidgetConfig* clockConf_ptr;
extern const WidgetConfig* vuConf_ptr;
extern const WidgetConfig* bootWdtConf_ptr;
extern const ProgressConfig* bootPrgConf_ptr;
extern const BitrateConfig*  fullbitrateConf_ptr;
extern const VUBandsConfig*   bandsConf_ptr;
extern const MoveConfig*    clockMove_ptr;
extern const MoveConfig*    weatherMove_ptr;
extern const MoveConfig*    weatherMoveVU_ptr;
extern const bool*          boomboxStyle_ptr;

extern LayoutData activeLayout;
extern uint8_t layoutCount;

#endif
