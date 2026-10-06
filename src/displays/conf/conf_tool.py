#!/usr/bin/env python3
"""
conf_tool.py - the layout conf file tool.

The conf files served their purpose by being copies of each other, and have drifted: a widget
that a layout omits is simply not written, so a reader has to open several files to learn which
options exist at all.  This tool makes every file the same shape again.

MASTER TEMPLATE:  ../widgets/widgetsconfig.h   (struct LayoutData, struct BootData)

    That struct is the authority for everything below, because it is also the authority for the
    compiler: these are designated initialisers, so a field written out of declaration order does
    not build.  The master supplies
      - the field ORDER, which the tool rewrites any entry into
      - the field TYPE, which decides what an empty value looks like ({ } or false)
      - the section headers  (/* SCROLLS ... */, /* WIDGETS ... */, ...)
      - the per-field comments (e.g. // VU rotated 90 degrees)
    There is no second list to keep in step: edit the master and re-run this.

USAGE:
    py conf_tool.py --clean [--comments=keep|fill|master] [--dry-run]
    py conf_tool.py --import <conf_file> --name "Name" [--target <file.h>] [--dry-run]

OPTIONS:
    --clean            Repair every display*conf.h in this directory (the wildcard pass).
    --comments=...     (clean, default keep) how a trailing comment is chosen for a field the
                       conf already has.  keep writes none at all.  fill writes the master's
                       comment where the conf has none, so a conf that never documented a field
                       comes to match the master.  master does that AND re-texts the comments
                       that already exist when they differ.  Only the fields the master itself
                       comments can be affected.  A protected marker such as
                       // <--------- NEEDS EDITING! always keeps its own text.
    --import           Ingest a community yoRadio display conf file, creating a new
                       displayTFT{W}x{H}conf.h or appending layout entries to an existing one.
    --name, -n         (import) layout display name.  Required by --import.
    --target           (import) target conf file; default is chosen by DSP_WIDTH x DSP_HEIGHT.
    --dry-run          Write the .new.h temp files but never touch the originals.  The run ends
                       by offering to delete the temps, and prints their paths so they can be
                       read in the editor first.
    --help, -h         Print this text.

EXAMPLES:
    py conf_tool.py --clean --dry-run
    py conf_tool.py --clean
    py conf_tool.py --import krzxsiek-displayNV3007_142conf.h --name "krzxsiek"
    py conf_tool.py --import krzxsiek-displayNV3007_142conf.h --name "krzxsiek" --dry-run

WHAT --clean DOES (per file, nothing is written until the final prompt):
    1. _layoutNames[] is checked against the number of _layouts[] entries.  Too few names are
       appended from the layout entries' own // Name comments; too many are reported and cut.
       Names win: every entry comment is then rewritten from _layoutNames[].
    2. Every field of struct LayoutData is written into every layout entry - missing ones as { },
       or false for a boolean - in master order, so the files read identically.  Same for the
       _bootConfig block against struct BootData.
    3. Section headers are normalised to the master's text, so /* BANDS ... */ becomes
       /* VU BANDS ... */ and a missing header is added.  Duplicated headers collapse to one.
    4. Comments are preserved.  Existing trailing comments (// unused, // <--------- NEEDS EDITING!,
       // clock disappears when VU is on) are kept verbatim; commented-out alternatives such as
       // .clockConf = {...} stay on the side of the field they were found on; unknown or obsolete
       // fields (// ??? ...) travel with the field they followed.  --comments=fill writes the
       // master's comment onto a field that has none, and --comments=master also re-texts the ones
       // that exist when they differ; either way every change is named in the report, and a
       // protected marker stops it.
    5. A layout entry missing metaConf or playlistConf is NOT repaired: those two are load
       bearing (dialogs write into the meta line, the playlist page is built on playlistConf), so
       the entry is left byte-identical, shouted about, and listed as NEEDS HAND EDITING.  No
       layout is ever removed, because removing one shifts every index after it and that index is
       persisted in config.store.layoutId.

WHAT --import DOES:
    Reads old-style `const <Type> <name> PROGMEM = {...};` lines (and new-style _layouts[]),
    handles #ifdef/#ifndef BOOMBOX_STYLE variants, HIDE_* defines (zeroed, original kept as a
    comment), RSSI_DIGIT / HIDE_IP_ONLY_MAIN_SCREEN (set true), the BITRATE_FULL/TITLE_FIX block,
    and always emits a conforming entry: every master field, in master order, with
    `// <--------- NEEDS EDITING!` on any value the source did not provide.
"""

import re, sys, os, glob, shutil

MAX_NAME_LEN = 50          # _layoutNames[][64] -- truncate at 50 for safety
MASTER_REL   = os.path.join('..', 'widgets', 'widgetsconfig.h')

# --- import-mode tables (unchanged from importlayout.py) ----------------------
COLOR_TO_FIELD_NOTE = None  # unused placeholder kept out of the way

REQUIRED_STRINGS = {
    'numtxtFmt':       '"%d"',
    'rssiFmt':         '"%d"',
    'iptxtFmt':        '""',
    'voltxtFmt':       '""',
    'batterytxtFmt':   '"%d%%"',
    'bitrateFmt':      '"%d"',
}

NAME_MAP = {'heapbarConf': 'bufferbarConf'}

HIDE_TO_FIELD = {
    'HIDE_TITLE2': 'title2Conf', 'HIDE_VU': 'vuConf', 'HIDE_VOLBAR': 'volbarConf',
    'HIDE_BUFFERBAR': 'bufferbarConf', 'HIDE_VOL': 'voltxtConf', 'HIDE_IP': 'iptxtConf',
    'HIDE_RSSI': 'rssiConf', 'HIDE_BATTERY': 'batteryConf', 'HIDE_WEATHER': 'weatherConf',
    'HIDE_HEAPBAR': 'bufferbarConf',
}

TRUE_DEFINES = {
    'HIDE_IP_ONLY_MAIN_SCREEN': 'shareWeatherIP',
    'RSSI_DIGIT':               'rssiDigit',
}

# Fields this tool must never invent.
MANDATORY_LAYOUT_FIELDS = ('metaConf', 'playlistConf')

# Header labels seen in the wild that mean the same master group.
HEADER_ALIASES = {'BANDS': 'VU BANDS', 'CODEC BADGE': 'CODEC BADGE'}

# --comments: how a trailing comment is chosen for a field the conf already has.
#   keep   - the conf's own text, always, and nothing is ever added.  The default.
#   fill   - the master's comment is written where the conf has none, so every conf ends up
#            documenting the fields the master documents.  Existing text is never touched.
#   master - fill, plus the master's wording where both sides carry one, so a sentence that
#            drifted is corrected.  The superset: the master's comments win everywhere.
# Only the fields the master itself comments can be affected, which today is the seven TRANSFORMS
# booleans in widgetsconfig.h.  Everything else in struct LayoutData and struct BootData carries a
# section header and no per-field comment, so there is nothing for these modes to apply.
COMMENT_MODES = ('keep', 'fill', 'master')

