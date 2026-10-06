#!/usr/bin/env python3
"""
bdf2adafruit3.py — Convert BDF bitmap font to Adafruit GFXfont header (Python 3)

Target character cell: 6 pixels wide × 8 rows tall (matching CHARWIDTH / glcdfont).

The converter detects the font's bounding-box size and applies smart
padding/cutting rules to normalise every glyph to the target 6×8 cell:
  - Height 6..10 and width 5..9 are handled automatically or interactively.
  - Variable-width fonts are rejected.
  - All other sizes are rejected with a clear error message.

Usage:
    py bdf2adafruit3.py MatrixChunky8x6.bdf 0x0020 0x04FF -o MatrixChunky8x6.h
    py bdf2adafruit3.py MatrixLight8x6.bdf 0x20 0x04FF --auto -o MatrixLight8x6.h

Options:
    --auto      Use default choices for padding/cutting (non-interactive).
    --yes       Same as --auto.

Copyright (c) 2026 Trip5

This is not the same as the tool in the Matrix Fonts repo.
This one is specifically for ehRadio.

SPDX-License-Identifier: GPL-3.0-only
"""

import sys
import re
import os

# ---------------------------------------------------------------------------
# Target cell dimensions
# ---------------------------------------------------------------------------
TARGET_W = 6   # columns wide  (CHARWIDTH)
TARGET_H = 8   # rows tall      (glcdfont line height)
Y_ADVANCE = 8  # cursor step after each glyph

# Accepted ranges (inclusive)
MIN_W = 5; MAX_W = 9
MIN_H = 6; MAX_H = 10


# ---------------------------------------------------------------------------
# BDF parser
# ---------------------------------------------------------------------------

class Glyph:
    __slots__ = ('encoding', 'name', 'advance', 'width', 'height',
                 'xoffs', 'yoffs', 'rows')
    def __init__(self):
        self.encoding = -1
        self.name = ''
        self.advance = 0
        self.width = 0
        self.height = 0
        self.xoffs = 0
        self.yoffs = 0
        self.rows = []


def parse_bdf(filepath: str):
    """Parse a BDF file and return (glyphs dict, ascent, descent)."""
    glyphs = {}
    ascent = 8
    descent = 0
    current = None
    bitmap_mode = False

    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line = line.rstrip('\n\r')
            if not line:
                continue

            if line.startswith('FONT_ASCENT'):
                ascent = int(line.split()[1])
            elif line.startswith('FONT_DESCENT'):
                descent = int(line.split()[1])

            if line.startswith('STARTCHAR'):
                current = Glyph()
                current.name = line.split(None, 1)[1]
                bitmap_mode = False
                continue

            if current is None:
                continue

            if line.startswith('ENDCHAR'):
                if current.encoding >= 0:
                    glyphs[current.encoding] = current
                current = None
                bitmap_mode = False
                continue

            if line.startswith('ENCODING'):
                current.encoding = int(line.split()[1])
            elif line.startswith('DWIDTH'):
                current.advance = int(line.split()[1])
            elif line.startswith('BBX'):
                parts = line.split()
                current.width = int(parts[1])
                current.height = int(parts[2])
                current.xoffs = int(parts[3])
                current.yoffs = int(parts[4])
            elif line.startswith('BITMAP'):
                bitmap_mode = True
            elif bitmap_mode:
                current.rows.append(int(line.strip(), 16))

    return glyphs, ascent, descent


def is_empty_glyph(g: Glyph) -> bool:
    """Check if glyph has no visible pixels."""
    return all(r == 0 for r in g.rows) or len(g.rows) == 0


# ---------------------------------------------------------------------------
# Font validation
# ---------------------------------------------------------------------------

def detect_variable_width(glyphs: dict):
    """Return True if different glyphs have different DWIDTH (advance) values."""
    advances = set()
    for g in glyphs.values():
        if not is_empty_glyph(g):
            advances.add(g.advance)
    return len(advances) > 1


def get_uniform_dwidth(glyphs: dict):
    """Return the DWIDTH if all non-empty glyphs share the same one, else 0."""
    advances = set()
    for g in glyphs.values():
        if not is_empty_glyph(g):
            advances.add(g.advance)
    if len(advances) == 1:
        return advances.pop()
    return 0


def detect_max_width(glyphs: dict):
    """Return max BBX width across all non-empty glyphs."""
    max_w = 0
    for g in glyphs.values():
        if not is_empty_glyph(g):
            if g.width > max_w:
                max_w = g.width
    return max_w


