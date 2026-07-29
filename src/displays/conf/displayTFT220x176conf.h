/*************************************************************************************
    TFT220x176 displays configuration file.
*************************************************************************************/

#ifndef displayTFT220x176conf_h
#define displayTFT220x176conf_h

#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     28

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, DSP_WIDTH+10, 0, 2, SCROLLTIME },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 150, 1, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 38, 2, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 62, 2, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 102, 2, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 126, 2, WA_CENTER },
        .bootWdtConf         = { 0, 130, 1, WA_CENTER },
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, DSP_WIDTH+10, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 28, 1, WA_LEFT }, 140, true, MAX_WIDTH-28, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 40, 1, WA_LEFT }, 140, true, MAX_WIDTH-28, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 80, 2, WA_LEFT }, 140, true, DSP_WIDTH+10, SCROLLDELAY/5, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 56, 2, WA_LEFT }, 140, true, DSP_WIDTH+10, 0, 2, SCROLLTIME },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        .metaBGConfInv       = {{ 0, 22, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-4, 0, WA_LEFT }, MAX_WIDTH, 4, true },
        .playlBGConf         = {{ 0, 76, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { TFT_FRAMEWDT+6, DSP_HEIGHT-TFT_FRAMEWDT-14-14, 1, WA_RIGHT },
        .voltxtConf          = { TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-14, 1, WA_LEFT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-14, 1, WA_CENTER },
        .rssiConf            = { TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-14, 1, WA_RIGHT },
        .numConf             = { 0, 110, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT*3, 130, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 58, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */
        .fullbitrateConf     = {{DSP_WIDTH-TFT_FRAMEWDT-21, 25, 1, WA_LEFT}, 22 },
        /* BANDS               { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 19, 90, 2, 2, 10, 2 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { TFT_FRAMEWDT*2, 130, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 64, DSP_WIDTH+10 },
        .weatherMoveVU       = { TFT_FRAMEWDT+46, 58, DSP_WIDTH+10-46 },
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