# A trailing comment that means "a human has to look at this line" outranks the master, so it
# keeps its own text even under --comments=master.  Nothing here is ever rewritten: the note in
# the report says what the master wanted to say instead.
PROTECTED_COMMENT_RE = re.compile(r'NEEDS EDITING|DO NOT EDIT|DO NOT REMOVE|SAME AS ABOVE')


def comment_key(text):
    """Compare two trailing comments ignoring the marker and the spacing.

    The conf keeps the raw text ('// VU rotated 90 degrees') and the master is stored already
    prefixed, so without this a re-run would report a change on every line for ever."""
    s = re.sub(r'^/\*+', '', text.strip())
    s = re.sub(r'\*+/$', '', s)
    s = re.sub(r'^//\s*', '', s)
    return re.sub(r'\s+', ' ', s).strip()


# ==============================================================================
#  Master template
# ==============================================================================

class Master:
    """struct LayoutData / struct BootData from widgetsconfig.h, with the comments."""
    def __init__(self, path):
        self.path = path
        with open(path, 'r', encoding='utf-8', errors='replace') as f:
            self.text = f.read()
        self.layout = []          # [(field, type)]
        self.boot = []
        self.header = {}          # first field of a group -> header text
        self.comment = {}         # field -> trailing comment text (without //)
        self.type = {}            # field -> type
        self.header_label = {}    # header text -> label, e.g. 'VU BANDS'
        for sname, attr in (('LayoutData', 'layout'), ('BootData', 'boot')):
            self._parse_struct(sname, attr)
        if not self.layout:
            die(f"master '{path}' has no struct LayoutData - cannot continue")

    def _parse_struct(self, name, attr):
        lines = self.text.split('\n')
        start = None
        for i, l in enumerate(lines):
            if re.match(r'\s*struct\s+' + name + r'\s*\{', l):
                start = i
                break
        if start is None:
            return
        depth, pending_header, i = 0, None, start
        while i < len(lines):
            raw = lines[i]
            s = raw.strip()
            depth += raw.count('{') - raw.count('}')
            if s.startswith('/*') and not s.endswith('*/'):
                block = [s]
                i += 1
                while i < len(lines) and '*/' not in lines[i]:
                    block.append(lines[i].strip())
                    i += 1
                if i < len(lines):
                    block.append(lines[i].strip())
                pending_header = ' '.join(block)
                i += 1
                continue
            if s.startswith('/*'):
                pending_header = s
            else:
                m = re.match(r'^(\w+)\s+(\w+)\s*;(?:\s*//\s*(.*))?$', s)
                if m:
                    ftype, fname, cmt = m.group(1), m.group(2), (m.group(3) or '').strip()
                    getattr(self, attr).append((fname, ftype))
                    self.type[fname] = ftype
                    if cmt:
                        self.comment[fname] = '// ' + cmt
                    if pending_header:
                        self.header[fname] = pending_header
                        self.header_label[self._norm_header(pending_header)] = pending_header
                        pending_header = None
            i += 1
            if depth <= 0 and i > start + 1:
                break

    @staticmethod
    def _norm_header(text):
        """'/* BANDS   { onebandwidth, ... } */' -> 'BANDS' (label only)."""
        m = re.match(r'/\*\s*([A-Z][A-Z ]*?)(?:\s{2,}|\s*\{|$)', text.strip())
        return m.group(1).strip() if m else text.strip()

    def label(self, header_text):
        lab = self._norm_header(header_text)
        return HEADER_ALIASES.get(lab, lab)

    def group_first_field(self):
        """field -> True when it is the first field of its master group."""
        out, seen = {}, set()
        for field, _ in self.layout:
            hdr = self.header.get(field)
            if hdr is not None:
                lab = self.label(hdr)
                if lab in seen:
                    out[field] = False
                else:
                    seen.add(lab)
                    out[field] = True
            else:
                out[field] = False
        return out

    def null_value(self, field):
        return 'false' if self.type.get(field) == 'bool' else '{ }'


# ==============================================================================
#  small helpers
# ==============================================================================

def die(msg):
    print(f"\nERROR: {msg}\n")
    sys.exit(1)


