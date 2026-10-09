#include "../../core/options.h"
#if DSP_MODEL!=DSP_DUMMY
#include <Arduino.h>
#include <esp_heap_caps.h>        // the screensaver's canvas is a PSRAM allocation
#include "../dspcore.h"
#include "../../core/display.h"
#include "../tools/psframebuffer.h"
#include "widgets.h"
#include "widget_vu.h"
#include "../../locale/dsplocale.h"
#include "../../core/config.h"
#include "../../core/logging.h"
#include "../../core/network.h"
#include "../../core/startup.h"   //  for the services-busy window the limiter slows down in
#include "../../core/player.h"    //  for the VU level source
#include "../../core/utility.h"

/************************
      VU WIDGET
 ************************/

#if defined(DSP_TFT)
// GFXcanvas16 keeps its raster pointer protected - "no longer private, allow subclass access" is how the
// header puts it - and offers getBuffer() as the only reader.  A buffer we allocated ourselves can only be
// handed over from a subclass, so this is the thinnest one that can: it owns and frees nothing, and the
// widget remains the thing that allocates and releases the PSRAM block behind it.
class VuCanvas : public Canvas {
  public:
    VuCanvas(uint16_t w, uint16_t h) : Canvas(w, h, false) {}
    void takeBuffer(uint16_t* p) { buffer = p; }
};

// Give the canvas its buffer.  The member is protected, hence the subclass.
static inline void vuCanvasTake(Canvas* c, uint16_t* buf) {
  if (c) static_cast<VuCanvas*>(c)->takeBuffer(buf);
}

// The canvas is only a staging buffer for the single-burst blit, and the screensaver's box is far past
// what the internal heap can hold - 346 KB for a 480x360 panel - so the buffer is taken from PSRAM in
// the shape vuScratchAlloc() uses for the FFT scratch.  It is counted in `psramFrameBufferBytes`, the
// same figure psFrameBuffer keeps for its own windows, so the Core Monitor's PSRAM line covers every
// framebuffer this build owns.  No internal fallback is aimed at here: a build without PSRAM cannot
// host the feature, and the caller treats a null buffer as "no canvas" (see ready()).
static uint16_t* vuCanvasBuffer(uint16_t w, uint16_t h, bool& fromPsram) {
  const size_t bytes = (size_t)w * h * sizeof(uint16_t);
  if (!bytes) return nullptr;
  fromPsram = true;
  void* p = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM);
  if (!p) { fromPsram = false; p = malloc(bytes); }
  if (!p) return nullptr;
  if (fromPsram) psramFrameBufferBytes += bytes;
  memset(p, 0, bytes);
  return (uint16_t*)p;
}

// Give a buffer back, keeping the accounting straight.  free() is the same call psFrameBuffer makes
// on its own PSRAM block.
static void vuCanvasRelease(uint16_t* buf, size_t bytes, bool fromPsram) {
  if (!buf) return;
  if (fromPsram && psramFrameBufferBytes >= bytes) psramFrameBufferBytes -= bytes;
  free(buf);
}

// The bytes a canvas holds, from the canvas itself - the only record the widget keeps.
static inline size_t vuCanvasBytes(const Canvas* c) {
  return (size_t)c->width() * (size_t)c->height() * sizeof(uint16_t);
}
#endif

VuWidget::~VuWidget() {
  #if defined(DSP_TFT)
    if (_canvas) {
      // The buffer is ours - VuCanvas is built with allocate_buffer = false - so it is ours to give back,
      // and _canvasPsram is the only record of whether those bytes came from PSRAM.
      if (_canvasBuf) vuCanvasRelease(_canvasBuf, vuCanvasBytes(_canvas), _canvasPsram);
      _canvasBuf = nullptr;
      delete _canvas; _canvas = nullptr;
    }
  #endif
}
  
// The style this instance draws.  0xFF means "the player's widget", which follows the live setting: the
// vustyle=<n> command only queues SHOWVUMETER, so the value has to be read on every frame rather than
// latched at init.  The screensaver's instance is pinned to screensaverVUStyle instead.
uint8_t VuWidget::_style() const {
  return (_styleOverride == 0xFF) ? config.store.vustyle : _styleOverride;
}

// The same split for the peaks/axis switch, and for the same reason: with peers to respect, a screensaver
// meter must be able to show its markers and reference lines while the player page's box does not.
bool VuWidget::_vupeak() const {
  return (_styleOverride == 0xFF) ? config.store.vupeak : config.store.screensaverVUpeak;
}

// True only for the painter that lays down its own background as it draws: the bar family in the ROTATED
// shape, whose incremental path fills a band with the background before re-lighting the lit part of it, and
// whose full frames fill the whole box.  A full-box wipe before that is pure duplication, and leaving it out
// is also what makes the partial blit possible - a wipe marks every pixel dirty by definition.
//
// It is deliberately not scoped to the style family alone.  The family's pattern draws one segment per step
// and leaves bandsConf.vspace pixels of every step, plus the whole strip between the two channels, for
// something else to fill - and for the layout's three shapes (the ribbon and the two side-by-side forms)
// that something is still _draw()'s wipe.  Exempting them left the previous frame sitting in those gaps,
// which showed up as the peak marker surviving there and as the bar appearing to miss its middle points.
//
// Every other painter draws over whatever was already on the canvas - a trace has no way to erase its own
// previous line - and needs the wipe too.
bool VuWidget::_selfErasing() const {
  const uint8_t s = _style();
  if (s != VU_STYLE_BARS && s != VU_STYLE_DIGITAL_LED) return false;
  return _rotate;
}

bool VuWidget::ready() {
  #if defined(DSP_TFT)
    return _canvas && _canvasBuf;
  #else
    // The OLED panels draw straight into their own panel buffer, so there is nothing that can be missing.
    return true;
  #endif
}

// The screensaver's meter, from a box resolveScreensaverBox() has already sized and placed.  The box is
// described to the painters in the same terms a layout uses: _draw() resolves rotate mode as
// _cw = bands.height and _ch = bands.width * 2 + bands.space, so a box of w x h is bands.height = w with
// bands.width = (h - space) / 2.  The bar family instead wants its two channels as whole rows with the gap
// between them, which is that identity read the other way round: bands.width = barH, bands.space = gapH,
// and the box is exactly 2 * barH + gapH tall.  vspace and perheight - the gap between segments and the
// count of them inside a bar - stay the layout's own.
//
// The palette comes from the live theme rather than from any stored conf, because this widget is not
// derived from the layout in any way: it is the theme's VU colours over an area the layout never saw.
bool VuWidget::initScreensaver(const VuBox& box, const VUBandsConfig& layoutBands, uint8_t style) {
  _forceRotate = true;        // the box IS the rotated shape: level axis along it, channels stacked
  _styleOverride = style;     // pinned, so this instance ignores config.store.vustyle
  VUBandsConfig b = layoutBands;
  b.height = box.w;           // the level axis is the box's width
  if (box.bars) {
    b.width = box.barH;
    // space is a byte in the master, so a gap wider than 255 px could not be described.  No panel's can be:
    // the budget caps the box long before that.
    b.space = (box.gapH > 255) ? 255 : (uint8_t)box.gapH;
  } else {
    b.space = layoutBands.space;
    b.width = (box.h > (uint16_t)b.space) ? (uint16_t)((box.h - b.space) / 2) : 1;
  }
  if (b.width < 1) b.width = 1;
  if (b.height < 1) b.height = 1;
  WidgetConfig wc;
  wc.left = box.left; wc.top = box.top; wc.textsize = 1; wc.align = WA_LEFT;
  // Black, never config.theme.background: the screensaver paints its own picture and its background is always
  // black - the clock and the info line do the same while isScreensaver.  The four VU colours stay the live
  // theme's, which is what makes the meter look like the theme's; only what it erases to is black.
  init(wc, b, config.theme.vumax, config.theme.vumin, config.theme.vupeak,
       0, config.theme.vuaxis);
  return ready();
}

// The screensaver's box for a style.  Three rules, and each one only engages where the area can afford it:
//
//   bars       the two channels are rows - VU_BAR_PERCENT each, VU_BAR_GAP_PERCENT between them, and the
//              remainder above and below left as placement rather than painted rows - so the box is
//              2 * bar + gap and it is centred.  A box over the budget scales the bar and the gap
//              together, which keeps the shape and buys bigger bars than trimming the bar alone would.
//   Lissajous  a square, because the figure is an X-Y plot and reads wrong in a rectangle.  Its budget is
//              VU_MAX_HEIGHT squared, deliberately smaller than the rectangle's.
//   the rest   the budget rectangle, VU_MAX_WIDTH x VU_MAX_HEIGHT at most, centred.
//
// Below the budget the area comes back untouched, which is what keeps a small panel and an OLED as they were.

// The bar family's shape, as percentages of the content height - Bars (VU_STYLE_BARS) and Digital LED
// (VU_STYLE_DIGITAL_LED) are one painter over one box.  Each channel takes VU_BAR_PERCENT and the gap
// between them VU_BAR_GAP_PERCENT; the remainder is placement, split evenly above and below by the
// centring.  The shipped shape is 25 + 20 + 10 + 20 + 25.
#define VU_BAR_PERCENT      20
#define VU_BAR_GAP_PERCENT  10

