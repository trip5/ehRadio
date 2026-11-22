/*************************************************************************************
    TFT480x320 displays configuration file.
*************************************************************************************/

#ifndef displayTFT480x320conf_h
#define displayTFT480x320conf_h

#define TFT_BORDER      10
#define MAX_WIDTH       DSP_WIDTH-TFT_BORDER*2

#if defined(BIG_BOOT_LOGO) || BIG_BOOT_LOGO // true
  #define BOOTLOGOTOP   60
#else
  #define BOOTLOGOTOP   110
#endif

const BootData _bootConfig PROGMEM = {
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .apTitleConf         = {{ TFT_BORDER, TFT_BORDER, 4, WA_CENTER }, MAX_WIDTH },
        .apSettConf          = {{ TFT_BORDER, 320-TFT_BORDER-16, 2, WA_LEFT }, MAX_WIDTH },
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
    "Big Text",
    "BoomBox Classic",
    "BoomBox Frame",
    "BoomBox Neon",
    "Flip Flop",
    "Flip Flop Big Clock",
    "High Bars",
};

/* LAYOUT DEFINITIONS */

const LayoutData _layouts[] PROGMEM = {
    {   // Default
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 4, WA_LEFT }, MAX_WIDTH },
        .title1Conf          = {{ TFT_BORDER, 62, 2, WA_LEFT }, 410 },
        .title2Conf          = {{ TFT_BORDER, 86, 2, WA_LEFT }, 410 },
        .playlistConf        = {{ TFT_BORDER, 146, 3, WA_LEFT }, MAX_WIDTH },
        .weatherConf         = {{ TFT_BORDER, 120, 2, WA_LEFT }, MAX_WIDTH },
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
        .vuConf              = { TFT_BORDER, 120, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ DSP_WIDTH-TFT_BORDER-38, 59, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 32, 135, 4, 2, 9 },
        /* MOVES               { left, top, width } */
        .clockMove           = { TFT_BORDER*2, 230, 0 },
        .weatherMove         = { TFT_BORDER, 120, MAX_WIDTH },
        .weatherMoveVU       = { 90, 120, 380 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Default (VU Rotated)
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ TFT_BORDER, TFT_BORDER, 4, WA_LEFT }, MAX_WIDTH },
        .title1Conf          = {{ TFT_BORDER, 62, 2, WA_LEFT }, MAX_WIDTH-(44==0?6*2*7-6:44) },
        .title2Conf          = {{ TFT_BORDER, 86, 2, WA_LEFT }, MAX_WIDTH-44 },
        .playlistConf        = {{ TFT_BORDER, 146, 3, WA_LEFT }, MAX_WIDTH },
        .weatherConf         = {{ TFT_BORDER, 120, 2, WA_LEFT }, MAX_WIDTH },
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
    {   // Big Text
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 10, 4, WA_LEFT }, 460 },
        .title1Conf          = {{ 10, 58, 3, WA_LEFT }, 390 },
        .title2Conf          = {{ 10, 95, 3, WA_LEFT }, 390 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 100, 3, WA_LEFT }, 460 },
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
    {   // BoomBox Classic
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 10, 4, WA_LEFT }, 460 },
        .title1Conf          = {{ 10, 62, 2, WA_LEFT }, 460 },
        .title2Conf          = {{ 10, 86, 2, WA_LEFT }, 460 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 221, 2, WA_CENTER }, 460 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 308, 0, 0 }, 460, 5, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, 480, 2, false },
        .underLineConf       = { }, // unused
        .overLineConf        = { }, // unused
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, 289, 2, WA_CENTER },
        .batteryConf         = { 322, 289, 2, WA_LEFT },
        .iptxtConf           = { 10, 289, 2, WA_LEFT },
        .rssiConf            = { 10, 289, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 10, 185, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 10, 250, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 48, 120, 2, WA_RIGHT }, 60 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 228, 24, 4, 2, 19 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 10, 185, 0 },
        .weatherMove         = { 10, 221, 0 },
        .weatherMoveVU       = { 10, 221, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox Frame
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 6, 4, WA_CENTER }, 460 },
        .title1Conf          = {{ 10, 58, 2, WA_CENTER }, 460 },
        .title2Conf          = {{ 10, 80, 2, WA_CENTER }, 460 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 21, 253, 2, WA_CENTER }, 438 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 308, 0, 0 }, 460, 5, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 47, false },
        .metaBGConfInv       = {{ 0, 47, 0, 0 }, 480, 2, false },
        .underLineConf       = {{ 10, 156, 0, 0 }, 460, 122, true },
        .overLineConf        = {{ 85, 147, 0, 0 }, 310, 97, true },
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 10, 289, 2, WA_LEFT },
        .batteryConf         = { 322, 289, 2, WA_LEFT },
        .iptxtConf           = { 0, 289, 2, WA_CENTER },
        .rssiConf            = { 10, 289, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 0, 219, 3, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 25, 109, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 28, 173, 2, WA_RIGHT }, 40 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 213, 24, 4, 2, 21 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 219, 0 },
        .weatherMove         = { 21, 253, 0 },
        .weatherMoveVU       = { 21, 253, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // BoomBox Neon
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 8, 4, WA_CENTER }, 460 },
        .title1Conf          = {{ 10, 205, 2, WA_CENTER }, 460 },
        .title2Conf          = {{ 10, 226, 2, WA_CENTER }, 460 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 168, 2, WA_CENTER }, 460 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 308, 0, 0 }, 460, 5, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, 480, 2, false },
        .underLineConf       = {{ 0, 193, 0, 0 }, 480, 2, false },
        .overLineConf        = {{ 10, 63, 0, 0 }, 130, 70, true },
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, 289, 2, WA_CENTER },
        .batteryConf         = { 322, 289, 2, WA_LEFT },
        .iptxtConf           = { 10, 289, 2, WA_LEFT },
        .rssiConf            = { 10, 289, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 10, 133, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 38, 252, 0, WA_CENTER },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 50, 72, 2, WA_RIGHT }, 52 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 200, 25, 4, 2, 20 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 10, 133, 0 },
        .weatherMove         = { 10, 168, 0 },
        .weatherMoveVU       = { 10, 168, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Flip Flop
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 6, 4, WA_LEFT }, 460 },
        .title1Conf          = {{ 10, 59, 2, WA_LEFT }, 406 },
        .title2Conf          = {{ 10, 83, 2, WA_LEFT }, 406 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 203, 2, WA_CENTER }, 460 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 308, 0, 0 }, 460, 5, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 46, false },
        .metaBGConfInv       = {{ 0, 46, 0, 0 }, 480, 2, false },
        .underLineConf       = {{ 10, 151, 0, 0 }, 217, 1, false },
        .overLineConf        = {{ 227, 111, 0, 0 }, 243, 83, true },
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, 289, 2, WA_CENTER },
        .batteryConf         = { 322, 289, 2, WA_LEFT },
        .iptxtConf           = { 10, 289, 2, WA_LEFT },
        .rssiConf            = { 10, 289, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 18, 172, 3, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 10, 232, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 428, 55, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 19, 460, 4, 3, 42 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 18, 172, 0 },
        .weatherMove         = { 10, 203, 0 },
        .weatherMoveVU       = { 10, 203, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = true, // the divider and the day/date column right of the time
        .seconds             = true, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // Flip Flop Big Clock
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 10, 4, WA_LEFT }, 460 },
        .title1Conf          = {{ 10, 62, 2, WA_LEFT }, 406 },
        .title2Conf          = {{ 10, 86, 2, WA_LEFT }, 406 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 205, 2, WA_CENTER }, 460 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 10, 308, 0, 0 }, 460, 5, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 50, false },
        .metaBGConfInv       = {{ 0, 50, 0, 0 }, 480, 2, false },
        .underLineConf       = {{ 10, 150, 0, 0 }, 100, 2, false },
        .overLineConf        = {{ 370, 150, 0, 0 }, 100, 2, false },
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 0, 289, 2, WA_CENTER },
        .batteryConf         = { 322, 289, 2, WA_LEFT },
        .iptxtConf           = { 10, 289, 2, WA_LEFT },
        .rssiConf            = { 10, 289, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 0, 186, 4, WA_CENTER },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 10, 234, 0, WA_RIGHT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 428, 58, 2, WA_LEFT }, 42 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 18, 460, 4, 3, 39 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 0, 186, 0 },
        .weatherMove         = { 10, 205, 0 },
        .weatherMoveVU       = { 10, 205, 0 },
        /* TRANSFORMS          boolean */
        .fullClock           = false, // the divider and the day/date column right of the time
        .seconds             = false, // the seconds block right of the time
        .shareWeatherIP      = false, // IP and weather share the same row
        .shareBattRSSI       = false, // RSSI and battery share the same row
        .rssiDigit           = false, // signal drawn as a number, not bars
    },
    {   // High Bars
        /* SCROLLS             {{ offset, top, fontsize, align }, width } */
        .metaConf            = {{ 10, 6, 4, WA_LEFT }, 460 },
        .title1Conf          = {{ 10, 62, 2, WA_LEFT }, 375 },
        .title2Conf          = {{ 10, 86, 2, WA_LEFT }, 375 },
        .playlistConf        = {{ 10, 146, 3, WA_LEFT }, 460 },
        .weatherConf         = {{ 10, 250, 2, WA_LEFT }, 375 },
        /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
        .volbarConf          = {{ 0, 307, 0, 0 }, 480, 6, true },
        .bufferbarConf       = {{ 0, 318, 0, 0 }, 480, 2, false },
        /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
        .metaBGConf          = {{ 0, 0, 0, 0 }, 480, 46, false },
        .metaBGConfInv       = {{ 0, 46, 0, 0 }, 480, 1, false },
        .underLineConf       = {{ 0, 51, 0, 0 }, 395, 231, true },
        .overLineConf        = {{ 400, 51, 0, 0 }, 80, 231, true },
        .playlBGConf         = {{ 0, 138, 0, 0 }, 480, 36, false },
        /* WIDGETS             { offset, top, fontsize, align } */
        .bitrateConf         = { }, // unused
        .voltxtConf          = { 10, 288, 2, WA_LEFT },
        .batteryConf         = { 322, 288, 2, WA_LEFT },
        .iptxtConf           = { 0, 288, 2, WA_CENTER },
        .rssiConf            = { 10, 288, 2, WA_RIGHT },
        /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
        .numConf             = { 0, 200, 4, WA_CENTER },
        .clockConf           = { 93, 200, 4, WA_RIGHT },
        /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
        .vuConf              = { 410, 61, 0, WA_LEFT },
        /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
        .fullbitrateConf     = {{ 10, 131, 2, WA_LEFT }, 44 },
        /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
        .bandsConf           = { 28, 220, 4, 2, 24 },
        /* MOVES               { left, top, width } */
        .clockMove           = { 93, 200, 0 },
        .weatherMove         = { 10, 250, 0 },
        .weatherMoveVU       = { 10, 250, 0 },
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
const char iptxtFmt[]             PROGMEM = "\021\022%s";
const char voltxtFmt[]            PROGMEM = "%d";
const char batterytxtFmt[]        PROGMEM = "%d%%";
const char bitrateFmt[]           PROGMEM = "%s kbps";

#endif
