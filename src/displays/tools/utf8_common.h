/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#ifndef UTF8_COMMON_H
#define UTF8_COMMON_H

#include "Arduino.h"
#include "../../core/options.h"
#include <pgmspace.h>
#include <ctype.h>

// LATIN_PRESERVE and CYRILLIC_PRESERVE are defined in utf8_common.cpp to keep
// these tables as a single copy in flash rather than duplicated per TU.
extern const uint16_t LATIN_PRESERVE[] PROGMEM;
extern const uint16_t LATIN_PRESERVE_SIZE;

extern const uint16_t CYRILLIC_PRESERVE[] PROGMEM;
extern const uint16_t CYRILLIC_PRESERVE_SIZE;

// Determine whether a two-byte UTF-8 sequence should be preserved (mapped to a
// glyph available in the project's font) according to the selected codepage.
static inline bool shouldPreserveChar(uint8_t first, uint8_t second) {
  if (first < 0xC2 || first > 0xDF) return false;
  uint16_t cp = ((first & 0x1F) << 6) | (second & 0x3F);

#ifdef L10N_CP_LATIN
  for (uint16_t i = 0; i < LATIN_PRESERVE_SIZE; i++)
    if (pgm_read_word(&LATIN_PRESERVE[i]) == cp) return true;
  return false;

#elif defined(L10N_CP_CYRILLIC)
  // Preserve the main Cyrillic block U+0410..U+044F (А..я) which maps to
  // dedicated GLCD glyph slots; additional non-contiguous Cyrillic letters
  // that the font provides are listed in CYRILLIC_PRESERVE and checked below.
  // Do NOT preserve Latin accented letters when using the Cyrillic codepage;
  // instead transliterate them to ASCII so they behave consistently with
  // Cyrillic displays (e.g. 'á' -> 'A').
  if (cp >= 0x0410 && cp <= 0x044F) return true;
  for (uint16_t i = 0; i < CYRILLIC_PRESERVE_SIZE; i++)
    if (pgm_read_word(&CYRILLIC_PRESERVE[i]) == cp) return true;
  return false;

#else
  (void)first; (void)second; (void)cp;
  return false;
#endif
}

// Determine whether the given UTF-8 string can be rendered natively using the
// project's GLCD font for the active codepage. Allows ASCII, preserved two-
// byte sequences (per shouldPreserveChar) and a couple of common 3-byte
// punctuation sequences (ellipsis, trademark).
static inline bool canRenderNative(const char *info) {
  if (!info || info[0] == '\0') return false;
  size_t len = strnlen(info, 512);
  if (len == 0) return false;
  for (size_t i = 0; i < len; i++) {
    uint8_t b1 = (uint8_t)info[i];
    // ASCII printable
    if (b1 >= 0x20 && b1 <= 0x7E) continue;

    // Two-byte UTF-8 sequences: preserve if allowed for current codepage
    if (i + 1 < len && b1 >= 0xC2 && b1 <= 0xDF) {
      uint8_t b2 = (uint8_t)info[i+1];
      if (shouldPreserveChar(b1, b2)) { i++; continue; }
      return false; // non-preserved 2-byte sequence - not renderable natively
    }

    // Allow a couple of common 3-byte punctuation sequences (handled by utf8ToAscii)
    if (i + 2 < len && i + 1 < len) {
      uint8_t b2 = (uint8_t)info[i+1];
      uint8_t b3 = (uint8_t)info[i+2];
      // ellipsis … (E2 80 A6) and trademark ™ (E2 84 A2)
      if (b1 == 0xE2 && ((b2 == 0x80 && b3 == 0xA6) || (b2 == 0x84 && b3 == 0xA2))) { i += 2; continue; }
    }

    // Other sequences cannot be rendered natively
    return false;
  }
  return true;
}

// Maps UTF-8 sequences (Latin blocks C2/C3/C4/C5) to ASCII equivalents.
// Table defined in utf8_common.cpp.

struct CharMapping {
  uint8_t first;
  uint8_t second;
  char output[4]; // Max 3 chars + null terminator (e.g., "(c)")
};

extern const CharMapping LATIN_MAP[] PROGMEM;
extern const uint16_t LATIN_MAP_SIZE;

// Cyrillic transliteration table (used for both Cyrillic and Latin codepage builds).
// Table defined in utf8_common.cpp.
struct CyrillicMapping { uint16_t code; char output; };
extern const CyrillicMapping CYRILLIC_MAP[] PROGMEM;
extern const uint16_t CYRILLIC_MAP_SIZE;
static inline char transliterateCyrillic(uint8_t first, uint8_t second);

