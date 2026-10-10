#!/usr/bin/env python3
"""
Import theme files into themes.h.

Two input formats, told apart by the file itself:

  * an old-style yoRadio theme (`#define COLOR_*  r,g,b`), and
  * an ehRadio JSON export from the WebUI theme editor - the single-theme document `Export` downloads or
    the array `Backup All` downloads.

Both are appended to an existing src/displays/themes.h and both go through the same fallback rules for
anything they leave out, so a JSON that is missing a field is filled in exactly like an old colour file.
The old format's #ifdef/#ifndef branching still generates theme variants.

Every colour that lands in themes.h is forced onto the same 8-bit steps the WebUI editor offers: see
QUANTISING below.

USAGE:
    py importtheme.py <theme_file.h> --name "Name"      [--dry-run]
    py importtheme.py <theme.json>                      [--dry-run]
    py importtheme.py <theme_file.h> --name "Name" -n 1 [--dry-run]
    py importtheme.py --quantise                        [--dry-run]

OPTIONS:
    --name           Theme display name.  Required for the old format, optional for JSON, which carries its
                     own name - giving --name overrides that.
    -n, --number     Starting index (auto-detects if omitted)
    --quantise       Rewrite the colours already in themes.h onto the 565 steps, then exit.  -q is an
                     alias and --quantize is accepted too.
    --dry-run        Write to .new.h instead of modifying themes.h
    --help, -h       Show this help

EXAMPLES:
    py importtheme.py mytheme.h --name "Default"
    py importtheme.py krzxsiek_theme_gray.h --name "krzxsiek gray"
    py importtheme.py "ehRadioTheme-My Neon.json"
    py importtheme.py ehRadioThemes-Backup.json
    py importtheme.py ehRadioThemes-Backup.json --dry-run
    py importtheme.py --quantise --dry-run

QUANTISING:
    The panel stores RGB565 - 32 red levels, 64 green, 32 blue - so the 8-bit values are only buckets: every
    one of 248..255 is red level 31 and the panel draws the identical colour for all of them.  The WebUI
    editor therefore shows a level as `level * step`: 0, 8, 16 ... 248 for red and blue, 0, 4, 8 ... 252 for
    green.  This script writes those same numbers, so a theme file, the editor and the panel all agree.

    Quantising cannot change a pixel: `(r >> 3) * 8` packs back to the same 565 that `r` did, and a value
    that is already a step is left alone, which is what makes the mode safe to re-run and why it reports how
    many colours it moved.  A JSON export arrives already on the grid, because its values are 565 and the
    conversion is level times step; an old colour file and a computed fallback (a percentage of another
    colour) are not, and both are snapped on the way in.
"""

import json, re, sys, os

COLOR_TO_FIELD = {
    'COLOR_BACKGROUND':       'background',
    'COLOR_STATION_NAME':     'meta',
    'COLOR_STATION_BG':       'metabg',
    'COLOR_STATION_FILL':     'metafill',
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
    'COLOR_PL_CURRENT_FILL':  'plcurrentfill',
    'COLOR_PLAYLIST_0':       'playlist[0]',
    'COLOR_PLAYLIST_1':       'playlist[1]',
    'COLOR_PLAYLIST_2':       'playlist[2]',
    'COLOR_PLAYLIST_3':       'playlist[3]',
    'COLOR_PLAYLIST_4':       'playlist[4]',
}

FIELD_ORDER = [
    'background', 'meta', 'metabg', 'metafill',
    'title1', 'title2', 'digit', 'div', 'line', 'weather', 'vuaxis',
    'vupeak', 'vumax', 'vumin',
    'clock', 'clockbg', 'seconds', 'secondsbg', 'dow', 'date',
    'clockss', 'clockbgss', 'secondsss', 'secondsbgss', 'dowss', 'datess', 'textss',
    'buffer', 'ip', 'vol', 'rssi', 'battery', 'bitrate',
    'volbarout', 'volbarin',
    'plcurrent', 'plcurrentbg', 'plcurrentfill',
    'playlist',
]

SMART_FALLBACK = [
    ('vupeak',  'title1'),
    ('dow',     'date'),
    ('battery', 'rssi'),
]

