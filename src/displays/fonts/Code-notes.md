# Unicode GFXfonts on Arduino

How to get a display drawing real Unicode text with Adafruit_GFX, and how the fonts in this folder were made.
The notes are about the technique - Arduino and Adafruit calls, C structures, flash arithmetic - and deliberately
not about any one project's widget code.

## The problem with the classic font

Adafruit_GFX ships `glcdfont.c`: a fixed 256-slot, 5x7 font. It is selected at compile time and covers ASCII plus
*one* codepage of your choice, so Latin, Cyrillic and Greek cannot coexist, and anything the chosen codepage lacks
renders as garbage or blanks.

The alternative is the library's newer font format, `GFXfont`, which is just a struct in flash. One of those can
carry a few thousand glyphs, and the number it carries is down to the file you convert.

## The format

```c
// gfxfont.h - Adafruit GFX 1.1 and later
typedef struct {          // per glyph
  uint16_t bitmapOffset;  // byte offset into the font's bitmap array
  uint8_t  width;         // bitmap size in pixels
  uint8_t  height;
  uint8_t  xAdvance;      // cursor_x step after this glyph
  int8_t   xOffset;       // left edge, relative to the cursor
  int8_t   yOffset;       // top edge, relative to the baseline
} GFXglyph;

typedef struct {          // the font as a whole
  uint8_t  *bitmap;       // every glyph's rows, concatenated
  GFXglyph *glyph;        // the descriptor array, one slot per codepoint
  uint16_t  first;        // codepoint of glyph[0]
  uint16_t  last;         // codepoint of glyph[last - first]
  uint8_t   yAdvance;     // cursor_y step after a newline
} GFXfont;
```

Field widths are limits, and they are the first thing to check before planning a big font:

| Field | Type | Consequence |
| --- | --- | --- |
| `first`, `last` | `uint16_t` | A range above U+00FF works. Some older forks declared these `uint8_t`, which silently truncates any codepoint above U+00FF - check your copy. |
| `bitmapOffset` | `uint16_t` | The whole bitmap array must stay under 64 KB. A slot costs `height` bytes, so roughly 8,000 glyphs at 8x8. |
| `width`, `xAdvance` | `uint8_t` | No glyph may be wider than 255 px, and no advance may be. |
| `xOffset`, `yOffset` | `int8_t` | -128..127 px of overhang/raise, which is plenty for text and not for decoration. |
| `yAdvance` | `uint8_t` | The newline step, i.e. the font's own line height. |

Pixels are 1 bit per pixel, one row per `ceil(width / 8)` bytes, most significant bit leftmost. A 6-pixel-wide
glyph therefore costs 8 bytes of bitmap (one byte per row) - which is why the bitmap arrays here are small and the
*glyph table* is not.

`glyph[cp - first]` is the descriptor for `cp`. Any slot in the array may be empty (a zeroed descriptor, no bitmap)
- the codepoint then draws nothing, which is a legitimate way to reserve code space.

## Making one from a BDF

[`bdf2adafruit3.py`](bdf2adafruit3.py) converts a BDF (the classic bitmap font format, and the format most
old-school Unicode terminal fonts are distributed in) into a `GFXfont` header:

```bash
py bdf2adafruit3.py MatrixLight8x6.bdf 21 1279 -o MatrixLight8x6.h
```

* The two numbers are the **first and last codepoints**. They are **decimal unless written `0x`-prefixed** - so `21`
  is U+0015, not U+0021, and `0x21 0x4FF` is the same window as `33 1279`.
* The requested window is then pulled in to the slots that actually hold a glyph, and the tool reports
  `Dropped empty slots: N from beginning / M from end.` A leading empty slot is worse than wasted flash: the renderer
  reads a space's cell width from the **first** glyph, so a range that starts on an empty slot draws every space
  zero-width (the layout still reserves `CHARWIDTH` per character, so the cell is occupied but blank). Trimming to
  the first real glyph is what makes it safe to give every font the same start, U+0021 on the fonts here.
* The target cell is uniform (here 6x8). If a BDF glyph is taller or wider, it is trimmed or padded and its offsets
  adjusted, so the appearance is kept while every glyph occupies the same box. Turn this off only if you have
  layout code that can cope with variable widths - simple `characters * cellWidth` arithmetic cannot.
* Vertical placement drops the whole `ascent + descent` box onto the top of the cell, so the baseline lands
  `descent` rows above the cell floor and a glyph sits at `yoffs + descent`. That is what makes a font that declares
  a descent (the usual "7 above, 1 below") line up with one that declares none (8 above, 0 below) instead of sitting
  a row low; the descent row is the cell's bottom row, so descenders stay visible there.
* The generated file ends with a count line, `// N glyphs in range (M slots), B bytes bitmap data, T bytes glyph
  table`. Treat `T` as an estimate: the tool counts `M * 6`, while `sizeof(GFXglyph)` is **8** (7 bytes of payload
  plus one padding byte for 2-byte alignment), so the real table is `M * 8`.

Where the fonts here came from, and the exact commands used to regenerate them, are recorded in
[`fonts-notes.txt`](fonts-notes.txt).

## Drawing with it: two overrides

`Adafruit_GFX::write(uint8_t)` receives one byte and treats it as one codepoint. UTF-8 disagrees - `é` arrives as
two bytes, `α` and Cyrillic letters as two, and so on - so with the stock `write()` a multi-byte character is
looked up as several nonsense codepoints. The fix is to decode in the `Print` path:

