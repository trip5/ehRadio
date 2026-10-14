#ifndef conf_index_h
#define conf_index_h
#pragma once

// ==========================================================================
// conf_index.h — the selected conf file's own table, described for other code
// ==========================================================================
// Included at the tail of dspcore.h, AFTER the conf file, because the constants
// at the bottom are that file's macros.  It serves the layout JSON parser (which
// field is which struct, and where it sits in LayoutData) and the editor page
// (the same facts, to draw and label the boxes).  Nothing here decides behaviour:
// a layout file stores numbers and this header says what they mean.
// rationale, alternatives and measurements: plans/layout-editor.md

#include <stddef.h>
#include <stdint.h>
#include "../widgets/widgetsconfig.h"

// ---------------------------------------------------------------------------
// Shapes — one per struct in widgetsconfig.h
// ---------------------------------------------------------------------------
// The cell list per type is what the page draws AND what the parser reads, so
// the two cannot disagree about whether .bandsConf has a "dimension" or a
// "perheight".

enum confType_e : uint8_t {
  CT_WIDGET  = 0, // WidgetConfig
  CT_SCROLL,      // ScrollConfig
  CT_FILL,        // FillConfig
  CT_BITRATE,     // BitrateConfig
  CT_BANDS,       // VUBandsConfig
  CT_MOVE,        // MoveConfig
  CT_BOOL,        // bool: one value under the field's own key, no cells
  CT_COUNT
};

enum confCellKind_e : uint8_t {
  CK_NUM   = 0, // a plain number box
  CK_ALIGN,     // the WA_LEFT / WA_CENTER / WA_RIGHT dropdown
  CK_BOOL,      // a checkbox that stores 0 or 1
};

struct ConfCell {
  const char* key;    // the cell's name in the document
  const char* label;  // what a human reads; never parsed
  uint8_t     kind;   // confCellKind_e
};

struct ConfField {
  const char* key;    // the member name in LayoutData, and the key in every layout file
  const char* label;  // what a human reads; never parsed, never stored
  uint8_t     type;   // confType_e
  uint8_t     group;  // index into _confGroups[]
  uint8_t     flags;  // the CONF_* bits below
  uint16_t    offset; // offsetof(LayoutData, key): built from the struct, so it cannot drift
  uint8_t     unused; // a bit per cell index, for a cell this type carries but never reads (0 = all read)
};

// The field is load-bearing, not merely optional: dialogs write into the meta
// line and the playlist page is built on the playlist line, which is why
// conf_tool.py refuses to repair an entry missing either one.
#define CONF_REQUIRED      0x01
// The fontsize cell is a clock size INDEX, where 0 is the system font, so 0 is a
// meaningful value rather than "off".  Only the clock and the number screen read
// it that way.
#define CONF_FONT_INDEX    0x02
// outlined means "frame around the bar" rather than "frame instead of fill".
#define CONF_OUTLINE_FRAME 0x04
// The field's `align` cell is not text alignment: it is an orientation selector, so the page offers
// Default / Boombox / Rotate 90 rather than Left / Centre / Right.  Only vuConf has it today.
#define CONF_ALIGN_ORIENT  0x08

// A cell the master's struct carries but this field never reads, as a bit per index into its own cell list.
// `fontsize` on a FillConfig - and on the VU, whose cell 2 is a presence marker rather than a size - is one
// of these.  The conf still writes the cell because the struct has it; the editor labels it "unused" and
// greys it instead of inviting an edit that could not matter.
#define CONF_CELL_BIT(i) (1u << (i))

// ---------------------------------------------------------------------------
// Groups — the section headers struct LayoutData already carries
// ---------------------------------------------------------------------------
// They keep a 32-card page readable.  A card in the wrong group is a display bug
// and never a data bug.

const char* const _confGroups[] = {
    "SCROLLS", "SLIDER BARS", "LINES + RECTANGLES", "WIDGETS", "NUMBERS FONT",
    "VU BARS WIDGET", "CODEC BADGE", "VU BANDS", "MOVES", "TRANSFORMS",
};
#define CONF_GROUP_COUNT (sizeof(_confGroups) / sizeof(_confGroups[0]))

