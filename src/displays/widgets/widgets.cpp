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

#if CLOCKFONT == YO_MONO // no special character but an 8 on a 7-segment display is the same as filling in background pixels
  #define CLOCKGLOW_STRING "88:88"
#else //if CLOCKFONT == CHUNKY6_PX || CLOCKFONT == CHUNKY6 // these use a special character
  #define CLOCKGLOW_STRING "//://"
#endif

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
// with, which is theme.line for the two reference lines, metafill for the meta band, plcurrentfill for the playlist
// highlight - so a conf that says outlined draws the same pixels as one that does not until both dimensions are 2 or
// more, a one-pixel-thick rectangle being its own outline.  The frame is drawn inside the given bounds and the
// interior is left alone: clearing it to the background would erase the widgets the frame exists to enclose.
void FillWidget::_draw(){
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.left, _config.top, _width, _height, _bgcolor);
  else           dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
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

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
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
  _uppercase = uppercase;
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
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void TextWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

uint16_t TextWidget::_realLeft(bool w_fb) {
  uint16_t realwidth = (_width>0 && w_fb)?_width:dsp.width();
  uint16_t offset = w_fb?0:_config.left;
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
ScrollWidget::ScrollWidget(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  init(separator, conf, fgcolor, bgcolor);
}

ScrollWidget::~ScrollWidget() {
  if (_fb)     { delete _fb;     _fb = nullptr; }
  if (_sep)    { free(_sep);     _sep = nullptr; }
  if (_window) { free(_window);  _window = nullptr; }
}

void ScrollWidget::init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);
  if (_sep)    { free(_sep);    _sep = nullptr; }
  if (_window) { free(_window); _window = nullptr; }
  _sep = (char *) malloc(sizeof(char) * 4);
  memset(_sep, 0, 4);
  snprintf(_sep, 4, " %.*s ", 1, separator);
  // Resolved for the same reason as _text: _sepwidth is strlen-based, so a
  // multi-byte separator would otherwise be measured in bytes and drawn in
  // codepoints.  This also sanitises the "%.*s" above, which cuts on a byte and
  // can leave a broken lead byte when the separator is not ASCII.
  preTextString(_sep, displayFont());
  _x = conf.widget.left;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _sepwidth = strlen(_sep) * _charWidth;
  _width = conf.width;
  if (_width > (uint16_t)MAX_WIDTH) _width = (uint16_t)MAX_WIDTH;
  _backMove.width = _width;
  uint16_t wndsz = _width / _charWidth * 4 + 1;   /* worst-case: 4-byte UTF-8 chars */
  _window = (char *) malloc(sizeof(char) * wndsz);
  memset(_window, 0, wndsz);
  _doscroll = false;
  #ifdef PSFBUFFER
    if (_fb) _fb->freeBuffer();
    else     _fb = new psFrameBuffer(dsp.width(), dsp.height());
    // The window always starts where the conf - or a MOVE - puts it; `align` is what places the TEXT inside it,
    // in _realLeft(true).  Centring the window on the screen instead is what made `left` inert for a centred
    // scroll, and it is also why the erase rectangle and the printed text could disagree.
    _fb->begin(&dsp, _config.left, _config.top, _width, _textheight, _bgcolor);
  #endif
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
  _x = _fb->ready()?0:_config.left;
  _doscroll = _checkIsScrollNeeded();
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
        snprintf(_window, _width / _charWidth * 4 + 1, "%s", _text); //TODO
        // Truncate to visible character count
        { uint16_t maxVis = _width / _charWidth;
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
        dsp.fillRect(_config.left,  _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_config.left, _config.top);
        snprintf(_window, _width / _charWidth * 4 + 1, "%s", _text); //TODO
        { uint16_t maxVis = _width / _charWidth;
          if (utf8_strlen(_window) > maxVis) {
            char *cut = (char*)utf8_offset(_window, maxVis);
            *cut = '\0';
          }
        }
        dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.resetUTF8();
        dsp.print(_window);
        dsp.clearClipping();
      }
    } else {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(_realLeft(true), 0);
        _fb->resetUTF8();
        _fb->print(_text);
        _fb->display();
      #endif
      } else {
        dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
        dsp.setCursor(_realLeft(), _config.top);
        //dsp.setClipping({_config.left, _config.top, _width, _textheight});
        dsp.resetUTF8();
        dsp.print(_text);
        //dsp.clearClipping();
      }
    }
  }
}

