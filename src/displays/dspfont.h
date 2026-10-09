#ifndef dspfont_h
#define dspfont_h
#pragma once

// ==========================================================================
// dspfont.h — Centralized font, bootlogo, and TIME_SIZE selection
// ==========================================================================
// Selects bootlogo, clock font, and TIME_SIZE based on DSP_HEIGHT.
// DSP_WIDTH only used to differentiate 160x128 (wide) from 128x128 (narrow).
// DSP_OLED/DSP_TFT macros from dspcore.h used where OLED needs smaller fonts.
// ==========================================================================

/* --- SYSTEM FONTS (Unicode GFXfont, 6x8 cell) --- */
// One folder per font - fonts/<Name>/<Name>.h, converted from the .bdf beside it by bdf2adafruit3.py - and
// every one of them is compiled in.  Which font is used is a runtime setting, not a build choice, so the
// macro below is only the boot default (DISPLAYFONT).
//
// DECLARED here, DEFINED in fonts/fonts.cpp, and that is the whole point: a namespace-scope const array has
// internal linkage, so any translation unit that includes a font header gets its own copy in flash.  Including
// them from dspfont.h put the entire font set in the image twice - measured with nm on the linked ELF, ~39 KB
// for these three - and it would have doubled the clock fonts as well.  One defining unit, many declarations.
#include <Adafruit_GFX.h>
extern const GFXfont MatrixLight8x6;
extern const GFXfont MatrixChunky8x6;
extern const GFXfont Fixed;
extern const GFXfont BmPlus_HP_100LX_6x8;
extern const GFXfont Bm437_ATI_SmallW_6x8;
#if DISPLAYFONT > X11
  #warning "DISPLAYFONT value not recognized, defaulting to MATRIXCHUNKY"
  #undef DISPLAYFONT
  #define DISPLAYFONT MATRIXCHUNKY
#endif

// The index the WebUI list and config.store.systemFontId both run on: append only, never renumber, or a
// stored preference lands on a different font.  The names are what /fonts.json shows the user.
const char _systemFontNames[][64] PROGMEM = {
  "Matrix Light",
  "Matrix Chunky",
  "Unix X11",
  "HP 100LX",
  "ATI Small Wonder",
};
const GFXfont* const _systemFonts[] = {
  &MatrixLight8x6,
  &MatrixChunky8x6,
  &Fixed,
  &BmPlus_HP_100LX_6x8,
  &Bm437_ATI_SmallW_6x8,
};
#define _systemFontCount (sizeof(_systemFonts) / sizeof(_systemFonts[0]))

// The id of the font in use.  dspfont.h cannot read config itself - config.h includes options.h, which
// includes this - so the variable lives in display.cpp and displayFont() reads it here.
extern uint8_t activeSystemFontId;

// --- ACTIVE FONT ACCESSOR ---
// The one place the active display font is chosen, so runtime font switching changes only this function - see
// plans/font-overhaul.md for the obligations that come with it. Any font returned here must stay on the
// 6x8 metric class (xAdvance 6, yAdvance 8): widget layout comes from CHARWIDTH/CHARHEIGHT, not from the font.
inline const GFXfont *displayFont() { return _systemFonts[activeSystemFontId]; }

// There is deliberately NO size ladder here.  The clock size used to be TIME_SIZE, a single number
// derived from DSP_HEIGHT for the whole build; it is now the layout's own `clockConf.textsize`, a
// size index, and both the clock and the number page read it from their conf.  The DSP_HEIGHT rules
// were applied once, to every shipped conf, when the index moved there - the per-panel table is in
// plans/font-overhaul.md 2.11 and in the `clockConf` comment in widgetsconfig.h, which is where a
// conf author looks, and where a new layout gets its value from the layout tooling.

/*--- BOOTLOGO ---*/
#if DSP_HEIGHT >= 320 && defined(BIG_BOOT_LOGO) && BIG_BOOT_LOGO
  #include "bootlogo/198x128.h"
#elif DSP_HEIGHT >= 176           // 480x320, 320x240, 240x240, 220x176
  #include "bootlogo/99x64.h"
#elif DSP_WIDTH >= 160 && DSP_HEIGHT >= 128   // 160x128 (wider than 128x128)
  #include "bootlogo/99x64.h"
