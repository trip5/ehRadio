#!/usr/bin/env python3
"""
makefont.py — build one clock font STYLE from its folder of PNGs.

    py makefont.py LEDClock
    py makefont.py Chunky6px
    py makefont.py NixieTube --glow-out
    py makefont.py NixieTube --glow-out 70

Input:   clockfonts/<Style>/{10,15,21,28,35,52,70}.png
         (+ optional <size>glow.png and <size>glow_char.png, see THE GLOW IMAGE below)
Output:  clockfonts/<Style>.h          (or, with --glow-out, <size>glow_char.png and nothing else)

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

Every glyph is packed at its FULL CELL — digit cell width x image height, colon cell width x image
height, xOffset 0, yOffset -height.  Nothing is trimmed to its ink, which is what puts every glyph of
a size on one grid: the artwork's own margins and its position inside the cell survive into the font,
and every style of a size measures the same.  There is deliberately no trim-to-ink mode.

Every symbol is unique per style and size (LED_35, Chunky6px_35, ...) and every definition is
`extern const` so it has external linkage: the whole point is that the three styles can all be
compiled at once and switched at runtime, and that a namespace-scope const does not end up in
flash once per translation unit that reaches dspfont.h.  Only clockfonts.cpp includes the
generated headers; dspfont.h only declares the ClockFontStyle instances.

The glow CHARACTER is emitted here, and only the character: this is the tool that chose the glyph
slot, so it is the only thing that knows which one to name.  The two shapes the widget draws with it
— the clock's <g><g>:<g><g> and the seconds' <g><g> — are built in the widget, because the clock's
shape is also its WIDTH TEMPLATE (see the clock-widget code).  The slot is always "/" — the one the
codebase expects — and is not configurable.

THE GLOW IMAGE.  The glow glyph is the union of the lit pixels across the ten digits: the residual
"LED" shapes that sit behind the time.  --glow-out writes that union to <Style>/<size>glow_char.png,
one file per size, white on black on the size's own digit cell (digit width x image height) — and
writes NOTHING else; the .h is not generated in that mode, so the images can be tuned before the font
is rebuilt.  It resolves the union the same way a build does, so when <size>glow.png is present the
union is taken from that strip; the only thing it never reads is <size>glow_char.png, which is what
it is writing.  A bare --glow-out does all seven sizes and '--glow-out 70' does that one, and an
existing image is only overwritten after a y at its prompt (end of input counts as no), because this
tool never destroys a drawing silently.

An ordinary run then resolves each size's glow in this order.  The files are the entire switch - there
is no flag for any of it:

  1. <size>glow.png — an alternative drawing of the SAME ten digits on the same canvas as
     <size>.png.  The union is taken from IT, which is how a deliberately exaggerated or reshaped
     glow gets drawn without touching the font's own artwork.
  2. <size>glow_char.png — the finished glow glyph, one digit cell, used exactly as drawn.
  3. neither — the union is computed from <size>.png itself.

Every step is decided per size, so one style can use all three at once.  A file of the wrong size, or
a blank one, is a hard error naming it, exactly like a mis-sized digit PNG.

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

# The one character the synthetic glow glyph is injected into.  Always "/" — the codebase's widget
# builds its <g><g>:<g><g> from whatever the style names, and that is this character.  It has to be
# inside the glyph table's range or no lookup would ever reach the union glyph.
GLOW_CHAR = "/"
GLYPH_FIRST, GLYPH_LAST = 0x20, 0x3A  # the range the glyph table covers

# The two glow files an ordinary run looks for, listed in PRECEDENCE order.  <size>glow.png is an
# alternative drawing of the same ten digits, so the union is taken from it; <size>glow_char.png is
# the finished glow glyph and is used as it is.  --glow-out writes the second one.
GLOW_STRIP_SUFFIX = "glow"
GLOW_CHAR_SUFFIX = "glow_char"


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


def pack_bits(bits):
    """Pack an iterable of 0/1 MSB-first, continuous bit stream (no per-row padding)."""
    data = []
    byte, count = 0, 0
    for bit in bits:
        byte = (byte << 1) | (1 if bit else 0)
        count += 1
        if count == 8:
            data.append(byte)
            byte, count = 0, 0
    if count:
        data.append(byte << (8 - count))  # final partial byte, zero-padded
    return data


def pack_glyph(pixels, x_start, y_top, y_bottom, x_left_rel, x_right_rel, threshold):
    """Pack glyph pixels MSB-first, continuous bit stream (no per-row padding)."""
    def bits():
        for y in range(y_top, y_bottom + 1):
            for x in range(x_start + x_left_rel, x_start + x_right_rel + 1):
                yield 1 if pixels[x, y] > threshold else 0
    return pack_bits(bits())


def digit_union_mask(pixels, digit_width, height, threshold):
    """Return the union of the lit pixels across digits 0-9 (the colon is not part of it).

    A list of `height` rows of `digit_width` 0/1 values, on the digit cell's own coordinates.  This is
    the glow artwork AND what --glow-out writes, so the computed glyph and the written image can
    never disagree.
    """
    mask = [[0] * digit_width for _ in range(height)]
    for d in range(10):
        x_start = d * digit_width
        for y in range(height):
            row = mask[y]
            for xr in range(digit_width):
                if pixels[x_start + xr, y] > threshold:
                    row[xr] = 1
    return mask


def mask_has_ink(mask):
    return any(any(row) for row in mask)


def glow_char_name(size):
    """The finished glow glyph --glow-out writes, e.g. 10glow_char.png."""
    return f"{size}{GLOW_CHAR_SUFFIX}.png"


def glow_strip_name(size):
    """The alternative digit strip whose union can stand in for the glow, e.g. 10glow.png."""
    return f"{size}{GLOW_STRIP_SUFFIX}.png"


def find_named(style_dir, name):
    """Return the path of `name` in style_dir, or None.  Case-insensitive, because .PNG happens."""
    path = os.path.join(style_dir, name)
    if os.path.isfile(path):
        return path
    # Windows would have matched above already; the scan keeps the tool honest on filesystems that
    # care about the case (a hand-exported 10glow.PNG).
    try:
        for entry in sorted(os.listdir(style_dir)):
            if entry.lower() == name:
                return os.path.join(style_dir, entry)
    except OSError:
        pass
    return None


def find_glow_input(style_dir, size):
    """Return (path, kind) for this size's glow source, or (None, None).

    The order below IS the precedence: a glow strip outranks a finished glow glyph, which outranks
    the union computed from <size>.png.  The files are the whole switch - there is no flag.
    """
    for suffix, kind in ((GLOW_STRIP_SUFFIX, "strip"), (GLOW_CHAR_SUFFIX, "char")):
        path = find_named(style_dir, f"{size}{suffix}.png")
        if path:
            return path, kind
    return None, None


def write_glow_png(mask, path, digit_width, height):
    """Write a glow mask as 8-bit greyscale, white ink on black, at the digit cell's size."""
    img = Image.new("L", (digit_width, height), 0)
    px = img.load()
    for y in range(height):
        row = mask[y]
        for x in range(digit_width):
            if row[x]:
                px[x, y] = 255
    img.save(path)