```cpp
size_t write(uint8_t c) override {
  if (c < 0x80) {                       // ASCII, complete in one byte
    _cp = c; _remaining = 0;
    _writeGlyph(_cp);
  } else if (c < 0xC0) {                // 10xxxxxx: continuation
    if (_remaining > 0) {
      _cp = (_cp << 6) | (c & 0x3F);
      if (--_remaining == 0) _writeGlyph(_cp);
    }
  } else if (c < 0xE0) { _cp = c & 0x1F; _remaining = 1; }   // 110xxxxx: 2-byte
  else if (c < 0xF0)   { _cp = c & 0x0F; _remaining = 2; }   // 1110xxxx: 3-byte
  else                 { _cp = c & 0x07; _remaining = 3; }   // 11110xxx: 4-byte
  return 1;
}
```

Keep the codepoint in 32 bits while decoding: a 4-byte sequence does not fit in 16. Keep a `resetUTF8()` that
zeroes `_remaining`, and call it before every `print()` in anything that redraws a frame - if a frame ends in the
middle of a sequence, the leftover state corrupts the first character of the next one.

`_writeGlyph(cp)` is yours: look up `cp`, then draw the glyph's rows. The details that matter:

```cpp
const GFXfont *f = &MyFont;
if (cp < f->first || cp > f->last) { /* see "unrenderable" below */ }
GFXglyph *g = &f->glyph[cp - f->first];
uint16_t bo = g->bitmapOffset;                 // read with pgm_read_word()
uint8_t w = g->width, h = g->height, adv = g->xAdvance;   // pgm_read_byte()
for (uint8_t yy = 0; yy < h; yy++) {
  uint8_t bits = 0, bit = 0;
  for (uint8_t xx = 0; xx < w; xx++) {
    if (bit == 0) { bits = pgm_read_byte(&f->bitmap[bo++]); bit = 0x80; }
    if ((int16_t)(xOffset + xx) < (int16_t)adv) {           // clip, see below
      if (bits & bit) startWrite(), writePixel(...), endWrite();      // foreground
      else if (textbgcolor != textcolor) /* background fill */;
    }
    bit >>= 1;
  }
}
cursor_x += adv * textsize_x;
```

Five things that will bite:

1. **Glyph bleed.** Nothing stops a designer's glyph from having `xOffset + width > xAdvance`. Drawing the overhang
   smears into the next cell, which is very visible when glyphs are drawn with a background colour. Clip the
   *rendering* to `xAdvance` (the line above) - but keep consuming bitmap bits for every column, or the next glyph
   starts mid-byte and everything after it is garbage.
2. **Space may have no glyph.** If the font's range starts at `0x21`, `' '` is not in it. Handle space before any
   lookup and advance by a cell (`f->glyph[0].xAdvance`), or words run together.
3. **Unrenderable codepoints must still advance.** If the codepoint is outside the font, or its slot is empty, step
   `cursor_x` by one cell anyway. Otherwise the cursor stagnates and every later character piles up on it.
4. **Read flash with `pgm_read_*`.** Fonts live in PROGMEM: `pgm_read_byte`/`pgm_read_word` for fields,
   `pgm_read_ptr` when the struct itself is addressed through a pointer. A plain dereference may work on ESP32 and
   fail on AVR - write it correctly once.
5. **`startWrite()`/`endWrite()`** around each glyph's pixel work on SPI displays (they begin/end the bus
   transaction); they are no-ops on I2C. Call `writePixel`/`writeFillRect`, not `drawPixel`, if your own `write()`
   override is what the library's text path reaches - otherwise you can re-enter your decoder.

### If you render into a buffer

An off-screen framebuffer (for flicker-free or partial updates) draws with its own primitives, so it cannot share
the renderer above: the glyph loop has to exist a second time, differing only in the pixel call. Keep the two in
step - a fix applied to one belongs in both. In this firmware they are the two `_writeGlyph()` implementations in
`../tools/commongfx.h` and `../tools/psframebuffer.h`.

## Sizing, which is mostly about the table

Every slot in the range costs 8 bytes, populated or not, and a glyph's bitmap costs about its height in bytes:

| Font | Slots | Bitmaps | Glyph table | Total |
| --- | --- | --- | --- | --- |
| MatrixLight 8x6 / MatrixChunky 8x6 | 1247 (U+0021-U+04FF) | 2,760 B | 9,976 B | ~12.8 KB |
| Unix X11 6x9 | 1248 (U+0020-U+04FF) | 3,696 B | 9,984 B | ~13.7 KB |

The table is three to four times the bitmaps, so the range - not the artwork - is what you are paying for. Two
consequences: keep the range tight around what you actually render, and cover a script with two smaller fonts
(e.g. Latin+punct in one, Cyrillic+Greek in another) rather than one enormous one, if the sizes justify the swap.
If your layout code multiplies character count by a fixed cell width, all your fonts must share that cell.

A font draws only what it carries. If the text you must display can contain codepoints the font lacks, decide the
policy explicitly - a visible substitute glyph such as `_`, or a fallback step to a coarser equivalent (`α` to `a`)
- and make sure it produces exactly one glyph per input character, because any layout arithmetic counted the
characters before you drew them.

## Icons as control codepoints

A neat trick that costs nothing: reserve codepoints `0x01`-`0x1F`, which no text font needs, and have the renderer
draw a bitmap of your own for each of them instead of asking the font. Icons then live inside ordinary strings, in
the same cell grid as the text.

Two details: the icon check must come **before** any newline/carriage-return test, because `0x0D` is not just an
icon slot, it is carriage return - check the range first or `\015` disappears into the newline handler. And a
multi-cell symbol (a 12x8 motif, say) is simply two codepoints drawn next to each other; the renderer advances one
cell per codepoint, so the icon's own width has to fit that convention.
