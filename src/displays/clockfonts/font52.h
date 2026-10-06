#ifndef font_h
#define font_h

#if CLOCKFONT == YO_MONO
  #include "DS_DIGI/DS_DIGI42pt7b_mono.h"
#elif CLOCKFONT == CHUNKY6_PX
  #include "Trip5/Chunky6px_52.h"
#elif CLOCKFONT == CHUNKY6
  #include "Trip5/Chunky6_52.h"
#elif CLOCKFONT == LED
  #include "Trip5/LED_52.h"
#endif

#endif