void ScrollWidget::setText(const char* txt, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, txt);
  setText(buf);
}

void ScrollWidget::loop() {
  if(_locked) return;
  if (!_doscroll || _config.textsize == 0 || (dsp.getScrollId() != NULL && dsp.getScrollId() != this)) return;
  uint16_t fbl = _fb->ready()?0:_config.left;
  if (_checkDelay(_x == fbl ? _startscrolldelay : _scrolltime, _scrolldelay)) {
    _calcX();
    if (_active) _draw();
  }
}

void ScrollWidget::_clear(){
  if(_fb && _fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      // display() happens in _draw() after text is rendered — not here
    #endif
  } else {
    dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
  }
}

void ScrollWidget::_draw() {
  if(!_active || _locked) return;
  _setTextParams();
  if (_doscroll) {
    uint16_t fbl = _fb->ready()?0:_config.left;
    uint16_t _newx = fbl - _x;
    uint16_t charOffset = _newx / _charWidth;
    const char* _cursor = utf8_offset(_text, charOffset);
    uint16_t hiddenChars = charOffset;
    uint16_t textLen = utf8_strlen(_text);
    if (hiddenChars < textLen) {
      snprintf(_window, _width / _charWidth * 4 + 1, "%s%s%s", _cursor, _sep, _text);
    } else {
      uint16_t sepOffset = hiddenChars - textLen;
      const char* _scursor = utf8_offset(_sep, sepOffset);
      snprintf(_window, _width / _charWidth * 4 + 1, "%s%s", _scursor, _text);
    }
    // Truncate to visible character count so a multi-byte UTF-8 sequence straddling the window edge does not leave
    // an orphan lead byte.
    { uint16_t maxVis = _width / _charWidth;
      if (utf8_strlen(_window) > maxVis) {
        char *cut = (char*)utf8_offset(_window, maxVis);
        *cut = '\0';
      }
    }
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_x + hiddenChars * _charWidth, 0);
      _fb->resetUTF8();
      _fb->print(_window);
      _fb->display();
    #endif
    } else {
      dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_x + hiddenChars * _charWidth, _config.top);
      dsp.setClipping({_config.left, _config.top, _width, _textheight});
      dsp.resetUTF8();
      dsp.print(_window);
      dsp.resetUTF8();
      dsp.print(" ");
      dsp.clearClipping();
    }
  } else {
    if(_fb->ready()){
    #ifdef PSFBUFFER
      _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
      _fb->setCursor(_realLeft(true), 0);
      _fb->resetUTF8();
      _fb->print(_text);
      _fb->display();
    #endif
    } else {
      dsp.fillRect(_config.left, _config.top, _width, _textheight, _bgcolor);
      dsp.setCursor(_realLeft(), _config.top);
      dsp.setClipping({_realLeft(), _config.top, _width, _textheight});
      dsp.resetUTF8();
      dsp.print(_text);
      dsp.clearClipping();
    }
  }
}

