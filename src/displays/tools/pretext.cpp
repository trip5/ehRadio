#include "pretext.h"
#include "dspstats.h"   // Core Monitor counters
#include "pretext_fold.h"   // generated stepping table
#include "../../core/config.h"   // config.store.pretextALLCAPS, the runtime form of PRETEXT_ALLCAPS

#include <string.h>

// ==========================================================================
// Font-aware text resolution
// ==========================================================================
// preText() answers one question per codepoint: what can THIS font actually
// draw?  It walks a chain of ever-coarser representations and stops at the
// first one the font carries:
//
//   keep      the font has the glyph                     -> render as-is
//   fold      a step lands on something the font has     -> render that
//   replace   the chain runs out                         -> render a substitute
//
// The chain is what makes one table serve fonts of different coverage.  A font
// with polytonic Greek keeps U+1F00 as U+1F00; a font with only mono-tonic Greek
// gets U+03B1; a Latin-only font gets 'a'.  Nothing is special-cased per script:
// there is one stepping table and a loop.
//
// CONTRACT - every widget layout calculation depends on this:
//   one input codepoint produces EXACTLY one output codepoint, and never 0.
//   widgets.cpp sizes scrolling text as utf8_strlen(text) * charWidth, so a
//   mapping that changed the codepoint count, or that returned 0, would
//   desynchronise measurement from drawing.  (The original foldAccent()
//   returned 0 for every codepoint outside Latin-1 / Latin Extended-A, which
//   destroyed Greek, Cyrillic, 0x1E9E and the bullet outright whenever
//   PRETEXT_FOLDACCENT was enabled.)
// preTextString() below is the one place a codepoint may disappear, and only
// for invisible modifiers - see its comment for why that is safe there.
// ==========================================================================

// Shown when the chain runs out.  An underscore is preferred because a blank
// cell is indistinguishable from a space; a question mark backs it up, and a
// space is the last resort because the renderer can always advance a cell.
static const uint16_t SUBSTITUTE_FIRST  = '_';
static const uint16_t SUBSTITUTE_SECOND = '?';
static const uint16_t SUBSTITUTE_LAST   = ' ';

// Longest real chain: a polytonic vowel that decomposes to another precomposed
// vowel before reaching the base letter, e.g.
//   U+1FB4 -> U+03AC -> U+03B1 -> 'a'
#define PRETEXT_MAX_STEPS 4

// --------------------------------------------------------------------------
// Learned results
// --------------------------------------------------------------------------
// Only codepoints the font CANNOT draw reach the table, but those repeat on
// every repaint of the same label, so the answer is memoised.  Entries are keyed
// on the font pointer rather than an epoch counter, which makes invalidation
// automatic and free: a different font simply misses every entry.  That is also
// exactly what a runtime font switch needs, so no extra hook is required of it.
#define PRETEXT_CACHE_SLOTS 128

struct PreTextCacheEntry {
  uint32_t cp;        // the codepoint that was asked about (0 = empty slot)
  uint16_t out;       // what to draw instead (always <= 0xFFFF)
  const GFXfont *font;
};

static PreTextCacheEntry preTextCache[PRETEXT_CACHE_SLOTS];

void preTextInvalidateCache() {
  memset(preTextCache, 0, sizeof(preTextCache));
}

static inline PreTextCacheEntry *cacheSlot(uint16_t cp) {
  return &preTextCache[(uint8_t)(cp ^ (cp >> 7)) & (PRETEXT_CACHE_SLOTS - 1)];
}

// --------------------------------------------------------------------------
// Invisible codepoints
// --------------------------------------------------------------------------
// Marks that only modify the glyph before them, plus the zero-width formatting
// characters.  Curated rather than exhaustive: these are the families that
// accompany the scripts this pipeline folds.  A mark outside the list is not
// silently dropped by the per-glyph path - it becomes a substitute, which makes
// it visible in a bug report instead of invisible on a screen.
static bool preTextIsInvisible(uint32_t cp) {
  return (cp >= 0x0300 && cp <= 0x036F) ||   // combining diacritical marks
         (cp >= 0x0483 && cp <= 0x0487) ||   // Cyrillic combining
         (cp >= 0x1AB0 && cp <= 0x1AFF) ||   // combining marks extended
         (cp >= 0x1DC0 && cp <= 0x1DFF) ||   // combining marks supplement
         (cp >= 0x20D0 && cp <= 0x20FF) ||   // combining marks for symbols
         (cp >= 0xFE00 && cp <= 0xFE0F) ||   // variation selectors
         (cp >= 0xFE20 && cp <= 0xFE2F) ||   // combining half marks
         cp == 0x00AD ||                     // soft hyphen
         cp == 0x200B || cp == 0x200C || cp == 0x200D || cp == 0x2060 ||
         cp == 0xFEFF;                       // zero width space/joiner/BOM
}

// --------------------------------------------------------------------------
// Transforms
// --------------------------------------------------------------------------

