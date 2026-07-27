/*************************************************************************************
    TFT480x320 displays configuration file.
*************************************************************************************/

#ifndef displayTFT480x320conf_h
#define displayTFT480x320conf_h

#define TFT_FRAMEWDT    10
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#if defined(BIG_BOOT_LOGO) || BIG_BOOT_LOGO // true
  #define BOOTLOGOTOP   60
#else
  #define BOOTLOGOTOP   110
#endif

const char _layoutNames[][32] PROGMEM = {"Default", "VaraiTamas (BoomBox)"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_FRAMEWDT, 62, 2, WA_LEFT }, 140, true, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 86, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 146, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_CENTER }, 140, false, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 320-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 120, 2, WA_LEFT }, 140, true, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-8, 0, WA_LEFT }, MAX_WIDTH, 8, true },
        .playlBGConf         = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 243, 2, WA_CENTER },
        .bitrateConf         = { 6, 62, 2, WA_RIGHT },
        .voltxtConf          = { 0, DSP_HEIGHT-38, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-38, 2, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, DSP_HEIGHT-38-6, 3, WA_RIGHT },
        .numConf             = { 0, 200, 0, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 88, 3, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 120, 3, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 173, 3, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 205, 3, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT*2, 230, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 136, 1, WA_LEFT },
        .bootWdtConf         = { 0, 205, 2, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_FRAMEWDT-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 32, 130, 4, 2, 10, 3 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 230, -1 },
        .weatherMove         = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        .weatherMoveVU       = { 89, 120, MAX_WIDTH-89+TFT_FRAMEWDT },
    },
    {   // [1] VaraiTamas (BoomBox)
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 7, 40 },
        .title1Conf          = {{ TFT_FRAMEWDT, 62, 2, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 7, 40 },
        .title2Conf          = {{ TFT_FRAMEWDT, 86, 2, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 7, 40 },
        .playlistConf        = {{ TFT_FRAMEWDT, 146, 3, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 7, 40 },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_CENTER }, 140, false, MAX_WIDTH, 0, 7, 40 },
        .apSettConf          = {{ TFT_FRAMEWDT, 320-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 7, 40 },
        .weatherConf         = {{ TFT_FRAMEWDT, 116, 2, WA_CENTER }, 140, false, MAX_WIDTH, 5000, 4, 40 },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT - TFT_FRAMEWDT - 8, 0, WA_LEFT }, MAX_WIDTH, 5, true },
        .playlBGConf         = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 243, 2, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT, 145, 2, WA_RIGHT },
        // ??? chtxtConf     = { 310 ,282, 2, WA_LEFT };
        .voltxtConf          = { 0, 282, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, 282, 2, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 282, 2, WA_RIGHT },
        .numConf             = { 0, 200, 70, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 88, 3, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 120, 3, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 173, 3, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 205, 3, WA_CENTER },
        .clockConf           = { 10, 212, 2, WA_RIGHT },
        .vuConf              = { 35, 258, 1, WA_CENTER },
        // ??? namedayConf   = { TFT_FRAMEWDT, 175, 2, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT, 226, 1, WA_LEFT };
        .bootWdtConf         = { 0, 216, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 10, 142, 2, WA_RIGHT }, 60 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 200, 7, 4, 2, 20, 9 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 10, 116, MAX_WIDTH },
        .weatherMoveVU       = { 10, 116, MAX_WIDTH },
        /* BOOMBOX STYLE: middle-out VU */
        .boomboxStyle        = true,
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\010 %s";
const char voltxtFmt[]            PROGMEM = "\023\025%d";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" };
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