#elif DSP_HEIGHT >= 76            // 128x128, 284x76, 160x80
  #include "bootlogo/62x40.h"
#elif DSP_HEIGHT >= 64            // 128x64, 256x64
  #include "bootlogo/110x32mono.h"
// 128x32: no bootlogo
#endif

/* ---  CLOCK FONT --- */
// Three styles, all compiled in, chosen at runtime with config.store.clockFontId — the same shape
// as the system fonts above.  Which of a style's sizes is used is the LAYOUT's business
// (clockConf.textsize / numConf.textsize, a size index), so this table is only "which design".
//
// DECLARED here, DEFINED in clockfonts/clockfonts.cpp, for the reason given at the top of that
// file.  To add a style: draw clockfonts/<Style>/{8,15,21,28,35,52,70}.png, run
//   py makefont.py <Style>
// then add one include there and one row below.
#include "clockfonts/fontstyle.h"
extern const ClockFontStyle LEDClockStyle;
extern const ClockFontStyle Chunky6pxStyle;
extern const ClockFontStyle Chunky6Style;
#if CLOCKFONT > CHUNKY6
  #warning "CLOCKFONT value not recognized, defaulting to CHUNKY6"
  #undef CLOCKFONT
  #define CLOCKFONT CHUNKY6
#endif

// Indexed by config.store.clockFontId, whose values are the CLOCKFONT ids from options.h
// (LEDCLOCK 0, CHUNKY6PX 1, CHUNKY6 2) — keep the two in step, or a stored preference lands on a
// different design.  The names are what /clockfonts.json shows the user.
const ClockFontStyle* const _clockFontStyles[] = {
  &LEDClockStyle,
  &Chunky6pxStyle,
  &Chunky6Style,
};
#define _clockFontCount (sizeof(_clockFontStyles) / sizeof(_clockFontStyles[0]))

// The id of the style in use — assigned from display.cpp for the same reason activeSystemFontId
// is, and the accessor below is the one place the choice is made, so a live switch changes no
// drawing code.
extern uint8_t activeClockFontId;
inline const ClockFontStyle &clockFontStyle() {
  return *_clockFontStyles[activeClockFontId < _clockFontCount ? activeClockFontId : 0];
}

// ClockFontStyle::sizes[] is the seven PNGs in ladder order — 8, 15, 21, 28, 35, 52, 70 — so a
// size from makefont.py is a slot in it.  A LAYOUT's size index is 0..4 instead: 0 is the SYSTEM
// FONT (no clock font at all — the 128x32 panel), and 1..4 are the four sizes we offer.  The
// clock's four PNGs and the seconds' four are the same file in different slots, which is why the
// slot and the pixel size are separate lookups and both take the same index.
inline uint8_t clockSizePx(uint8_t i) {     // index -> 0, 15, 35, 52, 70
  switch (i) { case 1: return 15; case 2: return 35; case 3: return 52; case 4: return 70; default: return 0; }
}
inline uint8_t secondsSizePx(uint8_t i) {   // index -> 0, 10, 15, 21, 28
  switch (i) { case 1: return 10; case 2: return 15; case 3: return 21; case 4: return 28; default: return 0; }
}
inline int8_t clockSizeSlot(uint8_t i) {    // index -> sizes[] slot, -1 for the system font
  switch (i) { case 1: return 1; case 2: return 4; case 3: return 5; case 4: return 6; default: return -1; }
}
inline int8_t secondsSizeSlot(uint8_t i) {  // index -> sizes[] slot, -1 for none
  return (i >= 1 && i <= 4) ? (int8_t)(i - 1) : -1;
}

// The clock font a size index resolves to, or nullptr when it is the system font — the test the
// widget code has always made against a null clock font pointer.
inline const GFXfont *clockFontFor(uint8_t i) {
  int8_t slot = clockSizeSlot(i);
  return slot < 0 ? nullptr : clockFontStyle().sizes[slot];
}
// The same for the seconds block.  nullptr means this style has no sibling at that size and the
// widget falls back to the built-in font.
inline const GFXfont *secondsFontFor(uint8_t i) {
  int8_t slot = secondsSizeSlot(i);
  return slot < 0 ? nullptr : clockFontStyle().sizes[slot];
}

#endif // dspfont_h