# ---------------------------------------------------------------------------
# Interactive prompts
# ---------------------------------------------------------------------------

def ask_choice(question: str, options: list, default: int = 0):
    """Ask user to pick from a numbered list. Returns chosen index (0-based)."""
    print(f"\n  {question}", file=sys.stderr)
    for i, opt in enumerate(options):
        marker = " (default)" if i == default else ""
        print(f"    [{i+1}] {opt}{marker}", file=sys.stderr)

    if AUTO_MODE:
        print(f"  --auto: choosing [{default+1}] {options[default]}", file=sys.stderr)
        return default

    while True:
        try:
            resp = input(f"  Choose [1-{len(options)}] (default {default+1}): ").strip()
            if resp == '':
                return default
            choice = int(resp) - 1
            if 0 <= choice < len(options):
                return choice
        except (ValueError, EOFError, KeyboardInterrupt):
            pass
        print(f"  Please enter 1-{len(options)}.", file=sys.stderr)


def ask_yesno(question: str, default_yes: bool = True):
    """Ask a yes/no question."""
    prompt = " [Y/n] " if default_yes else " [y/N] "
    if AUTO_MODE:
        answer = 'y' if default_yes else 'n'
        print(f"\n  {question}{prompt}{answer}  (--auto)", file=sys.stderr)
        return default_yes

    while True:
        try:
            resp = input(f"\n  {question}{prompt}").strip().lower()
            if resp == '':
                return default_yes
            if resp in ('y', 'yes'):
                return True
            if resp in ('n', 'no'):
                return False
        except (EOFError, KeyboardInterrupt):
            return default_yes


# ---------------------------------------------------------------------------
# Glyph normalisation
# ---------------------------------------------------------------------------

def normalise_height(rows: list, strategy: str, yoffs: int) -> list:
    """
    Normalise glyph rows to TARGET_H (8) using the font-level strategy
    and the glyph's BBX yoffs for correct vertical positioning.

    BBX yoffs = distance from baseline (Y=0) to the bottom of the bitmap.
    In our 8-row cell: baseline = just below row 7, so:
      - bottom padding = yoffs rows (row 7 - yoffs + 1 through row 7)
      - glyph rows in the middle
      - top padding = remainder

    Font-level cutting is applied first (same for all glyphs), then yoffs
    positions each glyph individually within the cell.
    """
    result = list(rows)

    # 1. Apply font-level cutting strategy (uniform across all glyphs)
    if strategy == 'top':
        result = result[1:]       # discard top row
    elif strategy == 'bottom':
        result = result[:-1]      # discard bottom row
    elif strategy == '1+1':
        result = result[1:-1]     # discard top + bottom
    elif strategy == '2top':
        result = result[2:]       # discard 2 top rows
    elif strategy == '2bottom':
        result = result[:-2]      # discard 2 bottom rows
    # 'none', 'pad-bottom', 'pad-both' — no cutting

    # 2. Position using yoffs (distance from baseline to glyph bottom).
    #    Positive yoffs = space between glyph bottom and baseline.
    #    Negative yoffs = descender (clipped — yOffset=-8 has no below-baseline room).
    bottom_pad = max(0, yoffs)
    top_pad = TARGET_H - len(result) - bottom_pad
    if top_pad < 0:
        # Glyph + baseline space won't fit — clip glyph from bottom
        result = result[:TARGET_H - bottom_pad]
        top_pad = 0

    result = [0] * top_pad + result + [0] * bottom_pad
    result = result[:TARGET_H]
    while len(result) < TARGET_H:
        result.append(0)

    return result


