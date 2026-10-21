#ifndef pretext_h
#define pretext_h

#include <stdint.h>
#include <Adafruit_GFX.h>       // GFXfont, GFXglyph, pgm_read_*
#include "../../core/options.h" // PRETEXT_ALLCAPS

// ---------------------------------------------------------------------------
// preText() - the single funnel for every codepoint the display renders.
//
// It walks a chain of ever-coarser representations and stops at the first one
// the given font actually carries:  keep -> fold -> replace.  One table serves
// fonts of different coverage, so a font with polytonic Greek shows U+1F00, a
// mono-tonic font shows U+03B1 and a Latin-only font shows 'a'.
//
// CONTRACT, relied on by the widget layout maths (utf8_strlen * charWidth is
// computed from the string as stored): one input codepoint yields EXACTLY one
// output codepoint, and never 0.
//
// PRETEXT_ALLCAPS uppercases before resolution.  The fold chain below runs
// unconditionally: it is the fallback that lets one table serve fonts of
// different coverage.
// ---------------------------------------------------------------------------
// cp is 32-bit because the UTF-8 decoder produces four-byte sequences; a
// supplementary codepoint truncated to 16 bits would silently become a
// different, wrong glyph.  The RESULT is always <= 0xFFFF: the generator
// asserts that no step leaves the BMP.
uint16_t preText(uint32_t cp, const GFXfont *font);

// Resolve a UTF-8 string in place, once, where it enters a widget.  Prefer this
// over letting the per-glyph path do the work: a scrolling label re-prints its
// window on every scroll step, so this removes the repetition and makes
// measurement and drawing use the same bytes.  Invisible combining marks and
// zero-width characters are dropped - the only place a codepoint may disappear,
// which is safe exactly because this is ingress, not a per-glyph step.
// The result is never longer than the input, so the buffer is reused.
void preTextString(char *s, const GFXfont *font);

// One step of the chain, or cp unchanged when the table has nothing.
uint16_t preTextFoldStep(uint32_t cp);

// Drop the learned-result cache.  Not needed when the active font changes
// pointer (entries are keyed on it and simply miss), but required if a font's
// data can be rewritten in place - e.g. a font loaded into RAM at runtime.
void preTextInvalidateCache();

// Convert lowercase Latin/Cyrillic to uppercase.
// Used only when PRETEXT_ALLCAPS is defined (font testing).
uint16_t allCaps(uint16_t cp);

// True when cp is inside the font's [first..last] range AND its glyph slot has
// a non-zero bitmap (the font generator emits all-zero slots for codepoints it
// could not convert, and those are not renderable).
bool glyphAvailable(uint32_t cp, const GFXfont *font);

// Count Unicode characters (not bytes) in a UTF-8 string.
uint16_t utf8_strlen(const char *s);

// Return a pointer to the byte position of the Nth character in a UTF-8 string.
// If charIndex exceeds the string length, returns a pointer to the null
// terminator.
const char* utf8_offset(const char *s, uint16_t charIndex);

#endif
