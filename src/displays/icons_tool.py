#!/usr/bin/env python3
"""
icons_tool.py - one tool for the icon marks and their scale variants in icons.h.

USAGE:
    py icons_tool.py                        # walk every mark, ask about each mismatch
    py icons_tool.py --check                # report only, nothing written
    py icons_tool.py -b [NAME]              # the comment is the drawing: rebuild the binary
    py icons_tool.py -c [NAME]              # the binary is the drawing: regenerate the comment
    py icons_tool.py -s NAME N [--force]    # seed NAME_N from the largest divisor present
    py icons_tool.py -o LEFT RIGHT [FILE]   # two marks side by side on one sheet
    py icons_tool.py -i [FILE]              # read that sheet back into icons.h
    py icons_tool.py --list                 # which marks have which scales
    py icons_tool.py --stats                # flash cost per scale
    py icons_tool.py -h | --help

WHAT THE FILE HOLDS:
    scale 1          static const uint8_t  NAME[]    PROGMEM   8 rows of 6 bits
    scale N 2..5     static const uint32_t NAME_N[]  PROGMEM   8*N rows of 6*N bits

    A row is a binary literal with a pixel comment beside it (// @@....): `@` is a
    lit pixel, `.` is a dark one, leftmost character = leftmost pixel.  The
    comment is what keeps a hand-tuned row readable, and the preprocessor strips
    it, so it costs no flash.  The literal's base cannot reach the firmware
    either - 0b1100, 0x0C and 12 are the same folded constant - so there is no
    inflate/deflate mode: one format, always binary, always with the comment.

WHERE THE PIXELS LIVE:
    icons.h, beside this script.  An `icons.h` in the CURRENT FOLDER wins, so
    `cd src/displays && py icons_tool.py` works on the file you are standing in,
    and running it by path from the repo root reaches the same header through the
    script's own folder.  The header is committed, like pretext_fold.h; this is a
    maintenance tool and no part of the build.

MODES:
    (default)               Walk the marks that have a mismatch, print each one's
                            --check lines, and ask once per mark: keep the binary,
                            keep the comment, or skip.  Skip is the default, and
                            it is the only mode that settles a MIXED icon - one
                            whose row 3 value is wrong and whose row 4 comment is.
                            With piped input it degrades to --check, so nothing
                            can hang on a prompt.
    -o, --stitch-out LEFT RIGHT [FILE]
                            Two marks on ONE sheet, side by side with a single space
                            between the columns, at every scale BOTH have.  A scale
                            only one of them has is reported instead of written, so
                            the sheet can never carry art for a variant that does not
                            exist.  Default FILE: icon_stitch.txt beside icons.h.
    -i, --stitch-in [FILE]  The reverse.  Every row is split on its one space and
                            checked first: two segments, each exactly 6*N wide for
                            that group's scale, `@` and `.` only, 8*N rows, names that
                            exist at the same scale, and columns matching the
                            codepoints in the header.  Anything wrong is listed
                            against the SHEET's own line number and nothing is
                            written.  A named array is then replaced in place, or
                            created after its base's last variant, and the tables are
                            regenerated.
    --check                 Validate and write nothing.  Exits 1 on any problem.
    -b, --fix-binary [NAME] The COMMENT is the drawing: rebuild the row values from
                            it.  A row with no comment, a comment not exactly the
                            row's 6*N characters wide, or one holding anything but
                            `@` and `.`, cannot be read that way - it is reported
                            and skipped, because the tool cannot invent a drawing.
                            With NAME, that mark only, and nothing is asked.
    -c, --fix-comment [NAME] The BINARY is the drawing: regenerate the comment from
                            the value, which is always possible.  With NAME, that
                            mark only, and nothing is asked.
                            Without NAME, -b and -c walk the mismatched marks and
                            ask [y/N] once per mark, default No.
    -s NAME N               Seed NAME_N from the largest existing divisor of N, else
                            from scale 1, by nearest-neighbour expansion.  Nothing
                            is generated unless it is named: there is no bulk pass
                            and no mark gets a size it was not asked for.  An
                            existing NAME_N is left exactly as found - that is what
                            makes hand-tuning survive - and --force is the
                            deliberate exception.
    --list                  Every mark, its codepoint, and the scales it has.
    --stats                 Flash cost per scale, plus the pointer tables.

    No mode but --check, -b and -c ever changes a pixel, and nothing normalises
    silently: whatever the tool writes goes through the same writer, which owns the
    declaration's type, the row count and the trailing comment.

THE LADDER (what the renderer will actually draw):
    For a requested textsize N the renderer takes the largest existing scale
    k <= min(N, 5) that DIVIDES N - the request itself first, so a mark drawn for
    that size is drawn 1:1 - and otherwise falls back to scale 1 blown up by
    ps = N.  ps has to be an integer, so a scale-3 mark is ignored at textsize 4
    rather than padded into a 24 px cell.  6*N has to fit the row type, so N <= 5;
    the tool refuses 6 and above with that reason rather than emitting something
    the renderer cannot read.

EXAMPLES:
    py icons_tool.py --list
    py icons_tool.py --check
    py icons_tool.py -c MUTE
    py icons_tool.py -b MUTE_2
    py icons_tool.py -s PAUSE 2
    py icons_tool.py -s PAUSE 4
    py icons_tool.py --stats
"""

import argparse
import os
import re
import sys
from pathlib import Path

# This terminal is not always UTF-8 and the comments carry box drawing and arrows.
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

# The header lives beside this script.  An `icons.h` in the current folder wins, so
# the tool works on the file you are standing in; the script's own folder is the
# fallback, which is what makes the path form work from the repo root.
ICONS_H = Path('icons.h') if Path('icons.h').is_file() else Path(__file__).with_name('icons.h')

MAX_SCALE = 5                  # 6*5 = 30 bits still fits a uint32_t row
SLOTS = 32                     # codepoints 0..31
BASE_COLS, BASE_ROWS = 6, 8    # the glyph cell: CHARWIDTH / CHARHEIGHT
PTR_BYTES = 4                  # 32-bit target: one table entry is a pointer
LIT = '@'                      # the lit pixel in a comment; -b reads what -c emits