VuBox VuWidget::resolveScreensaverBox(uint16_t areaW, uint16_t areaH, uint8_t style) {
  VuBox box;
  if (!areaW || !areaH) return box;
  const uint32_t budget = (uint32_t)VU_MAX_WIDTH * VU_MAX_HEIGHT;
  if (style == VU_STYLE_BARS || style == VU_STYLE_DIGITAL_LED) {
    uint16_t gap = (uint16_t)(areaH * VU_BAR_GAP_PERCENT / 100u);
    uint16_t bar = (uint16_t)(areaH * VU_BAR_PERCENT / 100u);
    if (!gap) gap = 1;
    uint32_t h = (uint32_t)2 * bar + gap;
    const uint32_t hmax = budget / areaW;
    if (hmax && h > hmax) {
      bar = (uint16_t)((uint32_t)bar * hmax / h);
      gap = (uint16_t)((uint32_t)gap * hmax / h);
      if (!gap) gap = 1;
      if (bar < 2) bar = 2;
      h = (uint32_t)2 * bar + gap;
    }
    if (bar >= 2) {
      box.bars = true; box.barH = bar; box.gapH = gap;
      box.w = areaW; box.h = (uint16_t)h; box.left = 0;
      box.top = (uint16_t)((areaH - (uint16_t)h) / 2);
      return box;
    }
    // Too short for two rows: the rectangle below is what the style gets, drawn in the whole area.
  }
  if (style == VU_STYLE_LISSAJOUS && (uint32_t)areaW * areaH > (uint32_t)VU_MAX_HEIGHT * VU_MAX_HEIGHT) {
    uint16_t side = (areaW < areaH) ? areaW : areaH;
    if (side > VU_MAX_WIDTH) side = VU_MAX_WIDTH;
    if (side > VU_MAX_HEIGHT) side = VU_MAX_HEIGHT;
    box.w = side; box.h = side;
  } else if ((uint32_t)areaW * areaH > budget) {
    box.w = (areaW < VU_MAX_WIDTH) ? areaW : VU_MAX_WIDTH;
    box.h = (areaH < VU_MAX_HEIGHT) ? areaH : VU_MAX_HEIGHT;
  } else {
    box.w = areaW; box.h = areaH;
  }
  box.left = (uint16_t)((areaW - box.w) / 2);
  box.top  = (uint16_t)((areaH - box.h) / 2);
  return box;
}

// The largest linear factor, in 1/256ths, that brings an area of w x h down to `budget` or less.  Run once
// per init, so a search costs nothing and reads clearer than a square root would.
static uint16_t vuScaleToBudget(uint32_t w, uint32_t h, uint32_t budget) {
  const uint64_t area = (uint64_t)w * h;
  uint16_t f = 256;
  while (f > 1 && ((area * f * f) >> 16) > budget) f--;
  return f;
}

void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t vupeakcolor, uint16_t bgcolor, uint16_t vuaxiscolor) {
  // A screensaver instance forces rotate on: that is what makes the level axis the long one and stacks the
  // two channels, which is the shape its box was measured for.  The player's widget follows the conf.  It is
  // resolved first because the budget below reads it.
  _rotate = _forceRotate || (rotateVU_ptr ? *rotateVU_ptr : false);
  // The pixel budget, applied to a conf's box only when that box exceeds VU_MAX_WIDTH x VU_MAX_HEIGHT: both
  // dimensions are scaled by one factor, so the shape survives, and the box is re-centred in the space the
  // conf gave it.  Every shipped layout's VU box is inside the budget, so this leaves them all alone - and
  // the screensaver's box has been through the same budget already, which makes this a no-op for it.
  const uint32_t budget = (uint32_t)VU_MAX_WIDTH * VU_MAX_HEIGHT;
  const uint16_t boxW = _rotate ? bands.height : (uint16_t)(bands.width * 2 + bands.space);
  const uint16_t boxH = _rotate ? (uint16_t)(bands.width * 2 + bands.space) : bands.height;
  if ((uint32_t)boxW * boxH > budget) {
    const uint16_t f = vuScaleToBudget(boxW, boxH, budget);
    const uint16_t nW = (uint16_t)(((uint32_t)boxW * f) >> 8);
    const uint16_t nH = (uint16_t)(((uint32_t)boxH * f) >> 8);
    wconf.left = (int16_t)(wconf.left + (uint16_t)((boxW - nW) / 2));
    wconf.top  = (int16_t)(wconf.top + (uint16_t)((boxH - nH) / 2));
    const uint16_t across = _rotate ? nH : nW;         // the side the two channels share
    const uint16_t room = (across > bands.space) ? (uint16_t)(across - bands.space) : 0;
    bands.width = (room > 1) ? (uint16_t)(room / 2) : 1;
    bands.height = _rotate ? nW : nH;
  }
  Widget::init(wconf, bgcolor, bgcolor);
  _vumaxcolor = vumaxcolor;
  _vumincolor = vumincolor;
  _vupeakcolor = vupeakcolor;
  _vuaxiscolor = vuaxiscolor;
  _peakL = _peakR = 0xFFFF;   // sentinel: adopt the live reading on the next _levels()
  _lastMs = 0;                // 0 also means "not primed yet", so the next frame adopts the level
  _redrawMs = 0;              // the duty limiter starts from scratch too
  _drawUs = 0;
  _intervalMs = VU_REFRESH_MS;
  _histMs = 0;                // the history strip primes its own clock on its first frame
  // Start the frame counters over as well, so the cost report samples this instance's first frames rather
  // than having already passed them: _frame drives the simulated wobble (harmless to restart) and is the
  // clock _reportCost() samples against.
  _frame = 0;
  _eraseUs = _paintUs = _blitUs = 0;
  // A new canvas holds nothing this widget drew, so the first frame has to paint all of it: the bar
  // family's incremental path keys off these, and the sentinel means "no previous frame".
  _prevMeasL = _prevMeasR = _prevPkL = _prevPkR = 0xFFFF;
  _prevStyle = 0xFF;
  #ifdef WIDGET_DEBUG
    _dbgFrames = _dbgFills = _dbgDrawUs = _dbgPeakUs = 0;
    _dbgLogMs = millis();
    _fills = 0;
  #endif
  _accL = _accR = _accPL = _accPR = 0;
  _holdL = _holdR = 0;
  _bands = bands;
  #if defined(DSP_TFT)
    // TFT transfers the whole widget in one SPI burst, so it needs an intermediate canvas. OLED panels own
    // their framebuffer and are drawn to directly.  The buffer is allocated here rather than by the
    // library, so a failed allocation is a state the widget can report instead of a null dereference.
    if (_canvas) {
      if (_canvasBuf) vuCanvasRelease(_canvasBuf, vuCanvasBytes(_canvas), _canvasPsram);
      _canvasBuf = nullptr;
      delete _canvas; _canvas = nullptr;
    }
    const uint16_t cw = _rotate ? _bands.height : (uint16_t)(_bands.width * 2 + _bands.space);
    const uint16_t ch = _rotate ? (uint16_t)(_bands.width * 2 + _bands.space) : _bands.height;
    _canvas = new VuCanvas(cw, ch);
    _canvasBuf = vuCanvasBuffer(cw, ch, _canvasPsram);
    vuCanvasTake(_canvas, _canvasBuf);
  #endif
}


// LED steps. The segment grid is anchored at the start of the axis _drawBand() walks, and the lit region is a
// PREFIX of that stack for most families but a SUFFIX for the BoomBox's left channel, so the two snap opposite ends.
// Both are pure functions of the value, which is what keeps the fall snapped: the boundary holds on its grid line and
// then drops a whole segment. Only the DRAWN value is quantised - _levels() keeps the raw meas, because the fade
// accumulators carry sub-pixel remainders and the peak's high-water mark depends on the exact minimum.
static inline uint16_t vuSnapLit(uint16_t meas, uint16_t step, uint16_t len) {
  if (step < 2 || meas > len) return meas;
  uint16_t lit = len - meas;                 // the lit length; round it UP so the tip's segment lights
  uint16_t r = lit % step;
  if (r) lit += step - r;
  return (lit > len) ? 0 : (uint16_t)(len - lit);
}
static inline uint16_t vuSnapClear(uint16_t meas, uint16_t step) {
  return (step < 2) ? meas : (uint16_t)(meas - (meas % step));   // round the clear DOWN
}

bool VuWidget::_fillLocal(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
  // TFT renders into the canvas blitted at the end; OLED has no canvas, writes straight to the panel buffer, and so
  // needs the widget origin added. A zero dimension is a no-op, which keeps callers from each testing for degenerate
  // rects. Everything is CLIPPED to the box here: this is the one place that knows the pixel surface, and the
  // per-frame fill covers exactly _cw x _ch, so a fill that reaches outside is painted once per frame and erased by
  // nothing, leaving a permanent mark (an even-height history strip did exactly that).
  #if defined(DSP_TFT)
    if (!_canvas || !_canvasBuf) return false;   // a canvas that failed to allocate draws nothing
  #endif
  if (!w || !h) return false;
  if (x >= _cw || y >= _ch) return false;
  if ((uint32_t)x + w > _cw) w = (uint16_t)(_cw - x);
  if ((uint32_t)y + h > _ch) h = (uint16_t)(_ch - y);
  if (!w || !h) return false;
  #ifdef WIDGET_DEBUG
    _fills++;                  // only a real fill counts; the early return above is not one
  #endif
  #if defined(DSP_TFT)
    // Grow the frame's dirty rectangle.  The clipping above has already turned this into a real rectangle,
    // so this is exactly the set of canvas pixels the fill is about to touch - recorded here because this
    // is the one function that knows the pixel surface.
    if (x < _dirtyX0) _dirtyX0 = x;
    if (y < _dirtyY0) _dirtyY0 = y;
    if ((uint16_t)(x + w) > _dirtyX1) _dirtyX1 = (uint16_t)(x + w);
    if ((uint16_t)(y + h) > _dirtyY1) _dirtyY1 = (uint16_t)(y + h);
    _canvas->fillRect(x, y, w, h, color);
  #else
    dsp.fillRect(_config.left + x, _config.top + y, w, h, color);
  #endif
  return true;
}

