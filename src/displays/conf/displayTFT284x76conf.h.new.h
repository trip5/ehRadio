/*************************************************************************************
    TFT284x76 displays configuration file.
*************************************************************************************/

#ifndef displayTFT284x76conf_h
#define displayTFT284x76conf_h

#define TFT_FRAMEWDT    2
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     8

const char _layoutNames[][32] PROGMEM = {"Default", "krzxsiek"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 21, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+18, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+18, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 1, WA_CENTER }, 140, false, MAX_WIDTH-2, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-12, 1, WA_LEFT }, 140, true, DSP_WIDTH/2+18, 0, 1, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 19, false },
        .metaBGConfInv       = {{ 0, 19, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-4, 0, WA_LEFT }, DSP_WIDTH-TFT_FRAMEWDT*2, 3, true },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-10, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+20, 64-11-10, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        // .voltxtConf          = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 64-12, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 64-11-10, 1, WA_LEFT },
        .numConf             = { TFT_FRAMEWDT, 57, 0, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .clockConf           = { 0, 57, 0, WA_RIGHT },
        .vuConf              = { 2, DSP_HEIGHT-14, 1, WA_CENTER },
        .bootWdtConf         = { 0, DSP_HEIGHT-8*2-5, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        .fullbitrateConf     = { }, // unused
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { DSP_WIDTH/2-TFT_FRAMEWDT*2-2, 7, TFT_FRAMEWDT*2+4, 1, 17, 2 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 0, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
    },
    {   // [1] krzxsiek
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, 5000, 2, 25 },
        .title1Conf          = {{ TFT_FRAMEWDT+1, 21, 1, WA_LEFT }, 135, true, DSP_WIDTH/2+18, 5000, 2, 25 },
        .title2Conf          = {{ TFT_FRAMEWDT+1, 32, 1, WA_LEFT }, 135, true, DSP_WIDTH/2+18, 5000, 2, 25 },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, 500, 2, 25 },
        .apTitleConf         = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 1, WA_CENTER }, 140, false, MAX_WIDTH-2, 0, 2, 25 },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, 25 },
        .weatherConf         = {{ TFT_FRAMEWDT+1, DSP_HEIGHT-12, 1, WA_CENTER }, 140, true, MAX_WIDTH-2, 0, 2, 30 },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 18, false },
        .metaBGConfInv       = {{ 0, 19, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-7, 0, WA_CENTER }, MAX_WIDTH, 5, true },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .bufferbarConf       = {{ TFT_FRAMEWDT, DSP_HEIGHT-1, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-10, 1, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+21, 63-10-10, 1, WA_LEFT },
        // ??? chtxtConf     = { TFT_FRAMEWDT+70, 63-10-10, 1, WA_LEFT };
        .voltxtConf          = { TFT_FRAMEWDT+110, 63-10-10, 1, WA_LEFT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 64-11, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT+1, 63-10-10, 1, WA_LEFT },
        .numConf             = { TFT_FRAMEWDT, 59, 0, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .clockConf           = { 0, 57, 0, WA_RIGHT },
        // ??? namedayConf   = { TFT_FRAMEWDT, 64-11, 1, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 226, 1, WA_LEFT };
        .vuConf              = { 2, DSP_HEIGHT-13, 1, WA_CENTER },
        .bootWdtConf         = { 0, DSP_HEIGHT-22, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 8, 64-10-10, 1, WA_LEFT }, 41 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { DSP_WIDTH/2-TFT_FRAMEWDT*2-2, 2,             TFT_FRAMEWDT*2+4, 1, 17, 2 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* BOOMBOX STYLE: middle-out VU */
        .boomboxStyle        = true,
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