def read_glow_char_mask(png_path, digit_width, height, threshold):
    """Read a <size>glow_char.png into a mask: one cell, so it must be the digit cell's size."""
    name = os.path.basename(png_path)
    img = Image.open(png_path).convert("L")
    gw, gh = img.size
    if (gw, gh) != (digit_width, height):
        raise ValueError(f"{name}: the glow glyph is {gw}x{gh} but this size's digit cell is "
                         f"{digit_width}x{height}.  A <size>glow_char.png is ONE cell, the same "
                         f"canvas as a digit in <size>.png - re-export it at {digit_width}x{height}, "
                         f"or delete it and let makefont.py compute the digit union instead.")
    px = img.load()
    mask = [[1 if px[x, y] > threshold else 0 for x in range(gw)] for y in range(gh)]
    if not mask_has_ink(mask):
        raise ValueError(f"{name}: no foreground pixels - a glow glyph must be white on black.  "
                         f"Fix it, or delete it to have makefont.py compute the digit union instead.")
    return mask


def read_glow_strip_mask(png_path, digit_width, height, threshold):
    """Read a <size>glow.png strip and return the union across its digits.

    It is an alternative drawing of the SAME ten digits, so it has to be on the same canvas as
    <size>.png: that is the only way its union can land on the digit cell of the font being built.
    """
    name = os.path.basename(png_path)
    img = Image.open(png_path).convert("L")
    gw, gh = img.size
    cell_w = digit_width // 2
    want_w = 10 * digit_width + cell_w
    if (gw, gh) != (want_w, height):
        raise ValueError(f"{name}: the glow strip is {gw}x{gh} but this size's digit strip is "
                         f"{want_w}x{height} (10 x {digit_width} + {cell_w}).  A <size>glow.png is an "
                         f"alternative drawing of the same ten digits on the SAME canvas as "
                         f"<size>.png - re-export it at {want_w}x{height}, or delete it and let "
                         f"makefont.py take the union from <size>.png itself.")
    px = img.load()
    mask = digit_union_mask(px, digit_width, height, threshold)
    if not mask_has_ink(mask):
        raise ValueError(f"{name}: the digit union is empty - a glow strip must be white on black.  "
                         f"Fix it, or delete it to have makefont.py take the union from <size>.png.")
    return mask


