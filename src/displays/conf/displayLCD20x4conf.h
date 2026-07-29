/*************************************************************************************
    LCD20x4 displays configuration file.
*************************************************************************************/

#ifndef displayLCD20x4conf_h
#define displayLCD20x4conf_h

#define TFT_FRAMEWDT    0
#define MAX_WIDTH       20
#define PLMITEMS        4
#define META_MOVE
#define BOOTLOGOTOP     0

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 0, 0, 1, WA_LEFT }, 140, true, MAX_WIDTH-6, SCROLLDELAY, 2, SCROLLTIME },
        .title1Conf          = {{ 0, 1, 1, WA_LEFT }, 140, true, MAX_WIDTH-4, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ 0, 2, 1, WA_LEFT }, 140, true, MAX_WIDTH,   SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ 1, 1, 1, WA_LEFT }, 140, true, MAX_WIDTH-1, SCROLLDELAY, 2, SCROLLTIME },
        .apTitleConf         = { }, // unused
        .apSettConf          = { }, // unused
        .weatherConf         = {{ 0, 3, 1, WA_LEFT }, 140, false, MAX_WIDTH-4, SCROLLDELAY, 2, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { }, // unused
        .metaBGConfInv       = { }, // unused
        .volbarConf          = { }, // unused
        .playlBGConf         = { }, // unused
        .bufferbarConf       = { }, // unused
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 1, 1, WA_CENTER },
        .bitrateConf         = { 0, 1, 1, WA_RIGHT },
        .voltxtConf          = { 0, 3, 1, WA_RIGHT },
        .batteryConf         = { }, // unused
        .iptxtConf           = { }, // unused
        .rssiConf            = { }, // unused
        .numConf             = { 0, 2, 1, WA_CENTER },
        .apNameConf          = { }, // unused
        .apName2Conf         = { }, // unused
        .apPassConf          = { }, // unused
        .apPass2Conf         = { }, // unused
        .clockConf           = { 0, 0, 1, WA_RIGHT },
        .vuConf              = { }, // unused
        .bootWdtConf         = { 0, 2, 1, WA_CENTER },
        .bootPrgConf         = { 250, 10, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { }, // unused
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 0, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "";
const char iptxtFmt[]             PROGMEM = "";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";
const char const_lcdApMode[]      PROGMEM = "AP/IMPROV MODE";
const char const_lcdApName[]      PROGMEM = "AP NAME: ";
const char const_lcdApPass[]      PROGMEM = "PASSWORD: ";
//#define WEATHER_FMT_SHORT
//const char weatherFmt[]           PROGMEM = "%.1fC %dmm %s%%";

#endif
