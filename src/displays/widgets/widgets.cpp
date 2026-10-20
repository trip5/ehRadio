#include "../../core/options.h"
#if DSP_MODEL!=DSP_DUMMY
#include <Arduino.h>
#include "../dspcore.h"
#include "../../core/display.h"
#include "../tools/psframebuffer.h"
#include "widgets.h"
#include "../../locale/dsplocale.h"
#include "../../core/config.h"
#include "../../core/logging.h"
#include "../../core/network.h"   //  for Clock widget
#include "../../core/player.h"    //  for VU widget
#include "../../core/utility.h"

/************************
      FILL WIDGET
 ************************/
void FillWidget::init(FillConfig conf, uint16_t bgcolor){
  Widget::init(conf.widget, bgcolor, bgcolor);
  _width = conf.width;
  _height = conf.height;
  _outlined = conf.outlined;
  
}

// A filled rectangle, or its frame.  The colour is the same either way - it is the one the widget was constructed
// with, which is theme.line for the two reference lines, metabg for the meta band (line when the title is inverted)
// and plcurrentbg for the playlist highlight - so a conf that says outlined draws the same pixels as one that does
// not until both dimensions are 2 or more, a one-pixel-thick rectangle being its own outline.  The frame is drawn
// inside the given bounds and the interior is left alone: clearing it to the background would erase the widgets it
// exists to enclose.
void FillWidget::_draw(){
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.offset, _config.top, _width, _height, _bgcolor);
  else           dsp.fillRect(_config.offset, _config.top, _width, _height, _bgcolor);
}

void FillWidget::setHeight(uint16_t newHeight){
  _height = newHeight;
  //_draw();
}
/************************
      TEXT WIDGET
 ************************/
TextWidget::~TextWidget() {
  free(_text);
  free(_oldtext);
}

void TextWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
}

void TextWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  if (_text)    { free(_text);    _text = nullptr; }
  if (_oldtext) { free(_oldtext); _oldtext = nullptr; }
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _charSize(_config.textsize, _charWidth, _textheight);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _textPainted = false;   // a re-init is a new layout: the old rectangle no longer describes where this widget is
}

void TextWidget::setText(const char* txt) {
  // A widget whose init() has not run has null buffers and a zero _buffsize.  Bail rather than reach
  // strcmp(_oldtext, _text) with nulls: a predicate mistake upstairs should degrade to "nothing drawn",
  // not to a LoadProhibited boot loop.
  if (!_text || !_oldtext || !txt) return;
  strlcpy(_text, txt, _buffsize);
  // Resolve against the font once, here, rather than per glyph on every draw.
  // The text a scrolling widget re-prints each step is the same text, so the
  // chain walk is paid on change instead of 50 times a second.  It also means
  // the width computed just below and the glyphs actually drawn come from the
  // SAME bytes, instead of agreeing only because resolution happens to be 1:1.
  // Never longer than the input, so the buffer already sized for txt is enough.
  preTextString(_text, displayFont());
  // Compute width by character count (utf8_strlen) * _charWidth.
  // Pixel spacers (0x1E) are 2px wide instead of _charWidth, so adjust.
  uint16_t w = utf8_strlen(_text) * _charWidth;
  for (const char *p = _text; *p; ++p) {
    if ((unsigned char)*p == 0x1E) w += (2 - _charWidth); // spacer: 2px instead of _charWidth
  }
  _textwidth = w;
  if (strcmp(_oldtext, _text) == 0) return;
  _paint();
}

// The early return above is what makes setText() cheap, and it is also why a HIDDEN line needs something else: the
// one blank is issued when the string first becomes empty, and every tick after that the string is still empty, so
// nothing is ever painted again - whatever touched the panel in between then owns those pixels.  That is exactly
// the SD manager's countdown, whose blank state has to hold for minutes.
void TextWidget::repaint() {
  if (!_text || !_oldtext) return;
  _paint();
}

// Clears where the previous picture was, then draws the current text there.  Split out of setText() so both
// entries reach the same code.  The erase covers the UNION of the old and the new rectangle: the new one may be
// narrower, and it may also start further left, and in that case anchoring max(old, new) at min(old, new) left
// the old tail past the anchor on screen - pixels nobody erased.  The first paint has no old rectangle to
// honour, which is what _painted records: x == 0 is a legal place for a widget to be, so it cannot be the
// "nothing drawn yet" marker.
void TextWidget::_paint() {
  if (_active) {
    const uint16_t left = _realLeft();
    uint16_t from = left, to = (uint16_t)(left + _textwidth);
    if (_textPainted) {
      from = min(_oldleft, left);
      to   = max((uint16_t)(_oldleft + _oldtextwidth), to);
    }
    dsp.fillRect(from, _config.top, (uint16_t)(to - from), _textheight, _bgcolor);
  }
  _textPainted = true;
  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void TextWidget::setText(int val, const char *format){
  // A fixed array: a stack VLA is a GCC extension, and a formatted value is a number or a short label.
  // setText() truncates to the widget's own buffer anyway, which is what decides what is drawn.
  char buf[WIDGET_BUFF_SIZE];
  snprintf(buf, sizeof(buf), format, val);
  setText(buf);
}

void TextWidget::setText(const char* txt, const char *format){
  char buf[WIDGET_BUFF_SIZE];
  snprintf(buf, sizeof(buf), format, txt);
  setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb) {
  uint16_t realwidth = (_width>0 && w_fb)?_width:dsp.width();
  uint16_t offset = w_fb?0:_config.offset;
  // Text wider than the space it is being placed in would wrap these subtractions to ~65500 and paint the string
  // clean off the panel - which reads as "nothing was drawn" rather than as "drawn in the wrong place".  Park it
  // at the edge instead.  A ScrollWidget with a too-long string never reaches here: it scrolls in _draw().
  switch (_config.align) {
    case WA_CENTER: {
      // Centred in the widget's own span when it has one, and on the screen when it has not.  A scroll brings a
      // span (its width) and a MOVE hands one over, and in that span `left` is the left edge - which is what
      // lets weatherMove/weatherMoveVU shift a centred line clear of the VU rather than doing nothing at all.
      // Without a span there is nothing for `left` to be the edge of, so it stays inert, exactly as before; the
      // clock is the one that has always done something else, centring on the screen and adding `left`.
      const uint16_t span  = (_width > 0) ? _width : realwidth;
      const uint16_t start = (_width > 0) ? offset : 0;
      return (_textwidth >= span) ? 0 : (uint16_t)(start + (span - _textwidth) / 2);
    }
    case WA_RIGHT: return ((uint32_t)_textwidth + offset >= realwidth)?0:(uint16_t)(realwidth - _textwidth - offset); break;
    default: return offset; break;
  }
}

void TextWidget::_draw() {
  if(!_active) return;
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);

  // Render characters one-by-one (not byte-by-byte) so multi-byte UTF-8
  // sequences are written to the decoder as an unbroken group.  Pixel
  // spacers (0x1E = 2px) are handled per-byte as before.
  uint16_t x = _realLeft();
  const char *p = _text;
  while (*p) {
    unsigned char ch = (unsigned char)*p;
    if (ch == 0x1E) { /* 2-pixel spacer */
      x += 2;
      p++;
      continue;
    }
    uint8_t clen = 1;
    if      (ch >= 0xF0) clen = 4;
    else if (ch >= 0xE0) clen = 3;
    else if (ch >= 0xC0) clen = 2;
    dsp.setCursor(x, _config.top);
    for (uint8_t i = 0; i < clen; i++)
      dsp.write((uint8_t)p[i]);
    p += clen;
    x += _charWidth;
  }

  strlcpy(_oldtext, _text, _buffsize);
}

/************************
      SCROLL WIDGET
 ************************/

// The mark a looping line prints between its tail and its head, and what the screensaver's info line joins its own parts with.
#define SCROLL_MARK_TEXT "\xC2\xB7" // « U+00B7 "Bullet" in UTF-8: a bare 0xB7 byte is dropped as a stray continuation
#define SCROLL_MARK_ICON "\007\010" // Maple Icon (Happy Canada Day!)
// A function-local static because there is one mark shape at a time and the mark is consumed immediately either way- copied by the widget, concatenated by the screensaver line.
const char* scrollMark() {
  static char mark[6];
  #ifdef EVERYDAY_IS_CANADA_DAY
    const bool canadaDay = true;
  #else
    const bool canadaDay = (network.timeinfo.tm_mon == 6 && network.timeinfo.tm_mday == 1); // July is 6, 0-based
  #endif
  snprintf(mark, sizeof(mark), " %s ", canadaDay ? SCROLL_MARK_ICON : SCROLL_MARK_TEXT);
  return mark;
}

ScrollWidget::ScrollWidget(ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize) {
  init(conf, fgcolor, bgcolor, buffsize);
}

ScrollWidget::~ScrollWidget() {
  // The display has ONE single-scroll slot (dsp.setScrollId()), and a scroll line refuses to move while that slot names another widget.
  // A widget destroyed while it holds the slot would leave it pointing at freed memory, and then nothing would scroll.
  // it is cleared here rather than at the teardown sites, so every path   that frees widgets is covered.
  if (dsp.getScrollId() == (void*)this) dsp.setScrollId(NULL);
  if (_fb)     { delete _fb;     _fb = nullptr; }
  if (_sep)    { free(_sep);     _sep = nullptr; }
  if (_window) { free(_window);  _window = nullptr; }
}

