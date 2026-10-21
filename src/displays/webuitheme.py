#!/usr/bin/env python3
"""
webuitheme.py — import display themes into the WebUI palette.

USAGE:
    py webuitheme.py                    # Add any theme missing from style.css, then reconcile renames
    py webuitheme.py --update           # ...then compare every editable theme, element by element
    py webuitheme.py --blackbg --dry-run
    py webuitheme.py --anchor 0         # display theme whose design :root represents (default 0)
    py webuitheme.py -h | --help

WHAT IT READS AND WRITES:
    src/displays/themes.h          -> the display colours
    data/www/style.css             -> :root is the master design; each theme is a delta block
    The tool never writes :root and never deletes or reorders a block.

HOW A THEME IS IMPORTED:
    The master is the WebUI rendering of the anchor display theme.  For every theme the tool takes a
    per-role "feature" from the display (see FEATURE_NOTES below), measures the master's offset from
    the anchor's features, and adds that offset to the theme's features:

        final(role) = feature(role, theme) + (master(role) - feature(role, anchor))

    The anchor therefore comes out as an empty block - the master already paints it - while every
    other theme keeps the master's design offsets and follows its own display.  Roles with no
    feature (`--list-bars`, `--playlist-hover`) stay at the master value.

LOCKING A THEME:
    A block whose title contains the whole word PERFECT is inert: it is never added, renamed,
    compared or listed for update.  Delete the word to make it editable again.  The tool never
    writes it.

THE TRANSLATION (feature map, calibrated from the current :root):
    background      <- background            control-bg      <- metabg
    panel-gradient  <- background darkened   panel-border    <- line (softened when it glares)
    main-text-1     <- title2                main-text-2     <- title1
    accent-text     <- title1                icons-outline   <- title1
    list-names      <- title2                list-urls       <- title1
    icons-highlight <- clock darkened        logo            <- clock
    logo-text       <- metabg                input-border / logo-shadow / switch-text derived

BLACK BACKGROUND MODE:
    --blackbg keeps the WebUI background dark and uses the DISPLAY's background as the logo; text is
    checked against the dark master instead of the display background.
"""

import os
import re
import sys

# Force UTF-8 output on Windows (theme names contain non-ASCII characters)
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

HERE = os.path.dirname(os.path.abspath(__file__))
THEMES_H = os.path.join(HERE, 'themes.h')
STYLE_CSS = os.path.normpath(os.path.join(HERE, '..', '..', 'data', 'www', 'style.css'))

NEAR_MASTER = 32         # Euclidean RGB distance from the :root value that counts as "the master"
READ_TEXT = 4.5          # contrast target for body text
READ_ACCENT = 3.0        # contrast target for accents, icons and secondary text
HIGHLIGHT_MIX = 0.28     # icons-highlight = clock darkened by this much
SHADOW_MIX = 0.16        # logo-shadow = logo darkened by this much
SURFACE_MIX = 0.16       # blackbg control-bg = master background lifted by this much
PANEL_MIX = 0.15         # panel-gradient = background darkened by this much
DEFAULT_ANCHOR = 0
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

ROLE_ORDER = [
    'background', 'control-bg', 'list-bars', 'panel-gradient', 'panel-border', 'input-border',
    'main-text-1', 'main-text-2', 'accent-text', 'list-names', 'list-urls', 'switch-text',
    'highlight-text', 'icons-outline', 'icons-highlight', 'logo', 'logo-text', 'logo-shadow',
    'playlist-hover',
]

# Master-only roles: the design keeps these neutral on every theme.
MASTER_ONLY = ('list-bars', 'playlist-hover')

MARKER_RE = re.compile(r'/\* webuitheme\.py mode=(\w+)(?: anchor=(\d+))?[^*]*\*/')
LOCK_RE = re.compile(r'\bPERFECT\b')


# ---------------------------------------------------------------------------
# themes.h
# ---------------------------------------------------------------------------

