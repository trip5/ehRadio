/*************************************************************************************
    OLED128x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED128x64conf_h
#define displayOLED128x64conf_h

#define TFT_BORDER    1
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     8

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 2, WA_CENTER }, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 64-7, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = { }, // unused
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 64-8, 1, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "Big VU",
    "Compact",
    "Minimal",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 19, 1, WA_LEFT }, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 28, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-9, 1, WA_LEFT }, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, 0 }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 17, 0, 0 }, DSP_WIDTH, 1,  false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 17, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 0, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-9, 1, WA_RIGHT },
        .iptxtConf           = { TFT_BORDER, 64-9, 1, WA_LEFT },
        .rssiConf            = { 0, 64-9, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 44, 1, WA_CENTER },
        //.clockConf           = { TFT_BORDER, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_BORDER, 52, 1, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        //.vuConf              = { }, // unused
        .vuConf              = { TFT_BORDER, 37, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 7, 54-1, 1, 1, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER+(54/2)+1, 52, 0 },
        .weatherMove         = { TFT_BORDER, 64-9, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-9, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = true, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Big VU
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 19, 1, WA_LEFT }, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 28, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-9, 1, WA_LEFT }, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, 0 }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 17, 0, 0 }, DSP_WIDTH, 1,  false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 17, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 0, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-9, 1, WA_RIGHT },
        .iptxtConf           = { TFT_BORDER, 64-9, 1, WA_LEFT },
        .rssiConf            = { 0, 64-9, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 44, 1, WA_CENTER },
        //.clockConf           = { TFT_BORDER, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_BORDER, 52, 1, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 37, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 8, MAX_WIDTH-(TFT_BORDER*2), 1, 1, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { }, // clock disappears when VU is on
        .weatherMove         = { TFT_BORDER, 64-9, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-9, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = true, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Compact
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER+1, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 15, 1, WA_LEFT }, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 24, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-10, 1, WA_LEFT }, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, 0 }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, 0 }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 12, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 0, 15, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-10, 1, WA_RIGHT },
        .iptxtConf           = { TFT_BORDER, 64-10, 1, WA_LEFT },
        .rssiConf            = { 0, 64-10, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 41, 1, WA_CENTER },
        //.clockConf           = { TFT_BORDER, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_BORDER, 50, 1, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        //.vuConf              = { }, // unused
        .vuConf              = { TFT_BORDER, 35, 0, WA_RIGHT }, // rotated: ch = width*2+space = 15px tall, cw = height = 44px, the line beside the clock
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 7, 54-1, 1, 1, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER+(54/2)+1, 50, 0 },
        .weatherMove         = { TFT_BORDER, 64-10, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-10, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = true, // IP and weather share the same row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Minimal
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER+1, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 15, 1, WA_LEFT }, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 24, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 30, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 64-10, 1, WA_LEFT }, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, 0 }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, 0 }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 12, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, 0 }, DSP_WIDTH, 12, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 0, 15, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-10, 1, WA_RIGHT },
        .iptxtConf           = { TFT_BORDER, 64-10, 1, WA_LEFT },
        .rssiConf            = { 0, 64-10, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 41, 1, WA_CENTER },
        //.clockConf           = { TFT_BORDER, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_BORDER, 50, 1, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 37, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_BORDER*2, 11, 2, 1, 16 },
        /* MOVES               { left, top, width } */
        .clockMove           = { }, // clock disappears when VU is on
        .weatherMove         = { TFT_BORDER, 64-10, 0 },
        .weatherMoveVU       = { TFT_BORDER, 64-10, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
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
const char batterytxtFmt[]        PROGMEM = "";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
