#ifndef displayNV3007_142conf_h
#define displayNV3007_142conf_h

#define DSP_WIDTH       428
#define DSP_HEIGHT      142
#define TFT_FRAMEWDT    4
#define MAX_WIDTH       DSP_WIDTH-TFT_FRAMEWDT*2

//#define HIDE_VOLBAR_ONLY_MAIN_SCREEN // Ukrywa pasek glosnosci tylko na glownym ekranie
#define HIDE_IP_ONLY_MAIN_SCREEN// Ukrywa adres IP tylko na glownym ekranie
//#define HIDE_VOL_IP // Ukrywa adres IP również na stronie głośności
#define HIDE_VOL_FOOTER // Ukrywa stopke na ekranie glosnosci

#define bootLogoTop     28


/* SROLLS  */                            /* {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */
const ScrollConfig metaConf       PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_LEFT }, 140, true, MAX_WIDTH, 5000, 5, 30 };
const ScrollConfig title1Conf     PROGMEM = {{ TFT_FRAMEWDT, 43, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 };
const ScrollConfig title2Conf     PROGMEM = {{ TFT_FRAMEWDT, 65, 2, WA_LEFT }, 140, true, MAX_WIDTH-165, 5000, 4, 30 };
const ScrollConfig playlistConf   PROGMEM = {{ TFT_FRAMEWDT, 112, 2, WA_LEFT }, 140, true, MAX_WIDTH, 1000, 4, 30 };
const ScrollConfig apTitleConf    PROGMEM = {{ TFT_FRAMEWDT, TFT_FRAMEWDT, 3, WA_CENTER }, 140, false, MAX_WIDTH, 0, 4, 20 };
const ScrollConfig apSettConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-18, 2, WA_LEFT }, 140, false, MAX_WIDTH, 0, 4, 30 };
const ScrollConfig weatherConf    PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-50, 2, WA_CENTER }, 140, false, MAX_WIDTH, 0, 2, 30 };

/* BACKGROUNDS  */                       /* {{ left, top, fontsize, align }, width, height, outlined } */
//const FillConfig   metaBGConf     PROGMEM = {{ 0, 0, 0, WA_CENTER }, DSP_WIDTH, 1, false };
const FillConfig   metaBGConf     PROGMEM = {{ 0, 0,  0, WA_LEFT }, DSP_WIDTH, 29, false };
const FillConfig   metaBGConfInv  PROGMEM = {{ 0, 0, 0, WA_LEFT }, DSP_WIDTH, 1, false };
const FillConfig   volbarConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-TFT_FRAMEWDT-3, 0, WA_LEFT }, MAX_WIDTH, 4, true };
const FillConfig  playlBGConf     PROGMEM = {{ 0, 107, 0, WA_LEFT }, DSP_WIDTH, 24, false };
const FillConfig  heapbarConf     PROGMEM = {{ TFT_FRAMEWDT, DSP_HEIGHT-2, 0, WA_LEFT }, MAX_WIDTH, 2, false };

/* WIDGETS  */                           /* { left, top, fontsize, align } */
//const WidgetConfig bootstrConf    PROGMEM = { 0, 182, 1, WA_CENTER };
const WidgetConfig bootstrConf    PROGMEM = { 0, DSP_HEIGHT-16, 2, WA_CENTER };
const WidgetConfig bitrateConf    PROGMEM = { TFT_FRAMEWDT+120, DSP_HEIGHT-27, 2, WA_RIGHT };

/* FOOTER */
const WidgetConfig iptxtConf      PROGMEM = { TFT_FRAMEWDT, DSP_HEIGHT-27, 2, WA_CENTER };
const WidgetConfig voltxtConf     PROGMEM = { 230, DSP_HEIGHT+27, 2, WA_LEFT };  // Hangerő
const WidgetConfig chtxtConf      PROGMEM = { 316, DSP_HEIGHT-27, 2, WA_LEFT }; //→ az aktuális csatorna CH:szöveg (PLAYER footer)
const WidgetConfig rssiConf       PROGMEM = { TFT_FRAMEWDT+4, DSP_HEIGHT-27, 2, WA_RIGHT };

const WidgetConfig numConf        PROGMEM = { TFT_FRAMEWDT, 95, 0, WA_CENTER };
const WidgetConfig apNameConf     PROGMEM = { 0, 36, 2, WA_CENTER };
const WidgetConfig apName2Conf    PROGMEM = { 0, 56, 2, WA_CENTER };
const WidgetConfig apPassConf     PROGMEM = { 0, 83, 2, WA_CENTER };
const WidgetConfig apPass2Conf    PROGMEM = { 0, 103, 2, WA_CENTER };
const WidgetConfig clockConf      PROGMEM = { TFT_FRAMEWDT, 82, 1, WA_RIGHT };
const WidgetConfig bootWdtConf    PROGMEM = { 0, DSP_HEIGHT-30, 1, WA_CENTER };
const WidgetConfig namedayConf    PROGMEM = { TFT_FRAMEWDT, 239, 1, WA_LEFT }; // Módosítás új sor "nameday"
const WidgetConfig dateConf       PROGMEM = { TFT_FRAMEWDT *2, 269, 1, WA_LEFT }; // Módosítás új sor "date"

const ProgressConfig bootPrgConf  PROGMEM = { 90, 10, 4 };
//const BitrateConfig fullbitrateConf PROGMEM = {{300, 4, 2, WA_RIGHT}, 21 }; // left, top, fontsize, align, border size
const BitrateConfig fullbitrateConf PROGMEM = {{210, DSP_HEIGHT-29, 2, WA_LEFT}, 50};

/* BANDS  */                             
#ifdef BOOMBOX_STYLE
const WidgetConfig  vuConf        PROGMEM = { 24, 190, 1, WA_CENTER }; // center fektetett, "align" nincs használva
const VUBandsConfig bandsConf     PROGMEM = { 130, 5, 4, 2, 20, 5 }; /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
#else
//const WidgetConfig  vuConf        PROGMEM = { 33, 190, 1, WA_CENTER }; // center fektetett, "align" nincs használva
const WidgetConfig  vuConf        PROGMEM = { TFT_FRAMEWDT, DSP_HEIGHT-27, 1, WA_CENTER }; // center fektetett, "align" nincs használva
const VUBandsConfig bandsConf     PROGMEM = { 200, 6, 2, 2, 30, 4 }; /* { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */
#endif
/* STRINGS  */
const char         numtxtFmt[]    PROGMEM = "%d";
const char           rssiFmt[]    PROGMEM = "WiFi %ddBm";
const char          iptxtFmt[]    PROGMEM = "\010 %s";
const char         voltxtFmt[]    PROGMEM = "\023\025%d%%";
const char        bitrateFmt[]    PROGMEM = "%d kBs";

/* MOVES  */                             /* { left, top, width } */
const MoveConfig    clockMove     PROGMEM = { 0, 176, -1 };
const MoveConfig   weatherMove    PROGMEM = { 10, DSP_HEIGHT-50, MAX_WIDTH };
const MoveConfig   weatherMoveVU  PROGMEM = { TFT_FRAMEWDT, DSP_HEIGHT-50, MAX_WIDTH};

#endif
// clang-format on