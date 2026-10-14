#ifndef widgetsconfig_h
#define widgetsconfig_h

#include <stdint.h>

enum WidgetAlign { WA_LEFT, WA_CENTER, WA_RIGHT };
enum BitrateFormat { BF_UNKNOWN, BF_MP3, BF_AAC, BF_FLAC, BF_WAV, BF_VOR, BF_OPU };

struct WidgetConfig {
  // The widget's distance from the edge `align` names: the LEFT edge for WA_LEFT (the default), the RIGHT
  // edge for WA_RIGHT, and - for a centred line - the left edge of the span it centres in (or, for the
  // clock, a shift right of centre).  It was called `left`, which was true for only one of the three; see
  // TextWidget::_realLeft() and ClockWidget::_getTimeBounds() for the three branches.  The VU wears the
  // same cell with a different meaning: its `align` is the meter's ORIENTATION, so the pair is its corner.
  uint16_t offset;
  uint16_t top;
  uint16_t textsize;
  // A NUMBER rather than the enum: a conf may write 0 for a cell its type never reads (a fill's `align`), and
  // C++ refuses to convert a bare 0 to an enum.  The WA_LEFT/WA_CENTER/WA_RIGHT names still apply everywhere -
  // an enum converts to an integer - so no reader changed, and this is also the layout the round-1 note asked
  // for: it drops three bytes of padding per field, which is ~800 B across the ten stored slots.
  uint8_t  align;
};

struct ScrollConfig {
  WidgetConfig widget;
  uint16_t width;
  uint16_t startscrolldelay;
  uint8_t scrolldelta;
  uint16_t scrolltime;
};

struct FillConfig {
  WidgetConfig widget;
  uint16_t width;
  uint16_t height;
  bool outlined;
};

struct ProgressConfig {
  uint16_t speed;
  uint16_t width;
  uint16_t barwidth;
};

struct VUBandsConfig {
  uint16_t width;
  uint16_t height;
  uint8_t  space;
  uint8_t  vspace;
  uint8_t  perheight;
  // There is no fade speed here on purpose: the decay rate is derived from the band
  // length and VU_FADE_MS in options.h, so every bar falls in the same time no matter
  // how long it is or which panel it is on.
};

struct MoveConfig {
  uint16_t x;
  uint16_t y;
  int16_t width;
};

// Ids are persisted in config.store.vustyle and are the keys in /visuals.json, so they must never be renumbered: a new
// style is only ever appended, before VU_STYLE_COUNT. The label a style shows the user lives beside the code that
// draws it, in the /visuals.json table in netserver.cpp.
enum vuStyle_e : uint8_t {
  VU_STYLE_BARS = 0,          // the segmented original bar VU inherited from yoRadio - "Bars"
  VU_STYLE_DIGITAL_LED,       // the same painter, lit boundary snapped to whole segments - "Digital LED"
  VU_STYLE_HISTORY,           // level over time, both channels
  VU_STYLE_SPECTRUM_REFLECT,  // bands across the width, L above the baseline and R below it
  VU_STYLE_SPECTRUM_MIRROR,   // half the width each, full height, L mirrored on the left and R on the right
  VU_STYLE_WAVE,              // waveform, I2S only
  VU_STYLE_LISSAJOUS,         // vectorscope, I2S only
  VU_STYLE_COUNT
};

struct BitrateConfig {
  WidgetConfig widget;
  uint16_t dimension;
};

/* DO NOT INSERT COMMENTS IN THE NEXT 2 STRUCT BLOCKS */
/* This section's comments are used by conf_tool.py: a block comment here becomes the section
   header of the field BELOW it, and a field's own // comment is copied out into the conf files.
   Keep every field line as "Type name;" optionally followed by ONE // comment on the same line.
   The tool does not read a block comment there, and the field would then drop out of its master
   list with no warning at all. */