def parse_names(content):
    """Theme names from _themeNames, in order.  Same parser as deletetheme.py."""
    m = re.search(r'const char _themeNames\[\]\[\d+\] PROGMEM = \{', content)
    if not m:
        fail("Could not find _themeNames in themes.h.")
    brace_end = content.find('\n};', m.end())
    if brace_end == -1:
        fail("Could not find the closing } of _themeNames.")
    block = content[m.end():brace_end]
    return [n.strip().strip('"').strip(',').strip('"') for n in block.strip().split('\n') if n.strip()]


def parse_entries(content):
    """(start, end) of each theme entry in _themes[].  Same parser as deletetheme.py."""
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


FIELD_RE = re.compile(r'\.([A-Za-z0-9_]+)\s*=\s*RGB\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)')
PLAYLIST_RE = re.compile(r'\.playlist\s*=\s*\{([^}]*)\}')


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
    if not os.path.exists(THEMES_H):
        fail(f"{THEMES_H} not found.")
    with open(THEMES_H, 'r', encoding='utf-8') as f:
        content = f.read()
    names = parse_names(content)
    spans = parse_entries(content)
    if len(names) != len(spans):
        fail(f"Name/entry count mismatch in themes.h ({len(names)} names, {len(spans)} entries).")
    themes = [parse_fields(content[s:e]) for s, e in spans]
    for i, t in enumerate(themes):
        for required in ('background', 'title1', 'title2', 'clock', 'metabg'):
            if required not in t:
                fail(f"Theme [{i}] {names[i]!r} has no .{required} field.")
    return names, themes


# ---------------------------------------------------------------------------
# Colour
# ---------------------------------------------------------------------------

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


def luma(c):
    """Gamma-encoded brightness, for direction decisions only."""
    return (0.2126 * c[0] + 0.7152 * c[1] + 0.0722 * c[2]) / 255.0


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


# ---------------------------------------------------------------------------
# Import: features, offset, palette
# ---------------------------------------------------------------------------

def feature(t, ui_bg, blackbg):
    """Per-role colour taken from one display theme, before the master offset is applied."""
    bg = t['background']
    title1 = t['title1']
    title2 = t['title2']
    clock = t['clock']
    metabg = t['metabg']
    line = t.get('line') or t.get('div') or title2

    # Raw display text: contrast is a post-transfer check, so the anchor offset is measured on the
    # display's own values and not on a colour already darkened for a different page.
    main_text_2 = title1
    main_text_1 = title2

    if blackbg:
        background = ui_bg
        control = blend(ui_bg, WHITE, SURFACE_MIX)
        logo = bg
        if contrast(logo, ui_bg) < 1.6:
            logo = metabg
        if contrast(logo, ui_bg) < 1.6:
            logo = clock if contrast(clock, ui_bg) >= 1.6 else title1
    else:
        background = bg
        control = metabg
        logo = clock

    return {
        'background': background,
        'control-bg': control,
        'panel-border': tuple(line),
        'main-text-1': main_text_1,
        'main-text-2': main_text_2,
        'accent-text': main_text_2,
        'list-names': main_text_1,
        'list-urls': main_text_2,
        'icons-outline': main_text_2,
        'icons-highlight': blend(clock, BLACK, HIGHLIGHT_MIX),
        'logo': logo,
        'logo-text': metabg,
    }


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
    """Move the master's colour by the display's change from the anchor to the target.

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


def transfer(feat, master_rgb, anchor_feat):
    """Re-anchor each role from the master through the anchor, then derive and check the rest."""
    out = {}
    for role, c in feat.items():
        out[role] = clamp_rgb(reanchor(master_rgb[role], anchor_feat[role], c))

    out['panel-gradient'] = blend(out['background'], BLACK, PANEL_MIX)
    out['panel-border'] = panel_border_for(out['panel-border'], out['background'], out['main-text-1'])
    out['input-border'] = blend(out['background'], out['panel-border'], 0.52)
    out['logo-shadow'] = blend(out['logo'], BLACK, SHADOW_MIX)
    if contrast(out['logo-text'], out['logo']) < 1.5:
        out['logo-text'] = contrast_pick(out['logo'])
    for role in MASTER_ONLY:
        out[role] = master_rgb[role]

    if contrast(out['icons-highlight'], out['background']) < 1.5:
        out['icons-highlight'] = blend(out['icons-highlight'], WHITE, 0.25)
    out['switch-text'] = contrast_pick(out['icons-highlight'])
    out['highlight-text'] = out['switch-text']

    for role in ('main-text-1', 'main-text-2', 'list-names', 'list-urls'):
        out[role] = readable(out[role], out['background'], READ_TEXT)
    if contrast(out['main-text-2'], out['background']) < contrast(out['main-text-1'], out['background']):
        out['main-text-2'] = readable(out['main-text-2'], out['background'],
                                      min(20.0, contrast(out['main-text-1'], out['background']) + 1.0))
    # The list columns are the same two text tiers; keep them in step after the contrast pass.
    out['list-names'] = out['main-text-1']
    out['list-urls'] = out['main-text-2']
    for role in ('accent-text', 'icons-outline'):
        out[role] = readable(out[role], out['background'], READ_ACCENT)

    return {role: rgb_to_hex(out[role]) for role in ROLE_ORDER}


def build_palettes(themes, master, anchor, blackbg):
    """One final 19-role palette per theme, using the anchor's design offset."""
    master_rgb = {}
    for role in ROLE_ORDER:
        if role not in master:
            fail(f"style.css :root has no --{role}.")
        master_rgb[role] = hex_to_rgb(master[role])

    def ui_bg_for(t):
        return master_rgb['background'] if blackbg else t['background']

    anchor_feat = feature(themes[anchor], ui_bg_for(themes[anchor]), blackbg)
    return [transfer(feature(t, ui_bg_for(t), blackbg), master_rgb, anchor_feat) for t in themes]