void ScrollWidget::init(ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize) {
  TextWidget::init(conf.widget, fgcolor, bgcolor, buffsize);
  if (_window) { free(_window); _window = nullptr; }
  _x = conf.widget.offset;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _setMark(scrollMark());     // after _charSize: _sepwidth is measured with _charWidth
  _width = conf.width;
  if (_width > (uint16_t)MAX_WIDTH) _width = (uint16_t)MAX_WIDTH;
  _backMove.width = _width;
  // The span is the room this line has, not what it paints: _syncWindow() narrows the window to the text
  // whenever the text fits, and widens it back to the room only while the line scrolls.  One extra
  // character of room in _window, because a scrolling line deliberately emits the glyph that is only
  // partly visible at the edge (see _draw()).
  uint16_t wndsz = _width / _charWidth * 4 + 5;   /* worst-case: 4-byte UTF-8 chars, plus one */
  _window = (char *) malloc(sizeof(char) * wndsz);
  memset(_window, 0, wndsz);
  _winx = _winw = 0;
  _doscroll = false;
  #ifdef PSFBUFFER
    if (_fb) _fb->freeBuffer();
    else     _fb = new psFrameBuffer(dsp.width(), dsp.height());
    // The window starts as the whole span and the first _syncWindow() narrows it to the text once the
    // text is known.  `align` places the TEXT inside the window, in _realLeft(true); centring the window
    // on the screen instead is what made `left` inert for a centred scroll.  Narrowing is safe because a
    // window that was never blitted is not erased when it goes (see psFrameBuffer::freeBuffer()).
    _syncWindow(true);
  #endif
}

// Copy a mark in and re-measure it.  Allocation failure leaves the previous mark alone: a refresh that cannot
// allocate must not hand _draw() a null _sep.
void ScrollWidget::_setMark(const char* mark) {
  char* fresh = (char *) malloc(strlen(mark) + 1);
  if (!fresh) return;
  strcpy(fresh, mark);
  preTextString(fresh, displayFont());
  if (_sep) free(_sep);
  _sep = fresh;
  // Counted in codepoints, not bytes: the mark may be multi-byte (« is two), and _sepwidth is what the scroll cycle
  // is measured against - bytes would make the cycle longer than the mark it draws.
  _sepwidth = utf8_strlen(_sep) * _charWidth;
}

// The mark is the one piece of a scrolling line that can change without setText(): a holiday begins at midnight.
// scrollMark() caches its own answer for a minute, so this is a staleness check, and the rebuild behind it happens
// only when the mark really differs.
void ScrollWidget::_refreshMark() {
  if (!_sep) return;
  const char* mark = scrollMark();
  if (strcmp(_sep, mark) == 0) return;
  _setMark(mark);
}

// A scrolling paint owns the whole span: it erased and printed all of it.  Recording that as the painted rectangle
// is what lets a later, shorter text erase the row instead of only its own narrower box - without it the old line's
// pixels survive around a dialog's shorter header, which no background bar covers on a panel that has none.
void ScrollWidget::_spanPainted() {
  _oldleft = _config.offset;
  _oldtextwidth = _width;
  _textPainted = true;
}

void ScrollWidget::_setTextParams() {
  if (_config.textsize == 0) return;
  if(_fb->ready()){
  #ifdef PSFBUFFER
    _fb->setFont((GFXfont *)displayFont());
    _fb->setTextSize(_config.textsize);
    _fb->setTextColor(_fgcolor, _bgcolor);
  #endif
  }else{
    dsp.setTextSize(_config.textsize);
    dsp.setTextColor(_fgcolor, _bgcolor);
  }
}

bool ScrollWidget::_checkIsScrollNeeded() {
  return _textwidth > _width;
}

void ScrollWidget::setText(const char* txt) {
  // A ScrollWidget whose init() has not run (the layout omits it) has null buffers, a zero _buffsize and
  // a null _fb.  Without this, strlcpy would be handed _buffsize - 1 == 65535 and write into null.
  if (!_text || !_oldtext || !txt) return;
  strlcpy(_text, txt, _buffsize - 1);
  // Resolve once per change: the scroll step re-prints this window repeatedly,
  // and every window/slice offset below is computed from the resolved bytes.
  preTextString(_text, displayFont());
  if (strcmp(_oldtext, _text) == 0) return;
  _textwidth = utf8_strlen(_text) * _charWidth;
  _doscroll = _checkIsScrollNeeded();
  // Re-derive the window from the new text - but only while the line is on screen: a hidden line never
  // edits the panel, and its pixels were taken off when it hid.  _draw() syncs again on the way back.
  if (_active) _syncWindow();
  _x = (_fb && _fb->ready())?0:_config.offset;
  if (dsp.getScrollId() == this) dsp.setScrollId(NULL);
  _scrolldelay = millis();
  // Recorded whether or not the widget is drawn, so a hidden widget still knows which string it holds and a
  // later setText() with the same text is still recognised as "no change".
  strlcpy(_oldtext, _text, _buffsize);
  // Deliberately gated on _active and NOT on _locked: the OTA progress label is locked at construction
  // (display.cpp) and is drawn by nothing but this method.  A feature hide must therefore clear _active as
  // well as lock - hideWeatherIfChanged() in display.cpp does, which is what stops a line yielded to the VU
  // from reappearing here on the next weather refresh.
  if (_active) {
    _setTextParams();
    if (_doscroll) {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        snprintf(_window, _width / _charWidth * 4 + 5, "%s", _text); //TODO
        // Truncate to the visible characters PLUS ONE: the entering glyph is emitted whole and the clip
        // cuts it at the pixel it may reach, so it slides in instead of popping.
        { uint16_t maxVis = _width / _charWidth + 1;
          if (utf8_strlen(_window) > maxVis) {
            char *cut = (char*)utf8_offset(_window, maxVis);
            *cut = '\0';
          }
        }
        _fb->resetUTF8();
        _fb->print(_window);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.offset,  _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_config.offset, _config.top);
        snprintf(_window, _width / _charWidth * 4 + 5, "%s", _text); //TODO
        { uint16_t maxVis = _width / _charWidth + 1;   // the entering glyph is drawn and clipped, not dropped
          if (utf8_strlen(_window) > maxVis) {
            char *cut = (char*)utf8_offset(_window, maxVis);
            *cut = '\0';
          }
        }
        dsp.setClipping({_config.offset, _config.top, _width, _textheight});
        dsp.resetUTF8();
        dsp.print(_window);
        dsp.clearClipping();
      }
      _spanPainted();
    } else {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _winw, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        _fb->resetUTF8();
        _fb->print(_text);
        _fb->display();
      #endif
      } else {
        // Erase the union of the old and the new rectangle, like TextWidget::_paint(): the new text may be
        // narrower, or sit further left, and the tail nobody erased is what smeared.  The clip is the
        // text's OWN rectangle, never the room's, so a glyph cannot reach a neighbour on the same row.
        const uint16_t left = _realLeft();
        uint16_t from = left, to = (uint16_t)(left + _textwidth);
        if (_textPainted) {
          from = min(_oldleft, from);
          to   = max((uint16_t)(_oldleft + _oldtextwidth), to);
        }
        dsp.fillRect(from, _config.top, (uint16_t)(to - from), _textheight, _bgcolor);
        _oldleft = left; _oldtextwidth = _textwidth; _textPainted = true;
        dsp.setCursor(left, _config.top);
        dsp.setClipping({from, _config.top, (uint16_t)(to - from), _textheight});
        dsp.resetUTF8();
        dsp.print(_text);
        dsp.clearClipping();
      }
    }
  }
}

void ScrollWidget::setText(const char* txt, const char *format){
  char buf[WIDGET_BUFF_SIZE];
  snprintf(buf, sizeof(buf), format, txt);
  setText(buf);
}

// A scrolling line repaints through its own _draw(), which erases its span while it scrolls and its own text
// rectangle otherwise, and clips every print.  TextWidget::_paint() cannot serve it: its erase is unclipped and
// _textwidth here is the WHOLE string, so it uncovers the row - through a neighbour - where _draw() will never
// print.  _active and _locked are _draw()'s own gates.
void ScrollWidget::repaint() {
  _draw();
}

void ScrollWidget::loop() {
  if(_locked) return;
  _refreshMark();
  if (!_doscroll || _config.textsize == 0 || (dsp.getScrollId() != NULL && dsp.getScrollId() != this)) return;
  uint16_t fbl = _fb->ready()?0:_config.offset;
  if (_checkDelay(_x == fbl ? _startscrolldelay : _scrolltime, _scrolldelay)) {
    _calcX();
    if (_active) _draw();
  }
}

void ScrollWidget::_clear(){
  if(_fb && _fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _winw, _textheight, _bgcolor);
      // display() happens in _draw() after text is rendered — not here
    #endif
  } else {
    // No window on this path: erase the union of the last and the current rectangle, never the room.  A SCROLLING
    // line owns its span, though, and its _textwidth is the WHOLE string - erasing that uncovers the row past the
    // span, where _draw() never prints.
    const uint16_t left = _realLeft();
    uint16_t from = left, to = (uint16_t)(left + (_doscroll ? _width : _textwidth));
    if (_textPainted) {
      from = min(_oldleft, from);
      to   = max((uint16_t)(_oldleft + _oldtextwidth), to);
    }
    if (to > from) dsp.fillRect(from, _config.top, (uint16_t)(to - from), _textheight, _bgcolor);
    _textPainted = false;
  }
}

