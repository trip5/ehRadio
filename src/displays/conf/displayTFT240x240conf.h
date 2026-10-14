/*************************************************************************************
    TFT240x240 displays configuration file.
*************************************************************************************/

#ifndef displayTFT240x240conf_h
#define displayTFT240x240conf_h

#define TFT_BORDER    8
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     68

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 3, WA_CENTER }, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 240-TFT_BORDER-16, 2, WA_LEFT }, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 38, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 182, 1, WA_CENTER },
        .apNameConf          = { TFT_BORDER, 66, 2, WA_CENTER },
        .apName2Conf         = { TFT_BORDER, 90, 2, WA_CENTER },
        .apPassConf          = { TFT_BORDER, 130, 2, WA_CENTER },
        .apPass2Conf         = { TFT_BORDER, 154, 2, WA_CENTER },
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
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 3, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME*5/4 },
        .title1Conf          = {{ TFT_BORDER, 50, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 70, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 112, 2, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 198, 1, WA_LEFT }, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, 240-TFT_BORDER-6, 0, 0 }, MAX_WIDTH, 6, true },
        .bufferbarConf       = {{ 0, 239, 0, 0 }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 38, false },
        .metaBGConfInv       = {{ 0, 38, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 107, 0, 0 }, DSP_WIDTH, 24, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { TFT_BORDER, 188, 1, WA_LEFT },
        .voltxtConf          = { 80, 214, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, 214, 1, WA_LEFT },
        .rssiConf            = { TFT_BORDER, 214-6, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 120+30, 3, WA_CENTER },
        .clockConf           = { 0, 168, 3, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 94, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 100, 20, 10, 2, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, 0 },
        .weatherMove         = { TFT_BORDER, 202, MAX_WIDTH },
        .weatherMoveVU       = { TFT_BORDER, 202, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d%%";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
