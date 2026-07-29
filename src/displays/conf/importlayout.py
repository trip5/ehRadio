#!/usr/bin/env python3
"""
Ingest a community yoRadio display conf file and either:
  - Create a brand new displayTFT{W}x{H}conf.h (if target doesn't exist)
  - Append new layout entry(s) to an existing target (if target exists)

USAGE:
    py importlayout.py <conf_file> --name "Name"              [--dry-run]
    py importlayout.py <conf_file> [<target.h>] --name "Name" [--dry-run]

OPTIONS:
    --name, -n       Layout display name (required)
    --dry-run        Write to .new.h file instead of modifying target
    --help, -h       Show this help

EXAMPLES (target doesn't exist -- creates new file):
    py importlayout.py krzxsiek-displayNV3007_142conf.h --name "krzxsiek"

EXAMPLES (target exists -- appends layout):
    py importlayout.py VaraiTamas-displayST7796conf.h --name "VaraiTamas"

WHAT IT DOES:
    - Parses old-style `const ScrollConfig foo PROGMEM = { ... };` OR new-style _layouts[]
    - Detects `#ifdef BOOMBOX_STYLE` / `#ifndef BOOMBOX_STYLE` / `#if defined(BOOMBOX_STYLE)`
      blocks and creates separate standard + BoomBox layouts
    - Detects `#define BOOMBOX_STYLE` standalone (mandatory BoomBox)
    - Detects `#define HIDE_X` -> zeros out the corresponding config (`{ }`)
      and preserves the original value as a comment
    - Detects `#if BITRATE_FULL` blocks -> extracts TITLE_FIX value, strips
      the block, and substitutes TITLE_FIX in config expressions
    - Preserves `#define` lines, format strings, and MoveConfig declarations
    - Handles both `//` and `/* */` style trailing comments on config lines
"""

import re, sys, os

# --- Canonical LayoutData field order ---------------------------------------
LAYOUT_FIELDS = [
    ('metaConf','ScrollConfig'),('title1Conf','ScrollConfig'),('title2Conf','ScrollConfig'),
    ('playlistConf','ScrollConfig'),('weatherConf','ScrollConfig'),
    ('metaBGConf','FillConfig'),('metaBGConfInv','FillConfig'),('volbarConf','FillConfig'),
    ('playlBGConf','FillConfig'),('bufferbarConf','FillConfig'),
    ('bitrateConf','WidgetConfig'),('voltxtConf','WidgetConfig'),
    ('batteryConf','WidgetConfig'),('iptxtConf','WidgetConfig'),('rssiConf','WidgetConfig'),
    ('numConf','WidgetConfig'),('clockConf','WidgetConfig'),
    ('vuConf','WidgetConfig'),
    ('fullbitrateConf','BitrateConfig'),
    ('bandsConf','VUBandsConfig'),
    ('clockMove','MoveConfig'),('weatherMove','MoveConfig'),('weatherMoveVU','MoveConfig'),
    ('boomboxStyle','bool'),
]

# Boot/AP fields — extracted from first layout, output as global BootData block
BOOT_FIELDS = [
    ('apTitleConf','ScrollConfig'),('apSettConf','ScrollConfig'),
    ('bootstrConf','WidgetConfig'),
    ('apNameConf','WidgetConfig'),('apName2Conf','WidgetConfig'),
    ('apPassConf','WidgetConfig'),('apPass2Conf','WidgetConfig'),
    ('bootWdtConf','WidgetConfig'),
    ('bootPrgConf','ProgressConfig'),
]

# Format strings required in every conf file (linker must find them)
REQUIRED_STRINGS = {
    'numtxtFmt':       '"%d"',
    'rssiFmt':         '"%d"',
    'iptxtFmt':        '""',
    'voltxtFmt':       '""',
    'batterytxtFmt':   '"%d%%"',
    'bitrateFmt':      '"%d"',
}

SECTION_COMMENTS = {
    'metaConf':       '/* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */',
    'metaBGConf':     '/* BACKGROUNDS         {{ left, top, fontsize, align }, width, height, outlined } */',
    'bootstrConf':    '/* WIDGETS             { left, top, fontsize, align } */',
    'fullbitrateConf':'/* CODEC BADGE         {{ left, top, fontsize, align }, dimension} - if empty, bitrateConf will be used instead */',
    'bandsConf':      '/* VU BANDS            { onebandwidth, onebandheight, bandsHspace, bandsVspace, numofbands, fadespeed } */',
    'clockMove':      '/* MOVES               { left, top, width (-1 keeps Conf position) */',
}

