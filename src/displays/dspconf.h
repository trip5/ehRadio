#ifndef dspconf_h
#define dspconf_h
#pragma once

// ==========================================================================
// dspconf.h — Centralized display configuration file selection
// ==========================================================================
// Selects the appropriate conf/display*conf.h based on:
//   DSP_WIDTH, DSP_HEIGHT  — resolution (set in options.h, overridable in myoptions.h)
//   DSP_OLED               — display category (set in dspcore.h)
//   DSP_MODEL              — for special cases (GC9A01A round)
//
// TFT (PSFBUFFER) is the default fallback when DSP_OLED is not defined.
// ==========================================================================

// The hold before each scroll begins moving, per kind of line.
#ifndef SCROLLDELAY
  #define SCROLLDELAY 5000
#endif
#ifndef SCROLLDELAY_META
  #define SCROLLDELAY_META SCROLLDELAY
#endif
#ifndef SCROLLDELAY_TITLES
  #define SCROLLDELAY_TITLES SCROLLDELAY
#endif
#ifndef SCROLLDELAY_PLAYLIST
  #define SCROLLDELAY_PLAYLIST (SCROLLDELAY / 5)
#endif
#ifndef SCROLLDELAY_WEATHER
  #define SCROLLDELAY_WEATHER 0
#endif
#ifndef SCROLLDELAY_BOOT
  #define SCROLLDELAY_BOOT 0
#endif

// The share of the display task one scrolling line may take, in percent... higher is more smooth.
// This barely affects the VU FPS so lowering the number results in jitter.
#ifndef SCROLL_DUTY
  #define SCROLL_DUTY 90
#endif

// SCROLLPX is the speed in PIXELS PER SECOND AT FONTSIZE 1: one column of a fontsize-1 glyph is CHARWIDTH = 6 px, so 52
// is 8.7 of those columns a second.  Every larger size scales it by sqrt(textsize) - 100 / 141 / 173 / 200 / 224 % for
// sizes 1..5 - because the cell grows linearly with the size while the eye reads motion in pixels, so a square root
// keeps small text at a readable pixel speed without racing it.
//
// It then grows a little with the PANEL's width, because the same px/s replaces the content of the scrolling row far
// sooner on a small screen: ~1.8 s on a 110 px row against ~7.7 s on a 460 px one, which reads as too fast and too slow
// respectively.  The two ends are calibrated on those two panels by eye, and everything in between interpolates:
// 52 at 128 wide (128x64 / 128x32 OLED, 128x128 and 160x80 TFT), 54 at 240, 56 at 320, 58 at 428, 60 at 480.  Affine
// rather than proportional on purpose - a readable character rate barely depends on the panel, and only the
// sluggishness term grows with the row.  DSP_WIDTH is the configured width, before rotation, i.e. the space layouts
// are authored in.
//
// The step and the redraw pitch are then DERIVED in ScrollWidget::_applyPace().  There is no pitch knob: a step is whole
// pixels so no tick ever moves zero pixels, and the pitch follows from it to keep the speed exact.
//
// This can be overridden in myoptions.h and is not to be considered a perfect scale.-
#ifndef SCROLLPX
  #define SCROLLPX (52 + (DSP_WIDTH - 128) / 44)
#endif

#if defined(DSP_OLED)
  /* ----- Monochrome displays ----- */
  // Wire cost per pixel, used only until the widget has measured its own step.  1-bit at 8 MHz is a fraction of a
  // microsecond, so this only needs to be in the right order of magnitude.
  #ifndef SCROLL_PUSH_US_PER_PX
    #define SCROLL_PUSH_US_PER_PX 0.02f
  #endif
  #if DSP_WIDTH==256 && DSP_HEIGHT==64
    #include "conf/displayOLED256x64conf.h"
  #elif DSP_WIDTH==128 && DSP_HEIGHT==128
    #include "conf/displayOLED128x128conf.h"
  #elif DSP_WIDTH==128 && DSP_HEIGHT==64
    #include "conf/displayOLED128x64conf.h"
  #elif DSP_WIDTH==128 && DSP_HEIGHT==32
    #include "conf/displayOLED128x32conf.h"
  #else
    #error "Unsupported OLED resolution! Must be 256x64, 128x128, 128x64, or 128x32."
  #endif
#else
  /* ----- TFT / color displays (default) ----- */
  // Measured on an ILI9488 at 40 MHz in RGB888: 3 bytes a pixel, ~0.85 us each for the blit plus ~0.1 for the fill.
  #ifndef SCROLL_PUSH_US_PER_PX
    #define SCROLL_PUSH_US_PER_PX 0.95f
  #endif
  #if DSP_WIDTH==480 && DSP_HEIGHT==320
    #include "conf/displayTFT480x320conf.h"
  #elif DSP_WIDTH==428 && DSP_HEIGHT==142
    #include "conf/displayTFT428x142conf.h"
  #elif DSP_WIDTH==320 && DSP_HEIGHT==240
    #include "conf/displayTFT320x240conf.h"
  #elif DSP_WIDTH==284 && DSP_HEIGHT==76
    #include "conf/displayTFT284x76conf.h"
  #elif DSP_WIDTH==240 && DSP_HEIGHT==240
    #if DSP_MODEL==DSP_GC9A01A
      #include "conf/displayTFT240x240roundconf.h"
    #else
      #include "conf/displayTFT240x240conf.h"
    #endif
  #elif DSP_WIDTH==220 && DSP_HEIGHT==176
    #include "conf/displayTFT220x176conf.h"
  #elif DSP_WIDTH==160 && DSP_HEIGHT==128
    #include "conf/displayTFT160x128conf.h"
  #elif DSP_WIDTH==160 && DSP_HEIGHT==80
    #include "conf/displayTFT160x80conf.h"
  #elif DSP_WIDTH==128 && DSP_HEIGHT==128
    #include "conf/displayTFT128x128conf.h"
  #else
    #error "Unsupported TFT resolution! Must be 480x320, 428x142, 320x240, 284x76, 240x240, 220x176, 160x128, 160x80, or 128x128."
  #endif
#endif

#endif // dspconf_h
