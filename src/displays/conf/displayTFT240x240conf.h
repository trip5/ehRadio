/*************************************************************************************
    TFT240x240 displays configuration file.
*************************************************************************************/

#ifndef displayTFT240x240conf_h
#define displayTFT240x240conf_h

#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define PLMITEMS        11
#define PLMITEMLENGHT   40
#define PLMITEMHEIGHT   22
#define BOOTLOGOTOP     68

const char _layoutNames[][32] PROGMEM = {"Default"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME*5/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 50, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 70, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 2, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 198, 1, WA_LEFT }, 140, true, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false },
        .metaBGConfInv       = {{ 0, 38, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH, 6, true },
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        .bufferbarConf       = {{ 0, 239, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 182, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT, 188, 1, WA_LEFT },
        .voltxtConf          = { 80, 214, 1, WA_RIGHT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 214, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 214-6, 2, WA_RIGHT },
        .numConf             = { 0, 120+30, 0, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 66, 2, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 90, 2, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 130, 2, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 154, 2, WA_CENTER },
        .clockConf           = { 0, 168, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 94, 1, WA_CENTER },
        .bootWdtConf         = { 0, 162, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 100, 20, 10, 2, 10, 5 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 202, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, 202, MAX_WIDTH },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\010 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" };
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
