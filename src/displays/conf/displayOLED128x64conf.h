/*************************************************************************************
    OLED128x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED128x64conf_h
#define displayOLED128x64conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     8
#define IP_WEATHER_SHARED true // these widgets share the same space
#define RSSI_BATT_SHARED true // these widgets share the same space

#if CLOCKFONT == YO_MONO
  #define FONTSHIFT 0
#else // CHUNKY6 CHUNKY6_PX
  #define FONTSHIFT 15
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 64-8, 1, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 19, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 28, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-9, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 17, false },
        .volbarConf          = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { 0, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-9, 1, WA_RIGHT },
        .iptxtConf           = { TFT_FRAMEWDT, 64-9, 1, WA_LEFT },
        .rssiConf            = { 0, 64-9, 1, WA_RIGHT },
        .numConf             = { 0, 28+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 38+FONTSHIFT, 0, WA_CENTER },
        .vuConf              = { }, // unused
        // .vuConf              = { 1, 28, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 12, 48, 2, 1, 8, 3 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 0, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