void VuWidget::_draw(){
  if(!_active || _locked) return;
  #if defined(DSP_TFT)
    if (!_canvas || !_canvasBuf) return;   // no canvas: nothing to stage, nothing to blit - see ready()
  #endif

  // Resolve the box once, exactly as the single-style widget always did, so every style works from the same numbers.
  // rotateVU says which bandsConf axis is the level axis: read here for every style, while boomboxVU is a bar-family
  // flag only.
  if (!_rotate && _config.align) { _len = _bands.width;  _thk = _bands.height; }
  else                           { _len = _bands.height; _thk = _bands.width;  }
  if (_rotate) { _cw = _len; _ch = _bands.width * 2 + _bands.space; }
  else         { _cw = _bands.width * 2 + _bands.space; _ch = _bands.height; }

  _levels(_len, _measL, _measR);

  // The wipe is for the painters that have nothing to erase with: their previous frame is removed by
  // clearing the box rather than by the style itself.  The bar family does its own erasing, and leaving
  // the wipe out for it is also what makes a partial blit possible later - a full-box wipe marks every
  // pixel dirty by definition, which is the information a dirty rectangle is built from.
  // The dirty rectangle starts empty and _fillLocal() grows it, so what goes to the panel at the end of
  // this frame is what the painters actually touched rather than the whole box every time.
  _dirtyX0 = _cw; _dirtyY0 = _ch; _dirtyX1 = 0; _dirtyY1 = 0;

  const uint32_t tEraseStart = micros();
  if (!_selfErasing()) _fillLocal(0, 0, _cw, _ch, _bgcolor);
  const uint32_t tEraseEnd = micros();

  _frame++;

  switch (static_cast<vuStyle_e>(_style())) {
    case VU_STYLE_DIGITAL_LED:      _drawBars(true);        break;
    case VU_STYLE_HISTORY:          _drawHistory();         break;
    case VU_STYLE_SPECTRUM_REFLECT: _drawSpectrumReflect(); break;
    case VU_STYLE_SPECTRUM_MIRROR:  _drawSpectrumMirror();  break;
    case VU_STYLE_WAVE:             _drawWave();            break;
    case VU_STYLE_LISSAJOUS:        _drawLissajous();       break;
    // Only an unknown id, or a style this build cannot draw, reaches the bars - including a stored id from before
    // Spectrum A was removed. The two sample styles are deliberately not a fallback from a painter that has no data:
    // a dropped capture frame (a torn seqlock read) must leave the frame alone, not flash the bars for one refresh.
    default:                  _drawBars(false);     break;
  }

  const uint32_t tPaintEnd = micros();

  // Send what this frame touched, whole or in part - see _blit().  The OLED panels do nothing here: they
  // draw straight into their own panel buffer and DspCore::loop() flushes it after this returns.
  _blit();

  // The frame's split, for _reportCost().  Last frame's raw figures, not smoothed: the report samples a
  // few frames and the reader wants what those frames actually cost.
  _eraseUs = tEraseEnd - tEraseStart;
  _paintUs = tPaintEnd - tEraseEnd;
  _blitUs  = micros() - tPaintEnd;
}

// One line naming where a full-panel meter's frame time goes.  Printed only by the screensaver's own
// instance (the pinned style is what identifies it), and only on three frames: the first one after the
// fade and the peak markers have settled, then two more so a single unlucky sample cannot mislead.  A
// TFT with a canvas carries the wire cost in the blit; the OLED panels report zero there, because they
// have no canvas and their flush happens after this returns.
void VuWidget::_reportCost() {
  FUNCTIONLOG("VU.Widget", "SS meter %ux%u px, style %u: erase %lums, painter %lums, blit %lums, total %lums, frame %u, dirty %ux%u px",
      (unsigned)_cw, (unsigned)_ch, (unsigned)_style(),
      (unsigned long)(_eraseUs / 1000u), (unsigned long)(_paintUs / 1000u),
      (unsigned long)(_blitUs / 1000u),
      (unsigned long)((_eraseUs + _paintUs + _blitUs) / 1000u),
      (unsigned)_frame,                                            // the frame counter, not a fill count
      (unsigned)(_dirtyX1 > _dirtyX0 ? _dirtyX1 - _dirtyX0 : 0),   // the rectangle _blit() was handed
      (unsigned)(_dirtyY1 > _dirtyY0 ? _dirtyY1 - _dirtyY0 : 0));
}

void VuWidget::_drawBars(bool led){
  const uint16_t len = _len, thk = _thk, cw = _cw;
  uint16_t measL = _measL, measR = _measR;

  uint16_t step = len / _bands.perheight;
  if (step < 1) step = 1;
  uint16_t h = step;
  if (h > _bands.vspace) h -= _bands.vspace;
  else h = 1;

  // One of the two layout transforms is read live down here, so a screensaver box overrides it.  rotateVU
  // is already folded into _rotate by init() and every test below reads that member; boomboxVU is not, and
  // the ribbon shape belongs to a conf that asked for it - a screensaver box was measured for the plain
  // stacked form and would otherwise be repainted in the layout's shape.
  const bool bb = _forceRotate ? false : *boomboxVU_ptr;

  if (led) {
    if (_rotate || (_config.align && !bb)) {
      measL = vuSnapLit(measL, step, len);
      measR = vuSnapLit(measR, step, len);
    } else if (_config.align) {              // BoomBox left channel is cleared from x = 0
      measL = vuSnapClear(measL, step);
      measR = vuSnapLit(measR, step, len);
    } else {                                 // vertical: both are cleared from y = 0
      measL = vuSnapClear(measL, step);
      measR = vuSnapClear(measR, step);
    }
  }

  // Peak marker geometry, resolved once because both the incremental path just below and the full path
  // further down draw with it.  peakThk reserves the outermost pixels of the widget, which keeps the marker
  // inside the .bandsConf footprint.
  const uint16_t peakThk = ((len * VU_PEAK_THICKNESS_MILLI + 999) / 1000) > 0
                         ? (uint16_t)((len * VU_PEAK_THICKNESS_MILLI + 999) / 1000) : 1;
  uint16_t pkL = 0, pkR = 0;
  if (_vupeak()) {
    // Collapse the 0xFFFF sentinel and clamp BEFORE snapping: a snap can move a marker down to the quiet end, and a
    // negative x would wrap in the fillRect below.
    pkL = (_peakL < peakThk || _peakL > len) ? peakThk : _peakL;
    pkR = (_peakR < peakThk || _peakR > len) ? peakThk : _peakR;
    if (led) {
      if (_rotate || (_config.align && !bb)) {
        pkL = vuSnapLit(pkL, step, len);
        pkR = vuSnapLit(pkR, step, len);
      } else if (_config.align) {
        pkL = vuSnapClear(pkL, step);
        pkR = vuSnapLit(pkR, step, len);
      } else {
        pkL = vuSnapClear(pkL, step);
        pkR = vuSnapClear(pkR, step);
      }
      if (pkL < peakThk) pkL = peakThk;      // the snap must not push the marker out of the box
      if (pkR < peakThk) pkR = peakThk;
    }
  }

  // ---- The rotated shape, incrementally -------------------------------------------------------------
  // A screensaver's box is always rotated, and that is what this path exists for.  Only the band that can
  // have changed since the last frame is repainted: the span between the old tip and the new one for each
  // channel, widened to wherever its peak marker moved.  Everything outside it already holds this frame's
  // picture - below the old tip the colour pattern has not changed and past the new tip the background has
  // not - so a quiet station touches a few hundred pixels instead of the whole box, and _draw()'s blob
  // shrinks to match.  The invariant that makes it correct is that _prevMeas* / _prevPk* describe exactly
  // what the canvas holds, and every way of breaking that (a new canvas, a move, a lock) invalidates them
  // through init() or _reset().
  //
  // The other three shapes keep the full repaint: they are the layout's own arrangements - the ribbon and
  // the two side-by-side forms - they are small boxes on a real layout, and each would need its own version
  // of this arithmetic.
  if (_rotate) {
    const bool full = (_prevMeasL == 0xFFFF || _prevMeasR == 0xFFFF ||
                       _prevStyle != _style() || _prevVupeak != _vupeak());
    // A full repaint owns its background, exactly as _draw()'s wipe did for this family before
    // _selfErasing() took the wipe away from it - and for the same reason the incremental path below has to
    // lay down a band of it: the pattern leaves bandsConf.vspace pixels of every step, and the whole strip
    // between the two channels, untouched.  Nothing else fills them, so without this they hold whatever the
    // canvas was allocated with - vuCanvasBuffer() memsets to zero, and they are black.
    if (full) _fillLocal(0, 0, _cw, _ch, _bgcolor);
    else {
      for (uint8_t ch = 0; ch < 2; ch++) {
        const uint16_t meas = ch ? measR : measL;
        const uint16_t tipNew = (uint16_t)(len - meas);
        // Each channel reads its OWN previous tip: with the branches swapped the band was anchored on the
        // other channel's old tip, so wherever the two differed part of the changed span went unpainted.
        const uint16_t tipOld = (uint16_t)(len - (ch ? _prevMeasR : _prevMeasL));
        const uint16_t pkNew = ch ? pkR : pkL;
        const uint16_t pkOld = ch ? _prevPkR : _prevPkL;
        uint16_t lo = (tipOld < tipNew) ? tipOld : tipNew;
        uint16_t hi = (tipOld < tipNew) ? tipNew : tipOld;
        if (_vupeak()) {
          // Each marker sits just beyond its tip and moves on its own, so both its old and its new rect
          // belong to the band: repainting the band is what restores the old one to pattern or background.
          for (uint8_t k = 0; k < 2; k++) {
            const uint16_t pk = k ? pkOld : pkNew;
            if (pk == 0xFFFF || pk > len) continue;
            const uint16_t a = (uint16_t)(len - pk), b = (uint16_t)(a + peakThk);
            if (a < lo) lo = a;
            if (b > hi) hi = b;
          }
        }
        if (hi > len) hi = len;
        if (hi <= lo) continue;                       // this channel did not move at all
        const uint16_t off = ch ? (uint16_t)(_bands.width + _bands.space) : 0;
        // The band gets its background first - the gaps between segments and the strip between the two
        // channels are only the theme's background if something puts it there, and this is the something.
        // It also makes the whole band correct in one pass: below it, only the lit part is re-lit.
        _fillLocal(lo, off, (uint16_t)(hi - lo), thk, _bgcolor);
        // Then the lit part of the band, one segment at a time so the hot end keeps its colour.
        const uint16_t pEnd = (tipNew < hi) ? tipNew : hi;
        for (uint16_t i = (uint16_t)((lo / step) * step); i < pEnd; i = (uint16_t)(i + step)) {
          uint16_t hh = h;
          if ((uint16_t)(i + hh) > pEnd) hh = (uint16_t)(pEnd - i);
          _drawBand(i, ch, hh, (i > len - step * 3) ? _vumaxcolor : _vumincolor);
        }
        // Then the marker, over both.
        if (_vupeak() && pkNew != 0xFFFF && pkNew <= len)
          _fillLocal((uint16_t)(len - pkNew), off, peakThk, thk, _vupeakcolor);
      }
    }
    // Remember what this frame left there, whichever path drew it.
    _prevMeasL = measL;
    _prevMeasR = measR;
    _prevPkL = _vupeak() ? pkL : 0xFFFF;
    _prevPkR = _vupeak() ? pkR : 0xFFFF;
    _prevStyle = _style();
    _prevVupeak = _vupeak();
    if (!full) return;                                // nothing left to do, and no full-box work
  }

  for (int i = 0; i < len; i += step) {
    // Clamp the last segment to len. The loop bound only guarantees a segment STARTS before len, and anything drawn
    // past the box can never be erased - the fills and _clear() all stop at len. Clamp, not skip, so the bar tip still
    // reaches len - 1; only the last pass can overshoot.
    uint16_t hh = h;
    if ((uint16_t)(i + hh) > len) hh = len - i;
    uint16_t colorL, colorR;
    if (_rotate) {
      colorL = colorR = (i > len - step * 3) ? _vumaxcolor : _vumincolor;
    } else if (_config.align) {
      if (!bb) {
        colorL = colorR = (i > len - step * 4) ? _vumaxcolor : _vumincolor;
      } else {
        colorL = (i > step) ? _vumincolor : _vumaxcolor;
        colorR = (i > len - step * 3) ? _vumaxcolor : _vumincolor;
      }
    } else {
      colorL = colorR = (i < step * 3) ? _vumaxcolor : _vumincolor;
    }
    _drawBand(i, 0, hh, colorL);
    _drawBand(i, 1, hh, colorR);
  }

  if (_rotate) {
    _fillLocal(len - measL, 0, measL, thk, _bgcolor);
    _fillLocal(len - measR, thk + _bands.space, measR, thk, _bgcolor);
  } else if (_config.align) {
    if (!bb) {
      _fillLocal(len - measL, 0, measL, thk, _bgcolor);
      _fillLocal(cw - measR, 0, measR, thk, _bgcolor);
    } else {
      _fillLocal(0, 0, measL, thk, _bgcolor);
      _fillLocal(cw - measR, 0, measR, thk, _bgcolor);
    }
  } else {
    _fillLocal(0, 0, thk, measL, _bgcolor);
    _fillLocal(thk + _bands.space, 0, thk, measR, _bgcolor);
  }

  // Peak markers. Drawn after the clears so they survive them, and before the blit.  Their geometry was
  // resolved at the top of this function, because the incremental path needs the same numbers; this is only
  // the drawing of it.  Each marker sits in the cleared strip just beyond its channel's high-water mark.
  if (_vupeak()) {
    if (_rotate) {
      _fillLocal(len - pkL, 0, peakThk, thk, _vupeakcolor);
      _fillLocal(len - pkR, thk + _bands.space, peakThk, thk, _vupeakcolor);
    } else if (_config.align) {
      if (!bb) {
        _fillLocal(len - pkL, 0, peakThk, thk, _vupeakcolor);
        _fillLocal(cw - pkR, 0, peakThk, thk, _vupeakcolor);
      } else {
        _fillLocal(pkL - peakThk, 0, peakThk, thk, _vupeakcolor);
        _fillLocal(cw - pkR, 0, peakThk, thk, _vupeakcolor);
      }
    } else {
      _fillLocal(0, pkL - peakThk, _bands.width, peakThk, _vupeakcolor);
      _fillLocal(thk + _bands.space, pkR - peakThk, _bands.width, peakThk, _vupeakcolor);
    }
  }
}

