/*************************************************************************************
    TFT284x76 displays configuration file.
*************************************************************************************/

#ifndef displayTFT284x76conf_h
#define displayTFT284x76conf_h

#define TFT_BORDER    2
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     8

// Trip5 Note: This conf file was imported but remains un-implemented and un-tested...
// check displayOLED128x64 if needing to share IP & RSSI with Weather & Battery

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER+1, TFT_BORDER+1, 1, WA_CENTER }, MAX_WIDTH-2, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 64-7, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0,  0, 0 }, DSP_WIDTH, 19, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-10, 1, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, DSP_HEIGHT-8*2-5, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 10, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "krzxsiek",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER+1, TFT_BORDER+1, 2, WA_LEFT }, MAX_WIDTH-2, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 21, 1, WA_LEFT }, DSP_WIDTH/2+18, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 30, 1, WA_LEFT }, DSP_WIDTH/2+18, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-12, 1, WA_LEFT }, DSP_WIDTH/2+18, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-4, 0, 0 }, DSP_WIDTH-TFT_BORDER*2, 3, true },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, 0 }, DSP_WIDTH, 19, false },
        .metaBGConfInv       = {{ 0, 19, 0, 0 }, DSP_WIDTH, 1,  false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER+20, 64-11-10, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        // .voltxtConf          = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, 64-12, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER, 64-11-10, 1, WA_LEFT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 57, 0, WA_CENTER },
        .clockConf           = { 0, 57, 0, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 2, DSP_HEIGHT-14, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_BORDER*2-2, 7, TFT_BORDER*2+4, 1, 17 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 57, 0 },
        .weatherMove         = { TFT_BORDER, 64-12, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-12, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // krzxsiek
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER+1, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH-2, 5000, 2, 25 },
        .title1Conf          = {{ TFT_BORDER+1, 21, 1, WA_LEFT }, DSP_WIDTH/2+18, 5000, 2, 25 },
        .title2Conf          = {{ TFT_BORDER+1, 32, 1, WA_LEFT }, DSP_WIDTH/2+18, 5000, 2, 25 },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, 500, 2, 25 },
        .weatherConf         = {{ TFT_BORDER+1, DSP_HEIGHT-12, 1, WA_CENTER }, MAX_WIDTH-2, 0, 2, 30 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-7, 0, 0 }, MAX_WIDTH, 5, true },
        .bufferbarConf       = {{ TFT_BORDER, DSP_HEIGHT-1, 0, 0 }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, 0 }, DSP_WIDTH, 18, false },
        .metaBGConfInv       = {{ 0, 19, 0, 0 }, DSP_WIDTH, 1,  false },
        .underLineConf       = { },
        .overLineConf        = { },
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { TFT_BORDER+21, 63-10-10, 1, WA_LEFT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { TFT_BORDER+70, 63-10-10, 1, WA_LEFT };
        .voltxtConf          = { TFT_BORDER+110, 63-10-10, 1, WA_LEFT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, 64-11, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER+1, 63-10-10, 1, WA_LEFT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 59, 0, WA_CENTER },
        .clockConf           = { 0, 57, 0, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        // ??? namedayConf   = { TFT_BORDER, 64-11, 1, WA_LEFT };
        // ??? dateConf      = { TFT_BORDER *2, 226, 1, WA_LEFT };
        .vuConf              = { 2, DSP_HEIGHT-13, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 8, 64-10-10, 1, WA_LEFT }, 41 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_BORDER*2-2, 2,             TFT_BORDER*2+4, 1, 17 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 57, 0 },
        .weatherMove         = { TFT_BORDER+1, DSP_HEIGHT-12, 0 },
        .weatherMoveVU       = { TFT_BORDER+1, DSP_HEIGHT-12, 0 },
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
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
