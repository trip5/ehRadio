#!/usr/bin/env python3
"""
theme_tool.py - one tool for the WebUI palette files and the themes.h list they belong to.

USAGE:
    py theme_tool.py                          # audit: structure, names, quantise status
    py theme_tool.py -a | --audit             # the same, spelled out
    py theme_tool.py -i FILE [--name NAME]    # import a theme into themes.h AND themes.css
    py theme_tool.py -q | --quantise          # move the colours in themes.h onto the 565 steps
    py theme_tool.py -d [INDEX]               # delete a theme from both files
    py theme_tool.py --inflate                # every block spells its colours out
    py theme_tool.py --deflate                # drop what repeats :root, fold families back to a head
    py theme_tool.py --deflate-lossy          # the same, asking [y/N] for near-matches
    py theme_tool.py -h | --help

WHERE THE COLOURS LIVE:
    data/www/style.css      `:root` is the design - the families, their aliases, and every rule.  Read here and
                            never written here.
    data/www/themes.css     one `:root[data-theme="N"]` block per display theme, holding only what differs from
                            `:root`.  Written by the import, delete and maintenance modes and by the browser tool.
    src/displays/themes.h   `_themeNames[]` and the display palettes.  The authority for the theme list: how many
                            themes there are and what they are called.

MODES:
    (default), -a, --audit  Read-only health check: are the two files describing the same themes, are the names
                            current, and is themes.h already on the 565 steps?  Nothing is written.
    -i, --import FILE       The theme file's colours go into a new themes.h entry AND into the themes.css block
                            that entry belongs to, so the two can never drift apart.  The block is written
                            deflated, with the offset measured from display theme [0].  Without --name the tool
                            prints the name the file carries and asks whether to use it, because every theme
                            should read as "Name (Creator)"; refusing writes nothing and prints the command that
                            would name it.
    -q, --quantise          themes.h: Rewrite the RGB() values already in themes.h onto the 565 steps.  Cannot change a
                            pixel; a value already on the grid is left alone, so a second run does nothing.
    -d, --delete [INDEX]    Remove a theme from both files.  An index IS a position in themes.h, so every theme
                            above it is renumbered by one in both.
    --inflate               themes.css: Give every block every role it lacks, at the colour it already resolves to.
    --deflate               themes.css: Remove every declaration whose resolved colour is exactly the :root one, then fold
                            each family back into one head line.
    --deflate-lossy         themes.css: The same for values close to :root, one [y/N] per role per theme, default No.

ROOT IS THE ANCHOR:
    A block lists only its differences from `:root`, so `:root` is what everything is measured against: there is
    no second reference to name and no id to pass.  The generator does need one fixed yardstick for its offset
    and uses display theme [0], the theme `:root` was designed for - which is why a theme resembling it needs
    only a dozen lines.  To re-establish that after changing the design: --inflate, edit `:root`, --deflate.

EXAMPLES:
    py theme_tool.py
    py theme_tool.py -i krzsieksik_theme_gray.h --name "Gray (Krzsiek)"
    py theme_tool.py -i ehRadioThemes-Backup.json
    py theme_tool.py -q
    py theme_tool.py -d 3
    py theme_tool.py --deflate
"""

import json
import os
import re
import sys

# This terminal is not always UTF-8 and the theme names are not always ASCII.
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')


HERE = os.path.dirname(os.path.abspath(__file__))
THEMES_H = os.path.join(HERE, 'themes.h')
STYLE_CSS = os.path.normpath(os.path.join(HERE, '..', '..', 'data', 'www', 'style.css'))
THEMES_CSS = os.path.normpath(os.path.join(HERE, '..', '..', 'data', 'www', 'themes.css'))


def fail(message):
    print(f"ERROR: {message}")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Reading and writing
# ---------------------------------------------------------------------------

def read_text(path):
    """(text, eol).  newline='' keeps the file's own line endings, which every write then reproduces."""
    if not os.path.exists(path):
        fail(f"{path} not found.")
    with open(path, 'r', encoding='utf-8', newline='') as f:
        text = f.read()
    return text, ('\r\n' if '\r\n' in text else '\n')


def write_text(path, text):
    with open(path, 'w', encoding='utf-8', newline='') as f:
        f.write(text)


# ---------------------------------------------------------------------------
# CSS
# ---------------------------------------------------------------------------

MASTER_RE = re.compile(r':root\s*\{')
BLOCK_RE = re.compile(r':root\[data-theme="(\d+)"\]\s*\{')
# A palette declaration, which is the only kind of declaration this project owns: a hex colour or an alias.
PROP_RE = re.compile(r'--([A-Za-z0-9-]+)\s*:\s*(#[0-9A-Fa-f]{3,8}|var\(\s*--[A-Za-z0-9-]+\s*\))')
VAR_RE = re.compile(r'var\(\s*--([A-Za-z0-9-]+)\s*\)')
# The one declaration that is not a colour: a block's name, quoted so a title with a colon still parses.
NAME_RE = re.compile(r'--theme-name\s*:\s*"((?:[^"\\]|\\.)*)"')

#: The head of themes.css, written by every full rewrite.  webui_tool.html carries the same lines as a JS
#: constant - the browser tool has to regenerate this file without ever seeing this one.
HEADER_LINES = [
    '/* WEBUI THEMES - the per-display-theme overrides.',
    "   One block per entry in _themes[] of src/displays/themes.h, keyed by that entry's index.  Every colour a",
    '   block does not name comes from :root in style.css, which is the design, so a block stays a short list of',
    '   differences rather than a second palette.  --theme-name is a copy of _themeNames[] so a page can show the',
    '   name it belongs to: themes.h is the authority and theme_tool.py refreshes these on every run.',
    '   style.css imports this file.  It is written by src/displays/theme_tool.py and by the browser tool',
    '   in src/displays/webui_tool.html, which can only see values and therefore rewrites the file whole. */',
]


def brace_end(text, open_pos):
    """Index just past the } matching text[open_pos] == '{'.  Comments and strings are skipped."""
    depth, i = 0, open_pos
    while i < len(text):
        if text.startswith('/*', i):
            end = text.find('*/', i + 2)
            if end == -1:
                return -1
            i = end + 2
            continue
        ch = text[i]
        if ch == '{':
            depth += 1
        elif ch == '}':
            depth -= 1
            if depth == 0:
                return i + 1
        elif ch in '"\'':
            quote = ch
            i += 1
            while i < len(text) and text[i] != quote:
                i += 2 if text[i] == '\\' else 1
        i += 1
    return -1


def validate_props(text, path):
    """Every palette declaration must begin its line.

    A stray character in front of `--role:` makes the declaration invalid CSS, and the browser drops it
    silently: the role then falls back to whatever its alias resolves to, which is very hard to see.
    Catching it at read time is the difference between a five-second error and a colour hunt.
    """
    spans = [(m.start(), m.end()) for m in re.finditer(r'/\*.*?\*/', text, re.S)]

    def in_comment(pos):
        return any(a <= pos < b for a, b in spans)

    for m in PROP_RE.finditer(text):
        if in_comment(m.start()):
            continue
        line_start = text.rfind('\n', 0, m.start()) + 1
        prefix = text[line_start:m.start()]
        if prefix.strip():
            line = text.count('\n', 0, m.start()) + 1
            fail(f"{os.path.basename(path)} line {line}: unexpected {prefix.strip()!r} before --{m.group(1)}.\n"
                 f"       A palette declaration must start its line; only indentation is allowed.")


def parse_master(text):
    """The `:root` block as {name: value}, values not yet resolved."""
    m = MASTER_RE.search(text)
    if not m:
        fail("Could not find the :root palette in style.css.")
    end = brace_end(text, m.end() - 1)
    if end == -1:
        fail("Could not find the end of the :root block.")
    return dict(PROP_RE.findall(text[m.end():end - 1]))


def parse_blocks(text):
    """Every theme block: id, name, props, and the span it occupies.

    `props` holds colours only - --theme-name is the name, not a colour - so a caller may edit props and
    hand the dict straight back to render_block().
    """
    blocks = []
    for bm in BLOCK_RE.finditer(text):
        open_pos = bm.end() - 1
        end = brace_end(text, open_pos)
        if end == -1:
            fail(f"Unclosed theme block at offset {bm.start()} in themes.css.")
        body = text[open_pos + 1:end - 1]
        nm = NAME_RE.search(body)
        blocks.append({
            'id': int(bm.group(1)),
            'name': nm.group(1) if nm else None,
            'props': dict(PROP_RE.findall(body)),
            'start': bm.start(),
            'end': end,
        })
    return blocks