def normalise_width_columns(rows: list, w: int, strategy: str) -> list:
    """
    Normalise glyph rows from width 'w' to TARGET_W (6) columns.
    Each row is an int with MSB=leftmost pixel.
    Returns list of ints (TARGET_W bits significant, left-aligned).

    For padding: we add 0-column(s) on the specified side.
    For cutting: we remove column(s) from the specified side.
    """
    result = []

    if strategy == 'simple':
        # Per-glyph clip/pad: DWIDTH is uniform = TARGET_W,
        # but BBX widths may vary (font still being edited).
        if w <= TARGET_W:
            # Pad right — packer reads TARGET_W bits, extra bits are 0
            result = [r for r in rows]
        else:
            # Clip to leftmost TARGET_W columns (discard rightmost)
            result = [r & 0xFC for r in rows]

    elif w == TARGET_W:
        result = [r for r in rows]

    elif w == 5:
        # Pad 1 column on the right (shift existing bits left by 1 position in bit-space)
        # Actually: keep pixels left-aligned, just ensure width bits are at MSB side.
        # The glyph is 5 bits wide, we want 6 bits wide with padding on right.
        # Each row: significant bits are at positions [7:3], we shift right by 2
        # to make them [5:1], then add 1 column padding on right = no change to bits,
        # just the mask during packing is narrower.
        # Actually, for the GFXfont, the bitmap packer will handle this.
        # We just need to keep the rows as-is (5 significant bits, left-aligned)
        # and the pack_bitmap function will pack them into bytes correctly.
        result = [r for r in rows]

    elif w == 7:
        # Cut 1 column: strategy is 'left' (remove leftmost) or 'right' (remove rightmost)
        if strategy == 'left':
            # Remove leftmost column: shift each row left by 1 bit,
            # zeroing the rightmost significant bit
            for r in rows:
                # Original bits: [7:1] significant (7 bits). Remove bit 7 (MSB).
                # Shift left by 1, mask to 6 bits: (r << 1) & 0xFC
                new_r = (r << 1) & 0xFC  # 0xFC = bits [7:2]
                result.append(new_r)
        else:  # 'right'
            # Remove rightmost column: mask to 6 bits (bits [7:2])
            for r in rows:
                result.append(r & 0xFC)  # keep bits [7:2]

    elif w == 8:
        # Cut 2 columns: strategy is '2left', '2right', or '1+1'
        if strategy == '2left':
            for r in rows:
                result.append((r << 2) & 0xFC)
        elif strategy == '2right':
            for r in rows:
                result.append(r & 0xFC)
        else:  # '1+1'
            for r in rows:
                # Shift left by 1 (remove leftmost), then mask to 6 bits (remove rightmost)
                result.append((r << 1) & 0xFC)

    elif w == 9:
        # Cut 3 columns: strategy is '3left', '3right', or 'balanced' (2+1)
        if strategy == '3left':
            for r in rows:
                result.append((r << 3) & 0xFC)
        elif strategy == '3right':
            for r in rows:
                result.append(r & 0xFC)
        else:  # 'balanced'
            for r in rows:
                result.append((r << 2) & 0xFC)

    return result


# ---------------------------------------------------------------------------
# Determine strategies (interactive or auto)
# ---------------------------------------------------------------------------

def determine_height_strategy(h: int) -> str:
    """Determine height normalisation strategy. Returns strategy string."""
    if h == TARGET_H:
        return 'none'
    elif h == 7:
        print(f"  Height = {h}: adding 1 padding row at bottom → {TARGET_H}", file=sys.stderr)
        return 'pad-bottom'
    elif h == 6:
        print(f"  Height = {h}: adding 1 padding row at top + 1 at bottom → {TARGET_H}", file=sys.stderr)
        return 'pad-both'
    elif h == 9:
        print(f"  Height = {h}: need to cut 1 row to reach {TARGET_H}.", file=sys.stderr)
        choice = ask_choice(
            "Which row should be removed?",
            ["Cut top row (discard uppermost pixels)",
             "Cut bottom row (discard lowermost pixels)"],
            default=0
        )
        strategy = 'top' if choice == 0 else 'bottom'
        print(f"  → Cutting {strategy} row.", file=sys.stderr)
        return strategy
    elif h == 10:
        print(f"  Height = {h}: need to cut 2 rows to reach {TARGET_H}.", file=sys.stderr)
        choice = ask_choice(
            "How should the 2 rows be removed?",
            ["Cut 1 from top + 1 from bottom (balanced)",
             "Cut 2 from top (discard 2 uppermost rows)",
             "Cut 2 from bottom (discard 2 lowermost rows)"],
            default=0
        )
        strategies = {0: '1+1', 1: '2top', 2: '2bottom'}
        strategy = strategies[choice]
        print(f"  → Strategy: {strategy}.", file=sys.stderr)
        return strategy
    else:
        return 'reject'


