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

// The meter's pixel budget: no box the widget chooses may exceed VU_MAX_WIDTH x VU_MAX_HEIGHT, and the cap
// engages only above it - so a small panel, an OLED and a layout's own box all keep what they are given.  A
// guarded define, so a build can override either.  VU_MAX_HEIGHT is also the Lissajous's square, which is
// deliberately half the rectangle's budget rather than the same area: an X-Y plot reads right in a square,
// and a same-area square doubles its pixels and halves its frame rate for detail the figure cannot use.
#ifndef VU_MAX_WIDTH
  #define VU_MAX_WIDTH 280
#endif
#ifndef VU_MAX_HEIGHT
  #define VU_MAX_HEIGHT 140
#endif

// A meter box in panel coordinates, plus the two row heights the bar family needs (zero for every other
// style).  Produced by resolveScreensaverBox(), so the window, the bar ratios and the budget live beside the
// painters that care instead of in display.cpp.
struct VuBox {
  uint16_t left = 0, top = 0, w = 0, h = 0;
  uint16_t barH = 0, gapH = 0;
  bool bars = false;
};

class VuWidget: public Widget {
  public:
    VuWidget() {}
        VuWidget(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t vupeakcolor, uint16_t bgcolor, uint16_t vuaxiscolor)
            { init(wconf, bands, vumaxcolor, vumincolor, vupeakcolor, bgcolor, vuaxiscolor); }
    ~VuWidget();
    using Widget::init;
        void init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t vupeakcolor, uint16_t bgcolor, uint16_t vuaxiscolor);
    void loop();
    // The screensaver's own instance: a box the layout knows nothing about, with the two layout
    // transforms overridden (rotate forced on, boombox ignored) and the style pinned instead of read
    // from config.store.vustyle.  The box arrives already sized and placed by resolveScreensaverBox(),
    // which is also where the bar family's two row heights come from.  Returns false when the canvas
    // could not be allocated, which is the caller's signal to keep the clock screensaver instead.
    bool initScreensaver(const VuBox& box, const VUBandsConfig& layoutBands, uint8_t style);
    // The screensaver's box for a style, from the content area it may use - areaW x areaH at 0,0, with the
    // height already reduced by the info line's strip.  Applies the budget, the bar family's row ratios and
    // the Lissajous's square, and centres whatever it produces.  Static because the box is a property of
    // the panel and the style, not of an instance.
    static VuBox resolveScreensaverBox(uint16_t areaW, uint16_t areaH, uint8_t style);
    // True when the canvas exists.  A TFT widget with no canvas must draw nothing rather than
    // dereference a null buffer, which is what a failed PSRAM allocation has to look like.
    bool ready();
  protected:
    #if defined(DSP_TFT)
      Canvas *_canvas = nullptr;
      // The buffer behind _canvas, held here as well because the library keeps its own pointer protected
      // (GFXcanvas16::getBuffer() is the only reader it offers).  We allocate it, so we are the ones who
      // must know whether it exists - see ready() - and where it came from.
      uint16_t *_canvasBuf = nullptr;
    #endif
    VUBandsConfig _bands;
    uint16_t _vumaxcolor, _vumincolor, _vupeakcolor;
    uint16_t _vuaxiscolor;
    // High-water marks, measured as cleared pixels from the loud end. 0xFFFF means
    // "not set yet" and is adopted on the first _levels() call, once len is known.
    uint16_t _peakL = 0xFFFF, _peakR = 0xFFFF;
    // What the previous frame left on the panel, for the bar family's incremental path (see _drawBars):
    // the cleared length of each channel, where the two peak markers were drawn, which style drew it, and
    // whether the markers were on at all.  0xFFFF / 0xFF mean "no previous frame, repaint everything",
    // which is also what a change in any of the four means - a peak switched off has to erase its own
    // marker, and a style change repaints in a different pattern.  Invalidated by init() and _reset().
    uint16_t _prevMeasL = 0xFFFF, _prevMeasR = 0xFFFF;
    uint16_t _prevPkL = 0xFFFF, _prevPkR = 0xFFFF;
    uint8_t _prevStyle = 0xFF;
    bool _prevVupeak = false;
    uint32_t _lastMs = 0;                    // last call to _levels(), for the fade time base
    uint32_t _redrawMs = 0;                  // last redraw start, for the duty limiter below
    uint32_t _intervalMs = VU_REFRESH_MS;    // current gap between redraws, set from the draw cost
    uint32_t _drawUs = 0;                    // smoothed cost of one _draw(), in microseconds
    // The last frame's three parts, measured separately because they call for completely different
    // fixes: the ERASE is _draw()'s opening wipe of the whole box, the PAINTER is the style's own fills,
    // and the BLIT is the wire. On a full-panel TFT meter one of them dwarfs the others, and which one it
    // is decides whether the answer is a smaller box, fewer fills, or fewer pixels sent. Timed
    // unconditionally - three micros() calls are nothing next to a frame that costs milliseconds - and
    // reported for the screensaver's instance only, by _reportCost().
    uint32_t _eraseUs = 0, _paintUs = 0, _blitUs = 0;
    uint32_t _accL = 0, _accR = 0;           // sub-pixel carry for the bar fade, in px*ms
    uint32_t _accPL = 0, _accPR = 0;         // same for the peak markers
    uint32_t _holdL = 0, _holdR = 0;         // when each peak marker was last re-armed (VU_PEAK_FREEZE_MS)
    bool _rotate = false;
    // 0xFF means "unpinned": the player's box reads config.store.vustyle on every frame, because the
    // vustyle=<n> command only queues SHOWVUMETER and the style has to switch live.  The screensaver's
    // instance is pinned to screensaverVUStyle by initScreensaver() instead.
    uint8_t _styleOverride = 0xFF;
    // The screensaver's box IS the rotated shape, so its instance overrides the two layout transforms:
    // rotateVU forced on (that is what makes the level axis the long one and stacks the channels) and
    // boomboxVU off, or a conf carrying the ribbon flag would draw its shape into a box the layout
    // never described.  False on the player's own widget, which follows the conf.
    bool _forceRotate = false;
    // Whether the canvas buffer came from PSRAM, so the destructor can hand the byte count back to the
    // Core Monitor's figure.  The canvas is a plain allocation rather than a psFrameBuffer, so the
    // accounting is this widget's to keep.
    bool _canvasPsram = false;
    // The rectangle this frame's fills touched, in canvas coordinates: _fillLocal() is the only place that
    // knows the pixel surface, so it is the only place that has to keep this.  Empty means "nothing on the
    // canvas changed, so nothing goes to the panel"; _draw() empties it at the top of every frame.
    uint16_t _dirtyX0 = 0, _dirtyY0 = 0, _dirtyX1 = 0, _dirtyY1 = 0;
    // The style this instance draws: the live setting unless the instance is pinned.
    uint8_t _style() const;
    // The peaks/axis switch this instance obeys, derived the same way: the screensaver's meter and the
    // player page's box can be different VUs, so neither the markers nor the axis lines are shared with it.
    bool _vupeak() const;
    // True for the painters that clear their own area as they draw, so _draw() can leave out its wipe.
    bool _selfErasing() const;

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
    void _invalidate() override;
    // Send the canvas to the panel, whole or in part, from the rectangle _fillLocal() recorded this frame.
    void _blit();
    // One log line with the frame's split, printed a few times once the screensaver's meter has settled.
    void _reportCost();

    #ifdef WIDGET_DEBUG
      // Draw-cost report, emitted from loop() every 5 s like the Core Monitor. These members exist only in a debug
      // build, so a normal one pays neither the RAM nor the counting.
      uint32_t _dbgFrames = 0, _dbgFills = 0, _dbgDrawUs = 0, _dbgPeakUs = 0, _dbgLogMs = 0;
      uint32_t _fills = 0;                   // fills drawn by the frame now being drawn
    #endif
};

#endif // #if DSP_MODEL!=DSP_DUMMY
#endif // #ifndef widget_vu_h
