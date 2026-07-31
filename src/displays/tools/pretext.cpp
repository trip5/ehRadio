#include "pretext.h"

// Pre-render text processing pipeline.  Add new preprocessors here.
// No-op unless PRETEXT_FOLDACCENT or PRETEXT_ALLCAPS is defined.
uint16_t preText(uint16_t cp, const GFXfont *font) {
  #if defined(PRETEXT_FOLDACCENT) || defined(PRETEXT_ALLCAPS) || defined(PRETEXT_FOLDCYRILLIC)
    #ifdef PRETEXT_ALLCAPS
      cp = allCaps(cp);
    #endif
    #ifdef PRETEXT_FOLDACCENT
      cp = foldAccent(cp);
    #endif
    #ifdef PRETEXT_FOLDCYRILLIC
      cp = foldCyrillic(cp);
    #endif
    return cp;
  #else
    return cp;  // no-op when no preprocessors enabled
  #endif
}

// Convert lowercase letters to uppercase (Latin + Cyrillic).
// Used only when PRETEXT_ALLCAPS is defined (font testing).
uint16_t allCaps(uint16_t cp) {
  // ASCII a-z → A-Z
  if (cp >= 'a' && cp <= 'z') return cp - 32;
  // Latin-1 Supplement lowercase accented → uppercase
  if (cp >= 0x00E0 && cp <= 0x00F6 && cp != 0x00F7) return cp - 32;
  if (cp == 0x00F8) return 0x00D8;  // ø → Ø
  if (cp >= 0x00F9 && cp <= 0x00FE) return cp - 32;
  if (cp == 0x00FF) return 0x0178;  // ÿ → Ÿ
  // Latin Extended-A lowercase → uppercase (0x0100-0x0177: even=upper, odd=lower)
  if (cp >= 0x0101 && cp <= 0x0177 && (cp & 1)) return cp - 1;
  // Latin Extended-A Z-range (0x0179-0x017E): parity reversed because
  // U+0178 Ÿ (Y-diaeresis) occupies the even slot without a lowercase pair.
  // Here even=lower, odd=upper.
  if (cp >= 0x017A && cp <= 0x017E && !(cp & 1)) return cp - 1;
  // Cyrillic lowercase а-я → А-Я (U+0430–U+044F → U+0410–U+042F)
  if (cp >= 0x0430 && cp <= 0x044F) return cp - 32;
  // Cyrillic lowercase ё → Ё
  if (cp == 0x0451) return 0x0401;
  // Cyrillic Extended lowercase → uppercase
  if (cp >= 0x0460 && cp <= 0x0481 && (cp & 1)) return cp - 1;
  if (cp >= 0x048A && cp <= 0x04BF && (cp & 1)) return cp - 1;
  if (cp >= 0x04D0 && cp <= 0x04FF && (cp & 1)) return cp - 1;
  return cp;  // not lowercase, return unchanged
}

