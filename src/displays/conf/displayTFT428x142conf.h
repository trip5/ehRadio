/*************************************************************************************
    TFT428x142 displays configuration file.
*************************************************************************************/

#ifndef displayTFT428x142conf_h
#define displayTFT428x142conf_h

#define TFT_BORDER    4
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     28

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 3, WA_CENTER }, MAX_WIDTH, 0, 4, 20 },
        .apSettConf          = {{ TFT_BORDER, DSP_HEIGHT-18, 2, WA_LEFT }, MAX_WIDTH, 0, 4, 30 },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 29, false }, // The AP and SD-manager band.  Boot screens must not borrow a layout's
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-16, 2, WA_CENTER },
        .apNameConf          = { 0, 36, 2, WA_CENTER },
        .apName2Conf         = { 0, 56, 2, WA_CENTER },
        .apPassConf          = { 0, 83, 2, WA_CENTER },
        .apPass2Conf         = { 0, 103, 2, WA_CENTER },
        .bootWdtConf         = { 0, DSP_HEIGHT-30, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 10, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "Default Air",
    "Large VU",
    "BoomBox",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 3, WA_LEFT }, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_BORDER, 38, 2, WA_LEFT }, MAX_WIDTH-180, 5000, 4, 30 },
        .title2Conf          = {{ TFT_BORDER, 57, 2, WA_LEFT }, MAX_WIDTH-180, 5000, 4, 35 },
        .playlistConf        = {{ TFT_BORDER, 112, 2, WA_LEFT }, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_BORDER, DSP_HEIGHT-52, 2, WA_CENTER }, MAX_WIDTH, 0, 2, 16 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-3, 0, 0 }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_BORDER, DSP_HEIGHT-2, 0, 0 }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 31, false },
        .metaBGConfInv       = {{ 0, 31, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = {{TFT_BORDER, DSP_HEIGHT-62, WA_LEFT}, MAX_WIDTH-173, 1, false },
        .overLineConf        = {{MAX_WIDTH-169, 78, WA_LEFT}, 5, 5, false },
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER+372, DSP_HEIGHT-29, 1, WA_RIGHT },
        .voltxtConf          = { TFT_BORDER+372, DSP_HEIGHT-18, 1, WA_RIGHT },
        .batteryConf         = { }, // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-18, 1, WA_RIGHT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-36, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 100, 2, WA_CENTER },
        .clockConf           = { TFT_BORDER, 72, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER+58, DSP_HEIGHT-29, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 8, 258, 2, 2, 30 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER, 72, 0 },
        .weatherMove         = { TFT_BORDER, DSP_HEIGHT-52, 0 },
        .weatherMoveVU       = { TFT_BORDER, DSP_HEIGHT-52, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Default Air
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 3, WA_LEFT }, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_BORDER, 40, 2, WA_LEFT }, MAX_WIDTH-168, 5000, 4, 30 },
        .title2Conf          = {{ TFT_BORDER, 59, 2, WA_LEFT }, MAX_WIDTH-168, 5000, 4, 35 },
        .playlistConf        = {{ TFT_BORDER, 112, 2, WA_LEFT }, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_BORDER, DSP_HEIGHT-58, 2, WA_CENTER }, MAX_WIDTH, 0, 2, 16 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-3, 0, 0 }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_BORDER, DSP_HEIGHT-2, 0, 0 }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 31, false },
        .metaBGConfInv       = {{ 0, 31, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { },
        .overLineConf        = { },
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER+372, DSP_HEIGHT-30, 1, WA_RIGHT },
        .voltxtConf          = { TFT_BORDER+372, DSP_HEIGHT-19, 1, WA_RIGHT },
        .batteryConf         = { }, // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-19, 1, WA_RIGHT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-37, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 100, 2, WA_CENTER },
        .clockConf           = { TFT_BORDER, 74, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER+58, DSP_HEIGHT-30, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 8, 258, 2, 2, 30 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER, 74, 0 },
        .weatherMove         = { TFT_BORDER, DSP_HEIGHT-58, 0 },
        .weatherMoveVU       = { TFT_BORDER, DSP_HEIGHT-58, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Large VU
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 3, WA_LEFT }, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_BORDER, 40, 2, WA_LEFT }, MAX_WIDTH-168, 5000, 4, 30 },
        .title2Conf          = {{ TFT_BORDER, 59, 2, WA_LEFT }, MAX_WIDTH-168, 5000, 4, 35 },
        .playlistConf        = {{ TFT_BORDER, 112, 2, WA_LEFT }, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_BORDER, DSP_HEIGHT-52, 2, WA_CENTER }, MAX_WIDTH, 0, 2, 16 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-3, 0, 0 }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_BORDER, DSP_HEIGHT-2, 0, 0 }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 31, false },
        .metaBGConfInv       = {{ 0, 31, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { },
        .overLineConf        = { },
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { },
        .voltxtConf          = { TFT_BORDER/2, DSP_HEIGHT-17, 1, WA_CENTER },
        .batteryConf         = { }, // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-17, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-24, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 97, 2, WA_CENTER },
        .clockConf           = { TFT_BORDER, 74, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER+44, DSP_HEIGHT-60, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ TFT_BORDER, DSP_HEIGHT-60, 1, WA_RIGHT }, 34 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 16, MAX_WIDTH-44-1, 2, 2, 44 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER, 74, 0 },
        .weatherMove         = { TFT_BORDER+44, DSP_HEIGHT-52, MAX_WIDTH-44-1 },
        .weatherMoveVU       = { },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 3, WA_LEFT }, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_BORDER, 40, 2, WA_LEFT }, MAX_WIDTH-180, 5000, 4, 30 },
        .title2Conf          = {{ TFT_BORDER, 59, 2, WA_LEFT }, MAX_WIDTH-180, 5000, 4, 35 },
        .playlistConf        = {{ TFT_BORDER, 112, 2, WA_LEFT }, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_BORDER, DSP_HEIGHT-44, 2, WA_CENTER }, MAX_WIDTH, 0, 2, 16 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-3, 0, 0 }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_BORDER, DSP_HEIGHT-2, 0, 0 }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 31, false },
        .metaBGConfInv       = {{ 0, 31, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = {{TFT_BORDER, DSP_HEIGHT-62, WA_LEFT}, MAX_WIDTH, 1, false },
        .overLineConf        = {{MAX_WIDTH-162, 31, WA_LEFT}, 1, 50, false },
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER+372, DSP_HEIGHT-58, 1, WA_RIGHT },
        .voltxtConf          = { TFT_BORDER/2, DSP_HEIGHT-18, 1, WA_CENTER },
        .batteryConf         = { }, // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-18, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-25, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_BORDER, 96, 2, WA_CENTER },
        .clockConf           = { TFT_BORDER, 74, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, DSP_HEIGHT-46, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_BORDER*2, 23, 6, 2, 24 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER, 74, 0 },
        .weatherMove         = { TFT_BORDER, DSP_HEIGHT-44, 0 },
        .weatherMoveVU       = { },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]              PROGMEM = "%d";
const char rssiFmt[]                PROGMEM = "WiFi %d%%";
const char iptxtFmt[]               PROGMEM = "\021\022%s";
const char voltxtFmt[]              PROGMEM = "%d%%";
const char bitrateFmt[]             PROGMEM = "%d kBs";

// Automatically added by importlayout.py:
const char batterytxtFmt[]          PROGMEM = "%d%%";

#endif