# Computed fallbacks: (field, source_field, multiplier) — applied after SMART_FALLBACK.
# Resolved in dependency order so later entries can depend on earlier ones.
COMPUTED_FALLBACK = [
    # The two palette entries ehRadio added after the old theme format was defined.  Both come from
    # the divider, which every theme file sets, so both are rules rather than guesses - see
    # DERIVED_RULES.  line is the divider's own ink, used by the vertical and horizontal line
    # widgets; vuaxis is a quarter of it, which keeps the VU's reference lines (centre cross, bar
    # baseline, divider) visible in every theme, on black and on white alike.
    ('line',      'div',     1.00),   # the divider verbatim
    ('vuaxis',    'div',     0.25),   # 25% of the divider
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

# Computed values that are derived by a stated rule rather than guessed, so they are emitted
# without the '// needs fixing?' marker - it would appear on every line of every import otherwise.
# Every other computed fallback stays flagged, because those are eyeballed.
DERIVED_RULES = {'line', 'vuaxis', 'textss'}

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
    global_unknowns = []
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
            else:
                global_unknowns.append((name, rgb))

    # Step 4: generate variants
    n_blocks = len(blocks)
    if n_blocks == 0:
        return [{'colors': dict(global_colors), 'unknowns': list(global_unknowns), 'name_suffix': ''}], blocks

    results = []
    for combo in range(1 << n_blocks):
        active_lines = set()
        for bi in range(n_blocks):
            bit = (combo >> bi) & 1
            active_lines |= block_active[(bi, bit)]

        colors = dict(global_colors)
        unknowns = list(global_unknowns)
        for li, line in enumerate(lines):
            if li in active_lines:
                m = re.match(r'#define\s+(COLOR_\w+)\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)', line.strip())
                if m:
                    name = m.group(1)
                    rgb = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
                    if name in COLOR_TO_FIELD:
                        colors[COLOR_TO_FIELD[name]] = rgb
                    else:
                        unknowns.append((name, rgb))

        suffix = f" {combo+1}" if n_blocks > 0 else ""
        results.append({'colors': colors, 'unknowns': unknowns, 'name_suffix': suffix})

    return results, blocks


def parse_theme_json(path):
    """One or more themes out of a WebUI export.

    Two shapes, both written by the device: the single-theme document `Export` downloads, and the array
    `Backup All` downloads.  A plain object is the only thing that can be a single theme, so an array is
    always a list - and an empty one, or an older backup with nulls in it, is handled rather than trusted.

    The keys are the element names from themes.h with a leading dot, so the mapping is a strip and nothing
    else; a key that is not one of ours comes back as unknown, which is where the fallbacks and the report
    notice it.  Values are 565 integers converted to the numbers the editor shows, and an [r, g, b] array is
    accepted in place of one, the same tolerance the firmware's own loader has for a hand-edited file.
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

        colors, unknowns = {}, []
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
            else:
                unknowns.append((key, rgb))

        name = d.get('name') if isinstance(d.get('name'), str) and d.get('name').strip() else None
        results.append({'colors': colors, 'unknowns': unknowns, 'name_suffix': '', 'name': name})

    return results


def emit_theme_entry(name, data, index):
    colors = data['colors']       # original colors from theme file (do not mutate)
    unknowns = data['unknowns']

    # Working copy that we fill with fallbacks
    final = dict(colors)
    needs_fixing = set()

    # ---- smart fallbacks (dependency order) ----
    for field, source in SMART_FALLBACK:
        if field not in final and source in final:
            final[field] = final[source]
            needs_fixing.add(field)

    # ---- computed fallbacks (dependency order) ----
    for field, source, pct in COMPUTED_FALLBACK:
        if field not in final and source in final:
            sr, sg, sb = final[source]
            final[field] = (int(sr * pct), int(sg * pct), int(sb * pct))
            if field not in DERIVED_RULES:
                needs_fixing.add(field)

    # ---- meta fallback for everything still missing ----
    meta = final.get('meta', (0, 0, 0))
    for fname in FIELD_ORDER:
        if fname == 'playlist':
            for i in range(5):
                k = f'playlist[{i}]'
                if k not in final:
                    final[k] = meta
                    needs_fixing.add(k)
        elif fname not in final:
            final[fname] = meta
            needs_fixing.add(fname)

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
    lines = [f'    {{   // {name}']
    for fname in FIELD_ORDER:
        if fname == 'playlist':
            vals = []
            any_fix = False
            for i in range(5):
                k = f'playlist[{i}]'
                r, g, b = final[k]
                vals.append(f'RGB({r:3d}, {g:3d}, {b:3d})')
                if k in needs_fixing:
                    any_fix = True
            comment = ' // needs fixing?' if any_fix else ''
            lines.append(f'        .{fname:13s} = {{{", ".join(vals)}}},{comment}')
        else:
            r, g, b = final[fname]
            comment = ' // needs fixing?' if fname in needs_fixing else ''
            lines.append(f'        .{fname:13s} = RGB({r:3d}, {g:3d}, {b:3d}),{comment}')

    for uname, (r, g, b) in unknowns:
        lines.append(f'        // ??? (Unused by ehRadio) {uname} = RGB({r}, {g}, {b})')
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


def quantise_themes(target_path, dry_run):
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

    out_path = target_path + '.new.h' if dry_run else target_path
    with open(out_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    print(("DRY RUN -- wrote: " if dry_run else "Updated: ") + out_path)
    return True


def main():
    argv = sys.argv[1:]
    name, dry_run, start_idx, quantise_mode = None, False, None, False
    args = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--name',):
            if i+1 < len(argv): name = argv[i+1]; i += 2; continue
            else: print("ERROR: --name requires a value.\n"); print_help(); sys.exit(1)
        elif a.startswith('--name='): name = a.split('=',1)[1]; i += 1; continue
        elif a in ('-n', '--number'):
            if i+1 < len(argv): start_idx = int(argv[i+1]); i += 2; continue
            else: print("ERROR: -n requires a value.\n"); print_help(); sys.exit(1)
        elif a in ('--help', '-h'): print_help(); sys.exit(0)
        elif a == '--dry-run': dry_run = True; i += 1; continue
        elif a in ('--quantise', '--quantize', '-q'): quantise_mode = True; i += 1; continue
        else: args.append(a); i += 1

    script_dir = os.path.dirname(os.path.abspath(__file__))
    target_full = os.path.join(script_dir, 'themes.h')
    if not os.path.exists(target_full): print(f"ERROR: themes.h not found at {target_full}"); sys.exit(1)

    # --quantise is about the file itself, so it takes no input and is answered before anything else.
    if quantise_mode:
        if args:
            print("ERROR: --quantise works on themes.h itself and takes no input file.\n")
            print_help(); sys.exit(1)
        sys.exit(0 if quantise_themes(target_full, dry_run) else 1)

    if not args:
        print("ERROR: no input file given.\n"); print_help(); sys.exit(1)
    theme_path = args[0]
    if not os.path.exists(theme_path): print(f"ERROR: File not found: {theme_path}"); sys.exit(1)

    # The file says which format it is: an export starts with { or [ once the whitespace is gone, while an
    # old-style theme file starts with a comment or a #define.
    with open(theme_path, 'r', encoding='utf-8', errors='replace') as f:
        head = f.read().lstrip()
    basename = os.path.basename(theme_path)

    if head[:1] in ('{', '['):
        results = parse_theme_json(theme_path)
    else:
        if not name:
            print("ERROR: --name is required for an old-style theme file.\n")
            print_help(); sys.exit(1)
        results, _blocks = parse_theme(theme_path)
        if not any(r['colors'] for r in results):
            print(f"ERROR: No COLOR_* defines found in {basename}. Is this an old-style theme file?")
            sys.exit(1)

    if start_idx is None:
        with open(target_full, 'r', encoding='utf-8', errors='replace') as f: tc = f.read()
        idx = 0
        m = re.search(r'_themeNames\[\]\[\d+\] PROGMEM = \{', tc)
        if m:
            s, e = m.end(), tc.find('};', m.end())
            if e > s: idx = tc[s:e].count(',') + 1 if tc[s:e].strip() else 1
    else:
        idx = start_idx

    # A JSON theme is named by its own document unless --name overrides it; an old-style file has no name, so
    # it needs one.  Either way more than one theme is numbered the way the old format's variants are, because
    # a backup often holds several saves of the same theme and they all carry the same name inside.
    numbered = len(results) > 1
    entries, names_to_add, issues = [], [], False
    print(f"\nAdding {basename} theme(s) to themes.h...")

    for position, result in enumerate(results):
        given = name or result.get('name')
        base = given or os.path.splitext(basename)[0]
        if not given:
            print(f"WARNING: that theme carries no name - using the file name '{base}'")
        theme_name = _trunc_name(f"{base}{result.get('name_suffix') or (f' {position + 1}' if numbered else '')}")
        entry_text, converted, moved = emit_theme_entry(theme_name, result, idx)
        entries.append(entry_text)
        names_to_add.append(theme_name)
        if result['unknowns']: issues = True
        print(f"\nTheme: {theme_name}")
        print(f"  {len(result['colors'])} colors, {len(result['unknowns'])} unknown")
        if moved: print(f"  {moved} colour(s) snapped onto the 565 steps")
        idx += 1

    known = set(existing_names(target_full))
    for n in names_to_add:
        if n in known:
            print(f"WARNING: '{n}' is already in _themeNames - this adds a second entry with that name.")

    if dry_run:
        import shutil
        op = target_full + '.new.h'
        shutil.copy2(target_full, op)
        if not modify_themes(op, entries, names_to_add): sys.exit(1)
        print(f"\nDRY RUN -- wrote: {op}")
    else:
        if not modify_themes(target_full, entries, names_to_add): sys.exit(1)
        print(f"\nUpdated: {target_full}")
    if issues: print("Please review lines marked with // ??? before building!")


if __name__ == '__main__':
    main()
