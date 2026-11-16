#include "../../core/options.h"

// The one translation unit that DEFINES the system fonts.  Each font header declares a namespace-scope const
// array, which has internal linkage in C++, so every TU that includes it gets a private copy in flash.  Including
// them here and nowhere else leaves exactly one copy; dspfont.h declares them extern for everyone else.

#include "MatrixLight/MatrixLight8x6.h"
#include "MatrixChunky/MatrixChunky8x6.h"
#include "UnixX11/UnixX11_6x9.h"
#include "HP100LX/BmPlus_HP_100LX_6x8.h"
#include "ATISmallW/Bm437_ATI_SmallW_6x8.h"