void ScrollWidget::_draw() {
  if(!_active || _locked || !_sep) return;
  _syncWindow();          // the text may have changed width since this window was last made
  _setTextParams();
  if (_doscroll) {
    uint16_t fbl = (_fb && _fb->ready())?0:_config.offset;
    uint16_t _newx = fbl - _x;
    uint16_t charOffset = _newx / _charWidth;
    const char* _cursor = utf8_offset(_text, charOffset);
    uint16_t hiddenChars = charOffset;
    uint16_t textLen = utf8_strlen(_text);
    // One MORE character than the window holds.  The glyph on its way in is emitted whole and the clip
    // cuts it at the pixel it is allowed to reach, so it slides in at the edge instead of appearing only
    // once it fits - which is what the pop-in at one end of a scrolling line was.
    const uint16_t maxVis = _width / _charWidth + 1;
    if (hiddenChars < textLen) {
      snprintf(_window, _width / _charWidth * 4 + 5, "%s%s%s", _cursor, _sep, _text);
    } else {
      uint16_t sepOffset = hiddenChars - textLen;
      const char* _scursor = utf8_offset(_sep, sepOffset);
      snprintf(_window, _width / _charWidth * 4 + 5, "%s%s", _scursor, _text);
    }
    // Truncate to that count so a multi-byte UTF-8 sequence straddling the window edge does not leave
    // an orphan lead byte.
    if (utf8_strlen(_window) > maxVis) {
      char *cut = (char*)utf8_offset(_window, maxVis);
      *cut = '\0';
    }
    if(_fb && _fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_x + hiddenChars * _charWidth, 0);
      _fb->resetUTF8();
      _fb->print(_window);
      _fb->display();
    #endif
    } else {
      dsp.fillRect(_config.offset, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_x + hiddenChars * _charWidth, _config.top);
      dsp.setClipping({_config.offset, _config.top, _width, _textheight});
      dsp.resetUTF8();
      dsp.print(_window);
      dsp.resetUTF8();
      dsp.print(" ");
      dsp.clearClipping();
    }
    _spanPainted();
  } else {
    if(_fb && _fb->ready()){
    #ifdef PSFBUFFER
      // The window IS the text here, so the text starts at 0 inside it (see _windowRect()), and the
      // fill covers the window, not the span - which is what keeps a neighbour on the row intact.
      _fb->fillRect(0, 0, _winw, _textheight, _bgcolor);
      _fb->setCursor(0, 0);
      _fb->resetUTF8();
      _fb->print(_text);
      _fb->display();
    #endif
    } else {
      // No window on this path, so the erase is the union of the old and the new rectangle - the rule
      // TextWidget::_paint() already uses - and the clip is the TEXT's own rectangle, never the room's, so
      // a glyph cannot reach a neighbour that shares the row.
      const uint16_t left = _realLeft();
      uint16_t from = left, to = (uint16_t)(left + _textwidth);
      if (_textPainted) {
        from = min(_oldleft, from);
        to   = max((uint16_t)(_oldleft + _oldtextwidth), to);
      }
      dsp.fillRect(from, _config.top, (uint16_t)(to - from), _textheight, _bgcolor);
      _oldleft = left; _oldtextwidth = _textwidth; _textPainted = true;
      dsp.setCursor(left, _config.top);
      dsp.setClipping({from, _config.top, (uint16_t)(to - from), _textheight});
      dsp.resetUTF8();
      dsp.print(_text);
      dsp.clearClipping();
    }
  }
}

void ScrollWidget::_calcX() {
  if (!_doscroll || _config.textsize == 0) return;
  _x -= _scrolldelta;
  uint16_t fbl = _fb->ready()?0:_config.offset;
  if (-_x > _textwidth + _sepwidth - fbl) {
    _x = fbl;
    dsp.setScrollId(NULL);
  } else {
    dsp.setScrollId(this);
  }
}

bool ScrollWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

// The rectangle this line paints through, in the span's own coordinates.  A line that is NOT scrolling
// owns only its own glyphs, so its window is the TEXT: the erase and the blit both stop at the last
// glyph, and a neighbour that shares the row keeps its pixels.  A line that IS scrolling takes the whole
// room, because the text sweeps across it and every column in it can be painted.
//
// _realLeft(true) is where the text sits inside the span, so the window starts there.  Inside the new
// window that same call returns 0, which is why nothing else in this class had to change.
void ScrollWidget::_windowRect(uint16_t& x, uint16_t& w){
  if (_doscroll || _textwidth == 0) { x = _config.offset; w = _width; return; }
  x = _config.offset + _realLeft(true);
  w = _textwidth;
  if (w > _width) w = _width;                // cannot happen while the text fits, but never wider than the span
  if ((uint32_t)x + w > (uint32_t)dsp.width())   // a conf that reaches past the glass: stop at the edge
    w = ((uint32_t)x < (uint32_t)dsp.width()) ? (uint16_t)(dsp.width() - x) : 1;
}

// Make the framebuffer window match the text that is about to be drawn, and keep the scroll state with
// it.  _width is left exactly as it was - it is the span the conf (or a MOVE) gave the line, and it is
// still what decides whether the line scrolls - and only the window is narrowed.
//
// force is what Widget::lock() and Widget::moveTo() need: take this line's own window off the panel even
// when the rectangle has not changed.  A window that was never blitted is not erased when it goes (see
// psFrameBuffer::freeBuffer()), which is what makes narrowing one safe - otherwise the first shrink
// would wipe the very pixels a neighbour owns.
void ScrollWidget::_syncWindow(bool force){
  if (!_present) return;
  _doscroll = _checkIsScrollNeeded();
  #ifdef PSFBUFFER
    uint16_t x = 0, w = 0;
    _windowRect(x, w);
    if (_fb && (force || !_fb->ready() || x != _winx || w != _winw)) {
      _fb->freeBuffer();                       // erases the old window - the only pixels this line owns
      _fb->begin(&dsp, x, _config.top, w, _textheight, _bgcolor);
      _winx = x; _winw = w;
    }
  #endif
}

void ScrollWidget::_reset(){
  // Widget::lock() calls this, and hideByLayout() locks a widget the layout omits - one whose init() never ran, so
  // _fb is null. Bail before touching it: nothing is on screen and the framebuffer was never created. A widget the
  // layout DOES provide has always been through init(), so _fb exists on every path that reaches the rest.
  if(!_present) return;
  dsp.setScrollId(NULL);
  _scrolldelay = millis();
  _syncWindow(true);
  _x = _fb && _fb->ready()?0:_config.offset;
}

/************************
      SLIDER WIDGET
 ************************/
void SliderWidget::init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor) {
  Widget::init(conf.widget, fgcolor, bgcolor);
  _width = conf.width; _height = conf.height; _outlined = conf.outlined; _oucolor = oucolor, _max = maxval;
  _oldvalwidth = _value = 0;
}

void SliderWidget::setValue(uint32_t val) {
  _value = val;
  if (_active && !_locked) _drawslider();

}

void SliderWidget::_drawslider() {
  uint16_t innerWidth = _width - _outlined * 2;
  uint16_t innerHeight = _height - _outlined * 2;
  uint32_t clampedValue = (_max == 0) ? 0 : min(_value, _max);
  uint16_t valwidth = (_max == 0) ? 0 : map(clampedValue, 0, _max, 0, innerWidth);
  dsp.fillRect(_config.offset + _outlined, _config.top + _outlined, innerWidth, innerHeight, _bgcolor);
  if (valwidth > 0) {
    dsp.fillRect(_config.offset + _outlined, _config.top + _outlined, valwidth, innerHeight, _fgcolor);
  }
  _oldvalwidth = valwidth;
}

void SliderWidget::_draw() {
  if(_locked) return;
  _clear();
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.offset, _config.top, _width, _height, _oucolor);
  _drawslider();
}

void SliderWidget::_clear() {
  _oldvalwidth = 0;
  dsp.fillRect(_config.offset, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset() {
  _oldvalwidth = 0;
}

/************************
      NUM & CLOCK
 ************************/
// Clock font resolution.  What a clock glyph looks like is decided by two runtime choices:
//
//   activeClockFontId  which STYLE.  dspfont.h owns it; display.cpp assigns it from
//                      config.store.clockFontId, and clockFontStyle() is the accessor.
//   the size index     which of that style's seven sizes, 0..4 (see clockSizePx() in dspfont.h).
//
// The size index is PER WIDGET: it is clockConf.textsize for the clock and numConf.textsize for the number/volume page.
// Each widget resolves its own ClockFontSel in init() and the three helpers below take it as an argument.
static ClockFontSel _resolveClockFont(uint8_t sizeIndex){
  if (sizeIndex > 4) sizeIndex = 4;
  ClockFontSel sel;
  sel.index  = sizeIndex;
  sel.font   = clockFontFor(sizeIndex);      // nullptr for index 0
  sel.textPx = clockSizePx(sizeIndex);
  if (sel.textPx == 0) sel.textPx = 1;       // the built-in cell's multiplier
  return sel;
}

// The size index a clock conf really means RIGHT NOW: the layout's own, one rung lower while the screensaver's
// Smaller Clock option is on.  Index 0 is the built-in font and the floor - there is nothing below it - so a layout
// that already asks for 0 is left alone.  Read in ClockWidget::init() rather than by the painters: the index is what
// every measurement of the clock derives from, and a font swapped under measurements taken for another is a clock
// printed into the wrong box.
static uint8_t _clockConfSize(uint8_t confsize){
  if (confsize > 0 && config.isScreensaver && config.store.screensaverClockSmaller) return (uint8_t)(confsize - 1);
  return confsize;
}

inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
  return gfxFont->glyph + c;
}
uint8_t _charWidth(const ClockFontSel& cf, unsigned char c){
  if (cf.font == nullptr) return CHARWIDTH * cf.textPx;
  GFXglyph *glyph = pgm_read_glyph_ptr(cf.font, c - 0x20);
  return pgm_read_byte(&glyph->xAdvance);
}
uint16_t _textHeight(const ClockFontSel& cf){
  if (cf.font == nullptr) return CHARHEIGHT * cf.textPx;
  GFXglyph *glyph = pgm_read_glyph_ptr(cf.font, '8' - 0x20);
  return pgm_read_byte(&glyph->height);
}
uint16_t _textWidth(const ClockFontSel& cf, const char *txt){
  uint16_t w = 0, l=strlen(txt);
  for(uint16_t c=0;c<l;c++) w+=_charWidth(cf, txt[c]);
  return w;
}

/************************
      NUM WIDGET
 ************************/
void NumWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  // This widget's own size index: numConf.textsize.
  _cf = _resolveClockFont(wconf.textsize);
  if (_text)    { free(_text);    _text = nullptr; }
  if (_oldtext) { free(_oldtext); _oldtext = nullptr; }
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  // The PIXEL size (15/35/52/70, or the built-in cell for index 0), not the size index: it is the
  // built-in font's multiplier on the index-0 path and the erase offset on the GFXfont path, where
  // the ink sits ABOVE the cursor because every clock glyph has yOffset = -height.
  _textheight = _cf.textPx;
}

