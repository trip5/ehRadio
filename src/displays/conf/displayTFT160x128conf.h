/*************************************************************************************
    TFT160x128 displays configuration file.
*************************************************************************************/

#ifndef displayTFT160x128conf_h
#define displayTFT160x128conf_h

#define TFT_BORDER    4
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     16

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 2, WA_CENTER }, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 128-TFT_BORDER-8, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 22, false },
        /* WIDGETS             { offset, top, fontsize, align } */
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
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 26, 1, WA_LEFT }, MAX_WIDTH-24, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 36, 1, WA_LEFT }, MAX_WIDTH-24, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 56, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 42, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, 118, 0, 0 }, MAX_WIDTH, 5, true },
        .bufferbarConf       = {{ 0, 127, 0, 0 }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 22, false },
        .metaBGConfInv       = {{ 0, 22, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 52, 0, 0 }, DSP_WIDTH, 22, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { TFT_BORDER, 26, 1, WA_RIGHT },
        .bitrateConf         = { }, // unused
        // .bitrateConf       = { TFT_BORDER, 99, 1, WA_LEFT },
        .voltxtConf          = { TFT_BORDER, 108, 1, WA_LEFT },
        .batteryConf         = { TFT_BORDER, 108, 1, WA_RIGHT },
        .iptxtConf           = { TFT_BORDER, 108, 1, WA_CENTER },
        .rssiConf            = { TFT_BORDER, 108, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 86, 2, WA_CENTER },
        .clockConf           = { 0, 98, 2, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 54, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{DSP_WIDTH-TFT_BORDER-19, 23, 1, WA_LEFT}, 22 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 12, 50, 2, 1, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 14, 98, 0},
        .weatherMove         = {TFT_BORDER, 48, MAX_WIDTH},
        .weatherMoveVU       = { 34, 48, MAX_WIDTH-34+TFT_BORDER },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        /* batteryConf, iptxtConf and rssiConf are all on top 108, so RSSI and battery share a
           row and are drawn alternately rather than over each other. */
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d%%";
const char iptxtFmt[]             PROGMEM = "%s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