def theme_deltas(computed, master):
    """Only the properties that differ from the master, in ROLE_ORDER."""
    out = {}
    for role in ROLE_ORDER:
        value = computed[role]
        base = master.get(role)
        if base is None or dist(hex_to_rgb(value), hex_to_rgb(base)) > NEAR_MASTER:
            out[role] = value
    return out


# ---------------------------------------------------------------------------
# style.css
# ---------------------------------------------------------------------------

MASTER_RE = re.compile(r':root\s*\{')
BLOCK_RE = re.compile(r':root\[data-theme="(\d+)"\]\s*\{')
COMMENT_RE = re.compile(r'/\*\s*(.*?)\s*\*/')
PROP_RE = re.compile(r'--([A-Za-z0-9-]+)\s*:\s*(#[0-9A-Fa-f]{3,8})')


def brace_end(text, open_pos):
    """Index just past the } matching text[open_pos] == '{'."""
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


def parse_style(text):
    """(master dict, blocks list).  A block has id/name/locked/spans/props."""
    m = MASTER_RE.search(text)
    if not m:
        fail("Could not find the :root master palette in style.css.")
    master_end = brace_end(text, m.end() - 1)
    if master_end == -1:
        fail("Could not find the end of the :root block.")
    master = {}
    for name, value in PROP_RE.findall(text[m.end():master_end - 1]):
        master[name] = value.lower()

    blocks = []
    for bm in BLOCK_RE.finditer(text):
        open_pos = bm.end() - 1
        end = brace_end(text, open_pos)
        if end == -1:
            fail(f"Unclosed theme block at offset {bm.start()}.")
        # The title comment sits after the opening {, on the same line.
        line_end = text.find('\n', open_pos)
        header = text[bm.start():line_end if line_end != -1 else end]
        cm = COMMENT_RE.search(header)
        props = {}
        for name, value in PROP_RE.findall(text[open_pos + 1:end - 1]):
            props[name] = value.lower()
        name = cm.group(1) if cm else None
        blocks.append({
            'id': int(bm.group(1)),
            'name': name,
            'locked': bool(name and LOCK_RE.search(name)),
            'name_start': bm.start() + cm.start(1) if cm else None,
            'name_end': bm.start() + cm.end(1) if cm else None,
            'start': bm.start(),
            'end': end,
            'props': props,
        })
    return master, blocks