void NumWidget::setText(const char* txt) {
  if (!_text || !_oldtext || !txt) return;   // init() has not run - see TextWidget::setText()
  strlcpy(_text, txt, _buffsize);
  preTextString(_text, displayFont());       // resolve once - see TextWidget::setText()
  _getBounds();
  if (strcmp(_oldtext, _text) == 0) return;
  uint16_t realth = _textheight;
  uint16_t eraseTop = _config.top;
  if (_cf.font == nullptr) {
    realth = _textheight * CHARHEIGHT;
  } else {
    realth = _textHeight(_cf) + 1;
    eraseTop = _config.top - _textheight;
  }
  // The erase covers the UNION of the old and the new rectangle.  The old width alone is not enough: the
  // new text may be narrower, or start further left, and anchoring max(old, new) at min(old, new) is the
  // only rule that covers both.  _textPainted is the "something is on the panel" flag - x == 0 is a legal
  // position and so cannot be the sentinel it used to be (see TextWidget::_paint()).
  if (_active) {
    const uint16_t left = _realLeft();
    uint16_t from = left, to = (uint16_t)(left + _textwidth);
    if (_textPainted) {
      from = min(_oldleft, from);
      to   = max((uint16_t)(_oldleft + _oldtextwidth), to);
    }
    dsp.fillRect(from, eraseTop, (uint16_t)(to - from), realth, _bgcolor);
    _oldleft = left; _oldtextwidth = _textwidth; _textPainted = true;
  }
  if (_active) _draw();
}

void NumWidget::setText(int val, const char *format){
  char buf[WIDGET_BUFF_SIZE];
  snprintf(buf, sizeof(buf), format, val);
  setText(buf);
}

void NumWidget::setFieldMax(uint32_t maxval) {
  if (maxval == _fieldmax) return;
  _fieldmax = maxval;
  // The field changed, not the digits: make the next setText() repaint even when the value is the same.
  if (_oldtext) _oldtext[0] = '\0';
}

// The clock's three glow tests, unchanged: a clock font to take the glow glyph from, the stored setting, and a
// backdrop that differs from the glow colour.  A one-bit palette sets clockbg == background, so no such panel
// can glow and needs no panel macro here.
bool NumWidget::_glowOn(){
  return _cf.font != nullptr && editorOpt(config.store.clockglow)
         && config.theme.clockbg != config.theme.background;
}

void NumWidget::_getBounds() {
  _digitwidth = _textWidth(_cf, _text);
  // The field is the cells the widest value needs.  _textwidth becomes the field's width, so _realLeft() places
  // the reserved box and the ones column never moves as the value grows; without a glow it stays the digits' width.
  uint8_t cells = 1;
  for (uint32_t v = _fieldmax; v > 9; v /= 10) cells++;
  const uint8_t shown = (uint8_t)strlen(_text);   // "%d" only: one byte per digit
  if (shown > cells) cells = shown;
  uint8_t g = 0;
  if (_glowOn() && _text[0]) {
    const char ch = clockFontStyle().glowChar[0];
    if (cells > sizeof(_glowField) - 1) cells = sizeof(_glowField) - 1;
    for (; g < cells; g++) _glowField[g] = ch;
    _textwidth = (uint16_t)(cells * ::_charWidth(_cf, ch));   // the free helper: _charWidth is also a TextWidget member
  } else {
    _textwidth = _digitwidth;
  }
  _glowField[g] = '\0';
  if (_textwidth < _digitwidth) _textwidth = _digitwidth;
}

void NumWidget::_draw() {
  // Size index 0 draws with the SYSTEM font and its ink sits BELOW _config.top; a clock font's sits ABOVE it.
  // This widget draws to the panel directly, so it needs none of the clock's framebuffer care.
  if(!_active) return;
  const uint16_t x = _realLeft();
  if (_glowOn() && _text[0]) {
    // Glow cells first, then the digits over the right-hand ones.  Both prints use the ONE-argument
    // setTextColor(), which is transparent: an opaque print paints each glyph's background and would erase
    // the glow's unlit segments - the very thing the effect is.
    dsp.fillRect(x, _config.top - _textheight, _textwidth, (uint16_t)(_textHeight(_cf) + 1), _bgcolor);
    dsp.setTextSize(1);
    dsp.setFont(_cf.font);
    dsp.setTextColor(config.theme.clockbg);
    dsp.setCursor(x, _config.top);
    dsp.print(_glowField);
    dsp.setTextColor(_fgcolor);
    dsp.setCursor(x + (_textwidth - _digitwidth), _config.top);
    dsp.print(_text);
  } else {
    dsp.setTextSize(_cf.font==nullptr?_cf.textPx:1);
    dsp.setFont(_cf.font);
    dsp.setTextColor(_fgcolor, _bgcolor);
    dsp.setCursor(x, _config.top);
    dsp.print(_text);
  }
  // Recorded here as well as in setText(), because a page switch reaches this through setActive() alone.
  _oldleft = x; _oldtextwidth = _textwidth; _textPainted = true;
  strlcpy(_oldtext, _text, _buffsize);
  dsp.setFont();
}

/**************************
      PROGRESS WIDGET
 **************************/
// One blob element, appended one at a time so no format string has to reason about byte precision. \026 is the
// VOL_75 wave glyph from icons.h: an ICON codepoint, so it is drawn on the same seven-row grid as the speaker and
// the boot glyph at either end, unlike a font bullet whose vertical metrics sit off centre against them. One byte,
// so characters and bytes coincide here.
static const char PROGRESS_DOT[] = "\026";
#define PROGRESS_DOT_BYTES 1

void ProgressWidget::init(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor,
                          const char* frameLeft, const char* frameGlyph) {
  _frameL = (frameLeft != nullptr) ? frameLeft : "";
  _frameR = (frameGlyph != nullptr) ? frameGlyph : "";
  _speed = pconf.speed;
  _barwidth = pconf.barwidth;
  _scrolldelay = 0;    // read by _checkDelay() and not set anywhere else
  _pg = 0;
  _fieldX = 0;
  _oldLead = _oldDots = 0;
  _painted = false;
  _runway = (pconf.width > 2) ? (uint16_t)(pconf.width - 2) : 1;
  // BYTES, not characters: every dot in the runway costs one byte more than the column it occupies. The size has to
  // be right before this call and nothing may be assigned after it - TextWidget::init() ends in Widget::init(), which
  // zeroes _width.
  const uint16_t bufbytes = (uint16_t)(strlen(_frameL) + _runway + strlen(_frameR)
                                       + _barwidth * (PROGRESS_DOT_BYTES - 1) + 1);
  TextWidget::init(conf, fgcolor, bgcolor, bufbytes);
}

// Where the blob sits on this frame: it grows in at the speaker, slides right one column per frame, and then its head
// is eaten at the far end - the dots disappearing into the boot glyph.
void ProgressWidget::_blob(uint16_t& lead, uint16_t& dots) const {
  lead = (_pg <= _barwidth) ? 0 : (uint16_t)(_pg - _barwidth);
  if (lead > _runway) lead = _runway;
  dots = (_pg <= _barwidth) ? _pg : _barwidth;
  if (dots > (uint16_t)(_runway - lead)) dots = (uint16_t)(_runway - lead);
}

// One column of the runway: the middle dot, or the background that erases one. Deliberately cell sized, because this
// stops a TFT flashing the whole line eleven times a second.
void ProgressWidget::_dotCell(uint16_t col, bool on) {
  const uint16_t x = (uint16_t)(_fieldX + col * _charWidth);
  if (on) {
    dsp.setTextColor(_fgcolor, _bgcolor);
    dsp.setFont();
    dsp.setTextSize(_config.textsize);
    dsp.setCursor(x, _config.top);
    for (uint8_t i = 0; i < PROGRESS_DOT_BYTES; i++) dsp.write((uint8_t)PROGRESS_DOT[i]);
  } else {
    dsp.fillRect(x, _config.top, _charWidth, _textheight, _bgcolor);
  }
}

// Full paint: the speaker, the runway with the blob where it belongs, the boot glyph. Only activation and layout
// changes come through here, so the two static glyphs are drawn once and then left alone. Character columns are
// placed exactly as _dotCell() places them - _realLeft() plus one _charWidth per character - which keeps the dots
// from shifting when a full paint replaces a delta one.
void ProgressWidget::_draw() {
  if (!_active || _text == nullptr || _buffsize == 0) return;
  uint16_t lead = 0, dots = 0;
  _blob(lead, dots);
  int n = snprintf(_text, _buffsize, "%s%*s", _frameL, (int)lead, "");
  for (uint16_t i = 0; i < dots && (n + PROGRESS_DOT_BYTES) < (int)_buffsize; i++) {
    memcpy(_text + n, PROGRESS_DOT, PROGRESS_DOT_BYTES);
    n += PROGRESS_DOT_BYTES;
  }
  _text[n] = '\0';
  snprintf(_text + n, _buffsize - n, "%*s%s", (int)(_runway - lead - dots), "", _frameR);
  _textwidth = (uint16_t)(utf8_strlen(_text) * _charWidth);
  _fieldX = (uint16_t)(_realLeft() + strlen(_frameL) * _charWidth);
  // The erase below covers the whole line, so it does not need the old bounds - but keep TextWidget's own bookkeeping
  // in step for anything that reads it
  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  dsp.fillRect(_realLeft(), _config.top, _textwidth, _textheight, _bgcolor);
  TextWidget::_draw();
  _oldLead = lead;
  _oldDots = dots;
  _painted = true;
}