def read_text(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def write_text(path, text):
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(text)


def trunc_name(n):
    if len(n) > MAX_NAME_LEN:
        t = n[:MAX_NAME_LEN]
        print(f"WARNING: Name '{n}' exceeds {MAX_NAME_LEN} chars, truncated to '{t}'")
        return t
    return n


def count_braces(line):
    """Brace delta of one line, ignoring braces inside comments.

    The section headers carry {{ left, top, fontsize, align }, buffsize, ... } and there is one
    per group in every entry, so counting the raw text walks the depth upwards for ever and no
    block ever closes."""
    s = re.sub(r'/\*.*?\*/', '', line)
    s = re.sub(r'//.*$', '', s)
    return s.count('{') - s.count('}')


def find_block(lines, decl_re, start_from=0):
    """Return (start_idx, end_idx) of a brace block beginning at decl_re, or (None, None)."""
    start = None
    for i in range(start_from, len(lines)):
        if re.search(decl_re, lines[i]):
            start = i
            break
    if start is None:
        return None, None
    depth = 0
    for i in range(start, len(lines)):
        depth += count_braces(lines[i])
        if depth <= 0 and i > start:
            return start, i
    return start, None


def split_entries(block_lines):
    """Split a _layouts[] block body into entry line-lists, each starting at its '{   // Name'."""
    entries, cur, depth = [], None, 0
    for l in block_lines:
        if cur is None:
            if re.match(r'^\s*\{', l):
                cur = [l]
                depth = count_braces(l)
                if depth <= 0:
                    entries.append(cur); cur = None
            # anything before the first entry (comments) is ignored here
            continue
        cur.append(l)
        depth += count_braces(l)
        if depth <= 0:
            entries.append(cur); cur = None
    if cur:
        entries.append(cur)
    return entries


def entry_name(entry_lines):
    m = re.match(r'^\s*\{\s*//\s*(.*?)\s*$', entry_lines[0])
    return m.group(1) if m else ''


def indent_of(line):
    return line[:len(line) - len(line.lstrip())]


# ==============================================================================
#  Master-driven block rewriting (shared by the clean pass and the import pass)
# ==============================================================================

FIELD_RE = re.compile(
    r'^(?P<indent>[ \t]*)\.(?P<field>\w+)[ \t]*=[ \t]*(?P<val>.*?)[ \t]*(?P<end>[,;])?[ \t]*'
    r'(?P<comment>//.*|/\*.*\*/)?[ \t]*$')

COMMENTED_FIELD_RE = re.compile(r'^[ \t]*//[ \t]*\.(?P<field>\w+)')

# The label is all-caps plus spaces, and one group name carries punctuation ("LINES + RECTANGLES"),
# so the charset has to allow it or that header would be treated as an ordinary comment and left
# behind while the master's own header was emitted next to it.
HDR_RE = re.compile(r'^[ \t]*/\*\s*(?P<label>[A-Z][A-Z +&/]*?)(?:\s{2,}|\s*\{|\s*\*/|$)')


def normalise_value(v):
    v = v.strip()
    if re.fullmatch(r'\{\s*\}', v):
        return '{ }'
    return v


def trim_bands_conf(val):
    """VUBandsConfig no longer carries fadespeed (the fade rate is derived from the band length and
    VU_FADE_MS), and an old source still passes a sixth value - emitting it verbatim would be a
    "too many initializers" error.  Only a plain integer sixth value is treated as the old
    fadespeed; anything else is left alone so a real expression can never be silently dropped."""
    s = val.strip()
    if not (s.startswith('{') and s.endswith('}')):
        return val
    parts = s[1:-1].strip().split(',')
    if len(parts) == 6 and parts[5].strip().isdigit():
        print("NOTE: removing the obsolete fadespeed value from bandsConf")
        return '{ ' + ','.join(parts[:5]).strip() + ' }'
    return val


class BlockModel:
    """One initialiser block (a layout entry body, or the _bootConfig body), parsed and
    ready to be re-emitted in master order with its comments intact."""

    def __init__(self, interior_lines, master, fields, indent, comments_mode='keep'):
        self.master = master
        self.fields = fields               # [(field, type)] in master order
        self.indent = indent
        self.comments_mode = comments_mode
        self.values = {}                   # field -> value text
        self.comments = {}                 # field -> trailing comment text
        self.comment_report = []           # (action, field, old, new) for the run report
        self.prefix = {}                   # field -> [lines] emitted above it
        self.suffix = {}                   # field -> [lines] emitted below it
        self.head = []                     # lines before the first field
        self.tail = []                     # lines after the last field
        self.unordered = []                # fields found out of master order
        self.unknown = []                  # field names with no master slot
        self._parse(interior_lines)

    # -- parsing ------------------------------------------------------------
    def _parse(self, lines):
        known = set(f for f, _ in self.fields)
        order = {f: i for i, (f, _) in enumerate(self.fields)}
        pending = []           # lines waiting for the next field
        seen = []              # field names in the order found
        last_field = None
        for raw in lines:
            s = raw.strip()
            fh = HDR_RE.match(raw) if s else None
            if fh:
                # A section header: dropped here and re-emitted from the master, so the text
                # normalises and a duplicated one collapses.
                continue
            m = FIELD_RE.match(raw) if (s and not s.startswith('//')) else None
            if m:
                f = m.group('field')
                if f not in known:
                    self.unknown.append(f)
                seen.append(f)
                self.values[f] = normalise_value(m.group('val'))
                if m.group('comment'):
                    self.comments[f] = m.group('comment').strip()
                if pending:
                    if last_field is None:
                        # Everything above the first field stays above the first header.
                        self.head.extend(pending)
                    elif pending[-1].strip() == '':
                        # A group that ends in a blank line documents the field above it.
                        self.suffix.setdefault(last_field, []).extend(pending)
                    else:
                        # Otherwise it documents the field below it - this is what keeps the
                        # two-line note above .rotateVU with .rotateVU instead of leaving it
                        # trailing .weatherMoveVU, which put it above the TRANSFORMS header.
                        self.prefix[f] = list(pending)
                    pending = []
                last_field = f
                continue
            cf = COMMENTED_FIELD_RE.match(raw)
            if cf:
                f = cf.group('field')
                if f in known and f in self.values:
                    self.suffix.setdefault(f, []).append(raw)      # an alternative, below it
                elif f in known:
                    self.prefix.setdefault(f, []).append(raw)      # an alternative, above it
                else:
                    (self.suffix.setdefault(last_field, []) if last_field else self.tail).append(raw)
                last_field = f if f in known else last_field
                continue
            # every other comment, blank or preprocessor line waits for the next field
            pending.append(raw)
        if pending:
            if last_field is not None:
                self.tail.extend(pending)
            else:
                self.head.extend(pending)
        # order check
        idx = [order[f] for f in seen if f in order]
        for a, b in zip(idx, idx[1:]):
            if b < a:
                self.unordered.append(f"{[f for f in seen if f in order]}")
                break

    # -- emitting -----------------------------------------------------------
    def _field_comment(self, field):
        """The trailing comment for a field the conf already has.

        keep   - the conf's own text, and the master's comment is never consulted.
        fill   - the master's text where the conf has none, so a conf that never documented a
                 field comes to match the master.  An existing comment is never touched.
        master - fill, plus the master's wording where both sides carry one, so a sentence that
                 drifted is corrected, and a sentence only the confs have is still left alone.
                 A protected marker outranks the master in either mode, and every change lands
                 in comment_report so the run can name it."""
        own = self.comments.get(field)
        if self.comments_mode == 'keep':
            return own
        theirs = self.master.comment.get(field)
        if not own:
            if not theirs:
                return None
            self.comment_report.append(('+', field, '', theirs))
            return theirs
        if self.comments_mode != 'master':
            return own
        if not theirs or comment_key(own) == comment_key(theirs):
            return own
        if PROTECTED_COMMENT_RE.search(own):
            self.comment_report.append(('!', field, own, theirs))
            return own
        self.comment_report.append(('~', field, own, theirs))
        return theirs

    def emit(self, group_first):
        out = list(self.head)
        for field, _ in self.fields:
            hdr = self.master.header.get(field)
            if hdr is not None and group_first.get(field):
                out.append(f'{self.indent}{hdr}')
            out.extend(self.prefix.get(field, []))
            if field in self.values:
                comment = self._field_comment(field)
                line = f'{self.indent}.{field:19s} = {self.values[field]},'
                if comment:
                    line += ' ' + comment
                out.append(line)
            else:
                line = f'{self.indent}.{field:19s} = {self.master.null_value(field)},'
                if self.master.comment.get(field):
                    line += ' ' + self.master.comment[field]
                out.append(line)
            out.extend(self.suffix.get(field, []))
        out.extend(self.tail)
        return collapse_blanks(out)


def collapse_blanks(lines):
    out = []
    for l in lines:
        if l.strip() == '' and out and out[-1].strip() == '':
            continue
        out.append(l)
    return out


def rebuild_boot(block, master):
    """block = (start_idx, end_idx) of `const BootData _bootConfig PROGMEM = {...};`"""
    lines = block['lines']
    interior = lines[1:-1]
    indent = indent_of(interior[1]) if len(interior) > 1 else '        '
    model = BlockModel(interior, master, master.boot, indent)
    first = master.group_first_field()
    for f, _ in master.boot:            # group_first_field() was built for layout; rebuild for boot
        pass
    gf = {}
    seen = set()
    for field, _ in master.boot:
        hdr = master.header.get(field)
        if hdr is not None:
            lab = master.label(hdr)
            gf[field] = lab not in seen
            seen.add(lab)
        else:
            gf[field] = False
    return model.emit(gf), model


# ==============================================================================
#  Clean pass
# ==============================================================================

class ConfFile:
    def __init__(self, path, master, comments_mode='keep'):
        self.path = path
        self.master = master
        self.comments_mode = comments_mode
        self.text = read_text(path)
        self.lines = self.text.split('\n')
        self.changed = False
        self.notes = []          # human-readable report lines
        self.needs_hand = []     # serious warnings
        self.out = None

    def note(self, msg, serious=False):
        (self.needs_hand if serious else self.notes).append(msg)

    def _report_comments(self, model, where):
        """Name every trailing comment the master added or overwrote, and every one a marker kept.

        The additions are collected onto one line: a fill pass touches the most lines of all, and
        seven field names are easier to check as a list than as seven three-line entries."""
        added = [f for a, f, _, _ in model.comment_report if a == '+']
        if added:
            self.note(f'  {where}: {len(added)} trailing comment(s) added from the master: '
                      f'{", ".join("." + f for f in added)}')
        for action, field, old, new in model.comment_report:
            if action == '~':
                self.note(f'  {where}: .{field} trailing comment re-texted from the master:\n'
                          f'      was: {old}\n'
                          f'      now: {new}')
            elif action == '!':
                self.note(f'  {where}: .{field} kept its own comment - protected marker:\n'
                          f'      kept:   {old}\n'
                          f'      master: {new}')

    # -- top level ---------------------------------------------------------
    def run(self):
        lines = list(self.lines)
        new_lines = list(lines)

        # 1. _layoutNames block and _layouts block
        ls, le = find_block(new_lines, r'_layouts\[\]\s+PROGMEM\s*=\s*\{')
        ns, ne = find_block(new_lines, r'_layoutNames\[\]\[\d+\]\s+PROGMEM\s*=\s*\{')
        if ls is None or ns is None:
            self.note(f"{os.path.basename(self.path)}: no _layouts[]/_layoutNames[] - skipped")
            return False
        entries = split_entries(new_lines[ls + 1:le])
        names = [l.strip().strip(',').strip('"') for l in new_lines[ns + 1:ne] if l.strip()]
        entry_names = [entry_name(e) for e in entries]
        if not entries:
            self.note(f"{os.path.basename(self.path)}: _layouts[] has no entries - skipped")
            return False

        # 2. name list repair: _layoutNames wins, so grow it from the entry comments
        if len(names) < len(entries):
            for i in range(len(names), len(entries)):
                add = entry_names[i] or f"Layout {i + 1}"
                names.append(trunc_name(add))
                self.note(f"  _layoutNames: appended \"{names[-1]}\" (from the entry comment)")
        elif len(names) > len(entries):
            dropped = names[len(entries):]
            names = names[:len(entries)]
            self.note(f"  _layoutNames: dropped {len(dropped)} extra name(s): {dropped}")

        # 3. entry name comments follow _layoutNames
        for i, e in enumerate(entries):
            want = names[i]
            if entry_names[i] != want:
                e[0] = re.sub(r'(^\s*\{\s*//\s*).*$', lambda m: m.group(1) + want, e[0])
                self.note(f"  entry {i}: comment renamed to \"{want}\"")

        # 4. each layout entry, in master order, every field written
        new_entries = []
        group_first = self.master.group_first_field()
        for i, e in enumerate(entries):
            present = set()
            for l in e[1:-1]:
                m = FIELD_RE.match(l)
                if m and not l.strip().startswith('//'):
                    present.add(m.group('field'))
            missing_mandatory = [f for f in MANDATORY_LAYOUT_FIELDS if f not in present]
            if missing_mandatory:
                self.note(
                    f"  entry {i} (\"{names[i]}\") has no .{' and no .'.join(missing_mandatory)} - "
                    f"left byte-identical", serious=True)
                new_entries.append(e)
                continue
            indent = indent_of(e[1]) if len(e) > 2 else '        '
            model = BlockModel(e[1:-1], self.master, self.master.layout, indent, self.comments_mode)
            inserted = [f for f, _ in self.master.layout if f not in model.values]
            if inserted:
                self.note(f"  entry {i} (\"{names[i]}\"): wrote {len(inserted)} missing field(s): "
                          f"{', '.join('.' + f for f in inserted)}")
            if model.unknown:
                self.note(f"  entry {i} (\"{names[i]}\"): {len(model.unknown)} field(s) with no master "
                          f"slot, kept in place: {', '.join('.' + f for f in model.unknown)}")
            if model.unordered:
                self.note(f"  entry {i} (\"{names[i]}\"): fields were out of master order - the entry "
                          f"was rewritten into order")
            body = model.emit(group_first)
            self._report_comments(model, f'entry {i} ("{names[i]}")')
            new_entries.append([e[0]] + body + [e[-1]])

        # 5. rebuild the file
        names_block = [new_lines[ns]] + [f'    "{n}",' for n in names] + ['};']
        out = list(new_lines[:ns]) + names_block + list(new_lines[ne + 1:])
        # the layouts block has moved if the name block changed length
        shift = len(names_block) - (ne - ns + 1)
        ls2, le2 = ls + shift, le + shift
        body = []
        for e in new_entries:
            body.extend(e)
        out = out[:ls2 + 1] + body + out[le2:]
        # 6. boot block (same machinery, struct BootData)
        bs, be = find_block(out, r'const\s+BootData\s+_bootConfig\s+PROGMEM\s*=\s*\{')
        if bs is not None:
            bi = out[bs + 1:be]
            indent = indent_of(bi[1]) if len(bi) > 1 else '        '
            bmodel = BlockModel(bi, self.master, self.master.boot, indent, self.comments_mode)
            gf = {}
            seen = set()
            for field, _ in self.master.boot:
                hdr = self.master.header.get(field)
                if hdr is not None:
                    lab = self.master.label(hdr)
                    gf[field] = lab not in seen
                    seen.add(lab)
                else:
                    gf[field] = False
            bmis = [f for f, _ in self.master.boot if f not in bmodel.values]
            if bmis:
                self.note(f"  _bootConfig: wrote {len(bmis)} missing field(s): "
                          f"{', '.join('.' + f for f in bmis)}")
            boot_body = bmodel.emit(gf)
            self._report_comments(bmodel, '_bootConfig')
            out = out[:bs + 1] + boot_body + out[be:]

        new_text = '\n'.join(out)
        if new_text != self.text:
            self.changed = True
            self.out = new_text
        return self.changed


def run_clean(dry_run, script_dir, comments_mode='keep'):
    master_path = os.path.join(script_dir, MASTER_REL)
    master = Master(master_path)
    print(f"master: {os.path.relpath(master_path)}\n"
          f"        {len(master.layout)} layout fields, {len(master.boot)} boot fields\n")
    print("comments: " + comments_mode + {
        'keep':   " - every trailing comment is left exactly as the file has it\n",
        'fill':   " - the master's comment is added where the conf has none\n",
        'master': " - added where the conf has none, re-texted where the two differ\n",
    }[comments_mode])

    files = sorted(f for f in glob.glob(os.path.join(script_dir, 'display*conf.h'))
                   if not f.endswith('.new.h'))
    if not files:
        die("no display*conf.h files found next to this script")

    temps, unchanged, problems = [], [], []
    for path in files:
        cf = ConfFile(path, master, comments_mode)
        changed = cf.run()
        base = os.path.basename(path)
        if cf.notes:
            print(f"{base}")
            for n in cf.notes:
                print(n if n.startswith('  ') else '  ' + n)
            print()
        if cf.needs_hand:
            for n in cf.needs_hand:
                print('  ' + '!' * 66)
                print(f"  !! SERIOUS: {base}:")
                for part in n.strip().split('\n'):
                    print(f"  !!   {part}")
                print('  !!   Nothing was removed and no layout index moved.  Fix by hand, then re-run.')
                print('  ' + '!' * 66)
            print()
            problems.append(base)
        if changed:
            temp = path + '.new.h'
            write_text(temp, cf.out)
            temps.append((path, temp, base))
            print(f"  wrote {os.path.basename(temp)}\n")
        else:
            unchanged.append(base)

    print("=" * 72)
    print(f"unchanged : {len(unchanged)}" + (" (" + ', '.join(unchanged) + ")" if unchanged else ""))
    print(f"to update : {len(temps)}" + (" (" + ', '.join(b for _, _, b in temps) + ")" if temps else ""))
    if problems:
        print(f"NEEDS HAND EDITING: {', '.join(problems)}")
    print("=" * 72)

    if not temps:
        print("\nNothing to do.")
        return

    if dry_run:
        print("\nDRY RUN - the originals were not touched.  Temp files written:")
        for _, temp, _ in temps:
            print(f"  {os.path.basename(temp)}")
        if ask("Delete the temp files now? [y/N]: ", False):
            for _, temp, _ in temps:
                os.remove(temp)
            print("Temp files deleted.")
        else:
            print("Kept.  Read them in the editor, then re-run without --dry-run to install.")
        return

    if not ask(f"Update all {len(temps)} conf file(s)? [y/N]: ", False):
        if ask("Delete the temp files instead? [y/N]: ", False):
            for _, temp, _ in temps:
                os.remove(temp)
            print("Temp files deleted.")
        else:
            print("Kept - nothing was installed.")
        return

    for path, temp, _ in temps:
        shutil.copy2(temp, path)
        os.remove(temp)
    print(f"\nUpdated {len(temps)} file(s).  Rebuild to confirm.")


def ask(prompt, default):
    try:
        answer = input(prompt).strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return default
    if not answer:
        return default
    return answer.startswith('y')


# ==============================================================================
#  Import pass (from importlayout.py, now master-driven)
# ==============================================================================

def _extract_title_fix_from_text(text):
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        m_if = re.match(r'#if\s+BITRATE_FULL\s*$', s)
        m_ifndef = re.match(r'#ifndef\s+BITRATE_FULL\s*$', s)
        m_ifdef = re.match(r'#if\s+defined\s*\(\s*BITRATE_FULL\s*\)\s*$', s)
        if m_if or m_ifndef or m_ifdef:
            true_branch = (m_if or m_ifdef)
            nesting, j, else_idx, endif_idx = 1, i + 1, -1, -1
            while j < len(lines):
                ss = lines[j].strip()
                if re.match(r'#if', ss):
                    nesting += 1
                elif re.match(r'#endif', ss):
                    nesting -= 1
                    if nesting == 0:
                        endif_idx = j
                        break
                elif re.match(r'#else', ss) and nesting == 1:
                    else_idx = j
                j += 1
            if endif_idx == -1:
                i += 1
                continue
            if true_branch:
                start, end = i + 1, (else_idx if else_idx != -1 else endif_idx)
            else:
                start, end = ((else_idx + 1) if else_idx != -1 else endif_idx + 1), endif_idx
            for k in range(start, end):
                tm = re.match(r'#define\s+TITLE_FIX\s+(\d+)', lines[k].strip())
                if tm:
                    return int(tm.group(1))
            i = endif_idx + 1
        else:
            i += 1
    return None


def _parse_configs(text, boombox_active):
    bbm = re.search(r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', text)
    boombox_in_if = True
    if bbm:
        boombox_in_if = not bbm.group(1).startswith('ndef')
    configs = []
    in_block, in_active_branch = False, True
    for line in text.split('\n'):
        s = line.strip()
        if re.match(r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', s):
            in_block = True
            in_active_branch = boombox_active if boombox_in_if else not boombox_active
            continue
        if in_block and s.startswith('#else'):
            in_active_branch = not in_active_branch
            continue
        if in_block and s.startswith('#endif'):
            in_block = False
            in_active_branch = True
            continue
        if not in_active_branch:
            continue
        s_clean = re.sub(r'/\*.*?\*/', '', s)
        m = re.match(r'const\s+(\w+)\s+(\w+)\s+PROGMEM\s*=\s*(.+?);\s*(?://.*)?$', s_clean)
        if m:
            configs.append((NAME_MAP.get(m.group(2), m.group(2)), m.group(3).strip()))
    return configs


def parse_conf(path):
    text = read_text(path)
    lines = text.split('\n')
    dw = re.search(r'#define\s+DSP_WIDTH\s+(\d+)', text)
    dh = re.search(r'#define\s+DSP_HEIGHT\s+(\d+)', text)
    width = int(dw.group(1)) if dw else None
    height = int(dh.group(1)) if dh else None

    header_lines = []
    for line in lines:
        if line.strip().startswith('#ifndef '):
            break
        header_lines.append(line)
    header = '\n'.join(header_lines).rstrip()

    guard = 'UNKNOWN'
    for line in lines:
        m = re.search(r'#ifndef\s+(\w+)', line)
        if m:
            guard = m.group(1)
            break

    defines, defined_guard = [], False
    for line in lines:
        s = line.strip()
        if s == f'#ifndef {guard}':
            continue
        if s == f'#define {guard}':
            defined_guard = True
            continue
        if not defined_guard:
            continue
        if s.startswith('#define') or s.startswith('#if') or s.startswith('#ifndef') or s.startswith('#else') or s.startswith('#endif'):
            defines.append(line)
        elif s.startswith('const ') or s.startswith('//const ') or s.startswith('// const '):
            break
        elif s and not s.startswith('/*') and not s.startswith('*') and not s.startswith('//'):
            break
    filtered, in_block, block_depth = [], False, 0
    for d in defines:
        s = d.strip()
        if re.match(r'#if\s+BITRATE_FULL|#ifndef\s+BITRATE_FULL|#if\s+defined\s*\(\s*BITRATE_FULL\s*\)', s):
            in_block = True
            block_depth = 1
            continue
        if in_block:
            if re.match(r'#if', s):
                block_depth += 1
            elif re.match(r'#endif', s):
                block_depth -= 1
                if block_depth == 0:
                    in_block = False
            continue
        filtered.append(d)
    defines = filtered

    hidden_fields = set()
    for m in re.finditer(r'^#define\s+(HIDE_\w+)', text, re.MULTILINE):
        if m.group(1) in HIDE_TO_FIELD:
            hidden_fields.add(HIDE_TO_FIELD[m.group(1)])

    true_map = {}
    for m in re.finditer(r'^#define\s+(\w+)(?:\s+(\S+))?', text, re.MULTILINE):
        def_name, def_val = m.group(1), m.group(2)
        if def_name in TRUE_DEFINES and (def_val is None or def_val.lower() not in ('false', '0')):
            true_map[TRUE_DEFINES[def_name]] = True

    has_boombox = bool(re.search(
        r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', text))
    boombox_mandatory = (not has_boombox) and bool(
        re.search(r'^#define\s+BOOMBOX_STYLE\s*$', text, re.MULTILINE))
    title_fix = _extract_title_fix_from_text(text)

    configs_normal = _parse_configs(text, boombox_active=False)
    configs_boombox = _parse_configs(text, boombox_active=True) if has_boombox else None
    for cfgs in (configs_normal, configs_boombox):
        if cfgs is not None and true_map:
            cfgs.extend([(k, 'true') for k in true_map])
    if title_fix is not None:
        for cfgs in (configs_normal, configs_boombox):
            if cfgs is None:
                continue
            for i, (n, v) in enumerate(cfgs):
                cfgs[i] = (n, v.replace('TITLE_FIX', str(title_fix)))

    str_lines, in_strings = [], False
    for line in text.split('\n'):
        if '/* STRINGS' in line or '// STRINGS' in line:
            in_strings = True
            continue
        if in_strings:
            if line.strip().startswith('#endif') or (line.strip().startswith('/*') and 'STRINGS' not in line):
                break
            if 'const char' in line:
                str_lines.append(line)

    return {
        'width': width, 'height': height,
        'header': header, 'guard': guard, 'defines': defines,
        'configs_normal': configs_normal, 'configs_boombox': configs_boombox,
        'str_lines': str_lines, 'has_boombox': has_boombox,
        'boombox_mandatory': boombox_mandatory,
        'hidden_fields': hidden_fields, 'title_fix': title_fix, 'text': text,
    }


def emit_import_entry(name, configs, master, hidden_fields, boombox_style=False):
    """One conforming layout entry: every master field, in master order."""
    boot_names = set(f for f, _ in master.boot)
    vals = {}
    for n, v in configs:
        if n not in boot_names:
            vals[n] = v
    gf = master.group_first_field()
    out = [f'    {{   // {name}']
    indent = '        '
    converted, not_found = 0, 0
    for field, ftype in master.layout:
        hdr = master.header.get(field)
        if hdr is not None and gf.get(field):
            out.append(f'{indent}{hdr}')
        if ftype == 'bool':
            # Written either way, so every entry lists every field.  A BoomBox variant carries the
            # flag itself: the old format spelled it BOOMBOX_STYLE rather than a field.
            on = (field == 'boomboxVU' and boombox_style) or str(vals.get(field, '')).lower() == 'true'
            if field == 'boomboxVU' and on:
                out.append(f'{indent}/* BOOMBOX VU: middle-out */')
            out.append(f'{indent}.{field:19s} = {"true" if on else "false"},'
                       + ((' ' + master.comment[field]) if master.comment.get(field) else ''))
            continue
        if field in hidden_fields:
            out.append(f'{indent}.{field:19s} = {{ }}, // unused')
            if field in vals:
                out.append(f'{indent}// .{field:17s} = {normalise_value(vals[field])},')
            continue
        if field in vals:
            value = normalise_value(vals[field])
            if field == 'bandsConf':
                value = trim_bands_conf(value)
            out.append(f'{indent}.{field:19s} = {value},')
            converted += 1
        else:
            out.append(f'{indent}.{field:19s} = {{ }},                                                   // <--------- NEEDS EDITING!')
            not_found += 1
    out.append('    },')
    return '\n'.join(out), converted, not_found


META_HAIRLINE_MAX = 3   # a fill this thin is a rule under the title; anything thicker is a band


def _fill_height(value):
    """The height of a FillConfig initialiser: 0 for an empty one, the number for a plain integer
    height, or None when the height is an expression we cannot read.

    The initialiser is `{ { left, top, fontsize, align }, width, height, outlined }`, so the widget
    group is skipped and the remaining fields are split: width first, then the height.  The regex the
    old oled_swap() used for this (`}, <digits>, <bool>}`) cannot match that shape at all - the width
    sits between the brace and the height - which is why its height test never fired and only an
    empty metaBGConfInv ever triggered a swap.
    """
    v = value.split('//')[0].strip()
    if re.fullmatch(r'\{\s*\}', v):
        return 0                                  # an empty fill: present, but draws nothing
    inner = re.search(r'\{\s*\{[^}]*\}\s*,([^}]*)\}', v)
    if not inner:
        return None
    parts = [p.strip() for p in inner.group(1).split(',')]
    if len(parts) < 2:
        return None
    return int(parts[1]) if re.fullmatch(r'\d+', parts[1]) else None


def normalise_meta_pair(data, oled):
    """OLED targets only: sort the meta fill pair the way ehRadio draws it.

    ehRadio selects metaBGConfInv when invert title is on, and OLEDs default to inverted, so on an OLED
    the bar lives in metaBGConfInv and the hairline in metaBGConf.  A yoRadio conf is the other way
    round - and identically so for an OLED and for a TFT, which is why the family cannot be read out of
    a source file and has to be passed in.  So the pair is sorted by height: the hairline (height
    <= META_HAIRLINE_MAX) belongs in metaBGConf, the rectangle in metaBGConfInv.

    Nothing is guessed at: two hairlines or two rectangles are left as they are, so is a pair whose
    heights are not plain integers, and when only one of the two is present it is moved to the slot its
    own height calls for (the emitter writes the other as `{ }`).
    """
    if not oled:
        return
    for cfgs in (data['configs_normal'], data['configs_boombox']):
        if cfgs is None:
            continue
        bg = inv = None
        for i, (n, v) in enumerate(cfgs):
            if n == 'metaBGConf':
                bg = (i, v)
            elif n == 'metaBGConfInv':
                inv = (i, v)
        if bg and inv:
            hb, hi = _fill_height(bg[1]), _fill_height(inv[1])
            if hb is None or hi is None:
                print("  note: metaBGConf/metaBGConfInv heights are not plain integers - left as they are, check by hand")
                continue
            if (hb <= META_HAIRLINE_MAX) == (hi <= META_HAIRLINE_MAX):
                continue        # both rules or both bands: nothing to sort
            if hb <= META_HAIRLINE_MAX:
                continue        # already the ehRadio way round
            cfgs[bg[0]], cfgs[inv[0]] = ('metaBGConf', inv[1]), ('metaBGConfInv', bg[1])
            data['meta_swapped'] = True
            print("  meta pair sorted: hairline to metaBGConf, bar to metaBGConfInv")
        elif bg or inv:
            name, idx, value = ('metaBGConf', bg[0], bg[1]) if bg else ('metaBGConfInv', inv[0], inv[1])
            h = _fill_height(value)
            if h is None:
                print(f"  note: the lone {name} has no readable height - left where it is")
                continue
            hairline = h <= META_HAIRLINE_MAX
            if hairline == (name == 'metaBGConf'):
                continue        # already in the slot its height calls for
            cfgs[idx] = ('metaBGConf' if hairline else 'metaBGConfInv', value)
            data['meta_swapped'] = True
            print(f"  lone {name} moved to {cfgs[idx][0]}")


def _fmt_string(sname, sval):
    suffix = '[][8]' if sname == 'batteryRangeFmt' else '[]'
    decl = f'const char {sname}{suffix}'
    return f'{decl}{" " * (35 - len(decl))} PROGMEM = {sval};'


def create_target_file(out_path, data, name, master):
    """A brand new conf file: defines, the master's BootData block, _layoutNames, the entries and
    the STRINGS section.  Every field is written, so the file is conforming from the start."""
    w, h = data['width'], data['height']
    guard = os.path.basename(out_path).replace('.h', '') + '_h'
    out = ['/*************************************************************************************',
           f'    TFT{w}x{h} displays configuration file.',
           '*************************************************************************************/',
           '',
           f'#ifndef {guard}',
           f'#define {guard}',
           '']
    out += [d.rstrip() for d in data['defines']]
    out += ['', '// ******************** CHECK ALL #define LINES CAREFULLY! ********************', '']

    vals = {n: v for n, v in data['configs_normal']}
    boot, seen = ['const BootData _bootConfig PROGMEM = {'], set()
    for field, _t in master.boot:
        hdr = master.header.get(field)
        if hdr is not None:
            lab = master.label(hdr)
            if lab not in seen:
                boot.append('        ' + hdr)
                seen.add(lab)
        if field == 'apTitleBGConf':
            # The AP and SD-manager screens draw this band themselves and never read a layout, so it is
            # derived from the pair the source provided, after normalise_meta_pair() has sorted it: the
            # band on a TFT, the hairline on an OLED.
            band = vals.get('metaBGConf')
            if band is None:
                boot.append(f'        .{field:19s} = {{ }},   // no band: the layout has no metaBGConf')
            else:
                note = ('// was metaBGConfInv (ehRadio flips yoRadio\'s metaBGConf to metaBGConfInv)'
                        if data.get('meta_swapped') else '// from metaBGConf')
                boot.append(f'        .{field:19s} = {normalise_value(band)},   {note}')
            continue
        boot.append(f'        .{field:19s} = ' +
                    (f'{normalise_value(vals[field])},' if field in vals else '{ },'))
    boot.append('};')
    out += ['\n'.join(boot), '']

    if data['has_boombox']:
        labels = [trunc_name(name), trunc_name(f'{name} (BoomBox)')]
        combos = [(labels[0], data['configs_normal'], False),
                  (labels[1], data['configs_boombox'], True)]
    elif data['boombox_mandatory']:
        labels = [trunc_name(name)]
        combos = [(labels[0], data['configs_normal'], True)]
    else:
        labels = [trunc_name(name)]
        combos = [(labels[0], data['configs_normal'], False)]

    out.append('const char _layoutNames[][64] PROGMEM = {')
    out += [f'    "{n}",' for n in labels]
    out.append('};')
    out += ['', '/* LAYOUT DEFINITIONS */', '', 'const LayoutData _layouts[] PROGMEM = {']
    for label, cfgs, bb in combos:
        et, conv, nf = emit_import_entry(label, cfgs, master, data['hidden_fields'], bb)
        out.append(et)
        print(f"  {label}: {conv} values from the source, {nf} needing editing")
    out.append('};')

    out += ['// ******************** CHECK ALL const char LINES CAREFULLY! ********************',
            '// check all needed strings are present... and double-check octal codes \\0xx too!',
            '// Note that rssi and battery will still render 2-glyph icons even when blank',
            '', '/* STRINGS */']
    existing = set()
    for s in data['str_lines']:
        m = re.match(r'const\s+char\s+(\w+)\[', s.strip())
        if not m:
            continue
        sname = m.group(1)
        existing.add(sname)
        vm = re.search(r'=\s*(.+?);(?:\s*(?://|/\*).*)?$', s.strip())
        out.append(_fmt_string(sname, vm.group(1).strip() if vm else '""'))
    if existing:
        out.append('')
    missing = [k for k in REQUIRED_STRINGS if k not in existing]
    if missing:
        out.append('// Automatically added by conf_tool.py:')
        for k in missing:
            out.append(_fmt_string(k, REQUIRED_STRINGS[k]))
        out.append('')
    out.append('#endif')
    out.append('')
    return '\n'.join(out)


def finish(out_path, new_text, dry_run, out_name):
    """Write the *.new.h temp, then let the user decide whether it replaces the original."""
    temp = out_path + '.new.h'
    write_text(temp, new_text)
    print(f"\nwrote {os.path.basename(temp)}")
    if dry_run:
        if ask("Delete the temp file now? [y/N]: ", False):
            os.remove(temp)
            print("Temp file deleted.")
        else:
            print(f"Kept: {temp}")
        return
    if ask(f"Update {out_name}? [y/N]: ", False):
        shutil.copy2(temp, out_path)
        os.remove(temp)
        print(f"Updated {out_name}.")
    elif ask("Delete the temp file instead? [y/N]: ", False):
        os.remove(temp)
        print("Temp file deleted.")


def run_import(community_path, name, target, dry_run, script_dir):
    master = Master(os.path.join(script_dir, MASTER_REL))
    if not os.path.exists(community_path):
        die(f"file not found: {community_path}")
    data = parse_conf(community_path)
    if data['width'] is None or data['height'] is None:
        die("DSP_WIDTH/DSP_HEIGHT not found - is this a valid conf file?")

    names = {n for n, _ in data['configs_normal']}
    missing = [f for f in MANDATORY_LAYOUT_FIELDS if f not in names]
    if missing:
        die(f"missing mandatory widget(s): {', '.join(missing)} - is this a valid conf file?")

    basename = os.path.basename(community_path)
    if target:
        out_name = os.path.basename(target)
        target_full = os.path.join(script_dir, out_name)
        oled = 'OLED' in out_name.upper()
        if not oled and 'TFT' not in out_name.upper() and not os.path.exists(target_full):
            oled = ask(f"Creating {out_name}, which says neither TFT nor OLED. Is this display an OLED? [y/N]: ", False)
    else:
        TARGET_RE = re.compile(r'^display(TFT|OLED)(\d{2,4})x(\d{2,4})conf\.h$')
        cands = [f for f in os.listdir(script_dir)
                 if TARGET_RE.match(f) and int(TARGET_RE.match(f).group(2)) == data['width']
                 and int(TARGET_RE.match(f).group(3)) == data['height']]
        if len(cands) == 1:
            out_name = cands[0]        # an existing conf answers it: the filename is the family
            oled = 'OLED' in out_name.upper()
        else:
            # A new conf.  No file here matches the source size, and the source cannot say which family
            # it is for - a yoRadio OLED conf and a yoRadio TFT conf are the same band-plus-rule pair -
            # so ask.  The answer names the file as well as deciding the meta pair order.
            oled = ask(f"No conf here matches {data['width']}x{data['height']}. Is this display an OLED? [y/N]: ", False)
            out_name = f"display{'OLED' if oled else 'TFT'}{data['width']}x{data['height']}conf.h"
        target_full = os.path.join(script_dir, out_name)
    normalise_meta_pair(data, oled)

    if not os.path.exists(target_full):
        print(f"\nCreating {out_name} from {basename} as \"{name}\"...")
        finish(target_full, create_target_file(target_full, data, name, master), dry_run, out_name)
        return

    print(f"\nImporting {basename} into {out_name} as \"{name}\"...")
    tc = read_text(target_full)
    ls, le = find_block(tc.split('\n'), r'_layouts\[\]\s+PROGMEM\s*=\s*\{')
    if ls is None:
        die(f"{out_name} has no _layouts[]")

    entries = []
    labels = []
    if data['has_boombox']:
        combos = [(name, data['configs_normal'], False, ''),
                  (f'{name} (BoomBox)', data['configs_boombox'], True, ' (BoomBox)')]
    elif data['boombox_mandatory']:
        combos = [(name, data['configs_normal'], True, ' (BoomBox mandatory)')]
    else:
        combos = [(name, data['configs_normal'], False, '')]
    for lname, cfgs, bb, _suffix in combos:
        et, conv, nf = emit_import_entry(trunc_name(lname), cfgs, master, data['hidden_fields'], bb)
        entries.append(et)
        labels.append(trunc_name(lname))
        print(f"  {trunc_name(lname)}: {conv} values from the source, {nf} needing editing")

    lines = tc.split('\n')
    ns, ne = find_block(lines, r'_layoutNames\[\]\[\d+\]\s+PROGMEM\s*=\s*\{')
    if ns is None:
        die(f"{out_name} has no _layoutNames[]")
    old_names = [l.strip().strip(',').strip('"') for l in lines[ns + 1:ne] if l.strip()]
    new_names = old_names + labels
    names_block = [lines[ns]] + [f'    "{n}",' for n in new_names] + ['};']
    out = lines[:ns] + names_block + lines[ne + 1:]
    shift = len(names_block) - (ne - ns + 1)
    ls2, le2 = ls + shift, le + shift
    out = out[:le2] + entries + out[le2:]

    finish(target_full, '\n'.join(out), dry_run, out_name)


# ==============================================================================
#  main
# ==============================================================================

def main():
    argv = sys.argv[1:]
    if '-h' in argv or '--help' in argv:
        print(__doc__)
        return
    if not argv:
        print("conf_tool.py - no mode given, nothing was done.")
        print("  py conf_tool.py --clean [--comments=keep|fill|master] [--dry-run]")
        print("  py conf_tool.py --import <conf_file> --name \"Name\" [--dry-run]")
        print("Run with --help for the full help (also the top of this file).")
        sys.exit(2)

    clean = '--clean' in argv
    do_import = '--import' in argv
    dry_run = '--dry-run' in argv
    comments = 'keep'
    name = target = source = None
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--name', '-n'):
            if i + 1 >= len(argv):
                die("--name requires a value")
            name = argv[i + 1]; i += 2; continue
        if a.startswith('--name='):
            name = a.split('=', 1)[1]; i += 1; continue
        if a == '--target':
            if i + 1 >= len(argv):
                die("--target requires a value")
            target = argv[i + 1]; i += 2; continue
        if a == '--import':
            if i + 1 >= len(argv):
                die("--import requires a conf file")
            source = argv[i + 1]; i += 2; continue
        if a == '--comments':
            if i + 1 >= len(argv):
                die("--comments requires a value: " + ' or '.join(COMMENT_MODES))
            comments = argv[i + 1]; i += 2; continue
        if a.startswith('--comments='):
            comments = a.split('=', 1)[1]; i += 1; continue
        if a in ('--clean', '--dry-run'):
            i += 1; continue
        die(f"unknown argument: {a}")
    if comments not in COMMENT_MODES:
        die(f"unknown --comments value '{comments}' - use one of: {', '.join(COMMENT_MODES)}")
    if do_import and comments != 'keep':
        die("--comments applies to --clean: --import already writes the master's comments")
    if clean and do_import:
        die("--clean and --import are separate modes - run one at a time")
    if do_import and not name:
        die("--import requires --name \"Name\"")
    script_dir = os.path.dirname(os.path.abspath(__file__))
    if clean:
        run_clean(dry_run, script_dir, comments)
    elif do_import:
        run_import(source, name, target, dry_run, script_dir)
    else:
        die("no mode given - use --clean or --import (see --help)")


if __name__ == '__main__':
    main()