enum confGroup_e : uint8_t {
  CG_SCROLLS = 0, CG_SLIDERS, CG_RECTS, CG_WIDGETS, CG_NUMBERS,
  CG_VUS, CG_CODEC, CG_BANDS, CG_MOVES, CG_SWITCHES,
};
static_assert(CONF_GROUP_COUNT == CG_SWITCHES + 1,
              "a group was added to _confGroups[]: name it in confGroup_e too");

// ---------------------------------------------------------------------------
// Cells — the leading four repeat across the widget-shaped types on purpose:
// each list is the whole shape, so one array per field can be walked alone.
// ---------------------------------------------------------------------------

const ConfCell _confCellsWidget[] = {
  { "offset",   "Offset",    CK_NUM },
  { "top",      "Top",       CK_NUM },
  { "fontsize", "Font size", CK_NUM },
  { "align",    "Align",     CK_ALIGN },
};

const ConfCell _confCellsScroll[] = {
  { "offset",      "Offset",       CK_NUM },
  { "top",         "Top",          CK_NUM },
  { "fontsize",    "Font size",    CK_NUM },
  { "align",       "Align",        CK_ALIGN },
  { "width",       "Width",        CK_NUM },
  { "scrolldelay", "Scroll delay", CK_NUM },
  { "scrolldelta", "Scroll delta", CK_NUM },
  { "scrolltime",  "Scroll time",  CK_NUM },
};

const ConfCell _confCellsFill[] = {
  { "offset",   "Offset",    CK_NUM },
  { "top",      "Top",       CK_NUM },
  { "fontsize", "Font size", CK_NUM },
  { "align",    "Align",     CK_ALIGN },
  { "width",    "Width",     CK_NUM },
  { "height",   "Height",    CK_NUM },
  { "outlined", "Outlined",  CK_BOOL },
};

const ConfCell _confCellsBitrate[] = {
  { "offset",    "Offset",    CK_NUM },
  { "top",       "Top",       CK_NUM },
  { "fontsize",  "Font size", CK_NUM },
  { "align",     "Align",     CK_ALIGN },
  { "dimension", "Dimension", CK_NUM },
};

const ConfCell _confCellsBands[] = {
  { "width",     "Band width",           CK_NUM },
  { "height",    "Band height",          CK_NUM },
  { "space",     "Gap between bars",     CK_NUM },
  { "vspace",    "Gap between segments", CK_NUM },
  { "perheight", "Segments wanted",      CK_NUM },
};

const ConfCell _confCellsMove[] = {
  { "left",  "Left",  CK_NUM },
  { "top",   "Top",   CK_NUM },
  { "width", "Width (0 keeps the conf's own span)", CK_NUM },
};

#define CONF_CELLS_OF(list) (sizeof(list) / sizeof(list[0]))

const ConfCell* const _confCells[] = {
    _confCellsWidget, _confCellsScroll, _confCellsFill,
    _confCellsBitrate, _confCellsBands, _confCellsMove, nullptr,
};
const uint8_t _confCellCounts[] = {
    (uint8_t)CONF_CELLS_OF(_confCellsWidget),
    (uint8_t)CONF_CELLS_OF(_confCellsScroll),
    (uint8_t)CONF_CELLS_OF(_confCellsFill),
    (uint8_t)CONF_CELLS_OF(_confCellsBitrate),
    (uint8_t)CONF_CELLS_OF(_confCellsBands),
    (uint8_t)CONF_CELLS_OF(_confCellsMove),
    0,
};
static_assert(sizeof(_confCells) / sizeof(_confCells[0]) == CT_COUNT,
              "a confType_e was added: give it a cell list and a count");

// ---------------------------------------------------------------------------
// Fields — every member of struct LayoutData, in declaration order
// ---------------------------------------------------------------------------
// That order is load-bearing twice: it is the order of the designated
// initialisers in every conf file, and the order of the cards on the page.

