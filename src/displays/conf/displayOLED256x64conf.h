/*************************************************************************************
    OLED256x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED256x64conf_h
#define displayOLED256x64conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     8

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ 0, 20, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+2, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ 0, 29, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+2, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 1, WA_CENTER }, 140, false, MAX_WIDTH-2, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        .weatherConf         = {{ 0, 64-12, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+2, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { }, // unused
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 18, false },
        .volbarConf          = {{ 0, 64-1-1-1, 0, WA_LEFT }, DSP_WIDTH, 3, true },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 64-8, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+20, 64-11-10, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        // .voltxtConf          = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { 0, 64-12, 1, WA_LEFT },
        .rssiConf            = { 0, 64-11-10, 1, WA_LEFT },
        .numConf             = { TFT_FRAMEWDT, 57, 0, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .clockConf           = { 0, 57, 0, WA_RIGHT },
        // .clockConf         = { 6, 34, 2, WA_CENTER },
        .vuConf              = { }, // unused
        //.vuConf              = { 1, 28, 1, WA_LEFT },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
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
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
