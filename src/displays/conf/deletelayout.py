#!/usr/bin/env python3
"""
deletelayout.py — List or delete layouts from a conf file.

USAGE:
    py deletelayout.py <conf_file>                  # List layouts with indices
    py deletelayout.py <conf_file> <index>          # Delete layout at index (0-based)

EXAMPLES:
    py deletelayout.py displayTFT480x320conf.h
    py deletelayout.py displayTFT480x320conf.h 1
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


def list_layouts(target):
    if not os.path.exists(target):
        print(f"ERROR: {target} not found.")
        sys.exit(1)
    with open(target, 'r', encoding='utf-8') as f:
        content = f.read()
    content = _strip_block_comments(content)
    names, _, _ = _parse_names(content)
    if not names:
        print("No layouts found.")
        return
    print(f"Layouts in file ({len(names)}):")
    for i, name in enumerate(names):
        print(f"[{i}] {name}")


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

    print(f"Deleting: [{index}] {target_name}")

    # Remove name from names list
    del names[index]

    # Remove entry from original content
    new_orig = orig[:orig_start] + orig[orig_end:]

    # Fix excessive blank lines
    new_orig = re.sub(r'\n{4,}', '\n\n\n', new_orig)

    # Rebuild _layoutNames in the original
    indented = '\n'.join(f'    "{n}",' for n in names)
    new_names_block = f'const char _layoutNames[][64] PROGMEM = {{\n{indented}\n}};'

    # Find _layoutNames position in new_orig (after entry removal)
    nm = re.search(r'const char _layoutNames\[\]\[\d+\] PROGMEM = \{', new_orig)
    if nm:
        be = new_orig.find('\n};', nm.end())
        if be != -1:
            new_orig = new_orig[:nm.start()] + new_names_block + new_orig[be + 1:]

    # Clean up };}; duplication
    new_orig = re.sub(r'\};(\};)+', '};', new_orig)

    with open(target, 'w', encoding='utf-8') as f:
        f.write(new_orig)

    print(f"Deleted [{index}] {target_name}")
    print()
    list_layouts(target)


def main():
    argv = sys.argv[1:]
    if not argv:
        print(__doc__)
        sys.exit(1)

    target = argv[0]
    if not os.path.exists(target):
        script_dir = os.path.dirname(os.path.abspath(__file__))
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
        delete_layout(target, argv[1])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == '__main__':
    main()