void ProgressWidget::_progress() {
  if (_buffsize == 0 || _runway == 0) return;   // init() has not run: stay inert
  if (!_painted) { _draw(); return; }           // never delta-paint against a picture we did not paint
  _pg++;
  // The single dot at the far end is the last frame of the cycle, so the next one is the blank frame - stopping a
  // frame earlier than the runway would is what keeps that to ONE blank frame instead of two.
  if (_pg > (uint8_t)(_runway + _barwidth - 1)) _pg = 0;
  uint16_t lead = 0, dots = 0;
  _blob(lead, dots);
  // Only the cells this frame and the last one disagree about are touched: at most two of them, against a whole-line
  // erase plus fourteen glyph writes before. The static glyphs are never part of this.
  for (uint16_t c = _oldLead; c < (uint16_t)(_oldLead + _oldDots); c++)
    if (c < lead || c >= (uint16_t)(lead + dots)) _dotCell(c, false);
  for (uint16_t c = lead; c < (uint16_t)(lead + dots); c++)
    if (c < _oldLead || c >= (uint16_t)(_oldLead + _oldDots)) _dotCell(c, true);
  _oldLead = lead;
  _oldDots = dots;
}

bool ProgressWidget::_checkDelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

void ProgressWidget::loop() {
  if (_checkDelay(_speed, _scrolldelay)) {
    _progress();
  }
}

/**************************
      CLOCK WIDGET
  **************************/
ClockWidget::~ClockWidget() {
  if (_fb) { delete _fb; _fb = nullptr; }
}

void ClockWidget::init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(wconf, fgcolor, bgcolor);
  // clockConf.textsize is this clock's size index.  CLOCK_SIZE (the panel default dspfont.h derives
  // from DSP_HEIGHT) is only what the layout tooling fills a new layout with, so a conf that says
  // nothing still gets the size that panel has always had.  _clockConfSize() is where the screensaver's
  // Smaller Clock option takes its rung off - everything below is derived from the answer, so nothing
  // else has to know about the option.
  _cf = _resolveClockFont(_clockConfSize(wconf.textsize));
  // The two shapes this clock draws with, built from the active style's ONE glow character.  A clock
  // glyph's advance is what the union glyph carries - makefont.py injects it at the digit width - so
  // <g><g>:<g><g> measures the clock exactly as "00:00" would, which is why it doubles as the width
  // template read in _getTimeBounds().
  //
  // Size index 0 has no clock font to ask, so the digit '0' stands in: that is the member's own
  // initialiser, and '0' is the one character every system font is guaranteed to have at CHARWIDTH
  // (validateSystemFonts() checks it).  There it is only ever measured, never printed - index 0 is
  // the 128x32 OLED and the glow is TFT-only.
  const char g = _cf.font ? clockFontStyle().glowChar[0] : '0';
  _glow[0] = _glow[1] = g;
  _glow[2] = ':';
  _glow[3] = _glow[4] = g;
  _glowSec[0] = _glowSec[1] = g;
  _timeheight = _textHeight(_cf);
  // The two transforms, straight from the layout.  _fullclock was "TIME_SIZE>35 || ILI9225" and
  // the seconds block was inferred from _superfont being non-zero; both are per-layout booleans now
  // (LayoutData.fullClock / .seconds), which is what lets a 35 px panel show a date column and a
  // 15 px one show seconds without either being a size rule or a model test.
  _fullclock   = fullClock_ptr ? *fullClock_ptr : false;
  _showSeconds = seconds_ptr   ? *seconds_ptr   : false;
  // _superfont is the size INDEX now, not TIME_SIZE/17 (which is the same thing for the two sizes
  // that had it: 52/17 = 3 and 70/17 = 4).  It stays because the DOW string's built-in textSize,
  // _space and _clockheight are all derived from it.
  _superfont = _cf.index;
  _space = (5*_superfont)/2; //magick
  // One row's worth of gap for this size, for the seconds' gap and for the date's own row.  Identical to
  // _space for 1..4, and a whole built-in character row at index 0 - where _space is 0, which is what put the
  // date ON the time's row there.  A clock font's ink sits above its cursor and the built-in cell's below it,
  // so a full row is the honest gap at that size.
  _rowgap = (_superfont == 0) ? (uint8_t)CHARHEIGHT : _space;
  // The date's row height in built-in cells: one, or two at the 70 px size whose date is drawn larger.  Sized
  // for every case rather than only inside the fullClock branch, because it is also what the compact box and
  // the erase math read.
  _dateheight = _superfont<4?1:2;
  if(_cf.font == nullptr){
    // Size index 0 draws with the SYSTEM font, and its cell sits BELOW the cursor where a clock glyph's sits
    // ABOVE it.  The window therefore starts at the clock's own `top` (see _begin) and has to carry that cell
    // itself, plus the date's row when fullClock is on - 8, or 16.  Sized for a clock font instead, only the
    // digits' first row fell inside the window with fullClock off, which is what "a few pixels, all in the same
    // row" was.
    _clockheight = (uint8_t)(CHARHEIGHT * _cf.textPx * (1 + (_fullclock ? _dateheight : 0)));
  } else if(_fullclock){
    _clockheight = _timeheight + _rowgap + CHARHEIGHT * _dateheight;
  } else {
    _clockheight = _timeheight;
  }
  _getTimeBounds();
  #ifdef PSFBUFFER
    if (_fb) _fb->freeBuffer();
    else     _fb = new psFrameBuffer(dsp.width(), dsp.height());
    _begin();
  #endif
}

// Rebuild the clock for the size the current screen state gives it, and do nothing when that is the size it is
// already drawn at.  The Display calls this on the way into the screensaver and on the way back out, because the
// option is not a layout change and init() is the only place a size is resolved.  The conf is an ARGUMENT, never
// _config: moveTo()/moveBack() write _config.offset/top, so re-initing from it would take the moved position as the
// widget's own and lose the layout's for the rest of the session.
void ClockWidget::refreshSize(WidgetConfig conf){
  if (_cf.index == _clockConfSize(conf.textsize)) return;
  init(conf, _fgcolor, _bgcolor);
}

void ClockWidget::_begin(){
  #ifdef PSFBUFFER
    uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
    // How far ABOVE `_config.top` the window has to reach.  A clock font's glyph has yOffset = -height, so its
    // ink is above its baseline and the whole _timeheight must be inside the window; the system font's ink is
    // below its cursor, so at size index 0 there is nothing to reserve and the window starts AT `top`.  _top()
    // reports the matching row base inside the window, so the two have to be read together.  Computed here
    // rather than taken from _top() because on the very first call the buffer does not exist yet and _top()
    // would still answer with the panel row.
    const uint16_t topOff = (_cf.font == nullptr) ? 0 : _timeheight;
    _fb->begin(&dsp, _clockleft, _config.top - topOff, _clockwidth, _clockheight+1, bgColor);
  #endif
}

bool ClockWidget::_getTime(){
  char newTimeBuffer[20];
  if (config.store.clock12) strftime(newTimeBuffer, sizeof(newTimeBuffer), "%l:%M", &network.timeinfo);
  if (!config.store.clock12) strftime(newTimeBuffer, sizeof(newTimeBuffer), "%H:%M", &network.timeinfo);
  bool timeChanged = (strcmp(_timebuffer, newTimeBuffer) != 0);
  strcpy(_timebuffer, newTimeBuffer);
  bool ret = network.timeinfo.tm_sec==0 || _forceflag!=network.timeinfo.tm_year || timeChanged;
  _forceflag = network.timeinfo.tm_year;
  return ret;
}

uint16_t ClockWidget::_left(){
  if(_fb && _fb->ready()) return 0; else return _clockleft;
}
uint16_t ClockWidget::_top(){
  // The buffer row where this clock's ink begins.  A clock font's ink ENDS at _timeheight (the date row
  // follows below it); the system font's BEGINS there, so at size index 0 the row base is the window's own row
  // 0 - the system font would otherwise raise the time a whole cell.  Must agree with _begin()'s window.
  if(_fb && _fb->ready()) return (_cf.font == nullptr) ? 0 : _timeheight;
  else return _config.top;
}

// The date line the full clock prints, built in one place so the print and the box's geometry always agree:
// at the compact sizes the DATE is usually the wider of the clock's two rows, and the box is sized for it.
void ClockWidget::_dateString(){
  sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday, l10n_month(network.timeinfo.tm_mon), network.timeinfo.tm_year+1900);
  strlcpy(_datebuf, _tmp, sizeof(_datebuf));
}

// Where a row of width rowW starts, for the sizes that align each of the clock's rows on its own.  The box is
// the wider of the two rows, so an inset inside it IS that row's own alignment: at WA_CENTER both rows centre
// on the same axis, at WA_RIGHT both end at the box's right edge, at WA_LEFT both start at its left.
uint16_t ClockWidget::_rowLeft(uint16_t rowW){
  if(!_compactClock() || rowW >= _clockwidth) return _left();
  switch(_config.align){
    case WA_CENTER: return (uint16_t)(_left() + (_clockwidth - rowW)/2);
    case WA_RIGHT:  return (uint16_t)(_left() + (_clockwidth - rowW));
    default:        return _left();
  }
}

