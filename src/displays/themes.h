#ifndef ehRadio_themes_h
#define ehRadio_themes_h

#include <stdint.h>

struct ThemeData {
  uint16_t background;
  uint16_t meta;
  uint16_t metabg;
  uint16_t metafill;
  uint16_t title1;
  uint16_t title2;
  uint16_t digit;
  uint16_t div;
  uint16_t weather;
  uint16_t vumax;
  uint16_t vumin;
  uint16_t clock;
  uint16_t clockbg;
  uint16_t seconds;
  uint16_t dow;
  uint16_t date;
  uint16_t clockss;
  uint16_t clockbgss;
  uint16_t secondsss;
  uint16_t dowss;
  uint16_t datess;
  uint16_t buffer;
  uint16_t ip;
  uint16_t vol;
  uint16_t rssi;
  uint16_t battery;
  uint16_t bitrate;
  uint16_t volbarout;
  uint16_t volbarin;
  uint16_t plcurrent;
  uint16_t plcurrentbg;
  uint16_t plcurrentfill;
  uint16_t playlist[5];
};

#define RGB(r, g, b) ((uint16_t)(((r) >> 3) << 11) | ((uint16_t)((g) >> 2) << 5) | ((uint16_t)(b) >> 3))

const char _themeNames[][32] PROGMEM = {
    "ehRadio Blue & Red (Trip5)",
    "ёRadio Gold (e2002)",
    "Gray (Krzsiek)",
    "Navy Blue (Rico van Dooren)",
    "Golden Wind (András Daradici)",
};