// Strip diacritical marks from Latin-1 and Latin Extended-A characters.
// Folds unconditionally — does not check whether the glyph exists in the
// font.  (Use glyphAvailable() before calling if conditional folding is
// desired; checkFallbackGlyph() handles this for the fallback path.)
// Returns the folded codepoint, or cp unchanged if no mapping applies,
// or 0 if the codepoint has no ASCII equivalent.
uint16_t foldAccent(uint16_t cp) {
  // Latin-1 Supplement (U+00C0–U+00FF) → ASCII
  if (cp >= 0x00C0 && cp <= 0x00C5) return 'A';
  if (cp == 0x00C7) return 'C';
  if (cp >= 0x00C8 && cp <= 0x00CB) return 'E';
  if (cp >= 0x00CC && cp <= 0x00CF) return 'I';
  if (cp == 0x00D1) return 'N';
  if (cp >= 0x00D2 && cp <= 0x00D6) return 'O';
  if (cp == 0x00D8) return 'O';
  if (cp >= 0x00D9 && cp <= 0x00DC) return 'U';
  if (cp == 0x00DD) return 'Y';
  if (cp >= 0x00E0 && cp <= 0x00E5) return 'a';
  if (cp == 0x00E7) return 'c';
  if (cp >= 0x00E8 && cp <= 0x00EB) return 'e';
  if (cp >= 0x00EC && cp <= 0x00EF) return 'i';
  if (cp == 0x00F1) return 'n';
  if (cp >= 0x00F2 && cp <= 0x00F6) return 'o';
  if (cp == 0x00F8) return 'o';
  if (cp >= 0x00F9 && cp <= 0x00FC) return 'u';
  if (cp == 0x00FD) return 'y';
  if (cp == 0x00FF) return 'y';
  // Latin Extended-A (U+0100–U+017F) → ASCII
  if (cp >= 0x0100 && cp <= 0x0105) return 'A';
  if (cp >= 0x0106 && cp <= 0x010D) return 'C';
  if (cp >= 0x010E && cp <= 0x0111) return 'D';
  if (cp >= 0x0112 && cp <= 0x011B) return 'E';
  if (cp >= 0x011C && cp <= 0x0123) return 'G';
  if (cp >= 0x0124 && cp <= 0x0127) return 'H';
  if (cp >= 0x0128 && cp <= 0x0131) return 'I';
  if (cp >= 0x0134 && cp <= 0x0135) return 'J';
  if (cp >= 0x0136 && cp <= 0x0138) return 'K';
  if (cp >= 0x0139 && cp <= 0x0142) return 'L';
  if (cp >= 0x0143 && cp <= 0x014B) return 'N';
  if (cp >= 0x014C && cp <= 0x0151) return 'O';
  if (cp >= 0x0154 && cp <= 0x0159) return 'R';
  if (cp >= 0x015A && cp <= 0x0161) return 'S';
  if (cp >= 0x0162 && cp <= 0x0167) return 'T';
  if (cp >= 0x0168 && cp <= 0x0173) return 'U';
  if (cp >= 0x0174 && cp <= 0x0175) return 'W';
  if (cp >= 0x0176 && cp <= 0x0178) return 'Y';
  if (cp >= 0x0179 && cp <= 0x017E) return 'Z';
  return 0;
}

// Check whether a codepoint has a renderable glyph in the font.
// Returns true if cp is within [font->first..font->last] AND the
// glyph slot has non-zero width and height.
bool glyphAvailable(uint16_t cp, const GFXfont *font) {
  uint16_t first = pgm_read_word(&font->first);
  uint16_t last  = pgm_read_word(&font->last);
  if (cp < first || cp > last) return false;
  GFXglyph *glyph = (GFXglyph *)pgm_read_ptr(&font->glyph);
  glyph += (cp - first);
  return pgm_read_byte(&glyph->width) > 0 &&
         pgm_read_byte(&glyph->height) > 0;
}