def determine_width_strategy(w: int) -> str:
    """Determine width normalisation strategy. Returns strategy string."""
    if w == TARGET_W:
        return 'none'
    elif w == 5:
        print(f"  Width = {w}: adding 1 padding column on right → {TARGET_W}", file=sys.stderr)
        return 'pad-right'
    elif w == 7:
        print(f"  Width = {w}: need to cut 1 column to reach {TARGET_W}.", file=sys.stderr)
        choice = ask_choice(
            "Which column should be removed?",
            ["Cut leftmost column (remove first pixel of each row)",
             "Cut rightmost column (remove last pixel of each row)"],
            default=0
        )
        strategy = 'left' if choice == 0 else 'right'
        print(f"  → Cutting {strategy}most column.", file=sys.stderr)
        return strategy
    elif w == 8:
        print(f"  Width = {w}: need to cut 2 columns to reach {TARGET_W}.", file=sys.stderr)
        choice = ask_choice(
            "How should the 2 columns be removed?",
            ["Cut 2 from left (remove 2 leftmost pixels)",
             "Cut 2 from right (remove 2 rightmost pixels)",
             "Cut 1 from left + 1 from right (balanced)"],
            default=2
        )
        strategies = {0: '2left', 1: '2right', 2: '1+1'}
        strategy = strategies[choice]
        print(f"  → Strategy: {strategy}.", file=sys.stderr)
        return strategy
    elif w == 9:
        print(f"  Width = {w}: need to cut 3 columns to reach {TARGET_W}.", file=sys.stderr)
        choice = ask_choice(
            "How should the 3 columns be removed?",
            ["Cut 3 from left",
             "Cut 3 from right",
             "Cut 2 from left + 1 from right (balanced)"],
            default=2
        )
        strategies = {0: '3left', 1: '3right', 2: 'balanced'}
        strategy = strategies[choice]
        print(f"  → Strategy: {strategy}.", file=sys.stderr)
        return strategy
    else:
        return 'reject'


# ---------------------------------------------------------------------------
# Bitmap packing
# ---------------------------------------------------------------------------

def pack_bitmap_rows(rows: list, width: int) -> list:
    """
    Pack a list of row ints (MSB=left pixel, 'width' columns) into a byte array.
    Standard GFXfont 1bpp format: scan left-to-right, top-to-bottom, MSB-first.
    """
    packed = []
    bit_acc = 0
    bit_count = 0

    for row_val in rows:
        for col in range(width):
            mask = 0x80 >> col
            bit = 1 if (row_val & mask) else 0
            bit_acc = (bit_acc << 1) | bit
            bit_count += 1
            if bit_count == 8:
                packed.append(bit_acc)
                bit_acc = 0
                bit_count = 0

    if bit_count > 0:
        bit_acc <<= (8 - bit_count)
        packed.append(bit_acc)

    return packed


# ---------------------------------------------------------------------------
# Output generation
# ---------------------------------------------------------------------------

NULL_GLYPH_DESC = (0, 0, 0, 0, 0, 0)