const ThemeData _themes[] PROGMEM = {
    {   // ehRadio Blue & Red (Trip5)
        .background   = RGB(  0,   0,   0),
        .meta         = RGB(247, 247, 247),
        .metabg       = RGB(  0,  63, 207),
        .metafill     = RGB(  0,  55, 191),
        .title1       = RGB(239, 239, 239),
        .title2       = RGB(207, 207, 207),
        .digit        = RGB(255,  31,   7),
        .div          = RGB( 91,  91,  91),
        .weather      = RGB(223, 223,   0),
        .vumax        = RGB(175,  31,  31),
        .vumin        = RGB( 15, 127,  15),
        .clock        = RGB(255,  31,   7),
        .clockbg      = RGB( 31,   3,   0),
        .seconds      = RGB(247,  27,   5),
        .dow          = RGB(255, 192, 192),
        .date         = RGB(192, 192, 255),
        .clockss      = RGB(153, 217, 234),
        .clockbgss    = RGB(  8,  11,  12),
        .secondsss    = RGB(140, 200, 220),
        .dowss        = RGB(110, 110, 150),
        .datess       = RGB(150, 110, 110),
        .buffer       = RGB(231,  47, 255),
        .ip           = RGB(153, 217, 234),
        .vol          = RGB(223, 223,   0),
        .rssi         = RGB(153, 217, 234),
        .battery      = RGB(153, 217, 234),
        .bitrate      = RGB(231,  47, 255),
        .volbarout    = RGB(223, 223,   0),
        .volbarin     = RGB(207, 207,   0),
        .plcurrent    = RGB(255, 255, 255),
        .plcurrentbg  = RGB(255,  31,   7),
        .plcurrentfill= RGB(231,  23,   7),
        .playlist     = {RGB(231,231,231), RGB(199,199,199), RGB(167,167,167), RGB(135,135,135), RGB(103,103,103)},
    },
    {   // ёRadio Gold (e2002)
        .background    = RGB(  0,   0,   0),
        .meta          = RGB(  0,   0,   0),
        .metabg        = RGB(231, 211,  90),
        .metafill      = RGB(231, 211,  90),
        .title1        = RGB(255, 255, 255),
        .title2        = RGB(165, 162, 132),
        .digit         = RGB(255, 255, 255),
        .div           = RGB(165, 162, 132),
        .weather       = RGB(255, 150,   0),
        .vumax         = RGB(231, 211,  90),
        .vumin         = RGB(123, 125, 123),
        .clock         = RGB(231, 211,  90),
        .clockbg       = RGB( 28,  28,  28),
        .seconds       = RGB(231, 211,  90),
        .dow           = RGB(255, 255, 255),
        .date          = RGB(165, 162, 132),
        .clockss       = RGB(165, 162, 132),
        .clockbgss     = RGB( 14,  14,  14),
        .secondsss     = RGB(165, 162, 132),
        .dowss         = RGB(115, 115, 115),
        .datess        = RGB(115, 115,  90),
        .buffer        = RGB(165, 162, 132),
        .ip            = RGB(165, 162, 132),
        .vol           = RGB(165, 162, 132),
        .rssi          = RGB(165, 162, 132),
        .battery       = RGB(165, 162, 132),
        .bitrate       = RGB(231, 211,  90),
        .volbarout     = RGB(231, 211,  90),
        .volbarin      = RGB(231, 211,  90),
        .plcurrent     = RGB(  0,   0,   0),
        .plcurrentbg   = RGB(231, 211,  90),
        .plcurrentfill = RGB(231, 211,  90),
        .playlist      = {RGB(115, 115, 115), RGB( 89,  89,  89), RGB( 56,  56,  56), RGB( 35,  35,  35), RGB( 25,  25,  25)},
    },
    {   // Gray (Krzsiek)
        .background    = RGB(  0,   0,   0),
        .meta          = RGB(  0,   0,   0),
        .metabg        = RGB(255, 255, 255),
        .metafill      = RGB(125, 125, 125),
        .title1        = RGB(220, 220, 220),
        .title2        = RGB(162, 162, 162),
        .digit         = RGB(255, 255, 255),
        .div           = RGB(162, 162, 162),
        .weather       = RGB(220, 220, 220),
        .vumax         = RGB(255, 255, 255),
        .vumin         = RGB(130, 130, 130),
        .clock         = RGB(255, 255, 255),
        .clockbg       = RGB( 28,  28,  28),
        .seconds       = RGB(255, 255, 255),
        .dow           = RGB(255, 255, 255),
        .date          = RGB(255, 255, 255),
        .clockss       = RGB(255, 255, 255),
        .clockbgss     = RGB( 28,  28,  28),
        .secondsss     = RGB(255, 255, 255),
        .dowss         = RGB(255, 255, 255),
        .datess        = RGB(255, 255, 255),
        .buffer        = RGB( 90,  90,  90),
        .ip            = RGB(162, 162, 162),
        .vol           = RGB(162, 162, 162),
        .rssi          = RGB(162, 162, 162),
        .battery       = RGB(162, 162, 162),
        .bitrate       = RGB(200, 200, 200),
        .volbarout     = RGB(200, 200, 200),
        .volbarin      = RGB(200, 200, 200),
        .plcurrent     = RGB(255, 255, 255),
        .plcurrentbg   = RGB( 10,  10,  10),
        .plcurrentfill = RGB( 10,  10,  10),
        .playlist      = {RGB(115, 115, 115), RGB( 89,  89,  89), RGB( 56,  56,  56), RGB( 35,  35,  35), RGB( 25,  25,  25)},
        // ??? COLOR_NAMEDAY = RGB(200, 200, 200)
        // ??? COLOR_VU_MID = RGB(200, 200, 200)
        // ??? COLOR_CH = RGB(162, 162, 162)
        // ??? COLOR_HEAP = RGB(41, 40, 41)
        // ??? COLOR_PRST_BUTTON = RGB(21, 21, 21)
        // ??? COLOR_PRST_CARD = RGB(21, 21, 21)
        // ??? COLOR_PRST_ACCENT = RGB(50, 50, 50)
        // ??? COLOR_PRST_FAV = RGB(255, 255, 255)
        // ??? COLOR_PRST_TITLE_1 = RGB(255, 255, 255)
        // ??? COLOR_PRST_TITLE_2 = RGB(200, 200, 200)
        // ??? COLOR_PRST_TITLE_3 = RGB(150, 150, 150)
        // ??? COLOR_PRST_LINE = RGB(162, 162, 162)
    },
    {   // Navy Blue (Rico van Dooren)
        .background    = RGB( 40,  40,  40),
        .meta          = RGB(255, 255, 255),
        .metabg        = RGB(  0,   0, 125),
        .metafill      = RGB(  0,   0, 125),
        .title1        = RGB(255, 255, 255),
        .title2        = RGB(105, 105, 105),
        .digit         = RGB(100, 100, 255),
        .div           = RGB(255, 255, 255),
        .weather       = RGB(105, 105, 105),
        .vumax         = RGB(255, 255, 255),
        .vumin         = RGB(  0,   0, 125),
        .clock         = RGB(255, 255, 255),
        .clockbg       = RGB(  0,   0,   0),
        .seconds       = RGB(185, 185, 185),
        .dow           = RGB(185, 185, 185),
        .date          = RGB(185, 185, 185),
        .clockss       = RGB(127, 127, 127), // needs fixing?
        .clockbgss     = RGB( 19,  19,  19), // needs fixing?
        .secondsss     = RGB( 92,  92,  92), // needs fixing?
        .dowss         = RGB( 92,  92,  92), // needs fixing?
        .datess        = RGB( 92,  92,  92), // needs fixing?
        .buffer        = RGB(  0,   0, 105),
        .ip            = RGB(255, 255, 255),
        .vol           = RGB(255, 255, 255),
        .rssi          = RGB(  0,   0, 125),
        .battery       = RGB(  0,   0, 125), // needs fixing?
        .bitrate       = RGB(  0,   0, 125),
        .volbarout     = RGB(105, 105, 105),
        .volbarin      = RGB(255, 255, 255),
        .plcurrent     = RGB(  0,   0,   0),
        .plcurrentbg   = RGB( 91, 118, 255),
        .plcurrentfill = RGB( 91, 118, 255),
        .playlist      = {RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255)},
        // ??? COLOR_HEAP = RGB(255, 168, 162)
    },
    {   // Golden Wind (András Daradici)
        .background    = RGB(255, 177,   0),
        .meta          = RGB(255, 255, 255),
        .metabg        = RGB(195, 117,   0),
        .metafill      = RGB(195, 117,   0),
        .title1        = RGB(255, 255, 255),
        .title2        = RGB(225, 225, 225),
        .digit         = RGB(100, 100, 255),
        .div           = RGB(255, 255, 255),
        .weather       = RGB(255, 255, 255),
        .vumax         = RGB(105,  27,   0),
        .vumin         = RGB(195, 117,   0),
        .clock         = RGB(249, 255, 255),
        .clockbg       = RGB(195, 117,   0),
        .seconds       = RGB(255, 255, 255),
        .dow           = RGB(255, 255, 255),
        .date          = RGB(255, 255, 255),
        .clockss       = RGB(124, 127, 127), // needs fixing?
        .clockbgss     = RGB( 18,  19,  19), // needs fixing?
        .secondsss     = RGB(127, 127, 127), // needs fixing?
        .dowss         = RGB(127, 127, 127), // needs fixing?
        .datess        = RGB(127, 127, 127), // needs fixing?
        .buffer        = RGB(195, 117,   0),
        .ip            = RGB(255, 255, 255),
        .vol           = RGB(  0,   0,   0),
        .rssi          = RGB(195, 117,   0),
        .battery       = RGB(195, 117,   0), // needs fixing?
        .bitrate       = RGB(195, 117,   0),
        .volbarout     = RGB(195, 117,   0),
        .volbarin      = RGB(195, 117,   0),
        .plcurrent     = RGB(255, 255, 255),
        .plcurrentbg   = RGB(195, 117,   0),
        .plcurrentfill = RGB(195, 117,   0),
        .playlist      = {RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255), RGB(255, 255, 255)},
        // ??? COLOR_HEAP = RGB(255, 168, 162)
    },
};

#endif
