#ifndef widget_vu_h
#define widget_vu_h
#if DSP_MODEL!=DSP_DUMMY
// The VU lives apart from widgets.h because it is the one widget with its own canvas branch for TFT, its own inert
// stubs for character LCDs, and - since the runtime style selector - several draw paths over one box. widgets.h does
// NOT include this; display.cpp includes it explicitly, which keeps the two headers acyclic.
#include "widgets.h"

// Longest history the strip can ask for: the long axis of the biggest shipped box (200 px) divided by the smallest
// column width (VU_HISTORY_MIN_PX). Sized at compile time so the widget needs no heap.
#define VU_HISTORY_MAX 100

class VuWidget: public Widget {
  public:
    VuWidget() {}
        VuWidget(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t vupeakcolor, uint16_t bgcolor, uint16_t vuaxiscolor)
            { init(wconf, bands, vumaxcolor, vumincolor, vupeakcolor, bgcolor, vuaxiscolor); }
    ~VuWidget();
    using Widget::init;
        void init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t vupeakcolor, uint16_t bgcolor, uint16_t vuaxiscolor);
    void loop();
  protected:
    #if defined(DSP_TFT)
      Canvas *_canvas = nullptr;
    #endif
    VUBandsConfig _bands;
    uint16_t _vumaxcolor, _vumincolor, _vupeakcolor;
    uint16_t _vuaxiscolor;
    // High-water marks, measured as cleared pixels from the loud end. 0xFFFF means
    // "not set yet" and is adopted on the first _levels() call, once len is known.
    uint16_t _peakL = 0xFFFF, _peakR = 0xFFFF;
    uint32_t _lastMs = 0;                    // last call to _levels(), for the fade time base
    uint32_t _redrawMs = 0;                  // last redraw start, for the duty limiter below
    uint32_t _intervalMs = VU_REFRESH_MS;    // current gap between redraws, set from the draw cost
    uint32_t _drawUs = 0;                    // smoothed cost of one _draw(), in microseconds
    uint32_t _accL = 0, _accR = 0;           // sub-pixel carry for the bar fade, in px*ms
    uint32_t _accPL = 0, _accPR = 0;         // same for the peak markers
    uint32_t _holdL = 0, _holdR = 0;         // when each peak marker was last re-armed (VU_PEAK_FREEZE_MS)
    bool _rotate = false;

    // Geometry of the current frame. _draw() resolves it once and every style reads it: _len/_thk are the per-channel
    // level axis and cross-section, _cw/_ch the whole box, and _measL/_measR are what _levels() produced (the length
    // CLEARED from the loud end, so louder is a SMALLER value).
    uint16_t _len = 0, _thk = 0, _cw = 0, _ch = 0;
    uint16_t _measL = 0, _measR = 0;

    void _draw();
    void _levels(uint16_t len, uint16_t &measL, uint16_t &measR);
    void _drawBand(uint16_t pos, uint8_t ch, uint16_t h, uint16_t color);
    // One painter per style, chosen by config.store.vustyle in _draw(). They all draw into the same area, so they
    // share _fillLocal() and the geometry above; _draw() blits once, at the end. The bar family is the only one that
    // cares how a layout arranges its two channels: it draws two strips where bandsConf says they are. Every other
    // style draws time or frequency along the AREA's width and splits the area between the channels, so the OLED,
    // the portrait TFT box and the BoomBox ribbon all get the same picture.
    bool _fillLocal(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
    // Thickness of a reference line and of a trace, from the peak marker's own formula. Not used for bar widths, and
    // not retrofitted onto the segmented VU, or the look would change.
    uint16_t _stroke() const;
    // The meter's calibration as a display gain, in 8.8 fixed point where 256 is unity. The sample styles measure
    // against the same reference the level bars and the spectrum do.
    uint16_t _waveGain() const;
    // The graticule the waveform and the Lissajous plot against, in vupeak, 1 px, painted first.
    void _centreCross();
    // vumin for the body, vumax once an element reaches into the outer HOTSEG segments of its own axis. `len` and
    // `full` are in that axis' pixels, so "hot" means the same thing in every style.
    uint16_t _hotColor(uint16_t len, uint16_t full);
    uint16_t _bandColor(uint16_t lvl);
    uint8_t _bandCount(uint16_t span, uint16_t *barW, uint16_t *gap) const;
    void _synthBands(uint8_t *out, uint8_t n, uint16_t lvl);
    void _drawBars(bool led);
    void _drawHistory();
    void _drawSpectrumReflect();
    void _drawSpectrumMirror();
    void _drawWave();
    void _drawLissajous();

    // The trace scratch that used to sit here is now a file-static in widget_vu.cpp, so it comes out of .bss instead
    // of the heap - widgets are heap-allocated, and the heap is what the boot-time services exhaust. The spectrum's
    // per-band caps are gone: too faint to read inside a bar and costing pixels the bars need, so the vupeak switch
    // now means something else in that style (see _drawSpectrumReflect).

    // History strip: one column per VU_REFRESH_MS of wall clock, oldest first, each level scaled to 0..255 (0 =
    // silent) so that even a 404 px level axis fits in a byte.
    uint8_t _histL[VU_HISTORY_MAX] = {0}, _histR[VU_HISTORY_MAX] = {0};
    uint32_t _histMs = 0;                    // when the strip last advanced a column
    uint16_t _frame = 0;                     // redraws so far, drives the simulated wobble
    void _clear();
    void _reset();

    #ifdef WIDGET_DEBUG
      // Draw-cost report, emitted from loop() every 5 s like the Core Monitor. These members exist only in a debug
      // build, so a normal one pays neither the RAM nor the counting.
      uint32_t _dbgFrames = 0, _dbgFills = 0, _dbgDrawUs = 0, _dbgPeakUs = 0, _dbgLogMs = 0;
      uint32_t _fills = 0;                   // fills drawn by the frame now being drawn
    #endif
};

#endif // #if DSP_MODEL!=DSP_DUMMY
#endif // #ifndef widget_vu_h