// Whether this clock glows at all: the stored setting AND a clock font to glow with.  The size index 0 case is
// why it is a predicate rather than a repeated test - init() can only put the digit '0' in _glow/_glowSec when
// there is no clock font (they exist to be MEASURED there: _timewidth and _secwidth are read off them), so
// printing them would paint a literal "00:00" behind the time and "00" behind the seconds.  Size 0 therefore
// draws no glow anywhere, and the WebUI does not offer the setting while the clock is that size (GETACTIVE in
// netserver.cpp).  Three draws ask: the time's glow, the seconds' glow, and the colon's OFF colour - that last
// one is deliberately the glow colour so the block stays unbroken while the colon blinks, which is exactly what
// must not be punched into the background at size 0.  The theme's own colour test stays with each caller, which
// is the only place that knows which pair is in question: clockbg vs background, or secondsbg vs background.
bool ClockWidget::_glowOn(){
  return _cf.font != nullptr && editorOpt(config.store.clockglow);
}

void ClockWidget::_getTimeBounds() {
  // _glow is built from this style's glow character in init(), so a live font switch rebuilds it
  // there exactly as a layout change does - nothing here reads a macro.
  if(_fullclock) _dateString();     // the width below is a max() of the two rows, and the date is one of them
  _timewidth = _textWidth(_cf, _glow);
  uint8_t fs = _superfont>0?_superfont:_cf.textPx;
  // How wide the seconds block is.  With a seconds font in this style it is MEASURED from that
  // font's own advances, not computed: the seconds are a different face from the clock (its 10 px
  // digit is 8 px wide where a 15 px one is 12), so only the seconds font can answer.  Without one,
  // the built-in cell is CHARWIDTH * textSize like every other row.
  uint16_t rightside;
  const GFXfont* secFont = secondsFontFor(_cf.index);
  if (secFont) {
    ClockFontSel secSel = _cf;     // same index and textPx, the seconds FACE
    secSel.font = secFont;
    _secwidth = _textWidth(secSel, _glowSec);
  } else {
    _secwidth = (uint16_t)(CHARWIDTH * fs * 2);
  }
  rightside = _secwidth;
  // The clock's own row, needed by the box below and by the print (which aligns the two rows separately at the
  // compact sizes).  The one expression serves every size; only the compact ones read it.
  _rowwidth = (uint16_t)(_timewidth + (_showSeconds ? _rowgap + _secwidth : 0));
  if(_fullclock){
    if(_compactClock()){
      // No divider column at these sizes, so the box is exactly the two rows' own needs: the time with the
      // seconds after it, and the date below it.  The DATE can be the wider of the two (at index 0 it usually
      // is), so the box takes whichever is wider - which is also what keeps a framebuffer window from clipping
      // a row that aligning into a narrower box would push out of the window.
      const uint16_t datew = (uint16_t)(utf8_strlen(_datebuf) * CHARWIDTH * _dateheight);
      _clockwidth = (datew > _rowwidth) ? datew : _rowwidth;
    } else {
      rightside += _space*2+1; //2space+vline
      _clockwidth = _timewidth+rightside;
    }
  } else {
    // Anything to the right of the time is the layout's .seconds, not _superfont: with _superfont
    // now the size index it is non-zero for every panel that has a clock font, which would have
    // widened the clock on the 35 px family by 24 px and shifted it under WA_RIGHT/WA_CENTER.
    if(!_showSeconds)
      _clockwidth = _timewidth;
    else
      _clockwidth = _timewidth + _rowgap + rightside;
  }
  switch(_config.align){
    case WA_LEFT: _clockleft = _config.offset; break;
    case WA_RIGHT: _clockleft = dsp.width()-_clockwidth-_config.offset; break;
    default:
      _clockleft = (dsp.width()/2 - _clockwidth/2)+_config.offset;
      break;
  }
  _dotsleft = 0;
  for (const char* p = _glow; *p && *p != ':'; ++p) {
    _dotsleft += _charWidth(_cf, (unsigned char)*p);
  }
}

Adafruit_GFX& ClockWidget::getRealDsp(){
  #ifdef PSFBUFFER
    if (_fb && _fb->ready()) return *_fb;
  #endif
  return dsp;
}

void ClockWidget::_printClock(bool force){
  // The one place the time reaches the screen, whichever caller arrived: the tick-driven draw(), the forced _draw()
  // that Pager::setPage() and the layout helpers use, and the screensaver. Nothing is printed until the device has a
  // time it can stand behind (clockTrustworthy() in utility.cpp) - a zeroed timeinfo prints 00:00 and a chip in another
  // zone prints a plausible-looking wrong time. The first genuine paint needs no help: ticks() and doSync() both
  // request CLOCK the moment they have something to show.
  if (!clockTrustworthy()) return;
  auto& gfx = getRealDsp();
  gfx.setTextSize(_cf.font==nullptr?_cf.textPx:1);
  gfx.setFont(_cf.font);
  bool clockInTitle=!config.isScreensaver && _config.top<_timeheight; //DSP_SSD1306x32
  uint16_t clockColor = config.isScreensaver ? config.theme.clockss : config.theme.clock;
  uint16_t clockBgColor = config.isScreensaver ? config.theme.clockbgss : config.theme.clockbg;
  uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
  uint16_t secondsColor = config.isScreensaver ? config.theme.secondsss : config.theme.seconds;
  uint16_t secondsBgColor = config.isScreensaver ? config.theme.secondsbgss : config.theme.secondsbg;
  uint16_t dowColor = config.isScreensaver ? config.theme.dowss : config.theme.dow;
  uint16_t dateColor = config.isScreensaver ? config.theme.datess : config.theme.date;
  // _fb only exists on framebuffer (TFT) builds - guard it as getRealDsp() does.
  // What the full clock's own column may draw HERE: the divider, the day and the date need the framebuffer - a
  // one-bit panel's screensaver has no colours to draw them in - and at the two COMPACT sizes the screensaver
  // shows the basic time, because fullClock there is the DATE on the MAIN screen, on a row of its own.
  // `!config.isScreensaver` short-circuits first, so no part of this can reach the main screen.
  bool showFullClockOnScreensaver = !config.isScreensaver
                                    || (_fb && _fb->ready() && config.store.screensaverFullDateTime && !_compactClock());
  // The seconds are the LAYOUT's feature, so on every panel they follow the screensaver's own setting.  They used
  // to follow the full clock's permission as well, which is what asked for the framebuffer - so on an OLED,
  // turning fullClock on took the seconds off the screensaver entirely (see the draw gate below).
  bool showSecondsOnScreensaver = !config.isScreensaver || config.store.screensaverFullDateTime;
  static bool wasScreensaver = false;
  if (wasScreensaver != config.isScreensaver) {
    force = true;
    wasScreensaver = config.isScreensaver;
    #ifdef PSFBUFFER
      _reset();  // reinitialize framebuffer with new bgColor
    #endif
  }
  if(force){
    _clearClock(false);   // wipe the buffer only: this path paints and displays the whole window below
    _getTimeBounds();
    // Where the time's row starts.  At the compact sizes each of the clock's two rows is aligned on its own, so
    // the time's row starts at its own inset rather than at the box's edge (above those sizes _rowLeft is the
    // box's edge, i.e. no change).  The glow goes HERE too, not at the box's edge: it fills the cells the digits
    // sit in, so it has to start exactly where they do - and at the compact sizes the box is the wider of the two
    // rows, so the box's edge sits up to the date's own width away from the digits.
    uint16_t timeLeft = _rowLeft(_rowwidth);
    // Theme-aware rather than panel-type-based: the glow is only meaningful where the clock's own
    // background differs from what the digits sit on, so on a one-bit panel (clockbg == background)
    // this is the same no-op the old !DSP_OLED guard gave, while a colour theme that sets them equal
    // is now skipped too.  _glowOn() holds the other half of the rule: no clock font, no glow.
    if(_glowOn() && clockBgColor != bgColor) {
      gfx.setTextColor(clockBgColor, bgColor);
      gfx.setCursor(timeLeft, _top());
      gfx.print(_glow);
    }
    if(clockInTitle)
      gfx.setTextColor(config.theme.meta, config.theme.metabg);
    else
      gfx.setTextColor(clockColor, bgColor);
    const char* timeText = _timebuffer;
    if (config.store.clock12 && _timebuffer[0] == ' ') {
      timeLeft += _charWidth(_cf, (unsigned char)_glow[0]);
      timeText = _timebuffer + 1;
    }
    gfx.setCursor(timeLeft, _top());
    gfx.print(timeText);
    if(_fullclock){
      // lines, date & dow
      _linesleft = _left()+_timewidth+_space;
      // A compact clock has no divider column, so the divider and the day of week are dropped rather than
      // squeezed into it - there is a single pixel for the line and an 8 px row for the DOW beside a 15 px
      // time.  The date is the part worth keeping, and it takes the row below on its own.
      // The size test STAYS here as well as in the flag: the flag's own one sits in the right-hand half of an
      // `||` that the main screen short-circuits, so it only answers for the screensaver.
      if(showFullClockOnScreensaver && !_compactClock()){
        gfx.drawFastVLine(_linesleft, _top()-_timeheight, _timeheight, config.theme.div);
        gfx.drawFastHLine(_linesleft, _top()-(_timeheight)/2, CHARWIDTH * _superfont * 2 + _space, config.theme.div);
        gfx.setFont();
        gfx.setTextSize(_superfont);
        gfx.setCursor(_linesleft+_space+1, _top()-CHARHEIGHT * _superfont);
        gfx.setTextColor(dowColor, bgColor);
        gfx.print(l10n_dow(network.timeinfo.tm_wday));
      }
      if(showFullClockOnScreensaver){
        _dateString();
        uint16_t _datewidth = utf8_strlen(_datebuf) * CHARWIDTH*_dateheight;
        gfx.setFont();      // the date is a built-in-font row, whatever face the time is drawn in
        gfx.setTextSize(_dateheight);
        uint16_t dx;
        if(_compactClock()){
          // On its own row the date is aligned independently, by the same rule the time's row uses.
          dx = _rowLeft(_datewidth);
        } else {
          #if DSP_MODEL==DSP_GC9A01A
            dx = (dsp.width()-_datewidth)/2;
          #else
            dx = _left()+_clockwidth-_datewidth;
          #endif
        }
        gfx.setCursor(dx, _top() + _rowgap);
        gfx.setTextColor(dateColor, bgColor);
        gfx.print(_datebuf);
      }
    }
  }
  // What the seconds column is asked for.  At the two compact sizes there is no divider column for the
  // seconds to sit in, so fullClock there means the DATE ALONE: they are drawn only when the layout asks
  // for them with .seconds - and then _rowwidth already counts them, so the time's row, the alignment and
  // the ink all agree.  Drawing them while the box left them out is exactly what made the centred time
  // look off.  Above those sizes fullClock has always implied the seconds beside the divider, and it
  // still does.
  //
  // WHERE they may draw is their OWN question, though: `showSecondsOnScreensaver` is the user's setting on the
  // screensaver and true everywhere else, whatever fullClock says.  The gate used to AND in the full clock's
  // screensaver permission when `_fullclock` was set, and that permission is the framebuffer's - so on a panel
  // without one, a layout with fullClock on showed no seconds on the screensaver at all, while the same layout
  // without fullClock showed them.  A seconds column is the layout's feature, not the date column's.
  const bool drawSeconds = _showSeconds || (_fullclock && !_compactClock());
  if (drawSeconds && showSecondsOnScreensaver) {
    // The seconds column.  With a seconds sibling in the style (10, 15, 21 or 28 px for size index
    // 1..4 - see secondsSizePx()) the digits are drawn with the CLOCK font, ink-top aligned with
    // where the built-in cell's ink has always begun; without a sibling, the built-in font at
    // _superfont as before.  Both paths keep the same ink top and the same two-glyph width: the
    // ladder's digit is 6 px per index step, which is exactly what _getTimeBounds() reserves as
    // CHARWIDTH * fs * 2, so a style can be swapped without moving the column.
    const GFXfont* secFont = secondsFontFor(_cf.index);
    uint8_t  secPx = secondsSizePx(_cf.index);
    // The x: the divider column's gap goes with the divider, so the compact sizes - and every size without
    // fullClock - put the seconds one row-gap after the time, and only the divider sizes keep _linesleft.
    uint16_t sx = (_fullclock && !_compactClock()) ? (uint16_t)(_linesleft+_space+1)
                                                   : (uint16_t)(_rowLeft(_rowwidth)+_timewidth+_rowgap);
    // The y is the ink top of the time's own row, at every size and in BOTH fullClock states.  With no clock font
    // (size index 0) that row BEGINS at the cursor instead of ending there, so the seconds follow the time rather
    // than being raised a row above it.  There used to be a `+ _space` on the no-fullClock path - a legacy nudge
    // that dropped the seconds below the digits' first row (2 px at size 1, and 5/7/10 at 2..4), so the same clock
    // looked a row different depending on whether the layout also asked for the date.  The seconds font's own
    // glyphs carry yOffset = -height exactly as the clock's do, so this y IS their ink top.
    uint16_t sy = (_cf.font == nullptr) ? _top() : (uint16_t)(_top()-_timeheight);  // the ink top
    if (secFont) {
      // Clear first: a GFXfont paints only the lit pixels, so a narrower glyph ("1" after "0")
      // leaves the previous character's pixels behind.  Then the glow, which fills the cells the
      // digits sit in - the same treatment clockbg gives the clock, in this style's own character.
      gfx.fillRect(sx, sy, _secwidth, secPx, bgColor);
      if (_glowOn() && secondsBgColor != bgColor) {
        gfx.setFont(secFont);
        gfx.setTextSize(1);
        gfx.setTextColor(secondsBgColor, bgColor);
        gfx.setCursor(sx, sy + secPx);   // yOffset is -height: the cursor is a whole cell below
        gfx.print(_glowSec);
      }
      gfx.setFont(secFont);
      gfx.setTextSize(1);
      gfx.setTextColor(secondsColor, bgColor);
      gfx.setCursor(sx, sy + secPx);
    } else {
      gfx.setFont();
      // _superfont is the size INDEX and is 0 with no clock font, where the built-in cell's multiplier is
      // _cf.textPx - the same substitution _getTimeBounds() makes when it measures the seconds' width.
      const uint8_t fs = _superfont > 0 ? _superfont : _cf.textPx;
      // Clear the pair's cells first, exactly as the sibling branch above does and for the same reason: on a
      // framebuffer a glyph paints only its LIT pixels (psFrameBuffer::write() renders through _writeGlyph(),
      // which never touches a background), so "35" leaves behind every pixel "36" does not cover and the seconds
      // smear into one another.  They are the one thing redrawn in place every second - the time and the date
      // get the force path's clear.  _secwidth is exactly the pair's width and CHARHEIGHT * fs its height.
      gfx.fillRect(sx, sy, _secwidth, (uint16_t)(CHARHEIGHT * fs), bgColor);
      gfx.setTextSize(fs);
      gfx.setTextColor(secondsColor, bgColor);
      gfx.setCursor(sx, sy);
    }
    sprintf(_tmp, "%02d", network.timeinfo.tm_sec);
    gfx.print(_tmp);
  }
  gfx.setTextSize(_cf.font==nullptr?_cf.textPx:1);
  gfx.setFont(_cf.font);
  #ifndef DSP_OLED
    gfx.setTextColor(dots ? clockColor : (_glowOn()?clockBgColor:bgColor), bgColor);
  #else
    if(clockInTitle) {
      gfx.setTextColor(dots ? config.theme.meta:config.theme.metabg, config.theme.metabg);
    }else{
      gfx.setTextColor(dots ? clockColor : (_glowOn()?clockBgColor:bgColor), bgColor);
    }
  #endif
  dots=!dots;
  // The colon is drawn here, apart from the time string, so it is the position that is easiest to forget: it
  // follows the same row base as the time itself (identical to the box's edge above the compact sizes).
  gfx.setCursor(_rowLeft(_rowwidth)+_dotsleft, _top());
  gfx.print(":");
  gfx.setFont();
  if(_fb && _fb->ready()) _fb->display();
}