def css_string(text):
    return '"' + text.replace('\\', '\\\\').replace('"', '\\"') + '"'


def render_block(theme_id, name, props, eol):
    """One block: the name, then every colour, alphabetical - the same order `:root` uses."""
    lines = [f':root[data-theme="{theme_id}"] {{',
             '  ' + '--theme-name:'.ljust(22) + css_string(name) + ';']
    for role in sorted(props):
        lines.append('  ' + ('--' + role + ':').ljust(22) + props[role] + ';')
    lines.append('}')
    return eol.join(lines)


def render_file(names, props_by_id, eol):
    """The whole of themes.css: the header, then one block per display theme, in id order."""
    out = list(HEADER_LINES)
    for i, name in enumerate(names):
        out.append(render_block(i, name, props_by_id.get(i, {}), eol))
    return eol.join(out) + eol


def apply_edits(text, edits):
    """Apply (start, end, replacement) edits back to front, so every offset stays valid."""
    for start, end, replacement in sorted(edits, key=lambda e: e[0], reverse=True):
        text = text[:start] + replacement + text[end:]
    return text


# ---------------------------------------------------------------------------
# Colour resolution - no decisions, only "what colour does this role have"
# ---------------------------------------------------------------------------

def normalise_hex(value):
    """#abc -> #aabbcc, lower case; the form every comparison uses."""
    v = value.strip().lower().lstrip('#')
    if len(v) == 3:
        v = ''.join(ch * 2 for ch in v)
    return '#' + v


def resolve_value(value, props, root, seen=()):
    """A role's value as hex: follow var(--x) through the block first, then :root.

    Returns None when the chain reaches a name neither place declares, so a comparison can treat it
    as "different" rather than crash on a hand edit.
    """
    value = value.strip()
    m = VAR_RE.fullmatch(value)
    if not m:
        return normalise_hex(value) if value.startswith('#') else None
    name = m.group(1)
    if name in seen:
        fail(f"CSS variable loop: {' -> '.join(seen + (name,))}")
    if name in props:
        return resolve_value(props[name], props, root, seen + (name,))
    if name in root:
        return resolve_value(root[name], props, root, seen + (name,))
    return None


def resolve_master(master):
    """The :root colours as hex, aliases followed."""
    return {name: resolve_value(value, {}, master) for name, value in master.items()}


def resolve_block(block, master):
    """One block's declared colours as hex, its own declarations taking precedence."""
    return {name: resolve_value(value, block['props'], master)
            for name, value in block['props'].items()}


# ---------------------------------------------------------------------------
# Families - read from :root itself, never from a list
# ---------------------------------------------------------------------------

# The roles a theme may override: the MEMBERS of the :root families, never a head.  Alphabetical, because
# render_block() sorts too, so a block reads the same way as the family block in `:root`.
# The three hand-owned colours (`page-bg`, `list-bars`, `playlist-hover`) are deliberately absent: no display
# theme carries a colour meaning "the browser window" or "the zebra stripe", so the generator never writes
# them.  A block may still set one by hand - the absence is about derivation, not permission.
ROLE_ORDER = [
    'button-active-fill', 'button-active-text', 'button-fill', 'button-outline', 'button-text',
    'caption-text',
    'control-bg',
    'editor-cell-text', 'editor-grid',
    'eq-label-text',
    'heading-text', 'highlight-fill',
    'icon-active-glyph', 'icons-outline',
    'info-text',
    'input-border', 'input-fill', 'input-text',
    'link-quiet', 'link-text',
    'list-active-bg', 'list-active-text', 'list-names', 'list-outline', 'list-urls',
    'logo', 'logo-shadow', 'logo-text',
    'main-text', 'small-text',
    'meta-text',
    'meter-fill', 'meter-outline', 'meter-track',
    'panel-background', 'panel-border', 'panel-gradient',
    'scrollbar-thumb', 'scrollbar-track',
    'section-title-text',
    'select-fill', 'select-text',
    'spinner-ring',
    'station-name',
    'switch-off-text', 'switch-text', 'switch-thumb', 'switch-track', 'switch-track-on',
    'warning-outline',
]


def find_families(master):
    """head -> its members, read from `:root` itself.

    A head is a name another declaration points at (`--button-fill: var(--fills)`) that is not a role,
    so a family added by hand is picked up with no list to maintain here.
    """
    families = {}
    for name, value in master.items():
        m = VAR_RE.fullmatch(value.strip())
        if not m:
            continue
        head = m.group(1)
        if head in ROLE_ORDER or head not in master:
            continue
        families.setdefault(head, []).append(name)
    for head in families:
        families[head].sort()
    return families


def collapse_families(props, root, families):
    """Turn spelled-out members back into one head line: the inverse of `--inflate`.

    A block may declare a head (`--outlines: #e3d25f`) and let the members inherit.  When a block
    spells the members out instead, and two or more of them resolve to the same colour, one head line
    says the same thing - and the members the block left out join the family, which is exactly the
    short form a derived theme wants.
    A member that resolves differently is a deliberate split and stays declared, so a theme can still
    say "this one outline is not the family outline".

    `props` is edited in place; returns (removed, added).
    """
    removed, added = {}, {}
    for head in sorted(families):
        spelled = [member for member in families[head] if member in props]
        if head in props:
            # The block already says it: drop the members that only repeat it.
            candidate = resolve_value(props[head], props, root)
            if candidate is None:
                continue
            drop = [m for m in spelled if resolve_value(props[m], props, root) == candidate]
            put = None
        else:
            # The colour most of the members agree on IS the family colour; a lone member that says
            # something else is a deliberate split and keeps its own line under the new head.
            groups = {}
            for member in spelled:
                value = resolve_value(props[member], props, root)
                if value is not None:
                    groups.setdefault(value, []).append(member)
            if not groups:
                continue
            best = max(groups, key=lambda value: len(groups[value]))
            winners = groups[best]
            if len(winners) < 2:
                continue
            if sum(1 for value in groups if len(groups[value]) == len(winners)) > 1:
                continue          # a tie says nothing about which colour the family has
            candidate = best
            drop = winners
            put = best
        for member in drop:
            removed[member] = props.pop(member)
        if put is not None:
            props[head] = put
            added[head] = put
    return removed, added


# ---------------------------------------------------------------------------
# themes.h
# ---------------------------------------------------------------------------

FIELD_RE = re.compile(r'\.([A-Za-z0-9_]+)\s*=\s*RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)')
PLAYLIST_RE = re.compile(r'\.playlist\s*=\s*\{([^}]*)\}')


def parse_names(content):
    """Theme names from _themeNames, in order."""
    m = re.search(r'const char _themeNames\[\]\[\d+\] PROGMEM = \{', content)
    if not m:
        fail("Could not find _themeNames in themes.h.")
    stop = content.find('\n};', m.end())
    if stop == -1:
        fail("Could not find the closing } of _themeNames.")
    block = content[m.end():stop]
    return findall_names(block)