void ScrollWidget::_calcX() {
  if (!_doscroll || _config.textsize == 0) return;
  _x -= _scrolldelta;
  uint16_t fbl = _fb->ready()?0:_config.left;
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

void ScrollWidget::_reset(){
  // Widget::lock() calls this, and hideByLayout() locks a widget the layout omits - one whose init() never ran, so
  // _fb is null. Bail before touching it: nothing is on screen and the framebuffer was never created. A widget the
  // layout DOES provide has always been through init(), so _fb exists on every path that reaches the rest.
  if(!_present) return;
  dsp.setScrollId(NULL);
  _x = _fb && _fb->ready()?0:_config.left;
  _scrolldelay = millis();
  _doscroll = _checkIsScrollNeeded();
  #ifdef PSFBUFFER
    _fb->freeBuffer();
    // See init(): the window is at `left`, and `align` places the text inside it.
    _fb->begin(&dsp, _config.left, _config.top, _width, _textheight, _bgcolor);
  #endif
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
  dsp.fillRect(_config.left + _outlined, _config.top + _outlined, innerWidth, innerHeight, _bgcolor);
  if (valwidth > 0) {
    dsp.fillRect(_config.left + _outlined, _config.top + _outlined, valwidth, innerHeight, _fgcolor);
  }
  _oldvalwidth = valwidth;
}

void SliderWidget::_draw() {
  if(_locked) return;
  _clear();
  if(!_active) return;
  if (_outlined) dsp.drawRect(_config.left, _config.top, _width, _height, _oucolor);
  _drawslider();
}

void SliderWidget::_clear() {
  _oldvalwidth = 0;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
}
void SliderWidget::_reset() {
  _oldvalwidth = 0;
}

/************************
      NUM & CLOCK
 ************************/
#if TIME_SIZE<15 || (TIME_SIZE==15 && CLOCKFONT==YO_MONO)
  const GFXfont* Clock_GFXfontPtr = nullptr;
  #define CLOCKFONT5x7
#else
  const GFXfont* Clock_GFXfontPtr = &Clock_GFXfont;
#endif

#if !defined(CLOCKFONT5x7)
  inline GFXglyph *pgm_read_glyph_ptr(const GFXfont *gfxFont, uint8_t c) {
    return gfxFont->glyph + c;
  }
  uint8_t _charWidth(unsigned char c){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, c - 0x20);
    return pgm_read_byte(&glyph->xAdvance);
  }
  uint16_t _textHeight(){
    GFXglyph *glyph = pgm_read_glyph_ptr(&Clock_GFXfont, '8' - 0x20);
    return pgm_read_byte(&glyph->height);
  }
#else // !defined(CLOCKFONT5x7)
  uint8_t _charWidth(unsigned char c){
    return CHARWIDTH * TIME_SIZE;
  }
  uint16_t _textHeight(){
    return CHARHEIGHT * TIME_SIZE;
  }
#endif
uint16_t _textWidth(const char *txt){
  uint16_t w = 0, l=strlen(txt);
  for(uint16_t c=0;c<l;c++) w+=_charWidth(txt[c]);
  return w;
}

/************************
      NUM WIDGET
 ************************/
void NumWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  if (_text)    { free(_text);    _text = nullptr; }
  if (_oldtext) { free(_oldtext); _oldtext = nullptr; }
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
  _textheight = TIME_SIZE/*wconf.textsize*/;
}

void NumWidget::setText(const char* txt) {
  if (!_text || !_oldtext || !txt) return;   // init() has not run - see TextWidget::setText()
  strlcpy(_text, txt, _buffsize);
  preTextString(_text, displayFont());       // resolve once - see TextWidget::setText()
  _getBounds();
  if (strcmp(_oldtext, _text) == 0) return;
  uint16_t realth = _textheight;
  if (Clock_GFXfontPtr == NULL) realth = _textheight * CHARHEIGHT;
  #ifndef CLOCKFONT5x7
    else realth = _textHeight() + 1;
  #endif
  if (_active)
  #ifndef CLOCKFONT5x7
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top-_textheight, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #else
    dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), realth, _bgcolor);
  #endif

  _oldtextwidth = _textwidth;
  _oldleft = _realLeft();
  if (_active) _draw();
}

void NumWidget::setText(int val, const char *format){
  char buf[_buffsize];
  snprintf(buf, _buffsize, format, val);
  setText(buf);
}

void NumWidget::_getBounds() {
  _textwidth= _textWidth(_text);
}

