/*************************************************************************************
    TFT480x320 displays configuration file.
*************************************************************************************/

#ifndef displayTFT480x320conf_h
#define displayTFT480x320conf_h

#define TFT_FRAMEWDT    10
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

#if defined(BIG_BOOT_LOGO) || BIG_BOOT_LOGO // true
  #define BOOTLOGOTOP   60
#else
  #define BOOTLOGOTOP   110
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_CENTER }, 140, false, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 320-TFT_FRAMEWDT-16, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 243, 2, WA_CENTER },
        .apNameConf          = { TFT_FRAMEWDT, 88, 3, WA_CENTER },
        .apName2Conf         = { TFT_FRAMEWDT, 120, 3, WA_CENTER },
        .apPassConf          = { TFT_FRAMEWDT, 173, 3, WA_CENTER },
        .apPass2Conf         = { TFT_FRAMEWDT, 205, 3, WA_CENTER },
        .bootWdtConf         = { 0, 205, 2, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "Default (VU Rotated)",
    "BoomBox (VaraiTamas)",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_FRAMEWDT, 62, 2, WA_LEFT }, 140, true, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 86, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 146, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 120, 2, WA_LEFT }, 140, true, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-8, 0, WA_LEFT }, MAX_WIDTH, 8, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { 6, 62, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, DSP_HEIGHT-38, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-38, 2, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, DSP_HEIGHT-38-6, 3, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT*2, 230, 4, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { TFT_FRAMEWDT, 136, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_FRAMEWDT-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 32, 130, 4, 2, 10 },
        /* MOVES               { left, top, width */
        .clockMove           = { TFT_FRAMEWDT*2, 230, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        .weatherMoveVU       = { 89, 120, MAX_WIDTH-89+TFT_FRAMEWDT },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Default (VU Rotated)
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_FRAMEWDT, 62, 2, WA_LEFT }, 140, true, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 86, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 146, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 120, 2, WA_LEFT }, 140, true, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-8, 0, WA_LEFT }, MAX_WIDTH, 8, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { 6, 62, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, DSP_HEIGHT-38, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-38, 2, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, DSP_HEIGHT-38-6, 3, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT*2, 230, 4, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { TFT_FRAMEWDT, 161, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_FRAMEWDT-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 25, 130, 17, 3, 10 },
        /* MOVES               { left, top, width */
        .clockMove           = { TFT_FRAMEWDT*2, 230, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = true, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox (VaraiTamas)
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 4, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_FRAMEWDT, 62, 2, WA_LEFT }, 140, true, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 86, 2, WA_LEFT }, 140, true, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 146, 3, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 120, 2, WA_CENTER }, 140, true, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT - TFT_FRAMEWDT - 8, 0, WA_LEFT }, MAX_WIDTH, 5, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, WA_LEFT }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, WA_LEFT }, DSP_WIDTH, 36, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { TFT_FRAMEWDT, 145, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { 310 ,282, 2, WA_LEFT };
        .voltxtConf          = { 0, 282, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_FRAMEWDT, 282, 2, WA_LEFT },
        .rssiConf            = { TFT_FRAMEWDT, 282, 2, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 10, 215, 4, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { 35, 259, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        // ??? namedayConf   = { TFT_FRAMEWDT, 175, 2, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT, 226, 1, WA_LEFT };
        .fullbitrateConf     = {{ 10, 148, 2, WA_RIGHT }, 60 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 200, 7, 4, 2, 20 },
        /* MOVES               { left, top, width */
        .clockMove           = { 10, 215, 0 },
        .weatherMove         = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, 120, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .boomboxVU           = true, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