def findall_names(block):
    return [m.group(1) for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', block)]


def parse_entries(content):
    """(start, end) of each theme entry in _themes[]."""
    m = re.search(r'const ThemeData _themes\[\]\s*PROGMEM\s*=\s*\{', content)
    if not m:
        fail("Could not find _themes[] in themes.h.")
    entries = []
    search_start = m.end()
    while True:
        em = re.search(r'(?m)^\s+\{\s+//', content[search_start:])
        if not em:
            break
        em_start = search_start + em.start()
        brace_pos = em_start + em.group().index('{')
        depth, i = 1, brace_pos + 1
        while i < len(content) and depth > 0:
            if content[i] == '{':
                depth += 1
            elif content[i] == '}':
                depth -= 1
            i += 1
        if depth != 0:
            search_start = em_start + 1
            continue
        comma = content.find(',', i)
        if comma == -1 or comma > i + 5:
            search_start = em_start + 1
            continue
        entries.append((em_start, comma + 1))
        search_start = comma + 1
    return entries


def parse_fields(entry_text):
    """One entry as {field: (r,g,b)} plus playlist: [(r,g,b), ...]."""
    fields = {}
    for m in FIELD_RE.finditer(entry_text):
        fields[m.group(1)] = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
    pl = PLAYLIST_RE.search(entry_text)
    if pl:
        fields['playlist'] = [(int(a), int(b), int(c)) for a, b, c in
                              re.findall(r'RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)', pl.group(1))]
    return fields


def load_themes():
    """(names, entries, text).  The names and the entries are checked against each other."""
    content, _ = read_text(THEMES_H)
    names = parse_names(content)
    spans = parse_entries(content)
    if len(names) != len(spans):
        fail(f"Name/entry count mismatch in themes.h ({len(names)} names, {len(spans)} entries).")
    themes = [parse_fields(content[s:e]) for s, e in spans]
    for i, t in enumerate(themes):
        for required in ('background', 'title1', 'title2', 'clock', 'metabg'):
            if required not in t:
                fail(f"Theme [{i}] {names[i]!r} has no .{required} field.")
    return names, themes, content


# ---------------------------------------------------------------------------
# The two files together
# ---------------------------------------------------------------------------

def load():
    """(master_raw, blocks, themes_text, eol).  master_raw is :root, unresolved."""
    style_text, _ = read_text(STYLE_CSS)
    validate_props(style_text, STYLE_CSS)
    master = parse_master(style_text)
    themes_text, eol = read_text(THEMES_CSS)
    validate_props(themes_text, THEMES_CSS)
    return master, parse_blocks(themes_text), themes_text, eol


def check_sync(names, blocks):
    """(hard, soft) problems.  Hard ones mean blocks and display themes no longer correspond; soft ones
    are a name that differs, which themes.h always wins."""
    hard, soft = [], []
    ids = [b['id'] for b in blocks]
    if len(blocks) != len(names):
        hard.append(f"themes.h lists {len(names)} theme(s); themes.css has {len(blocks)} block(s).")
    missing = sorted(set(range(len(names))) - set(ids))
    extra = sorted(set(ids) - set(range(len(names))))
    if missing:
        hard.append("themes.css has no block for id(s): " + ', '.join(str(i) for i in missing) + '.')
    if extra:
        hard.append("themes.css has block(s) with no display theme: " + ', '.join(str(i) for i in extra) + '.')
    if ids != sorted(ids):
        hard.append("themes.css blocks are not in id order.")
    seen = set()
    for b in blocks:
        if b['id'] in seen:
            hard.append(f"themes.css has more than one block for id {b['id']}.")
        seen.add(b['id'])
        if b['id'] < len(names) and b['name'] != names[b['id']]:
            soft.append(f"[{b['id']}] themes.css says {b['name']!r}, themes.h says {names[b['id']]!r}.")
    return hard, soft


def require_sync(names, blocks, quiet=False):
    """Print the drift report and stop on anything the tools must not paper over."""
    hard, soft = check_sync(names, blocks)
    if soft and not quiet:
        print(f"Name drift in {len(soft)} block(s) - themes.h is the authority:")
        for line in soft:
            print('  ' + line)
        print("  (theme_tool.py rewrites every --theme-name from themes.h; the browser tool can only echo "
              "the copy it read.)")
    if hard:
        print("\nERROR: themes.h and themes.css do not describe the same set of themes:")
        for line in hard:
            print('  ' + line)
        print("\nHand-edit themes.h or themes.css until the count and the ids agree, then run this again.")
        sys.exit(1)
    if not quiet and not soft:
        print(f"themes.h and themes.css agree: {len(names)} theme(s).")


COLOR_TO_FIELD = {
    'COLOR_BACKGROUND':       'background',
    'COLOR_STATION_NAME':     'meta',
    'COLOR_STATION_BG':       'metabg',
    'COLOR_SNG_TITLE_1':      'title1',
    'COLOR_SNG_TITLE_2':      'title2',
    'COLOR_DIGITS':           'digit',
    'COLOR_DIVIDER':          'div',
    'COLOR_WEATHER':          'weather',
    'COLOR_VU_PEAK':          'vupeak',
    'COLOR_VU_MAX':           'vumax',
    'COLOR_VU_MIN':           'vumin',
    'COLOR_CLOCK':            'clock',
    'COLOR_CLOCK_BG':         'clockbg',
    'COLOR_SECONDS':          'seconds',
    'COLOR_DAY_OF_W':         'dow',
    'COLOR_DATE':             'date',
    'COLOR_CLOCK_SS':         'clockss',
    'COLOR_CLOCK_BG_SS':      'clockbgss',
    'COLOR_SECONDS_SS':       'secondsss',
    'COLOR_DAY_OF_W_SS':      'dowss',
    'COLOR_DATE_SS':          'datess',
    'COLOR_BUFFER':           'buffer',
    'COLOR_IP':               'ip',
    'COLOR_VOLUME_VALUE':     'vol',
    'COLOR_RSSI':             'rssi',
    'COLOR_BATTERY':          'battery',
    'COLOR_BITRATE':          'bitrate',
    'COLOR_VOLBAR_OUT':       'volbarout',
    'COLOR_VOLBAR_IN':        'volbarin',
    'COLOR_PL_CURRENT':       'plcurrent',
    'COLOR_PL_CURRENT_BG':    'plcurrentbg',
    'COLOR_PLAYLIST_0':       'playlist[0]',
    'COLOR_PLAYLIST_1':       'playlist[1]',
    'COLOR_PLAYLIST_2':       'playlist[2]',
    'COLOR_PLAYLIST_3':       'playlist[3]',
    'COLOR_PLAYLIST_4':       'playlist[4]',
}

FIELD_ORDER = [
    'background', 'meta', 'metabg',
    'title1', 'title2', 'digit', 'digitbg', 'div', 'line', 'weather', 'vuaxis',
    'vupeak', 'vumax', 'vumin',
    'clock', 'clockbg', 'seconds', 'secondsbg', 'dow', 'date',
    'clockss', 'clockbgss', 'secondsss', 'secondsbgss', 'dowss', 'datess', 'textss',
    'buffer', 'ip', 'vol', 'rssi', 'battery', 'bitrate',
    'volbarout', 'volbarin',
    'plcurrent', 'plcurrentbg',
    'playlist',
]

SMART_FALLBACK = [
    ('vupeak',  'title1'),
    ('dow',     'date'),
    ('battery', 'rssi'),
]

# Computed fallbacks: (field, source_field, multiplier) — applied after SMART_FALLBACK.
# Resolved in dependency order so later entries can depend on earlier ones.  These produce a VALUE only:
# the emitted entry carries no annotation, so a fallback worth eyeballing is found by reading the theme.
COMPUTED_FALLBACK = [
    # The two palette entries ehRadio added after the old theme format was defined.  Both come from
    # the divider, which every theme file sets.  line is the divider's own ink, used by the vertical
    # and horizontal line widgets; vuaxis is a quarter of it, which keeps the VU's reference lines
    # (centre cross, bar baseline, divider) visible in every theme, on black and on white alike.
    ('line',      'div',     1.00),   # the divider verbatim
    ('vuaxis',    'div',     0.25),   # 25% of the divider
    ('digitbg',   'digit',   0.15),   # 15% of the digits' ink - the glow behind the IR/volume number
    ('clockbg',   'clock',   0.15),   # 15% of clock
    ('clockss',   'clock',   0.50),   # 50% of clock
    ('textss',    'clockss', 1.00),   # the screensaver info line: the clockss ink verbatim
    ('secondsss', 'seconds', 0.50),   # 50% of seconds
    ('dowss',     'dow',     0.50),   # 50% of dow
    ('datess',    'date',    0.50),   # 50% of date
    ('clockbgss', 'clockss', 0.15),   # 15% of clockss (resolved above)
    ('secondsbg',   'seconds',   0.15),   # 15% of seconds
    ('secondsbgss', 'secondsss', 0.15),   # 15% of secondsss (resolved above)
]

MAX_NAME_LEN = 50  # _themeNames[][64] -- truncate at 50 for safety

# The 8-bit steps of the fields the panel stores: red and blue have 32 levels (256/32 = 8) and green has 64
# (256/64 = 4).  Writing a colour on these steps is what makes one number in themes.h mean one level.
STEP = (8, 4, 8)


def quantise(rgb):
    """Snap an 8-bit triple onto the 565 steps.

    Cannot change the packed 565: (r >> 3) * 8 >> 3 is r >> 3 for any r, so the bucket is preserved by
    construction and the panel keeps drawing exactly what it drew before.
    """
    return tuple(min(255, max(0, int(v))) // STEP[i] * STEP[i] for i, v in enumerate(rgb))


def on_grid(rgb):
    """True when every channel is already on its step."""
    return all(int(v) % STEP[i] == 0 for i, v in enumerate(rgb))


def to_565(rgb):
    """The packed 565 of an 8-bit triple - the same expression the RGB() macro uses."""
    r, g, b = (min(255, max(0, int(v))) for v in rgb)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from_565(value):
    """A stored 565 colour as the numbers the editor shows: each level times its own step.

    These are the values on the grid by definition, so a JSON export needs no snapping - converting it this
    way IS the quantisation, and `to_565` of the result is the value it came from.
    """
    value = int(value) & 0xFFFF
    return (((value >> 11) & 0x1F) * 8, ((value >> 5) & 0x3F) * 4, (value & 0x1F) * 8)


def _trunc_name(n):
    if len(n) > MAX_NAME_LEN:
        truncated = n[:MAX_NAME_LEN]
        print(f"WARNING: Name '{n}' exceeds {MAX_NAME_LEN} chars, truncated to '{truncated}'")
        return truncated
    return n


def print_help():
    print(__doc__)


def parse_theme(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        text = f.read()
    lines = text.split('\n')

    # Step 1: find all #ifdef/#ifndef/#if defined blocks that contain COLOR_ defines
    blocks = []
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        m_ifdef = re.match(r'#ifdef\s+(\w+)', s)
        m_ifndef = re.match(r'#ifndef\s+(\w+)', s)
        m_if = re.match(r'#if\s+defined\s*\(\s*(\w+)\s*\)', s)
        if m_ifdef or m_ifndef or m_if:
            cond = (m_ifdef or m_ifndef or m_if).group(1)
            is_ifdef = bool(m_ifdef or m_if)
            nesting, else_idx, endif_idx = 1, -1, -1
            has_color = False
            k = i + 1
            while k < len(lines):
                sk = lines[k].strip()
                if re.match(r'#define\s+COLOR_', sk): has_color = True
                if re.match(r'#if(?:def|ndef|\s)', sk): nesting += 1
                elif re.match(r'#endif\b', sk):
                    nesting -= 1
                    if nesting == 0: endif_idx = k; break
                elif re.match(r'#else\b', sk) and nesting == 1: else_idx = k
                k += 1
            if has_color and endif_idx != -1:
                if cond != 'ENABLE_THEME' and not cond.startswith('_'):
                    blocks.append((cond, is_ifdef, i, else_idx, endif_idx))
                    i = endif_idx + 1
                else:
                    i += 1
            else:
                i = endif_idx + 1 if endif_idx != -1 else i + 1
        else:
            i += 1

    # Step 2: build block_active mapping
    block_active = {}
    for bi, (cond, is_ifdef, start, else_idx, end) in enumerate(blocks):
        if is_ifdef:
            true_range = range(start+1, else_idx if else_idx!=-1 else end)
            false_range = range(else_idx+1 if else_idx!=-1 else end, end)
        else:
            true_range = range(else_idx+1 if else_idx!=-1 else end, end)
            false_range = range(start+1, else_idx if else_idx!=-1 else end)
        block_active[(bi, 0)] = set(false_range)
        block_active[(bi, 1)] = set(true_range)

    # Step 3: parse global colors (lines NOT inside any branch block)
    block_line_indices = set()
    for _, _, start, else_idx, end in blocks:
        for li in range(start, end + 1):
            block_line_indices.add(li)

    global_colors = {}
    for li, line in enumerate(lines):
        if li in block_line_indices: continue
        s = line.strip()
        if s.startswith('#') and not s.startswith('#define'): continue
        m = re.match(r'#define\s+(COLOR_\w+)\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)', s)
        if m:
            name = m.group(1)
            rgb = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
            if name in COLOR_TO_FIELD:
                global_colors[COLOR_TO_FIELD[name]] = rgb

    # Step 4: generate variants
    n_blocks = len(blocks)
    if n_blocks == 0:
        return [{'colors': dict(global_colors), 'name_suffix': ''}], blocks

    results = []
    for combo in range(1 << n_blocks):
        active_lines = set()
        for bi in range(n_blocks):
            bit = (combo >> bi) & 1
            active_lines |= block_active[(bi, bit)]

        colors = dict(global_colors)
        for li, line in enumerate(lines):
            if li in active_lines:
                m = re.match(r'#define\s+(COLOR_\w+)\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)', line.strip())
                if m:
                    name = m.group(1)
                    rgb = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
                    if name in COLOR_TO_FIELD:
                        colors[COLOR_TO_FIELD[name]] = rgb

        suffix = f" {combo+1}" if n_blocks > 0 else ""
        results.append({'colors': colors, 'name_suffix': suffix})

    return results, blocks


def parse_theme_json(path):
    """One or more themes out of a WebUI export.

    Two shapes, both written by the device: the single-theme document `Export` downloads, and the array
    `Backup All` downloads.  A plain object is the only thing that can be a single theme, so an array is
    always a list - and an empty one, or an older backup with nulls in it, is handled rather than trusted.

    The keys are the element names from themes.h with a leading dot, so the mapping is a strip and nothing
    else; a key that is not one of ours is skipped.  Values are 565 integers converted to the numbers the
    editor shows, and an [r, g, b] array is accepted in place of one, the same tolerance the firmware's own
    loader has for a hand-edited file.
    """
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        text = f.read()

    try:
        doc = json.loads(text)
    except ValueError as e:
        print(f"ERROR: {os.path.basename(path)} is not valid JSON: {e}")
        sys.exit(1)

    if isinstance(doc, dict):
        docs = [doc]
    elif isinstance(doc, list):
        docs = [d for d in doc if isinstance(d, dict)]     # an older backup may carry nulls: skip them
        if len(docs) != len(doc):
            print(f"NOTE: {len(doc) - len(docs)} empty entr(y/ies) in that backup were skipped.")
    else:
        print(f"ERROR: {os.path.basename(path)} is neither a theme nor a list of themes.")
        sys.exit(1)

    if not docs:
        print(f"ERROR: {os.path.basename(path)} holds no themes.")
        sys.exit(1)

    results = []
    for d in docs:
        raw = d.get('colors')
        if not isinstance(raw, dict):
            # A hand-written file may keep the colours at the top level instead; the firmware accepts that
            # too, and there "name" is simply not a colour.
            raw = {k: v for k, v in d.items() if k != 'name'}

        colors = {}
        for key, value in raw.items():
            key = str(key)
            fname = key[1:] if key.startswith('.') else key
            if fname == 'playlist':
                continue
            if isinstance(value, list):
                if len(value) < 3:
                    print(f"WARNING: ignoring '{key}' - fewer than three channels.")
                    continue
                try:
                    rgb = tuple(int(v) for v in value[:3])
                except (TypeError, ValueError):
                    print(f"WARNING: ignoring '{key}' - not a colour value.")
                    continue
            else:
                try:
                    rgb = from_565(value)
                except (TypeError, ValueError):
                    print(f"WARNING: ignoring '{key}' - not a colour value.")
                    continue
            if re.fullmatch(r'playlist\[[0-4]\]', fname) or fname in FIELD_ORDER:
                colors[fname] = rgb

        name = d.get('name') if isinstance(d.get('name'), str) and d.get('name').strip() else None
        results.append({'colors': colors, 'name_suffix': '', 'name': name})

    return results


def emit_theme_entry(name, data, index):
    colors = data['colors']       # original colors from theme file (do not mutate)

    # Working copy that we fill with fallbacks
    final = dict(colors)

    # ---- smart fallbacks (dependency order) ----
    for field, source in SMART_FALLBACK:
        if field not in final and source in final:
            final[field] = final[source]

    # ---- computed fallbacks (dependency order) ----
    for field, source, pct in COMPUTED_FALLBACK:
        if field not in final and source in final:
            sr, sg, sb = final[source]
            final[field] = (int(sr * pct), int(sg * pct), int(sb * pct))

    # ---- meta fallback for everything still missing ----
    meta = final.get('meta', (0, 0, 0))
    for fname in FIELD_ORDER:
        if fname == 'playlist':
            for i in range(5):
                k = f'playlist[{i}]'
                if k not in final:
                    final[k] = meta
        elif fname not in final:
            final[fname] = meta

    # ---- quantise ----
    # Everything that lands in themes.h goes onto the 565 steps, whichever way it arrived: a JSON export is
    # already there, an old colour file is not, and a computed fallback (a percentage of another colour)
    # never is.  This cannot change what the panel draws - the step is the bucket the shift already chose.
    quantised = set()
    for fname in list(final.keys()):
        if not on_grid(final[fname]):
            final[fname] = quantise(final[fname])
            quantised.add(fname)

    # ---- emit ----
    # No comments: too many tools rewrite this file for an annotation to survive, and nothing parses one.
    # The name header stays - it is what makes the table readable, and the names live in _themeNames anyway.
    lines = [f'    {{   // {name}']
    for fname in FIELD_ORDER:
        if fname == 'playlist':
            vals = []
            for i in range(5):
                r, g, b = final[f'playlist[{i}]']
                vals.append(f'RGB({r:3d}, {g:3d}, {b:3d})')
            lines.append(f'        .{fname:13s} = {{{", ".join(vals)}}},')
        else:
            r, g, b = final[fname]
            lines.append(f'        .{fname:13s} = RGB({r:3d}, {g:3d}, {b:3d}),')

    lines.append('    },')
    return '\n'.join(lines), len(colors), len(quantised)


def modify_themes(target_path, entries, names):
    with open(target_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    if not re.search(r'\b_themes\[\]\s*PROGMEM\s*=\s*\{', content):
        print("ERROR: Could not find _themes[]."); return False
    closing = content.rfind('\n};')  # last }; in file = _themes[] closing
    if closing == -1: print("ERROR: Could not find closing '};' of _themes[]."); return False
    nm = re.search(r'(const char _themeNames\[\]\[64\] PROGMEM = )\{', content)
    if not nm: print("ERROR: Could not find _themeNames."); return False
    nm_brace_end = content.find('\n};', nm.end())
    if nm_brace_end == -1: print("ERROR: Could not find closing } of _themeNames."); return False
    block = content[nm.end():nm_brace_end]
    old = [n.strip().strip('"').strip(',').strip('"') for n in block.strip().split('\n') if n.strip()]
    new_names = old + names
    indented = '\n'.join(f'    "{n}",' for n in new_names)
    new_line = f'const char _themeNames[][64] PROGMEM = {{\n{indented}\n}};'
    # Insert new entries before the closing }; of _themes[]
    new_content = content[:closing] + '\n' + '\n'.join(entries) + content[closing:]
    new_content = new_content[:nm.start()] + new_line + new_content[nm_brace_end+1:]
    new_content = re.sub(r'\};(\};)+', '};', new_content)
    with open(target_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def existing_names(target_path):
    """The names already in _themeNames, in order."""
    with open(target_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    m = re.search(r'const char _themeNames\[\]\[64\] PROGMEM = \{(.*?)\n\};', content, re.S)
    if not m:
        return []
    return [n.strip().strip(',').strip('"') for n in m.group(1).strip().split('\n') if n.strip()]


# One RGB(r, g, b) call, with its own spacing captured so the rewrite can put it back exactly: group 1/3/5
# are the numbers with whatever padding they carry, and 2/4/6 are the separators between and after them.
RGB_CALL = re.compile(r'RGB\((\s*\d+)(\s*,\s*)(\s*\d+)(\s*,\s*)(\s*\d+)(\s*)\)')


def quantise_themes(target_path):
    """Move every color already in themes.h onto the 565 steps.

    Only the numbers change, and each one only to its own bucket's step, so every packed 565 - and so every
    colour the panel can draw - is left exactly as it was.  A number can only shrink, never grow, and its
    original width is preserved, so the file's alignment survives; values already on the grid are left
    untouched, which is what makes a second run report nothing to do.
    """
    with open(target_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    if not re.search(r'\b_themes\[\]\s*PROGMEM\s*=\s*\{', content):
        print(f"ERROR: Could not find _themes[] in {target_path}.")
        return False

    counts = {'changed': 0, 'aligned': 0}
    broken = []

    def repl(m):
        rgb = tuple(int(m.group(i)) for i in (1, 3, 5))
        new = quantise(rgb)
        if to_565(rgb) != to_565(new):
            broken.append((rgb, new))       # cannot happen; if it does, the palette would change
        if new == rgb:
            counts['aligned'] += 1
            return m.group(0)
        counts['changed'] += 1
        return ('RGB(' + str(new[0]).rjust(len(m.group(1))) + m.group(2) +
                str(new[1]).rjust(len(m.group(3))) + m.group(4) +
                str(new[2]).rjust(len(m.group(5))) + m.group(6) + ')')

    new_content = RGB_CALL.sub(repl, content)

    if broken:
        print(f"ERROR: {len(broken)} colour(s) would have changed their 565 value - nothing written.")
        for old, new in broken[:5]:
            print(f"  RGB{old} -> RGB{new}   {to_565(old)} -> {to_565(new)}")
        return False

    total = counts['changed'] + counts['aligned']
    if total == 0:
        print(f"ERROR: no RGB(r, g, b) calls found in {os.path.basename(target_path)}.")
        return False

    print(f"\n{total} colour(s) in {os.path.basename(target_path)}:")
    print(f"  {counts['aligned']} already on the 565 steps")
    print(f"  {counts['changed']} moved onto them - the packed 565 is unchanged, so the panel is too")

    if counts['changed'] == 0:
        print("\nNothing to do.")
        return True

    with open(target_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print("Updated: " + target_path)
    return True


# ---------------------------------------------------------------------------
# The WebUI palette: the same display colours, as a block in themes.css
# ---------------------------------------------------------------------------
# style.css holds :root, the design the pages are drawn with.  themes.css holds one block per display theme,
# and a block lists only what that theme's colours do differently.  This script is the only thing that both
# adds a themes.h entry and writes the block that entry belongs to, which is what keeps the two in step.
#
# The one reference in the whole scheme is the offset measured from display theme [0]: :root was written for
# that theme, so re-anchoring each role through it makes a theme resembling it need only a few lines.  It is
# not an option and not a stored value - to retune the design, run webui_tool.py --inflate, adjust :root,
# then --deflate, which re-establishes the relationship without any index to name.

NEAR_MASTER = 32         # Euclidean RGB distance from the :root value that still counts as "the same"
READ_TEXT = 4.5          # contrast target for body text
READ_ACCENT = 3.0        # contrast target for accents, icons and secondary text
HIGHLIGHT_MIX = 0.28     # highlight-fill = clock darkened by this much
SHADOW_MIX = 0.16        # logo-shadow = logo darkened by this much
PANEL_MIX = 0.15         # panel-gradient = background darkened by this much
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)


def clamp(v):
    return max(0, min(255, int(round(v))))


def clamp_rgb(c):
    return tuple(clamp(v) for v in c)


def rgb_to_hex(c):
    return '#%02x%02x%02x' % clamp_rgb(c)


def hex_to_rgb(s):
    s = s.strip().lstrip('#')
    if len(s) == 3:
        s = ''.join(ch * 2 for ch in s)
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def blend(a, b, t):
    """Linear per-channel mix; t=0 is a, t=1 is b."""
    return tuple(a[i] * (1.0 - t) + b[i] * t for i in range(3))


def _linear(channel):
    c = channel / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def rel_lum(c):
    """WCAG relative luminance."""
    return 0.2126 * _linear(c[0]) + 0.7152 * _linear(c[1]) + 0.0722 * _linear(c[2])


def contrast(a, b):
    la, lb = rel_lum(a), rel_lum(b)
    hi, lo = max(la, lb), min(la, lb)
    return (hi + 0.05) / (lo + 0.05)


def dist(a, b):
    """Euclidean RGB distance, used for the near-master test."""
    return sum((a[i] - b[i]) ** 2 for i in range(3)) ** 0.5


def readable(c, bg, target):
    """Keep the tint but move toward black/white until it reads on bg."""
    if contrast(c, bg) >= target:
        return tuple(c)
    toward = BLACK if contrast(BLACK, bg) >= contrast(WHITE, bg) else WHITE
    for step in range(1, 11):
        candidate = blend(c, toward, step / 10.0)
        if contrast(candidate, bg) >= target:
            return candidate
    return toward


def contrast_pick(surface):
    """Black or white, whichever reads on surface."""
    return BLACK if contrast(BLACK, surface) >= contrast(WHITE, surface) else WHITE


def panel_border_for(line, bg, title2):
    """The display's divider, softened when it is too quiet or too loud against the page."""
    c = contrast(line, bg)
    if c < 1.4:
        return blend(bg, title2, 0.45)
    if c > 6.0:
        return blend(line, bg, 0.5)
    return tuple(line)


def rgb_to_hsl(c):
    r, g, b = (v / 255.0 for v in c)
    hi, lo = max(r, g, b), min(r, g, b)
    light = (hi + lo) / 2.0
    delta = hi - lo
    if delta == 0:
        return (0.0, 0.0, light)
    sat = delta / (2.0 - hi - lo) if light > 0.5 else delta / (hi + lo)
    if hi == r:
        hue = ((g - b) / delta) % 6.0
    elif hi == g:
        hue = (b - r) / delta + 2.0
    else:
        hue = (r - g) / delta + 4.0
    return (hue / 6.0, sat, light)


def hsl_to_rgb(h, s, l):
    if s <= 0:
        v = l * 255.0
        return (v, v, v)
    q = l * (1.0 + s) if l < 0.5 else l + s - l * s
    p = 2.0 * l - q

    def channel(t):
        t %= 1.0
        if t < 1.0 / 6.0:
            return p + (q - p) * 6.0 * t
        if t < 1.0 / 2.0:
            return q
        if t < 2.0 / 3.0:
            return p + (q - p) * (2.0 / 3.0 - t) * 6.0
        return p

    return (channel(h + 1.0 / 3.0) * 255.0, channel(h) * 255.0, channel(h - 1.0 / 3.0) * 255.0)


def is_neutral(c):
    """A near-grey, where HSL's hue and saturation are numerical noise (near-white is worst)."""
    return max(c) - min(c) <= 20


def reanchor(master_c, anchor_c, target_c):
    """Move the master's colour by the display's change from the yardstick to the target.

    Hue and saturation follow the display; lightness keeps the master's offset.  When a feature is
    neutral its hue means nothing, so the other side of the pair decides - that is what lets a black
    master background become a pink one, and a near-white clock stay grey rather than turn green.
    """
    mh, ms, ml = rgb_to_hsl(master_c)
    ah, asat, al = rgb_to_hsl(anchor_c)
    th, tsat, tl = rgb_to_hsl(target_c)
    if is_neutral(anchor_c):
        hue, sat = th, tsat
    elif is_neutral(target_c):
        hue, sat = mh, 0.0
    else:
        hue = (mh + (th - ah)) % 1.0
        sat = min(1.0, ms * (tsat / asat))
    light = min(1.0, max(0.0, ml + (tl - al)))
    return hsl_to_rgb(hue, sat, light)


def feature(t):
    """Per-role colour taken from one display theme, before the master offset is applied.

    The WebUI background is the display's own: the pages are dark because the palettes are, not because a
    mode forces it.  Contrast is a post-transfer check, so the offset is measured on the display's own
    values and not on a colour already darkened for a different page.
    """
    background = t['background']
    control = t['metabg']
    logo = t['clock']
    line = t.get('line') or t.get('div') or t['title2']
    main_text_1 = t['title2']
    main_text_2 = t['title1']
    highlight = blend(t['clock'], BLACK, HIGHLIGHT_MIX)

    return {
        'panel-background': background,
        'panel-gradient': background,
        'panel-border': tuple(line),
        'control-bg': control,
        'button-fill': control,
        'button-text': main_text_2,
        'button-outline': main_text_2,
        'button-active-fill': highlight,
        'button-active-text': main_text_2,
        'icons-outline': main_text_2,
        'highlight-fill': highlight,
        'icon-active-glyph': background,
        'main-text': main_text_1,
        'small-text': main_text_2,
        'caption-text': main_text_1,
        'editor-cell-text': main_text_1,
        'link-quiet': main_text_1,
        'section-title-text': main_text_1,
        'switch-off-text': main_text_1,
        'meta-text': main_text_2,
        'info-text': main_text_2,
        'eq-label-text': main_text_2,
        'heading-text': main_text_2,
        'link-text': main_text_2,
        'station-name': main_text_2,
        'list-names': main_text_1,
        'list-urls': main_text_2,
        'switch-text': main_text_2,
        'input-fill': control,
        'input-text': main_text_2,
        'input-border': tuple(line),
        'select-fill': control,
        'select-text': main_text_2,
        'meter-fill': highlight,
        'meter-track': control,
        'meter-outline': main_text_2,
        'switch-track': background,
        'switch-track-on': highlight,
        'switch-thumb': main_text_2,
        'spinner-ring': main_text_2,
        'scrollbar-thumb': tuple(line),
        'scrollbar-track': background,
        'list-outline': main_text_2,
        'list-active-bg': highlight,
        'list-active-text': main_text_2,
        'editor-grid': control,
        'logo': logo,
        'logo-text': control,
        'logo-shadow': logo,
        'warning-outline': main_text_2,
    }


def transfer(feat, master_rgb, yardstick_feat):
    """Re-anchor each role from the master through the yardstick, then derive and check the rest."""
    out = {}
    for role, c in feat.items():
        out[role] = clamp_rgb(reanchor(master_rgb[role], yardstick_feat[role], c))

    out['panel-gradient'] = blend(out['panel-background'], BLACK, PANEL_MIX)
    out['panel-border'] = panel_border_for(out['panel-border'], out['panel-background'], out['main-text'])
    out['input-border'] = blend(out['panel-background'], out['panel-border'], 0.52)
    out['logo-shadow'] = blend(out['logo'], BLACK, SHADOW_MIX)
    if contrast(out['logo-text'], out['logo']) < 1.5:
        out['logo-text'] = contrast_pick(out['logo'])

    # Every fill that means "highlight" follows the one highlight colour, and its text is chosen once.
    if contrast(out['highlight-fill'], out['panel-background']) < 1.5:
        out['highlight-fill'] = blend(out['highlight-fill'], WHITE, 0.25)
    for role in ('button-active-fill', 'meter-fill', 'switch-track-on', 'list-active-bg'):
        out[role] = out['highlight-fill']
    # One ink for everything lit: the button label, the icon glyph, the playing row, the switch word.
    on_highlight = contrast_pick(out['highlight-fill'])
    out['button-active-text'] = on_highlight
    out['icon-active-glyph'] = on_highlight
    out['list-active-text'] = on_highlight
    out['switch-text'] = on_highlight

    for role in ('main-text', 'small-text', 'list-names', 'list-urls',
                 'caption-text', 'editor-cell-text', 'link-quiet', 'section-title-text',
                 'switch-off-text',
                 'meta-text', 'info-text', 'eq-label-text'):
        out[role] = readable(out[role], out['panel-background'], READ_TEXT)
    if contrast(out['small-text'], out['panel-background']) < contrast(out['main-text'], out['panel-background']):
        out['small-text'] = readable(out['small-text'], out['panel-background'],
                                     min(20.0, contrast(out['main-text'], out['panel-background']) + 1.0))
    # The split roles are members of one of the two text tiers, and the list columns use the same two
    # tiers: keep every one of them in step with its tier after the contrast pass.
    for role in ('caption-text', 'editor-cell-text', 'link-quiet', 'list-names',
                 'section-title-text', 'switch-off-text'):
        out[role] = out['main-text']
    for role in ('eq-label-text', 'info-text', 'list-urls', 'meta-text'):
        out[role] = out['small-text']

    for role in ('heading-text', 'link-text', 'station-name', 'button-text', 'input-text',
                 'select-text', 'button-outline', 'icons-outline', 'meter-outline', 'list-outline',
                 'switch-thumb', 'spinner-ring', 'warning-outline'):
        out[role] = readable(out[role], out['panel-background'], READ_ACCENT)

    return {role: rgb_to_hex(out[role]) for role in ROLE_ORDER}


def build_palettes(themes, master_hex):
    """One final palette per display theme, offset through display theme [0] - the design's yardstick."""
    missing = [role for role in ROLE_ORDER if role not in master_hex]
    if missing:
        print("ERROR: style.css :root has no --" + ', --'.join(missing) + '.')
        sys.exit(1)
    master_rgb = {role: hex_to_rgb(master_hex[role]) for role in ROLE_ORDER}
    yardstick = feature(themes[0])
    return [transfer(feature(t), master_rgb, yardstick) for t in themes]


def theme_deltas(computed, master):
    """Only the properties that differ from the master, in ROLE_ORDER."""
    out = {}
    for role in ROLE_ORDER:
        value = computed[role]
        base = master.get(role)
        if base is None or dist(hex_to_rgb(value), hex_to_rgb(base)) > NEAR_MASTER:
            out[role] = value
    return out


def read_themes_h(path):
    """(names, entries) straight out of a themes.h - the one just written, or its .new.h twin."""
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    names = parse_names(content)
    spans = parse_entries(content)
    if len(names) != len(spans):
        print(f"ERROR: {os.path.basename(path)} has {len(names)} names but {len(spans)} entries.")
        sys.exit(1)
    themes = [parse_fields(content[s:e]) for s, e in spans]
    for i, t in enumerate(themes):
        for required in ('background', 'title1', 'title2', 'clock', 'metabg'):
            if required not in t:
                print(f"ERROR: theme [{i}] {names[i]!r} has no .{required} field.")
                sys.exit(1)
    return names, themes


def sync_webui(themes_h_path, first_new_id):
    """Write the blocks for the entries just added, and refresh every --theme-name from themes.h."""
    names, themes = read_themes_h(themes_h_path)
    master_raw, blocks, text, eol = load()
    master_hex = resolve_master(master_raw)
    families = find_families(master_raw)

    have = {b['id'] for b in blocks}
    new_ids = [i for i in range(first_new_id, len(names)) if i not in have]
    if len(blocks) + len(new_ids) != len(names):
        print(f"\nERROR: themes.h lists {len(names)} theme(s) but themes.css would end up with "
              f"{len(blocks) + len(new_ids)} block(s).")
        print("       Hand-edit themes.css or themes.h until the count and the ids line up, then run this again.")
        sys.exit(1)

    print("\nWebUI palette (data/www/themes.css):")
    palettes = build_palettes(themes, master_hex)
    edits = []
    for i in new_ids:
        props = theme_deltas(palettes[i], master_hex)
        collapse_families(props, master_raw, families)
        edits.append((len(text), len(text), render_block(i, names[i], props, eol) + eol))
        print(f"  block [{i}] {names[i]!r}: {len(props)} declaration(s)")
    for b in blocks:
        if b['id'] < len(names) and b['name'] != names[b['id']]:
            print(f"  block [{b['id']}] renamed to {names[b['id']]!r}")
            edits.append((b['start'], b['end'],
                          render_block(b['id'], names[b['id']], b['props'], eol)))
    if not edits:
        print("  nothing to change")
        return

    text = apply_edits(text, edits)
    write_text(THEMES_CSS, text)
    print(f"\nUpdated: {THEMES_CSS}")


# ---------------------------------------------------------------------------
# Modes
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


def ask_index(prompt, count):
    """A 0-based index into `count` entries.  Blank, EOF or ^C cancels (None)."""
    while True:
        try:
            resp = input(prompt).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return None
        if resp == '':
            return None
        if resp.isdigit() and 0 <= int(resp) < count:
            return int(resp)
        print(f"  Please enter 0-{count - 1}, or blank to cancel.")


def name_hint(theme_path):
    """Stop, and say exactly how to name the theme instead of this one."""
    print("\nNothing was written. Re-run the tool with this command to name the theme:")
    print(f'  py theme_tool.py -i "{theme_path}" --name "New Theme (Creator)"')
    sys.exit(1)


def ask_name(theme_path, current, position, total):
    """Confirm the name the file carries, or stop with the command that names it.

    Every theme has to read as "Name (Creator)", and a file's own name is only a starting point: spelling gets
    fixed and the creator gets appended.  A name is never typed at a prompt here - the command line owns it - so
    refusing sends the user back to the shell with that command already written out.
    """
    where = f"[{position} of {total}] " if total > 1 else ''
    if not current:
        print(f'\n{where}This theme file carries no name, and every theme should be named "Name (Creator)".')
        name_hint(theme_path)

    print(f'\n{where}The theme should be formatted as "Name (Creator)".')
    print(f'The current name is "{current}".')
    try:
        reply = input("Do you wish to use this name? [y/N] ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        reply = ''
    if reply in ('y', 'yes'):
        return current
    name_hint(theme_path)


def mode_audit():
    """Read-only: do both files describe the same themes, are the names current, is themes.h on the grid?

    Deliberately says nothing about colours: a block is hand-tuned after it is generated, so "differs from a
    re-derivation" is the normal state of a finished theme, not a fault.
    """
    names, _, content = load_themes()
    _, blocks, _, _ = load()
    hard, soft = check_sync(names, blocks)

    print(f"themes.h   {len(names)} theme(s)")
    print(f"themes.css {len(blocks)} block(s)")
    for line in soft:
        print(f"  name drift: {line}")
    for line in hard:
        print(f"  ERROR: {line}")

    values = list(RGB_CALL.finditer(content))
    off = [m for m in values if not on_grid(tuple(int(m.group(i)) for i in (1, 3, 5)))]
    print(f"themes.h   {len(values)} RGB() value(s), {len(off)} off the 565 steps")

    if hard:
        print("\nHand-edit themes.h or themes.css until the count and the ids agree, then run this again.")
        sys.exit(1)
    if off:
        print("  -q moves them onto the grid; the packed 565, and so the panel, does not change")
    if soft:
        print("  themes.h is the authority - the next import rewrites those names from it")
    if not hard and not soft and not off:
        print("\nAll three agree: same themes, same names, every colour on the grid.")


def run_maintenance(text, eol, master_raw, blocks, mode):
    """--inflate / --deflate / --deflate-lossy.  No colour is ever chosen: declarations only move."""
    master_hex = resolve_master(master_raw)
    families = find_families(master_raw)
    edits = []
    touched_blocks = 0
    touched_props = 0

    for block in blocks:
        props = dict(block['props'])
        removed = {}
        added = {}

        for name, value in list(props.items()):
            have = resolve_value(value, props, master_raw)
            want = master_hex.get(name)
            if have is None:
                print(f"  [{block['id']}] --{name}: kept (not resolvable to one hex value)")
                continue
            if want is None:
                print(f"  [{block['id']}] --{name}: kept (not in master)")
                continue
            if mode == 'inflate':
                continue
            if have == want:
                removed[name] = value
            elif mode == 'deflate-lossy':
                distance = dist(hex_to_rgb(have), hex_to_rgb(want))
                if 0 < distance <= NEAR_MASTER:
                    if ask_yes(f"  [{block['id']}] remove --{name} "
                               f"({have}, {distance:.1f} from master {want})?", default=False):
                        removed[name] = value

        if mode == 'inflate':
            for role in ROLE_ORDER:
                if role in props:
                    continue
                # The value the role already resolves to in THIS block, so a block that declares a
                # head keeps its cascade instead of being shadowed by the master's hex.
                value = resolve_value(master_raw.get(role, 'var(--' + role + ')'), props, master_raw)
                if value is None:
                    value = master_hex.get(role)
                if value is None:
                    continue
                props[role] = value
                added[role] = value
        else:
            for name in removed:
                props.pop(name, None)
            # members that all follow their head go back to being one head line
            collapsed, kept = collapse_families(props, master_raw, families)
            removed.update(collapsed)
            added.update(kept)

        if not added and not removed:
            continue
        touched_blocks += 1
        touched_props += len(added) + len(removed)
        edits.append((block['start'], block['end'],
                      render_block(block['id'], block['name'], props, eol)))

    if mode == 'inflate':
        verb, noun = 'Inflated', 'added'
    else:
        verb, noun = 'Deflated', 'removed'
    if not edits:
        print("Nothing to do - themes.css is already in this form.")
        return text
    text = apply_edits(text, edits)
    write_text(THEMES_CSS, text)
    print(f"{verb} {touched_blocks} theme(s), {noun} {touched_props} declaration(s).")
    return text


def mode_maintain(mode):
    master_raw, blocks, text, eol = load()
    names, _, _ = load_themes()
    hard, soft = check_sync(names, blocks)
    if hard:
        print("ERROR: themes.h and themes.css do not describe the same set of themes:")
        for line in hard:
            print('  ' + line)
        print("\nHand-edit them until the count and the ids agree, then run this again.")
        sys.exit(1)
    if soft:
        print(f"Note: {len(soft)} block name(s) differ from themes.h; -a lists them.")

    print(f"Maintenance mode: --{mode}")
    run_maintenance(text, eol, master_raw, blocks, mode)


def mode_quantise():
    if not os.path.exists(THEMES_H):
        fail(f"{THEMES_H} not found.")
    sys.exit(0 if quantise_themes(THEMES_H) else 1)


def rewrite_palette(removed_index, names):
    """Drop the block that belonged to the deleted theme and renumber every block above it.

    The whole file is re-rendered from the blocks that survive: ids are positions, so block N+1 becomes block
    N and the names come from themes.h.  A block's declarations are moved, never recomputed - deleting a theme
    must not retune the ones that stay.
    """
    _, blocks, _, eol = load()
    by_id = {b['id']: b for b in blocks}
    props_by_id = {}
    for new_id in range(len(names)):
        old_id = new_id if new_id < removed_index else new_id + 1
        if old_id not in by_id:
            print(f"ERROR: themes.css has no block for id {old_id}; it was left untouched.")
            print("       Hand-edit themes.css until its blocks match themes.h, then run this again.")
            sys.exit(1)
        props_by_id[new_id] = by_id[old_id]['props']
    text = render_file(names, props_by_id, eol)
    write_text(THEMES_CSS, text)
    print(f"themes.css: block [{removed_index}] removed, "
          f"{max(0, len(names) - removed_index)} block(s) renumbered")


def mode_delete(index):
    content, _ = read_text(THEMES_H)
    names = parse_names(content)
    spans = parse_entries(content)
    if len(names) != len(spans):
        fail(f"Name/entry count mismatch in themes.h ({len(names)} names, {len(spans)} entries).")
    if not names:
        print("No themes found.")
        return

    print(f"\nThemes in themes.h ({len(names)}):\n")
    for i, name in enumerate(names):
        print(f"[{i}] {name}")

    if index is None:
        index = ask_index(f"\nDelete which theme? [0-{len(names) - 1}] (blank to cancel): ", len(names))
        if index is None:
            print("Cancelled. Nothing was changed.")
            return
    if index < 0 or index >= len(names):
        fail(f"Index {index} out of range (0-{len(names) - 1}).")
    if len(names) <= 1:
        fail("Cannot delete the last remaining theme.")

    # Both files must describe the same list before either is touched.
    _, blocks, _, _ = load()
    hard, _soft = check_sync(names, blocks)
    if hard:
        print("\nERROR: themes.h and themes.css do not describe the same set of themes:")
        for line in hard:
            print('  ' + line)
        print("\nHand-edit them until the count and the ids agree, then run this again.")
        sys.exit(1)

    target_name = names[index]
    print(f"\nAbout to delete: [{index}] {target_name}")
    print("A theme's index is its position in themes.h, so every theme above this one moves down by one:")
    print("  a device set to one of those will show a different theme until it is set again.")
    if not ask_yes("This cannot be undone. Continue?", default=False):
        print("Cancelled. Nothing was changed.")
        return

    # Remove the entry.  parse_entries() runs its ^ against a slice, so for any entry after the first it also
    # swallows the previous line's newline: normalise that start, then consume the entry's own line terminator,
    # so the neighbours end up on adjacent lines instead of leaving a blank one behind.
    entry_start, entry_end = spans[index]
    if content[entry_start] == '\n':
        entry_start += 1
    if entry_end < len(content) and content[entry_end] == '\n':
        entry_end += 1
    content = content[:entry_start] + content[entry_end:]
    content = re.sub(r'\n{4,}', '\n\n\n', content)

    # Rebuild _themeNames, keeping the file's own trailing-comma style.
    del names[index]
    nm = re.search(r'const char _themeNames\[\]\[\d+\] PROGMEM = \{', content)
    if nm:
        be = content.find('\n};', nm.end())
        if be != -1:
            keeps_trailing_comma = content[nm.end():be].rstrip().endswith(',')
            lines = [f'    "{n}",' for n in names]
            if not keeps_trailing_comma:
                lines[-1] = lines[-1][:-1]
            content = (content[:nm.start()] +
                       'const char _themeNames[][64] PROGMEM = {\n' + '\n'.join(lines) + '\n};' +
                       content[be + 1:])
    content = re.sub(r'\};(\};)+', '};', content)

    write_text(THEMES_H, content)
    print(f"Deleted [{index}] {target_name}")

    rewrite_palette(index, names)

    print(f"\nThemes now ({len(names)}):\n")
    for i, name in enumerate(names):
        print(f"[{i}] {name}")


def mode_import(theme_path, name, start_idx):
    """One theme file in, one themes.h entry and one themes.css block out - never just one of them."""
    if not os.path.exists(theme_path):
        fail(f"File not found: {theme_path}")

    # The file says which format it is: an export starts with { or [ once the whitespace is gone, while an
    # old-style theme file starts with a comment or a #define.
    with open(theme_path, 'r', encoding='utf-8', errors='replace') as f:
        head = f.read().lstrip()
    basename = os.path.basename(theme_path)

    if head[:1] in ('{', '['):
        results = parse_theme_json(theme_path)
    else:
        results, _blocks = parse_theme(theme_path)
        if not any(r['colors'] for r in results):
            fail(f"No COLOR_* defines found in {basename}. Is this an old-style theme file?")

    # Ids: the next free position unless -n says otherwise.
    if start_idx is None:
        content, _ = read_text(THEMES_H)
        idx = 0
        m = re.search(r'_themeNames\[\]\[\d+\] PROGMEM = \{', content)
        if m:
            s, e = m.end(), content.find('};', m.end())
            if e > s:
                idx = content[s:e].count(',') + 1 if content[s:e].strip() else 1
    else:
        idx = start_idx

    # A name from the command line wins outright.  Without one, the name the file carries has to be confirmed -
    # or refused, which stops before anything is written and prints the command that would name it.  More than
    # one theme is numbered the way the old format's variants are, because a backup often holds several saves of
    # the same theme and they all carry the same name inside.
    numbered = len(results) > 1
    entries, names_to_add = [], []
    print(f"\nAdding {basename} theme(s) to themes.h...")

    for position, result in enumerate(results):
        given = name or result.get('name')
        if not name:
            given = ask_name(theme_path, given, position + 1, len(results))
        theme_name = _trunc_name(f"{given}{result.get('name_suffix') or (f' {position + 1}' if numbered else '')}")
        entry_text, _converted, moved = emit_theme_entry(theme_name, result, idx)
        entries.append(entry_text)
        names_to_add.append(theme_name)
        print(f"\nTheme: {theme_name}")
        print(f"  {len(result['colors'])} colors")
        if moved:
            print(f"  {moved} colour(s) snapped onto the 565 steps")
        idx += 1

    existing = existing_names(THEMES_H)
    known = set(existing)
    for n in names_to_add:
        if n in known:
            print(f"WARNING: '{n}' is already in _themeNames - this adds a second entry with that name.")

    # modify_themes appends, so the new themes take the ids after the ones already there.
    first_new_id = len(existing)
    if not modify_themes(THEMES_H, entries, names_to_add):
        sys.exit(1)
    print(f"\nUpdated: {THEMES_H}")

    sync_webui(THEMES_H, first_new_id)


# ---------------------------------------------------------------------------
# Command line
# ---------------------------------------------------------------------------

def main():
    import argparse

    ap = argparse.ArgumentParser(add_help=False, description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    mode = ap.add_mutually_exclusive_group()
    mode.add_argument('-a', '--audit', action='store_true',
                      help='read-only check of both files; this is what a bare run does')
    mode.add_argument('-i', '--import', dest='import_file', metavar='FILE',
                      help='import a theme file into themes.h and themes.css')
    mode.add_argument('-q', '--quantise', '--quantize', dest='quantise', action='store_true',
                      help='move the colours already in themes.h onto the 565 steps')
    mode.add_argument('-d', '--delete', dest='delete', nargs='?', const=-1, type=int, metavar='INDEX',
                      help='delete a theme from both files; no INDEX asks which')
    mode.add_argument('--inflate', action='store_true',
                      help='spell every block out at the colours it already has')
    mode.add_argument('--deflate', action='store_true',
                      help='drop what repeats :root and fold families back into one head line')
    mode.add_argument('--deflate-lossy', dest='deflate_lossy', action='store_true',
                      help='the same for near-matches, asking per role')
    ap.add_argument('--name', help='theme name; without it, the name the file carries must be confirmed')
    ap.add_argument('-n', '--number', type=int, help='starting index for an import')
    ap.add_argument('-h', '--help', action='help', help='show this help and exit')

    args = ap.parse_args()

    if args.import_file:
        mode_import(args.import_file, args.name, args.number)
    elif args.quantise:
        mode_quantise()
    elif args.delete is not None:
        mode_delete(None if args.delete == -1 else args.delete)
    elif args.inflate:
        mode_maintain('inflate')
    elif args.deflate:
        mode_maintain('deflate')
    elif args.deflate_lossy:
        mode_maintain('deflate-lossy')
    else:
        mode_audit()


if __name__ == '__main__':
    main()