// ---- Shared geometry for the non-bar painters ----
// Every style except the bar family draws time or frequency along the AREA's width and splits the area's height
// between the channels. The area is _cw x _ch, which _draw() has already derived from bandsConf using the two layout
// booleans. Nothing below transposes, on any layout family: the OLED, the portrait TFT box and the BoomBox ribbon all
// get the same picture, and the side-by-side channel arrangement is a bar-family concern only (the bar painters keep
// their own mapping in _drawBand()).

// Thickness of a reference line and of a trace: 1 px on a compact area, 2 px once there is room. Sized by the area's
// HEIGHT - the axis the thickness is perpendicular to - and NOT by the length of the axis the line runs along (the
// axis-length form made the two OLED layouts disagree on the same panel: both areas are 15 px tall, but Big VU's
// level axis is 126 px against Default's 44). The peak markers keep the axis-length rule, which is right for them.
// 48 px is where a second pixel stops being a large fraction of the height.
uint16_t VuWidget::_stroke() const {
  return (_ch >= 48) ? 2 : 1;
}

// The meter's calibration as a display gain: 65536 / vuThreshold in 8.8 fixed point, so 256 is unity.
// config.vuThreshold is the loudest the stream has been in units of sample / 128, so this puts that content at full
// scale - the same reference the level bars and the spectrum drive to. Without it the sample styles are an absolute
// reading of raw int16 and a trace hugs the middle of the area; an uncalibrated meter reads 0 and maps to unity.
uint16_t VuWidget::_waveGain() const {
  const uint16_t ref = config.vuThreshold ? config.vuThreshold : 256;
  uint32_t g = (256u * 256u) / ref;
  if (g < 64u)   g = 64u;      // 0.25x
  if (g > 8192u) g = 8192u;    // 32x
  return (uint16_t)g;
}

void VuWidget::_centreCross() {
  // One line across the middle of the area and one down it, meeting at the centre: two fills, and it is what a
  // border was only standing in for - with no signal the trace sits flat ON the axis, so silence reads as a flat
  // line rather than an empty box. Deliberately 1 px, not the trace thickness: the graticule is a reference, and at
  // the data weight it reads as heavy as the data. Painted first, so the trace overdraws it. It is reference
  // geometry, so the existing VU Meter Peaks switch owns it - the same checkbox that owns the level peak markers.
  // The spectrum's baseline and the history strip's divider are deliberately NOT gated with it: those two are what
  // their bars and traces grow out of, so hiding them would leave the data with nothing to read it against.
  if (!_vupeak()) return;
  _fillLocal(0, _ch / 2, _cw, 1, _vuaxiscolor);
  _fillLocal(_cw / 2, 0, 1, _ch, _vuaxiscolor);
}

// vumin for the body, vumax once an element reaches into the outer HOTSEG segments of its own axis - the same
// figure the bar family uses at the loud end, so a hot tip means the same thing everywhere. `len` and `full` are
// lengths in that axis' pixels, which is why the non-bar styles pass their own channel's available span.
uint16_t VuWidget::_hotColor(uint16_t len, uint16_t full) {
  const uint16_t seg = (_bands.perheight && full) ? (uint16_t)(full / _bands.perheight) : 1;
  return (((uint32_t)len + (uint32_t)seg * 3) > full) ? _vumaxcolor : _vumincolor;
}

uint16_t VuWidget::_bandColor(uint16_t lvl) {
  return _hotColor(lvl, _len);
}

// MIN_PX per bar, a real gap between them, and the count cut until it fits, capped PER CHANNEL. Both channels use the
// same N so their bands line up.
uint8_t VuWidget::_bandCount(uint16_t span, uint16_t *barW, uint16_t *gap) const {
  const uint16_t minw = VU_SPECTRUM_MIN_PX ? VU_SPECTRUM_MIN_PX : 1;
  // Its own tunable, NOT bandsConf.space: that value is the gap between the two meter strips (4 px on the TFT Default
  // layout, 17 px on the rotated one), and using it as an inter-bar gap left 15 bars on the first box and only 7 on
  // the second.
  const uint16_t g = VU_SPECTRUM_SPACE_PX ? VU_SPECTRUM_SPACE_PX : 1;
  uint16_t n = (uint16_t)((span + g) / (minw + g));
  if (n < 1) n = 1;
  if (n > VU_SPECTRUM_MAX_CHANNELS) n = VU_SPECTRUM_MAX_CHANNELS;
  *gap = g;
  *barW = (uint16_t)((span - (n - 1) * g) / n);
  if (!*barW) *barW = 1;
  return (uint8_t)n;
}

// The synthesised spectrum: the level spread across the bands with a fixed tilt plus a deterministic wobble so it
// does not look frozen. This is NOT a measurement, and it is not labelled as one: the style is called Spectrum
// Reflect on every backend. The bands are in the same units the real source returns (0..255, 255 a full-height bar),
// so the painter has one conversion and the two sources are interchangeable. There are no frequencies here to place,
// so the tilt runs across the band index; the log edges live with the transform in the audio library.
void VuWidget::_synthBands(uint8_t *out, uint8_t n, uint16_t lvl) {
  const uint16_t span = _len ? _len : 1;
  const uint16_t lvl255 = (uint16_t)((uint32_t)lvl * 255 / span);   // level px -> the 0..255 band scale
  for (uint8_t b = 0; b < n; b++) {
    const uint16_t tilt = (uint16_t)(100 - (uint32_t)b * 55 / (n ? n : 1));   // ~100% low to ~45% high
    const uint16_t wob  = (uint16_t)((b * 37 + _frame * 11) % 21);
    const uint32_t v = ((uint32_t)lvl255 * (tilt + wob)) / 120;
    out[b] = (v > 255) ? 255 : (uint8_t)v;
  }
}

