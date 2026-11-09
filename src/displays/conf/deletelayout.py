#!/usr/bin/env python3
"""
deletelayout.py — List or delete layouts from a conf file.

USAGE:
    py deletelayout.py                              # Pick a conf file, then a layout, then delete
    py deletelayout.py <conf_file>                  # List layouts with indices
    py deletelayout.py <conf_file> <index>          # Delete layout at index (0-based)
    py deletelayout.py -h | --help                  # Show this help

Deleting ALWAYS asks for confirmation — including when the index is passed on
the command line.  There is no bypass flag.

EXAMPLES:
    py deletelayout.py
    py deletelayout.py displayOLED128x64conf.h
    py deletelayout.py displayOLED128x64conf.h 3
"""

import re, sys, os

# Force UTF-8 output on Windows
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')


def _strip_block_comments(text):
    """Remove C block comments (/* */) — keep // line comments."""
    return re.sub(r'/\*.*?\*/', '', text, flags=re.DOTALL)


def _parse_names(content):
    """Return list of layout names from _layoutNames block."""
    m = re.search(r'const char _layoutNames\[\]\[\d+\] PROGMEM = \{', content)
    if not m:
        print("ERROR: Could not find _layoutNames.")
        sys.exit(1)
    brace_end = content.find('\n};', m.end())
    if brace_end == -1:
        print("ERROR: Could not find closing } of _layoutNames.")
        sys.exit(1)
    block = content[m.end():brace_end]
    names = [n.strip().strip('"').strip(',').strip('"') for n in block.strip().split('\n') if n.strip()]
    return names, m.start(), brace_end


def _parse_entries(content):
    """Return list of (start_pos, end_pos) for each layout entry in _layouts[].

    Works on content with /* */ comments already stripped so brace counting
    is not confused by braces inside block comments.
    """
    m = re.search(r'const LayoutData _layouts\[\]\s*PROGMEM\s*=\s*\{', content)
    if not m:
        print("ERROR: Could not find _layouts[].")
        sys.exit(1)
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
            ch = content[i]
            if ch == '{': depth += 1
            elif ch == '}': depth -= 1
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
    return entries, m.start()


def _conf_files(script_dir):
    """Return the sorted list of display*conf.h files in script_dir."""
    return sorted(f for f in os.listdir(script_dir)
                  if f.startswith('display') and f.endswith('conf.h'))


def list_conf_files(script_dir):
    """Print the conf files numbered the same way as layouts.  Returns the list."""
    files = _conf_files(script_dir)
    print()
    if not files:
        print(f"No display*conf.h files found in {script_dir}.")
        return files
    print(f"Conf files in {script_dir} ({len(files)}):")
    print()
    for i, fname in enumerate(files):
        print(f"[{i}] {fname}")
    return files


def _ask_index(prompt, count, what):
    """Prompt for a 0-based index into 'count' entries.  Blank/EOF/^C cancels (None)."""
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
        print(f"  Please enter 0-{count - 1} for the {what}, or blank to cancel.")


def interactive_delete(script_dir):
    """No-argument flow: choose a conf file, choose a layout, then delete it."""
    files = list_conf_files(script_dir)
    if not files:
        return
    fi = _ask_index(f"\nChoose a conf file [0-{len(files) - 1}] (blank to cancel): ", len(files), "conf file")
    if fi is None:
        print("Cancelled. Nothing was changed.")
        return
    target = os.path.join(script_dir, files[fi])

    names = list_layouts(target)
    if not names:
        return
    li = _ask_index(f"\nDelete which layout? [0-{len(names) - 1}] (blank to cancel): ", len(names), "layout")
    if li is None:
        print("Cancelled. Nothing was changed.")
        return

    print()
    delete_layout(target, li)


def list_layouts(target):
    """List the layouts in a conf file, numbered.  Returns the names list."""
    if not os.path.exists(target):
        print(f"ERROR: {target} not found.")
        sys.exit(1)
    with open(target, 'r', encoding='utf-8') as f:
        content = f.read()
    content = _strip_block_comments(content)
    names, _, _ = _parse_names(content)
    if not names:
        print("No layouts found.")
        return names
    print()
    print(f"Layouts in file ({len(names)}):")
    print()
    for i, name in enumerate(names):
        print(f"[{i}] {name}")
    return names


