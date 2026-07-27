/*************************************************************************************
    TFT240x240round displays configuration file.
*************************************************************************************/

#ifndef displayTFT240x240roundconf_h
#define displayTFT240x240roundconf_h

#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define RSSI_DIGIT      true
#define BOOTLOGOTOP     68
#define HIDE_TITLE2
#define BOOMBOX_STYLE

const char _layoutNames[][32] PROGMEM = {"Default"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+12, TFT_FRAMEWDT+28+20, 3, WA_CENTER }, 140, true, MAX_WIDTH-24, SCROLLDELAY, 3, SCROLLTIME*5/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, /*70*/90, 2, WA_CENTER }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = { }, // unused
        // .title2Conf        = {{ TFT_FRAMEWDT, 90, 2, WA_CENTER }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 2, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT+12, TFT_FRAMEWDT+28+20, 3, WA_CENTER }, 140, false, MAX_WIDTH-24, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT+32, 240-TFT_FRAMEWDT-34, 2, WA_LEFT }, 140, false, MAX_WIDTH-64, 0, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT+30, 37, 1, WA_LEFT }, 140, true, MAX_WIDTH-60, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 32+20, 0, WA_LEFT }, DSP_WIDTH, 30, false },
        .metaBGConfInv       = {{ 0, 32+20+30, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT+56, 240-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH-112, 6+TFT_FRAMEWDT+1, true },
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        .bufferbarConf       = {{ 0, 83, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 182, 1, WA_CENTER },
        .bitrateConf         = { 134, 23, 1, WA_RIGHT },
        .voltxtConf          = { 80, 12, 1, WA_CENTER },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 214, 1, WA_CENTER },
        .rssiConf            = { 134, 23, 1, WA_LEFT },
        .numConf             = { 0, 120+30+20, 0, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 96, 2, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 118, 2, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 146, 2, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 168, 2, WA_CENTER },
        .clockConf           = { 0, 176, 0, WA_CENTER },
        .vuConf              = { TFT_FRAMEWDT+20, 188, 1, WA_CENTER },
        .bootWdtConf         = { 0, 162, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 90, 20, 6, 2, 10, 5 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 164, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 202, -1 },
        .weatherMoveVU       = { TFT_FRAMEWDT, 202, -1/*MAX_WIDTH*/ },
        .boomboxStyle        = true,
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\010 %s";
const char voltxtFmt[]            PROGMEM = "\023\025%d";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" };
const char bitrateFmt[]           PROGMEM = "%d KBS";

#endif