#: Notes the generated tables carry.  Hand-written, so the writer preserves them.
SLOT_NOTES = {
    0: ' (unused, should probably stay unused)',
    9: ' (DO NOT USE: TAB control char conflicts with TFT print())',
    10: ' (DO NOT USE: LF control char conflicts with TFT print())',
}

DECL_RE = re.compile(r'^\s*static\s+const\s+(uint8_t|uint16_t|uint32_t)\s*'
                     r'(?:\*\s*const\s*)?(\w+)\s*\[\]\s*PROGMEM\s*=\s*\{\s*$')
ROW_RE = re.compile(r'^\s*(0b[01]+|0x[0-9A-Fa-f]+|\d+)\s*,?\s*(?://\s?(.*?))?\s*$')
END_RE = re.compile(r'^\s*\}\s*;\s*$')
ENTRY_RE = re.compile(r'^\s*(\w+)\s*,?\s*(?://\s*(.*?))?\s*$')
CP_RE = re.compile(r'\\([0-7]{3})')
SCALE_RE = re.compile(r'^(.*)_(\d+)$')
TABLE_RE = re.compile(r'^ICON_TABLE(?:_(\d))?$')

BEGIN_MARK = '// ===== BEGIN ICON_TABLES'
END_MARK = '// ===== END ICON_TABLES'


def fail(message):
    print(f'ERROR: {message}')
    sys.exit(1)


# ---------------------------------------------------------------------------
# Reading and writing
# ---------------------------------------------------------------------------

EOL = '\n'    # the header's own line endings, read once and reproduced on every write


def read_lines(path=ICONS_H):
    """The file's lines.  newline='' keeps its own line endings, which EOL then
    reproduces on every write."""
    global EOL
    if not Path(path).exists():
        fail(f'{path} not found.')
    with open(path, 'r', encoding='utf-8', newline='') as f:
        text = f.read()
    EOL = '\r\n' if '\r\n' in text else '\n'
    lines = text.split(EOL)
    # A trailing newline leaves an empty last element.  Keeping it made every write add
    # one more, so the file grew a blank line per run; one newline is put back on write.
    while lines and lines[-1] == '':
        lines.pop()
    return lines


def separate_arrays(lines):
    """One blank line between an array's closing `};` and whatever follows it.

    Applied on every write rather than only on insertion, so a seeded array and a
    hand-pasted one end up laid out the same, and a file that lost a blank line
    somewhere gets it back.  Blanks are only ever ADDED: the runs of two above the
    header block are the author's, and the generated tables already carry their own.
    """
    out = []
    for i, line in enumerate(lines):
        out.append(line)
        if line.strip() != '};' or i + 1 >= len(lines):
            continue
        nxt = lines[i + 1].strip()
        # Only a real neighbour: a `//` line or the next declaration.  A `};` inside a
        # commented-out block is followed by `*/`, and a blank there would sit inside the
        # comment; the generated block's last `};` is followed by the END marker.
        if (nxt.startswith('//') or nxt.startswith('static')) \
                and not nxt.startswith(END_MARK):
            out.append('')
    return out


def write_lines(lines, path=ICONS_H):
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(EOL.join(separate_arrays(lines)) + EOL)


# ---------------------------------------------------------------------------
# The model
# ---------------------------------------------------------------------------

class Row:
    def __init__(self, value, comment, lineno):
        self.value = value
        self.comment = comment          # None when the row carries no comment
        self.lineno = lineno            # 0-based; 0 for a row the tool invented


class Icon:
    """One PROGMEM array: a scale-1 mark or one of its variants."""

    def __init__(self, name, ctype, decl_line, rows, above):
        self.name = name
        self.ctype = ctype
        self.decl_line = decl_line      # 0-based index of the declaration
        self.rows = rows
        self.above = above              # the // block directly above it
        self.end_line = None            # 0-based index of the closing };

    @property
    def base(self):
        m = SCALE_RE.match(self.name)
        if m and 2 <= int(m.group(2)) <= MAX_SCALE:
            return m.group(1)
        return self.name

    @property
    def scale(self):
        return 1 if self.base == self.name else int(self.name.rsplit('_', 1)[1])

    @property
    def codepoint(self):
        for line in self.above:
            m = CP_RE.search(line)
            if m:
                return int(m.group(1), 8)
        return None

    @property
    def bits(self):
        return BASE_COLS * self.scale

    @property
    def rows_wanted(self):
        return BASE_ROWS * self.scale

    @property
    def want_type(self):
        return 'uint8_t' if self.scale == 1 else 'uint32_t'

    def where(self):
        return self.decl_line + 1


def parse(lines):
    """Live declarations only.  Anything inside a /* */ block is skipped, which is
    what keeps the retired IP[] and the BLANK template out of the model."""
    icons, in_comment, i = [], False, 0
    while i < len(lines):
        line = lines[i]
        if in_comment:
            if '*/' in line:
                in_comment = False
            i += 1
            continue
        if line.strip().startswith('/*'):
            in_comment = '*/' not in line
            i += 1
            continue
        m = DECL_RE.match(line)
        if not m:
            i += 1
            continue
        above, j = [], i - 1
        while j >= 0 and lines[j].strip().startswith('//'):
            above.insert(0, lines[j])
            j -= 1
        rows, k = [], i + 1
        while k < len(lines) and not END_RE.match(lines[k]):
            rm = ROW_RE.match(lines[k])
            if rm:
                rows.append(Row(int(rm.group(1).replace("'", ''), 0), rm.group(2), k))
            k += 1
        icon = Icon(m.group(2), m.group(1), i, rows, above)
        icon.end_line = k
        icons.append(icon)
        i = k + 1
    return icons


def model(icons):
    """name -> Icon, and base -> {scales present}.  The ICON_TABLE* arrays are not
    marks and are excluded by name."""
    by_name = {i.name: i for i in icons if not TABLE_RE.match(i.name)}
    scales = {}
    for icon in by_name.values():
        scales.setdefault(icon.base, set()).add(icon.scale)
    return by_name, scales


