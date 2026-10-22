#!/usr/bin/env python3
"""
webuitheme.py — import display themes into the WebUI palette.

USAGE:
    py webuitheme.py                    # Add any display theme from themes.h missing from style.css, then reconcile renames
    py webuitheme.py --update           # ...then compare every editable theme, element by element
    py webuitheme.py --blackbg --dry-run
    py webuitheme.py --inflate          # give every block every role, copied from :root
    py webuitheme.py --deflate          # drop a block's roles that exactly match :root
    py webuitheme.py --deflate-lossy    # the same, asking [y/N] for near-matches
    py webuitheme.py -h | --help

    --inflate, --deflate and --deflate-lossy are standalone maintenance modes: they never read
    themes.h, they also process PERFECT blocks, and they cannot be combined with the other flags.
    `--inflate` spells every role out at the colour it ALREADY has in that block; `--deflate` removes
    what repeats `:root` and then folds each family back into one head line, keeping any member that
    deliberately differs.  Running the two in either order changes nothing on screen.

WHAT IT READS AND WRITES:
    src/displays/themes.h          -> the display colours
    data/www/style.css             -> :root is the master design; each theme is a delta block
    The tool never writes :root and never deletes or reorders a block.

HOW A THEME IS IMPORTED:
    The master is the WebUI rendering of ONE display theme - the one whose block is empty, which is
    exactly what `--deflate` leaves behind after a palette is copied into `:root`.  For every theme
    the tool takes a per-role "feature" from the display, measures the master's offset from those
    anchor features, and adds that offset to the theme's features:

        final(role) = feature(role, theme) + (master(role) - feature(role, anchor))

    The anchor therefore comes out as an empty block - the master already paints it - while every
    other theme keeps the master's design offsets and follows its own display.

FAMILIES IN :root:
    `:root` is organised in families: a head such as `--fills` or `--infotexts` holds the one colour and every
    member role points straight at its head.  A head is never painted by a rule, so a name that appears
    only in `:root` is a head.  A head is NOT a role: `ROLE_ORDER` lists the members only, and no palette
    is generated for a head.  A head may not appear in a page, but a BLOCK may declare one - `--outlines:
    #e3d25f` recolours that whole family in one line, which is how a derived theme stays short.
    Values are resolved before any comparison - block declarations first, then `:root` - so a block that
    spells out `#262626` where `:root` says `var(--fills)` is redundant, and `--deflate` folds it back into
    the head.  The generator always writes hex.  A theme splits a member by declaring that member on top of
    the head; the split stays put through both maintenance modes.

LOCKING A THEME:
    A block whose title contains the whole word PERFECT is inert for the import stages: it is never
    added, renamed, compared or listed for update.  Delete the word to make it editable again.  The
    maintenance modes do process it - they only move declarations between a block and `:root`.

THE TRANSLATION (feature map, calibrated from the current :root):
    panel-background  <- background          panel-gradient  <- background darkened
    panel-border      <- line (softened)     control-bg      <- metabg
    button/input/select/meter fills -> metabg
    buttons, inputs, dropdown text, headings, links, names, outlines, icons -> title1
    main-text, caption-text, editor-cell-text, link-quiet, section-title-text,
    switch-off-text   <- title2
    small-text, meta-text, info-text, eq-label-text -> title1
    list-names        <- title2              list-urls       <- title1
    highlight-fill    <- clock darkened      logo            <- clock
    logo-text         <- metabg              input-border / logo-shadow / on-highlight text derived

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
HIGHLIGHT_MIX = 0.28     # highlight-fill = clock darkened by this much
SHADOW_MIX = 0.16        # logo-shadow = logo darkened by this much
SURFACE_MIX = 0.16       # blackbg control-bg = master background lifted by this much
PANEL_MIX = 0.15         # panel-gradient = background darkened by this much
BLACK = (0, 0, 0)
WHITE = (255, 255, 255)

# The roles a theme may override - the MEMBERS of the :root families, never a head.  Alphabetical, and
# `render_block()` sorts the declarations too, so a block reads the same way as a family block in `:root`.
# The fixed colours (`page-bg`, `list-bars`, `playlist-hover`) are deliberately absent: never themed.
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

    highlight = blend(clock, BLACK, HIGHLIGHT_MIX)

    # One raw colour per role.  The contrast passes in transfer() replace the text roles; the
    # placeholder values here only give the HSL transfer something to measure the offset against.
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
        'logo-text': metabg,
        'logo-shadow': logo,
        'warning-outline': main_text_2,
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


def build_palettes(themes, master, anchor, blackbg):
    """One final palette per theme, using the anchor's design offset.  `master` is hex already."""
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
PROP_RE = re.compile(r'--([A-Za-z0-9-]+)\s*:\s*(#[0-9A-Fa-f]{3,8}|var\(\s*--[A-Za-z0-9-]+\s*\))')
VAR_RE = re.compile(r'var\(\s*--([A-Za-z0-9-]+)\s*\)')


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
        fail(f"CSS variable loop in style.css: {' -> '.join(seen + (name,))}")
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


