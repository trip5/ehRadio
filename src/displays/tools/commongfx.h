#ifndef common_gfx_h
#define common_gfx_h
#include <Arduino.h>
#include "../widgets/widgetsconfig.h" // displayXXXDDDDconf.h
#include "../icons.h"                 // icon bitmaps + ICON_TABLE
#include "../dspfont.h"               // DSP_UNICODE_FONT definition
#include "pretext.h"
#include "dspstats.h"                 // Core Monitor counters

// Define missing macros for SSD1306x32 if not already defined
#ifndef CHARWIDTH
  #define CHARWIDTH 6
#endif
#if !defined(TFT_FG) && !defined(TFT_BG)
  #define TFT_FG 1
  #define TFT_BG 0
#endif

// In-band pixel-spacer control character used by display and widget rendering
// (ASCII Record Separator 0x1E). Inserting this byte into a display string
// adds a 2-pixel gap without advancing a full character cell.
#define DSP_PIXEL_SPACER '\x1E'

typedef struct clipArea {
  uint16_t left; 
  uint16_t top; 
  uint16_t width;  
  uint16_t height;
} clipArea;

class psFrameBuffer;

class DspCore: public yoDisplay {
  public:
    DspCore();
    void initDisplay();
    void clearDsp(bool black=false);
    void printClock(){}
    #ifdef DSP_OLED
      inline void loop(bool force=false){
        cmCountPush();                // one panel push per display() call
        display();
      }
      inline void drawLogo(uint16_t top) {
        #if !(DSP_MODEL==DSP_SSD1306 && DSP_HEIGHT==32)
          drawBitmap((width()  - LOGO_WIDTH ) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT, 1);
        #else
          // Plain literal: transliteration now happens at render time in
          // preText(), so the old utf8To() pre-pass is gone.
          setTextSize(1); setCursor((width() - 6*CHARWIDTH) / 2, 0); setTextColor(TFT_FG, TFT_BG); print("ehRadio");
        #endif
        display();
      }
    #else
      inline void loop(bool force=false){}
      inline void drawLogo(uint16_t top){ drawRGBBitmap((width() - LOGO_WIDTH) / 2, top, logo, LOGO_WIDTH, LOGO_HEIGHT); }
    #endif
    void flip();
    void invert();
    void sleep();
    void wake();
    void setScrollId(void * scrollid) { _scrollid = scrollid; }
    void * getScrollId() { return _scrollid; }
    uint16_t textWidth(const char *txt);
    inline void writePixel(int16_t x, int16_t y, uint16_t color) {
      if(_clipping){
        if ((x < _cliparea.left) || (x > _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height)) return;
      }
      yoDisplay::writePixel(x, y, color);
    }
    inline void writeFillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
      if(_clipping){
        if ((x < _cliparea.left) || (x >= _cliparea.left+_cliparea.width) || (y < _cliparea.top) || (y > _cliparea.top + _cliparea.height))  return;
      }
      cmCountFill();                  // counted after the clip test: only real fills
      yoDisplay::writeFillRect(x, y, w, h, color);
    }
    inline void setClipping(clipArea ca){
      _cliparea = ca;
      _clipping = true;
    }
    inline void clearClipping(){
      _clipping = false;
    }

    // Draw a 5-pixel-wide icon from icons.h at the current cursor position.
    void drawIcon(const uint8_t* icon, uint16_t color, uint16_t bg) {
      for (int8_t i = 0; i < 5; i++) {
        uint8_t line = pgm_read_byte(icon + i);
        for (int8_t j = 0; j < 8; j++) {
          if (line & (0x80 >> j))
            yoDisplay::writePixel(cursor_x + i, cursor_y + j, color);
          else if (bg != color && bg != (uint16_t)-1)
            yoDisplay::writePixel(cursor_x + i, cursor_y + j, bg);
        }
      }
      cursor_x += 6;
    }

    // UTF-8 aware write() — replaces the stock library's uint8_t-limited version.
    using Print::write;
    size_t write(uint8_t c) {
      if (c < 0x80) {
        _utf8_cp = c;
        _utf8_remaining = 0;
        _writeGlyph(_utf8_cp);
      } else if (c < 0xC0) {
        if (_utf8_remaining > 0) {
          _utf8_cp = (_utf8_cp << 6) | (c & 0x3F);
          if (--_utf8_remaining == 0) _writeGlyph(_utf8_cp);
        }
      } else if (c < 0xE0) {
        _utf8_cp = c & 0x1F;
        _utf8_remaining = 1;
      } else if (c < 0xF0) {
        _utf8_cp = c & 0x0F;
        _utf8_remaining = 2;
      } else {
        _utf8_cp = c & 0x07;
        _utf8_remaining = 3;
      }
      return 1;
    }

    // Reset the UTF-8 decoder state so a partial sequence from a previous print() call does not corrupt the first
    // glyph of the next frame.
    void resetUTF8() { _utf8_remaining = 0; }

  private:
    // Render a 16-bit codepoint using DisplayFont.  All characters
    // (ASCII + non-ASCII) are rendered directly with background fill —
    // unlike the library's GFXfont drawChar which draws only foreground.
    // cp is 32-bit: the decoder can produce four-byte sequences and truncating
    // them here would turn a supplementary codepoint into a different glyph.
    void _writeGlyph(uint32_t cp) {
      cmCountGlyph();
      const GFXfont *f = displayFont();
      // Icon codepoints (0x01-0x1F) — render directly from ICON_TABLE.
      // Must precede \n / \r checks so that \015 (0x0D = CR)
      // reaches the icon handler instead of being swallowed as carriage return.
      if (cp >= 0x01 && cp <= 0x1F) {
        const uint8_t* const *table = ICON_TABLE;
        if (cp < 32) {
          const uint8_t* icon = (const uint8_t*)pgm_read_ptr(&table[cp]);
          if (icon) {
            startWrite();
            int16_t renderY = cursor_y + (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
            for (uint8_t row = 0; row < 8; row++) {
              uint8_t line = pgm_read_byte(icon + row);
              for (uint8_t col = 0; col < 6; col++) {
                if (line & (0x20 >> col)) {
                  if (textsize_x == 1 && textsize_y == 1)
                    writePixel(cursor_x + col, renderY - 8 + row, textcolor);
                  else
                    writeFillRect(cursor_x + col * textsize_x, renderY + (int16_t)(row - 8) * textsize_y, textsize_x, textsize_y, textcolor);
                } else if (textbgcolor != textcolor) {
                  if (textsize_x == 1 && textsize_y == 1)
                    writePixel(cursor_x + col, renderY - 8 + row, textbgcolor);
                  else
                    writeFillRect(cursor_x + col * textsize_x, renderY + (int16_t)(row - 8) * textsize_y, textsize_x, textsize_y, textbgcolor);
                }
              }
            }
            endWrite();
            cursor_x += 6 * textsize_x;
            return;
          }
        }
        // NULL icon (e.g. \012 / LF): fall through to normal handling below
      }
      if (cp == '\n') { cursor_x = 0; cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&f->yAdvance); return; }
      if (cp == '\r') return;
      // Space (0x20) — advance cursor by one character cell.  Some fonts
      // start at 0x21, so the space has no glyph of its own; resolving it
      // through preText() would replace it with the substitute rather than a
      // gap, and either way the cursor must advance or characters run together.
      if (cp == ' ') {
        uint8_t spaceAdv = pgm_read_byte(&((GFXglyph *)pgm_read_ptr(&f->glyph))->xAdvance);
        cursor_x += (int16_t)spaceAdv * textsize_x;
        return;
      }
      // If a clock font is active (not ours, not NULL), let the library handle it.
      // Dispatched BEFORE preText() so the resolver is never asked about a font
      // other than the one that will draw the glyph - otherwise a codepoint the
      // clock font carries could be folded or replaced against DisplayFont.
      // Clock text is ASCII digits, and the stock write() is single-byte, which
      // is why this path stays 1-byte.
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
        int16_t renderY = cursor_y;
        if (gfxFont == NULL) renderY += (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
        if (w > 0 && h > 0) {
          startWrite();
          int8_t xo = (int8_t)pgm_read_byte(&glyph->xOffset);
          int8_t yo = (int8_t)pgm_read_byte(&glyph->yOffset);
          if (wrap && (cursor_x + textsize_x * (xo + w) > _width)) {
            cursor_x = 0;
            cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&f->yAdvance);
            renderY = cursor_y;
            if (gfxFont == NULL) renderY += (int16_t)pgm_read_byte(&f->yAdvance) * textsize_y;
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
                    writePixel(cursor_x + xo + xx, renderY + yo + yy, textcolor);
                  else
                    writeFillRect(cursor_x + (xo + xx) * textsize_x, renderY + (yo + yy) * textsize_y, textsize_x, textsize_y, textcolor);
                } else if (textbgcolor != textcolor) {
                  if (textsize_x == 1 && textsize_y == 1)
                    writePixel(cursor_x + xo + xx, renderY + yo + yy, textbgcolor);
                  else
                    writeFillRect(cursor_x + (xo + xx) * textsize_x, renderY + (yo + yy) * textsize_y, textsize_x, textsize_y, textbgcolor);
                }
              }
              bit >>= 1;
            }
          }
          endWrite();
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

    // UTF-8 decoder state
    uint32_t _utf8_cp = 0;
    uint8_t _utf8_remaining = 0;

    bool _clipping;
    clipArea _cliparea;
    void * _scrollid;
    #ifdef PSFBUFFER
      psFrameBuffer* _fb=nullptr;
    #endif
};

extern DspCore dsp;
#endif