def pixels(value, cols):
    return ''.join(LIT if value & (1 << (cols - 1 - c)) else '.' for c in range(cols))


def from_pixels(text, cols):
    """The row value a pixel comment describes, or None when it cannot be read."""
    t = (text or '').strip()
    if len(t) != cols or set(t) - set(LIT + '.'):
        return None
    value = 0
    for i, ch in enumerate(t):
        if ch == LIT:
            value |= 1 << (cols - 1 - i)
    return value


# ---------------------------------------------------------------------------
# Checking
# ---------------------------------------------------------------------------

class Problem:
    """One fault, or one repair.

    An icon is a picture, so a block renders the WHOLE glyph and never a selection
    of rows: `icon.rows` for a disagreement, in two columns (binary and comment, as
    the file reads them); `rows` for a repair, one column of the reading each row now
    has.  `align` names the side a repair corrected, `count` how many rows moved,
    `msg` is a fault with no glyph to draw, `fault` marks a TABLE fault (a slot and
    a declaration disagreeing, settled by regenerating the tables), and `decl` is the
    mark's declaration line, which heads a block."""

    def __init__(self, line, owner=None, msg=None, decl=0, align=None, icon=None,
                 rows=None, count=0, fault=False):
        self.line = line
        self.owner = owner
        self.msg = msg
        self.decl = decl
        self.align = align
        self.icon = icon
        self.rows = rows
        self.count = count
        self.fault = fault

    @property
    def disagrees(self):
        return self.icon is not None and self.align is None

    @property
    def repairs(self):
        return self.align is not None

    @property
    def repairs_table(self):
        return self.fault


def note(line, msg, owner=None):
    return Problem(line, owner=owner, msg=msg)


def table_fault(line, owner, msg):
    return Problem(line, owner=owner, msg=msg, fault=True)


def aligned(line, owner, decl, to, icon, rows, count):
    return Problem(line, owner=owner, decl=decl, align=to, icon=icon, rows=rows,
                   count=count)


def table_slots(lines, icons):
    """scale -> [(entry name or None, 0-based line)], read from the ICON_TABLE* arrays.
    The line is kept so a fault can name the slot it is in."""
    out = {}
    for icon in icons:
        m = TABLE_RE.match(icon.name)
        if not m:
            continue
        scale = int(m.group(1)) if m.group(1) else 1
        entries = []
        for lineno in range(icon.decl_line + 1, icon.end_line):
            em = ENTRY_RE.match(lines[lineno])
            if em:
                entries.append((None if em.group(1) == 'NULL' else em.group(1), lineno))
        out[scale] = entries
    return out


def table_problems(lines, icons, by_name, scales):
    """Every scale's table must agree with the declarations, slot by slot.  The two
    faults are named apart because they mean opposite things: a slot naming a mark
    that no longer exists (the header does not compile), and a mark that nothing
    points at (nobody draws it)."""
    slots = table_slots(lines, icons)
    cp_of = {b: by_name[b].codepoint for b in scales}
    problems = []
    for scale in range(1, MAX_SCALE + 1):
        label = 'ICON_TABLE' if scale == 1 else f'ICON_TABLE_{scale}'
        want = [None] * SLOTS
        for base, cp in cp_of.items():
            if cp is not None and cp < SLOTS and scale in scales[base]:
                want[cp] = base if scale == 1 else f'{base}_{scale}'
        got = slots.get(scale)
        if got is None:
            problems.append(table_fault(0, '', f'{label} is missing: every scale'
                                               f' 1..{MAX_SCALE} is emitted'))
            continue
        for cp in range(min(len(got), SLOTS)):
            entry, lineno = got[cp]
            if entry == want[cp]:
                continue
            if entry and entry not in by_name:
                problems.append(table_fault(lineno + 1, entry,
                    f'{label}[{cp}] names {entry}, which is not declared anywhere in'
                    f' this file: nothing draws \\{cp:03o} at scale {scale} and the'
                    f' header does not compile'))
            elif want[cp]:
                problems.append(table_fault(lineno + 1, want[cp],
                    f'{want[cp]} is declared, but {label}[{cp}] is {entry or "NULL"}:'
                    f' it is never drawn'))
            else:
                problems.append(table_fault(lineno + 1, '',
                    f'{label}[{cp}] is {entry or "NULL"}, \\{cp:03o} declares nothing'))
    return problems