// =============================================================================
// MAIN CONVERSION FUNCTION (shared)
// =============================================================================
static inline char* utf8ToAscii(const char* src) {
    static char buf[STATION_FIELD_LENGTH];
    int outIdx = 0;
    const char* p = src;
    while (*p && outIdx < STATION_FIELD_LENGTH - 1) {
      if ((uint8_t)*p < 0x80) { buf[outIdx++] = *p++; continue; }
      uint8_t first = (uint8_t)*p; uint8_t second = (uint8_t)*(p + 1);
      if (first >= 0xE0 && first <= 0xEF) {
        uint8_t third = (uint8_t)*(p + 2); bool processed = false;
        if (first == 0xE2 && second == 0x80 && third == 0xA6) { if (outIdx + 3 <= STATION_FIELD_LENGTH - 1) { buf[outIdx++] = '.'; buf[outIdx++] = '.'; buf[outIdx++] = '.'; } processed = true; }
        else if (first == 0xE2 && second == 0x84 && third == 0xA2) { if (outIdx + 2 <= STATION_FIELD_LENGTH - 1) { buf[outIdx++] = 'T'; buf[outIdx++] = 'M'; } processed = true; }
        if (processed) { p += 3; } else { buf[outIdx++] = ' '; p += 3; } continue;
      }
      if (first >= 0xC2 && first <= 0xDF) {
        bool processed = false;
        if (shouldPreserveChar(first, second)) {
        if (outIdx + 2 <= STATION_FIELD_LENGTH - 1) {
#ifdef L10N_CP_CYRILLIC
          // Convert preserved lowercase Cyrillic (U+0430..U+044F) to uppercase
          // (U+0410..U+042F) so preserved glyphs render as uppercase on GLCD
          uint16_t cp = ((first & 0x1F) << 6) | (second & 0x3F);
          if (cp >= 0x0430 && cp <= 0x044F) {
            cp -= 0x20; // uppercase codepoint
            uint8_t u1 = 0xC0 | (cp >> 6);
            uint8_t u2 = 0x80 | (cp & 0x3F);
            buf[outIdx++] = (char)u1;
            buf[outIdx++] = (char)u2;
          } else {
            buf[outIdx++] = first;
            buf[outIdx++] = second;
          }
#else
          buf[outIdx++] = first;
          buf[outIdx++] = second;
#endif
        }
        p += 2; continue; }
        #if defined(L10N_CP_CYRILLIC) || defined(L10N_CP_LATIN)
          if (!processed && (first == 0xD0 || first == 0xD1)) {
            char tr = transliterateCyrillic(first, second);
            if (tr && outIdx < STATION_FIELD_LENGTH - 1) { buf[outIdx++] = (char)toupper((unsigned char)tr); processed = true; }
          }
        #endif
        if (!processed) {
          for (uint16_t i = 0; i < LATIN_MAP_SIZE; i++) {
            if (pgm_read_byte(&LATIN_MAP[i].first) == first && pgm_read_byte(&LATIN_MAP[i].second) == second) {
              for (uint8_t j = 0; j < 4 && outIdx < STATION_FIELD_LENGTH - 1; j++) {
                char ch = pgm_read_byte(&LATIN_MAP[i].output[j]); if (ch == 0) break; buf[outIdx++] = (char)toupper((unsigned char)ch); }
              processed = true; break;
            }
          }
        }
        if (processed) { p += 2; } else { buf[outIdx++] = ' '; p += 2; } continue;
      }
      int bytesToSkip = 0; if (first >= 0xC0 && first <= 0xDF) bytesToSkip = 2; else if (first >= 0xE0 && first <= 0xEF) bytesToSkip = 3; else if (first >= 0xF0 && first <= 0xF7) bytesToSkip = 4; else bytesToSkip = 1;
      buf[outIdx++] = ' '; p += bytesToSkip;
    }
    buf[outIdx] = 0; return buf;
}

// Public helper: transliterate a UTF-8 string to ASCII equivalents suitable for
// glcdfont-based display rendering. Returns pointer to a static buffer.
static inline char* u8transliterate(const char* src) {
  return utf8ToAscii(src);
}

// Fast Cyrillic transliteration helper: returns ASCII letter for D0/D1 two-byte
// sequences or 0 when not a Cyrillic transliteration target. Implemented using
// the shared `CYRILLIC_MAP` table above for single-source-of-truth behavior.
static inline char transliterateCyrillic(uint8_t first, uint8_t second) {
#if defined(L10N_CP_CYRILLIC) || defined(L10N_CP_LATIN)
  if (!(first == 0xD0 || first == 0xD1)) return 0;
  uint16_t code = ((first & 0x1F) << 6) | (second & 0x3F);
  for (uint8_t i = 0; i < CYRILLIC_MAP_SIZE; i++) {
    if (pgm_read_word(&CYRILLIC_MAP[i].code) == code) {
      return (char)pgm_read_byte(&CYRILLIC_MAP[i].output);
    }
  }
#endif
  return 0;
}

#endif // UTF8_COMMON_H
