#ifndef _OLEDCOLORFIX_H_
#define _OLEDCOLORFIX_H_
#pragma once

/* Shared 1-bit palette for monochrome OLED panels, applied by each driver's
   initDisplay(). Assignments follow the theme_t declaration order in
   core/config.h so this list can be diffed directly against the grayscale
   palettes in displaySSD1322.cpp / displaySSD1327.cpp. */

  config.theme.background = TFT_BG;
  config.theme.meta       = TFT_FG;
  config.theme.metabg     = TFT_BG;
  config.theme.metafill   = TFT_FG;
  config.theme.title1     = TFT_FG;
  config.theme.title2     = TFT_FG;
  config.theme.digit      = TFT_FG;
  config.theme.div        = TFT_FG;
  config.theme.line       = TFT_FG;
  config.theme.weather    = TFT_FG;
  config.theme.vuaxis     = TFT_FG;
  config.theme.vupeak     = TFT_FG;
  config.theme.vumax      = TFT_FG;
  config.theme.vumin      = TFT_FG;
  config.theme.clock      = TFT_FG;
  config.theme.clockbg    = TFT_BG;
  config.theme.seconds    = TFT_FG;
  config.theme.secondsbg  = TFT_BG;
  config.theme.dow        = TFT_FG;
  config.theme.date       = TFT_FG;
  config.theme.clockss    = TFT_FG;
  config.theme.clockbgss  = TFT_BG;
  config.theme.secondsss  = TFT_FG;
  config.theme.secondsbgss = TFT_BG;
  config.theme.dowss      = TFT_FG;
  config.theme.datess     = TFT_FG;
  config.theme.textss     = TFT_FG;
  config.theme.buffer     = TFT_FG;
  config.theme.ip         = TFT_FG;
  config.theme.vol        = TFT_FG;
  config.theme.rssi       = TFT_FG;
  config.theme.battery    = TFT_FG;
  config.theme.bitrate    = TFT_FG;
  config.theme.volbarout  = TFT_FG;
  config.theme.volbarin   = TFT_FG;
  config.theme.plcurrent     = TFT_BG;
  config.theme.plcurrentbg   = TFT_FG;
  config.theme.plcurrentfill = TFT_FG;
  for(uint8_t i=0;i<5;i++) config.theme.playlist[i] = TFT_FG;


#endif
