/*************************************************************************************
    TFT160x80 displays configuration file.
*************************************************************************************/

#ifndef displayTFT160x80conf_h
#define displayTFT160x80conf_h

#define TFT_BORDER    1
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     5

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 2, WA_CENTER }, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 80-TFT_BORDER-8, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 16, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 65, 1, WA_CENTER },
        .apNameConf          = { 0, 20, 1, WA_CENTER },
        .apName2Conf         = { 0, 32, 1, WA_CENTER },
        .apPassConf          = { 0, 46, 1, WA_CENTER },
        .apPass2Conf         = { 0, 58, 1, WA_CENTER },
        .bootWdtConf         = { 0, 50, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 19, 1, WA_LEFT }, MAX_WIDTH-6*3-4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = { }, // unused
        // .title2Conf        = {{ TFT_BORDER, 36, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 33, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 80-13, 1, WA_LEFT }, MAX_WIDTH-6*3, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, 80-1-1-2, 0, 0 }, MAX_WIDTH, 2, false },
        .bufferbarConf       = {{ 0, 79, 0, 0 }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 16, false },
        .metaBGConfInv       = {{ 0, 16, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 30, 0, 0 }, DSP_WIDTH, 20, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        // .voltxtConf        = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { }, // unused
        // .iptxtConf         = { TFT_BORDER, 108, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER, 80-13, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 29+32, 0, WA_CENTER },
        .clockConf           = { 20, 29+34, 0, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 1, 28, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 12, 48, 2, 1, 8 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 6, 29+34, 0},
        .weatherMove         = { TFT_BORDER, 80-13, MAX_WIDTH-6*3-30 },
        .weatherMoveVU       = { 30, 80-13, MAX_WIDTH-6*3-30 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d%%";
const char iptxtFmt[]             PROGMEM = "";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
