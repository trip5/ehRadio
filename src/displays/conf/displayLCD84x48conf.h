/*************************************************************************************
    LCD84x48 displays configuration file.
*************************************************************************************/

#ifndef displayLCD84x48conf_h
#define displayLCD84x48conf_h

#define TFT_FRAMEWDT    0
#define MAX_WIDTH       DSP_WIDTH
#define BOOTLOGOTOP     0

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 6, SCROLLTIME },
        .title1Conf          = {{ TFT_FRAMEWDT, 8, 1, WA_LEFT }, 140, true, MAX_WIDTH-24, SCROLLDELAY, 6, SCROLLTIME },
        .title2Conf          = { }, // unused
        .playlistConf        = {{ 2, 22, 1, WA_LEFT }, 140, true, MAX_WIDTH-4, SCROLLDELAY/5, 6, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 1, WA_CENTER }, 140, false, MAX_WIDTH, 0, 6, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 48-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 6, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 48-11, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*3-2, 0, 6, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = { }, // unused
        // .metaBGConf        = {{ MAX_WIDTH-22, 9, 0, WA_LEFT }, 1, 5, false },
        .metaBGConfInv       = { }, // unused
        .volbarConf          = {{ 0, 45, 0, WA_LEFT }, MAX_WIDTH, 3, true },
        .playlBGConf         = {{ 0, 20, 0, WA_LEFT }, DSP_WIDTH, 11, false },
        .bufferbarConf       = { }, // unused
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 48-7, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT, 8, 1, WA_RIGHT },
        .voltxtConf          = { 0, 48-11, 1, WA_RIGHT },
        .batteryConf         = { }, // unused
        .iptxtConf           = { }, // unused
        .rssiConf            = { }, // unused
        .numConf             = { 0, 34, 0, WA_CENTER },
        .apNameConf          = { 0, 8, 1, WA_CENTER },
        .apName2Conf         = { 0, 16, 1, WA_CENTER },
        .apPassConf          = { 0, 24, 1, WA_CENTER },
        .apPass2Conf         = { 0, 32, 1, WA_CENTER },
        .clockConf           = { 4, 35, 0, WA_RIGHT },
        .vuConf              = { }, // unused
        // .vuConf              = { TFT_FRAMEWDT, 50, 1, WA_LEFT },
        .bootWdtConf         = { 0, 48-7-10, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 3 },
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
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "%s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