/* ---- History strip ---------------------------------------------------------------------------- */

void VuWidget::_drawHistory(){
  const uint16_t full = _cw, hgt = _ch;
  const uint16_t th = _stroke();
  const uint16_t half = hgt / 2;
  const uint16_t lineY = (half > th / 2) ? (uint16_t)(half - th / 2) : 0;
  // Space a channel has, measured outward from the divider's outer edge. It is the SMALLER of the two halves, not
  // half the height: on an even height the divider row and its thickness come out of the lower half, so the R trace
  // was drawn one row past the bottom of the box - a line the area fill never reaches. An even 10 px box showed it.
  const uint16_t below = (hgt > (uint16_t)(lineY + th)) ? (uint16_t)(hgt - lineY - th) : 0;
  uint16_t band = (lineY < below) ? lineY : below;
  if (!band) band = 1;

  uint16_t nb = full / (VU_HISTORY_MIN_PX ? VU_HISTORY_MIN_PX : 1);
  if (nb < 1) nb = 1;
  if (nb > VU_HISTORY_MAX) nb = VU_HISTORY_MAX;
  // colW rounds UP, so that full / colW can only come back at or below the cap.  Floored, 480 / 100 is 4
  // and 480 / 4 is 120 - twenty bytes past each history array, every frame, and a panic a few hundred
  // frames in.
  uint16_t colW = (uint16_t)((full + nb - 1) / nb);
  if (!colW) colW = 1;
  nb = full / colW;
  if (nb > VU_HISTORY_MAX) nb = VU_HISTORY_MAX;   // the clamp belongs after the second division
  if (!nb) nb = 1;

  // One column per VU_REFRESH_MS of WALL CLOCK, not per redraw: the duty limiter makes the redraw rate vary with what
  // the frame costs, and a strip that advanced one column per frame would have a time axis that stretched with the
  // load. The columns owed since the last frame are pushed at once, and because every column is repainted from the
  // ring below, the scroll advances by exactly that many columns. Shifting two rows of at most 100 bytes is cheaper
  // than a ring plus index arithmetic. The levels are held as 0..255 rather than in axis pixels: the ring is one byte
  // per channel per column and the level axis on a BoomBox layout is 404 px, which does not fit in a byte.
  const uint32_t tickMs = VU_REFRESH_MS ? VU_REFRESH_MS : 1;
  const uint32_t now = millis();
  uint32_t adv = 1;
  if (!_histMs) {
    _histMs = now;                     // prime the clock; 0 is the sentinel, never a real stamp
  } else {
    adv = (now - _histMs) / tickMs;
    if (adv < 1) adv = 1;              // one column per frame at worst
    if (adv > nb) { adv = nb; _histMs = now; }    // a gap longer than the strip is one flat hold
    else            _histMs += adv * tickMs;      // otherwise keep the sub-tick remainder
  }
  const uint16_t span = _len ? _len : 1;
  const uint8_t lvL = (uint8_t)(((uint32_t)(span - _measL) * 255) / span);   // 0 = silent
  const uint8_t lvR = (uint8_t)(((uint32_t)(span - _measR) * 255) / span);
  memmove(&_histL[0], &_histL[adv], nb - adv);
  memmove(&_histR[0], &_histR[adv], nb - adv);
  for (uint16_t k = (uint16_t)(nb - adv); k < nb; k++) { _histL[k] = lvL; _histR[k] = lvR; }

  // Tick thickness is the trace thickness - the same figure the waveform and the Lissajous use, so the three trace
  // styles match. It used to be band / 4, which on the 130 px TFT area was a 16 px band of colour per column.
  uint16_t tick = _stroke();
  if (tick > band) tick = band;
  if (tick < 1) tick = 1;
  const uint16_t levelSpan = (band > tick) ? (uint16_t)(band - tick) : 1;

  // The middle line, and the fill, are the two halves of the vupeak switch in this style, and they are opposites:
  // with it on you get the line and the bare traces, with it off the filled form and no line. Painting the line first
  // keeps it under the data. The line is REFERENCE GEOMETRY, so it is drawn in vuaxis.
  if (_vupeak()) _fillLocal(0, lineY, full, th, _vuaxiscolor);

  uint16_t prevYL = 0, prevYR = 0;
  bool havePrev = false;
  for (uint16_t i = 0; i < nb; i++) {
    const uint16_t x = i * colW;
    const uint16_t lvL = _histL[i], lvR = _histR[i];
    const uint16_t pxL = (uint16_t)(((uint32_t)lvL * levelSpan) / 255);
    const uint16_t pxR = (uint16_t)(((uint32_t)lvR * levelSpan) / 255);
    // Silence sits on the divider for both channels: L grows up out of it, R grows down.
    const uint16_t yL = (pxL + tick <= lineY) ? (uint16_t)(lineY - pxL - tick) : 0;
    const uint16_t yR = (uint16_t)(lineY + th + pxR);
    const uint16_t colourL = _hotColor(pxL, band), colourR = _hotColor(pxR, band);
    // The fill is the other half of the switch, and it is DATA rather than a reference line: drawn in the trace
    // colours, each half by its own channel's level, so it fills in green and only runs hot once that channel does.
    // It goes down first so the traces overdraw it.
    // Either half of the switch draws exactly TWO rects per column, and the bare tick fills are folded into them:
    // the filled form runs from the L tip through the divider to the R tip, which contains both ticks, and a joined
    // run spans the previous tick to this one, which does too. Only a column with no predecessor to join to is drawn
    // as two bare ticks. The fills show in the WIDGET_DEBUG report as fills/frame, which made the redundancy worth
    // finding.
    if (!_vupeak()) {
      const uint16_t mid = (uint16_t)(lineY + th / 2);
      const uint16_t bottom = (uint16_t)(yR + tick);
      if (mid > yL)     _fillLocal(x, yL, colW, (uint16_t)(mid - yL), colourL);
      if (bottom > mid) _fillLocal(x, mid, colW, (uint16_t)(bottom - mid), colourR);
    } else if (havePrev) {
      // Switch on: the bare traces are joined column to column in BOTH directions - the run from the previous tick
      // to this one is the union of the two ticks and the gap between them. Without it a 2 px tick on a 2-4 px column
      // pitch reads as a row of dashes; joining only the rises is not what a trace looks like.
      const uint16_t topL = (yL < prevYL) ? yL : prevYL;
      const uint16_t botL = (uint16_t)(((yL > prevYL) ? yL : prevYL) + tick);
      _fillLocal(x, topL, colW, (uint16_t)(botL - topL), colourL);
      const uint16_t topR = (yR < prevYR) ? yR : prevYR;
      const uint16_t botR = (uint16_t)(((yR > prevYR) ? yR : prevYR) + tick);
      _fillLocal(x, topR, colW, (uint16_t)(botR - topR), colourR);
    } else {
      _fillLocal(x, yL, colW, tick, colourL);   // first column: nothing to join to yet
      _fillLocal(x, yR, colW, tick, colourR);
    }
    prevYL = yL; prevYR = yR; havePrev = true;
  }
}

// ---- Spectrum Reflect ----
// Frequency along the area's width, both channels ascending left to right, and the divider between them is the
// baseline: L's bands rise above it, R's descend below it, both growing OUTWARD, which makes the two halves read as
// one instrument reflected about the line.

void VuWidget::_drawSpectrumReflect(){
  const uint16_t full = _cw;
  const uint16_t th = _stroke();
  const uint16_t half = _ch / 2;
  const uint16_t lineY = (half > th / 2) ? (uint16_t)(half - th / 2) : 0;
  // Space a channel has, measured outward from the baseline's outer edge - the SMALLER of the two halves, for the
  // same reason as the history strip: on an even height the divider row and its thickness come out of the descending
  // half, so R's bars would be drawn one row past the bottom.
  const uint16_t below = (_ch > (uint16_t)(lineY + th)) ? (uint16_t)(_ch - lineY - th) : 0;
  uint16_t band = (lineY < below) ? lineY : below;
  if (!band) band = 1;

  uint16_t barW = 1, gap = 1;
  const uint8_t n = _bandCount(full, &barW, &gap);
  // barW is floored, so the bars never fill the area exactly. Centre the block instead of leaving the remainder
  // against the right edge, so they sit symmetrically under the baseline.
  const uint16_t used = (uint16_t)((uint32_t)n * barW + (uint32_t)((n > 1) ? (n - 1) : 0) * gap);
  const uint16_t x0 = (used < full) ? (uint16_t)((full - used) / 2) : 0;

  // The baseline is half of what the vupeak switch does here; the other half is the bar colouring below. With it on
  // you get the line and bars split into a vumin body with a vumax tip; with it off, no line and whole bars of one
  // colour. The line is REFERENCE GEOMETRY so it is drawn in vuaxis; the bars are DATA, in the trace colours.
  if (_vupeak()) _fillLocal(0, lineY, full, th, _vuaxiscolor);

  const uint16_t seg = (_bands.perheight && band) ? (uint16_t)(band / _bands.perheight) : 1;
  const uint16_t hot = (uint16_t)(seg * 3);   // the outer HOTSEG segments, as everywhere else
  const uint16_t span = _len ? _len : 1;
  const uint16_t lvL = span - _measL, lvR = span - _measR;
  uint8_t bands[VU_SPECTRUM_MAX_CHANNELS];
  // The real transform when the audio backend can offer one, the labelled simulation when it cannot (a VS1053 build),
  // so the geometry below is written once and both sources feed it. A false return can also mean "this frame's window
  // was published under us", in which case the simulation fills in for one refresh rather than the box going blank.
  uint8_t realBands[VU_SPECTRUM_MAX_CHANNELS * 2];
  const bool haveReal = player.getSpectrum(realBands, n);

  for (uint8_t ch = 0; ch < 2; ch++) {
    if (haveReal) {
      for (uint8_t b = 0; b < n; b++) bands[b] = realBands[ch * n + b];
    } else {
      _synthBands(bands, n, (ch ? lvR : lvL));
    }
    // L's bars end AT the baseline and R's start there.
    const uint16_t base = ch ? (uint16_t)(lineY + th) : lineY;
    for (uint8_t b = 0; b < n; b++) {
      const uint16_t x = (uint16_t)(x0 + (uint16_t)b * (barW + gap));
      if ((uint32_t)x + barW > full) break;
      // 255 in the band scale is a full-height bar, which is one channel's whole span.
      uint16_t h = (uint16_t)(((uint32_t)bands[b] * band) / 255);
      if (!h) h = 1;
      if (_vupeak()) {
        // Annotated: the baseline is drawn, so the bar is split at the hot zone - a vumin body with the outer HOTSEG
        // segments of the axis in vumax. A bar that does not reach the hot zone has no tip, the same threshold the
        // plain rule uses, so the two agree at the boundary.
        const uint16_t tipStart = (band > hot) ? (uint16_t)(band - hot) : 0;
        const uint16_t tip = (h > tipStart) ? (uint16_t)(h - tipStart) : 0;
        const uint16_t body = (uint16_t)(h - tip);
        if (ch) {
          _fillLocal(x, base, barW, body, _vumincolor);
          if (tip) _fillLocal(x, (uint16_t)(base + body), barW, tip, _vumaxcolor);
        } else {
          if (body) _fillLocal(x, (uint16_t)(base - body), barW, body, _vumincolor);
          if (tip)  _fillLocal(x, (uint16_t)(base - h), barW, tip, _vumaxcolor);
        }
      } else {
        // Plain: no baseline, and the bar is one colour - vumax once it reaches the hot zone.
        const uint16_t y = ch ? base : (uint16_t)(base - h);
        _fillLocal(x, y, barW, h, _hotColor(h, band));
      }
    }
  }
}