const ConfField _confFields[] = {
  // SCROLLS
  { "metaConf",       "Station name / status",        CT_SCROLL,  CG_SCROLLS,  CONF_REQUIRED,      offsetof(LayoutData, metaConf) },
  { "title1Conf",     "Title line 1",                 CT_SCROLL,  CG_SCROLLS,  0,                  offsetof(LayoutData, title1Conf) },
  { "title2Conf",     "Title line 2",                 CT_SCROLL,  CG_SCROLLS,  0,                  offsetof(LayoutData, title2Conf) },
  { "playlistConf",   "Playlist text",                CT_SCROLL,  CG_SCROLLS,  CONF_REQUIRED,      offsetof(LayoutData, playlistConf) },
  { "weatherConf",    "Weather line",                 CT_SCROLL,  CG_SCROLLS,  0,                  offsetof(LayoutData, weatherConf) },
  // SLIDER BARS
  { "volbarConf",     "Volume slider",                CT_FILL,    CG_SLIDERS,  CONF_OUTLINE_FRAME, offsetof(LayoutData, volbarConf),     CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  { "bufferbarConf",  "Stream buffer bar",            CT_FILL,    CG_SLIDERS,  CONF_OUTLINE_FRAME, offsetof(LayoutData, bufferbarConf),  CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  // LINES + RECTANGLES
  { "metaBGConf",     "Title band / rule",            CT_FILL,    CG_RECTS,    0,                  offsetof(LayoutData, metaBGConf),     CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  { "metaBGConfInv",  "Title band / rule (when title invert on)",
                                                      CT_FILL,    CG_RECTS,    0,                  offsetof(LayoutData, metaBGConfInv),  CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  { "underLineConf",  "Under line (behind the page)", CT_FILL,    CG_RECTS,    0,                  offsetof(LayoutData, underLineConf),  CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  { "overLineConf",   "Over line (on top of it)",     CT_FILL,    CG_RECTS,    0,                  offsetof(LayoutData, overLineConf),   CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  { "playlBGConf",    "Playlist row highlight",       CT_FILL,    CG_RECTS,    0,                  offsetof(LayoutData, playlBGConf),    CONF_CELL_BIT(2) | CONF_CELL_BIT(3) },
  // WIDGETS
  { "bitrateConf",    "Bitrate text",                 CT_WIDGET,  CG_WIDGETS,  0,                  offsetof(LayoutData, bitrateConf) },
  { "voltxtConf",     "Volume number",                CT_WIDGET,  CG_WIDGETS,  0,                  offsetof(LayoutData, voltxtConf) },
  { "batteryConf",    "Battery",                      CT_WIDGET,  CG_WIDGETS,  0,                  offsetof(LayoutData, batteryConf) },
  { "iptxtConf",      "IP address",                   CT_WIDGET,  CG_WIDGETS,  0,                  offsetof(LayoutData, iptxtConf) },
  { "rssiConf",       "WiFi signal",                  CT_WIDGET,  CG_WIDGETS,  0,                  offsetof(LayoutData, rssiConf) },
  // NUMBERS FONT
  { "numConf",        "Large Number (Volume, etc.)",  CT_WIDGET,  CG_NUMBERS,  CONF_FONT_INDEX,    offsetof(LayoutData, numConf) },
  { "clockConf",      "Clock",                        CT_WIDGET,  CG_NUMBERS,  CONF_FONT_INDEX,    offsetof(LayoutData, clockConf) },
  // VU BARS WIDGET
  { "vuConf",         "VU meter position",            CT_WIDGET,  CG_VUS,      CONF_ALIGN_ORIENT,  offsetof(LayoutData, vuConf),         CONF_CELL_BIT(2) },
  // CODEC BADGE
  // The badge reads `offset`, `top` and `dimension` and never `align` - the centring inside it is its own
  // square - so that cell is marked unused like a fill's, and the editor greys it rather than offering a
  // setting the widget cannot honour.
  { "fullbitrateConf","Codec badge",                  CT_BITRATE, CG_CODEC,    0,                  offsetof(LayoutData, fullbitrateConf), CONF_CELL_BIT(3) },
  // VU BANDS
  { "bandsConf",      "VU bar shape",                 CT_BANDS,   CG_BANDS,    0,                  offsetof(LayoutData, bandsConf) },
  // MOVES
  { "clockMove",      "Clock move (stream playing, VU on)",
                                                      CT_MOVE,    CG_MOVES,    0,                  offsetof(LayoutData, clockMove) },
  { "weatherMove",    "Weather move (stream playing, VU off)",
                                                      CT_MOVE,    CG_MOVES,    0,                  offsetof(LayoutData, weatherMove) },
  { "weatherMoveVU",  "Weather move (stream playing, VU on)",
                                                      CT_MOVE,    CG_MOVES,    0,                  offsetof(LayoutData, weatherMoveVU) },
  // TRANSFORMS
  { "fullClock",      "Clock: divider and day/date",  CT_BOOL,    CG_SWITCHES, 0,                  offsetof(LayoutData, fullClock) },
  { "seconds",        "Clock: seconds block",         CT_BOOL,    CG_SWITCHES, 0,                  offsetof(LayoutData, seconds) },
  { "shareWeatherIP", "IP and weather share a row",   CT_BOOL,    CG_SWITCHES, 0,                  offsetof(LayoutData, shareWeatherIP) },
  { "shareBattRSSI",  "RSSI and battery share a row", CT_BOOL,    CG_SWITCHES, 0,                  offsetof(LayoutData, shareBattRSSI) },
  { "rssiDigit",      "Signal drawn as a number",     CT_BOOL,    CG_SWITCHES, 0,                  offsetof(LayoutData, rssiDigit) },
};
#define CONF_FIELD_COUNT (sizeof(_confFields) / sizeof(_confFields[0]))

static_assert(CONF_FIELD_COUNT == LAYOUT_FIELD_COUNT,
              "LayoutData gained or lost a field: update _confFields[] and LAYOUT_FIELD_COUNT together");

// ---------------------------------------------------------------------------
// The build's own constants
// ---------------------------------------------------------------------------
// The conf file's macros under names that say which file they came from, for the
// editor page to EXPLAIN and VALIDATE a number and never to store notation.  The
// guard is on DSP_TFT / DSP_OLED because those are what say a conf file was
// included; with no display there is no TFT_BORDER or MAX_WIDTH to read.

#if defined(DSP_TFT) || defined(DSP_OLED)
  #define CONF_HAS_CONF_MACROS 1
  #define CONF_DSP_WIDTH       (DSP_WIDTH)
  #define CONF_DSP_HEIGHT      (DSP_HEIGHT)
  #define CONF_FRAMEWDT        (TFT_BORDER) // the inset that keeps text off the panel edge
  #define CONF_MAX_WIDTH       (MAX_WIDTH)    // the usable width inside that inset
  #define CONF_SCROLLDELAY     (SCROLLDELAY)
  #define CONF_SCROLLTIME      (SCROLLTIME)
  #define CONF_VU_MAX_WIDTH    (VU_MAX_WIDTH)  // the meter's pixel budget, so the page can explain a clamped box
  #define CONF_VU_MAX_HEIGHT   (VU_MAX_HEIGHT)
  #ifdef BOOTLOGOTOP
    #define CONF_BOOTLOGOTOP   (BOOTLOGOTOP)  // reported for context, never edited per layout
  #else
    #define CONF_BOOTLOGOTOP   (0)
  #endif
#else
  #define CONF_HAS_CONF_MACROS 0
  #define CONF_DSP_WIDTH       0
  #define CONF_DSP_HEIGHT      0
  #define CONF_FRAMEWDT        0
  #define CONF_MAX_WIDTH       0
  #define CONF_SCROLLDELAY     0
  #define CONF_SCROLLTIME      0
  #define CONF_VU_MAX_WIDTH    0
  #define CONF_VU_MAX_HEIGHT   0
  #define CONF_BOOTLOGOTOP     0
#endif

#endif // conf_index_h