// Convert lowercase letters to uppercase (Latin + Cyrillic).
// Used only when PRETEXT_ALLCAPS is defined (font testing).
uint16_t allCaps(uint16_t cp) {
  if (cp >= 'a' && cp <= 'z') return cp - 32;

  // Latin-1 Supplement (0xD7 is multiply, so it is excluded)
  if (cp >= 0x00E0 && cp <= 0x00F6 && cp != 0x00F7) return cp - 32;
  if (cp == 0x00F8) return 0x00D8;
  if (cp >= 0x00F9 && cp <= 0x00FE) return cp - 32;
  if (cp == 0x00FF) return 0x0178;

  // Latin Extended-A.  Most of the block alternates even=upper, odd=lower, but
  // three zones break that:
  //   - 0x0131 dotless i uppercases to plain I, not to 0x0130
  //   - 0x0138 kra and 0x0149 n-apostrophe are lowercase with no uppercase
  //   - 0x0139-0x0142 and 0x0143-0x0148 put the uppercase on the ODD slot,
  //     the opposite of the surrounding block
  if (cp >= 0x0101 && cp <= 0x0137 && (cp & 1)) return cp - 1;
  if (cp == 0x0131) return 'I';
  if (cp == 0x0138) return cp;
  if (cp >= 0x013A && cp <= 0x0142 && !(cp & 1)) return cp - 1;
  if (cp >= 0x0144 && cp <= 0x0148 && !(cp & 1)) return cp - 1;
  if (cp == 0x0149) return cp;
  if (cp >= 0x014B && cp <= 0x0177 && (cp & 1)) return cp - 1;
  if (cp >= 0x017A && cp <= 0x017E && !(cp & 1)) return cp - 1;
  if (cp == 0x017F) return 'S';

  // Cyrillic
  if (cp >= 0x0430 && cp <= 0x044F) return cp - 32;
  if (cp == 0x0451) return 0x0401;
  if (cp >= 0x0460 && cp <= 0x0481 && (cp & 1)) return cp - 1;
  if (cp >= 0x048A && cp <= 0x04BF && (cp & 1)) return cp - 1;
  if (cp >= 0x04D0 && cp <= 0x04FF && (cp & 1)) return cp - 1;

  return cp;
}

// --------------------------------------------------------------------------
// Table lookup
// --------------------------------------------------------------------------

// One step of the chain.  Returns cp unchanged when the table has nothing,
// which keeps the walk loop simple.
uint16_t preTextFoldStep(uint32_t cp) {
  // Two halves keep both entries 4 bytes wide: the low half is keyed on the
  // codepoint, the high half on the offset from 0x10000.
  const PreTextFoldStep *table = preTextFoldTable;
  uint16_t len = (uint16_t)PRETEXT_FOLD_TABLE_LEN;
  uint16_t wanted;
  if (cp >= PRETEXT_FOLD_HIGH_BASE) {
    table = preTextFoldTableHigh;
    len = (uint16_t)PRETEXT_FOLD_TABLE_HIGH_LEN;
    wanted = (uint16_t)(cp - PRETEXT_FOLD_HIGH_BASE);
  } else {
    wanted = (uint16_t)cp;
  }

  uint16_t lo = 0, hi = len;
  while (lo < hi) {
    uint16_t mid = (uint16_t)(lo + ((hi - lo) >> 1));
    uint16_t key = pgm_read_word(&table[mid].key);
    if (key == wanted) return pgm_read_word(&table[mid].next);
    if (key < wanted) lo = (uint16_t)(mid + 1);
    else hi = mid;
  }
  return (uint16_t)cp;   // no step; callers compare against their input
}

// True when cp is inside the font's [first..last] range AND its glyph slot has
// a non-zero bitmap.  The font generator emits all-zero slots for codepoints it
// could not convert, and those are not renderable.
bool glyphAvailable(uint32_t cp, const GFXfont *font) {
  if (font == nullptr) return false;
  uint16_t first = pgm_read_word(&font->first);
  uint16_t last  = pgm_read_word(&font->last);
  if (cp < first || cp > last) return false;
  GFXglyph *glyph = (GFXglyph *)pgm_read_ptr(&font->glyph);
  glyph += (cp - first);
  return pgm_read_byte(&glyph->width) > 0 &&
         pgm_read_byte(&glyph->height) > 0;
}

static uint16_t substituteGlyph(const GFXfont *font) {
  if (glyphAvailable(SUBSTITUTE_FIRST, font))  return SUBSTITUTE_FIRST;
  if (glyphAvailable(SUBSTITUTE_SECOND, font)) return SUBSTITUTE_SECOND;
  return SUBSTITUTE_LAST;
}

// --------------------------------------------------------------------------
// Resolver
// --------------------------------------------------------------------------

