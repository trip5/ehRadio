 #ifndef psframebuffer_h
#define psframebuffer_h

#ifdef PSFBUFFER
#include <Adafruit_GFX.h>
#include "../dspfont.h"               // DSP_UNICODE_FONT definition
#include "../icons.h"                  // ICON_TABLE for icon rendering
#include "pretext.h"                   // preText() pipeline

/* PSRAM framebuffer size tracker — updated by psFrameBuffer on allocation */
extern size_t psramFrameBufferBytes;

class  psFrameBuffer : public Adafruit_GFX {
  public:
    // UTF-8 aware write() override (same as DspCore).
    using Print::write;
    size_t write(uint8_t c) {
      if (c < 0x80) { _utf8_cp = c; _utf8_remaining = 0; _writeGlyph(_utf8_cp); }
      else if (c < 0xC0) { if (_utf8_remaining > 0) { _utf8_cp = (_utf8_cp << 6) | (c & 0x3F); if (--_utf8_remaining == 0) _writeGlyph(_utf8_cp); } }
      else if (c < 0xE0) { _utf8_cp = c & 0x1F; _utf8_remaining = 1; }
      else if (c < 0xF0) { _utf8_cp = c & 0x0F; _utf8_remaining = 2; }
      else { _utf8_cp = c & 0x07; _utf8_remaining = 3; }
      return 1;
    }

    /* Reset the UTF-8 decoder state so a partial sequence from a previous
       print() call does not corrupt the first glyph of the next frame. */
    void resetUTF8() { _utf8_remaining = 0; }

    psFrameBuffer(int16_t w, int16_t h):Adafruit_GFX(w, h){ setTextWrap(false); cp437(true); }
    ~psFrameBuffer(){ freeBuffer(); }
    bool ready() { return _ready; }
    