// ---- Spectrum Mirror ----
// The same two measurements the other way round: a half of the width each instead of one above the other, and the
// whole height instead of half. Both channels run low-to-high away from the divider, so the bass meets in the middle:
// R is the right half and already runs that way, L is the left half drawn backwards, both low ends against the divider.

void VuWidget::_drawSpectrumMirror(){
  const uint16_t th = _stroke();
  const uint16_t halfW = _cw / 2;
  // The height the bars grow into: above the bottom line when it is drawn, the whole box when it is not - the same
  // reservation _drawSpectrumReflect() makes at its baseline, moved to the bottom edge.
  const uint16_t avail = (_vupeak() && _ch > th) ? (uint16_t)(_ch - th) : _ch;
  const uint16_t axis = avail ? avail : 1;
  // The divider and the daylight around it. The blocks are placed from the DIVIDER outwards rather than centred in
  // their own halves: centring each half looks even-handed, but the line's own pixel comes out of the right half, so
  // the left kept two pixels of daylight and the right only one. Anchoring on the line gives the same clearance on
  // both sides - 1 + the line + 1, so 3 px where the line is one pixel thick and 4 px where it is two.
  const uint16_t day = 1;
  const uint16_t xd = (uint16_t)(halfW - th / 2);            // the divider's left edge
  const uint16_t innerGap = _vupeak() ? day : 0;   // no line drawn, no reservation made
  const uint16_t leftRoom  = (xd > innerGap) ? (uint16_t)(xd - innerGap) : 0;
  const uint16_t rightRoom = (_cw > (uint16_t)(xd + th + innerGap)) ? (uint16_t)(_cw - xd - th - innerGap) : 0;
  // The tighter of the two, so one block width fits on both sides of the line.
  const uint16_t bandSpan = (leftRoom < rightRoom) ? leftRoom : rightRoom;

  uint16_t barW = 1, gap = 1;
  const uint8_t n = _bandCount(bandSpan, &barW, &gap);   // `bandSpan`, not `span`: that is the level span
  const uint16_t used = (uint16_t)((uint32_t)n * barW + (uint32_t)((n > 1) ? (n - 1) : 0) * gap);

  // The two reference lines this style reads against: the bottom edge the bars grow from, and the divider the two
  // halves meet on. Painted first, so a bar overdraws them where it touches. Both are REFERENCE GEOMETRY, so they are
  // drawn in vuaxis; the bars are DATA and keep the vumin/vumax trace colours.
  if (_vupeak()) {
    if (_ch > th) _fillLocal(0, (uint16_t)(_ch - th), _cw, th, _vuaxiscolor);
    _fillLocal(xd, 0, th, _ch, _vuaxiscolor);
  }

  const uint16_t seg = (_bands.perheight && axis) ? (uint16_t)(axis / _bands.perheight) : 1;
  const uint16_t hot = (uint16_t)(seg * 3);   // the outer HOTSEG segments, as everywhere else
  const uint16_t span = _len ? _len : 1;
  const uint16_t lvL = span - _measL, lvR = span - _measR;
  uint8_t bands[VU_SPECTRUM_MAX_CHANNELS];
  uint8_t realBands[VU_SPECTRUM_MAX_CHANNELS * 2];
  const bool haveReal = player.getSpectrum(realBands, n);

  for (uint8_t ch = 0; ch < 2; ch++) {
    if (haveReal) {
      for (uint8_t b = 0; b < n; b++) bands[b] = realBands[ch * n + b];
    } else {
      _synthBands(bands, n, (ch ? lvR : lvL));
    }
    const bool mirrored = (ch == 0);                  // L is the left half, drawn back to front
    // Placed from the line: the left block ends innerGap before it and the right one starts innerGap after it, both
    // the same width, so the daylight around the line matches.
    const uint16_t xStart = mirrored
        ? ((xd > (uint16_t)(innerGap + used)) ? (uint16_t)(xd - innerGap - used) : 0)
        : (uint16_t)(xd + th + innerGap);
    const uint32_t limit = mirrored ? (uint32_t)xd : (uint32_t)_cw;
    for (uint8_t col = 0; col < n; col++) {
      const uint16_t x = (uint16_t)(xStart + (uint16_t)col * (barW + gap));
      if ((uint32_t)x + barW > limit) break;
      const uint8_t b = mirrored ? (uint8_t)(n - 1 - col) : col;
      uint16_t h = (uint16_t)(((uint32_t)bands[b] * axis) / 255);
      if (!h) h = 1;
      if (_vupeak()) {
        // Annotated like the Spectrum: the bottom line is drawn, so each bar is a vumin body with the outer HOTSEG
        // segments of the axis in vumax.
        const uint16_t tipStart = (axis > hot) ? (uint16_t)(axis - hot) : 0;
        const uint16_t tip = (h > tipStart) ? (uint16_t)(h - tipStart) : 0;
        const uint16_t body = (uint16_t)(h - tip);
        if (body) _fillLocal(x, (uint16_t)(axis - body), barW, body, _vumincolor);
        if (tip)  _fillLocal(x, (uint16_t)(axis - h), barW, tip, _vumaxcolor);
      } else {
        _fillLocal(x, (uint16_t)(axis - h), barW, h, _hotColor(h, axis));
      }
    }
  }
}

// ---- Waveform and Lissajous ----
// Both plot against the centre cross, so the origin is where the two axes meet: the waveform measures its amplitude
// up and down from the horizontal axis and walks the time window left to right, and the Lissajous is a point cloud
// around the same point with X from the left channel and Y from the right. Neither transposes, and both take their
// samples from the audio library, which returns false on a backend that has no PCM.
// The scratch is a file-static rather than a widget member so it comes out of .bss instead of the heap: widgets are
// heap-allocated, and the heap is what the boot-time services exhaust. It exists only where PCM can.

#if defined(USE_AUDIO_I2S)
static int16_t vuSampleBuf[VU_CAPTURE_SAMPLES * 2];
#endif

void VuWidget::_drawWave() {
#if !defined(USE_AUDIO_I2S)
  return;   // no PCM on this backend, and no buffer to draw from
#else
  const uint16_t full = _cw, hgt = _ch;
  if (full < 2 || hgt < 2) return;
  // One column per pixel of the time axis, and never more than the capture holds.
  const uint16_t n = (full < VU_CAPTURE_SAMPLES) ? full : VU_CAPTURE_SAMPLES;

  _centreCross();
  if (!player.getWaveform(vuSampleBuf, n)) return;   // no capture: the graticule alone, not a fallback

  const uint16_t th = _stroke();
  const uint16_t gain = _waveGain();
  const uint16_t mid = hgt / 2;
  const uint16_t spread = mid ? mid : 1;
  uint16_t prevY = mid;
  for (uint16_t i = 0; i < n; i++) {
    const int32_t v = vuSampleBuf[i * 2];         // the trace is the left channel; the pair is interleaved
    const int32_t vg = (v * (int32_t)gain) / 256; // the meter's reference, applied
    int32_t y = (int32_t)mid - (vg * (int32_t)spread) / 32768;
    if (y < 0) y = 0;
    if (y > (int32_t)hgt - 1) y = (int32_t)hgt - 1;
    const uint16_t yA = (uint16_t)((y < (int32_t)prevY) ? y : prevY);
    const uint16_t yB = (uint16_t)((y < (int32_t)prevY) ? prevY : y);
    const uint16_t top = (uint16_t)((yA > th / 2) ? (yA - th / 2) : 0);
    uint16_t h = (uint16_t)(yB - yA + th);
    if ((uint32_t)top + h > hgt) h = (uint16_t)(hgt - top);
    // The column advances one pixel per sample but is th wide, so consecutive columns overlap and the trace reads as
    // a line th px thick rather than a one pixel thread.
    const uint16_t w = (uint16_t)((i + th <= full) ? th : 1);
    // The hot rule is the level bar's, one dimension over: the outer HOTSEG * step px either side of the axis are
    // vumax, everything nearer is vumin, measured on the GAINED amplitude so a loud passage lights the trace edges.
    const uint32_t amp = (uint32_t)(vg < 0 ? -vg : vg);
    uint32_t lvl = amp * (uint32_t)mid / 32768;
    if (lvl > mid) lvl = mid;
    _fillLocal(i, top, w, h, _hotColor((uint16_t)lvl, mid));
    prevY = (uint16_t)y;
  }
#endif // USE_AUDIO_I2S
}