def check(lines, icons):
    """Every fault in the file as Problem records.  `owner` is the mark's name, which
    is how the walk groups them; a table fault is owned by the mark it is about and
    carries `fault`, so it can be asked about and settled."""
    by_name, scales = model(icons)
    problems = []
    # A name declared twice cannot compile, and nothing downstream can see it: model()
    # is keyed on the name, so the LAST declaration silently wins and every later edit
    # addresses the wrong copy.
    declared = {}
    for icon in icons:
        if TABLE_RE.match(icon.name):
            continue
        if icon.name in declared:
            problems.append(note(icon.where(), f'{icon.name} is declared a second time'
                                                f' here; the first is at'
                                                f' {ICONS_H.name}:{declared[icon.name]}.'
                                                f' Only one may exist, and every other'
                                                f' check and edit addresses the last',
                                 icon.name))
        else:
            declared[icon.name] = icon.where()
    for icon in by_name.values():
        cols, want_rows = icon.bits, icon.rows_wanted
        if icon.scale > 1 and icon.base not in by_name:
            problems.append(note(icon.where(), f'{icon.name} is a scale-{icon.scale}'
                                                f' variant but there is no {icon.base}'
                                                f' to fall back to', icon.name))
        if len(icon.rows) != want_rows:
            problems.append(note(icon.where(), f'{icon.name} has {len(icon.rows)} rows,'
                                                f' scale {icon.scale} needs {want_rows}',
                                 icon.name))
        if icon.ctype != icon.want_type:
            extra = ('' if icon.scale == 1 else
                     f' (a {cols}-bit row does not fit {icon.ctype}: this does not compile)')
            problems.append(note(icon.where(), f'{icon.name} is declared {icon.ctype},'
                                                f' scale {icon.scale} needs'
                                                f' {icon.want_type}{extra}', icon.name))
        for row in icon.rows:
            if row.value >> cols:
                problems.append(note(row.lineno + 1, f'{icon.name} row value 0b'
                                                      f'{row.value:b} is wider than'
                                                      f' {cols} bits', icon.name))
            have = (row.comment or '').strip()
            if not have:
                problems.append(note(row.lineno + 1, f'{icon.name} row has no pixel'
                                                      f' comment', icon.name))
                continue
            if have != pixels(row.value, cols):
                problems.append(Problem(row.lineno + 1, owner=icon.name, icon=icon,
                                        decl=icon.where()))
        # Only the scale-1 master needs the hand-written `\DDD` comment; a variant takes
        # its codepoint from that master.  A variant that carries one anyway must agree.
        if icon.scale == 1 and icon.codepoint is None:
            problems.append(note(icon.where(), f'{icon.name} has no codepoint comment above'
                                                f' it, so it cannot be placed in a table',
                                 icon.name))
        elif (icon.scale > 1 and icon.codepoint is not None and icon.base in by_name
              and icon.codepoint != by_name[icon.base].codepoint):
            problems.append(note(icon.where(),
                                 f'{icon.name} sits under a \\{icon.codepoint:03o} comment'
                                 f' but {icon.base} is'
                                 f' \\{by_name[icon.base].codepoint:03o}', icon.name))
    seen = {}
    for base in sorted(scales, key=lambda b: by_name[b].decl_line):
        cp = by_name[base].codepoint
        if cp is None:
            continue
        if cp in seen:
            problems.append(note(by_name[base].where(), f'{base} and {seen[cp]} both claim'
                                                        f' \\{cp:03o}', base))
        seen.setdefault(cp, base)
    problems.extend(table_problems(lines, icons, by_name, scales))
    return problems


def summarise(problems):
    """One honest closing line: rows that disagree and how many marks they are in,
    table faults, and anything else."""
    rows = [p for p in problems if p.disagrees]
    faults = [p for p in problems if p.repairs_table]
    notes = [p for p in problems if not p.disagrees and not p.repairs
             and not p.repairs_table]
    if not rows and not faults and not notes:
        return '0 problem(s)'
    parts = []
    if rows:
        marks = len({p.owner for p in rows})
        parts.append(f'{len(rows)} row(s) disagree in {marks} mark(s)')
    if faults:
        parts.append(f'{len(faults)} table fault(s)')
    if notes:
        parts.append(f'{len(notes)} other problem(s)')
    return ', '.join(parts)


def print_problems(problems):
    """One line per note; a mark's disagreements as one block, the binary on the left
    and its comment on the right, in the order the file reads them:

        icons.h:278: VOL_75_2 disagreements:
        binary:       comment:
        ....@@......  ...@@.......
        ....@@......  ....@@......

    Eight sentences about ten characters each is a wall; two columns is a picture.
    """
    groups, order = {}, []
    for p in problems:
        key = p.owner or ''
        if key not in groups:
            groups[key] = []
            order.append(key)
        groups[key].append(p)
    for key in sorted(order, key=lambda k: min(p.line for p in groups[k])):
        block = sorted(groups[key], key=lambda p: p.line)
        for p in block:
            if not p.disagrees and not p.repairs:
                where = f'{ICONS_H.name}:{p.line}' if p.line else ICONS_H.name
                print(f'{where}: {p.msg}')
        dis = next((p for p in block if p.disagrees), None)
        if dis:
            cols = dis.icon.bits
            pad = max(cols + 2, len('binary:') + 1)
            print(f'{ICONS_H.name}:{dis.decl}: {dis.owner} disagreements:')
            print('binary:'.ljust(pad) + 'comment:')
            for r in dis.icon.rows:
                have = (r.comment or '').strip() or '(none)'
                print(pixels(r.value, cols).ljust(cols) + '  ' + have)
        fix = next((p for p in block if p.repairs), None)
        if fix:
            cols = fix.icon.bits
            print(f'{ICONS_H.name}:{fix.decl}: {fix.owner} align to {fix.align}:')
            for r in fix.rows:
                print(pixels(r.value, cols))


# ---------------------------------------------------------------------------
# Writing
# ---------------------------------------------------------------------------

def render_array(icon, rows, ctype):
    """The array as the tool writes it: padded literal, comment regenerated from the value."""
    cols = icon.bits
    field = len('0b') + cols + 1
    out = [f'static const {ctype} {icon.name}[] PROGMEM = {{']
    for row in rows:
        literal = '0b' + format(row.value, f'0{cols}b')
        out.append(f'    {literal + ",":<{field}}  // {pixels(row.value, cols)}')
    out.append('};')
    return out


def write_tables(lines, icons):
    """Regenerate the five tables between the markers, from the declarations."""
    by_name, scales = model(icons)
    cp_of = {b: by_name[b].codepoint for b in scales}
    body = [BEGIN_MARK + ' (generated by icons_tool.py, do not hand-edit) =====',
            '// Icon lookup: codepoint -> bitmap pointer.  Scale 1 is uint8_t rows of 6 bits;',
            '// every variant is uint32_t rows of 6*N bits.  A NULL slot falls through to the',
            '// normal glyph handling, so a codepoint that has no mark prints its font glyph.']
    for scale in range(1, MAX_SCALE + 1):
        name = 'ICON_TABLE' if scale == 1 else f'ICON_TABLE_{scale}'
        ctype = 'uint8_t' if scale == 1 else 'uint32_t'
        body.append(f'static const {ctype}* const {name}[] PROGMEM = {{')
        for cp in range(SLOTS):
            entry = 'NULL'
            for base, base_cp in cp_of.items():
                if base_cp == cp and scale in scales[base]:
                    entry = base if scale == 1 else f'{base}_{scale}'
                    break
            body.append(f'    {entry + ",":<17}// {cp:2d}: \\{cp:03o}'
                        f'{SLOT_NOTES.get(cp, "")}')
        body.append('};')
        body.append('')
    body[-1] = END_MARK + ' ====='
    start = next(i for i, l in enumerate(lines) if l.startswith(BEGIN_MARK))
    end = next(i for i, l in enumerate(lines) if l.startswith(END_MARK))
    return lines[:start] + body + lines[end + 1:]