    void freeBuffer(){
      _ready = false;
      if(buffer) {
        _dspl->fillRect(_ll, _tt, _ww, _hh, _bgcolor);
        psramFrameBufferBytes -= _hh * _ww * sizeof(uint16_t);
        free(buffer);
      }
      buffer = nullptr;
    }
    bool begin(yoDisplay *dspl, int16_t l, int16_t t, int16_t w, int16_t h, uint16_t bgcolor = 0){
      _dspl = dspl; _ll = l; _tt = t; _ww = w; _hh = h; _bgcolor = bgcolor;
      // Clamp to display bounds – prevents writePixels overflow when placed near screen edges
      if (_ll < 0) { _ww += _ll; _ll = 0; }
      if (_tt < 0) { _hh += _tt; _tt = 0; }
      if (_ll + _ww > _dspl->width())  _ww = _dspl->width()  - _ll;
      if (_tt + _hh > _dspl->height()) _hh = _dspl->height() - _tt;
      if (_ww <= 0 || _hh <= 0) { _ready = false; return false; }
      _createBuffer();
      return _ready;
    }
    void move(int16_t l, int16_t t, int16_t w, int16_t h){
      _ll = l; _tt = t; _ww = w; _hh = h;
      // Clamp to display bounds (mirrors begin() clamping)
      if (_ll < 0) { _ww += _ll; _ll = 0; }
      if (_tt < 0) { _hh += _tt; _tt = 0; }
      if (_dspl) {
        if (_ll + _ww > _dspl->width())  _ww = _dspl->width()  - _ll;
        if (_tt + _hh > _dspl->height()) _hh = _dspl->height() - _tt;
      }
      if (_ww <= 0 || _hh <= 0) return;
      freeBuffer();
      _createBuffer();
    }
    void drawPixel( int16_t x, int16_t y, uint16_t color){
      if (x < 0 || x >= _ww || y < 0 || y >= _hh) return;
      if(!buffer) return;
      uint16_t &pixel = buffer[x + y * _ww];
      if (pixel != color) pixel = color;  // skip write if already target color
    }
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
      // Write directly to PSRAM buffer — do NOT use base-class writeFillRect
      // which bypasses the framebuffer and writes to the display over SPI.
      if(!buffer) return;
      if(x < 0){ w += x; x = 0; }
      if(y < 0){ h += y; y = 0; }
      if(x + w > _ww) w = _ww - x;
      if(y + h > _hh) h = _hh - y;
      if(w <= 0 || h <= 0) return;
      for(int16_t j = 0; j < h; j++){
        uint16_t *row = &buffer[x + (y + j) * _ww];
        for(int16_t i = 0; i < w; i++) row[i] = color;
      }
    }
    void display(){
      if(!buffer) return;
      _dspl->startWrite();
      _dspl->setAddrWindow(_ll, _tt, _ww, _hh);
      _dspl->writePixels((uint16_t*)buffer,  _ww * _hh);
      _dspl->endWrite();
    }
    void clear(){
      if(!buffer) return;
      fillRect(0, 0, _ww, _hh, _bgcolor);
    }
    int16_t width(){ return _ww; }
    int16_t height(){ return _hh; }
  private:
    int16_t _ll, _tt, _ww, _hh;
    yoDisplay *_dspl;
    uint16_t *buffer=nullptr;
    bool _ready = false;
    uint16_t _bgcolor;
    void _createBuffer(){
      #if (defined(USE_FBUFFER) && USE_FBUFFER)
        if(psramInit()) {
          buffer = (uint16_t*) ps_calloc(_hh * _ww, sizeof(uint16_t));
          psramFrameBufferBytes += _hh * _ww * sizeof(uint16_t);
        } else {
          buffer = (uint16_t*) calloc(_hh * _ww, sizeof(uint16_t));
        }
      #endif
      if(buffer){
        for (int i = 0; i < _hh * _ww; i++)
          buffer[i] = _bgcolor;
        _ready = true;
      }
    }

  private:
    void _writeGlyph(uint16_t cp) {
      const GFXfont *f = &DisplayFont;
      // Icon codepoints (0x01-0x1F) — render directly from ICON_TABLE.
      // Must precede \n / \r checks so that \015 (0x0D = CR)
      // reaches the icon handler instead of being swallowed as carriage return.
      if (cp >= 0x01 && cp <= 0x1F) {
        const uint8_t* const *table = ICON_TABLE;
        if (cp < 32) {
          const uint8_t* icon = (const uint8_t*)pgm_read_ptr(&table[cp]);
          if (icon) {
            int16_t renderY = cursor_y + (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
            for (uint8_t row = 0; row < 8; row++) {
              uint8_t line = pgm_read_byte(icon + row);
              for (uint8_t col = 0; col < 6; col++) {
                if (line & (0x20 >> col)) {
                  if (textsize_x == 1 && textsize_y == 1)
                    drawPixel(cursor_x + col, renderY - 8 + row, textcolor);
                  else
                    writeFillRect(cursor_x + col * textsize_x, renderY + (int16_t)(row - 8) * textsize_y, textsize_x, textsize_y, textcolor);
                }
              }
            }
            cursor_x += 6 * textsize_x;
            return;
          }
        }
        // NULL icon (e.g. \012 / LF): fall through to normal handling below
      }
      if (cp == '\n') { cursor_x = 0; cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&f->yAdvance); return; }
      if (cp == '\r') return;
      // Space (0x20) — advance cursor by one character cell.
      if (cp == ' ') {
        uint8_t spaceAdv = pgm_read_byte(&((GFXglyph *)pgm_read_ptr(&f->glyph))->xAdvance);
        cursor_x += (int16_t)spaceAdv * textsize_x;
        return;
      }
      // Run optional pre-processing (allcaps, accent folding)
      cp = preText(cp, f);

      if (gfxFont != NULL && gfxFont != (GFXfont *)f) {
        Adafruit_GFX::write((uint8_t)(cp & 0xFF));
        return;
      }
      uint16_t first = pgm_read_word(&f->first);
      uint16_t last  = pgm_read_word(&f->last);
      if (cp >= first && cp <= last) {
        GFXglyph *glyph = (GFXglyph *)pgm_read_ptr(&f->glyph);
        glyph += (cp - first);
        uint8_t w = pgm_read_byte(&glyph->width), h = pgm_read_byte(&glyph->height);
        int16_t renderY = cursor_y + (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
        if (w > 0 && h > 0) {
          int8_t xo = (int8_t)pgm_read_byte(&glyph->xOffset);
          int8_t yo = (int8_t)pgm_read_byte(&glyph->yOffset);
          if (wrap && (cursor_x + textsize_x * (xo + w) > _width)) {
            cursor_x = 0;
            cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&f->yAdvance);
            renderY = cursor_y + (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
          }
          uint8_t *bitmap = (uint8_t *)pgm_read_ptr(&f->bitmap);
          uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
          uint8_t bits = 0, bit = 0;
          for (uint8_t yy = 0; yy < h; yy++) {
            for (uint8_t xx = 0; xx < w; xx++) {
              if (bit == 0) { bits = pgm_read_byte(&bitmap[bo++]); bit = 0x80; }
              if ((int16_t)(xo + xx) < (int16_t)pgm_read_byte(&glyph->xAdvance)) {
                if (bits & bit) {
                  if (textsize_x == 1 && textsize_y == 1)
                    drawPixel(cursor_x + xo + xx, renderY + yo + yy, textcolor);
                  else
                    writeFillRect(cursor_x + (xo + xx) * textsize_x, renderY + (yo + yy) * textsize_y, textsize_x, textsize_y, textcolor);
                }
              }
              bit >>= 1;
            }
          }
        } else {
          // Empty glyph slot — try fallback mapping
          uint16_t mapped = checkFallbackGlyph(cp, f);
          if (mapped && mapped != cp) { _writeGlyph(mapped); return; }
          // No fallback available — advance cursor by one space width so
          // the missing glyph appears as a visible gap instead of being
          // silently deleted (xAdvance is 0 for empty slots).
          uint8_t spaceAdv = pgm_read_byte(&((GFXglyph *)pgm_read_ptr(&f->glyph))->xAdvance);
          cursor_x += (int16_t)spaceAdv * textsize_x;
          return;
        }
        cursor_x += (int16_t)pgm_read_byte(&glyph->xAdvance) * textsize_x;
      } else {
        uint16_t mapped = checkFallbackGlyph(cp, f);
        if (mapped && mapped != cp) { _writeGlyph(mapped); return; }
        // Unrenderable codepoint — advance by one character cell.
        uint8_t spaceAdv = pgm_read_byte(&((GFXglyph *)pgm_read_ptr(&f->glyph))->xAdvance);
        cursor_x += (int16_t)spaceAdv * textsize_x;
      }
    }

    uint32_t _utf8_cp = 0;
    uint8_t _utf8_remaining = 0;
};
#else
struct psFrameBuffer {
  bool ready() { return false; }
  void display() {}
};
#endif //#ifdef PSFBUFFER


#endif
