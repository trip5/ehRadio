#ifndef COMMON_H
#define COMMON_H
#pragma once

enum displayMode_e { PLAYER, VOL, STATIONS, NUMBERS, LOST, UPDATING, INFO, SETTINGS, TIMEZONE, WIFI, CLEAR, SLEEPING, SDCHANGE, SCREENSAVER, SCREENBLANK, SDMAN, VALIDATING };
enum pages_e      : uint8_t  { PG_PLAYER=0, PG_DIALOG=1, PG_PLAYLIST=2, PG_SCREENSAVER=3 };

// SDINDEXSTART/SDINDEXDONE bracket a re-index of the SD card that is NOT the boot scan: the File Manager's close-time
// walk.  They are requests rather than a flag on the caller because the hold they set has to live on the display task -
// see the guard in Display::_switchMode().
enum displayRequestType_e { BOOTSTRING, NEWMODE, CLOCK, NEWTITLE, NEWSTATION, NEXTSTATION, DRAWPLAYLIST, CLOSEPLAYLIST, DRAWVOL, DBITRATE, SHOWBUFFERBAR, SHOWVUMETER, DSPRSSI, SHOWWEATHER, NEWWEATHER, PSTOP, PSTART, DSP_START, WAITFORSD, FORMATTING, SCANNINGWIFI, SDFILEINDEX, SDINDEXSTART, SDINDEXDONE, NEWIP, DSPBATTERY, APPLYSTATE, THEMEPREVIEW, LAYOUTPREVIEW, SSREBUILD, BOOTVSPATCH, BOOTVERSION, NOPE };
struct requestParams_t
{
  displayRequestType_e type;
  int payload;
};

enum controlEvt_e { EVT_NONE=255, EVT_BTN_DOWN=0, EVT_BTN_PLAY=1, EVT_BTN_UP=2, EVT_ENC_SW=3, EVT_BTN_PREV=4, EVT_BTN_NEXT=5, EVT_ENC2_SW=6, EVT_BTN_MODE=7 };



#endif
