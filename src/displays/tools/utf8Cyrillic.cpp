/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Arduino.h"
#include "../../core/options.h"
#include "../dspcore.h"
#include "utf8Cyrillic.h"
#include "utf8_common.h" // transliteration and helpers moved here

// Map a Unicode Cyrillic codepoint (U+0400..U+04FF) to GLCD glyph index per
// scripts/glcdfont_Cyrillic.md. Lowercase codepoints are mapped to the
// corresponding uppercase GLCD slot so display is case-insensitive (like
// utf8Latin behaviour).
static uint8_t map_cyrillic_cp_to_glyph(uint16_t cp) {
  // Main block: А..Я -> 0x80..0x9F (GLCD extra slots per glcdfont_Cyrillic.md)
  if (cp >= 0x0410 && cp <= 0x042F) return (uint8_t)(0x80 + (cp - 0x0410));
  // Lowercase а..я -> map to the same uppercase glyph slots
  if (cp >= 0x0430 && cp <= 0x044F) return (uint8_t)(0x80 + (cp - 0x0430));

  // Additional required glyphs (uppercase and lowercase variants)
  switch (cp) {
    case 0x0404: case 0x0454: return 0xA0; // Є / є
    case 0x0490: case 0x0491: return 0xA1; // Ґ / ґ
    case 0x0407: case 0x0457: return 0xA2; // Ї / ї
    case 0x0406: case 0x0456: return 0xA3; // І / і
    case 0x0401: case 0x0451: return 0xA4; // Ё / ё
    case 0x0403: case 0x0453: return 0xA5; // Ѓ / ѓ
    case 0x0405: case 0x0455: return 0xA6; // Ѕ / ѕ
    case 0x0408: case 0x0458: return 0xA7; // Ј / ј
    case 0x0409: case 0x0459: return 0xA8; // Љ / љ
    case 0x040A: case 0x045A: return 0xA9; // Њ / њ
    case 0x040C: case 0x045C: return 0xAA; // Ќ / ќ
    case 0x040F: case 0x045F: return 0xAB; // Џ / џ
    case 0x0402: case 0x0452: return 0xAC; // Ђ / ђ
    case 0x040B: case 0x045B: return 0xAD; // Ћ / ћ
    case 0x040E: case 0x045E: return 0xAE; // Ў / ў
    case 0x04D8: case 0x04D9: return 0xAF; // Ә / ә
    case 0x0492: case 0x0493: return 0xB0; // Ғ / ғ
    case 0x049A: case 0x049B: return 0xB1; // Қ / қ
    case 0x04A2: case 0x04A3: return 0xB2; // Ң / ң
    case 0x04E8: case 0x04E9: return 0xB3; // Ө / ө
    case 0x04AE: case 0x04AF: return 0xB4; // Ү / ү
    case 0x04B0: case 0x04B1: return 0xB5; // Ұ / ұ
    case 0x04BA: case 0x04BB: return 0xB6; // Ҳ / ҳ
    case 0x04EE: case 0x04EF: return 0xB7; // Ӯ / ӯ
    default: return 0;
  }
}

char* utf8Cyrillic(const char* str, bool uppercase) {
  static char out[STATION_FIELD_LENGTH];
  
  // Stream-based conversion: process directly from input
  // Read from str[r], write to out[w] to avoid double buffer copy
  int r = 0, w = 0;
  while (str[r] && w < STATION_FIELD_LENGTH - 1) {
    uint8_t b1 = (uint8_t)str[r];

    // ASCII pass-through (uppercase if requested)
    if (b1 < 0x80) {
      out[w++] = uppercase ? toupper((char)str[r]) : (char)str[r];
      r++;
      continue;
    }

    // Handle two-byte UTF-8 sequences
    if ((b1 & 0xE0) == 0xC0 && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint16_t cp = ((b1 & 0x1F) << 6) | (b2 & 0x3F);
      uint8_t code = map_cyrillic_cp_to_glyph(cp);
      if (code && w < STATION_FIELD_LENGTH - 1) {
        // Replace two-byte sequence with single byte glyph index
        out[w++] = (char)code;
        r += 2;
        continue;
      }

      // If it's not a Cyrillic sequence and looks like a common Latin/extended
      // sequence, transliterate it to ASCII using the shared `utf8ToAscii()`
      // helper. This centralizes transliteration and avoids duplicate tables.
      if (b1 >= 0xC2 && b1 <= 0xDF) {
        char seq[3] = {(char)b1, (char)b2, 0};
        char* tr = utf8ToAscii(seq);
        for (char* p = tr; *p && w < STATION_FIELD_LENGTH - 1; ++p) out[w++] = *p;
        r += 2;
        continue;
      }
    }

    // Unhandled high byte: likely Latin-1 / Windows-1250 single-byte encoding
    // (common with older streaming servers). Re-encode as UTF-8 and look up via
    // the shared LATIN_MAP so e.g. Latin-1 0xE1 (á) → UTF-8 C3 A1 → "A".
    if (b1 >= 0xA0) {
      uint8_t f = (b1 < 0xC0) ? 0xC2 : 0xC3;
      uint8_t s = 0x80 | (b1 & 0x3F);
      char seq[3] = {(char)f, (char)s, 0};
      char* tr = utf8ToAscii(seq);
      if (tr && *tr && *tr != ' ') {
        for (char* p = tr; *p && w < STATION_FIELD_LENGTH - 1; ++p) out[w++] = *p;
      } else {
        out[w++] = ' ';
      }
      r++;
      continue;
    }

    // Unknown or unhandled sequence: copy single byte and advance
    out[w++] = (char)str[r++];
  }

  // Null-terminate compacted output
  out[w] = '\0';
  return out;
}