void ClockWidget::_clearClock(bool push){
  // Nothing to clear when the layout omits the clock: the geometry below is only valid after init(), so clearing
  // would fill a rectangle at indeterminate coordinates.
  if(!_present) return;
  uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
  #ifdef PSFBUFFER
    if(_fb && _fb->ready()) {
      // psFrameBuffer::clear() fills the PSRAM buffer ONLY - it never touches the panel - so an erase that means
      // to take the clock off the screen has to display the wiped window itself.  That is `push`, true for every
      // caller except _printClock()'s force path, which paints this same window and displays it a few lines
      // later (and must not blit a wiped window first: that would show as a flash).
      //
      // Deliberately NOT gated on painted().  lock() calls _reset() first, which recreates the buffer and so
      // clears _painted, so a guard here would skip exactly the erase that matters.  The OLED path below fills
      // its rect unconditionally too, and the clock owns this rectangle for its whole life - so the two families
      // behave identically.
      // An erase that does not take is worth being able to read, so the diagnostic stays under WIDGET_DEBUG: it
      // prints the window in PANEL coordinates, the framebuffer's own size, and whether the panel ever received
      // it.  _left()/_top() are BUFFER coordinates - a clock at top 218 with a 70 px cell lives at panel y 148 -
      // so it is this pair, not those, that matters when the rectangle is the suspect.
      #ifdef WIDGET_DEBUG
        FUNCTIONLOG("Widgets.debug", "erase=FRAMEBUFFER active=%d locked=%d win=%d,%d %dx%d fb=%dx%d push=%d painted=%d",
                    (int)isActive(), (int)locked(), (int)_clockleft, (int)(_config.top - _timeheight),
                    (int)_clockwidth, (int)(_clockheight + 1), (int)_fb->width(), (int)_fb->height(),
                    (int)push, (int)_fb->painted());
      #endif
      _fb->clear();
      if (push) _fb->display();
      return;
    }
  #endif
  if (_cf.font == nullptr) {
    #ifdef WIDGET_DEBUG
      FUNCTIONLOG("Widgets.debug", "erase=fillRect active=%d locked=%d rect=%d,%d %ux%u",
                  (int)isActive(), (int)locked(), (int)_left(), (int)_top(),
                  (unsigned)(_clockwidth+1), (unsigned)(_clockheight+1));
    #endif
    dsp.fillRect(_left(), _top(), _clockwidth+1, _clockheight+1, bgColor);
  } else {
    #ifdef WIDGET_DEBUG
      FUNCTIONLOG("Widgets.debug", "erase=fillRect active=%d locked=%d rect=%d,%d %ux%u",
                  (int)isActive(), (int)locked(), (int)_left(), (int)(_top()-_timeheight),
                  (unsigned)(_clockwidth+2), (unsigned)(_clockheight+1));
    #endif
    dsp.fillRect(_left(), _top()-_timeheight, _clockwidth+2, _clockheight+1, bgColor);
  }
}

void ClockWidget::draw(){
  if(!_active || _locked) return;
  _printClock(_getTime());
}

void ClockWidget::_draw(){
  if(!_active || _locked) return;
  _printClock(true);
}

void ClockWidget::_reset(){
  if(!_present) return;   // omit-by-layout: _fb was never created, so there is nothing to reset
  // _getTimeBounds() derives _clockleft/_clockwidth from _config.offset and align, and _left() returns _clockleft on
  // non-framebuffer builds - so this has to run on every display, not just PSFBUFFER ones, or a moveTo()'s horizontal
  // component is silently ignored (the vertical one works, because _top() reads _config.top directly).
  _getTimeBounds();
  #ifdef PSFBUFFER
    if(_fb && _fb->ready()) {
      _fb->freeBuffer();
      _begin();
    }
  #endif
}

void ClockWidget::_clear(){
  _clearClock();
}

/**************************
      BITRATE WIDGET
 **************************/