void VuWidget::_drawLissajous() {
#if !defined(USE_AUDIO_I2S)
  return;   // no PCM on this backend, and no buffer to draw from
#else
  const uint16_t full = _cw, hgt = _ch;
  if (full < 2 || hgt < 2) return;
  // A scatter rather than a column per pixel, so it wants many more points than the waveform: 256 pairs is ~5.8 ms
  // of audio, enough to show a phase shape without turning into fog.
  const uint16_t n = (VU_CAPTURE_SAMPLES < 256) ? VU_CAPTURE_SAMPLES : 256;

  _centreCross();
  if (!player.getWaveform(vuSampleBuf, n)) return;

  const uint16_t th = _stroke();
  const uint16_t gain = _waveGain();
  const uint16_t halfW = full / 2, halfH = hgt / 2;
  // Each channel gets its own half axis, so equal amplitudes in both trace a line corner to corner rather than a
  // line that only fills the shorter dimension.
  const uint16_t spreadL = halfW ? halfW : 1, spreadR = halfH ? halfH : 1;
  const uint16_t xMax = (full > th) ? (uint16_t)(full - th) : 0;
  const uint16_t yMax = (hgt > th) ? (uint16_t)(hgt - th) : 0;
  // The beam, not a scatter: every pair is joined to the one before it, which makes the shape close on itself like an
  // X-Y vectorscope instead of reading as a cloud of dots. The joint is a stepped line - one fill per pixel of the
  // longer axis - rather than a Bresenham, because the primitive here is a rect and at these sizes the eye cannot tell.
  uint16_t prevX = 0, prevY = 0;
  bool havePrev = false;
  const uint32_t seg = _bands.perheight ? (uint32_t)_bands.perheight : 1;
  const uint32_t hotNum = (seg > 3) ? (seg - 3) : 1;   // the outer HOTSEG segments of the axis
  for (uint16_t i = 0; i < n; i++) {
    const int16_t l = vuSampleBuf[i * 2], r = vuSampleBuf[i * 2 + 1];
    const int32_t lg = ((int32_t)l * (int32_t)gain) / 256;   // the meter's reference, applied
    const int32_t rg = ((int32_t)r * (int32_t)gain) / 256;
    int32_t x = (int32_t)halfW + (lg * (int32_t)spreadL) / 32768;
    int32_t y = (int32_t)halfH - (rg * (int32_t)spreadR) / 32768;
    if (x < 0) x = 0;
    if (x > (int32_t)xMax) x = (int32_t)xMax;
    if (y < 0) y = 0;
    if (y > (int32_t)yMax) y = (int32_t)yMax;
    // Hot when the beam is out in the outer HOTSEG segments of the box, measured proportionally on whichever axis it
    // is furthest out on. A single min(halfW, halfH) scale, as the first cut had, can never go hot at the left or
    // right edge of a wide box however loud the signal is.
    const uint32_t dx = (uint32_t)((x > (int32_t)halfW) ? (x - (int32_t)halfW) : ((int32_t)halfW - x));
    const uint32_t dy = (uint32_t)((y > (int32_t)halfH) ? (y - (int32_t)halfH) : ((int32_t)halfH - y));
    const bool hot = (dx * seg > (uint32_t)halfW * hotNum) || (dy * seg > (uint32_t)halfH * hotNum);
    const uint16_t colour = hot ? _vumaxcolor : _vumincolor;
    if (!havePrev) {
      _fillLocal((uint16_t)x, (uint16_t)y, th, th, colour);
    } else {
      const int32_t dx = x - (int32_t)prevX, dy = y - (int32_t)prevY;
      const int32_t adx = (dx < 0) ? -dx : dx, ady = (dy < 0) ? -dy : dy;
      const int32_t steps = (adx > ady) ? adx : ady;
      for (int32_t s = 1; s <= steps; s++) {
        const uint16_t px = (uint16_t)((int32_t)prevX + (dx * s) / steps);
        const uint16_t py = (uint16_t)((int32_t)prevY + (dy * s) / steps);
        _fillLocal(px, py, th, th, colour);
      }
      if (!steps) _fillLocal((uint16_t)x, (uint16_t)y, th, th, colour);   // a stationary beam
    }
    prevX = (uint16_t)x; prevY = (uint16_t)y; havePrev = true;
  }
#endif // USE_AUDIO_I2S
}

// Advance cur toward limit at speed px/second. The remainder is carried in acc so a rate below one pixel per frame
// still progresses, which is what makes the 44 px OLED bar and the 200 px boombox bar spend the same time falling.
static uint16_t vuFadeUp(uint16_t cur, uint16_t limit, uint16_t speed, uint32_t &acc, uint32_t dt) {
  if (cur >= limit) { acc = 0; return limit; }
  acc += (uint32_t)speed * dt;              // px * ms
  uint16_t step = acc / 1000;               // whole pixels
  if (step) {
    acc -= (uint32_t)step * 1000;
    cur += step;
    if (cur > limit) cur = limit;
  }
  return cur;
}

void VuWidget::_levels(uint16_t len, uint16_t &measL, uint16_t &measR) {
  static uint16_t mL = 0, mR = 0;
  uint16_t vulevel = player.get_VUlevel(len);
  uint8_t L = (vulevel >> 8) & 0xFF;
  uint8_t R = vulevel & 0xFF;

  // Time base. Counting the fade in display ticks made the same number behave differently on every panel and bar
  // length. dt is clamped so a long stall (a page switch, an OTA write) cannot make the bar jump.
  bool first = (_lastMs == 0);
  uint32_t now = millis();
  uint32_t dt = first ? 0 : (now - _lastMs);
  _lastMs = now ? now : 1;                  // keep it non-zero so "first" stays one-shot
  if (dt > 250) dt = 250;

  // px/second derived from the bar length, so every bar falls in VU_FADE_MS regardless of how long it is or its panel.
  uint16_t fadePxSec = (uint32_t)len * 1000 / VU_FADE_MS;
  if (fadePxSec < 1) fadePxSec = 1;
  uint16_t peakPxSec = fadePxSec / VU_PEAK_FADE_DIV;
  if (peakPxSec < 1) peakPxSec = 1;

  bool played = player.isRunning();
  if (first) {
    // First frame after init or unlock: adopt the live level rather than sweeping in.
    mL = played ? (uint16_t)L : len;
    mR = played ? (uint16_t)R : len;
    _accL = _accR = 0;
  } else if (played) {
    // Attack stays instant and unthrottled - a louder reading snaps the bar out at once. Only the receding (quieter)
    // direction is rate limited.
    if (mL < L) mL = vuFadeUp(mL, L, fadePxSec, _accL, dt);
    else        { mL = L; _accL = 0; }
    if (mR < R) mR = vuFadeUp(mR, R, fadePxSec, _accR, dt);
    else        { mR = R; _accR = 0; }
  } else {
    mL = vuFadeUp(mL, len, fadePxSec, _accL, dt);
    mR = vuFadeUp(mR, len, fadePxSec, _accR, dt);
  }
  if (mL > len) mL = len;
  if (mR > len) mR = len;
  measL = mL;
  measR = mR;

  // Peak markers. meas is the length CLEARED from the loud end, so the loudest recent reading is the SMALLEST meas
  // seen. 0xFFFF is the "not set yet" sentinel, and the snap branch collapses it on the first call, which is why
  // _holdL/_holdR are always armed before they are read.
  if (!_vupeak()) { _peakL = _peakR = 0xFFFF; return; }
  // A new high snaps the marker out and re-arms the hold; the decay only starts once that hold has expired. A level
  // that merely stays put does not re-arm it, so VU_PEAK_FREEZE_MS is really "how long the marker stays parked after
  // the level starts falling". If the marker ends up behind the bar tip, the next frame's snap pulls it forward.
  if (mL < _peakL) { _peakL = mL; _accPL = 0; _holdL = now; }
  else if ((now - _holdL) >= VU_PEAK_FREEZE_MS)
    _peakL = vuFadeUp(_peakL, len, peakPxSec, _accPL, dt);
  if (mR < _peakR) { _peakR = mR; _accPR = 0; _holdR = now; }
  else if ((now - _holdR) >= VU_PEAK_FREEZE_MS)
    _peakR = vuFadeUp(_peakR, len, peakPxSec, _accPR, dt);
}

void VuWidget::_drawBand(uint16_t pos, uint8_t ch, uint16_t h, uint16_t color) {
  uint16_t off = 0;
  if (ch) off = _bands.width + _bands.space;
  #ifdef WIDGET_DEBUG
    _fills++;                  // a rect like any other: the bars draw one per segment per channel
  #endif
  #if defined(DSP_TFT)
    if (_rotate) {
      _canvas->fillRect(pos, off, h, _bands.width, color);
    } else if (_config.align) {
      _canvas->fillRect(off + pos, 0, h, _bands.height, color);
    } else {
      _canvas->fillRect(off, pos, _bands.width, h, color);
    }
  #else
    if (_rotate) {
      dsp.fillRect(_config.left + pos, _config.top + off, h, _bands.width, color);
    } else if (_config.align) {
      dsp.fillRect(_config.left + off + pos, _config.top, h, _bands.height, color);
    } else {
      dsp.fillRect(_config.left + off, _config.top + pos, _bands.width, h, color);
    }
  #endif
}

