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
        .apName2Conf         = { 0, 28, 1, WA_CENTER },
        .apPassConf          = { 0, 39, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 10, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "BoomBox",
    "Fatty",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 2, 2, 1, WA_LEFT }, 252, 5000, 2, 35 },
        .title1Conf          = {{ 2, 16, 1, WA_LEFT }, 144, 5000, 1, 20 },
        .title2Conf          = {{ 2, 26, 1, WA_LEFT }, 144, 5000, 1, 20 },
        .playlistConf        = {{ 1, 30, 1, WA_LEFT }, 254, 1000, 1, 20 },
        .weatherConf         = {{ 2, 52, 1, WA_LEFT }, 144, 0, 1, 20 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 61, 0, 0 }, 256, 3, true },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, 0 }, 256, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, 256, 12, false },
        .underLineConf       = {{ 154, 12, 0, 0 }, 1, 49, false },
        .overLineConf        = {{ 0, 37, 0, 0 }, 154, 1, false },
        .playlBGConf         = {{ 0, 26, 0, 0 }, 256, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 46, 42, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { }, // unused
        .iptxtConf           = { 4, 52, 1, WA_RIGHT },
        .rssiConf            = { 4, 42, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 55, 2, WA_CENTER },
        .clockConf           = { 162, 30, 1, WA_LEFT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 2, 41, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 8, 144, 1, 1, 30 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 162, 30, 0 },
        .weatherMove         = { 1, 52, 0 },
        .weatherMoveVU       = { },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 2, 2, 1, WA_LEFT }, 252, 5000, 2, 35 },
        .title1Conf          = {{ 2, 15, 1, WA_LEFT }, 192, 5000, 1, 20 },
        .title2Conf          = {{ 2, 26, 1, WA_LEFT }, 192, 5000, 1, 20 },
        .playlistConf        = {{ 1, 30, 1, WA_LEFT }, 254, 1000, 1, 20 },
        .weatherConf         = {{ 2, 50, 1, WA_LEFT }, 253, 0, 1, 20 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 61, 0, 0 }, 256, 3, true },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, 0 }, 256, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, 256, 12, false },
        .underLineConf       = { }, // unused
        .overLineConf        = {{ 0, 36, 0, 0 }, 218, 1, false },
        .playlBGConf         = {{ 0, 26, 0, 0 }, 256, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 2, 18, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { }, // unused
        .iptxtConf           = { 2, 40, 1, WA_LEFT },
        .rssiConf            = { 2, 40, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 55, 2, WA_CENTER },
        .clockConf           = { 2, 30, 0, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 2, 50, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 125, 10, 2, 1, 24 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 2, 30, 0 },
        .weatherMove         = { 1, 50, 0 },
        .weatherMoveVU       = { },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Fatty
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 1, 1, 2, WA_LEFT }, 252, 5000, 2, 35 },
        .title1Conf          = {{ 1, 21, 1, WA_LEFT }, 120, 5000, 1, 20 },
        .title2Conf          = {{ 1, 30, 1, WA_LEFT }, 120, 5000, 1, 20 },
        .playlistConf        = {{ 1, 30, 1, WA_LEFT }, 254, 1000, 1, 20 },
        .weatherConf         = {{ 1, 51, 1, WA_LEFT }, 120, 0, 1, 20 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 61, 0, 0 }, 256, 3, true },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 18, 0, 0 }, 256, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, 256, 18, false },
        .underLineConf       = {{ 128, 18, 0, 0 }, 1, 43, false },
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, 256, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 21, 42, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { }, // unused
        .iptxtConf           = { 0, 51, 1, WA_LEFT },
        .rssiConf            = { 0, 42, 1, WA_LEFT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 1, 57, 2, WA_CENTER },
        .clockConf           = { 0, 58, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 132, 25, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 13, 124, 4, 1, 24 },
        /* MOVES               { left, top, width } */
        .clockMove           = { },
        .weatherMove         = { 1, 51, 0 },
        .weatherMoveVU       = { 1, 51, 0 },
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