NAME_MAP = {'heapbarConf': 'bufferbarConf'}

HIDE_TO_FIELD = {
    'HIDE_TITLE2': 'title2Conf', 'HIDE_VU': 'vuConf', 'HIDE_VOLBAR': 'volbarConf',
    'HIDE_BUFFERBAR': 'bufferbarConf', 'HIDE_VOL': 'voltxtConf', 'HIDE_IP': 'iptxtConf',
    'HIDE_RSSI': 'rssiConf', 'HIDE_BATTERY': 'batteryConf', 'HIDE_WEATHER': 'weatherConf',
    'HIDE_HEAPBAR': 'bufferbarConf',
}


# --- Helpers -----------------------------------------------------------------

MAX_NAME_LEN = 50  # _layoutNames[][64] -- truncate at 50 for safety


def _trunc_name(n):
    if len(n) > MAX_NAME_LEN:
        truncated = n[:MAX_NAME_LEN]
        print(f"WARNING: Name '{n}' exceeds {MAX_NAME_LEN} chars, truncated to '{truncated}'")
        return truncated
    return n


def print_help():
    print(__doc__)

def _extract_title_fix_from_text(text):
    """Return TITLE_FIX value from BITRATE_FULL block, or None."""
    lines = text.split('\n')
    i = 0
    while i < len(lines):
        s = lines[i].strip()
        m_if = re.match(r'#if\s+BITRATE_FULL\s*$', s)
        m_ifndef = re.match(r'#ifndef\s+BITRATE_FULL\s*$', s)
        m_ifdef = re.match(r'#if\s+defined\s*\(\s*BITRATE_FULL\s*\)\s*$', s)
        if m_if or m_ifndef or m_ifdef:
            true_branch = (m_if or m_ifdef)
            nesting, j, else_idx, endif_idx = 1, i+1, -1, -1
            while j < len(lines):
                ss = lines[j].strip()
                if re.match(r'#if', ss): nesting += 1
                elif re.match(r'#endif', ss):
                    nesting -= 1
                    if nesting == 0: endif_idx = j; break
                elif re.match(r'#else', ss) and nesting == 1: else_idx = j
                j += 1
            if endif_idx == -1: i += 1; continue
            if true_branch: start = i+1; end = else_idx if else_idx!=-1 else endif_idx
            else: start = (else_idx+1) if else_idx!=-1 else endif_idx+1; end = endif_idx
            for k in range(start, end):
                tm = re.match(r'#define\s+TITLE_FIX\s+(\d+)', lines[k].strip())
                if tm: return int(tm.group(1))
            i = endif_idx + 1
        else: i += 1
    return None


