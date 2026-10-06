#include "../../core/options.h"

// The one translation unit that DEFINES the clock fonts — the same arrangement as
// displays/fonts/fonts.cpp, and for the same measured reason: a namespace-scope const has internal
// linkage, so a header of font data included by more than one translation unit puts its own copy of
// the array in flash in each one, and dspfont.h is reached by two (three on the ILI9488 family).
// Twenty-one clock fonts duplicated that way would be tens of kilobytes of nothing.
//
// So: the three generated style headers are included here and nowhere else, and dspfont.h declares
// the three ClockFontStyle instances extern.  To add a style, draw its folder and run
//   py makefont.py <Style>
// then add one line here and one row to the table in dspfont.h.

#include "LEDClock.h"
#include "Chunky6px.h"
#include "Chunky6.h"