def confirm_overwrite(path):
    """Ask before clobbering an existing file.  Anything but y - and end of input - means skip."""
    try:
        answer = input(f"  {os.path.basename(path)} exists - overwrite? [y/N] ")
    except EOFError:
        return False
    return answer.strip().lower() == "y"


# ---------------------------------------------------------------------------
# One PNG -> the three C chunks for that size
# ---------------------------------------------------------------------------

def read_cells(png_path, expect_h):
    """Open one size PNG, validate its cell layout, return (pixels, W, H, dw, cw)."""
    img = Image.open(png_path).convert("L")
    W, H = img.size
    name = os.path.basename(png_path)

    if W % 21 == 0:
        dw = 2 * (W // 21)
    else:
        raise ValueError(f"{name}: width {W} is not divisible by 21 "
                         f"(10 digits x 2 units + 1 colon x 1 unit) — is it the right image?")

    cw = dw // 2  # colon cell width
    expected_w = 10 * dw + cw
    if W != expected_w:
        raise ValueError(f"{name}: width {W} != expected {expected_w} "
                         f"(10 x {dw} + {cw})")
    if H != expect_h:
        # A hard error, not a warning.  The filename IS the contract and each of the seven sizes is
        # its own drawing, so a height that disagrees with the name is a mis-saved or mis-named
        # export, never a design choice.  Built anyway, every glyph would sit on the wrong grid and
        # the layout would be asking for a size the font does not have.
        raise ValueError(f"{name}: the image is {H} px tall but the filename "
                         f"says {expect_h} px.  Every size is a separate drawing - re-export it at "
                         f"{expect_h} px, or give the file the name that matches its height.")
    return img.load(), W, H, dw, cw


def convert(png_path, prefix, threshold, log, glow_path, glow_kind, expect_h):
    """Return (bitmaps_lines, glyphs_lines, font_line) for one PNG, or raise ValueError."""
    pixels, W, H, dw, cw = read_cells(png_path, expect_h)
    log(f"    {W}x{H} px   digit {dw}   colon {cw}")

    cell_widths = [dw] * 10 + [cw]

    # --- Extract glyphs, each at its full cell ---------------------------------
    glyphs = []
    x = 0
    for ch, cell_w in zip(CHARS, cell_widths):
        top, bot, lrel, rrel = bounding_box(pixels, x, x + cell_w, H, threshold)
        if bot < 0:
            raise ValueError(f"{os.path.basename(png_path)}: no foreground pixels for '{ch}'")
        # Full cell, always: width/height are the CELL, xOffset 0, yOffset -height.  Trimming to the
        # ink would make the eleven boxes differ and break the one-grid-per-size rule.
        glyphs.append(dict(char=ch, data=pack_glyph(pixels, x, 0, H - 1, 0, cell_w - 1, threshold),
                           width=cell_w, height=H, xAdvance=cell_w, xOffset=0, yOffset=-H))
        x += cell_w

    # --- Flat bitmap + per-glyph offsets -------------------------------------
    flat_bmp = []
    offsets = []
    for g in glyphs:
        offsets.append(len(flat_bmp))
        flat_bmp.extend(g["data"])

    # --- The glow glyph: a glow strip, else a finished glow glyph, else the digit union --
    # find_glow_input() has already decided which of the three this size gets, so all that is left
    # here is reading that source.  There is no fourth way to reach the union.
    glow_code = ord(GLOW_CHAR)
    if glow_kind == "strip":
        mask = read_glow_strip_mask(glow_path, dw, H, threshold)
        log(f"    glow input: {os.path.basename(glow_path)} -> {dw}x{H} digit-union")
    elif glow_kind == "char":
        mask = read_glow_char_mask(glow_path, dw, H, threshold)
        log(f"    glow input: {os.path.basename(glow_path)} -> {dw}x{H}")
    else:
        mask = digit_union_mask(pixels, dw, H, threshold)
        if not mask_has_ink(mask):
            raise ValueError(f"{os.path.basename(png_path)}: glow mask is empty")
        log(f"    glow '{GLOW_CHAR}' -> {dw}x{H} digit-union")

    # Same cell as a digit, because the glow fills the cells the digits sit in and this glyph's
    # xAdvance is what makes the widget's <g><g>:<g><g> width template measure like "00:00".
    glow_entry = {
        "offset": len(flat_bmp),
        "width": dw,
        "height": H,
        "xAdvance": dw,
        "xOffset": 0,
        "yOffset": -H,
    }
    flat_bmp.extend(pack_bits(bit for row in mask for bit in row))

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
    for code in range(GLYPH_FIRST, GLYPH_LAST + 1):
        is_last = code == GLYPH_LAST
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
           f"  (GFXglyph *){prefix}Glyphs, 0x{GLYPH_FIRST:02X}, 0x{GLYPH_LAST:02X}, {y_advance} }};"

    return bitmaps, glyphs_out, font


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Build clockfonts/<Style>.h from "
                    "clockfonts/<Style>/{10,15,21,28,35,52,70}.png, or write the editable glow "
                    "images with --glow-out.")
    parser.add_argument("style", help="Style folder name, e.g. LED, Chunky6, Chunky6px")
    parser.add_argument("--threshold", type=int, default=THRESHOLD,
                        help="Foreground threshold 0..255 (lower preserves anti-aliased edges).")
    parser.add_argument("--glow-out", nargs="?", const=0, type=int, metavar="SIZE",
                        help="Write <Style>/<size>glow_char.png and exit WITHOUT building "
                             "<Style>.h.  No SIZE means all seven sizes; '--glow-out 70' does that "
                             "size alone.  An existing image is overwritten only if you answer y at "
                             "its prompt.")
    args = parser.parse_args()

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

    # --glow-out takes an optional size: a bare flag means all seven, a number means that one.
    if args.glow_out is not None and args.glow_out != 0 and args.glow_out not in SIZES:
        print(f"Error: --glow-out {args.glow_out}: no such size.  Use one of "
              f"{', '.join(str(s) for s in SIZES)}, or no size at all for every size.",
              file=sys.stderr)
        sys.exit(2)

    out_path = os.path.join(SCRIPT_DIR, style + ".h")

    print(f"makefont.py {style}")
    print(f"  reading {os.path.relpath(style_dir).replace(os.sep, '/')}")

    # --- The all-or-nothing check, before anything is written ---------------
    # Building the .h is all seven sizes or nothing.  --glow-out is not a build - it writes no header
    # and reads only the sizes it was asked for, so '--glow-out 70' needs 70.png and nothing else.
    targets = SIZES if args.glow_out in (None, 0) else [args.glow_out]
    missing = [s for s in targets if not os.path.isfile(os.path.join(style_dir, f"{s}.png"))]
    if missing:
        if args.glow_out is None:
            print(f"Error: '{style}' is incomplete — a style is all seven sizes or it is not a style.",
                  file=sys.stderr)
        else:
            print(f"Error: nothing to take the glow from — this style is missing:", file=sys.stderr)
        for s in missing:
            print(f"  missing: clockfonts/{style}/{s}.png", file=sys.stderr)
        if args.glow_out is None:
            print(f"  draw them from clockfonts/templates/Template_<size>.png "
                  f"and run this again.", file=sys.stderr)
        sys.exit(1)

    def log(msg):
        print(msg)

    # --- --glow-out: write the glow glyphs and stop --------------------------
    # The union is resolved exactly as a build resolves it, so what is written here is what the NEXT
    # build would use: <size>glow.png when that strip is present (it wins in a build, so it has to win
    # here or the file would be born out of date), <size>.png otherwise.  <size>glow_char.png is the
    # one thing never read - it is the target.  Every size read is validated as the .h build does it,
    # the .h is deliberately NOT written, and nothing existing is clobbered without a y at its prompt.
    if args.glow_out is not None:
        written, skipped = 0, 0
        try:
            for size in targets:
                log(f"  {size}.png")
                target = os.path.join(style_dir, glow_char_name(size))
                if os.path.isfile(target) and not confirm_overwrite(target):
                    log(f"    output: {glow_char_name(size)} - skipped, existing file kept")
                    skipped += 1
                    continue
                pixels, W, H, dw, cw = read_cells(os.path.join(style_dir, f"{size}.png"), size)
                log(f"    {W}x{H} px   digit {dw}   colon {cw}")
                strip = find_named(style_dir, glow_strip_name(size))
                if strip:
                    mask = read_glow_strip_mask(strip, dw, H, args.threshold)
                    log(f"    glow input: {os.path.basename(strip)} -> {dw}x{H} digit-union")
                else:
                    mask = digit_union_mask(pixels, dw, H, args.threshold)
                    if not mask_has_ink(mask):
                        raise ValueError(f"{size}.png: glow mask is empty")
                    log(f"    glow '{GLOW_CHAR}' -> {dw}x{H} digit-union")
                write_glow_png(mask, target, dw, H)
                log(f"    output: {glow_char_name(size)}")
                written += 1
        except ValueError as e:
            print(f"Error: {e}", file=sys.stderr)
            sys.exit(1)
        if written:
            print(f"  wrote {written} glow image{'s' if written != 1 else ''} "
                  f"-> clockfonts/{style}/")
        if skipped:
            print(f"  skipped {skipped} existing image{'s' if skipped != 1 else ''}")
        print("(no .h written: --glow-out only produces the glow images)")
        return

    # --- Convert every size --------------------------------------------------
    sections = []
    try:
        for size in SIZES:
            log(f"  {size}.png")
            glow_path, glow_kind = find_glow_input(style_dir, size)
            bitmaps, glyphs, font = convert(os.path.join(style_dir, f"{size}.png"),
                                            f"{style}_{size}", args.threshold, log,
                                            glow_path, glow_kind, size)
            sections.append((size, bitmaps, glyphs, font))
    except ValueError as e:
        print(f"Error: {e}", file=sys.stderr)
        sys.exit(1)

    # --- Assemble the header -------------------------------------------------
    out = []
    out.append("// Generated by makefont.py %s — do not edit by hand." % style)
    out.append("//")
    out.append("// The seven sizes of the %s clock font, ascending (10, 15, 21, 28, 35, 52, 70), and" % style)
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
    out.append(f'extern const char Glow_{style}[] = "{GLOW_CHAR}";')
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
    print(f"  glow char    : '{GLOW_CHAR}'")
    print(f"  clock sizes  : {CLOCK_SIZES}")
    print(f"  seconds sizes: {SECONDS_SIZES}")
    print(f"  bitmaps total: {total} bytes")
    print(f"Written: clockfonts/{style}.h")


if __name__ == "__main__":
    main()