def _parse_configs(text, boombox_active):
    """Extract config lines, taking the appropriate BOOMBOX_STYLE branch."""
    bbm = re.search(r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', text)
    boombox_in_if = True
    if bbm:
        directive = bbm.group(1)
        boombox_in_if = not directive.startswith('ndef')

    configs = []
    in_block, in_active_branch = False, True
    for line in text.split('\n'):
        s = line.strip()
        if re.match(r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', s):
            in_block = True
            in_active_branch = boombox_active if boombox_in_if else not boombox_active
            continue
        if in_block and s.startswith('#else'): in_active_branch = not in_active_branch; continue
        if in_block and s.startswith('#endif'): in_block = False; in_active_branch = True; continue
        if not in_active_branch: continue

        # Strip both // and /* */ style trailing comments before matching
        s_clean = re.sub(r'/\*.*?\*/', '', s)
        m = re.match(r'const\s+(\w+)\s+(\w+)\s+PROGMEM\s*=\s*(.+?);\s*(?://.*)?$', s_clean)
        if m:
            name = m.group(2)
            val = m.group(3).strip()
            configs.append((NAME_MAP.get(name, name), val))
    return configs


def parse_conf(path):
    """Parse a conf file. Returns dict with all parsed data."""
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        text = f.read()

    lines = text.split('\n')

    dw = re.search(r'#define\s+DSP_WIDTH\s+(\d+)', text)
    dh = re.search(r'#define\s+DSP_HEIGHT\s+(\d+)', text)
    width = int(dw.group(1)) if dw else None
    height = int(dh.group(1)) if dh else None

    # Header comment
    header_lines = []
    for line in lines:
        if line.strip().startswith('#ifndef '): break
        header_lines.append(line)
    header = '\n'.join(header_lines).rstrip()

    # Guard name
    guard = 'UNKNOWN'
    for line in lines:
        m = re.search(r'#ifndef\s+(\w+)', line)
        if m: guard = m.group(1); break

    # Extract #define lines
    defines = []
    defined_guard = False
    for line in lines:
        s = line.strip()
        if s == f'#ifndef {guard}': continue
        if s == f'#define {guard}': defined_guard = True; continue
        if not defined_guard: continue
        if s.startswith('#define') or s.startswith('#if') or s.startswith('#ifndef') or s.startswith('#else') or s.startswith('#endif'):
            defines.append(line)
        elif s.startswith('const ') or s.startswith('//const ') or s.startswith('// const '): break
        elif s and not s.startswith('/*') and not s.startswith('*') and not s.startswith('//'): break

    # Strip BITRATE_FULL/TITLE_FIX block from defines
    filtered, in_block, block_depth = [], False, 0
    for d in defines:
        s = d.strip()
        if re.match(r'#if\s+BITRATE_FULL|#ifndef\s+BITRATE_FULL|#if\s+defined\s*\(\s*BITRATE_FULL\s*\)', s):
            in_block = True; block_depth = 1; continue
        if in_block:
            if re.match(r'#if', s): block_depth += 1
            elif re.match(r'#endif', s):
                block_depth -= 1
                if block_depth == 0: in_block = False
            continue
        filtered.append(d)
    defines = filtered

    # Detect HIDE_* defines
    hidden_fields = set()
    for m in re.finditer(r'^#define\s+(HIDE_\w+)', text, re.MULTILINE):
        hide_name = m.group(1)
        if hide_name in HIDE_TO_FIELD:
            hidden_fields.add(HIDE_TO_FIELD[hide_name])

    # BOOMBOX_STYLE detection
    has_boombox = bool(re.search(
        r'#if(def\s+BOOMBOX_STYLE|ndef\s+BOOMBOX_STYLE|\s+defined\s*\(\s*BOOMBOX_STYLE\s*\))', text))
    boombox_mandatory = False
    if not has_boombox:
        boombox_mandatory = bool(re.search(r'^#define\s+BOOMBOX_STYLE\s*$', text, re.MULTILINE))

    # TITLE_FIX
    title_fix = _extract_title_fix_from_text(text)

    # Two-pass config parsing
    configs_normal = _parse_configs(text, boombox_active=False)
    configs_boombox = _parse_configs(text, boombox_active=True) if has_boombox else None

    # TITLE_FIX substitution
    if title_fix is not None:
        for cfgs in [configs_normal, configs_boombox]:
            if cfgs is None: continue
            for i, (n, v) in enumerate(cfgs):
                cfgs[i] = (n, v.replace('TITLE_FIX', str(title_fix)))

    # Format strings
    str_lines, in_strings = [], False
    for line in text.split('\n'):
        if '/* STRINGS' in line or '// STRINGS' in line: in_strings = True; continue
        if in_strings:
            if line.strip().startswith('#endif') or (line.strip().startswith('/*') and 'STRINGS' not in line): break
            if 'const char' in line: str_lines.append(line)

    return {
        'width': width, 'height': height,
        'header': header, 'guard': guard, 'defines': defines,
        'configs_normal': configs_normal, 'configs_boombox': configs_boombox,
        'str_lines': str_lines,
        'has_boombox': has_boombox, 'boombox_mandatory': boombox_mandatory,
        'hidden_fields': hidden_fields, 'title_fix': title_fix,
        'text': text,
    }


def resolve_target(width, height, target_arg, script_dir):
    """Return target conf filename or None."""
    if target_arg: return os.path.basename(target_arg)
    if width is None or height is None:
        print("ERROR: Cannot auto-detect -- DSP_WIDTH/DSP_HEIGHT not found.")
        print("The target output file must be specified.")
        return None
    TARGET_RE = re.compile(r'^display(TFT|OLED)(\d{2,4})x(\d{2,4})conf\.h$')
    candidates = []
    for fname in os.listdir(script_dir):
        m = TARGET_RE.match(fname)
        if m and int(m.group(2)) == width and int(m.group(3)) == height:
            candidates.append(fname)
    if len(candidates) == 1: return candidates[0]
    if len(candidates) > 1:
        print(f"ERROR: Multiple matching targets for {width}x{height}:")
        for c in candidates: print(f"  {c}")
    else:
        print(f"ERROR: No TFT/OLED target found for {width}x{height}.")
    print("The target output file must be specified (LCD targets require explicit path).")
    return None


def _normalize(val):
    val = val.strip()
    val = re.sub(r'\{\{(?!\s)', '{{ ', val)
    val = re.sub(r'(?<!\s)\}\}', ' }}', val)
    val = re.sub(r'\{(?![\s\{])', '{ ', val)
    val = re.sub(r'(?<![\s\}])\}', ' }', val)
    val = re.sub(r',\}', ', }', val)
    val = re.sub(r',([^\s])', r', \1', val)
    return val


def emit_layout_entry(name, configs, layout_fields, index, boombox_style=False, hidden_fields=None):
    if hidden_fields is None: hidden_fields = set()
    # Filter out boot fields — they're extracted separately into BootData block
    boot_names = set(f for f, _ in BOOT_FIELDS)
    configs = [(n, v) for n, v in configs if n not in boot_names]
    lines = [f'    {{   // {name}']
    seen_sections = set()
    config_dict = {c[0]: c[1] for c in configs}
    known_set = set(f for f, _ in layout_fields)
    unknown_map, trailing_unknowns, last_known = {}, [], None

    for cname, cval in configs:
        if cname in known_set: last_known = cname
        else:
            if last_known: unknown_map.setdefault(last_known, []).append((cname, cval))
            else: trailing_unknowns.append((cname, cval))

    converted, not_found = 0, 0
    unknown_count = len([c for c in configs if c[0] not in known_set])

    for fname, ftype in layout_fields:
        if fname in SECTION_COMMENTS and fname not in seen_sections:
            if SECTION_COMMENTS[fname]: lines.append(f'        {SECTION_COMMENTS[fname]}')
            seen_sections.add(fname)
        if fname in unknown_map:
            for uname, uval in unknown_map[fname]:
                pad = max(1, 28 - 8 - 7 - len(uname))
                lines.append(f'        // ??? {uname}{" " * pad} = {uval};')
        if fname in config_dict:
            val = config_dict[fname]
            if fname == 'boomboxStyle':
                if boombox_style:
                    lines.append('        /* BOOMBOX STYLE: middle-out VU */')
                    lines.append(f'        .{fname:19s} = true,')
                continue
            if fname in hidden_fields:
                lines.append(f'        .{fname:19s} = {{ }}, // unused')
                lines.append(f'        // .{fname:17s} = {_normalize(val)},')
            else:
                lines.append(f'        .{fname:19s} = {_normalize(val)},')
            converted += 1
        elif fname == 'boomboxStyle':
            if boombox_style:
                lines.append('        /* BOOMBOX STYLE: middle-out VU */')
                lines.append(f'        .{fname:19s} = true,')
            continue
        elif fname in hidden_fields:
            lines.append(f'        .{fname:19s} = {{ }}, // unused')
        else:
            lines.append(f'        .{fname:19s} = {{ }},                                                   // <-- NEEDS EDITING!')
            not_found += 1

    for uname, uval in trailing_unknowns:
        pad = max(1, 28 - 8 - 7 - len(uname))
        lines.append(f'        // ??? {uname}{" " * pad} = {uval};')

    lines.append('    },')
    return '\n'.join(lines), converted, not_found, unknown_count


def modify_target(target_path, entries, names):
    with open(target_path, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()
    # Find closing }; of _layouts[] — scan from _layouts[] decl, counting braces
    if not re.search(r'\b_layouts\[\]\s*PROGMEM\s*=\s*\{', content):
        print("ERROR: Could not find _layouts[]."); return False
    closing = content.rfind('\n};')  # last }; in file = _layouts[] closing
    if closing == -1: print("ERROR: Could not find closing '};' of _layouts[]."); return False
    nm = re.search(r'(const char _layoutNames\[\]\[64\] PROGMEM = )\{', content)
    if not nm: print("ERROR: Could not find _layoutNames."); return False
    nm_brace_end = content.find('\n};', nm.end())
    if nm_brace_end == -1: print("ERROR: Could not find closing } of _layoutNames."); return False
    block = content[nm.end():nm_brace_end]
    old = [n.strip().strip('"').strip(',').strip('"') for n in block.strip().split('\n') if n.strip()]
    new = old + names
    indented = '\n'.join(f'    "{n}",' for n in new)
    new_line = f'const char _layoutNames[][64] PROGMEM = {{\n{indented}\n}};'
    entries_text = '\n'.join(entries)
    new_content = content[:closing].rstrip('\n') + '\n' + entries_text + '\n' + content[closing:].lstrip('\n')
    new_content = new_content[:nm.start()] + new_line + new_content[nm_brace_end+1:]
    new_content = re.sub(r'\};(\};)+', '};', new_content)
    with open(target_path, 'w', encoding='utf-8') as f:
        f.write(new_content)
    return True


def create_target_file(out_path, data, name, dry_run):
    defines = data['defines']
    hidden_fields = data['hidden_fields']
    has_boombox = data['has_boombox']; boombox_mandatory = data['boombox_mandatory']
    str_lines = data['str_lines']; title_fix = data['title_fix']
    w, h = data['width'], data['height']

    # Derive guard from output filename: displayTFT428x142conf.h -> displayTFT428x142conf_h
    out_base = os.path.basename(out_path).replace('.h', '')
    guard = f'{out_base}_h'

    out = []
    out.append('/*************************************************************************************')
    out.append(f'    TFT{w}x{h} displays configuration file.')
    out.append('*************************************************************************************/')
    out.append('')
    out.append(f'#ifndef {guard}')
    out.append(f'#define {guard}')
    out.append('')
    for d in defines: out.append(d.rstrip())
    out.append('')
    out.append('// ******************** CHECK ALL #define LINES CAREFULLY! ********************')
    out.append('')

    # --- BootData block ---
    config_dict = {c[0]: c[1] for c in data['configs_normal']}
    boot_lines = ['const BootData _bootConfig PROGMEM = {']
    boot_section_done = set()
    for bf, btype in BOOT_FIELDS:
        if btype == 'ScrollConfig' and 'SCROLLS' not in boot_section_done:
            boot_lines.append('        /* SCROLLS             {{ left, top, fontsize, align }, buffsize, uppercase, width, scrolldelay, scrolldelta, scrolltime } */')
            boot_section_done.add('SCROLLS')
        elif btype in ('WidgetConfig', 'ProgressConfig') and 'WIDGETS' not in boot_section_done:
            boot_lines.append('        /* WIDGETS             { left, top, fontsize, align } */')
            boot_section_done.add('WIDGETS')
        if bf in config_dict:
            boot_lines.append(f'        .{bf:19s} = {_normalize(config_dict[bf])},')
        else:
            boot_lines.append(f'        .{bf:19s} = {{ }},')
    boot_lines.append('};')
    out.append('\n'.join(boot_lines))
    out.append('')

    if has_boombox:
        names = [f'"{_trunc_name(name)}"', f'"{_trunc_name(f"{name} (BoomBox)")}"']
    else:
        names = [f'"{_trunc_name(name)}"']
    indented = '\n'.join(f'    {n},' for n in names)
    out.append(f'const char _layoutNames[][64] PROGMEM = {{\n{indented}\n}};')
    out.append('')
    out.append('/* LAYOUT DEFINITIONS */')
    out.append('')
    out.append('const LayoutData _layouts[] PROGMEM = {')

    issues = False; idx = 0

    if has_boombox:
        et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, boombox_style=False, hidden_fields=hidden_fields)
        out.append(et)
        if nf>0 or uk>0: issues = True
        print(f"\nLayout: {_trunc_name(name)}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
        idx += 1
        et, c, nf, uk = emit_layout_entry(_trunc_name(f"{name} (BoomBox)"), data['configs_boombox'], LAYOUT_FIELDS, idx, boombox_style=True, hidden_fields=hidden_fields)
        out.append(et)
        if nf>0 or uk>0: issues = True
        print(f"\nLayout: {_trunc_name(f'{name} (BoomBox)')}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
    elif boombox_mandatory:
        et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, boombox_style=True, hidden_fields=hidden_fields)
        out.append(et)
        if nf>0 or uk>0: issues = True
        print(f"\nLayout: {_trunc_name(name)} (BoomBox mandatory)"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
    else:
        et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, hidden_fields=hidden_fields)
        out.append(et)
        if nf>0 or uk>0: issues = True
        print(f"\nLayout: {_trunc_name(name)}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")

    out.append('};')
    out.append('// ******************** CHECK ALL const char LINES CAREFULLY! ********************')
    out.append('// check all needed strings are present... and double-check octal codes \\0xx too!')
    out.append('// Note that rssi and battery will still render 2-glyph icons even when blank')
    out.append('')
    out.append('/* STRINGS */')

    # Helper to format a string declaration: PROGMEM at column 35
    def _fmt_str(sname, sval):
        suffix = '[][8]' if sname == 'batteryRangeFmt' else '[]'
        decl = f'const char {sname}{suffix}'
        return f'{decl}{" " * (35 - len(decl))} PROGMEM = {sval};'

    # Build set of existing string names from community file
    existing_strings = set()
    if str_lines:
        for s in str_lines:
            m = re.match(r'const\s+char\s+(\w+)\[', s.strip())
            if m:
                name = m.group(1)
                existing_strings.add(name)
                # Extract value
                val_m = re.search(r'=\s*(.+?);(?:\s*(?://|/\*).*)?$', s.strip())
                val = val_m.group(1).strip() if val_m else '""'
                out.append(_fmt_str(name, val))
        out.append('')

    # Add any required strings missing from the community file
    missing = []
    for sname, sdefault in REQUIRED_STRINGS.items():
        if sname not in existing_strings:
            missing.append(_fmt_str(sname, sdefault))
    if missing:
        out.append('// Automatically added by importlayout.py:')
        for m in missing: out.append(m)
        out.append('')

    out.append('#endif')
    out.append('')

    content = '\n'.join(out)
    actual = out_path + '.new.h' if dry_run else out_path
    with open(actual, 'w', encoding='utf-8') as f: f.write(content)
    if dry_run: print(f"\nDRY RUN -- wrote: {actual}")
    else: print(f"\nCreated: {actual}")
    if issues: print("Please review lines marked 'NEEDS EDITING!' before building.")
    if title_fix is not None: print(f"TITLE_FIX = {title_fix} applied.")


# --- Main --------------------------------------------------------------------

def main():
    argv = sys.argv[1:]
    name, dry_run, args = None, False, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ('--name', '-n'):
            if i+1 < len(argv): name = argv[i+1]; i += 2; continue
            else: print("ERROR: --name requires a value.\n"); print_help(); sys.exit(1)
        elif a.startswith('--name='): name = a.split('=',1)[1]; i += 1; continue
        elif a in ('--help', '-h'): print_help(); sys.exit(0)
        elif a == '--dry-run': dry_run = True; i += 1; continue
        else: args.append(a); i += 1

    if not args or not name:
        if not name: print("ERROR: --name is required.\n")
        print_help(); sys.exit(1)

    community_path = args[0]
    target_path = args[1] if len(args)>1 else None
    if not os.path.exists(community_path): print(f"ERROR: File not found: {community_path}"); sys.exit(1)

    data = parse_conf(community_path)
    if data['width'] is None or data['height'] is None:
        print("ERROR: DSP_WIDTH/DSP_HEIGHT not found."); sys.exit(1)

    # Guard: mandatory widgets must be present
    config_names = {n for n, _ in data['configs_normal']}
    missing_mandatory = [f for f in ('metaConf', 'playlistConf') if f not in config_names]
    if missing_mandatory:
        print(f"ERROR: Missing mandatory widget(s): {', '.join(missing_mandatory)}. Is this a valid conf file?")
        sys.exit(1)

    script_dir = os.path.dirname(os.path.abspath(__file__))
    basename = os.path.basename(community_path)

    # Determine target filename
    if target_path:
        out_name = os.path.basename(target_path)
    else:
        out_name = resolve_target(data['width'], data['height'], None, script_dir)
        if not out_name:
            # No existing match -- create new filename
            out_name = f"displayTFT{data['width']}x{data['height']}conf.h"
    target_full = os.path.join(script_dir, out_name)

    # OLED swap: if output is OLED and metaBGConfInv is empty, swap with metaBGConf
    if 'OLED' in out_name.upper():
        for cfgs in [data['configs_normal'], data['configs_boombox']]:
            if cfgs is None: continue
            bg_val = bg_inv_val = None
            for i, (name, val) in enumerate(cfgs):
                if name == 'metaBGConf': bg_val = (i, val)
                elif name == 'metaBGConfInv': bg_inv_val = (i, val)
            if bg_val and bg_inv_val:
                inv_val = bg_inv_val[1].strip()
                swap = inv_val == '{ }'
                if not swap:
                    hm = re.search(r'\}\s*,\s*(\d+)\s*,\s*(?:true|false)\s*\}', inv_val)
                    if hm and int(hm.group(1)) <= 1:
                        swap = True
                if swap:
                    cfgs[bg_val[0]] = ('metaBGConf', '{ }')
                    cfgs[bg_inv_val[0]] = ('metaBGConfInv', bg_val[1])

    if not os.path.exists(target_full):
        # Target doesn't exist -- create new file
        print(f"\nCreating {out_name} from {basename}...")
        create_target_file(target_full, data, name, dry_run)
    else:
        # Target exists -- append layouts
        print(f"\nAdding {basename} layout(s) to {out_name}...")

        with open(target_full, 'r', encoding='utf-8', errors='replace') as f: tc = f.read()
        existing_count = 0
        m = re.search(r'_layoutNames\[\]\[\d+\] PROGMEM = \{', tc)
        if m:
            s, e = m.end(), tc.find('};', m.end())
            if e > s: existing_count = tc[s:e].count(',') + 1 if tc[s:e].strip() else 1

        entries, names_to_add, issues, idx = [], [], False, existing_count

        if data['has_boombox']:
            et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, boombox_style=False, hidden_fields=data['hidden_fields'])
            idx += 1; entries.append(et); names_to_add.append(_trunc_name(name))
            if nf>0 or uk>0: issues = True
            print(f"\nLayout import complete [{idx-1}]: {_trunc_name(name)}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
            et, c, nf, uk = emit_layout_entry(_trunc_name(f"{name} (BoomBox)"), data['configs_boombox'], LAYOUT_FIELDS, idx, boombox_style=True, hidden_fields=data['hidden_fields'])
            idx += 1; entries.append(et); names_to_add.append(_trunc_name(f"{name} (BoomBox)"))
            if nf>0 or uk>0: issues = True
            print(f"\nLayout import complete [{idx-1}]: {_trunc_name(f'{name} (BoomBox)')}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
        elif data['boombox_mandatory']:
            et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, boombox_style=True, hidden_fields=data['hidden_fields'])
            entries.append(et); names_to_add.append(_trunc_name(name))
            if nf>0 or uk>0: issues = True
            print(f"\nLayout import complete [{idx}]: {_trunc_name(name)} (BoomBox mandatory)"); print(f"  {c} widgets, {nf} not found, {uk} unknown")
        else:
            et, c, nf, uk = emit_layout_entry(_trunc_name(name), data['configs_normal'], LAYOUT_FIELDS, idx, hidden_fields=data['hidden_fields'])
            entries.append(et); names_to_add.append(_trunc_name(name))
            if nf>0 or uk>0: issues = True
            print(f"\nLayout import complete [{idx}]: {_trunc_name(name)}"); print(f"  {c} widgets, {nf} not found, {uk} unknown")

        if dry_run:
            import shutil
            op = target_full + '.new.h'
            shutil.copy2(target_full, op)
            if not modify_target(op, entries, names_to_add): sys.exit(1)
            print(f"\nDRY RUN -- wrote: {op}")
        else:
            if not modify_target(target_full, entries, names_to_add): sys.exit(1)
        if issues: print("\nPlease manually edit the file before testing!\n")
        else: print("\nNo editing required!  (Really?)\n")


if __name__ == '__main__':
    main()
