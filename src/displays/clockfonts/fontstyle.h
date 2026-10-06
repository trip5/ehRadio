#ifndef clockfontstyle_h
#define clockfontstyle_h
#pragma once

// ==========================================================================
// fontstyle.h — one clock font STYLE, as makefont.py emits it
// ==========================================================================
// A style is a folder of seven PNGs (clockfonts/<Style>/{8,15,21,28,35,52,70}.png) and one
// generated header.  makefont.py writes the header, and it writes one of these at the end of
// it so dspfont.h only has to name the style, not list its seven fonts:
//
//   extern const ClockFontStyle LEDStyle;
//
// dspfont.h collects the three into _clockFontStyles[] and picks with
// config.store.clockFontId, exactly as _systemFonts[] picks with systemFontId.
//
// sizes[] is in LADDER ORDER, ascending — 8, 15, 21, 28, 35, 52, 70 — never reorder it and never
// drop a size: a style is all seven or the tool refuses to build it, and dspfont.h's
// clockSizeSlot() / secondsSizeSlot() are slot numbers into this array.  A layout's size index
// (clockConf.textsize) selects the clock sizes 15/35/52/70 and the seconds sizes 8/15/21/28.
//
// glowChar is this style's own character, not a macro: all three styles are compiled in at once and
// the style is chosen at runtime, so no compile-time constant can describe "the selected font".
// It is ONE character because that is all the tool actually knows — which glyph slot it filled with
// the union of the lit digit pixels.  The shapes drawn with it are the widget's business:
//
//   the clock    <g><g>:<g><g>   — which is also the clock's WIDTH TEMPLATE, since the union glyph
//                                  carries the digit's xAdvance
//   the seconds  <g><g>
//
// so this field has to be present even when the glow effect is switched off: on a TFT it is what the
// clock's width is measured with, and on a mono panel, where the glow cannot be drawn at all, it is
// the only thing it is used for.

#include <Adafruit_GFX.h>

struct ClockFontStyle {
  const char    *name;          // shown in the WebUI list; a proper noun, not translated
  const GFXfont *sizes[7];      // ladder order: 8, 15, 21, 28, 35, 52, 70
  const char    *glowChar;      // the one character the glow glyph was injected into, e.g. "/"
};

#endif // clockfontstyle_h