// Map non-basic Cyrillic characters to the closest basic Cyrillic or
// visually-equivalent Latin letter.  Covers all modern-language Cyrillic
// outside the Russian alphabet (0x0410-0x044F): Slavic extensions for
// Serbian/Macedonian/Ukrainian/Belarusian, non-Slavic extended letters
// (Kazakh, Mongolian, Tatar, etc.), and accented Cyrillic (Chuvash, Mari,
// Udmurt, etc.).  Skips historical/religious characters (0x0460-0x0489).
uint16_t foldCyrillic(uint16_t cp) {
  // Slavic extensions (0x0400-0x040F, 0x0450-0x045F) — uppercase at
  // 0x04xx, lowercase at 0x045x; bit 0x0050 selects the case.
  bool lo = cp & 0x0050;
  if (cp == 0x0400 || cp == 0x0450) return lo ? 0x0435 : 0x0415; // Ѐ/ѐ → Е/е
  if (cp == 0x0402 || cp == 0x0452) return lo ? 0x0434 : 0x0414; // Ђ/ђ → Д/д
  if (cp == 0x0403 || cp == 0x0453) return lo ? 0x0433 : 0x0413; // Ѓ/ѓ → Г/г
  if (cp == 0x0405 || cp == 0x0455) return lo ? 0x0441 : 0x0421; // Ѕ/ѕ → С/с
  if (cp == 0x0408 || cp == 0x0458) return lo ? 'j'   : 'J';    // Ј/ј → J/j
  if (cp == 0x0409 || cp == 0x0459) return lo ? 0x043B : 0x041B; // Љ/љ → Л/л
  if (cp == 0x040A || cp == 0x045A) return lo ? 0x043D : 0x041D; // Њ/њ → Н/н
  if (cp == 0x040B || cp == 0x045B) return lo ? 0x0442 : 0x0422; // Ћ/ћ → Т/т
  if (cp == 0x040C || cp == 0x045C) return lo ? 'k'   : 'K';    // Ќ/ќ → K/k
  if (cp == 0x040D || cp == 0x045D) return lo ? 0x0438 : 0x0418; // Ѝ/ѝ → И/и
  if (cp == 0x040F || cp == 0x045F) return lo ? 0x0446 : 0x0426; // Џ/џ → Ц/ц

  // Non-Slavic extended (0x048A-0x04BF): even=upper, odd=lower pairs.
  // Map each pair to the visually closest basic Cyrillic letter.
  if (cp >= 0x0492 && cp <= 0x0495) return (cp & 1) ? 0x0433 : 0x0413; // Ғ-ҕ → Г
  if (cp >= 0x0496 && cp <= 0x0497) return (cp & 1) ? 0x0436 : 0x0416; // Җ-җ → Ж
  if (cp >= 0x0498 && cp <= 0x0499) return (cp & 1) ? 0x0437 : 0x0417; // Ҙ-ҙ → З
  if (cp >= 0x049A && cp <= 0x049D) return (cp & 1) ? 0x043A : 0x041A; // Қ-ҝ → К
  if (cp >= 0x049E && cp <= 0x04A1) return (cp & 1) ? 0x043A : 0x041A; // Ҟ-ҡ → К
  if (cp >= 0x04A2 && cp <= 0x04A5) return (cp & 1) ? 0x043D : 0x041D; // Ң-ҥ → Н
  if (cp >= 0x04A6 && cp <= 0x04A7) return (cp & 1) ? 0x043F : 0x041F; // Ҧ-ҧ → П
  if (cp >= 0x04A8 && cp <= 0x04A9) return (cp & 1) ? 'h'   : 'H';    // Ҩ-ҩ → H
  if (cp >= 0x04AA && cp <= 0x04AB) return (cp & 1) ? 0x0441 : 0x0421; // Ҫ-ҫ → С
  if (cp >= 0x04AC && cp <= 0x04AD) return (cp & 1) ? 0x0442 : 0x0422; // Ҭ-ҭ → Т
  if (cp >= 0x04AE && cp <= 0x04B1) return (cp & 1) ? 0x0443 : 0x0423; // Ү-ұ → У
  if (cp >= 0x04B2 && cp <= 0x04B3) return (cp & 1) ? 0x0445 : 0x0425; // Ҳ-ҳ → Х
  if (cp >= 0x04B4 && cp <= 0x04B5) return (cp & 1) ? 0x0446 : 0x0426; // Ҵ-ҵ → Ц
  if (cp >= 0x04B6 && cp <= 0x04B9) return (cp & 1) ? 0x0447 : 0x0427; // Ҷ-ҹ → Ч
  if (cp >= 0x04BA && cp <= 0x04BB) return (cp & 1) ? 0x0445 : 0x0425; // Һ-һ → Х
  if (cp >= 0x04BC && cp <= 0x04BF) return (cp & 1) ? 0x0447 : 0x0427; // Ҽ-ҿ → Ч

  // Palochka (0x04C0) — vertically similar to Latin I
  if (cp == 0x04C0) return 'I';

  // Accented/extended Cyrillic (0x04C1-0x04FF): even=upper, odd=lower pairs.
  // Strip diacritic (breve/diaeresis/macron/hook/descender) → base letter.
  if (cp >= 0x04C1 && cp <= 0x04C2) return (cp & 1) ? 0x0436 : 0x0416; // Ӂ-ӂ → Ж
  if (cp >= 0x04C3 && cp <= 0x04C4) return (cp & 1) ? 0x043A : 0x041A; // Ӄ-ӄ → К
  if (cp >= 0x04C5 && cp <= 0x04C6) return (cp & 1) ? 0x043B : 0x041B; // Ӆ-ӆ → Л
  if (cp >= 0x04C7 && cp <= 0x04CA) return (cp & 1) ? 0x043D : 0x041D; // Ӈ-ӊ → Н
  if (cp >= 0x04CB && cp <= 0x04CC) return (cp & 1) ? 0x0447 : 0x0427; // Ӌ-ӌ → Ч
  if (cp >= 0x04CD && cp <= 0x04CE) return (cp & 1) ? 0x043C : 0x041C; // Ӎ-ӎ → М
  if (cp >= 0x04D0 && cp <= 0x04D3) return (cp & 1) ? 0x0430 : 0x0410; // Ӑ-ӓ → А
  if (cp >= 0x04D4 && cp <= 0x04D5) return (cp & 1) ? 0x0430 : 0x0410; // Ӕ-ӕ → А
  if (cp >= 0x04D6 && cp <= 0x04D7) return (cp & 1) ? 0x0435 : 0x0415; // Ӗ-ӗ → Е
  if (cp >= 0x04D8 && cp <= 0x04DB) return (cp & 1) ? 0x0430 : 0x0410; // Ә-ӛ → А
  if (cp >= 0x04DC && cp <= 0x04DD) return (cp & 1) ? 0x0436 : 0x0416; // Ӝ-ӝ → Ж
  if (cp >= 0x04DE && cp <= 0x04DF) return (cp & 1) ? 0x0437 : 0x0417; // Ӟ-ӟ → З
  if (cp >= 0x04E0 && cp <= 0x04E1) return (cp & 1) ? 0x0437 : 0x0417; // Ӡ-ӡ → З
  if (cp >= 0x04E2 && cp <= 0x04E5) return (cp & 1) ? 0x0438 : 0x0418; // Ӣ-ӥ → И
  if (cp >= 0x04E6 && cp <= 0x04EB) return (cp & 1) ? 0x043E : 0x041E; // Ӧ-ӫ → О
  if (cp >= 0x04EC && cp <= 0x04ED) return (cp & 1) ? 0x044D : 0x042D; // Ӭ-ӭ → Э
  if (cp >= 0x04EE && cp <= 0x04F3) return (cp & 1) ? 0x0443 : 0x0423; // Ӯ-ӳ → У
  if (cp >= 0x04F4 && cp <= 0x04F5) return (cp & 1) ? 0x0447 : 0x0427; // Ӵ-ӵ → Ч
  if (cp >= 0x04F6 && cp <= 0x04F7) return (cp & 1) ? 0x0433 : 0x0413; // Ӷ-ӷ → Г
  if (cp >= 0x04F8 && cp <= 0x04F9) return (cp & 1) ? 0x044B : 0x042B; // Ӹ-ӹ → Ы

  return 0;
}

// Try all fallback strategies for an unavailable glyph.
// Returns cp unchanged if the glyph is already available, otherwise
// chains through foldCyrillic → foldAccent to find a substitute.
// Returns 0 if no fallback renders.
uint16_t checkFallbackGlyph(uint16_t cp, const GFXfont *font) {
  if (glyphAvailable(cp, font)) return cp;
  uint16_t mapped = foldCyrillic(cp);
  if (mapped && glyphAvailable(mapped, font)) return mapped;
  mapped = foldAccent(cp);
  if (mapped && mapped != cp && glyphAvailable(mapped, font)) return mapped;
  return 0;
}

// Count Unicode characters (not bytes) in a UTF-8 string.
uint16_t utf8_strlen(const char *s) {
  uint16_t count = 0;
  while (*s) {
    if ((*s & 0xC0) != 0x80) count++; // not a continuation byte
    s++;
  }
  return count;
}

// Return byte pointer to the Nth Unicode character in a UTF-8 string.
const char* utf8_offset(const char *s, uint16_t charIndex) {
  uint16_t idx = 0;
  while (*s && idx < charIndex) {
    if ((*s & 0xC0) != 0x80) idx++; // not a continuation byte
    s++;
  }
  return s;
}
