/*************************************************************************************
    TFT160x80 displays configuration file.
*************************************************************************************/

#ifndef displayTFT160x80conf_h
#define displayTFT160x80conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     5

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 80-TFT_FRAMEWDT-8, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 65, 1, WA_CENTER },
        .apNameConf          = { 0, 20, 1, WA_CENTER },
        .apName2Conf         = { 0, 32, 1, WA_CENTER },
        .apPassConf          = { 0, 46, 1, WA_CENTER },
        .apPass2Conf         = { 0, 58, 1, WA_CENTER },
        .bootWdtConf         = { 0, 50, 1, WA_CENTER },
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
        .title1Conf          = {{ TFT_FRAMEWDT, 19, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*3-4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = { }, // unused
        // .title2Conf        = {{ TFT_FRAMEWDT, 36, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 33, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 80-13, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*3, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 16, false },
        .metaBGConfInv       = {{ 0, 16, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, 80-1-1-2, 0, WA_LEFT }, MAX_WIDTH, 2, false },
        .playlBGConf         = {{ 0, 30, 0, WA_LEFT }, DSP_WIDTH, 20, false },
        .bufferbarConf       = {{ 0, 79, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { TFT_FRAMEWDT, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        // .voltxtConf        = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { }, // unused
        // .iptxtConf         = { TFT_FRAMEWDT, 108, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 80-13, 1, WA_RIGHT },
        .numConf             = { 0, 29+32, 0, WA_CENTER },
        .clockConf           = { 20, 29+34, 0, WA_RIGHT },
        .vuConf              = { 1, 28, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 12, 48, 2, 1, 8, 2 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 6, 29+34, 0},
        .weatherMove         = { TFT_FRAMEWDT, 80-13, MAX_WIDTH-6*3-30 },
        .weatherMoveVU       = { 30, 80-13, MAX_WIDTH-6*3-30 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
