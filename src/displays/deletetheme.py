#!/usr/bin/env python3
"""
deletetheme.py — List or delete themes from themes.h.

USAGE:
    py deletetheme.py              # List themes with indices
    py deletetheme.py <index>      # Delete theme at index (0-based)
"""

import re, sys, os

# Force UTF-8 output on Windows
if sys.platform == 'win32':
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')

TARGET = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'themes.h')


def _parse_names(content):
    """Return list of theme names from _themeNames block."""
    m = re.search(r'const char _themeNames\[\]\[\d+\] PROGMEM = \{', content)
    if not m:
        print("ERROR: Could not find _themeNames.")
        sys.exit(1)
    brace_end = content.find('\n};', m.end())
    if brace_end == -1:
        print("ERROR: Could not find closing } of _themeNames.")
        sys.exit(1)
    block = content[m.end():brace_end]
    names = [n.strip().strip('"').strip(',').strip('"') for n in block.strip().split('\n') if n.strip()]
    return names, m.start(), brace_end


def _parse_entries(content):
    """Return list of (start_pos, end_pos) for each theme entry in _themes[]."""
    m = re.search(r'const ThemeData _themes\[\]\s*PROGMEM\s*=\s*\{', content)
    if not m:
        print("ERROR: Could not find _themes[].")
        sys.exit(1)
    entries = []
    # Only scan entries after the _themes[] opening brace
    search_start = m.end()
    while True:
        em = re.search(r'(?m)^\s+\{\s+//', content[search_start:])
        if not em:
            break
        em_start = search_start + em.start()
        # Find the { in the match and start counting after it
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
        # Find the comma after the closing }
        comma = content.find(',', i)
        if comma == -1 or comma > i + 5:
            search_start = em_start + 1
            continue
        entries.append((em_start, comma + 1))
        search_start = comma + 1
    return entries, m.start()


def list_themes():
    if not os.path.exists(TARGET):
        print(f"ERROR: {TARGET} not found.")
        sys.exit(1)
    with open(TARGET, 'r', encoding='utf-8') as f:
        content = f.read()
    names, _, _ = _parse_names(content)
    if not names:
        print("No themes found.")
        return
    print(f"Themes in file ({len(names)}):")
    for i, name in enumerate(names):
        print(f"[{i}] {name}")


def delete_theme(index):
    index = int(index)
    if not os.path.exists(TARGET):
        print(f"ERROR: {TARGET} not found.")
        sys.exit(1)
    with open(TARGET, 'r', encoding='utf-8') as f:
        content = f.read()

    names, nm_start, nm_brace_end = _parse_names(content)
    entries, _ = _parse_entries(content)

    if index < 0 or index >= len(names):
        print(f"ERROR: Index {index} out of range (0-{len(names)-1}).")
        sys.exit(1)

    if len(names) <= 1:
        print("ERROR: Cannot delete the last remaining theme.")
        sys.exit(1)

    if len(names) != len(entries):
        print(f"ERROR: Name/entry count mismatch ({len(names)} names, {len(entries)} entries).")
        sys.exit(1)

    target_name = names[index]
    print(f"Deleting: [{index}] {target_name}")

    # Remove name from names list
    del names[index]

    # Remove entry block from content
    entry_start, entry_end = entries[index]
    content = content[:entry_start] + content[entry_end:]

    # Fix up: ensure no double-newlines after removal (max 1 blank line between entries)
    content = re.sub(r'\n{4,}', '\n\n\n', content)

    # Rebuild _themeNames block
    indented = '\n'.join(f'    "{n}",' for n in names)
    new_names_block = f'const char _themeNames[][64] PROGMEM = {{\n{indented}\n}};'
    content = content[:nm_start] + new_names_block + content[nm_brace_end + 1:]

    content = re.sub(r'\};(\};)+', '};', content)
    with open(TARGET, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"Deleted [{index}] {target_name}")
    print()
    list_themes()

def main():
    argv = sys.argv[1:]
    if not argv:
        list_themes()
    elif len(argv) == 1 and argv[0].isdigit():
        delete_theme(argv[0])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()
