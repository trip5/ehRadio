#ifndef dspstats_h
#define dspstats_h
#pragma once

#include <stdint.h>

// ==========================================================================
// dspstats.h — display-subsystem counters for the Core Monitor
// ==========================================================================
// The Core Monitor in core/main.cpp reports how often each of its two counted
// tasks runs, but a rate alone says nothing about WHAT the display task spent
// the time on.  These counters answer that, and they exist only while
// CORE_MONITOR is defined: the helpers below compile to nothing otherwise, so
// a build with the monitor disabled pays nothing at all.
//
// All of them are written by the display task and read (and reset) by the main
// loop, so they are volatile for the same reason cmDspLoopCount is.  The
// definitions live in core/display.cpp beside cmDspLoopCount.
//
//   cmGlyphCount     glyphs drawn — the renderer's real unit of work
//   cmPreTextCalls   codepoints that needed resolving (the cheap early-outs in
//                    preText() are not counted; they decide nothing)
//   cmPreTextHits    those answered without walking the fold chain: the glyph
//                    was present, or the memo already knew the answer
//   cmFillCount      rectangle fills that reached the panel or its buffer —
//                    the class-agnostic "how much drawing happened" figure
//   cmPushCount      framebuffer flushes: OLED display() calls plus TFT PSRAM
//                    region blits.  On TFT only ScrollWidget and ClockWidget
//                    draw through a psFrameBuffer, so this is NOT "the panel
//                    was written" — direct-to-driver drawing never shows here
//   cmDspCore        the core the display task is really running on, recorded
//                    by the task itself so the log can never mislabel it
// ==========================================================================

#ifdef CORE_MONITOR
  extern volatile uint32_t cmDspLoopCount;
  extern volatile uint32_t cmGlyphCount;
  extern volatile uint32_t cmPreTextCalls;
  extern volatile uint32_t cmPreTextHits;
  extern volatile uint32_t cmFillCount;
  extern volatile uint32_t cmPushCount;
  extern volatile uint8_t  cmDspCore;

  // The task passes its own core in, so the figure can never be mislabelled even if the task is pinned elsewhere.
  static inline void cmCountDspLoop(uint8_t core) { cmDspLoopCount++; cmDspCore = core; }
  static inline void cmCountGlyph()       { cmGlyphCount++; }
  static inline void cmCountPreTextCall() { cmPreTextCalls++; }
  static inline void cmCountPreTextHit()  { cmPreTextHits++; }
  static inline void cmCountFill()        { cmFillCount++; }
  static inline void cmCountPush()        { cmPushCount++; }
#else
  static inline void cmCountDspLoop(uint8_t core) { }
  static inline void cmCountGlyph()       { }
  static inline void cmCountPreTextCall() { }
  static inline void cmCountPreTextHit()  { }
  static inline void cmCountFill()        { }
  static inline void cmCountPush()        { }
#endif

#endif // dspstats_h
