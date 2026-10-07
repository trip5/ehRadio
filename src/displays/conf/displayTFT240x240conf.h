/*************************************************************************************
    TFT240x240 displays configuration file.
*************************************************************************************/

#ifndef displayTFT240x240conf_h
#define displayTFT240x240conf_h

#define TFT_FRAMEWDT    8
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     68

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 182, 1, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 66, 2, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 90, 2, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 130, 2, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 154, 2, WA_CENTER },
        .bootWdtConf         = { 0, 162, 1, WA_CENTER },
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
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME*5/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 50, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 70, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 198, 1, WA_LEFT }, 140, true, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, 240-TFT_FRAMEWDT-6, 0, WA_LEFT }, MAX_WIDTH, 6, true },
        .bufferbarConf       = {{ 0, 239, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 38, false },
        .metaBGConfInv       = {{ 0, 38, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { TFT_FRAMEWDT, 188, 1, WA_LEFT },
        .voltxtConf          = { 80, 214, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 214, 1, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 214-6, 2, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 120+30, 3, WA_CENTER },
        .clockConf           = { 0, 168, 3, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { TFT_FRAMEWDT, 94, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 100, 20, 10, 2, 10 },
        /* MOVES               { left, top, width */
        .clockMove           = { 0, 176, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 202, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, 202, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
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
