/*************************************************************************************
    OLED128x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED128x64conf_h
#define displayOLED128x64conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     8

// THIS DEFINITELY NEEDS TO BE REMOVED IF WE ALTER HOW CLOCK LOCATION IS DETERMINED

#if CLOCKFONT == YO_MONO
  #define FONTSHIFT_X -3
  #define FONTSHIFT_Y 1
#else
  #define FONTSHIFT_X 0
  #define FONTSHIFT_Y 15
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .apTitleBGConf       = { }, // unused
        /* WIDGETS             { left, top, fontsize, align } */
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
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 19, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 28, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-9, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 17, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 17, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { 0, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-9, 1, WA_RIGHT },
        .iptxtConf           = { TFT_FRAMEWDT, 64-9, 1, WA_LEFT },
        .rssiConf            = { 0, 64-9, 1, WA_RIGHT },
        .numConf             = { 0, 29+FONTSHIFT_Y, 0, WA_CENTER },
        //.clockConf           = { TFT_FRAMEWDT, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 37+FONTSHIFT_Y, 0, WA_CENTER },
        //.vuConf              = { }, // unused
        .vuConf              = { TFT_FRAMEWDT, 37, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 7, 54+FONTSHIFT_X*2-1, 1, 1, 10 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { TFT_FRAMEWDT+(54/2)+1+FONTSHIFT_X, 37+FONTSHIFT_Y, 0 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
        .rotateVU            = true,
        .shareWeatherIP      = true,
        .shareBattRSSI       = true,
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
    {   // Big VU
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 19, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 28, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-9, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 17, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 17, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { 0, 19, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-9, 1, WA_RIGHT },
        .iptxtConf           = { TFT_FRAMEWDT, 64-9, 1, WA_LEFT },
        .rssiConf            = { 0, 64-9, 1, WA_RIGHT },
        .numConf             = { 0, 29+FONTSHIFT_Y, 0, WA_CENTER },
        //.clockConf           = { TFT_FRAMEWDT, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 37+FONTSHIFT_Y, 0, WA_CENTER },
        .vuConf              = { TFT_FRAMEWDT, 37, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 8, MAX_WIDTH-(TFT_FRAMEWDT*2), 1, 1, 10 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { }, // clock disappears when VU is on
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
        .rotateVU            = true,
        .shareWeatherIP      = true,
        .shareBattRSSI       = true,
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
    {   // Compact
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+1, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 15, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 24, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-10, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { 0, 15, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-10, 1, WA_RIGHT },
        .iptxtConf           = { TFT_FRAMEWDT, 64-10, 1, WA_LEFT },
        .rssiConf            = { 0, 64-10, 1, WA_RIGHT },
        .numConf             = { 0, 26+FONTSHIFT_Y, 0, WA_CENTER },
        //.clockConf           = { TFT_FRAMEWDT, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 35+FONTSHIFT_Y, 0, WA_CENTER },
        //.vuConf              = { }, // unused
        .vuConf              = { TFT_FRAMEWDT, 35, 1, WA_LEFT },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 7, 54+FONTSHIFT_X*2-1, 1, 1, 10 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { TFT_FRAMEWDT+(54/2)+1+FONTSHIFT_X, 35+FONTSHIFT_Y, 0 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
        /* Rotated so the bands run horizontally: ch = width*2+space = 15px tall and
           cw = height = 44px long, fitting the y=38..54 line beside the clock */
        .rotateVU            = true,
        .shareWeatherIP      = true, // IP and weather share the bottom row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
    {   // Minimal
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT+1, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 15, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 24, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-10, 1, WA_LEFT }, 140, true, MAX_WIDTH-6*4, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .bufferbarConf       = { }, // unused
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 12, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { 0, 15, 1, WA_RIGHT },
        .voltxtConf          = { }, // unused
        .batteryConf         = { 0, 64-10, 1, WA_RIGHT },
        .iptxtConf           = { TFT_FRAMEWDT, 64-10, 1, WA_LEFT },
        .rssiConf            = { 0, 64-10, 1, WA_RIGHT },
        .numConf             = { 0, 26+FONTSHIFT_Y, 0, WA_CENTER },
        //.clockConf           = { TFT_FRAMEWDT, 38+FONTSHIFT, 0, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 35+FONTSHIFT_Y, 0, WA_CENTER },
        .vuConf              = { TFT_FRAMEWDT, 37, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_FRAMEWDT*2, 11, 2, 1, 16 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { }, // clock disappears when VU is on
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = true,
        .rotateVU            = false,
        .shareWeatherIP      = true, // IP and weather share the bottom row
        .shareBattRSSI       = true, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "";
const char bitrateFmt[]           PROGMEM = "%d";

#endif