def apply_block(lines, icon, block):
    return lines[:icon.decl_line] + block + lines[icon.end_line + 1:]


# ---------------------------------------------------------------------------
# Seeding
# ---------------------------------------------------------------------------

def expand(src, target):
    """Nearest-neighbour upscale of src to `target`: target/source must be an integer."""
    factor = target // src.scale
    cols = target * BASE_COLS
    rows = []
    for r in range(src.rows_wanted):
        for _ in range(factor):
            value = 0
            for c in range(src.bits):
                if src.rows[r].value & (1 << (src.bits - 1 - c)):
                    for dup in range(factor):
                        value |= 1 << (cols - 1 - (c * factor + dup))
            rows.append(Row(value, None, 0))
    return rows


def mode_seed(lines, icons, base, target, force):
    by_name, scales = model(icons)
    if base not in scales:
        fail(f'no mark named {base} in {ICONS_H.name}')
    if not 2 <= target <= MAX_SCALE:
        fail(f'scale {target} is not a variant scale: the tool seeds 2..{MAX_SCALE},'
             f' because 6*N has to fit the uint32_t row')
    new_name = f'{base}_{target}'
    existing = by_name.get(new_name)
    if existing is not None and not force:
        fail(f'{new_name} already exists; --force overwrites it')
    source = max((s for s in scales[base] if s < target and target % s == 0), default=1)
    src = by_name[base] if source == 1 else by_name[f'{base}_{source}']
    # A seed copies the BINARY.  If the source's comments disagree, the drawing is
    # probably in them and the result will look nothing like it - say so before writing.
    off = sum(1 for r in src.rows
              if (r.comment or '').strip() != pixels(r.value, src.bits))
    if off:
        print(f'{ICONS_H.name}:{src.where()}: {src.name} has {off} row(s) whose comment'
              f' disagrees with its binary, and a seed copies the BINARY - run'
              f' `-b {src.name}` first if the comments are the drawing')
    rows = expand(src, target)
    icon = Icon(new_name, 'uint32_t', 0, rows, [])
    if existing is not None:
        # --force REPLACES this array.  Appending a second block of the same name left
        # two declarations and stopped the header compiling, so inserting is for a new
        # name only.
        lines = apply_block(lines, existing, render_array(icon, rows, 'uint32_t'))
    else:
        # No comment above a variant: the scale-1 master carries the hand-written one,
        # and a variant inherits its codepoint from that master.
        highest = max(scales[base])
        anchor = by_name[base] if highest == 1 else by_name[f'{base}_{highest}']
        block = [''] + render_array(icon, rows, 'uint32_t')
        lines = lines[:anchor.end_line + 1] + block + lines[anchor.end_line + 1:]
    lines = write_tables(lines, parse(lines))
    write_lines(lines)
    fresh = parse(read_lines())
    placed = next((i for i in fresh if i.name == new_name), None)
    where = f'{ICONS_H.name}:{placed.decl_line + 1}' if placed else ICONS_H.name
    verb = 'replaced' if existing is not None else 'seeded'
    print(f'{where}: {new_name} {verb} from {src.name} x{target // source}'
          f' ({src.bits}x{src.rows_wanted} -> {target * BASE_COLS}x{target * BASE_ROWS})')


# ---------------------------------------------------------------------------
# Stitching two marks onto one sheet
# ---------------------------------------------------------------------------

def stitch_path(name=None):
    """The sheet lives beside icons.h, whatever the cwd is."""
    return Path(name) if name else ICONS_H.parent / 'icon_stitch.txt'


CP_TOKEN_RE = re.compile(r'^\\([0-7]{3})$')


def mode_stitch_out(icons, left_name, right_name, sheet):
    """Two marks side by side, one space between the columns, at every scale both
    have.  Only shared scales: the sheet must never carry art for a variant that
    does not exist, or -i would create one."""
    by_name, scales = model(icons)
    for name in (left_name, right_name):
        if name not in by_name:
            fail(f'no mark named {name} in {ICONS_H.name}')
    if left_name == right_name:
        fail('the two columns are the same mark')
    left, right = by_name[left_name], by_name[right_name]
    if left.codepoint is None or right.codepoint is None:
        fail(f'{left_name} and {right_name} both need a codepoint comment to head the sheet')
    common = sorted(scales[left_name] & scales[right_name])
    if not common:
        fail(f'{left_name} and {right_name} share no scale, so there is nothing to line up')

    def pick(name, n):
        return by_name[name] if n == 1 else by_name[f'{name}_{n}']

    out = [f'\\{left.codepoint:03o} \\{right.codepoint:03o}']
    for n in common:
        a, b = pick(left_name, n), pick(right_name, n)
        out.append(f'{a.name} {b.name}')
        for ra, rb in zip(a.rows, b.rows):
            out.append(pixels(ra.value, a.bits) + ' ' + pixels(rb.value, b.bits))
        out.append('')
    if out[-1] == '':
        out.pop()
    sheet.write_text('\n'.join(out) + '\n', encoding='utf-8')
    print(f'{sheet}: {left_name} + {right_name}, scale(s)'
          f' {", ".join(str(n) for n in common)}')
    for n in sorted(scales[left_name] - scales[right_name]):
        print(f'  scale {n} skipped: only {left_name} has it')
    for n in sorted(scales[right_name] - scales[left_name]):
        print(f'  scale {n} skipped: only {right_name} has it')


