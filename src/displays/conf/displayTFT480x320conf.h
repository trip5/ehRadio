/*************************************************************************************
    TFT480x320 displays configuration file.
*************************************************************************************/

#ifndef displayTFT480x320conf_h
#define displayTFT480x320conf_h

#define TFT_BORDER    10
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2

#if defined(BIG_BOOT_LOGO) || BIG_BOOT_LOGO // true
  #define BOOTLOGOTOP   60
#else
  #define BOOTLOGOTOP   110
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 4, WA_CENTER }, MAX_WIDTH, 0, 3, SCROLLTIME },
        .apSettConf          = {{ TFT_BORDER, 320-TFT_BORDER-16, 2, WA_LEFT }, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, 0 }, DSP_WIDTH, 50, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bootstrConf         = { 0, 243, 2, WA_CENTER },
        .apNameConf          = { TFT_BORDER, 88, 3, WA_CENTER },
        .apName2Conf         = { TFT_BORDER, 120, 3, WA_CENTER },
        .apPassConf          = { TFT_BORDER, 173, 3, WA_CENTER },
        .apPass2Conf         = { TFT_BORDER, 205, 3, WA_CENTER },
        .bootWdtConf         = { 0, 205, 2, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
    "Default (VU Rotated)",
    "BoomBox (VaraiTamas)",
    "Big Text",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 4, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_BORDER, 62, 2, WA_LEFT }, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 86, 2, WA_LEFT }, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 146, 3, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 120, 2, WA_LEFT }, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-8, 0, 0 }, MAX_WIDTH, 8, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, 0 }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, 0 }, DSP_WIDTH, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { 6, 62, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, DSP_HEIGHT-38, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-38, 2, WA_LEFT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-38-6, 3, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { TFT_BORDER*2, 230, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 136, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_BORDER-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 32, 130, 4, 2, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER*2, 230, 0 },
        .weatherMove         = { TFT_BORDER, 120, MAX_WIDTH },
        .weatherMoveVU       = { 89, 120, MAX_WIDTH-89+TFT_BORDER },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Default (VU Rotated)
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 4, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_BORDER, 62, 2, WA_LEFT }, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 86, 2, WA_LEFT }, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 146, 3, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 120, 2, WA_LEFT }, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT-TFT_BORDER-8, 0, 0 }, MAX_WIDTH, 8, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, 0 }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, 0 }, DSP_WIDTH, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { 6, 62, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, DSP_HEIGHT-38, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_BORDER, DSP_HEIGHT-38, 2, WA_LEFT },
        .rssiConf            = { TFT_BORDER, DSP_HEIGHT-38-6, 3, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { TFT_BORDER*2, 230, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { TFT_BORDER, 161, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_BORDER-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 25, 130, 17, 3, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER*2, 230, 0 },
        .weatherMove         = { TFT_BORDER, 120, MAX_WIDTH },
        .weatherMoveVU       = { TFT_BORDER, 120, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox (VaraiTamas)
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 4, WA_LEFT }, MAX_WIDTH, SCROLLDELAY, 3, SCROLLTIME },
        .title1Conf          = {{ TFT_BORDER, 62, 2, WA_LEFT }, MAX_WIDTH-(44==0?6*2*7-6:44), SCROLLDELAY, 2, SCROLLTIME },
        .title2Conf          = {{ TFT_BORDER, 86, 2, WA_LEFT }, MAX_WIDTH-44, SCROLLDELAY, 2, SCROLLTIME },
        .playlistConf        = {{ TFT_BORDER, 146, 3, WA_LEFT }, MAX_WIDTH, SCROLLDELAY/5, 3, SCROLLTIME },
        .weatherConf         = {{ TFT_BORDER, 120, 2, WA_CENTER }, MAX_WIDTH, 0, 2, SCROLLTIME },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ TFT_BORDER, DSP_HEIGHT - TFT_BORDER - 8, 0, 0 }, MAX_WIDTH, 5, true },
        .bufferbarConf       = {{ 0, DSP_HEIGHT-2, 0, 0 }, DSP_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, DSP_WIDTH, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, DSP_WIDTH, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, 0 }, DSP_WIDTH, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        //.bitrateConf         = { TFT_BORDER, 145, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { 310 ,282, 2, WA_LEFT };
        .voltxtConf          = { 0, 282, 2, WA_CENTER },
        .batteryConf         = { (DSP_WIDTH*2)/3+2, DSP_HEIGHT-38, 2, WA_LEFT },
        .iptxtConf           = { TFT_BORDER, 282, 2, WA_LEFT },
        .rssiConf            = { TFT_BORDER, 282, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 10, 215, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 35, 259, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        // ??? namedayConf   = { TFT_BORDER, 175, 2, WA_LEFT };
        // ??? dateConf      = { TFT_BORDER, 226, 1, WA_LEFT };
        .fullbitrateConf     = {{ 10, 148, 2, WA_RIGHT }, 60 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 200, 7, 4, 2, 20 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 10, 215, 0 },
        .weatherMove         = { TFT_BORDER, 120, MAX_WIDTH },
        .weatherMoveVU       = { TFT_BORDER, 120, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Big Text
        /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ 10, 10, 4, WA_LEFT }, 460, 5000, 3, 20 },
        .title1Conf          = {{ 10, 58, 3, WA_LEFT }, 390, 5000, 2, 20 },
        .title2Conf          = {{ 10, 95, 3, WA_LEFT }, 390, 5000, 2, 20 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460, 1000, 3, 20 },
        .weatherConf         = {{ 10, 100, 3, WA_LEFT }, 460, 0, 2, 20 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 294, 0, 0 }, 460, 14, true },
        .bufferbarConf       = {{ 0, 315, 0, 0 }, 480, 5, true },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 48, false },
        .metaBGConfInv       = {{ 0, 42, 0, 0 }, 480, 2, true },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 10, 264, 3, WA_LEFT },
        .batteryConf         = { 115, 270, 2, WA_LEFT },
        .iptxtConf           = { 80, 270, 2, WA_RIGHT },
        .rssiConf            = { 10, 256, 4, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 10, 218, 4, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 10, 180, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 410, 55, 3, WA_LEFT }, 60 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 220, 60, 20, 5, 10 },
        /* MOVES               { left, top, width } */
        .clockMove           = { },
        .weatherMove         = { },
        .weatherMoveVU       = { 10, 135, 460 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
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