def render_block(theme_id, name, props, eol):
    head = f':root[data-theme="{theme_id}"] {{ /* {name} */'
    if not props:
        return head + eol + '}'
    lines = [head]
    for role in ROLE_ORDER:
        if role in props:
            lines.append('  ' + ('--' + role + ':').ljust(22) + props[role] + ';')
    for role in sorted(props):
        if role not in ROLE_ORDER:              # hand-added property: keep it at the end
            lines.append('  ' + ('--' + role + ':').ljust(22) + props[role] + ';')
    lines.append('}')
    return eol.join(lines)


def set_mode_marker(text, mode, anchor, eol):
    """Keep one mode/anchor marker above the theme blocks."""
    marker = (f'/* webuitheme.py mode={mode} anchor={anchor} - colours imported from '
              f'src/displays/themes.h;' + eol +
              '   hand edits welcome, add PERFECT to lock a theme, --update compares this mode */')
    existing = MARKER_RE.search(text)
    if existing:
        end = text.find('*/', existing.start()) + 2
        return text[:existing.start()] + marker + text[end:]
    anchor_c = re.search(r'/\* Per-theme overrides[^\n]*\*/', text)
    if anchor_c:
        return text[:anchor_c.end()] + eol + marker + text[anchor_c.end():]
    m = MASTER_RE.search(text)
    end = brace_end(text, m.end() - 1) if m else -1
    if end == -1:
        return text
    return text[:end] + eol + marker + text[end:]


def mode_in(text):
    """(mode, anchor) from the marker; (None, None) when absent, anchor may be None."""
    m = MARKER_RE.search(text)
    if not m:
        return (None, None)
    return (m.group(1), int(m.group(2)) if m.group(2) is not None else None)


def apply_edits(text, edits):
    for start, end, replacement in sorted(edits, key=lambda e: e[0], reverse=True):
        text = text[:start] + replacement + text[end:]
    return text


def write_style(text):
    with open(STYLE_CSS, 'w', encoding='utf-8', newline='') as f:
        f.write(text)


def read_style():
    if not os.path.exists(STYLE_CSS):
        fail(f"{STYLE_CSS} not found.")
    with open(STYLE_CSS, 'r', encoding='utf-8', newline='') as f:
        text = f.read()
    return text, ('\r\n' if '\r\n' in text else '\n')


# ---------------------------------------------------------------------------
# Interaction
# ---------------------------------------------------------------------------

def ask_yes(prompt, default):
    suffix = ' [Y/n] ' if default else ' [y/N] '
    try:
        reply = input(prompt + suffix).strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        return default
    if reply == '':
        return default
    return reply in ('y', 'yes')


def ask_theme_id(candidates, prompt="Which theme? (blank to exit): "):
    while True:
        try:
            reply = input(prompt).strip()
        except (EOFError, KeyboardInterrupt):
            print()
            return None
        if reply == '':
            return None
        if reply.isdigit() and int(reply) in candidates:
            return int(reply)
        print("  Enter one of: " + ', '.join(str(i) for i in candidates))


def fail(message):
    print(f"ERROR: {message}")
    sys.exit(1)


# ---------------------------------------------------------------------------
# Stages
# ---------------------------------------------------------------------------

def stage_missing(text, eol, names, themes, master, anchor, blackbg, dry_run):
    """Add a block for every theme id that style.css does not have yet."""
    master_palette, blocks = parse_style(text)
    have = {b['id'] for b in blocks}
    missing = [i for i in range(len(names)) if i not in have]
    if not missing:
        print("Theme count: style.css already has a block for every theme.")
        return text, blocks

    print(f"\nMissing theme block(s) in style.css ({len(missing)}):")
    for i in missing:
        print(f"  [{i}] {names[i]}")

    palettes = build_palettes(themes, master_palette, anchor, blackbg)
    proposed = [(i, theme_deltas(palettes[i], master_palette)) for i in missing]

    if dry_run:
        print("\n--dry-run: blocks that would be added --")
        for i, props in proposed:
            print()
            print(render_block(i, names[i], props, '\n'))
        return text, blocks

    if not ask_yes(f"\nAdd {len(missing)} missing theme block(s)?", default=True):
        print("Cancelled. Nothing was changed.")
        return text, blocks

    # Insert every missing block at its id position, so the file stays in theme order.
    groups = {}
    for i, props in proposed:
        lower = [b for b in blocks if b['id'] < i]
        higher = [b for b in blocks if b['id'] > i]
        if lower:
            point, where = max(b['end'] for b in lower), 'after'
        elif higher:
            point, where = min(b['start'] for b in higher), 'before'
        else:
            anchor_c = re.search(r'/\* Per-theme overrides[^\n]*\*/', text)
            point = anchor_c.end() if anchor_c else brace_end(text, MASTER_RE.search(text).end() - 1)
            where = 'after'
        groups.setdefault((point, where), []).append(render_block(i, names[i], props, eol))

    edits = []
    for (point, where), rendered in groups.items():
        chunk = eol + eol.join(rendered) if where == 'after' else eol.join(rendered) + eol
        edits.append((point, point, chunk))
    text = apply_edits(text, edits)
    text = set_mode_marker(text, 'blackbg' if blackbg else 'normal', anchor, eol)
    write_style(text)
    print(f"Added {len(missing)} theme block(s).")
    text, eol = read_style()
    _, blocks = parse_style(text)
    return text, blocks