uint16_t preText(uint32_t cp, const GFXfont *font) {
  // Control range and space are handled by the renderer before this point
  // (icons, newline, carriage return, the space advance) and neither can be
  // substituted without changing the layout.
  if (cp < 0x20 || cp == ' ' || cp == 0) return (uint16_t)cp;
  if (font == nullptr) return (uint16_t)cp;

  // Counted from here: a codepoint that got this far needed an answer, and the
  // two early-outs above decided nothing.  A "hit" below is an answer that did
  // not need the fold chain, so the pair measures how much real work is left.
  cmCountPreTextCall();

  const bool caps = editorOpt(config.store.pretextALLCAPS);
  // The cache is keyed on the caller's codepoint, which is taken BEFORE the transform, so the flag has to be part of
  // the key or turning all-caps off would keep serving the capped entry.  Codepoints stop at 0x10FFFF, so the top
  // bit is free: folding it in costs nothing and needs no cache flush.
  const uint32_t asked = cp | (caps ? 0x80000000u : 0u);  // cache key: the caller's codepoint, plus the caps flag

  if (caps) cp = allCaps(cp);
  #if defined(PRETEXT_FOLDACCENT) || defined(PRETEXT_FOLDCYRILLIC)
    // Kept as a deliberate "force fold": take a step even where the glyph
    // exists, for an intentionally ASCII-only display.
    uint16_t forced = preTextFoldStep(cp);
    if (forced != cp) cp = forced;
  #endif

  if (glyphAvailable(cp, font)) { cmCountPreTextHit(); return (uint16_t)cp; }   // kept (the cheap path)

  PreTextCacheEntry *slot = cacheSlot(asked);
  if (slot->cp == asked && slot->font == font) { cmCountPreTextHit(); return slot->out; }

  uint16_t out = 0;
  for (uint8_t step = 0; step < PRETEXT_MAX_STEPS; step++) {
    uint16_t next = preTextFoldStep(cp);
    if (next == cp) break;                      // chain ended
    cp = next;
    if (glyphAvailable(cp, font)) { out = cp; break; }
  }
  if (out == 0) out = substituteGlyph(font);    // never 0

  slot->cp = asked;
  slot->out = out;
  slot->font = font;
  return out;
}

// --------------------------------------------------------------------------
// Whole-string resolution (the ingress pass)
// --------------------------------------------------------------------------

// Resolve a UTF-8 string in place, once, where it enters a widget.
//
// Doing it here rather than per glyph matters: a scrolling label re-prints its
// whole window on every scroll step, so a per-glyph pass repeats the same work
// 50 times a second for as long as the title scrolls.  It also removes a latent
// divergence - measurement and drawing then use the SAME bytes by construction,
// instead of agreeing only because resolution happens to be 1:1.
//
// The rewrite is safe in place because no step in the table grows a codepoint's
// UTF-8 length (the generator asserts it), so the output never needs more room
// than the input.
//
// Combining marks and zero-width characters are dropped here.  That is the only
// place a codepoint may vanish, and it is correct here precisely because this is
// ingress: the buffer widgets measure and draw is the string this function
// returns.  The per-glyph path keeps its 1:1 contract and shows a substitute for
// those codepoints instead, so nothing is ever silently swallowed mid-pipeline.
void preTextString(char *s, const GFXfont *font) {
  char *dst = s;
  const char *src = s;
  while (*src) {
    uint8_t lead = (uint8_t)*src;
    uint32_t cp;                    // 32-bit: a four-byte sequence must survive
    uint8_t len;
    if (lead < 0x80) { cp = lead; len = 1; }
    else if ((lead & 0xE0) == 0xC0) { cp = lead & 0x1F; len = 2; }
    else if ((lead & 0xF0) == 0xE0) { cp = lead & 0x0F; len = 3; }
    else if ((lead & 0xF8) == 0xF0) { cp = lead & 0x07; len = 4; }
    else { src++; continue; }                   // stray continuation byte

    uint8_t i = 1;
    for (; i < len; i++) {
      if (((uint8_t)src[i] & 0xC0) != 0x80) break;   // truncated sequence
      cp = (cp << 6) | ((uint8_t)src[i] & 0x3F);
    }
    if (i < len) { src++; continue; }           // malformed: drop the lead byte
    src += len;

    if (preTextIsInvisible(cp)) continue;

    uint16_t out = preText(cp, font);
    if (out < 0x80) {
      *dst++ = (char)out;
    } else if (out < 0x800) {
      *dst++ = (char)(0xC0 | (out >> 6));
      *dst++ = (char)(0x80 | (out & 0x3F));
    } else {
      *dst++ = (char)(0xE0 | (out >> 12));
      *dst++ = (char)(0x80 | ((out >> 6) & 0x3F));
      *dst++ = (char)(0x80 | (out & 0x3F));
    }
  }
  *dst = '\0';
}

// --------------------------------------------------------------------------
// UTF-8 helpers (used by the widgets for width and slicing)
// --------------------------------------------------------------------------

uint16_t utf8_strlen(const char *s) {
  uint16_t count = 0;
  while (*s) {
    if ((*s & 0xC0) != 0x80) count++; // not a continuation byte
    s++;
  }
  return count;
}

const char* utf8_offset(const char *s, uint16_t charIndex) {
  uint16_t idx = 0;
  while (*s && idx < charIndex) {
    if ((*s & 0xC0) != 0x80) idx++; // not a continuation byte
    s++;
  }
  return s;
}
