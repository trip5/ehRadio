 #ifndef psframebuffer_h
#define psframebuffer_h

#ifdef PSFBUFFER
#include <Adafruit_GFX.h>
#include "../dspfont.h"               // DSP_UNICODE_FONT definition
#include "../icons.h"                  // ICON_TABLE for icon rendering
#include "pretext.h"                   // preText() pipeline
#include "dspstats.h"                  // Core Monitor counters

// PSRAM framebuffer size tracker — updated by psFrameBuffer on allocation
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

    // Reset the UTF-8 decoder state so a partial sequence from a previous print() call does not corrupt the first
    // glyph of the next frame.
    void resetUTF8() { _utf8_remaining = 0; }

    psFrameBuffer(int16_t w, int16_t h):Adafruit_GFX(w, h){ setTextWrap(false); cp437(true); }
    ~psFrameBuffer(){ freeBuffer(); }
    bool ready() { return _ready; }
    // True once display() has put this window on the panel.  An erase uses it to skip a window the panel never
    // received - the same guard freeBuffer() applies before it fills the rect - so a widget that was begun but
    // never shown cannot paint its background over whatever shares its band.
    bool painted() { return _painted; }
    
    void freeBuffer(){
      _ready = false;
      if(buffer) {
        // Only erase the panel if this window ever reached it.  A buffer that was begun and then
        // dropped before its first display() holds nothing, and filling its rect would wipe pixels
        // this window never owned - which is what a shrinking ScrollWidget window would do to
        // whatever shares the row (the codec badge, in the reported case).
        if (_painted) _dspl->fillRect(_ll, _tt, _ww, _hh, _bgcolor);
        if (_psram) psramFrameBufferBytes -= _hh * _ww * sizeof(uint16_t);
        free(buffer);
      }
      _painted = false;
      _psram = false;
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
    // Into the PSRAM buffer, clipped to the window - nothing here reaches the panel; display() is what pushes it.
    // Overriding fillRect is enough: the base writeFillRect() calls this (virtual), so both spellings land in the buffer.
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
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
      _painted = true;                 // from here the panel shows this window, so it owns those pixels
      _dspl->startWrite();
      cmCountPush();
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
    yoDisplay *_dspl = nullptr;
    uint16_t *buffer=nullptr;
    bool _ready = false;
    bool _painted = false;           // true once display() has put this window on the panel
    bool _psram = false;
    uint16_t _bgcolor;
    void _createBuffer(){
      if(psramInit()) {
        buffer = (uint16_t*) ps_calloc(_hh * _ww, sizeof(uint16_t));
        _psram = true;
        psramFrameBufferBytes += _hh * _ww * sizeof(uint16_t);
      } else {
        buffer = (uint16_t*) calloc(_hh * _ww, sizeof(uint16_t));
        _psram = false;
      }
      if(buffer){
        for (int i = 0; i < _hh * _ww; i++)
          buffer[i] = _bgcolor;
        _painted = false;              // a fresh buffer holds only the background; the panel owes it nothing
        _ready = true;
      }
    }

  private:
    // cp is 32-bit: the decoder can produce four-byte sequences and truncating
    // them here would turn a supplementary codepoint into a different glyph.
    void _writeGlyph(uint32_t cp) {
      cmCountGlyph();
      const GFXfont *f = displayFont();
      // Icon codepoints (0x01-0x1F) — render directly from ICON_TABLE.
      // Must precede \n / \r checks so that \015 (0x0D = CR)
      // reaches the icon handler instead of being swallowed as carriage return.
      if (cp >= 0x01 && cp <= 0x1F) {
        if (cp < 32) {
          // Same cell rule as commongfx: iconAt() only returns a divisor of textsize.
          // Clear bits are never painted; the buffer already holds the background.
          const IconSel sel = (textsize_x == textsize_y) ? iconAt((uint8_t)cp, textsize_x)
                                                        : IconSel{ iconBits((uint8_t)cp, 1), 1 };
          if (sel.bits) {
            const uint8_t k = sel.scale;
            const uint8_t psx = textsize_x / k, psy = textsize_y / k;
            const uint8_t w = 6 * k, h = 8 * k;
            const uint8_t *bits = (const uint8_t *)sel.bits;
            int16_t renderY = cursor_y + (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
            for (uint8_t row = 0; row < h; row++) {
              const uint32_t line = (k == 1) ? pgm_read_byte(bits + row)
                                             : pgm_read_dword(bits + row * 4);
              for (uint8_t col = 0; col < w; col++) {
                if (line & (1UL << (w - 1 - col))) {
                  const int16_t x = cursor_x + (int16_t)col * psx;
                  const int16_t y = renderY + (int16_t)((int16_t)row - h) * psy;
                  fillRect(x, y, psx, psy, textcolor);   // this class's fillRect: into the buffer, clipped
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
      // Clock font dispatch happens BEFORE preText(), so the resolver is never
      // asked about a font other than the one that will draw the glyph - the
      // clock font has its own glyph set and the stock write() is single-byte.
      if (gfxFont != NULL && gfxFont != (GFXfont *)f) {
        Adafruit_GFX::write((uint8_t)(cp & 0xFF));
        return;
      }
      // Resolve against the font actually used: keep the glyph when the font has
      // it, fold to a base character when it does not, substitute otherwise.
      // Returns a renderable codepoint, never 0.
      // preText() narrows to a BMP codepoint, so the lookups below are 16-bit
      // again - anything the font carries is inside its own (BMP) range.
      uint16_t resolved = preText(cp, f);
      cp = resolved;
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
          const int16_t adv = (int16_t)pgm_read_byte(&glyph->xAdvance);
          for (uint8_t yy = 0; yy < h; yy++) {
            for (uint8_t xx = 0; xx < w; xx++) {
              if (bit == 0) { bits = pgm_read_byte(&bitmap[bo++]); bit = 0x80; }
              if ((int16_t)(xo + xx) < adv) {
                if (bits & bit) {
                  // Per DISPLAY pixel, never a textsize-wide block - the same rule the textsize-1 path has always
                  // used.  A glyph entering a scrolling line always straddles the window edge, and fillRect handles a
                  // straddling block by SHAVING its width, which is a different clip from the per-pixel test and looks
                  // like a glyph that arrives whole at some text sizes.  drawPixel clips per pixel at every size.
                  const int16_t gx = cursor_x + (int16_t)(xo + xx) * textsize_x;
                  const int16_t gy = renderY + (int16_t)(yo + yy) * textsize_y;
                  for (uint8_t py = 0; py < (uint8_t)textsize_y; py++) {
                    for (uint8_t px = 0; px < (uint8_t)textsize_x; px++) {
                      drawPixel(gx + px, gy + py, textcolor);
                    }
                  }
                }
              }
              bit >>= 1;
            }
          }
        } else {
          // preText() promises a renderable codepoint, so an empty slot here can
          // only mean the font changed underneath us (clock font swap mid-frame).
          // Advance one space width rather than drawing a zero-width glyph.
          uint8_t spaceAdv = pgm_read_byte(&((GFXglyph *)pgm_read_ptr(&f->glyph))->xAdvance);
          cursor_x += (int16_t)spaceAdv * textsize_x;
          return;
        }
        cursor_x += (int16_t)pgm_read_byte(&glyph->xAdvance) * textsize_x;
      } else {
        // Same defensive case as above: outside [first..last] after resolution
        // means the font changed, not that preText() failed to find a glyph.
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
  // No buffer, so there is nothing to count and nowhere to push: the WIDGET_DEBUG probe reads these and reports -1.
  int16_t columnInk(int16_t) { return -1; }
  int16_t left()   { return -1; }
  int16_t top()    { return -1; }
  int16_t width()  { return -1; }
  int16_t height() { return -1; }
};
#endif //#ifdef PSFBUFFER


#endif
