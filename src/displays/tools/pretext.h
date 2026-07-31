#ifndef pretext_h
#define pretext_h

#include <stdint.h>
#include <Adafruit_GFX.h>   // GFXfont, GFXglyph, pgm_read_*
#include "../../core/options.h"  // PRETEXT_* macros

// Pre-render text processing pipeline.
// Currently only does accent folding, but can chain other preprocessors.
// Returns the processed codepoint (may be unchanged if no mapping needed),
// or 0 if the character should be silently dropped.
uint16_t preText(uint16_t cp, const GFXfont *font);
uint16_t foldAccent(uint16_t cp);
uint16_t allCaps(uint16_t cp);

// Check whether a codepoint has a renderable glyph in the font
// (i.e., it is within the font range AND the glyph slot has non-zero dimensions).
bool glyphAvailable(uint16_t cp, const GFXfont *font);

// Map Cyrillic characters that have empty font slots to available fallbacks.
// Returns the mapped codepoint, or 0 if no mapping exists.
uint16_t foldCyrillic(uint16_t cp);

// Try all fallback strategies for an unavailable glyph.
// Chains: glyphAvailable → foldCyrillic → foldAccent.
// Returns the best fallback codepoint, or 0 if nothing works.
uint16_t checkFallbackGlyph(uint16_t cp, const GFXfont *font);

// Count Unicode characters (not bytes) in a UTF-8 string.
uint16_t utf8_strlen(const char *s);

// Return a pointer to the byte position of the Nth character in a UTF-8 string.
// If charIndex exceeds the string length, returns a pointer to the null terminator.
const char* utf8_offset(const char *s, uint16_t charIndex);

#endif
