/*************************************************************************************
    TFT128x128 displays configuration file.
*************************************************************************************/

#ifndef displayTFT128x128conf_h
#define displayTFT128x128conf_h

#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     16

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 128-TFT_FRAMEWDT-8, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 110, 1, WA_CENTER },
        .apNameConf          = { 0, 40, 1, WA_CENTER },
        .apName2Conf         = { 0, 54, 1, WA_CENTER },
        .apPassConf          = { 0, 74, 1, WA_CENTER },
        .apPass2Conf         = { 0, 88, 1, WA_CENTER },
        .bootWdtConf         = { 0, 90, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
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
        .title1Conf          = {{ TFT_FRAMEWDT, 26, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 36, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*3-4, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 56, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 42, 1, WA_LEFT }, 140, true, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, 118, 0, WA_LEFT }, MAX_WIDTH-6*3-4, 5, true },
        .bufferbarConf       = {{ 0, 127, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        .metaBGConfInv       = {{ 0, 22, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 52, 0, WA_LEFT }, DSP_WIDTH, 22, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { TFT_FRAMEWDT, 36, 1, WA_RIGHT },
        .voltxtConf          = { TFT_FRAMEWDT, 128-10, 1, WA_RIGHT },
        // .voltxtConf        = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 108, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 108, 1, WA_RIGHT },
        .numConf             = { 0, 86, 0, WA_CENTER },
        .clockConf           = { 0, 94, 0, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, 99, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 56, 7, 2, 1, 8 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 94, -1 },
        .weatherMove         = { TFT_FRAMEWDT, 48, 122 },
        .weatherMoveVU       = { TFT_FRAMEWDT, 48, 122 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share one row (was the IP_WEATHER_SHARED macro)
        .shareBattRSSI       = false, // RSSI and battery share one row (was the RSSI_BATT_SHARED macro)
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
