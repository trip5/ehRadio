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
  
}

void FillWidget::_draw(){
  if(!_active) return;
  dsp.fillRect(_config.left, _config.top, _width, _height, _bgcolor);
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
  #ifndef DSP_LCD
    width = textsize * CHARWIDTH;
    height = textsize * CHARHEIGHT;
  #else
    width = 1;
    height = 1;
  #endif
}

void TextWidget::init(WidgetConfig wconf, uint16_t buffsize, bool uppercase, uint16_t fgcolor, uint16_t bgcolor) {
  Widget::init(wconf, fgcolor, bgcolor);
  _buffsize = buffsize;
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _charSize(_config.textsize, _charWidth, _textheight);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
}

void TextWidget::setText(const char* txt) {
  strlcpy(_text, txt, _buffsize);
  /* Compute width by character count (utf8_strlen) * _charWidth.
     Pixel spacers (0x1E) are 2px wide instead of _charWidth, so adjust. */
  uint16_t w = utf8_strlen(_text) * _charWidth;
  for (const char *p = _text; *p; ++p) {
    if ((unsigned char)*p == 0x1E) w += (2 - _charWidth); /* spacer: 2px instead of _charWidth */
  }
  _textwidth = w;
  if (strcmp(_oldtext, _text) == 0) return;
  if (_active) dsp.fillRect(_oldleft == 0 ? _realLeft() : min(_oldleft, _realLeft()),  _config.top, max(_oldtextwidth, _textwidth), _textheight, _bgcolor);
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
  switch (_config.align) {
    case WA_CENTER: return (realwidth - _textwidth) / 2; break;
    case WA_RIGHT: return (realwidth - _textwidth - (!w_fb?_config.left:0)); break;
    default: return !w_fb?_config.left:0; break;
  }
}

void TextWidget::_draw() {
  if(!_active) return;
  dsp.setTextColor(_fgcolor, _bgcolor);
  dsp.setFont();
  dsp.setTextSize(_config.textsize);

  /* Render characters one-by-one (not byte-by-byte) so multi-byte UTF-8
     sequences are written to the decoder as an unbroken group.  Pixel
     spacers (0x1E = 2px) are handled per-byte as before. */
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
  free(_fb);
  free(_sep);
  free(_window);
}

void ScrollWidget::init(const char* separator, ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor) {
  TextWidget::init(conf.widget, conf.buffsize, conf.uppercase, fgcolor, bgcolor);
  _sep = (char *) malloc(sizeof(char) * 4);
  memset(_sep, 0, 4);
  snprintf(_sep, 4, " %.*s ", 1, separator);
  _x = conf.widget.left;
  _startscrolldelay = conf.startscrolldelay;
  _scrolldelta = conf.scrolldelta;
  _scrolltime = conf.scrolltime;
  _charSize(_config.textsize, _charWidth, _textheight);
  _sepwidth = strlen(_sep) * _charWidth;
  _width = conf.width;
  _backMove.width = _width;
  _window = (char *) malloc(sizeof(char) * (MAX_WIDTH / _charWidth * 4 + 1));  /* worst-case: 4-byte UTF-8 chars */
  memset(_window, 0, (MAX_WIDTH / _charWidth + 1));  // +1?
  _doscroll = false;
  #ifdef PSFBUFFER
    _fb = new psFrameBuffer(dsp.width(), dsp.height());
    uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
    _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
  #endif
}

