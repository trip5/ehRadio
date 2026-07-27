/*************************************************************************************
    LCD16x2 displays configuration file.
*************************************************************************************/

#ifndef displayLCD16x2conf_h
#define displayLCD16x2conf_h

#define TFT_FRAMEWDT    0
#define MAX_WIDTH       16
#define PLMITEMS        2
#define HIDE_IP
#define HIDE_TITLE2
#define HIDE_VOL
#define HIDE_VOLBAR
#define HIDE_BUFFERBAR
#define HIDE_RSSI
#define HIDE_VU
#define HIDE_WEATHER
#define HIDE_BATTERY
#define META_MOVE
#define BOOTLOGOTOP     0

const char _layoutNames[][32] PROGMEM = {"Default"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 0, 0, 1, WA_LEFT }, 140, true, MAX_WIDTH-6, SCROLLDELAY, 2, SCROLLTIME },
        .title1Conf          = {{ 0, 1, 1, WA_LEFT }, 140, true, MAX_WIDTH-4, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = { }, // unused
        .playlistConf        = {{ 1, 1, 1, WA_LEFT }, 140, true, MAX_WIDTH-1, SCROLLDELAY, 2, SCROLLTIME },
        .apTitleConf         = { }, // unused
        .apSettConf          = { }, // unused
        .weatherConf         = { }, // unused
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { }, // unused
        .metaBGConfInv       = { }, // unused
        .volbarConf          = { }, // unused
        .playlBGConf         = { }, // unused
        .bufferbarConf       = { }, // unused
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 0, 1, WA_CENTER },
        .bitrateConf         = { 0, 1, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { }, // unused
        .iptxtConf           = { }, // unused
        .rssiConf            = { }, // unused
        .numConf             = { 0, 1, 1, WA_CENTER },
        .apNameConf          = { }, // unused
        .apName2Conf         = { }, // unused
        .apPassConf          = { }, // unused
        .apPass2Conf         = { }, // unused
        .clockConf           = { 0, 0, 1, WA_RIGHT },
        .vuConf              = { }, // unused
        .bootWdtConf         = { 0, 1, 1, WA_CENTER },
        .bootPrgConf         = { 250, 10, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { }, // unused
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 0, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "";
const char iptxtFmt[]             PROGMEM = "";
const char voltxtFmt[]            PROGMEM = "";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" }; // probably useless
const char bitrateFmt[]           PROGMEM = "%d";
const char const_lcdApMode[]      PROGMEM = "AP-IMPROV MODE";
const char const_lcdApName[]      PROGMEM = "AP NAME: ";
const char const_lcdApPass[]      PROGMEM = "PASSWORD: ";
//const char bootstrFmt[]           PROGMEM = "Wifi- %s";

#endif