void BitrateWidget::init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor){
  Widget::init(bconf.widget, fgcolor, bgcolor);
  _dimension = bconf.dimension;
  _bitrate = 0;
  _format = BF_UNKNOWN;
  _charSize(bconf.widget.textsize, _charWidth, _textheight);
  memset(_buf, 0, 6);
}

void BitrateWidget::setBitrate(uint16_t bitrate){
  _bitrate = bitrate;
  if(_bitrate>999) _bitrate=999;
  _draw();
}

void BitrateWidget::setFormat(BitrateFormat format){
  _format = format;
  _draw();
}

//TODO move to parent
void BitrateWidget::_charSize(uint8_t textsize, uint8_t& width, uint16_t& height){
  width = textsize * CHARWIDTH;
  height = textsize * CHARHEIGHT;
}

void BitrateWidget::_draw(){
  // A hidden badge is still handed setBitrate() and setFormat() on every DBITRATE, and the _clear() below is not
  // gated the way the text path's paint is, so without this a layout that asks for no badge would have a
  // background rectangle painted where the badge would have been.  _locked is deliberately not tested: the text
  // path ignores it too, because the OTA label relies on a locked widget still drawing.
  if (!_active) return;
  _clear();
  if(!_active || (_format == BF_UNKNOWN && _bitrate==0)) return;
  dsp.drawRect(_config.offset, _config.top, _dimension, _dimension, _fgcolor);
  dsp.fillRect(_config.offset, _config.top + _dimension/2, _dimension, _dimension/2, _fgcolor);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.setTextColor(_fgcolor, _bgcolor);
  if (_bitrate == 0) _buf[0] = '\0'; // work-around so we get blank space on badge instead of 0 (for VS1053)
    else snprintf(_buf, 6, "%d", _bitrate);
  dsp.setCursor(_config.offset + _dimension/2 - _charWidth*strlen(_buf)/2 + 1, _config.top + _dimension/4 - _textheight/2+1);
  dsp.print(_buf);
  dsp.setTextColor(_bgcolor, _fgcolor);
  dsp.setCursor(_config.offset + _dimension/2 - _charWidth*3/2 + 1, _config.top + _dimension - _dimension/4 - _textheight/2);
  switch(_format){
    case BF_MP3:  dsp.print("MP3"); break;
    case BF_AAC:  dsp.print("AAC"); break;
    case BF_FLAC: dsp.print("FLC"); break;
    case BF_WAV:  dsp.print("WAV"); break;
    case BF_VOR:  dsp.print("OGG"); break;
    case BF_OPU:  dsp.print("OPU"); break;
    default:                        break;
  }
}

void BitrateWidget::_clear() {
  dsp.fillRect(_config.offset, _config.top, _dimension, _dimension, _bgcolor);
}


/**************************
      PLAYLIST WIDGET
 **************************/
void PlayListWidget::init(ScrollWidget* current){
  Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
  _current = current;
  // The mode is a runtime option now, so both geometries are computed and _paged remembers the one init() took.  It
  // is re-taken whenever the widget is re-inited, which is what APPLYSTATE does.
  _paged = editorOpt(config.store.playlistPages);
  _plItemHeight = playlistConf_ptr->widget.textsize*(CHARHEIGHT-1)+playlistConf_ptr->widget.textsize*4;
  if (_paged) {
    _plPlaylistTop = TFT_BORDER;
    _plPlaylistBottom = dsp.height() - TFT_BORDER;
    uint16_t available = _plPlaylistBottom - _plPlaylistTop;
    _plPageSize = available / _plItemHeight;
    if (_plPageSize < 1) _plPageSize = 1;
    uint16_t totalHeight = _plPageSize * _plItemHeight;
    _plYStart = _plPlaylistTop + (available - totalHeight) / 2;
    _plPageStart = 0;
    _plPrevItem = 0;
  } else {
    _plTtemsCount = round((float)dsp.height()/_plItemHeight);
    if(_plTtemsCount%2==0) _plTtemsCount++;
    _plCurrentPos = _plTtemsCount/2;
    _plYStart = (dsp.height() / 2 - _plItemHeight / 2) - _plItemHeight * (_plTtemsCount - 1) / 2 + playlistConf_ptr->widget.textsize*2;
  }
}

// currentTop() used to be two inline bodies behind one #if.  It is one function with both shapes now, because the
// option is runtime: both of them position the playlist highlight, so it takes the branch init() took.
uint16_t PlayListWidget::currentTop() {
  if (!_paged) return _plYStart + _plCurrentPos * _plItemHeight;
  if (_plPrevItem == 0) return _plYStart;
  uint8_t slot = (_plPrevItem - _plPageStart);
  return _plYStart + slot * _plItemHeight;
}

// --- Dispatcher ---
void PlayListWidget::drawPlaylist(uint16_t currentItem) {
  if (_paged) _drawPaged(currentItem);
  else        _drawFade(currentItem);
}

// ==================== FADE MODE (original centered) ====================

uint8_t PlayListWidget::_fillPlMenu(int from, uint8_t count) {
  static char names[31][STATION_FIELD_LENGTH / 2];
  uint8_t safeCount = min(count, (uint8_t)31);
  uint16_t stationsCount = utility.fillPlaylistRange(from, safeCount, names);
  if (stationsCount == 0) return 0;
  for (uint8_t c = 0; c < safeCount; ++c) {
    int stationId = from + c;
    if (stationId < 1 || stationId > stationsCount) { _printPLitem(c, ""); continue; }
    if (config.store.numplaylist && names[c][0] != '\0') {
      String label = String(stationId) + " " + names[c];
      _printPLitem(c, label.c_str());
    } else { _printPLitem(c, names[c]); }
  }
  return safeCount;
}

void PlayListWidget::_drawFade(uint16_t currentItem) {
  uint8_t lastPos = _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  if(lastPos<_plTtemsCount){
    dsp.fillRect(0, lastPos*_plItemHeight+_plYStart, dsp.width(), dsp.height()/2, config.theme.background);
  }
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  dsp.setTextSize(playlistConf_ptr->widget.textsize);
  if (pos == _plCurrentPos) {
    _current->setText(item);
  } else {
    uint8_t plColor = (abs(pos - _plCurrentPos)-1)>4?4:abs(pos - _plCurrentPos)-1;
    dsp.setTextColor(config.theme.playlist[plColor], config.theme.background);
    dsp.setCursor(TFT_BORDER, _plYStart + pos * _plItemHeight);
    dsp.fillRect(0, _plYStart + pos * _plItemHeight - 1, dsp.width(), _plItemHeight - 2, config.theme.background);
    dsp.print(item);
  }
}

// ==================== PAGED MODE ====================

void PlayListWidget::_printPLitemPaged(uint16_t stationId, uint16_t y, bool selected, const char* name){
  dsp.setTextSize(playlistConf_ptr->widget.textsize);
  uint8_t charH = CHARHEIGHT * playlistConf_ptr->widget.textsize;
  int16_t textY = y + ((int16_t)_plItemHeight - (int16_t)charH) / 2 + playlistConf_ptr->widget.textsize;
  if (selected) {
    dsp.fillRect(0, y, dsp.width(), _plItemHeight, config.theme.plcurrentbg);
    dsp.fillRect(TFT_BORDER, y, MAX_WIDTH, _plItemHeight, config.theme.plcurrentbg);
    dsp.setTextColor(config.theme.plcurrent, config.theme.plcurrentbg);
  } else {
    dsp.fillRect(0, y, dsp.width(), _plItemHeight, config.theme.background);
    dsp.setTextColor(config.theme.playlist[0], config.theme.background);
  }
  dsp.setCursor(TFT_BORDER, textY);
  if (name && name[0] != '\0') {
    if (config.store.numplaylist) {
      char label[STATION_FIELD_LENGTH / 2 + 6];
      snprintf(label, sizeof(label), "%d %s", stationId, name);
      dsp.print(label);
    } else {
      dsp.print(name);
    }
  }
}

void PlayListWidget::_drawPaged(uint16_t currentItem) {
  if (currentItem < 1) currentItem = 1;
  uint16_t cs = utility.playlistLength();
  if (cs == 0) return;

  uint8_t page = (currentItem - 1) / _plPageSize;
  uint16_t newPageStart = page * _plPageSize + 1;
  uint8_t safeCount = min(_plPageSize, (uint8_t)31);
  static char names[31][STATION_FIELD_LENGTH / 2];

  // Same-page partial update: redraw just the two changed slots
  if (_plPrevItem != 0 && _plPageStart == newPageStart) {
    utility.fillPlaylistRange(_plPageStart, safeCount, names);
    uint8_t prevSlot = (_plPrevItem - _plPageStart);
    uint8_t newSlot  = (currentItem - _plPageStart);
    if (prevSlot < safeCount) {
      _printPLitemPaged(_plPrevItem, _plYStart + prevSlot * _plItemHeight, false, names[prevSlot]);
    }
    if (newSlot < safeCount) {
      _printPLitemPaged(currentItem, _plYStart + newSlot * _plItemHeight, true, names[newSlot]);
    }
    _plPrevItem = currentItem;
    return;
  }

  // Full page draw: clear area, draw all items, selector is just another item
  _plPageStart = newPageStart;
  _plPrevItem = currentItem;
  dsp.fillRect(0, _plPlaylistTop, dsp.width(), _plPlaylistBottom - _plPlaylistTop, config.theme.background);
  uint16_t stationsCount = utility.fillPlaylistRange(_plPageStart, safeCount, names);
  for (uint8_t i = 0; i < safeCount; i++) {
    uint16_t itemIndex = _plPageStart + i;
    if (itemIndex > stationsCount) break;
    _printPLitemPaged(itemIndex, _plYStart + i * _plItemHeight, itemIndex == currentItem, names[i]);
  }
}


#endif // #if DSP_MODEL!=DSP_DUMMY
