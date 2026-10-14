/*************************************************************************************
    TFT220x176 displays configuration file.
*************************************************************************************/

#ifndef displayTFT220x176conf_h
#define displayTFT220x176conf_h

#define TFT_BORDER    4
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2
#define BOOTLOGOTOP     28

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 2, WA_CENTER }, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-16, 2, WA_LEFT }, DSP_WIDTH+10, 0, 2, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 22, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 150, 1, WA_CENTER },
        .apNameConf          = { TFT_BORDER, 38, 2, WA_CENTER },
        .apName2Conf         = { TFT_BORDER, 62, 2, WA_CENTER },
        .apPassConf          = { TFT_BORDER, 102, 2, WA_CENTER },
        .apPass2Conf         = { TFT_BORDER, 126, 2, WA_CENTER },
        .bootWdtConf         = { 0, 130, 1, WA_CENTER },
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
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 2, WA_LEFT }, DSP_WIDTH+10, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_BORDER, 28, 1, WA_LEFT }, MAX_WIDTH-28, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 40, 1, WA_LEFT }, MAX_WIDTH-28, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 80, 2, WA_LEFT }, DSP_WIDTH+10, SCROLLDELAY/5, 2, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 56, 2, WA_LEFT }, DSP_WIDTH+10, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-4, 0, 0 }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-1, 0, 0 }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 22, false },
        .metaBGConfInv       = {{ 0, 22, 0, 0 }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 76, 0, 0 }, DSP_WIDTH, 22, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { TFT_BORDER+6, DSP_HEIGHT-TFT_BORDER-14-14, 1, WA_RIGHT },
        .bitrateConf         = { }, // unused
        .voltxtConf          = { TFT_BORDER, DSP_HEIGHT-TFT_BORDER-14, 1, WA_LEFT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-TFT_BORDER-14, 1, WA_CENTER },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-TFT_BORDER-14, 1, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 110, 2, WA_CENTER },
        .clockConf           = { TFT_BORDER*3, 130, 2, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 58, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{DSP_WIDTH-TFT_BORDER-21, 25, 1, WA_LEFT}, 22 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 19, 90, 2, 2, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER*2, 130, 0 },
        .weatherMove         = { TFT_BORDER, 64, DSP_WIDTH+10 },
        .weatherMoveVU       = { TFT_BORDER+46, 58, DSP_WIDTH+10-46 },
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