def read_sheet(sheet, by_name):
    """(groups, problems).  A group is (scale, left_name, right_name, rows) with one
    pixel string per column per row.  Everything is checked before anything is
    written, and every problem is reported against the SHEET's line number."""
    text = sheet.read_text(encoding='utf-8')
    lines = text.splitlines()
    problems = []
    i = 0
    while i < len(lines) and not lines[i].strip():
        i += 1
    if i >= len(lines):
        return [], [f'{sheet}: the sheet is empty']
    head = lines[i].split()
    if len(head) != 2 or not all(CP_TOKEN_RE.match(t) for t in head):
        return [], [f'{i + 1}: {lines[i]}\n        (the first line must be two '
                    f'codepoints, like \\007 \\010)']
    codepoints = [int(CP_TOKEN_RE.match(t).group(1), 8) for t in head]
    i += 1

    groups = []
    while i < len(lines):
        if not lines[i].strip():
            i += 1
            continue
        names_line, names = i + 1, lines[i].split()
        if len(names) != 2:
            problems.append(f'{names_line}: {lines[i]}\n        (expected two array names)')
            i += 1
            continue
        scale, cols, bases = None, 0, []
        for col, name in enumerate(names):
            base = by_name[name].base if name in by_name else SCALE_RE.sub(
                lambda m: m.group(1) if 2 <= int(m.group(2)) <= MAX_SCALE else m.group(0),
                name)
            bases.append(base)
            if base not in by_name:
                problems.append(f'{names_line}: {lines[i]}\n        ({base} is not in'
                                f' this file)')
                scale = None
                break
            n = 1 if name == base else int(name.rsplit('_', 1)[1])
            if scale is None:
                scale = n
            elif n != scale:
                problems.append(f'{names_line}: {lines[i]}\n        (the two columns are'
                                f' different scales)')
                break
        if scale is None:
            i += 1
            while i < len(lines) and lines[i].strip():
                i += 1
            continue
        for col, base in enumerate(bases):
            cp = by_name[base].codepoint
            if cp != codepoints[col]:
                problems.append(f'{names_line}: {lines[i]}\n        (column {col + 1} is'
                                f' \\{cp:03o}, the header says \\{codepoints[col]:03o})')
        cols = BASE_COLS * scale
        i += 1
        rows, row_lines = [], []
        while i < len(lines) and lines[i].strip():
            row_lines.append(i + 1)
            segs = lines[i].split(' ')
            if len(segs) != 2 or any(len(s) != cols or set(s) - set(LIT + '.')
                                     for s in segs):
                # The line itself IS the error a reader can see; no note under it.
                problems.append(f'{i + 1}: {lines[i]}')
            else:
                rows.append(segs)
            i += 1
        want = BASE_ROWS * scale
        if len(row_lines) != want:      # rows PRESENT, not rows that parsed
            problems.append(f'{names_line}: {lines[names_line - 1]}'
                            f'\n        (scale {scale} needs {want} rows, found'
                            f' {len(row_lines)})')
        groups.append((scale, names[0], names[1], rows))
    return groups, problems


def mode_stitch_in(lines, icons, sheet):
    by_name, _ = model(icons)
    if not sheet.exists():
        fail(f'{sheet} not found; run with -o first to write one')
    groups, problems = read_sheet(sheet, by_name)
    if problems:
        print('Problem(s) in input file:')
        for p in problems:
            print(p)
        print(f'{len(problems)} problem(s), nothing written')
        return 1
    reported, written = [], 0
    for scale, left_name, right_name, rows in groups:
        for col, name in enumerate((left_name, right_name)):
            by_name, scales = model(parse(lines))
            base = by_name[name].base if name in by_name else (
                SCALE_RE.match(name).group(1) if SCALE_RE.match(name) else name)
            new_rows = [Row(from_pixels(r[col], BASE_COLS * scale), None, 0) for r in rows]
            icon = Icon(name, 'uint32_t' if scale > 1 else 'uint8_t', 0, new_rows, [])
            if name in by_name:
                lines = apply_block(lines, by_name[name],
                                    render_array(icon, new_rows, icon.want_type))
                verb = 'updated'
            else:
                highest = max(scales[base])
                anchor = by_name[base] if highest == 1 else by_name[f'{base}_{highest}']
                block = [''] + render_array(icon, new_rows, icon.want_type)
                lines = lines[:anchor.end_line + 1] + block + lines[anchor.end_line + 1:]
                verb = 'created'
            reported.append((name, verb))
            written += 1
    lines = write_tables(lines, parse(lines))
    write_lines(lines)
    fresh = {i.name: i for i in parse(read_lines())}
    for name, verb in reported:
        at = f'{ICONS_H.name}:{fresh[name].decl_line + 1}' if name in fresh else ICONS_H.name
        print(f'{at}: {name} {verb}')
    print(f'{sheet}: {written} array(s) from {len(groups)} scale(s)')
    return 0


# ---------------------------------------------------------------------------
# Reconciling a value with its comment
# ---------------------------------------------------------------------------

def reconcile(icon, direction):
    """(rows, changes, skipped).  `binary` rebuilds the values from the comments,
    `comment` regenerates the comments from the values (render_array writes them).
    The report is one aligned block for the whole mark plus notes for anything that
    could not be settled - never one line per row."""
    cols, owner = icon.bits, icon.name
    rows = list(icon.rows)
    changes, skipped = [], []
    if len(rows) > icon.rows_wanted:
        return rows, changes, [note(icon.where(), f'{owner} has {len(rows)} rows and needs'
                                                 f' {icon.rows_wanted}: the extra row holds'
                                                 f' pixels, so remove it by hand', owner)]
    while len(rows) < icon.rows_wanted:
        rows.append(Row(0, None, 0))
        changes.append(note(icon.where(), f'{owner}: padded a blank row to {len(rows)}',
                            owner))
    if icon.ctype != icon.want_type:
        changes.append(note(icon.where(), f'{owner}: {icon.ctype} -> {icon.want_type}',
                            owner))
    fixed = 0
    for row in rows:
        want = pixels(row.value, cols)
        have = (row.comment or '').strip()
        if direction == 'binary':
            value = from_pixels(have, cols)
            if value is None:
                if row.lineno:
                    skipped.append(note(row.lineno + 1, f'{owner}: comment'
                                                        f' {have or "<none>"!r} is not a'
                                                        f' {cols}-pixel drawing, so it'
                                                        f' cannot be read', owner))
                continue
            if value != row.value:
                row.value = value
                fixed += 1
        elif have != want:
            fixed += 1
    if fixed:
        changes.append(aligned(icon.where(), owner, icon.where(),
                               'comments' if direction == 'binary' else 'binary',
                               icon, rows, fixed))
    return rows, changes, skipped


