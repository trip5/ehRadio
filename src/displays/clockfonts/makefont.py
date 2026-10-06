#!/usr/bin/env python3
"""
makefont.py — build one clock font STYLE from its folder of PNGs.

    py makefont.py LED
    py makefont.py Chunky6px
    py makefont.py Chunky6 --tight-bounds

Input:   clockfonts/<Style>/{10,15,21,28,35,52,70}.png
Output:  clockfonts/<Style>.h

A style is ALL SEVEN SIZES or it is not a style, so a missing file is a hard error naming every file
that is absent — never a half-built header.  A file whose IMAGE HEIGHT does not match the size in its
filename is refused for the same reason: the seven sizes are seven separate hand-drawn designs, so a
height that disagrees with the name is a mis-saved or mis-named export and never a deliberate one.
(That check is what stops an 8 px drawing being dropped in as 10.png.)

Those seven are the ladder: 10, 15, 21 and 28 are the seconds sizes and 15, 35, 52 and 70 the clock
ones, and the order in the header is ascending so ClockFontStyle::sizes[] can be indexed by slot.

PNG layout (left to right):  0  1  2  3  4  5  6  7  8  9  :
  - White (or light) glyphs on a black background.
  - Each digit cell is 2x the width of the colon cell, so the total width is divisible by 21.
    Chunky6 is a pixel-display font built from five rows of a square pixel, so its natural heights
    are 5, 10, 15 ... 70: the original four are 70 x 56, 52 x 40, 35 x 28 and 15 x 12.  The later
    sizes are hand-drawn — 10 happens to land on that grid, 21 and 28 do not — so none of them can
    be derived from another.  See ClockFonts.txt.
  - Draw new ones from clockfonts/templates/Template_<size>.png.  The templates live outside
    the style folders precisely because this tool counts the files in them.

Every symbol is unique per style and size (LED_35, Chunky6px_35, ...) and every definition is
`extern const` so it has external linkage: the whole point is that the three styles can all be
compiled at once and switched at runtime, and that a namespace-scope const does not end up in
flash once per translation unit that reaches dspfont.h.  Only clockfonts.cpp includes the
generated headers; dspfont.h only declares the ClockFontStyle instances.

The glow CHARACTER is emitted here, and only the character: this is the tool that chose the glyph
slot, so it is the only thing that knows which one to name.  The two shapes the widget draws with it
— the clock's <g><g>:<g><g> and the seconds' <g><g> — are built in the widget, because the clock's
shape is also its WIDTH TEMPLATE (see the clock-widget code).  --glow-char picks the slot.

The old one-PNG/one-size tool (Trip5/png_to_gfxfont.py) is folded into this one: same glyph
conversion, but it takes the whole style so the set can never be incomplete and the symbol
names can never collide.
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    print("Pillow is required:  pip install Pillow", file=sys.stderr)
    sys.exit(1)

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))

# The ladder, ascending.  Clock sizes are 15/35/52/70 and seconds sizes 10/15/21/28; the output
# order is ascending so a slot lookup is a table in dspfont.h.
SIZES = [10, 15, 21, 28, 35, 52, 70]

# The clock sizes a layout's size index picks from, and the seconds sizes it picks from.
# A style is rejected if any of the seven is missing, so both lists always resolve.
CLOCK_SIZES = [15, 35, 52, 70]
SECONDS_SIZES = [10, 15, 21, 28]

CHARS = list("0123456789:")
THRESHOLD = 32  # luminance above this = foreground (white on black)
GLOW_CHAR = "/"  # default slot the synthetic glow glyph is injected into — see --glow-char
GLOW_CHAR_MIN, GLOW_CHAR_MAX = 0x20, 0x3A  # the range the glyph table covers


# ---------------------------------------------------------------------------
# Glyph helpers
# ---------------------------------------------------------------------------

def bounding_box(pixels, x_start, x_end, height, threshold):
    """Return (top, bottom, left_rel, right_rel) of foreground pixels in cell."""
    top, bottom, left_rel, right_rel = height, -1, x_end - x_start, -1
    for y in range(height):
        for x in range(x_start, x_end):
            if pixels[x, y] > threshold:
                if y < top:        top       = y
                if y > bottom:     bottom    = y
                cx = x - x_start
                if cx < left_rel:  left_rel  = cx
                if cx > right_rel: right_rel = cx
    return top, bottom, left_rel, right_rel


def pack_glyph(pixels, x_start, y_top, y_bottom, x_left_rel, x_right_rel, threshold):
    """Pack glyph pixels MSB-first, continuous bit stream (no per-row padding)."""
    data = []
    byte, count = 0, 0
    for y in range(y_top, y_bottom + 1):
        for x in range(x_start + x_left_rel, x_start + x_right_rel + 1):
            byte = (byte << 1) | (1 if pixels[x, y] > threshold else 0)
            count += 1
            if count == 8:
                data.append(byte)
                byte, count = 0, 0
    if count:
        data.append(byte << (8 - count))  # final partial byte, zero-padded
    return data


def build_digit_union_glow(pixels, digit_width, height, threshold, tight_bounds):
    """Build the glow glyph as the union of lit pixels across digits 0-9 (excluding the colon)."""
    mask = [[0] * digit_width for _ in range(height)]

    for d in range(10):
        x_start = d * digit_width
        for y in range(height):
            row = mask[y]
            for xr in range(digit_width):
                if pixels[x_start + xr, y] > threshold:
                    row[xr] = 1

    top, bottom, left, right = height, -1, digit_width, -1
    for y in range(height):
        for x in range(digit_width):
            if mask[y][x]:
                if y < top:      top    = y
                if y > bottom:   bottom = y
                if x < left:     left   = x
                if x > right:    right  = x

    if bottom < 0:
        return dict(data=[], width=0, height=0, xOffset=0, yOffset=0)

    if not tight_bounds:
        left = 0
        right = digit_width - 1
        top = 0
        bottom = height - 1

    data = []
    byte, count = 0, 0
    for y in range(top, bottom + 1):
        for x in range(left, right + 1):
            byte = (byte << 1) | (1 if mask[y][x] else 0)
            count += 1
            if count == 8:
                data.append(byte)
                byte, count = 0, 0
    if count:
        data.append(byte << (8 - count))

    return dict(
        data=data,
        width=(right - left + 1),
        height=(bottom - top + 1),
        xOffset=left,
        yOffset=-(bottom - top + 1),
    )


# ---------------------------------------------------------------------------
# One PNG -> the three C chunks for that size
# ---------------------------------------------------------------------------

def convert(png_path, prefix, threshold, tight_bounds, log, glow_char, expect_h):
    """Return (bitmaps_lines, glyphs_lines, font_line) for one PNG, or raise ValueError."""
    img = Image.open(png_path).convert("L")
    W, H = img.size
    pixels = img.load()

    # --- Cell widths ---------------------------------------------------------
    if W % 21 == 0:
        dw = 2 * (W // 21)
    else:
        raise ValueError(f"{os.path.basename(png_path)}: width {W} is not divisible by 21 "
                         f"(10 digits x 2 units + 1 colon x 1 unit) — is it the right image?")

    cw = dw // 2  # colon cell width
    expected_w = 10 * dw + cw
    if W != expected_w:
        raise ValueError(f"{os.path.basename(png_path)}: width {W} != expected {expected_w} "
                         f"(10 x {dw} + {cw})")
    if H != expect_h:
        # A hard error, not a warning.  The filename IS the contract and each of the seven sizes is
        # its own drawing, so a height that disagrees with the name is a mis-saved or mis-named
        # export, never a design choice.  Built anyway, every glyph would sit on the wrong grid and
        # the layout would be asking for a size the font does not have.
        raise ValueError(f"{os.path.basename(png_path)}: the image is {H} px tall but the filename "
                         f"says {expect_h} px.  Every size is a separate drawing - re-export it at "
                         f"{expect_h} px, or give the file the name that matches its height.")
    log(f"    {W}x{H} px   digit {dw}   colon {cw}")

    cell_widths = [dw] * 10 + [cw]

    # --- Extract glyphs ------------------------------------------------------
    glyphs = []
    x = 0
    for ch, cell_w in zip(CHARS, cell_widths):
        top, bot, lrel, rrel = bounding_box(pixels, x, x + cell_w, H, threshold)
        if bot < 0:
            raise ValueError(f"{os.path.basename(png_path)}: no foreground pixels for '{ch}'")
        if tight_bounds:
            gw = rrel - lrel + 1
            gh = bot - top + 1
            data = pack_glyph(pixels, x, top, bot, lrel, rrel, threshold)
            yo = -gh
            xo = lrel
        else:
            gw = cell_w
            gh = H
            data = pack_glyph(pixels, x, 0, H - 1, 0, cell_w - 1, threshold)
            yo = -H
            xo = 0
        glyphs.append(dict(char=ch, data=data, width=gw, height=gh,
                           xAdvance=cell_w, xOffset=xo, yOffset=yo))
        x += cell_w

    # --- Flat bitmap + per-glyph offsets -------------------------------------
    flat_bmp = []
    offsets = []
    for g in glyphs:
        offsets.append(len(flat_bmp))
        flat_bmp.extend(g["data"])

    # --- The synthetic glow glyph -------------------------------------------
    glow_code = ord(glow_char)
    glow_glyph = build_digit_union_glow(pixels, dw, H, threshold, tight_bounds)
    if glow_glyph["width"] == 0 or glow_glyph["height"] == 0:
        raise ValueError(f"{os.path.basename(png_path)}: glow mask is empty")
    glow_entry = {
        "offset": len(flat_bmp),
        "width": glow_glyph["width"],
        "height": glow_glyph["height"],
        "xAdvance": dw,
        "xOffset": glow_glyph["xOffset"],
        "yOffset": glow_glyph["yOffset"],
    }
    flat_bmp.extend(glow_glyph["data"])
    log(f"    glow '{glow_char}' -> {glow_entry['width']}x{glow_entry['height']} digit-union")

    y_advance = H + max(1, H // 8)  # line height with a small gap

    # --- Bitmap array --------------------------------------------------------
    out = [f"extern const uint8_t {prefix}Bitmaps[] = {{"]
    for i in range(0, len(flat_bmp), 12):
        chunk = flat_bmp[i:i + 12]
        out.append("  " + ", ".join(f"0x{b:02X}" for b in chunk) + ",")
    if out[-1].endswith(","):
        out[-1] = out[-1][:-1]
    out.append("};")

    bitmaps = out

    # --- Glyph table: 0x20 (space) .. 0x3A (':') -----------------------------
    glyph_by_code = {
        0x30 + i: {
            "offset": offsets[i],
            "width": glyphs[i]["width"],
            "height": glyphs[i]["height"],
            "xAdvance": glyphs[i]["xAdvance"],
            "xOffset": glyphs[i]["xOffset"],
            "yOffset": glyphs[i]["yOffset"],
        }
        for i in range(10)
    }
    glyph_by_code[0x3A] = {
        "offset": offsets[10],
        "width": glyphs[10]["width"],
        "height": glyphs[10]["height"],
        "xAdvance": glyphs[10]["xAdvance"],
        "xOffset": glyphs[10]["xOffset"],
        "yOffset": glyphs[10]["yOffset"],
    }
    glyph_by_code[glow_code] = glow_entry

    out = [f"extern const GFXglyph {prefix}Glyphs[] = {{"]
    for code in range(0x20, 0x3B):
        is_last = code == 0x3A
        suffix = "" if is_last else ","
        comment = f"   // 0x{code:02X} '{chr(code)}'"
        if code == glow_code:
            comment += "  (glow)"
        if code in glyph_by_code:
            g = glyph_by_code[code]
            out.append(
                f"  {{ {g['offset']:6d}, {g['width']:3d}, {g['height']:3d}, {g['xAdvance']:3d}, "
                f"{g['xOffset']:4d}, {g['yOffset']:4d} }}{suffix}{comment}"
            )
        elif code == 0x20:
            # Space gets a non-zero xAdvance so text layout stays predictable.
            out.append(f"  {{     0,   0,   0, {cw:3d},    0,    1 }}{suffix}{comment}")
        else:
            out.append(f"  {{     0,   0,   0,   0,    0,    0 }}{suffix}{comment}")
    out.append("};")

    glyphs_out = out

    font = f"extern const GFXfont {prefix} = {{\n" \
           f"  (uint8_t  *){prefix}Bitmaps,\n" \
           f"  (GFXglyph *){prefix}Glyphs, 0x20, 0x3A, {y_advance} }};"

    return bitmaps, glyphs_out, font


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build clockfonts/<Style>.h from clockfonts/<Style>/{8,15,21,28,35,52,70}.png")
    parser.add_argument("style", help="Style folder name, e.g. LED, Chunky6, Chunky6px")
    parser.add_argument("--threshold", type=int, default=THRESHOLD,
                        help="Foreground threshold 0..255 (lower preserves anti-aliased edges).")
    parser.add_argument("--glow-char", type=str, default=GLOW_CHAR,
                        help="The one character whose glyph slot the synthetic glow glyph is "
                             "injected into (0x20..0x3A).  The generated header names this "
                             "character; the widget builds its glow strings from it.")
    parser.add_argument("--tight-bounds", action="store_true",
                        help="Trim each glyph to its ink. Default keeps fixed cell metrics, which "
                             "is what keeps every style of a size on one ink grid.")
    args = parser.parse_args()

    # The glow character has to be a slot the glyph table actually covers, or the union glyph would
    # be emitted at a code point no lookup would ever reach.
    if len(args.glow_char) != 1 or not (GLOW_CHAR_MIN <= ord(args.glow_char) <= GLOW_CHAR_MAX):
        print(f"Error: --glow-char must be exactly one character in "
              f"0x{GLOW_CHAR_MIN:02X}..0x{GLOW_CHAR_MAX:02X}.", file=sys.stderr)
        sys.exit(2)

    style = args.style
    prefix_ok = all(c.isalnum() or c == "_" for c in style) and not style[0].isdigit()
    if not prefix_ok:
        print(f"Error: '{style}' is not usable as a C symbol prefix "
              f"(letters, digits and underscores, not starting with a digit).", file=sys.stderr)
        sys.exit(2)

    style_dir = os.path.join(SCRIPT_DIR, style)
    if not os.path.isdir(style_dir):
        print(f"Error: {os.path.relpath(style_dir).replace(os.sep, '/')} does not exist. "
              f"Create it and add {{10,15,21,28,35,52,70}}.png.", file=sys.stderr)
        sys.exit(2)

    out_path = os.path.join(SCRIPT_DIR, style + ".h")

    print(f"makefont.py {style}")
    print(f"  reading {os.path.relpath(style_dir).replace(os.sep, '/')}")

    # --- The all-or-nothing check, before anything is written ---------------
    missing = [s for s in SIZES if not os.path.isfile(os.path.join(style_dir, f"{s}.png"))]
    if missing:
        print(f"Error: '{style}' is incomplete — a style is all seven sizes or it is not a style.",
              file=sys.stderr)
        for s in missing:
            print(f"  missing: clockfonts/{style}/{s}.png", file=sys.stderr)
        print(f"  draw them from clockfonts/templates/Template_<size>.png "
              f"and run this again.", file=sys.stderr)
        sys.exit(1)

    # --- Convert every size --------------------------------------------------
    def log(msg):
        print(msg)

    sections = []
    try:
        for size in SIZES:
            log(f"  {size}.png")
            bitmaps, glyphs, font = convert(os.path.join(style_dir, f"{size}.png"),
                                            f"{style}_{size}", args.threshold,
                                            args.tight_bounds, log, args.glow_char, size)
            sections.append((size, bitmaps, glyphs, font))
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # --- Assemble the header -------------------------------------------------
    out = []
    out.append("// Generated by makefont.py %s — do not edit by hand." % style)
    out.append("//")
    out.append("// The seven sizes of the %s clock font, ascending (8, 15, 21, 28, 35, 52, 70), and" % style)
    out.append("// this style's glow character.  Included by clockfonts.cpp and nothing else: a")
    out.append("// namespace-scope const has internal linkage, so including this from dspfont.h would put")
    out.append("// every one of these arrays in flash once per translation unit that reaches it.")
    out.append("#pragma once")
    out.append("")
    out.append('#include "fontstyle.h"')
    out.append("")
    for size, bitmaps, glyphs, font in sections:
        out.append(f"// ---- {size} px " + "-" * (68 - len(str(size))))
        out.extend(bitmaps)
        out.append("")
        out.extend(glyphs)
        out.append("")
        out.append(font)
        out.append("")
    out.append("// ---- glow character ---------------------------------------------------")
    out.append("// The ONE character this style draws its glow with - the slot the union glyph above")
    out.append("// was injected into.  The shapes drawn with it, the clock's <g><g>:<g><g> and the")
    out.append("// seconds' <g><g>, belong to the widget: the clock's is also its width template.")
    out.append(f'extern const char Glow_{style}[] = "{args.glow_char}";')
    out.append("")
    sizes_ptrs = ", ".join(f"&{style}_{s}" for s in SIZES)
    out.append(f"extern const ClockFontStyle {style}Style = {{")
    out.append(f'  "{style}",')
    out.append(f"  {{ {sizes_ptrs} }},")
    out.append(f"  Glow_{style}")
    out.append("};")
    out.append("")

    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(out))

    # --- Summary -------------------------------------------------------------
    total = 0
    for size, bitmaps, glyphs, font in sections:
        # bitmap data bytes = hex literals on the non-'{' lines of the array
        data_lines = [l for l in bitmaps[1:-1]]
        nbytes = sum(l.count("0x") for l in data_lines)
        total += nbytes
        print(f"    {size:>2} px -> {style}_{size:<3} ({nbytes} bitmap bytes)")
    print(f"  glow char    : '{args.glow_char}'")
    print(f"  clock sizes  : {CLOCK_SIZES}")
    print(f"  seconds sizes: {SECONDS_SIZES}")
    print(f"  bitmaps total: {total} bytes")
    print(f"Written: clockfonts/{style}.h")


if __name__ == "__main__":
    main()
