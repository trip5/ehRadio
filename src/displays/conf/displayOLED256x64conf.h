/*************************************************************************************
    OLED256x64 displays configuration file.
*************************************************************************************/

#ifndef displayOLED256x64conf_h
#define displayOLED256x64conf_h

#define TFT_FRAMEWDT    1
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     8

/* YO_MONO's glyphs sit a little narrower and lower than the chunky fonts, so its
   clock wants a small nudge right and up.  Two shifts, one per axis, so the
   .clockConf below stays one line for every font.

   THIS DEFINITELY NEEDS TO BE REMOVED IF WE ALTER HOW CLOCK LOCATION IS DETERMINED */

#if CLOCKFONT == YO_MONO
  #define CLOCKSHIFT_X 4
  #define CLOCKSHIFT_Y -1
#else
  #define CLOCKSHIFT_X 0
  #define CLOCKSHIFT_Y 0
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 1, WA_CENTER }, 140, false, MAX_WIDTH-2, 0, 1, SCROLLTIME },
        .apSettConf          = {{ TFT_FRAMEWDT, 64-7, 1, WA_LEFT }, 140, false, MAX_WIDTH, 0, 1, SCROLLTIME },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        //.apTitleBGConf       = {{ 0, 18, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .apTitleBGConf       = { }, // unused
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, 64-8, 1, WA_CENTER },
        .apNameConf          = { 0, 18, 1, WA_CENTER },
        .apName2Conf         = { 0, 26, 1, WA_CENTER },
        .apPassConf          = { 0, 37, 1, WA_CENTER },
        .apPass2Conf         = { 0, 45, 1, WA_CENTER },
        .bootWdtConf         = { 0, 64-8*2-5, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 10, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "Default",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, SCROLLDELAY, 2, SCROLLTIME*7/4 },
        .title1Conf          = {{ TFT_FRAMEWDT, 21, 1, WA_LEFT }, 140, true, DSP_WIDTH/2-3, SCROLLDELAY, 1, SCROLLTIME },
        .title2Conf          = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, DSP_WIDTH/2-3, SCROLLDELAY, 1, SCROLLTIME },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 1, WA_LEFT }, 140, true, MAX_WIDTH, SCROLLDELAY/5, 1, SCROLLTIME },
        .weatherConf         = {{ TFT_FRAMEWDT, 64-13, 1, WA_LEFT }, 140, true, DSP_WIDTH/2-3, 0, 1, SCROLLTIME },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ 0, 64-1-1-1, 0, WA_LEFT }, DSP_WIDTH, 3, true },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf       = {{ 0, 63, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 18, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 18, false },
        .underLineConf       = {{ DSP_WIDTH/2, 18, 0, WA_LEFT }, 1, 43, false },
        // .overLineConf       = {{ 0, 39, 0, WA_LEFT }, DSP_WIDTH/2, 1, false },
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bitrateConf         = { TFT_FRAMEWDT+20, 64-12-10, 1, WA_LEFT },
        .voltxtConf          = { }, // unused
        // .voltxtConf          = { 32, 108, 1, WA_RIGHT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { 0, 64-13, 1, WA_LEFT },
        .rssiConf            = { 0, 64-12-10, 1, WA_LEFT },
        .numConf             = { TFT_FRAMEWDT, 57, 0, WA_CENTER },
        .clockConf           = { CLOCKSHIFT_X, 58 + CLOCKSHIFT_Y, 0, WA_RIGHT },
        // .clockConf         = { 6, 34, 2, WA_CENTER },
        //.vuConf              = { }, // unused
        .vuConf              = { DSP_WIDTH/2+4, DSP_HEIGHT/2-7, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = { }, // unused
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 11, 121, 7, 1, 25 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter (was boomboxStyle)
        .rotateVU            = true, // VU rotated 90 degrees
        .shareWeatherIP      = true, // IP and weather share one row (was the IP_WEATHER_SHARED macro)
        .shareBattRSSI       = true, // RSSI and battery share one row (was the RSSI_BATT_SHARED macro)
        .rssiDigit           = false, // signal drawn as a number, not bars (was the RSSI_DIGIT macro)
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "%d";
const char iptxtFmt[]             PROGMEM = "\037 %s";
const char voltxtFmt[]            PROGMEM = "";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