void NumWidget::_draw() {
  if(!_active || TIME_SIZE<2) return;
  dsp.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  dsp.setFont(Clock_GFXfontPtr);
  dsp.setTextColor(_fgcolor, _bgcolor);
  if(!_active) return;
  dsp.setCursor(_realLeft(), _config.top);
  dsp.print(_text);
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
  TextWidget::init(conf, bufbytes, false, fgcolor, bgcolor);
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
  _timeheight = _textHeight();
  _fullclock = TIME_SIZE>35 || DSP_MODEL==DSP_ILI9225;
  if(_fullclock) _superfont = TIME_SIZE / 17; //magick
  else if(TIME_SIZE==15 || TIME_SIZE==2) _superfont=1;
  else _superfont=0;
  _space = (5*_superfont)/2; //magick
  if(_fullclock){
    _dateheight = _superfont<4?1:2;
    _clockheight = _timeheight + _space + CHARHEIGHT * _dateheight;
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

void ClockWidget::_begin(){
  #ifdef PSFBUFFER
    uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
    _fb->begin(&dsp, _clockleft, _config.top-_timeheight, _clockwidth, _clockheight+1, bgColor);
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
  if(_fb && _fb->ready()) return _timeheight; else return _config.top;
}

void ClockWidget::_getTimeBounds() {
  _timewidth = _textWidth(CLOCKGLOW_STRING);
  uint8_t fs = _superfont>0?_superfont:TIME_SIZE;
  uint16_t rightside = CHARWIDTH * fs * 2; // seconds
  if(_fullclock){
    rightside += _space*2+1; //2space+vline
    _clockwidth = _timewidth+rightside;
  } else {
    if(_superfont==0)
      _clockwidth = _timewidth;
    else
      _clockwidth = _timewidth + rightside;
  }
  switch(_config.align){
    case WA_LEFT: _clockleft = _config.left; break;
    case WA_RIGHT: _clockleft = dsp.width()-_clockwidth-_config.left; break;
    default:
      _clockleft = (dsp.width()/2 - _clockwidth/2)+_config.left;
      break;
  }
  _dotsleft = 0;
  for (const char* p = CLOCKGLOW_STRING; *p && *p != ':'; ++p) {
    _dotsleft += _charWidth((unsigned char)*p);
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
  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  bool clockInTitle=!config.isScreensaver && _config.top<_timeheight; //DSP_SSD1306x32
  uint16_t clockColor = config.isScreensaver ? config.theme.clockss : config.theme.clock;
  uint16_t clockBgColor = config.isScreensaver ? config.theme.clockbgss : config.theme.clockbg;
  uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
  uint16_t secondsColor = config.isScreensaver ? config.theme.secondsss : config.theme.seconds;
  uint16_t dowColor = config.isScreensaver ? config.theme.dowss : config.theme.dow;
  uint16_t dateColor = config.isScreensaver ? config.theme.datess : config.theme.date;
  // _fb only exists on framebuffer (TFT) builds - guard it as getRealDsp() does
  bool showFullClockOnScreensaver = !config.isScreensaver || (_fb && _fb->ready() && config.store.screensaverFullDateTime);
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
    _clearClock();
    _getTimeBounds();
    #ifndef DSP_OLED
      if(CLOCKGLOW) {
        gfx.setTextColor(clockBgColor, bgColor);
        gfx.setCursor(_left(), _top());
        gfx.print(CLOCKGLOW_STRING);
      }
    #endif
    if(clockInTitle)
      gfx.setTextColor(config.theme.meta, config.theme.metabg);
    else
      gfx.setTextColor(clockColor, bgColor);
    uint16_t timeLeft = _left();
    const char* timeText = _timebuffer;
    if (config.store.clock12 && _timebuffer[0] == ' ') {
      timeLeft += _charWidth((unsigned char)CLOCKGLOW_STRING[0]);
      timeText = _timebuffer + 1;
    }
    gfx.setCursor(timeLeft, _top());
    gfx.print(timeText);
    if(_fullclock){
      // lines, date & dow
      _linesleft = _left()+_timewidth+_space;
      if(showFullClockOnScreensaver){
        gfx.drawFastVLine(_linesleft, _top()-_timeheight, _timeheight, config.theme.div);
        gfx.drawFastHLine(_linesleft, _top()-(_timeheight)/2, CHARWIDTH * _superfont * 2 + _space, config.theme.div);
        gfx.setFont();
        gfx.setTextSize(_superfont);
        gfx.setCursor(_linesleft+_space+1, _top()-CHARHEIGHT * _superfont);
        gfx.setTextColor(dowColor, bgColor);
        gfx.print(l10n_dow(network.timeinfo.tm_wday));
        sprintf(_tmp, "%2d %s %d", network.timeinfo.tm_mday, l10n_month(network.timeinfo.tm_mon), network.timeinfo.tm_year+1900);
        strlcpy(_datebuf, _tmp, sizeof(_datebuf));
        uint16_t _datewidth = utf8_strlen(_datebuf) * CHARWIDTH*_dateheight;
        gfx.setTextSize(_dateheight);
        #if DSP_MODEL==DSP_GC9A01A
          gfx.setCursor((dsp.width()-_datewidth)/2, _top() + _space);
        #else
          gfx.setCursor(_left()+_clockwidth-_datewidth, _top() + _space);
        #endif
        gfx.setTextColor(dateColor, bgColor);
        gfx.print(_datebuf);
      }
    }
  }
  if ((_fullclock || _superfont>0) && (!_fullclock || showFullClockOnScreensaver) && (_fullclock || showSecondsOnScreensaver)) {
    gfx.setFont();
    gfx.setTextSize(_superfont);
    if(!_fullclock){
      #ifndef CLOCKFONT5x7
        gfx.setCursor(_left()+_timewidth+_space, _top()-_timeheight+_space);
      #else
        gfx.setCursor(_left()+_timewidth+_space, _top());
      #endif
    }else{
      gfx.setCursor(_linesleft+_space+1, _top()-_timeheight);
    }
    gfx.setTextColor(secondsColor, bgColor);
    // Clear seconds area before drawing — GFXfont drawChar only paints
    // foreground pixels, so narrower glyphs (e.g. "1" after "0") leave
    // leftover pixels from the previous character.
    if (Clock_GFXfontPtr != NULL) {
      uint16_t sx = !_fullclock ? _left()+_timewidth+_space : _linesleft+_space+1;
      uint16_t sy = !_fullclock ? _top()-_timeheight+_space : _top()-_timeheight;
      gfx.fillRect(sx, sy, 2 * CHARWIDTH * _superfont, CHARHEIGHT * _superfont, bgColor);
    }
    sprintf(_tmp, "%02d", network.timeinfo.tm_sec);
    gfx.print(_tmp);
  }
  gfx.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
  gfx.setFont(Clock_GFXfontPtr);
  #ifndef DSP_OLED
    gfx.setTextColor(dots ? clockColor : (CLOCKGLOW?clockBgColor:bgColor), bgColor);
  #else
    if(clockInTitle) {
      gfx.setTextColor(dots ? config.theme.meta:config.theme.metabg, config.theme.metabg);
    }else{
      gfx.setTextColor(dots ? clockColor:bgColor, bgColor);
    }
  #endif
  dots=!dots;
  gfx.setCursor(_left()+_dotsleft, _top());
  gfx.print(":");
  gfx.setFont();
  if(_fb && _fb->ready()) _fb->display();
}

void ClockWidget::_clearClock(){
  // Nothing to clear when the layout omits the clock: the geometry below is only valid after init(), so clearing
  // would fill a rectangle at indeterminate coordinates.
  if(!_present) return;
  uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
  #ifdef PSFBUFFER
    if(_fb && _fb->ready()) { _fb->clear(); return; }
  #endif
  #ifndef CLOCKFONT5x7
    dsp.fillRect(_left(), _top()-_timeheight, _clockwidth+2, _clockheight+1, bgColor);
  #else
    dsp.fillRect(_left(), _top(), _clockwidth+1, _clockheight+1, bgColor);
  #endif
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
  // _getTimeBounds() derives _clockleft/_clockwidth from _config.left and align, and _left() returns _clockleft on
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
  dsp.drawRect(_config.left, _config.top, _dimension, _dimension, _fgcolor);
  dsp.fillRect(_config.left, _config.top + _dimension/2, _dimension, _dimension/2, _fgcolor);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);
  dsp.setTextColor(_fgcolor, _bgcolor);
  if (_bitrate == 0) _buf[0] = '\0'; // work-around so we get blank space on badge instead of 0 (for VS1053)
    else snprintf(_buf, 6, "%d", _bitrate);
  dsp.setCursor(_config.left + _dimension/2 - _charWidth*strlen(_buf)/2 + 1, _config.top + _dimension/4 - _textheight/2+1);
  dsp.print(_buf);
  dsp.setTextColor(_bgcolor, _fgcolor);
  dsp.setCursor(_config.left + _dimension/2 - _charWidth*3/2 + 1, _config.top + _dimension - _dimension/4 - _textheight/2);
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
  dsp.fillRect(_config.left, _config.top, _dimension, _dimension, _bgcolor);
}


/**************************
      PLAYLIST WIDGET
 **************************/
void PlayListWidget::init(ScrollWidget* current){
  Widget::init({0, 0, 0, WA_LEFT}, 0, 0);
  _current = current;
#if PLAYLIST_MODE_PAGED
  _plItemHeight = playlistConf_ptr->widget.textsize*(CHARHEIGHT-1)+playlistConf_ptr->widget.textsize*4;
  _plPlaylistTop = TFT_FRAMEWDT;
  _plPlaylistBottom = dsp.height() - TFT_FRAMEWDT;
  uint16_t available = _plPlaylistBottom - _plPlaylistTop;
  _plPageSize = available / _plItemHeight;
  if (_plPageSize < 1) _plPageSize = 1;
  uint16_t totalHeight = _plPageSize * _plItemHeight;
  _plYStart = _plPlaylistTop + (available - totalHeight) / 2;
  _plPageStart = 0;
  _plPrevItem = 0;
#else
  _plItemHeight = playlistConf_ptr->widget.textsize*(CHARHEIGHT-1)+playlistConf_ptr->widget.textsize*4;
  _plTtemsCount = round((float)dsp.height()/_plItemHeight);
  if(_plTtemsCount%2==0) _plTtemsCount++;
  _plCurrentPos = _plTtemsCount/2;
  _plYStart = (dsp.height() / 2 - _plItemHeight / 2) - _plItemHeight * (_plTtemsCount - 1) / 2 + playlistConf_ptr->widget.textsize*2;
#endif
}

// --- Dispatcher ---
void PlayListWidget::drawPlaylist(uint16_t currentItem) {
#if PLAYLIST_MODE_PAGED
  _drawPaged(currentItem);
#else
  _drawFade(currentItem);
#endif
}

// ==================== FADE MODE (original centered) ====================
#if !PLAYLIST_MODE_PAGED

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
    dsp.setCursor(TFT_FRAMEWDT, _plYStart + pos * _plItemHeight);
    dsp.fillRect(0, _plYStart + pos * _plItemHeight - 1, dsp.width(), _plItemHeight - 2, config.theme.background);
    dsp.print(item);
  }
}

#endif // FADE MODE

// ==================== PAGED MODE ====================
#if PLAYLIST_MODE_PAGED

void PlayListWidget::_printPLitemPaged(uint16_t stationId, uint16_t y, bool selected, const char* name){
  dsp.setTextSize(playlistConf_ptr->widget.textsize);
  uint8_t charH = CHARHEIGHT * playlistConf_ptr->widget.textsize;
  int16_t textY = y + ((int16_t)_plItemHeight - (int16_t)charH) / 2 + playlistConf_ptr->widget.textsize;
  if (selected) {
    dsp.fillRect(0, y, dsp.width(), _plItemHeight, config.theme.plcurrentfill);
    dsp.fillRect(TFT_FRAMEWDT, y, MAX_WIDTH, _plItemHeight, config.theme.plcurrentbg);
    dsp.setTextColor(config.theme.plcurrent, config.theme.plcurrentbg);
  } else {
    dsp.fillRect(0, y, dsp.width(), _plItemHeight, config.theme.background);
    dsp.setTextColor(config.theme.playlist[0], config.theme.background);
  }
  dsp.setCursor(TFT_FRAMEWDT, textY);
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

#endif // PAGED MODE


#endif // #if DSP_MODEL!=DSP_DUMMY