def generate_header(glyphs_dict, first_cp, last_cp, font_name: str,
                    h_strategy: str, w_strategy: str, orig_w: int, orig_h: int):
    """Generate the complete GFXfont .h file."""
    prefix = re.sub(r'[^a-zA-Z0-9]', '_', font_name)
    if not prefix[0].isalpha():
        prefix = 'F_' + prefix

    range_size = last_cp - first_cp + 1

    lines = []
    lines.append('#pragma once')
    lines.append('#include <Adafruit_GFX.h>')
    lines.append('')
    lines.append(f'// Font: {font_name}')
    lines.append(f'// Original BBX: {orig_w}×{orig_h}  →  normalised to {TARGET_W}×{TARGET_H}')
    lines.append(f'// Height strategy: {h_strategy},  Width strategy: {w_strategy}')
    lines.append(f'// Range: 0x{first_cp:04X}-0x{last_cp:04X}  ({range_size} slots)')
    lines.append(f'// yAdvance: {Y_ADVANCE},  yOffset: -{TARGET_H} (glcdfont-style)')
    lines.append('')

    # Build glyph array
    all_bitmaps = []
    glyph_descriptors = []

    for cp in range(first_cp, last_cp + 1):
        g = glyphs_dict.get(cp)
        if g is None:
            glyph_descriptors.append(NULL_GLYPH_DESC)
        elif is_empty_glyph(g):
            glyph_descriptors.append((0, 0, 0, g.advance, 0, 0))
        else:
            # Normalise height: font-level strategy + per-glyph yoffs positioning
            norm_rows = normalise_height(g.rows, h_strategy, g.yoffs)
            # Normalise width
            norm_rows = normalise_width_columns(norm_rows, g.width, w_strategy)

            offset = len(all_bitmaps)
            packed = pack_bitmap_rows(norm_rows, TARGET_W)
            all_bitmaps.extend(packed)

            glyph_descriptors.append(
                (offset, TARGET_W, TARGET_H, TARGET_W, g.xoffs, -TARGET_H)
            )

    # Write bitmap data.
    # `extern` on all three definitions is deliberate and load-bearing: a namespace-scope const array has
    # INTERNAL linkage, so every translation unit that includes this header would get its own private copy in
    # flash.  With `extern` the data has external linkage, so the header is included from exactly one .cpp and
    # every other unit declares it (see fonts.cpp and dspfont.h).  Measured on the linked ELF before the fix:
    # each font was in the image twice, ~39 KB wasted for the three system fonts.
    lines.append(f'extern const uint8_t {prefix}Bitmaps[] PROGMEM = {{')
    for i in range(0, len(all_bitmaps), 12):
        chunk = all_bitmaps[i:i + 12]
        hex_vals = ', '.join(f'0x{b:02X}' for b in chunk)
        lines.append(f'    {hex_vals},')
    lines.append('};')
    lines.append('')

    # Write glyph descriptors
    lines.append(f'extern const GFXglyph {prefix}Glyphs[] PROGMEM = {{')
    for desc, cp in zip(glyph_descriptors, range(first_cp, last_cp + 1)):
        offset, w, h, adv, xo, yo = desc
        g = glyphs_dict.get(cp)
        name = g.name if g and not is_empty_glyph(g) else '(empty)'
        lines.append(
            f'    {{ {offset}, {w}, {h}, {adv}, {xo}, {yo} }},'
            f' /* 0x{cp:04X} {name} */'
        )
    lines.append('};')
    lines.append('')

    # GFXfont struct
    lines.append(f'extern const GFXfont {prefix} PROGMEM = {{')
    lines.append(f'    (uint8_t *){prefix}Bitmaps,')
    lines.append(f'    (GFXglyph *){prefix}Glyphs,')
    lines.append(f'    0x{first_cp:04X},  /* first */')
    lines.append(f'    0x{last_cp:04X},   /* last */')
    lines.append(f'    {Y_ADVANCE}         /* yAdvance */')
    lines.append('};')
    lines.append('')

    # Stats
    non_empty = sum(1 for d in glyph_descriptors if d != NULL_GLYPH_DESC)
    lines.append(
        f'// {non_empty} glyphs in range ({range_size} slots),'
        f' {len(all_bitmaps)} bytes bitmap data,'
        f' {range_size * 6} bytes glyph table'
    )

    return '\n'.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

AUTO_MODE = False