def delete_layout(target, index):
    index = int(index)
    if not os.path.exists(target):
        print(f"ERROR: {target} not found.")
        sys.exit(1)

    # Read original for editing
    with open(target, 'r', encoding='utf-8') as f:
        orig = f.read()

    # Work on a copy with /* */ comments stripped for parsing
    content = _strip_block_comments(orig)

    names, nm_start, nm_brace_end = _parse_names(content)
    entries, _ = _parse_entries(content)

    if index < 0 or index >= len(names):
        print(f"ERROR: Index {index} out of range (0-{len(names)-1}).")
        sys.exit(1)

    if len(names) <= 1:
        print("ERROR: Cannot delete the last remaining layout.")
        sys.exit(1)

    if len(names) != len(entries):
        print(f"ERROR: Name/entry count mismatch ({len(names)} names, {len(entries)} entries).")
        sys.exit(1)

    target_name = names[index]

    # Find the entry in the ORIGINAL content using the entry start position.
    # Since we stripped /* */ comments, positions in 'content' are shifted
    # relative to 'orig'.  We locate the entry in 'orig' by finding the
    # same // Name comment line.
    entry_start_clean, entry_end_clean = entries[index]

    # Get the // Name text from the clean entry
    clean_entry = content[entry_start_clean:entry_end_clean]
    name_match = re.search(r'//\s*(.+)', clean_entry)
    if not name_match:
        print(f"ERROR: Could not extract name from entry at index {index}.")
        sys.exit(1)
    entry_name_comment = name_match.group(1).strip()

    # Find this entry in the original content
    # Look for a line matching: whitespace, {, whitespace, //, whitespace, name
    pattern = r'(?m)^(\s+\{\s+//\s*' + re.escape(entry_name_comment) + r')'
    om = re.search(pattern, orig)
    if not om:
        print(f"ERROR: Could not locate entry '{entry_name_comment}' in original file.")
        sys.exit(1)

    # From this point in the original, count braces (skipping /* */ comments)
    orig_start = om.start()
    brace_pos = orig_start + om.group(1).index('{')
    depth, i = 1, brace_pos + 1
    in_block = False
    while i < len(orig) and depth > 0:
        ch = orig[i]
        # Skip /* */ block comments
        if ch == '/' and i + 1 < len(orig) and orig[i+1] == '*':
            in_block = True
            i += 2
            continue
        if in_block and ch == '*' and i + 1 < len(orig) and orig[i+1] == '/':
            in_block = False
            i += 2
            continue
        if in_block:
            i += 1
            continue
        if ch == '{': depth += 1
        elif ch == '}': depth -= 1
        i += 1

    if depth != 0:
        print(f"ERROR: Unmatched braces in original entry '{entry_name_comment}'.")
        sys.exit(1)

    comma = orig.find(',', i)
    if comma == -1 or comma > i + 5:
        print(f"ERROR: Could not find trailing comma for entry '{entry_name_comment}'.")
        sys.exit(1)

    orig_end = comma + 1
    # Consume the entry's own line terminator as well, so its neighbours end up on
    # adjacent lines instead of leaving a blank one behind.
    if orig_end < len(orig) and orig[orig_end] == '\n':
        orig_end += 1

    print(f"About to delete: [{index}] {target_name}  (from {os.path.basename(target)})")
    try:
        reply = input("This cannot be undone. Continue? [y/N] ").strip().lower()
    except (EOFError, KeyboardInterrupt):
        print()
        reply = ''
    if reply not in ('y', 'yes'):
        print("Cancelled. Nothing was changed.")
        return False

    # Remove name from names list
    del names[index]

    # Remove entry from original content
    new_orig = orig[:orig_start] + orig[orig_end:]

    # Fix excessive blank lines
    new_orig = re.sub(r'\n{4,}', '\n\n\n', new_orig)

    # Rebuild _layoutNames in the original, keeping the file's own trailing-comma
    # style so a delete rewrites only the removed name's line.
    nm = re.search(r'const char _layoutNames\[\]\[\d+\] PROGMEM = \{', new_orig)
    if nm:
        be = new_orig.find('\n};', nm.end())
        if be != -1:
            keeps_trailing_comma = new_orig[nm.end():be].rstrip().endswith(',')
            lines = [f'    "{n}",' for n in names]
            if not keeps_trailing_comma:
                lines[-1] = lines[-1][:-1]
            new_names_block = f'const char _layoutNames[][64] PROGMEM = {{\n' + '\n'.join(lines) + '\n};'
            new_orig = new_orig[:nm.start()] + new_names_block + new_orig[be + 1:]

    # Clean up };}; duplication
    new_orig = re.sub(r'\};(\};)+', '};', new_orig)

    with open(target, 'w', encoding='utf-8') as f:
        f.write(new_orig)

    print(f"Deleted [{index}] {target_name}")
    list_layouts(target)
    return True


def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    argv = sys.argv[1:]

    # No arguments: pick a conf file, pick a layout in it, then confirm the delete.
    if not argv:
        interactive_delete(script_dir)
        return

    if argv[0] in ('-h', '--help'):
        print(__doc__)
        return

    target = argv[0]
    if not os.path.exists(target):
        alt = os.path.join(script_dir, target)
        if os.path.exists(alt):
            target = alt
        else:
            print(f"ERROR: File not found: {target}")
            print(f"       (also tried: {alt})")
            sys.exit(1)

    if len(argv) == 1:
        list_layouts(target)
    elif len(argv) == 2 and argv[1].lstrip('-').isdigit():
        delete_layout(target, argv[1])  # asks for confirmation, always
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()