struct BootData {
    /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
    ScrollConfig   apTitleConf;
    ScrollConfig   apSettConf;
    /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
    FillConfig     apTitleBGConf;
    /* WIDGETS             { offset, top, fontsize, align } */
    WidgetConfig   bootstrConf;
    WidgetConfig   apNameConf;
    WidgetConfig   apName2Conf;
    WidgetConfig   apPassConf;
    WidgetConfig   apPass2Conf;
    WidgetConfig   bootWdtConf;
    /* BOOT PROGRESS       { frame interval, line character width, progress characters } */
    ProgressConfig bootPrgConf;
};

struct LayoutData {
    /* SCROLLS             {{ offset, top, fontsize, align }, width, scrolldelay, scrolldelta, scrolltime } */
    ScrollConfig metaConf;
    ScrollConfig title1Conf;
    ScrollConfig title2Conf;
    ScrollConfig playlistConf;
    ScrollConfig weatherConf;
    /* SLIDER BARS         {{ offset, top, 0, 0 }, width, height, outlined } */
    FillConfig   volbarConf;
    FillConfig   bufferbarConf;
    /* LINES + RECTANGLES  {{ offset, top, 0, 0 }, width, height, outlined } */
    FillConfig   metaBGConf;
    FillConfig   metaBGConfInv;
    FillConfig   underLineConf;
    FillConfig   overLineConf;
    FillConfig   playlBGConf;
    /* WIDGETS             { offset, top, fontsize, align } */
    WidgetConfig bitrateConf;
    WidgetConfig voltxtConf;
    WidgetConfig batteryConf;
    WidgetConfig iptxtConf;
    WidgetConfig rssiConf;
    /* NUMBERS FONT        { offset, top, fontsize (1=15/10, 2=35/15, 3=52/21, 4=70/28), align } */
    WidgetConfig numConf;
    WidgetConfig clockConf;
    /* VU BARS WIDGET      { offset, top, 0, align (WA_LEFT=default / WA_CENTER=boombox / WA_RIGHT=rotate 90) } */
    WidgetConfig vuConf;
    /* CODEC BADGE         {{ offset, top, fontsize, align }, dimension} */
    BitrateConfig fullbitrateConf;
    /* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands } */
    VUBandsConfig bandsConf;
    /* MOVES               { left, top, width } */
    MoveConfig   clockMove;
    MoveConfig   weatherMove;
    MoveConfig   weatherMoveVU;
    /* TRANSFORMS          boolean */
    bool         fullClock; // the divider and the day/date column right of the time
    bool         seconds; // the seconds block right of the time
    // There used to be `boomboxVU` and `rotateVU` here.  Both are the VU's ORIENTATION, which is
    // `vuConf.align`: WA_LEFT is the default, WA_CENTER is side-by-side middle-out (boombox) and
    // WA_RIGHT is rotated 90 degrees.  See plans/layout-editor-round4.md.
    bool         shareWeatherIP; // IP and weather share the same row
    bool         shareBattRSSI; // RSSI and battery share the same row
    bool         rssiDigit; // signal drawn as a number, not bars
};

// The number of members in struct LayoutData above.  conf_index.h asserts its
// own field table against this, so adding a field here fails the build there
// rather than silently orphaning a key in every layout file already on disk.
// Bump it in the same edit that adds or removes a field.
#define LAYOUT_FIELD_COUNT 30

// ---------------------------------------------------------------------------
// The VU's pixel budget
// ---------------------------------------------------------------------------
// No box the meter chooses may exceed VU_MAX_WIDTH x VU_MAX_HEIGHT, and the cap engages only above it - so a
// small panel, an OLED and a layout's own box all keep what they are given.  A guarded define, so a build can
// override either.  VU_MAX_HEIGHT is also the Lissajous's square, deliberately half the rectangle's budget
// rather than the same area: an X-Y plot reads right in a square, and a same-area square doubles its pixels and
// halves its frame rate for detail the figure cannot use.  It lives here, beside the structs, because three
// readers need the same two numbers: widget_vu.h (which includes this header), conf_index.h (which lends them to
// the editor page as CONF_VU_MAX_*), and a build with no display, which still compiles this header.
#ifndef VU_MAX_WIDTH
  #define VU_MAX_WIDTH 280
#endif
#ifndef VU_MAX_HEIGHT
  #define VU_MAX_HEIGHT 140
#endif