void ScrollWidget::_setTextParams() {
  if (_config.textsize == 0) return;
  if(_fb->ready()){
  #ifdef PSFBUFFER
    _fb->setFont((GFXfont *)&DisplayFont);
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
  strlcpy(_text, txt, _buffsize - 1);
  if (strcmp(_oldtext, _text) == 0) return;
  _textwidth = utf8_strlen(_text) * _charWidth;
  _x = _fb->ready()?0:_config.left;
  _doscroll = _checkIsScrollNeeded();
  if (dsp.getScrollId() == this) dsp.setScrollId(NULL);
  _scrolldelay = millis();
  if (_active) {
    _setTextParams();
    if (_doscroll) {
      if(_fb->ready()){
      #ifdef PSFBUFFER
        _fb->fillRect(0, 0, _width, _textheight, _bgcolor);
        _fb->setCursor(0, 0);
        snprintf(_window, _width / _charWidth * 4 + 1, "%s", _text); //TODO
        /* Truncate to visible character count */
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
    strlcpy(_oldtext, _text, _buffsize);
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
  if(_fb->ready()){
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
    /* Truncate to visible character count so a multi-byte UTF-8 sequence
       straddling the window edge doesn't leave an orphan lead byte. */
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
      #ifndef DSP_LCD
        dsp.resetUTF8();
        dsp.print(" ");
      #endif
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
  dsp.setScrollId(NULL);
  _x = _fb->ready()?0:_config.left;
  _scrolldelay = millis();
  _doscroll = _checkIsScrollNeeded();
  #ifdef PSFBUFFER
    _fb->freeBuffer();
    uint16_t _rl = (_config.align==WA_CENTER)?(dsp.width()-_width)/2:_config.left;
    _fb->begin(&dsp, _rl, _config.top, _width, _textheight, _bgcolor);
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
      VU WIDGET
 ************************/
#if !defined(DSP_LCD) && !defined(DSP_OLED)
VuWidget::~VuWidget() {
  if(_canvas) free(_canvas);
}
  
void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
  _vumaxcolor = vumaxcolor;
  _vumincolor = vumincolor;
  _bands = bands;
  _canvas = new Canvas(_bands.width * 2 + _bands.space, _bands.height);
}


void VuWidget::_draw(){
  if(!_active || _locked) return;
  #if defined(USE_AUDIO_VS1053)
  /*  static uint8_t cc = 0;
    cc++;
    if(cc>0){
      player.getVUlevel();
      cc=0;
    }*/
  #endif
  static uint16_t measL, measR;
  uint16_t bandColor;
  uint16_t dimension = _config.align?_bands.width:_bands.height;
  uint16_t vulevel = player.get_VUlevel(dimension);
  
  uint8_t L = (vulevel >> 8) & 0xFF;
  uint8_t R = vulevel & 0xFF;
  
  bool played = player.isRunning();
  if(played){
    measL=(L>=measL)?measL + _bands.fadespeed:L;
    measR=(R>=measR)?measR + _bands.fadespeed:R;
  }else{
    if(measL<dimension) measL += _bands.fadespeed;
    if(measR<dimension) measR += _bands.fadespeed;
  }
  if(measL>dimension) measL=dimension;
  if(measR>dimension) measR=dimension;
  uint8_t h=(dimension/_bands.perheight)-_bands.vspace;
  _canvas->fillRect(0,0,_bands.width * 2 + _bands.space,_bands.height, _bgcolor);
  for(int i=0; i<dimension; i++){
    if(i%(dimension/_bands.perheight)==0){
      if(_config.align){
        if (!*boomboxStyle_ptr) {
          bandColor = (i>_bands.width-(_bands.width/_bands.perheight)*4)?_vumaxcolor:_vumincolor;
          _canvas->fillRect(i, 0, h, _bands.height, bandColor);
          _canvas->fillRect(i + _bands.width + _bands.space, 0, h, _bands.height, bandColor);
        } else {
          bandColor = (i>(_bands.width/_bands.perheight))?_vumincolor:_vumaxcolor;
          _canvas->fillRect(i, 0, h, _bands.height, bandColor);
          bandColor = (i>_bands.width-(_bands.width/_bands.perheight)*3)?_vumaxcolor:_vumincolor;
          _canvas->fillRect(i + _bands.width + _bands.space, 0, h, _bands.height, bandColor);
        }
      }else{
        bandColor = (i<(_bands.height/_bands.perheight)*3)?_vumaxcolor:_vumincolor;
        _canvas->fillRect(0, i, _bands.width, h, bandColor);
        _canvas->fillRect(_bands.width + _bands.space, i, _bands.width, h, bandColor);
      }
    }
  }
  if(_config.align){
    if (!*boomboxStyle_ptr) {
      _canvas->fillRect(_bands.width-measL, 0, measL, _bands.width, _bgcolor);
      _canvas->fillRect(_bands.width * 2 + _bands.space - measR, 0, measR, _bands.width, _bgcolor);
      dsp.drawRGBBitmap(_config.left, _config.top, _canvas->getBuffer(), _bands.width * 2 + _bands.space, _bands.height);
    } else {
      _canvas->fillRect(0, 0, _bands.width-(_bands.width-measL), _bands.width, _bgcolor);
      _canvas->fillRect(_bands.width * 2 + _bands.space - measR, 0, measR, _bands.width, _bgcolor);
      dsp.startWrite();
      dsp.setAddrWindow(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height);
      dsp.writePixels((uint16_t*)_canvas->getBuffer(), (_bands.width * 2 + _bands.space)*_bands.height);
      dsp.endWrite();
    }
  }else{
    _canvas->fillRect(0, 0, _bands.width, measL, _bgcolor);
    _canvas->fillRect(_bands.width + _bands.space, 0, _bands.width, measR, _bgcolor);
      dsp.startWrite();
      dsp.setAddrWindow(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height);
      dsp.writePixels((uint16_t*)_canvas->getBuffer(), (_bands.width * 2 + _bands.space)*_bands.height);
      dsp.endWrite();
  }
}

void VuWidget::loop(){
  if(_active || !_locked) _draw();
}

void VuWidget::_clear(){
  dsp.fillRect(_config.left, _config.top, _bands.width * 2 + _bands.space, _bands.height, _bgcolor);
}
#else // DSP_LCD
VuWidget::~VuWidget() { }
void VuWidget::init(WidgetConfig wconf, VUBandsConfig bands, uint16_t vumaxcolor, uint16_t vumincolor, uint16_t bgcolor) {
  Widget::init(wconf, bgcolor, bgcolor);
}
void VuWidget::_draw(){ }
void VuWidget::loop(){ }
void VuWidget::_clear(){ }
#endif

/************************
      NUM & CLOCK
 ************************/
#if !defined(DSP_LCD)
  #if TIME_SIZE<15 || (TIME_SIZE==15 && CLOCKFONT==YO_MONO)
    const GFXfont* Clock_GFXfontPtr = nullptr;
    #define CLOCKFONT5x7
  #else
    const GFXfont* Clock_GFXfontPtr = &Clock_GFXfont;
  #endif
#endif //!defined(DSP_LCD)

#if !defined(CLOCKFONT5x7) && !defined(DSP_LCD)
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
#else //!defined(CLOCKFONT5x7) && !defined(DSP_LCD)
  uint8_t _charWidth(unsigned char c){
  #ifndef DSP_LCD
    return CHARWIDTH * TIME_SIZE;
  #else
    return 1;
  #endif
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
  _text = (char *) malloc(sizeof(char) * _buffsize);
  memset(_text, 0, _buffsize);
  _oldtext = (char *) malloc(sizeof(char) * _buffsize);
  memset(_oldtext, 0, _buffsize);
  _textwidth = _oldtextwidth = _oldleft = 0;
  _uppercase = uppercase;
  _textheight = TIME_SIZE/*wconf.textsize*/;
}

void NumWidget::setText(const char* txt) {
  strlcpy(_text, txt, _buffsize);
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
  #ifndef DSP_LCD
    if(!_active || TIME_SIZE<2) return;
    dsp.setTextSize(Clock_GFXfontPtr==nullptr?TIME_SIZE:1);
    dsp.setFont(Clock_GFXfontPtr);
    dsp.setTextColor(_fgcolor, _bgcolor);
  #endif
  if(!_active) return;
  dsp.setCursor(_realLeft(), _config.top);
  dsp.print(_text);
  strlcpy(_oldtext, _text, _buffsize);
  dsp.setFont();
}

/**************************
      PROGRESS WIDGET
 **************************/
void ProgressWidget::_progress() {
  char buf[_width + 1];
  snprintf(buf, _width, "%*s%.*s%*s", _pg <= _barwidth ? 0 : _pg - _barwidth, "", _pg <= _barwidth ? _pg : 5, ".....", _width - _pg, "");
  _pg++; if (_pg >= _width + _barwidth) _pg = 0;
  setText(buf);
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
    _fb = new psFrameBuffer(dsp.width(), dsp.height());
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
  if(_fb->ready()) return 0; else return _clockleft;
}
uint16_t ClockWidget::_top(){
  if(_fb->ready()) return _timeheight; else return _config.top;
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

#ifndef DSP_LCD
  Adafruit_GFX& ClockWidget::getRealDsp(){
    #ifdef PSFBUFFER
      if (_fb && _fb->ready()) return *_fb;
    #endif
    return dsp;
  }

  void ClockWidget::_printClock(bool force){
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
    bool showFullClockOnScreensaver = !config.isScreensaver || (_fb->ready() && config.store.screensaverFullDateTime);
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
    if(_fb->ready()) _fb->display();
  }

  void ClockWidget::_clearClock(){
    uint16_t bgColor = config.isScreensaver ? 0 : config.theme.background;
  #ifdef PSFBUFFER
    if(_fb->ready()) { _fb->clear(); return; }
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
  #ifdef PSFBUFFER
    if(_fb->ready()) {
      _fb->freeBuffer();
      _getTimeBounds();
      _begin();
    }
  #endif
  }

  void ClockWidget::_clear(){
    _clearClock();
  }
#else //#ifndef DSP_LCD

  void ClockWidget::_printClock(bool force){
    if (config.store.clock12) strftime(_timebuffer, sizeof(_timebuffer), "%l:%M", &network.timeinfo);
    if (!config.store.clock12) strftime(_timebuffer, sizeof(_timebuffer), "%H:%M", &network.timeinfo);
    if(force){
      dsp.setCursor(dsp.width()-5, 0);
      dsp.print(_timebuffer);
    }
    dsp.setCursor(dsp.width()-5+2, 0);
    dsp.print((network.timeinfo.tm_sec % 2 == 0)?":":" ");
  }

  void ClockWidget::_clearClock(){}

  void ClockWidget::draw(){
    if(!_active || _locked) return;
    _printClock(true);
  }
  void ClockWidget::_draw(){
    if(!_active || _locked) return;
    _printClock(true);
  }
  void ClockWidget::_reset(){}
  void ClockWidget::_clear(){}

#endif //#ifndef DSP_LCD

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
  #ifndef DSP_LCD
    width = textsize * CHARWIDTH;
    height = textsize * CHARHEIGHT;
  #else
    width = 1;
    height = 1;
  #endif
}

void BitrateWidget::_draw(){
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
#if DSP_LCD
  _plTtemsCount = PLMITEMS;
  _plCurrentPos = 1;
#elif PLAYLIST_MODE_PAGED
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
#if DSP_LCD
  _drawOneLine(currentItem);
#elif PLAYLIST_MODE_PAGED
  _drawPaged(currentItem);
#else
  _drawFade(currentItem);
#endif
}

// ==================== FADE MODE (original centered) ====================
#if !DSP_LCD && !PLAYLIST_MODE_PAGED

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

// ==================== ONE-LINE MODE (LCD) ====================
#if DSP_LCD

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

void PlayListWidget::_drawOneLine(uint16_t currentItem) {
  dsp.clear();
  _fillPlMenu(currentItem - _plCurrentPos, _plTtemsCount);
  dsp.setCursor(0,1);
  dsp.write(uint8_t(126));
}

void PlayListWidget::_printPLitem(uint8_t pos, const char* item){
  if (pos == _plCurrentPos) {
    _current->setText(item);
  } else {
    dsp.setCursor(1, pos);
    char tmp[dsp.width()] = {0};
    strlcpy(tmp, item, dsp.width());
    dsp.print(tmp);
  }
}

#endif // ONE-LINE MODE

// ==================== PAGED MODE ====================
#if PLAYLIST_MODE_PAGED && !DSP_LCD

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
