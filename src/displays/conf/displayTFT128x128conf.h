/*************************************************************************************
    TFT128x128 displays configuration file.
*************************************************************************************/

#ifndef displayTFT128x128conf_h
#define displayTFT128x128conf_h

#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     16

const char _layoutNames[][32] PROGMEM = {"Default"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 26, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 36, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*3-4, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 56, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 128-TFT_FRAMEWDT-8, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 42, 1, WA_LEFT }, 140, true, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        .metaBGConfInv       = {{ 0, 22, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, 118, 0, WA_LEFT }, MAX_WIDTH-6*3-4, 5, true },
        .playlBGConf         = {{ 0, 52, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        .bufferbarConf       = {{ 0, 127, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 110, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT, 36, 1, WA_RIGHT },
        .voltxtConf          = { TFT_FRAMEWDT, 128-10, 1, WA_RIGHT },
        // .voltxtConf        = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 108, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 108, 1, WA_RIGHT },
        .numConf             = { 0, 86, 0, WA_CENTER },
        .apNameConf          = { 0, 40, 1, WA_CENTER },
        .apName2Conf         = { 0, 54, 1, WA_CENTER },
        .apPassConf          = { 0, 74, 1, WA_CENTER },
        .apPass2Conf         = { 0, 88, 1, WA_CENTER },
        .clockConf           = { 0, 94, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 99, 1, WA_CENTER },
        .bootWdtConf         = { 0, 90, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 56, 7, 2, 1, 8, 2 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 94, -1 },
        .weatherMove         = { TFT_FRAMEWDT, 48, 122 },
        .weatherMoveVU       = { TFT_FRAMEWDT, 48, 122 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "\010 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" };
const char bitrateFmt[]           PROGMEM = "%d";

#endif
