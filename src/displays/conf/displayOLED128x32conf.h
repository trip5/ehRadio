/*************************************************************************************
    OLED128x32 displays configuration file.
*************************************************************************************/

#ifndef displayOLED128x32conf_h
#define displayOLED128x32conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     68

const char _layoutNames[][32] PROGMEM = {"Default"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*5-2, SCROLLDELAY, 1, SCROLLTIME },
        .title1Conf          = {{ 0, 11, 1, WA_LEFT }, 140, true, DSP_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = { }, // unused
        // .title2Conf        = {{ 0, 26, 1, WA_LEFT }, 140, true, DSP_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 14, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, 1, 1, WA_CENTER }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 32-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        .weatherConf         = {{ 0, 20, 1, WA_LEFT }, 140, true, DSP_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { }, // unused
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH/*-6*5-3*/, 9, false },
        .volbarConf          = {{ 0, 32-1-1-1, 0, WA_LEFT }, DSP_WIDTH, 3, true },
        .playlBGConf         = {{ 0, 13, 0, WA_LEFT }, DSP_WIDTH, 9, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf     = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 32-8, 1, WA_CENTER },
        .bitrateConf         = { 0, 11, 1, WA_RIGHT },
        .voltxtConf          = { 0, 20, 1, WA_RIGHT },
        .batteryConf         = { }, // unused
        .iptxtConf           = { }, // unused
        // .iptxtConf           = { 0, 64-11, 1, WA_LEFT },
        .rssiConf            = { 0, 64-11, 1, WA_RIGHT },
        .numConf             = { 0, 12, 0, WA_CENTER },
        .apNameConf          = { 0, 9, 1, WA_LEFT },
        .apName2Conf         = { 0, 9, 1, WA_RIGHT },
        .apPassConf          = { 0, 17, 1, WA_LEFT },
        .apPass2Conf         = { 0, 17, 1, WA_RIGHT },
        .clockConf           = { 0,  1, 0, WA_RIGHT },
        .vuConf              = { }, // unused
        // .vuConf            = { 1, 28, 1, WA_LEFT },
        .bootWdtConf         = { 0, 32-8*2-5, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 12, 48, 2, 1, 8, 3 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 0, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "%s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
