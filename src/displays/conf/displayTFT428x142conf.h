/*************************************************************************************
    TFT428x142 displays configuration file.
*************************************************************************************/

#ifndef displayTFT428x142conf_h
#define displayTFT428x142conf_h

#define DSP_WIDTH       428
#define DSP_HEIGHT      142
#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define BOOTLOGOTOP     28

// Trip5 Note: This conf file was imported but remains un-implemented and un-tested...
// check displayOLED128x64 if needing to share IP & RSSI with Weather & Battery
// (which is what the next defines seem to imply - imported from the original layout)

#define HIDE_IP_ONLY_MAIN_SCREEN// Ukrywa adres IP tylko na glownym ekranie
#define HIDE_VOL_FOOTER // Ukrywa stopke na ekranie glosnosci

// ******************** CHECK ALL #define LINES CAREFULLY! ********************

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 },
        .apSettConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-18, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .apTitleBGConf       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 29, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-16, 2, WA_CENTER },
        .apNameConf          = { 0, 36, 2, WA_CENTER },
        .apName2Conf         = { 0, 56, 2, WA_CENTER },
        .apPassConf          = { 0, 83, 2, WA_CENTER },
        .apPass2Conf         = { 0, 103, 2, WA_CENTER },
        .bootWdtConf         = { 0, DSP_HEIGHT-30, 1, WA_CENTER },
        /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
        .bootPrgConf         = { 90, 14, 4 },
};

const char _layoutNames[][64] PROGMEM = {
    "krzxsiek",
    "krzxsiek (BoomBox)",
    "krzxsiek kopia",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // krzxsiek
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_FRAMEWDT, 43, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .title2Conf          = {{ TFT_FRAMEWDT, 65, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_FRAMEWDT, DSP_HEIGHT-50, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 30 },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-3, 0, WA_LEFT }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 29, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { TFT_FRAMEWDT+120, DSP_HEIGHT-27, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { 316, DSP_HEIGHT-27, 2, WA_LEFT };
        .voltxtConf          = { 230, DSP_HEIGHT+27, 2, WA_LEFT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-27, 2, WA_CENTER },
        .rssiConf            = { TFT_FRAMEWDT+4, DSP_HEIGHT-27, 2, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_FRAMEWDT, 95, 2, WA_CENTER },
        // ??? namedayConf   = { TFT_FRAMEWDT, 239, 1, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 269, 1, WA_LEFT };
        .clockConf           = { TFT_FRAMEWDT, 82, 2, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { TFT_FRAMEWDT, DSP_HEIGHT-27, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 210, DSP_HEIGHT-29, 2, WA_LEFT }, 50 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 200, 6, 2, 2, 30 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 10, DSP_HEIGHT-50, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, DSP_HEIGHT-50, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .boomboxVU           = false, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // krzxsiek (BoomBox)
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_FRAMEWDT, 43, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .title2Conf          = {{ TFT_FRAMEWDT, 65, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 },
        .weatherConf         = {{ TFT_FRAMEWDT, DSP_HEIGHT-50, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 30 },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-3, 0, WA_LEFT }, MAX_WIDTH, 4, true },
        .bufferbarConf       = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, MAX_WIDTH, 2, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 29, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { TFT_FRAMEWDT+120, DSP_HEIGHT-27, 2, WA_RIGHT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { 316, DSP_HEIGHT-27, 2, WA_LEFT };
        .voltxtConf          = { 230, DSP_HEIGHT+27, 2, WA_LEFT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-27, 2, WA_CENTER },
        .rssiConf            = { TFT_FRAMEWDT+4, DSP_HEIGHT-27, 2, WA_RIGHT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_FRAMEWDT, 95, 2, WA_CENTER },
        // ??? namedayConf   = { TFT_FRAMEWDT, 239, 1, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 269, 1, WA_LEFT };
        .clockConf           = { TFT_FRAMEWDT, 82, 2, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        .vuConf              = { 24, 190, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 210, DSP_HEIGHT-29, 2, WA_LEFT }, 50 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 130, 5, 4, 2, 20 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 10, DSP_HEIGHT-50, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, DSP_HEIGHT-50, MAX_WIDTH },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        /* BOOMBOX VU: middle-out */
        .boomboxVU           = true, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // krzxsiek kopia
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+2, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, 5000, 2, 25 },
        .title1Conf          = {{ TFT_FRAMEWDT+1, 19+14, 2, WA_LEFT }, 135, true, DSP_WIDTH/2+38, 5000, 2, 25 },
        .title2Conf          = {{ TFT_FRAMEWDT+1, 19+6+14*2, 2, WA_LEFT }, 135, true, DSP_WIDTH/2+38, 5000, 2, 25 },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 2, WA_LEFT }, 140, true, MAX_WIDTH, 500, 2, 25 },
        .weatherConf         = {{ TFT_FRAMEWDT+1, DSP_HEIGHT-38, 2, WA_CENTER }, 140, true, MAX_WIDTH-2, 0, 2, 30 },
        /* SLIDER BARS         {{ left, top, fontsize, align }, width, height, outlined } */
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-3, 0, WA_CENTER }, MAX_WIDTH, 5, true },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf     = {{ 0, 93, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* LINES + RECTANGLES  {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 24, false },
        .metaBGConfInv       = {{ 0, 19, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .underLineConf       = { },
        .overLineConf        = { },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        /* WIDGETS             { left, top, fontsize, align } */
        //.bitrateConf         = { TFT_FRAMEWDT+31, 100-10-10, 2, WA_LEFT },
        .bitrateConf         = { }, // unused
        // ??? chtxtConf     = { TFT_FRAMEWDT+125, 100-10-10, 2, WA_LEFT };
        .voltxtConf          = { TFT_FRAMEWDT+197, 100-10-10, 2, WA_LEFT },
        .batteryConf         = { }, // <--------- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 100-12, 1, WA_RIGHT },
        .rssiConf            = { TFT_FRAMEWDT+1, 100-10-10, 2, WA_LEFT },
        /* NUMBERS FONT        { left, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { TFT_FRAMEWDT, 105, 2, WA_CENTER },
        .clockConf           = { 0, 80, 2, WA_RIGHT },
        /* VU BARS WIDGET      { left, top, 1, align } */
        // ??? namedayConf   = { TFT_FRAMEWDT, 175, 2, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 226, 1, WA_LEFT };
        .vuConf              = { 2, DSP_HEIGHT-26, 1, WA_CENTER },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 8, 104-10-10, 1, WA_LEFT }, 41 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { DSP_WIDTH/2-TFT_FRAMEWDT*2-2, 7, TFT_FRAMEWDT*2+4, 1, 17 },
        /* MOVES               { left, top, width (-1 keeps Conf position) */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        /* BOOMBOX VU: middle-out */
        .boomboxVU           = true, // VU drawn as a "boombox" horizontal meter
        .rotateVU            = false, // VU rotated 90 degrees
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
};
// ******************** CHECK ALL const char LINES CAREFULLY! ********************
// check all needed strings are present... and double-check octal codes \0xx too!
// Note that rssi and battery will still render 2-glyph icons even when blank

/* STRINGS */
const char numtxtFmt[]              PROGMEM = "%d";
const char rssiFmt[]                PROGMEM = "WiFi %ddBm";
const char iptxtFmt[]               PROGMEM = "\010 %s";
const char voltxtFmt[]              PROGMEM = "%d%%";
const char bitrateFmt[]             PROGMEM = "%d kBs";

// Automatically added by conf_tool.py:
const char batterytxtFmt[]          PROGMEM = "%d%%";

#endif