def stage_renames(text, eol, names, themes, master, anchor, blackbg, dry_run):
    """A comment name that no longer matches themes.h can be re-imported in place."""
    master_palette, blocks = parse_style(text)
    changed = [b for b in blocks
               if b['id'] < len(names) and b['name'] and not b['locked']
               and b['name'] != names[b['id']]]
    if not changed:
        print("Theme names: no renames to reconcile.")
        return text, blocks

    print(f"\nRenamed theme(s) ({len(changed)}):")
    for b in changed:
        print(f"  [{b['id']}] {b['name']!r} -> {names[b['id']]!r}")

    palettes = build_palettes(themes, master_palette, anchor, blackbg)
    edits = []
    for b in changed:
        props = theme_deltas(palettes[b['id']], master_palette)
        if dry_run:
            print(f"\n--dry-run: [{b['id']}] would become --")
            print(render_block(b['id'], names[b['id']], props, '\n'))
            continue
        if ask_yes(f"Overwrite [{b['id']}] {names[b['id']]!r} with the display theme?", default=False):
            edits.append((b['start'], b['end'], render_block(b['id'], names[b['id']], props, eol)))
        elif b['name_start'] is not None:
            edits.append((b['name_start'], b['name_end'], names[b['id']]))
    if dry_run:
        return text, blocks
    if edits:
        text = apply_edits(text, edits)
        text = set_mode_marker(text, 'blackbg' if blackbg else 'normal', anchor, eol)
        write_style(text)
        print(f"Reconciled {len(edits)} renamed theme(s).")
        text, eol = read_style()
    _, blocks = parse_style(text)
    return text, blocks


def theme_diff(computed_props, current_props):
    """(added, changed, removed) between the computed deltas and the block on disk."""
    added = {k: v for k, v in computed_props.items() if k not in current_props}
    changed = {k: (current_props[k], v) for k, v in computed_props.items()
               if k in current_props and current_props[k] != v}
    removed = {k: v for k, v in current_props.items() if k not in computed_props}
    return added, changed, removed


def print_diff(theme_id, name, added, changed, removed):
    print(f"\n[{theme_id}] {name}")
    for k, v in added.items():
        print(f"  + --{k:<18} (missing) -> {v}")
    for k, (old, new) in changed.items():
        print(f"  ~ --{k:<18} {old} -> {new}")
    for k, v in removed.items():
        print(f"  - --{k:<18} {v} -> (master; removed)")