def write_icon(lines, icon, rows):
    """Write one icon through the shared writer, then regenerate the tables."""
    block = render_array(icon, rows, icon.want_type)
    if block == lines[icon.decl_line:icon.end_line + 1]:
        return lines, False
    lines = apply_block(lines, icon, block)
    return write_tables(lines, parse(lines)), True


def reconcile_one(lines, icon, direction):
    """Apply one direction to one mark.  Returns (lines, changes, skipped, wrote)."""
    rows, changes, skipped = reconcile(icon, direction)
    if skipped and any(s.msg.startswith(icon.name) for s in skipped):
        return lines, changes, skipped, False
    lines, wrote = write_icon(lines, icon, rows)
    return lines, changes, skipped, wrote


# ---------------------------------------------------------------------------
# Prompts
# ---------------------------------------------------------------------------

def ask_yes(prompt, default=False):
    suffix = ' [Y/n] ' if default else ' [y/N] '
    try:
        reply = input(prompt + suffix).strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return default
    if reply == '':
        return default
    return reply in ('y', 'yes')


def ask_keep():
    """Which half is the drawing: 'binary' or 'comment', or None for skip (the default)."""
    print('  Keep the BINARY, keep the COMMENT, or skip?')
    try:
        reply = input('  [1] binary  [2] comment  [3] skip (default) > ').strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return None
    return {'1': 'binary', 'b': 'binary', 'binary': 'binary',
            '2': 'comment', 'c': 'comment', 'comment': 'comment'}.get(reply)


# ---------------------------------------------------------------------------
# Modes
# ---------------------------------------------------------------------------

def mode_list(icons):
    by_name, scales = model(icons)
    for base in sorted(scales, key=lambda b: (by_name[b].codepoint or 0, b)):
        have = ', '.join(str(s) for s in sorted(scales[base]))
        print(f'\\{by_name[base].codepoint:03o}  {base:<12} scales {have}')
    only_one = [b for b in scales if len(scales[b]) == 1]
    print(f'\n{len(scales)} marks, {len(only_one)} at scale 1 only')


def kb(n):
    return f'{n / 1024:.1f} KB'


def mode_stats(lines, icons):
    """What icons.h costs the image, as far as that can be known from here.

    The data side is exact: every array is 8*N rows of one row type, and every table
    is SLOTS pointers.  Two things are not derivable from this file and are said out
    loud instead of guessed: the COPY COUNT (each TU that emits _writeGlyph() keeps
    its own - measured as 2 on a PSFBUFFER TFT build and 3 on an OLED one) and the
    cost of the loop itself, which is code rather than data.
    """
    by_name, scales = model(icons)
    size = {'uint8_t': 1, 'uint16_t': 2, 'uint32_t': 4}
    referenced = {entry for entries in table_slots(lines, icons).values()
                  for entry, _ in entries if entry}
    per_scale, in_image, orphaned = {}, 0, 0
    for base, scale_set in scales.items():
        for s in scale_set:
            icon = by_name[base] if s == 1 else by_name[f'{base}_{s}']
            n = icon.rows_wanted * size[icon.ctype]
            slot = per_scale.setdefault(s, [0, 0])
            slot[0] += n
            slot[1] += 1
            if icon.name in referenced:
                in_image += n
            else:
                orphaned += n
    print('pixels, per scale:')
    for s in sorted(per_scale):
        n, count = per_scale[s]
        print(f'  scale {s}: {n:5d} B ({kb(n)}) over {count:2d} mark(s),'
              f' {BASE_ROWS * s} rows of {BASE_COLS * s} bits')
    tables = MAX_SCALE * SLOTS * PTR_BYTES
    data = in_image + tables
    print(f'  tables:  {tables:5d} B ({kb(tables)}), {MAX_SCALE} tables of {SLOTS}'
          f' pointers')
    print(f'in the image: {data} B ({kb(data)}): the arrays its tables name, plus'
          f' the tables')
    if orphaned:
        print(f'not in the image: {orphaned} B declared but named by no table -'
              f' static and unreferenced, so the linker drops it')
    print(f'with copies: {kb(data * 2)} on a PSFBUFFER TFT build, {kb(data * 3)} on'
          f' an OLED build')
    print('  measured, not derived: each TU that emits _writeGlyph() keeps its own'
          ' copy of every array.')
    print('not counted: the _writeGlyph() loop itself, which is code; and the pixel'
          ' comments, which the preprocessor strips first.')


def mode_check(lines, icons):
    problems = check(lines, icons)
    if problems:
        print_problems(problems)
    print(summarise(problems))
    if any(p.repairs_table for p in problems):
        print('  run without parameters to be asked about each of these')
    return 1 if problems else 0


