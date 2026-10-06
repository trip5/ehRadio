#include "../../core/options.h"

// The one translation unit that DEFINES the system fonts.  Every one of the font headers declares its data
// as a namespace-scope const array, which has internal linkage in C++, so a TU that includes the header gets
// its own private copy in flash - and dspfont.h is included by more than one TU, which meant the whole font
// set was in the image twice (~39 KB for these three, measured with nm on the linked ELF; see
// plans/font-overhaul.md).  Including them here and nowhere else leaves exactly one copy, and dspfont.h
// declares them extern for everyone else.

#include "MatrixLight/MatrixLight8x6.h"
#include "MatrixChunky/MatrixChunky8x6.h"
#include "X11/UnixX11_6x9.h"