void VuWidget::loop(){
  // Duty limiter. The display task runs every DSP_TASK_DELAY (10 ms) and the widget used to redraw on every one,
  // which was wasteful and made the fade look instant on fast panels. VU_REFRESH_MS is the FLOOR on the interval (the
  // ceiling on the frame rate), chosen to keep up with the audio core's 30-50 levels per second: go much slower and
  // the peak marker starts missing transients. The interval then stretches to what the recent frames cost, so the
  // widget spends at most 1/VU_DUTY_FACTOR of its time drawing and a heavier style is paced out rather than left to
  // eat CPU the network stack needs. This replaces VU_SAMPLE_REFRESH_DIV, a fixed guess at which styles were heavy.
  // The cost is smoothed so a single late frame cannot pin the interval and a single quick one cannot reopen the
  // throttle. The sample styles need no special case: the waveform and the Lissajous plot the whole capture every
  // frame, so their shapes are set by the capture, not by the refresh rate; the history strip advances on wall clock.
  // _redrawMs is deliberately separate from _lastMs: while they shared a variable, a frame skipped here showed up
  // there as a doubled dt. On OLED the panel flush is paid after _draw() returns, so the figure timed is the widget's
  // own cost; on TFT the blit is inside _draw() and is measured.
  const uint32_t now = millis();
  if (_redrawMs && (now - _redrawMs) < _intervalMs) return;
  _redrawMs = now;

  #ifdef WIDGET_DEBUG
    _fills = 0;
  #endif
  const uint32_t t0 = micros();
  _draw();   // _draw() itself bails out when the widget is inactive or locked
  const uint32_t cost = micros() - t0;
  _drawUs = (_drawUs * 3u + cost) / 4u;              // a quarter of every new sample

  // The screensaver's meter reports the split of its frame at three points: the first frame after the
  // fade and the peak markers have settled, then twice more.  A pinned style is what marks this instance
  // as the screensaver's - the player's box never reports.
  if (_styleOverride != 0xFF && (_frame == 3 || _frame == 10 || _frame == 30)) _reportCost();

  // While the startup services are downloading, the network core holds up to three TLS sessions and the audio stream
  // is usually up, so the floor is raised and every style redraws VU_STARTUP_SERVICES_DIV times slower until they
  // finish. The flag is the work itself, not "until the boot is stable": in SD playback the services can stay parked
  // for the whole session and the display must not be slowed for that.
  uint32_t floorMs = VU_REFRESH_MS ? VU_REFRESH_MS : 1;
  if (startup.servicesBusy()) {
    const uint32_t div = (VU_STARTUP_SERVICES_DIV > 1) ? (uint32_t)VU_STARTUP_SERVICES_DIV : 1u;
    floorMs *= div;
  }

  const uint32_t duty = _drawUs * ((VU_DUTY_FACTOR > 1) ? (uint32_t)VU_DUTY_FACTOR : 1u);
  uint32_t interval = floorMs;
  const uint32_t needMs = (duty + 999u) / 1000u;     // the owed time, rounded up to a whole ms
  if (needMs > interval) interval = needMs;
  _intervalMs = interval;

  #ifdef WIDGET_DEBUG
    // Same shape as the Core Monitor's report: counters accumulated over the window, printed every 5 s and zeroed.
    // fills/frame says whether a style is drawing rects it does not need; the draw time whether the box is too big.
    _dbgFrames++; _dbgFills += _fills; _dbgDrawUs += cost;
    if (cost > _dbgPeakUs) _dbgPeakUs = cost;
    const uint32_t elapsed = now - _dbgLogMs;
    if (elapsed >= 5000) {
      const float secs = (float)elapsed / 1000.0f;
      FUNCTIONLOG("VU.Widget", "Box %ux%u, style %u: %.1f FPS, draw %.2fms avg / %.2fms peak, interval %ums, %.1f fills/frame, free heap %u",
          (unsigned)_cw, (unsigned)_ch, (unsigned)_style(),
          (float)_dbgFrames / secs,
          _dbgFrames ? ((float)_dbgDrawUs / 1000.0f) / (float)_dbgFrames : 0.0f,
          (float)_dbgPeakUs / 1000.0f,
          (unsigned)_intervalMs,
          _dbgFrames ? (float)_dbgFills / (float)_dbgFrames : 0.0f,
          (unsigned)ESP.getFreeHeap());
      _dbgLogMs = now;
      _dbgFrames = _dbgFills = _dbgDrawUs = _dbgPeakUs = 0;
    }
  #endif
}

void VuWidget::_clear(){
  if (_rotate)
    dsp.fillRect(_config.left, _config.top, _bands.height, _bands.width * 2 + _bands.space, _bgcolor);
  else
    dsp.fillRect(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);
}

void VuWidget::_reset(){
  // Widget::lock() and Widget::moveTo() call this. Dropping the high-water marks makes the marker restart at the bar
  // tip, and clearing _lastMs makes the next frame adopt the live level instead of fading in from a stale position.
  _peakL = _peakR = 0xFFFF;
  // Whatever the previous frame drew is no longer where this widget is about to draw, so the incremental
  // path starts from "repaint everything" again.  Both callers clear the area first, so the canvas really
  // does hold only the background where the band assumes it does.
  _prevMeasL = _prevMeasR = _prevPkL = _prevPkR = 0xFFFF;
  _prevStyle = 0xFF;
  _accL = _accR = _accPL = _accPR = 0;
  _holdL = _holdR = 0;
  _lastMs = 0;
  _redrawMs = 0;              // the duty limiter starts over with the rest of the state
  _intervalMs = VU_REFRESH_MS;
  _drawUs = 0;
  _histMs = 0;                // and the history strip re-primes its own clock
  #ifdef WIDGET_DEBUG
    _dbgFrames = _dbgFills = _dbgDrawUs = _dbgPeakUs = 0;
    _dbgLogMs = millis();
    _fills = 0;
  #endif
}

// Forget where the incremental paths believe the panel is.  Something cleared the screen underneath us -
// a page switch fills the panel with the background - so both the bar band and the blit rectangle have to
// start again from "repaint everything".
void VuWidget::_invalidate() {
  _prevMeasL = _prevMeasR = _prevPkL = _prevPkR = 0xFFFF;
  _prevStyle = 0xFF;
}

// Send the canvas to the panel: the rectangle this frame's fills touched, or the whole box when sending
// that rectangle in pieces would cost more than sending everything.
//
// A sub-rectangle cannot go out in one call, because the canvas rows are strided: its rows have to be
// gathered into something contiguous first.  They are gathered a few at a time into a small scratch, and
// each group is ONE address window plus ONE data stream - the shape every other multi-row write in this
// driver uses, and the shape the full-box blit below uses.  That matters.  The first version of this sent
// one row per writePixels(), which re-enters the driver's transaction per row and so raises CSX between
// them; the panel came back with everything outside the band missing and the band itself mangled, which is
// what a broken write stream looks like.  Gathering costs about a fifth of what the wire does per pixel
// (0.2 us against 0.85, both measured - see plans/vu-framerate.md), so it is worth it whenever it saves
// even one row of traffic.
void VuWidget::_blit() {
  #if defined(DSP_TFT)
    const uint16_t dw = (_dirtyX1 > _dirtyX0) ? (uint16_t)(_dirtyX1 - _dirtyX0) : 0;
    const uint16_t dh = (_dirtyY1 > _dirtyY0) ? (uint16_t)(_dirtyY1 - _dirtyY0) : 0;
    if (!dw || !dh) return;                       // nothing on the canvas changed, so nothing goes out
    const uint16_t dx = _dirtyX0, dy = _dirtyY0;
    const uint32_t wholePx = (uint32_t)_cw * _ch;
    const uint32_t partPx  = (uint32_t)dw * dh;
    dsp.startWrite();
    if ((wholePx - partPx) <= (uint32_t)dh * 8u) {
      // What the rectangle would save is worth less than the window calls it would take: send the lot.
      dsp.setAddrWindow(_config.left, _config.top, _cw, _ch);
      dsp.writePixels((uint16_t*)_canvas->getBuffer(), _cw * _ch);
    } else {
      // Tiles of at most kBlitScratchPx pixels, in whole columns and rows, so cols * rows can never
      // exceed the scratch for any dw or dh.  Sizing the groups from dw alone did: a band wider than
      // 256 px wrote up to 288 bytes past this array on a 480 px box and smashed the display task's stack.
      constexpr uint16_t kBlitScratchPx = 256;
      uint16_t scratch[kBlitScratchPx];
      const uint16_t cols = (dw > kBlitScratchPx) ? kBlitScratchPx : dw;
      const uint16_t rows = kBlitScratchPx / cols;      // >= 1, because cols <= kBlitScratchPx
      for (uint16_t x = 0; x < dw; x = (uint16_t)(x + cols)) {
        const uint16_t tw = (uint16_t)(((uint16_t)(dw - x) < cols) ? (uint16_t)(dw - x) : cols);
        for (uint16_t y = 0; y < dh; y = (uint16_t)(y + rows)) {
          const uint16_t n = (uint16_t)(((uint16_t)(dh - y) < rows) ? (uint16_t)(dh - y) : rows);
          uint16_t* p = scratch;
          for (uint16_t r = 0; r < n; r++, p += tw)
            memcpy(p, _canvasBuf + (uint32_t)(dy + y + r) * _cw + dx + x, (size_t)tw * sizeof(uint16_t));
          dsp.setAddrWindow((int16_t)(_config.left + dx + x), (int16_t)(_config.top + dy + y),
                            (int16_t)tw, (int16_t)n);
          dsp.writePixels(scratch, (uint32_t)tw * n);
        }
      }
    }
    dsp.endWrite();
  #endif
  // An OLED panel owns its buffer and DspCore::loop() flushes it, so there is nothing to send here.
}

#endif // #if DSP_MODEL!=DSP_DUMMY