def find_anchor(blocks, marker_anchor):
    """The display theme whose block is empty - the one :root is the WebUI rendering of."""
    empty = sorted(b['id'] for b in blocks if not b['props'])
    if empty:
        if len(empty) > 1:
            print(f"Note: several blocks are empty ({', '.join(str(i) for i in empty)}); "
                  f"using [{empty[0]}] as the master's reference.")
        return empty[0]
    if marker_anchor is not None:
        return marker_anchor
    print("WARNING: no block is empty, so the master's display theme cannot be inferred; assuming [0].")
    print("         Copy a palette into :root and run --deflate to establish it.")
    return 0


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
    """A block in alphabetical order - the same order as `:root`, hand-added properties included."""
    head = f':root[data-theme="{theme_id}"] {{ /* {name} */'
    if not props:
        return head + eol + '}'
    lines = [head]
    for role in sorted(props):
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


def validate_props(text):
    """Every palette declaration must begin its line.

    A stray character in front of `--role:` makes the declaration invalid CSS, and the browser drops
    it silently: the role then falls back to whatever its alias resolves to, which is very hard to
    see.  Catching it at read time is the difference between a five-second error and a colour hunt.
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
            fail(f"style.css line {line}: unexpected {prefix.strip()!r} before --{m.group(1)}.\n"
                 f"       A palette declaration must start its line; only indentation is allowed.")


def read_style():
    if not os.path.exists(STYLE_CSS):
        fail(f"{STYLE_CSS} not found.")
    with open(STYLE_CSS, 'r', encoding='utf-8', newline='') as f:
        text = f.read()
    validate_props(text)
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
    master_palette = resolve_master(master_palette)
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
    master_palette = resolve_master(master_palette)
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
    raw_master, blocks = parse_style(text)
    families = find_families(raw_master)
    master_palette = resolve_master(raw_master)
    by_id = {b['id']: b for b in blocks}

    def pending_list():
        pending = []
        for i in range(len(names)):
            b = by_id.get(i)
            if b is None or b['locked']:
                continue
            # A head line is not a role, so it is never a difference against themes.h.
            current = {k: v for k, v in resolve_block(b, master_palette).items() if k in ROLE_ORDER}
            added, changed, removed = theme_diff(theme_deltas(palettes[i], master_palette), current)
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
        collapse_families(props, master_palette, families)   # write it short, one line per family
        block = by_id[choice]
        text = apply_edits(text, [(block['start'], block['end'],
                                   render_block(choice, names[choice], props, eol))])
        text = set_mode_marker(text, 'blackbg' if blackbg else 'normal', anchor, eol)
        write_style(text)
        print(f"Updated [{choice}] {names[choice]}.")
        text, eol = read_style()
        raw_master, blocks = parse_style(text)
        families = find_families(raw_master)
        master_palette = resolve_master(raw_master)
        by_id = {b['id']: b for b in blocks}


# ---------------------------------------------------------------------------
# Maintenance modes: style.css only, no themes.h, PERFECT blocks included
# ---------------------------------------------------------------------------

def run_maintenance(text, eol, master_raw, blocks, mode, dry_run):
    """--inflate / --deflate / --deflate-lossy.  Works on every block, locked or not."""
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
                    if dry_run:
                        print(f"  [{block['id']}] --{name}: {have} is {distance:.1f} from master {want}")
                        removed[name] = value
                    elif ask_yes(f"  [{block['id']}] remove --{name} "
                                 f"({have}, {distance:.1f} from master {want})?", default=False):
                        removed[name] = value

        if mode == 'inflate':
            for role in ROLE_ORDER:
                if role in props:
                    continue
                # The value the role already resolves to in THIS block, so a block that declares a
                # head keeps its cascade instead of being shadowed by the master's hex.
                value = resolve_value(master_raw.get(role, 'var(--' + role + ')'),
                                      props, master_raw)
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
        if dry_run:
            verb = 'add' if mode == 'inflate' else 'remove'
            print(f"[{block['id']}] {block['name']}: would {verb} {len(added) + len(removed)} role(s)")
            continue
        edits.append((block['start'], block['end'],
                      render_block(block['id'], block['name'], props, eol)))

    if mode == 'inflate':
        verb, noun = 'Inflated', 'added'
    else:
        verb, noun = 'Deflated', 'removed'
    if dry_run:
        print(f"\n--dry-run: {touched_blocks} theme(s) would change, "
              f"{touched_props} declaration(s) {noun}.")
        return text
    text = apply_edits(text, edits)
    write_style(text)
    print(f"{verb} {touched_blocks} theme(s), {noun} {touched_props} declaration(s).")
    return text


# ---------------------------------------------------------------------------

def main():
    argv = sys.argv[1:]
    if any(a in ('-h', '--help') for a in argv):
        print(__doc__)
        return

    update = False
    blackbg = False
    dry_run = False
    maintenance = None
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '--update':
            update = True
        elif arg == '--blackbg':
            blackbg = True
        elif arg == '--dry-run':
            dry_run = True
        elif arg in ('--inflate', '--deflate', '--deflate-lossy'):
            if maintenance is not None:
                fail("choose only one of --inflate, --deflate, --deflate-lossy.")
            maintenance = arg[2:]
        else:
            print(f"ERROR: unknown option: {arg}")
            print(__doc__)
            sys.exit(1)
        i += 1

    if maintenance and (update or blackbg):
        fail(f"--{maintenance} cannot be combined with --update or --blackbg.")

    text, eol = read_style()
    file_mode, file_anchor = mode_in(text)

    if maintenance:
        master_raw, blocks = parse_style(text)
        print(f"Maintenance mode: --{maintenance} (PERFECT blocks included).")
        run_maintenance(text, eol, master_raw, blocks, maintenance, dry_run)
        if dry_run:
            print("\n--dry-run: nothing was written.")
        return

    names, themes = load_themes()
    master_raw, blocks = parse_style(text)
    anchor = find_anchor(blocks, file_anchor)
    if not 0 <= anchor < len(themes):
        fail(f"The inferred anchor {anchor} is out of range (0-{len(themes) - 1}).")
    print(f"Master reference: display theme [{anchor}] {names[anchor]}")

    mode = 'blackbg' if blackbg else 'normal'
    if file_mode is not None:
        if file_mode != mode:
            print(f"WARNING: style.css was generated with mode={file_mode}, this run is mode={mode}.")
            print("         --update would show every editable theme as changed.")
            if not dry_run and not ask_yes("Continue anyway?", default=False):
                print("Cancelled. Nothing was changed.")
                return
        elif file_anchor is not None and file_anchor != anchor:
            print(f"Note: the marker says anchor={file_anchor}, but [{anchor}] is the empty block; "
                  f"using [{anchor}].")

    master = resolve_master(master_raw)
    locked = sum(1 for b in blocks if b['locked'])
    if locked:
        print(f"Locked by PERFECT: {locked} theme(s), skipped.")

    text, blocks = stage_missing(text, eol, names, themes, master, anchor, blackbg, dry_run)

    # A different number of ids means stale blocks (a custom slot, or a deleted theme); stop before
    # the name stage, which assumes ids line up.
    master, blocks = parse_style(text)
    master = resolve_master(master)
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