def main():
    global AUTO_MODE

    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <font.bdf> [first_cp last_cp] [-o output.h] [--auto|--yes]',
              file=sys.stderr)
        sys.exit(1)

    bdf_path = sys.argv[1]

    # Parse optional arguments
    output_path = None
    AUTO_MODE = False

    # Check for --auto / --yes
    for flag in ('--auto', '--yes'):
        if flag in sys.argv:
            AUTO_MODE = True
            sys.argv.remove(flag)

    # Check for -o output file
    if '-o' in sys.argv:
        idx = sys.argv.index('-o')
        if idx + 1 < len(sys.argv):
            output_path = sys.argv[idx + 1]
            sys.argv = sys.argv[:idx] + sys.argv[idx+2:]

    # ------------------------------------------------------------------
    # Parse BDF
    # ------------------------------------------------------------------
    glyphs_dict, ascent, descent = parse_bdf(bdf_path)
    print(f'Parsed {len(glyphs_dict)} glyphs.  Ascent={ascent}, Descent={descent}',
          file=sys.stderr)

    if not glyphs_dict:
        print('ERROR: No glyphs found in BDF file!', file=sys.stderr)
        sys.exit(1)

    # ------------------------------------------------------------------
    # Validate font
    # ------------------------------------------------------------------
    non_empty = sorted(cp for cp, g in glyphs_dict.items() if not is_empty_glyph(g))
    if not non_empty:
        print('ERROR: All glyphs are empty!', file=sys.stderr)
        sys.exit(1)

    print(f'{len(non_empty)} non-empty glyphs.', file=sys.stderr)

    # Check variable width (using DWIDTH / advance)
    if detect_variable_width(glyphs_dict):
        print('ERROR: Variable-width font detected!', file=sys.stderr)
        print('  Different glyphs have different DWIDTH (advance) values.', file=sys.stderr)
        print('  This converter requires a fixed-width (monospace) font.', file=sys.stderr)
        # Print some examples
        advances_seen = {}
        for cp in sorted(glyphs_dict.keys()):
            g = glyphs_dict[cp]
            if not is_empty_glyph(g):
                a = g.advance
                if a not in advances_seen:
                    advances_seen[a] = cp
        for a, cp in sorted(advances_seen.items()):
            print(f'    DWIDTH={a}: example 0x{cp:04X}', file=sys.stderr)
        sys.exit(1)

    # Detect max BBX width
    max_w = detect_max_width(glyphs_dict)
    # Height comes from the BDF header: FONT_ASCENT + FONT_DESCENT
    font_height = ascent + descent
    print(f'Glyph BBX width: {max_w} columns  |  Font height: {font_height} rows (ascent={ascent}+descent={descent})', file=sys.stderr)

    # ------------------------------------------------------------------
    # Determine font name
    # ------------------------------------------------------------------
    font_name = 'CustomFont'
    with open(bdf_path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('FAMILY_NAME'):
                font_name = line.split('"')[1] if '"' in line else \
                            line.split(None, 1)[1].strip()
                break

    # ------------------------------------------------------------------
    # Determine codepoint range
    # ------------------------------------------------------------------
    if len(sys.argv) >= 4:
        first_cp = int(sys.argv[2], 16) if sys.argv[2].startswith('0x') else int(sys.argv[2])
        last_cp = int(sys.argv[3], 16) if sys.argv[3].startswith('0x') else int(sys.argv[3])
        first_cp = max(first_cp, min(non_empty))
        last_cp = min(last_cp, max(non_empty))
    else:
        first_cp = min(non_empty)
        last_cp = max(non_empty)

    range_size = last_cp - first_cp + 1
    est_flash = range_size * 6

    print(f'Font name: {font_name}', file=sys.stderr)
    print(f'Range: 0x{first_cp:04X} — 0x{last_cp:04X} ({range_size} slots)', file=sys.stderr)
    print(f'Estimated glyph table: {est_flash} bytes ({est_flash/1024:.1f} KB)', file=sys.stderr)

    if est_flash > 80000:
        print('WARNING: Glyph table is very large! Consider specifying a tighter',
              file=sys.stderr)
        print('  range, e.g.: py bdf2adafruit3.py font.bdf 0x0020 0x04FF',
              file=sys.stderr)

    # ------------------------------------------------------------------
    # Determine normalisation strategies
    # ------------------------------------------------------------------
    print(f'\n--- Normalising to {TARGET_W}×{TARGET_H} ---', file=sys.stderr)

    # Height
    if font_height < MIN_H or font_height > MAX_H:
        print(f'ERROR: Font height {font_height} is outside accepted range {MIN_H}-{MAX_H}.',
              file=sys.stderr)
        print(f'  The font must be {MIN_H}-{MAX_H} rows tall (target: {TARGET_H}).',
              file=sys.stderr)
        sys.exit(1)

    h_strategy = determine_height_strategy(font_height)
    if h_strategy == 'reject':
        print(f'ERROR: Cannot handle height {font_height}.', file=sys.stderr)
        sys.exit(1)

    # Width
    uniform_dw = get_uniform_dwidth(glyphs_dict)
    if uniform_dw == TARGET_W:
        # All glyphs share DWIDTH=6 — per-glyph simple clip/pad.
        # Allows processing in-progress fonts where some glyph BBX widths
        # still exceed the target (they get right-clipped).
        print(f'  DWIDTH is uniform = {TARGET_W}: using per-glyph clip/pad.', file=sys.stderr)
        if max_w > TARGET_W:
            print(f'  Wide glyphs (>{TARGET_W} cols) will be right-clipped.', file=sys.stderr)
        w_strategy = 'simple'
    elif max_w < MIN_W or max_w > MAX_W:
        print(f'ERROR: Font width {max_w} is outside accepted range {MIN_W}-{MAX_W}.',
              file=sys.stderr)
        print(f'  The font must be {MIN_W}-{MAX_W} columns wide (target: {TARGET_W}).',
              file=sys.stderr)
        sys.exit(1)
    else:
        w_strategy = determine_width_strategy(max_w)
        if w_strategy == 'reject':
            print(f'ERROR: Cannot handle width {max_w}.', file=sys.stderr)
            sys.exit(1)

    # ------------------------------------------------------------------
    # Generate output
    # ------------------------------------------------------------------
    output = generate_header(glyphs_dict, first_cp, last_cp, font_name,
                             h_strategy, w_strategy, max_w, font_height)

    if output_path:
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(output)
        print(f'\nWrote to {output_path}', file=sys.stderr)
    else:
        print(output)


if __name__ == '__main__':
    main()