def walk(lines, icons, direction):
    """Ask about each mark with a problem, apply the answer, and re-read the file after
    every write: a write that changes a row count moves every line below it, so the
    icon objects from before it no longer describe the file.  `direction` is 'binary'
    or 'comment' for -b / -c, or 'ask' for the bare run.

    A table fault is asked first and settled by regenerating the tables - one action
    that fixes a slot naming a deleted mark AND a mark nothing points at, because the
    writer derives every slot from the declarations."""
    handled, written, left = set(), 0, 0
    while True:
        owners = [o for o in owners_with_problems(lines, icons) if o not in handled]
        if not owners:
            break
        owner = owners[0]
        handled.add(owner)
        print()
        block = [p for p in check(lines, icons) if p.owner == owner]
        print_problems(block)

        if any(p.repairs_table for p in block):
            if ask_yes(f'  {owner}: regenerate the icon tables?'):
                lines = write_tables(lines, parse(lines))
                write_lines(lines)
                lines = read_lines()
                icons = parse(lines)
                written += 1
            else:
                left += 1
            block = [p for p in check(lines, icons) if p.owner == owner]

        if not any(p.disagrees for p in block):
            continue
        if direction == 'ask':
            # The answer names the half that is RIGHT, so the other half is what gets
            # written: keeping the binary means regenerating the comment, and back.
            kept = ask_keep()
            chosen = None if kept is None else ('comment' if kept == 'binary' else 'binary')
        else:
            verb = ('rebuild the binary from the comment' if direction == 'binary'
                    else 'regenerate the comment from the binary')
            chosen = direction if ask_yes(f'  {owner}: {verb}?') else None
        if chosen is None:
            left += 1
            continue
        by_name, _ = model(icons)
        if owner not in by_name:
            continue                  # a slot naming a deleted mark: no pixels to settle
        lines, changes, skipped, wrote = reconcile_one(lines, by_name[owner], chosen)
        print_problems(changes + skipped)
        if wrote:
            write_lines(lines)
            lines = read_lines()
            icons = parse(lines)
            written += 1
    return written, left, lines, icons


def mode_reconcile(lines, icons, direction, name):
    """-b / -c.  With a name: that mark, silently.  Without: walk the mismatched
    marks, show each one's problems, and ask [y/N] once per mark."""
    if name:
        by_name, _ = model(icons)
        if name not in by_name:
            fail(f'no mark named {name} in {ICONS_H.name}; if a table slot names it,'
                 f' that is a table fault - run with no parameters to be asked')
        lines, changes, skipped, wrote = reconcile_one(lines, by_name[name], direction)
        print_problems(changes + skipped)
        if wrote:
            write_lines(lines)
        fixed = sum(p.count for p in changes if p.repairs)
        print(f'{fixed} row(s) realigned, {len(skipped)} skipped'
              f'{"" if wrote else ", nothing written"}')
        return 0 if not skipped else 1

    if not sys.stdin.isatty():
        return mode_check(lines, icons)
    written, left, _, _ = walk(lines, icons, direction)
    print(f'\n{written} mark(s) rewritten, {left} left alone')
    return 0


def mode_walk(lines, icons):
    """The bare run: one question per mismatched mark, the answer naming which half
    of the pair is the drawing."""
    if not sys.stdin.isatty():
        return mode_check(lines, icons)
    written, left, lines, icons = walk(lines, icons, 'ask')
    print(f'\n{written} mark(s) rewritten, {left} skipped')
    remaining = check(lines, icons)
    print(summarise(remaining))
    return 1 if remaining else 0


def owners_with_problems(lines, icons):
    """The marks that have at least one problem, in declaration order."""
    seen = []
    for p in sorted(check(lines, icons), key=lambda p: p.line):
        if p.owner and p.owner not in seen:
            seen.append(p.owner)
    return seen


# ---------------------------------------------------------------------------

def main():
    ap = argparse.ArgumentParser(
        description='one tool for the icon marks and their scale variants in icons.h.',
        formatter_class=argparse.RawDescriptionHelpFormatter, add_help=False)
    action = ap.add_mutually_exclusive_group()
    action.add_argument('-s', '--seed', nargs=2, metavar=('NAME', 'SCALE'),
                        help='seed NAME_SCALE from the largest divisor that exists')
    action.add_argument('-o', '--stitch-out', nargs='+', metavar='NAME',
                        help='write LEFT and RIGHT side by side to a sheet, every scale'
                             ' both have (icon_stitch.txt by default)')
    action.add_argument('-i', '--stitch-in', nargs='?', const=True, metavar='FILE',
                        help='read a sheet back into icons.h, creating any array it names')
    action.add_argument('-b', '--fix-binary', nargs='?', const=True, metavar='NAME',
                        help='the comment is the drawing: rebuild the row values from it')
    action.add_argument('-c', '--fix-comment', nargs='?', const=True, metavar='NAME',
                        help='the binary is the drawing: regenerate the comment from it')
    action.add_argument('-k', '--check', action='store_true',
                        help='validate and write nothing')
    ap.add_argument('--force', action='store_true',
                    help='overwrite an existing variant (-s)')
    ap.add_argument('--list', action='store_true',
                    help='every mark, its codepoint, and the scales it has')
    ap.add_argument('--stats', action='store_true',
                    help='flash cost per scale, plus the pointer tables')
    ap.add_argument('-h', '--help', action='help', help='show this help and exit')

    args = ap.parse_args()
    lines = read_lines()
    icons = parse(lines)

    if args.list or args.stats:
        if args.list:
            mode_list(icons)
        if args.stats:
            mode_stats(lines, icons)
        return 0
    if args.seed:
        mode_seed(lines, icons, args.seed[0], int(args.seed[1]), args.force)
        return 0
    if args.stitch_out:
        if len(args.stitch_out) not in (2, 3):
            fail('--stitch-out takes LEFT RIGHT [FILE]')
        mode_stitch_out(icons, args.stitch_out[0], args.stitch_out[1],
                        stitch_path(args.stitch_out[2]
                                    if len(args.stitch_out) == 3 else None))
        return 0
    if args.stitch_in is not None:
        return mode_stitch_in(lines, icons,
                              stitch_path(None if args.stitch_in is True
                                          else args.stitch_in))
    if args.fix_binary is not None:
        name = None if args.fix_binary is True else args.fix_binary
        return mode_reconcile(lines, icons, 'binary', name)
    if args.fix_comment is not None:
        name = None if args.fix_comment is True else args.fix_comment
        return mode_reconcile(lines, icons, 'comment', name)
    if args.check:
        return mode_check(lines, icons)
    return mode_walk(lines, icons)


if __name__ == '__main__':
    sys.exit(main())
