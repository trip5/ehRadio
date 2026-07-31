#!/usr/bin/env python3
"""
Import old-style theme files (#define COLOR_*) into themes.h.

Always appends to an existing src/displays/themes.h.  Handles #ifdef/#ifndef
branching to generate multiple theme variants.

USAGE:
    py importtheme.py <theme_file.h> --name "Name"      [--dry-run]
    py importtheme.py <theme_file.h> --name "Name" -n 1 [--dry-run]

OPTIONS:
    --name           Theme display name (required)
    -n, --number     Starting index (auto-detects if omitted)
    --dry-run        Write to .new.h instead of modifying themes.h
    --help, -h       Show this help

EXAMPLES:
    py importtheme.py mytheme.h --name "Default"
    py importtheme.py krzxsiek_theme_gray.h --name "krzxsiek gray"
    py importtheme.py krzxsiek_theme_gray.h --name "krzxsiek gray" --dry-run
"""

import re, sys, os

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
    'title1', 'title2', 'digit', 'div', 'weather',
    'vumax', 'vumin',
    'clock', 'clockbg', 'seconds', 'dow', 'date',
    'clockss', 'clockbgss', 'secondsss', 'dowss', 'datess',
    'buffer', 'ip', 'vol', 'rssi', 'battery', 'bitrate',
    'volbarout', 'volbarin',
    'plcurrent', 'plcurrentbg', 'plcurrentfill',
    'playlist',
]

SMART_FALLBACK = [
    ('dow',     'date'),
    ('battery', 'rssi'),
]

# Computed fallbacks: (field, source_field, multiplier) — applied after SMART_FALLBACK.
# Resolved in dependency order so later entries can depend on earlier ones.
COMPUTED_FALLBACK = [
    ('clockbg',   'clock',   0.15),   # 15% of clock
    ('clockss',   'clock',   0.50),   # 50% of clock
    ('secondsss', 'seconds', 0.50),   # 50% of seconds
    ('dowss',     'dow',     0.50),   # 50% of dow
    ('datess',    'date',    0.50),   # 50% of date
    ('clockbgss', 'clockss', 0.15),   # 15% of clockss (resolved above)
]

MAX_NAME_LEN = 50  # _themeNames[][64] -- truncate at 50 for safety


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
        lines.append(f'        // ??? {uname} = RGB({r}, {g}, {b})')
    lines.append('    },')
    return '\n'.join(lines), len(colors)


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


def main():
    argv = sys.argv[1:]
    name, dry_run, start_idx = None, False, None
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
        else: args.append(a); i += 1
    if not args or not name:
        if not name: print("ERROR: --name is required.\n")
        print_help(); sys.exit(1)

    theme_path = args[0]
    if not os.path.exists(theme_path): print(f"ERROR: File not found: {theme_path}"); sys.exit(1)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    target_full = os.path.join(script_dir, 'themes.h')
    if not os.path.exists(target_full): print(f"ERROR: themes.h not found at {target_full}"); sys.exit(1)

    results, blocks = parse_theme(theme_path)
    basename = os.path.basename(theme_path)

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

    entries, names_to_add, issues = [], [], False
    print(f"\nAdding {basename} theme(s) to themes.h...")

    for result in results:
        theme_name = _trunc_name(f"{name}{result['name_suffix']}")
        entry_text, converted = emit_theme_entry(theme_name, result, idx)
        entries.append(entry_text)
        names_to_add.append(theme_name)
        if result['unknowns']: issues = True
        print(f"\nTheme: {theme_name}")
        print(f"  {len(result['colors'])} colors, {len(result['unknowns'])} unknown")
        idx += 1

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