def stage_update(text, eol, names, themes, master, anchor, blackbg, dry_run):
    """Element-by-element comparison for every editable theme."""
    palettes = build_palettes(themes, master, anchor, blackbg)
    master_palette, blocks = parse_style(text)
    by_id = {b['id']: b for b in blocks}

    def pending_list():
        pending = []
        for i in range(len(names)):
            b = by_id.get(i)
            if b is None or b['locked']:
                continue
            added, changed, removed = theme_diff(theme_deltas(palettes[i], master_palette), b['props'])
            if added or changed or removed:
                pending.append((i, added, changed, removed))
        return pending

    if dry_run:
        pending = pending_list()
        for i, added, changed, removed in pending:
            print_diff(i, names[i], added, changed, removed)
        if not pending:
            print("\nAll editable themes match themes.h.")
        return

    while True:
        pending = pending_list()
        if not pending:
            print("\nAll editable themes match themes.h.")
            return
        print(f"\nThemes with differences ({len(pending)}):")
        for i, _, _, _ in pending:
            print(f"  [{i}] {names[i]}")
        choice = ask_theme_id([i for i, _, _, _ in pending])
        if choice is None:
            print("Exited. Nothing was changed.")
            return
        _, added, changed, removed = next(p for p in pending if p[0] == choice)
        print_diff(choice, names[choice], added, changed, removed)
        if not ask_yes("Update this theme from themes.h?", default=False):
            continue
        props = theme_deltas(palettes[choice], master_palette)
        block = by_id[choice]
        text = apply_edits(text, [(block['start'], block['end'],
                                   render_block(choice, names[choice], props, eol))])
        text = set_mode_marker(text, 'blackbg' if blackbg else 'normal', anchor, eol)
        write_style(text)
        print(f"Updated [{choice}] {names[choice]}.")
        text, eol = read_style()
        master_palette, blocks = parse_style(text)
        by_id = {b['id']: b for b in blocks}


# ---------------------------------------------------------------------------

def main():
    argv = sys.argv[1:]
    if any(a in ('-h', '--help') for a in argv):
        print(__doc__)
        return

    update = False
    blackbg = False
    dry_run = False
    anchor = DEFAULT_ANCHOR
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '--update':
            update = True
        elif arg == '--blackbg':
            blackbg = True
        elif arg == '--dry-run':
            dry_run = True
        elif arg == '--anchor':
            if i + 1 >= len(argv) or not argv[i + 1].isdigit():
                fail("--anchor needs a theme number.")
            anchor = int(argv[i + 1])
            i += 1
        elif arg.startswith('--anchor='):
            value = arg.split('=', 1)[1]
            if not value.isdigit():
                fail("--anchor needs a theme number.")
            anchor = int(value)
        else:
            print(f"ERROR: unknown option: {arg}")
            print(__doc__)
            sys.exit(1)
        i += 1

    names, themes = load_themes()
    if not 0 <= anchor < len(themes):
        fail(f"--anchor {anchor} is out of range (0-{len(themes) - 1}).")

    text, eol = read_style()
    file_mode, file_anchor = mode_in(text)
    mode = 'blackbg' if blackbg else 'normal'
    if file_mode is not None:
        mode_differs = file_mode != mode
        anchor_differs = file_anchor is not None and file_anchor != anchor
        if mode_differs or anchor_differs:
            print(f"WARNING: style.css was generated with mode={file_mode} anchor={file_anchor}, "
                  f"this run is mode={mode} anchor={anchor}.")
            print("         --update would show every editable theme as changed.")
            if not dry_run and not ask_yes("Continue anyway?", default=False):
                print("Cancelled. Nothing was changed.")
                return

    master, blocks = parse_style(text)
    locked = sum(1 for b in blocks if b['locked'])
    if locked:
        print(f"Locked by PERFECT: {locked} theme(s), skipped.")

    text, blocks = stage_missing(text, eol, names, themes, master, anchor, blackbg, dry_run)

    # A different number of ids means stale blocks (a custom slot, or a deleted theme); stop before
    # the name stage, which assumes ids line up.
    master, blocks = parse_style(text)
    if {b['id'] for b in blocks} != set(range(len(names))):
        extra = sorted({b['id'] for b in blocks} - set(range(len(names))))
        if extra:
            print(f"\nNote: style.css has block(s) with no display theme: {', '.join(str(i) for i in extra)}.")
        if not dry_run:
            print("Theme counts still differ, so the rename and update stages are skipped.")
            return
        print("Theme counts still differ; --dry-run continues so the existing blocks can be compared.")

    text, blocks = stage_renames(text, eol, names, themes, master, anchor, blackbg, dry_run)

    if update:
        stage_update(text, eol, names, themes, master, anchor, blackbg, dry_run)

    if dry_run:
        print("\n--dry-run: nothing was written.")


if __name__ == '__main__':
    main()
