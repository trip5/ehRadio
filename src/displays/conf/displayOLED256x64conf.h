/*************************************************************************************
    OLED256x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED256x64conf_h
#define displayOLED256x64conf_h

#define TFT_BORDER    1
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     8

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER+1, TFT_BORDER+1, 1, WA_CENTER }, MAX_WIDTH-2, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 64-7, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        //.apTitleBGConf       = {{ 0, 18, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .apTitleBGConf       = { }, // unused
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 64-8, 1, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 10, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH-2, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 21, 1, WA_LEFT }, DSP_WIDTH/2-3, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 30, 1, WA_LEFT }, DSP_WIDTH/2-3, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-13, 1, WA_LEFT }, DSP_WIDTH/2-3, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1-1-1, 0, 0 }, DSP_WIDTH, 3, true },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 18, 0, 0 }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 18, false },
        .underLineConf       = {{ DSP_WIDTH/2, 18, 0, 0 }, 1, 43, false },
        // .overLineConf       = {{ 0, 39, 0, WA_LEFT }, DSP_WIDTH/2, 1, false },
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER+20, 64-12-10, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        // .voltxtConf          = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { 0, 64-13, 1, WA_LEFT },
        .rssiConf            = { 0, 64-12-10, 1, WA_LEFT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 57, 2, WA_CENTER },
        .clockConf           = { 0, 58, 2, WA_RIGHT },
        // .clockConf         = { 6, 34, 2, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        //.vuConf              = { }, // unused
        .vuConf              = { DSP_WIDTH/2+4, DSP_HEIGHT/2-7, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 11, 121, 7, 1, 25 },
        /* MOVES               { left, top, width } */
        .clockMove           = { },
        .weatherMove         = { TFT_BORDER, 64-13, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-13, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = true, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d%%";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
