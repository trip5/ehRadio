/*
 * Copyright (c) 2026 Aivaras Kasperaitis (@kasperaitis)
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "Arduino.h"
#include "../../core/options.h"
#include "../dspcore.h"
#include "utf8Latin.h"

// utf8Latin: a small consolidated mapper for common Latin-1, Latin Extended and
// basic Greek characters used by the GLCD font. Both lowercase and uppercase
// UTF-8 sequences are mapped to the GLCD glyph indices (the indices listed in
// scripts/glcdfont_required_table.md). Lowercase inputs are mapped to the
// uppercase GLCD glyph index so that `uppercase` display behavior is consistent.

char* utf8Latin(const char* str, bool uppercase) {
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

    // Latin-1 supplement (C3 xx) - common Western accented letters
    if (b1 == 0xC3 && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint8_t code = 0;
      switch (b2) {
        case 0x80: case 0xA0: code = 0x80; break; // À / à -> 0x80
        case 0x81: case 0xA1: code = 0x81; break; // Á / á -> 0x81
        case 0x82: case 0xA2: code = 0x82; break; // Â / â -> 0x82
        case 0x83: case 0xA3: code = 0x83; break; // Ã / ã -> 0x83
        case 0x84: case 0xA4: code = 0x84; break; // Ä / ä -> 0x84
        case 0x85: case 0xA5: code = 0x85; break; // Å / å -> 0x85
        case 0x86: case 0xA6: code = 0x86; break; // Æ / æ -> 0x86
        case 0x87: case 0xA7: code = 0x87; break; // Ç / ç -> 0x87
        case 0x88: case 0xA8: code = 0x88; break; // È / è -> 0x88
        case 0x89: case 0xA9: code = 0x89; break; // É / é -> 0x89
        case 0x8D: case 0xAD: code = 0x8A; break; // Í / í -> 0x8A
        case 0x8E: case 0xAE: code = 0x8B; break; // Î / î -> 0x8B
        case 0x90: case 0xB0: code = 0x8C; break; // Ð / ð -> 0x8C
        case 0x91: case 0xB1: code = 0x8D; break; // Ñ / ñ -> 0x8D
        case 0x93: case 0xB3: code = 0x8E; break; // Ó / ó -> 0x8E
        case 0x94: case 0xB4: code = 0x8F; break; // Ô / ô -> 0x8F
        case 0x95: case 0xB5: code = 0x90; break; // Õ / õ -> 0x90
        case 0x96: case 0xB6: code = 0x91; break; // Ö / ö -> 0x91
        case 0x98: case 0xB8: code = 0x92; break; // Ø / ø -> 0x92
        case 0x9A: case 0xBA: code = 0x93; break; // Ú / ú -> 0x93
        case 0x9C: case 0xBC: code = 0x94; break; // Ü / ü -> 0x94
        case 0x9D: case 0xBD: code = 0x95; break; // Ý / ý -> 0x95
        case 0x9E: case 0xBE: code = 0x96; break; // Þ / þ -> 0x96
        default: code = 0; break;
      }
      if (code && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = (char)code;
        r += 2;
        continue;
      }
    }

    // Latin Extended-A (C4 xx)
    if (b1 == 0xC4 && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint8_t code = 0;
      switch (b2) {
        case 0x80: case 0x81: code = 0x97; break; // Ā / ā -> 0x97
        case 0x82: case 0x83: code = 0x98; break; // Ă / ă -> 0x98
        case 0x84: case 0x85: code = 0x99; break; // Ą / ą -> 0x99
        case 0x86: case 0x87: code = 0x9A; break; // Ć / ć -> 0x9A
        case 0x8C: case 0x8D: code = 0x9B; break; // Č / č -> 0x9B
        case 0x8E: case 0x8F: code = 0x9C; break; // Ď / ď -> 0x9C
        case 0x90: case 0x91: code = 0x9D; break; // Đ / đ -> 0x9D
        case 0x92: case 0x93: code = 0x9E; break; // Ē / ē -> 0x9E
        case 0x96: case 0x97: code = 0x9F; break; // Ė / ė -> 0x9F
        case 0x98: case 0x99: code = 0xA0; break; // Ę / ę -> 0xA0
        case 0x9A: case 0x9B: code = 0xA1; break; // Ě / ě -> 0xA1
        case 0xA2: case 0xA3: code = 0xA2; break; // Ģ / ģ -> 0xA2
        case 0xAA: case 0xAB: code = 0xA3; break; // Ī / ī -> 0xA3
        case 0xAE: case 0xAF: code = 0xA4; break; // Į / į -> 0xA4
        case 0xB6: case 0xB7: code = 0xA5; break; // Ķ / ķ -> 0xA5
        case 0xB9: case 0xBA: code = 0xA6; break; // Ĺ / ĺ -> 0xA6
        case 0xBB: case 0xBC: code = 0xA7; break; // Ļ / ļ -> 0xA7
        case 0xBD: case 0xBE: code = 0xA8; break; // Ľ / ľ -> 0xA8
        default: code = 0; break;
      }
      if (code && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = (char)code;
        r += 2;
        continue;
      }
    }

    // C5 block - Central European letters
    if (b1 == 0xC5 && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint8_t code = 0;
      switch (b2) {
        case 0x81: case 0x82: code = 0xA9; break; // Ł / ł -> 0xA9
        case 0x83: case 0x84: code = 0xAA; break; // Ń / ń -> 0xAA
        case 0x85: case 0x86: code = 0xAB; break; // Ņ / ņ -> 0xAB
        case 0x87: case 0x88: code = 0xAC; break; // Ň / ň -> 0xAC
        case 0x8C: case 0x8D: code = 0xAD; break; // Ō / ō -> 0xAD
        case 0x90: case 0x91: code = 0xAE; break; // Ő / ő -> 0xAE
        case 0x92: case 0x93: code = 0xAF; break; // Œ / œ -> 0xAF
        case 0x94: case 0x95: code = 0xB0; break; // Ŕ / ŕ -> 0xB0
        case 0x98: case 0x99: code = 0xB1; break; // Ř / ř -> 0xB1
        case 0x9A: case 0x9B: code = 0xB2; break; // Ś / ś -> 0xB2
        case 0xA0: case 0xA1: code = 0xB3; break; // Š / š -> 0xB3
        case 0xA4: case 0xA5: code = 0xB4; break; // Ť / ť -> 0xB4
        case 0xAA: case 0xAB: code = 0xB5; break; // Ū / ū -> 0xB5
        case 0xAE: case 0xAF: code = 0xB6; break; // Ů / ů -> 0xB6
        case 0xB0: case 0xB1: code = 0xB7; break; // Ű / ű -> 0xB7
        case 0xB9: case 0xBA: code = 0xB8; break; // Ź / ź -> 0xB8
        case 0xBB: case 0xBC: code = 0xB9; break; // Ż / ż -> 0xB9
        case 0xBD: case 0xBE: code = 0xBA; break; // Ž / ž -> 0xBA
        default: code = 0; break;
      }
      if (code && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = (char)code;
        r += 2;
        continue;
      }
    }

    // Romanian (C8) - Ș, Ț
    if (b1 == 0xC8 && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint8_t code = 0;
      switch (b2) {
        case 0x98: case 0x99: code = 0xBB; break; // Ș / ș -> 0xBB
        case 0x9A: case 0x9B: code = 0xBC; break; // Ț / ț -> 0xBC
        default: code = 0; break;
      }
      if (code && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = (char)code;
        r += 2;
        continue;
      }
    }

    // Basic Greek letters (CE/CF xx) and tonos variants
    if ((b1 == 0xCE || b1 == 0xCF) && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      uint8_t code = 0;
      switch (b2) {
        case 0x8C: case 0xAC: code = 0xB4; break; // Ό / ό -> 0xB4 (U+038C)
        case 0x8E: case 0xAE: code = 0xB5; break; // Ύ / ύ -> 0xB5 (U+038E)
        case 0x8F: case 0xAF: code = 0xB6; break; // Ώ / ώ -> 0xB6 (U+038F)
        case 0x93: case 0xB3: code = 0xB7; break; // Γ / γ -> 0xB7 (U+0393)
        case 0x96: case 0xB6: code = 0xB8; break; // Ζ / ζ -> 0xB8 (U+0396)
        case 0x98: case 0xB8: code = 0xB9; break; // Θ / θ -> 0xB9 (U+0398)
        case 0x9A: case 0xBA: code = 0xBA; break; // Κ / κ -> 0xBA (U+039A)
        case 0x9B: case 0xBB: code = 0xBB; break; // Λ / λ -> 0xBB (U+039B)
        case 0x9D: case 0xBD: code = 0xBC; break; // Ν / ν -> 0xBC (U+039D)
        case 0xA1: case 0xB1: code = 0xBD; break; // Ρ / ρ -> 0xBD (U+03A1)
        case 0xA3: case 0x83: code = 0xBE; break; // Σ / σ/ς -> 0xBE (U+03A3)
        case 0xA6: case 0x86: code = 0xBF; break; // Φ / φ -> 0xBF (U+03A6)
        default: code = 0; break;
      }
      if (code && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = (char)code;
        r += 2;
        continue;
      }
    }

    // Cyrillic transliteration (for mixed-script metadata like Russian station names)
    // Use the shared helper `transliterateCyrillic()` for consistent behavior.
    if ((b1 == 0xD0 || b1 == 0xD1) && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      char ascii = transliterateCyrillic(b1, b2);
      if (ascii && w < STATION_FIELD_LENGTH - 1) {
        out[w++] = ascii;
        r += 2;
        continue;
      }
    }

    // Handle valid 2-byte UTF-8 sequences that weren't mapped above
    // Use the shared utf8ToAscii() helper for fallback transliteration
    if (b1 >= 0xC2 && b1 <= 0xDF && str[r + 1]) {
      uint8_t b2 = (uint8_t)str[r + 1];
      char seq[3] = {(char)b1, (char)b2, 0};
      char* tr = utf8ToAscii(seq);
      for (char* p = tr; *p && w < STATION_FIELD_LENGTH - 1; ++p) {
        out[w++] = uppercase ? toupper((unsigned char)*p) : *p;
      }
      r += 2;
      continue;
    }

    // Single byte (either ASCII >127 or continuation byte from broken UTF-8)
    out[w++] = str[r++];
  }

  // Null-terminate compacted output
  out[w] = '\0';

  return out;
}
