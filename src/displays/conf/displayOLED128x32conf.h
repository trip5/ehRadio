/*************************************************************************************
    OLED128x32 displays configuration file.
*************************************************************************************/

#ifndef displayOLED128x32conf_h
#define displayOLED128x32conf_h

#define TFT_BORDER    1
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     68

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, 1, 1, WA_CENTER }, MAX_WIDTH, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 32-7, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = { }, // unused
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 32-8, 1, WA_CENTER },
        .apNameConf          = { 0, 9, 1, WA_LEFT },
        .apName2Conf         = { 0, 9, 1, WA_RIGHT },
        .apPassConf          = { 0, 17, 1, WA_LEFT },
        .apPass2Conf         = { 0, 17, 1, WA_RIGHT },
        .bootWdtConf         = { 0, 32-8*2-5, 1, WA_CENTER },
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
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 1, WA_LEFT }, MAX_WIDTH-6*5-2, SCROLLDELAY, 1, SCROLLTIME },
        .title1Conf          = {{ 0, 11, 1, WA_LEFT }, DSP_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = { }, // unused - this panel shows one title line; it was tried at {{ 0, 9, 0, WA_LEFT }, DSP_WIDTH-6*5-3, 1, false }
        // .title2Conf        = {{ 0, 26, 1, WA_LEFT }, DSP_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 14, 1, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ 0, 20, 1, WA_LEFT }, DSP_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 32-1-1-1, 0, 0 }, DSP_WIDTH, 3, true },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf     = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 9, 0, 0 }, DSP_WIDTH/*-6*5-3*/, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, 0 }, DSP_WIDTH/*-6*5-3*/, 9, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 13, 0, 0 }, DSP_WIDTH, 9, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { 0, 11, 1, WA_RIGHT },
        .voltxtConf          = { 0, 20, 1, WA_RIGHT },
        .batteryConf         = { }, // unused
        .iptxtConf           = { }, // unused
        // .iptxtConf           = { 0, 64-11, 1, WA_LEFT },
        .rssiConf            = { 0, 64-11, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 12, 0, WA_CENTER },
        .clockConf           = { 0, 1, 0, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { }, // unused
        // .vuConf            = { 1, 28, 1, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 12, 48, 2, 1, 8 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 1, 0 },
        .weatherMove         = { 0, 20, 0 },
        .weatherMoveVU       = { 0, 20, 0 },
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
const char iptxtFmt[]             PROGMEM = "%s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
