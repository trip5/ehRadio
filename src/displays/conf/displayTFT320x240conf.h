/*************************************************************************************
    TFT320x240 displays configuration file.
*************************************************************************************/

#ifndef displayTFT320x240conf_h
#define displayTFT320x240conf_h

#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#define BOOTLOGOTOP     68

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME*5/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 48, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 72, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 2, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 87, 2, WA_LEFT }, 140, true, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false },
        .metaBGConfInv       = {{ 0, 38, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH, 6, true },
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        .bufferbarConf       = {{ 0, 239, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 182, 1, WA_CENTER },
        .bitrateConf         = { 70, 191, 1, WA_LEFT },
        .voltxtConf          = { 0, 214, 1, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, 214, 1, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, 214, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 208, 2, WA_RIGHT },
        .numConf             = { 0, 150, 0, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 66, 2, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 90, 2, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 130, 2, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 154, 2, WA_CENTER },
        .clockConf           = { 8, 176, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 100, 1, WA_LEFT },
        .bootWdtConf         = { 0, 162, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_FRAMEWDT-34, 43, 2, WA_LEFT }, 42 },
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 24, 100, 4, 2, 10, 2 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 8, 180, -1 },
        .weatherMove         = { TFT_FRAMEWDT, 97, MAX_WIDTH },
        .weatherMoveVU       = { 70, 97, MAX_WIDTH-70+TFT_FRAMEWDT },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[] PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