// ---------------------------------------------------------------------------
// The one presence rule
// ---------------------------------------------------------------------------
// EVERY CELL ZERO MEANS THE FIELD IS ABSENT.  That is the same spelling a conf file uses when it writes
// `{ }`, the same one the editor's per-card checkbox writes, and the same one conf_tool.py applies on the
// way back into a .h file - so all four agree by construction instead of by four copies of the test.
// It lives here, beside the structs, because the layout registry's save check (layoutregistry.cpp) must
// not be able to disagree with the draw-time test in display.cpp.
// MoveConfig is deliberately absent: a zeroed move is a DISPLACEMENT ("stay put"), a value rather than an
// absence, so presentOf() must not read its zeros as "this field is gone".  The confs and the editor still
// SPELL those zeros `{ }` - the spelling and the presence rule are two different questions (round 7).
static inline bool presentOf(const WidgetConfig&  c) { return c.offset || c.top || c.textsize || c.align; }
static inline bool presentOf(const ScrollConfig&  c) { return presentOf(c.widget) || c.width || c.startscrolldelay || c.scrolldelta || c.scrolltime; }
static inline bool presentOf(const FillConfig&    c) { return presentOf(c.widget) || c.width || c.height || c.outlined; }
static inline bool presentOf(const BitrateConfig& c) { return presentOf(c.widget) || c.dimension; }
static inline bool presentOf(const VUBandsConfig& c) { return c.width || c.height || c.space || c.vspace || c.perheight; }

// Layout switching — extern pointer declarations, defined in display.cpp
extern const ScrollConfig* metaConf_ptr;
extern const ScrollConfig* title1Conf_ptr;
extern const ScrollConfig* title2Conf_ptr;
extern const ScrollConfig* playlistConf_ptr;
extern const ScrollConfig* weatherConf_ptr;
extern const FillConfig*   metaBGConf_ptr;
extern const FillConfig*   metaBGConfInv_ptr;
extern const FillConfig*   volbarConf_ptr;
extern const FillConfig*   playlBGConf_ptr;
extern const FillConfig*   bufferbarConf_ptr;
extern const WidgetConfig* bitrateConf_ptr;
extern const WidgetConfig* voltxtConf_ptr;
extern const WidgetConfig* batteryConf_ptr;
extern const WidgetConfig* iptxtConf_ptr;
extern const WidgetConfig* rssiConf_ptr;
extern const WidgetConfig* numConf_ptr;
extern const WidgetConfig* clockConf_ptr;
extern const WidgetConfig* vuConf_ptr;
extern const BitrateConfig*  fullbitrateConf_ptr;
extern const VUBandsConfig*   bandsConf_ptr;
extern const MoveConfig*    clockMove_ptr;
extern const MoveConfig*    weatherMove_ptr;
extern const MoveConfig*    weatherMoveVU_ptr;
extern const bool*          fullClock_ptr;
extern const bool*          seconds_ptr;
extern const bool*          shareWeatherIP_ptr;
extern const bool*          shareBattRSSI_ptr;
extern const bool*          rssiDigit_ptr;
extern const FillConfig*    underLineConf_ptr;
extern const FillConfig*    overLineConf_ptr;

// The layout editor's two temporary switches.  Both are runtime only - a reboot clears them - and neither ever
// writes config.store: they are an editor session's overrides of what the user stored.  System variables rather
// than Display members because the VU painter, in its own translation unit, has to see them too.
//
// Force-on wins, else force-off, else the stored value.  That if/else IS the priority, so the two can never
// collide even if a stale page had both ticked, and the page unticks the other besides.  `editorOpt()` lives here,
// beside the flags and with no knowledge of config: each caller passes the stored value it already has.
extern bool widgetsEditorForceOn;
extern bool widgetsEditorForceOff;
static inline bool editorOpt(bool stored) {
  if (widgetsEditorForceOn)  return true;
  if (widgetsEditorForceOff) return false;
  return stored;
}

// layoutCount() used to be a variable here; the layout registry owns the id set now, because the list
// is the compiled table plus ten slots rather than the table alone.
extern LayoutData activeLayout;

#endif
