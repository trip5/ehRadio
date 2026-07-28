/*************************************************************************************
    TFT428x142 displays configuration file.
*************************************************************************************/

#ifndef displayTFT428x142conf_h
#define displayTFT428x142conf_h

#define DSP_WIDTH       428
#define DSP_HEIGHT      142
#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2
#define bootLogoTop     28

// Trip5 Note: This conf file was imported but remains un-implemented and un-tested...

#define HIDE_IP_ONLY_MAIN_SCREEN// Ukrywa adres IP tylko na glownym ekranie
#define HIDE_VOL_FOOTER // Ukrywa stopke na ekranie glosnosci

// ******************** CHECK ALL #define LINES CAREFULLY! ********************
const char _layoutNames[][32] PROGMEM = {"krzxsiek", "krzxsiek (BoomBox)", "krzxsiek-kopia"};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // [0] krzxsiek
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_FRAMEWDT, 43, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .title2Conf          = {{ TFT_FRAMEWDT, 65, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 },
        .apSettConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-18, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 },
        .weatherConf         = {{ TFT_FRAMEWDT, DSP_HEIGHT-50, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 30 },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 29, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-3, 0, WA_LEFT }, MAX_WIDTH, 4, true },
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        .bufferbarConf       = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, MAX_WIDTH, 2, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-16, 2, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+120, DSP_HEIGHT-27, 2, WA_RIGHT },
        // ??? chtxtConf     = { 316, DSP_HEIGHT-27, 2, WA_LEFT };
        .voltxtConf          = { 230, DSP_HEIGHT+27, 2, WA_LEFT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-27, 2, WA_CENTER },
        .rssiConf            = { TFT_FRAMEWDT+4, DSP_HEIGHT-27, 2, WA_RIGHT },
        .numConf             = { TFT_FRAMEWDT, 95, 0, WA_CENTER },
        .apNameConf          = { 0, 36, 2, WA_CENTER },
        .apName2Conf         = { 0, 56, 2, WA_CENTER },
        .apPassConf          = { 0, 83, 2, WA_CENTER },
        .apPass2Conf         = { 0, 103, 2, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 82, 1, WA_RIGHT },
        .vuConf              = { TFT_FRAMEWDT, DSP_HEIGHT-27, 1, WA_CENTER },
        // ??? namedayConf   = { TFT_FRAMEWDT, 239, 1, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 269, 1, WA_LEFT };
        .bootWdtConf         = { 0, DSP_HEIGHT-30, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 210, DSP_HEIGHT-29, 2, WA_LEFT }, 50 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 200, 6, 2, 2, 30, 4 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 10, DSP_HEIGHT-50, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, DSP_HEIGHT-50, MAX_WIDTH },
    },
    {   // [1] krzxsiek (BoomBox)
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 },
        .title1Conf          = {{ TFT_FRAMEWDT, 43, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .title2Conf          = {{ TFT_FRAMEWDT, 65, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 },
        .playlistConf        = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 },
        .apTitleConf         = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 },
        .apSettConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-18, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 },
        .weatherConf         = {{ TFT_FRAMEWDT, DSP_HEIGHT-50, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 30 },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 29, false },
        .metaBGConfInv       = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-3, 0, WA_LEFT }, MAX_WIDTH, 4, true },
        .playlBGConf         = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false },
        .bufferbarConf       = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, MAX_WIDTH, 2, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-16, 2, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+120, DSP_HEIGHT-27, 2, WA_RIGHT },
        // ??? chtxtConf     = { 316, DSP_HEIGHT-27, 2, WA_LEFT };
        .voltxtConf          = { 230, DSP_HEIGHT+27, 2, WA_LEFT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, DSP_HEIGHT-27, 2, WA_CENTER },
        .rssiConf            = { TFT_FRAMEWDT+4, DSP_HEIGHT-27, 2, WA_RIGHT },
        .numConf             = { TFT_FRAMEWDT, 95, 0, WA_CENTER },
        .apNameConf          = { 0, 36, 2, WA_CENTER },
        .apName2Conf         = { 0, 56, 2, WA_CENTER },
        .apPassConf          = { 0, 83, 2, WA_CENTER },
        .apPass2Conf         = { 0, 103, 2, WA_CENTER },
        .clockConf           = { TFT_FRAMEWDT, 82, 1, WA_RIGHT },
        .vuConf              = { 24, 190, 1, WA_CENTER },
        // ??? namedayConf   = { TFT_FRAMEWDT, 239, 1, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 269, 1, WA_LEFT };
        .bootWdtConf         = { 0, DSP_HEIGHT-30, 1, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 210, DSP_HEIGHT-29, 2, WA_LEFT }, 50 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { 130, 5, 4, 2, 20, 5 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 10, DSP_HEIGHT-50, MAX_WIDTH },
        .weatherMoveVU       = { TFT_FRAMEWDT, DSP_HEIGHT-50, MAX_WIDTH },
        /* BOOMBOX STYLE: middle-out VU */
        .boomboxStyle        = true,
    },
    {   // [2] krzxsiek-kopia
        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
        .metaConf            = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+2, 2, WA_LEFT }, 140, true, MAX_WIDTH-2, 5000, 2, 25 },
        .title1Conf          = {{ TFT_FRAMEWDT+1, 19+14, 2, WA_LEFT }, 135, true, DSP_WIDTH/2+38, 5000, 2, 25 },
        .title2Conf          = {{ TFT_FRAMEWDT+1, 19+6+14*2, 2, WA_LEFT }, 135, true, DSP_WIDTH/2+38, 5000, 2, 25 },
        .playlistConf        = {{ TFT_FRAMEWDT, 30, 2, WA_LEFT }, 140, true, MAX_WIDTH, 500, 2, 25 },
        .apTitleConf         = {{ TFT_FRAMEWDT+1, TFT_FRAMEWDT+1, 2, WA_CENTER }, 140, false, MAX_WIDTH-2, 0, 2, 25 },
        .apSettConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-18, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 2, 25 },
        .weatherConf         = {{ TFT_FRAMEWDT+1, DSP_HEIGHT-38, 2, WA_CENTER }, 140, true, MAX_WIDTH-2, 0, 2, 30 },
        /* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 24, false },
        .metaBGConfInv       = {{ 0, 19, 0, WA_LEFT }, DSP_WIDTH, 1,  false },
        .volbarConf          = {{ TFT_FRAMEWDT, DSP_HEIGHT-3, 0, WA_CENTER }, MAX_WIDTH, 5, true },
        .playlBGConf         = {{ 0, 26, 0, WA_LEFT }, DSP_WIDTH, 12, false },
        .bufferbarConf       = { }, // unused
        // .bufferbarConf     = {{ 0, 93, 0, WA_LEFT }, DSP_WIDTH, 1, false },
        /* WIDGETS             { left, top, fontsize, align } */
        .bootstrConf         = { 0, DSP_HEIGHT-16, 2, WA_CENTER },
        .bitrateConf         = { TFT_FRAMEWDT+31, 100-10-10, 2, WA_LEFT },
        // ??? chtxtConf     = { TFT_FRAMEWDT+125, 100-10-10, 2, WA_LEFT };
        .voltxtConf          = { TFT_FRAMEWDT+197, 100-10-10, 2, WA_LEFT },
        .batteryConf         = { },                                                   // <-- NEEDS EDITING!
        .iptxtConf           = { TFT_FRAMEWDT, 100-12, 1, WA_RIGHT },
        .rssiConf            = { TFT_FRAMEWDT+1, 100-10-10, 2, WA_LEFT },
        .numConf             = { TFT_FRAMEWDT, 105, 0, WA_CENTER },
        .apNameConf          = { 0, 33, 2, WA_CENTER },
        .apName2Conf         = { 0, 53, 2, WA_CENTER },
        .apPassConf          = { 0, 77, 2, WA_CENTER },
        .apPass2Conf         = { 0, 97, 2, WA_CENTER },
        .clockConf           = { 0, 80, 0, WA_RIGHT },
        // ??? namedayConf   = { TFT_FRAMEWDT, 175, 2, WA_LEFT };
        // ??? dateConf      = { TFT_FRAMEWDT *2, 226, 1, WA_LEFT };
        .vuConf              = { 2, DSP_HEIGHT-26, 1, WA_CENTER },
        .bootWdtConf         = { 0, 162, 2, WA_CENTER },
        .bootPrgConf         = { 90, 10, 4 },
        /* CODEC BADGE         {{ left, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 8, 104-10-10, 1, WA_LEFT }, 41 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
        .bandsConf           = { DSP_WIDTH/2-TFT_FRAMEWDT*2-2, 7,             TFT_FRAMEWDT*2+4, 1, 17, 2 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 176, -1 },
        .weatherMove         = { 0, 0, -1 },
        .weatherMoveVU       = { 0, 0, -1 },
        /* BOOMBOX STYLE: middle-out VU */
        .boomboxStyle        = true,
    },
};

/* STRINGS */
const char numtxtFmt[]            PROGMEM = "%d";
const char rssiFmt[]              PROGMEM = "WiFi %d";
const char iptxtFmt[]             PROGMEM = "\010 %s";
const char voltxtFmt[]            PROGMEM = "\023\025%d";
const char batteryRangeFmt[][8]   PROGMEM = { "\013 %d%%", "\014 %d%%", "\015 %d%%" };
const char bitrateFmt[]           PROGMEM = "%d kBs";

#endif
