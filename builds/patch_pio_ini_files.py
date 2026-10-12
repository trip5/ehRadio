#!/usr/bin/env python3
"""
patch_pio_ini_files.py - Patch platformio.ini files in builds/ subfolders.

Usage:
  python patch_pio_ini_files.py           # Mode 1: Audit library differences
                                          #         (versions, missing/extra, unparseable)
  python patch_pio_ini_files.py <folder>  # Mode 2: Sync [platformio], [ehradio], [library]
                                          #         sections from a master folder

Files must contain this comment to be eligible:
  ; patch_pio_ini_files can automatically patch this file
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

SCRIPT_DIR = Path(__file__).parent  # builds/
PATCH_SENTINEL = "; patch_pio_ini_files can automatically patch this file"
SYNC_SECTIONS = ("platformio", "ehradio", "library")


# ─────────────────────────────────────────────────────────────────────────────
# Data Structures
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class LibEntry:
    lib_name:       str            # e.g. "bblanchon/ArduinoJson"
    version_str:    str            # e.g. "7.4.3"
    version_prefix: str            # e.g. "^", "~", ">=" (chars between @ and first digit)
    trailing:       str            # e.g. " ; comment"  (preserved verbatim on write)
    source_folder:  str            # e.g. "trip5"
    section:        str            # "lib_deps" or "library"
    alias_key:      Optional[str]  # e.g. "sh110x" (library-section entries only)
    file_path:      Path
    line_idx:       int            # 0-based index into the file's lines list
    original_line:  str            # full raw line including newline chars


@dataclass
class RawLibEntry:
    """A library line captured verbatim, even when its version cannot be read."""
    identity:       str            # [library] alias key, or lib_deps name before '@'
    raw_spec:       str            # value with any trailing '; comment' stripped
    has_version:    bool           # whether parse_lib_version could read a version
    section:        str            # "lib_deps" or "library"
    source_folder:  str            # e.g. "trip5"
    file_path:      Path
    line_idx:       int            # 0-based index into the file's lines list


# ─────────────────────────────────────────────────────────────────────────────
# Shared Utilities
# ─────────────────────────────────────────────────────────────────────────────

def find_patchable_files():
    """Return sorted list of builds/*/platformio.ini Paths that contain the sentinel."""
    result = []
    for p in sorted(SCRIPT_DIR.glob("*/platformio.ini")):
        if PATCH_SENTINEL in p.read_text(encoding="utf-8"):
            result.append(p)
    return result


def version_tuple(s):
    """Convert "7.4.3" → (7, 4, 3) for numeric comparison. Falls back to (s,)."""
    try:
        return tuple(int(x) for x in s.split("."))
    except ValueError:
        return (s,)


def parse_lib_version(value):
    """
    Parse a library string like "bblanchon/ArduinoJson@^7.4.3 ; comment" into
    (lib_name, version_prefix, version_str, trailing).

    '@' is the delimiter for an explicit version. When there is no '@', a '#' git-ref
    delimiter is accepted instead, so a ".git#v1.4" spec is version-checked too (the
    lib name keeps the URL, the prefix becomes "#v"). version_prefix is the chars
    between the delimiter and the first digit (e.g. "^", "~", ">=", "#v", "").

    Returns None if neither delimiter or no version digits are found.
    """
    at = value.find("@")
    delim = at if at != -1 else value.find("#")
    if delim == -1:
        return None
    lib_name = value[:delim].strip()
    rest = value[delim + 1:]  # e.g. "^7.4.3 ; comment" or "v1.4"
    m = re.match(r'^([^0-9]*)([0-9][0-9.]*)(.*)', rest)
    if not m:
        return None
    return lib_name, m.group(1), m.group(2), m.group(3)


def update_version_in_line(line, new_version):
    """
    Replace the version digits after the '@' (or '#') delimiter in a raw line,
    preserving the prefix so "#v1.4" becomes "#v1.5".
    """
    return re.sub(r'([@#][^0-9]*)([0-9][0-9.]*)', lambda m: m.group(1) + new_version, line, count=1)


def prompt_yn(message):
    """Prompt for Y/N. Loops on blank or unrecognised input. Returns True for Y."""
    while True:
        try:
            answer = input(message).strip().lower()
        except EOFError:
            print()
            return False  # treat end-of-input as No
        if answer == "y":
            return True
        if answer == "n":
            return False
        # blank or anything else → re-prompt


# ─────────────────────────────────────────────────────────────────────────────
# Mode 1 — Version Audit (no args)
# ─────────────────────────────────────────────────────────────────────────────

def _add_lib_entry(entries, value, section, folder, path, idx, raw_line, alias_key=None):
    """Parse a library value string and append a LibEntry if it has a valid version."""
    parsed = parse_lib_version(value)
    if parsed:
        lib_name, pfx, ver, trail = parsed
        entries.append(LibEntry(
            lib_name=lib_name, version_str=ver, version_prefix=pfx, trailing=trail,
            source_folder=folder, section=section, alias_key=alias_key,
            file_path=path, line_idx=idx, original_line=raw_line,
        ))


def parse_lib_entries(ini_path, folder, lines):
    """
    Extract LibEntry objects from [ehradio] lib_deps and [library] sections.
    Skips comment-only lines, blank lines, and ${...} interpolation lines.
    """
    entries = []
    in_section = None   # "ehradio" | "library" | None
    in_lib_deps = False

    for idx, raw_line in enumerate(lines):
        line = raw_line.rstrip("\r\n")
        stripped = line.strip()

        # ── Section header ──────────────────────────────────────────────────
        if re.match(r'^\[', stripped):
            m = re.match(r'^\[([^\]]+)\]', stripped)
            sec = m.group(1) if m else None
            in_section = sec if sec in ("ehradio", "library") else None
            in_lib_deps = False
            continue

        if in_section is None:
            continue

        # Skip blank and pure-comment lines
        if stripped == "" or stripped.startswith(";"):
            continue

        # Skip interpolation-only lines
        if "${" in stripped:
            continue

        # ── [ehradio] — collect lib_deps continuation lines ─────────────────
        if in_section == "ehradio":
            if re.match(r'^lib_deps\s*=', stripped):
                in_lib_deps = True
                # Handle value placed on the same line as the key (unusual but valid)
                val = re.sub(r'^lib_deps\s*=\s*', '', stripped).strip()
                if val and not val.startswith(";") and not val.startswith("${"):
                    _add_lib_entry(entries, val, "lib_deps", folder, ini_path, idx, raw_line)
                continue

            if not in_lib_deps:
                continue  # some other key in [ehradio]; not relevant

            # We're inside a lib_deps value block.
            # A non-indented line ends the block (new key or section header).
            if not line.startswith((" ", "\t")):
                in_lib_deps = False
                continue

            _add_lib_entry(entries, stripped, "lib_deps", folder, ini_path, idx, raw_line)

        # ── [library] — each non-comment line is "alias_key = lib@version" ──
        elif in_section == "library":
            if "=" not in stripped:
                continue
            eq = stripped.index("=")
            alias_key = stripped[:eq].strip()
            value = stripped[eq + 1:].strip()
            if value.startswith("${"):
                continue  # alias/interpolation lines (e.g. st7789 = ${library.st7735})
            _add_lib_entry(entries, value, "library", folder, ini_path, idx, raw_line, alias_key)

    return entries


# ─────────────────────────────────────────────────────────────────────────────
# Mode 1 — Full Library Inventory (report-only categories)
# ─────────────────────────────────────────────────────────────────────────────

def strip_lib_comment(value):
    """Drop a trailing '; comment' from a library value and trim whitespace."""
    semi = value.find(";")
    return (value[:semi] if semi != -1 else value).strip()


def inventory_identity(value, section, alias_key):
    """
    Identity used to match a library across files:
      [library]  → its alias key (the left-hand side)
      lib_deps   → the name before '@' (or a '#' git ref), else the whole spec
    """
    if section == "library":
        return alias_key
    spec = strip_lib_comment(value)
    for delim in ("@", "#"):
        pos = spec.find(delim)
        if pos != -1:
            return spec[:pos].strip()
    return spec


def spec_skeleton(value):
    """
    The part of a spec that is not its version: the library name when the spec parses,
    otherwise the spec verbatim. Two specs with equal skeletons differ only by a version,
    which the version-drift pass already reports.
    """
    parsed = parse_lib_version(value)
    return parsed[0] if parsed else value


def _add_raw_entry(entries, value, section, folder, path, idx, alias_key):
    """Append a RawLibEntry for a library value, keeping versionless entries too."""
    spec = strip_lib_comment(value)
    if not spec:
        return
    entries.append(RawLibEntry(
        identity=inventory_identity(value, section, alias_key),
        raw_spec=spec,
        has_version=parse_lib_version(spec) is not None,
        section=section,
        source_folder=folder,
        file_path=path,
        line_idx=idx,
    ))


def parse_library_inventory(ini_path, folder, lines):
    """
    Collect every library line from [ehradio] lib_deps and [library] - parsed or not.

    Unlike parse_lib_entries nothing is dropped for lacking a version: this pass exists so
    the audit can expose libraries the version registry cannot see. Interpolation lines
    ('${...}') are still skipped - they are deliberate aliases, not libraries.
    """
    entries = []
    in_section = None   # "ehradio" | "library" | None
    in_lib_deps = False

    for idx, raw_line in enumerate(lines):
        line = raw_line.rstrip("\r\n")
        stripped = line.strip()

        if re.match(r'^\[', stripped):
            m = re.match(r'^\[([^\]]+)\]', stripped)
            sec = m.group(1) if m else None
            in_section = sec if sec in ("ehradio", "library") else None
            in_lib_deps = False
            continue

        if in_section is None:
            continue
        if stripped == "" or stripped.startswith(";"):
            continue
        if "${" in stripped:
            continue

        if in_section == "ehradio":
            if re.match(r'^lib_deps\s*=', stripped):
                in_lib_deps = True
                val = re.sub(r'^lib_deps\s*=\s*', '', stripped).strip()
                if val and not val.startswith(";") and not val.startswith("${"):
                    _add_raw_entry(entries, val, "lib_deps", folder, ini_path, idx, None)
                continue

            if not in_lib_deps:
                continue

            if not line.startswith((" ", "\t")):
                in_lib_deps = False
                continue

            _add_raw_entry(entries, stripped, "lib_deps", folder, ini_path, idx, None)

        elif in_section == "library":
            if "=" not in stripped:
                continue
            eq = stripped.index("=")
            alias_key = stripped[:eq].strip()
            value = stripped[eq + 1:].strip()
            if value.startswith("${"):
                continue
            _add_raw_entry(entries, value, "library", folder, ini_path, idx, alias_key)

    return entries


def report_coverage(raw_entries, folders):
    """
    Print the report-only coverage categories; return True if anything was printed:
      1. Missing / extra libraries - present in some files but not all
      2. Total (non-version) mismatches - same library, a different spec
      3. Unparseable entries - no readable version, so invisible to the version registry
    """
    all_folders = list(folders)
    folder_set = set(all_folders)

    by_id = {}  # (section, identity) -> {folder: {raw_spec, ...}}
    for e in raw_entries:
        per_folder = by_id.setdefault((e.section, e.identity), {})
        per_folder.setdefault(e.source_folder, set()).add(e.raw_spec)

    found = False

    # 1. Missing / extra libraries
    presence = []
    for (section, identity), per_folder in by_id.items():
        have = set(per_folder)
        if have != folder_set:
            missing = [f for f in all_folders if f not in have]
            presence.append((section, identity, sorted(have), missing))
    if presence:
        found = True
        print(f"Missing / extra libraries ({len(presence)}):")
        for section, identity, have, missing in sorted(presence):
            print(f"  [{section}] {identity}")
            print(f"    present in: {', '.join(have)}")
            print(f"    missing in: {', '.join(missing)}")
        print()

    # 2. Total (non-version) mismatches - specs that differ beyond the version digits
    value_mismatches = []
    for (section, identity), per_folder in by_id.items():
        if set(per_folder) != folder_set:
            continue  # presence already reported above
        skeletons = {spec_skeleton(s) for specs in per_folder.values() for s in specs}
        if len(skeletons) > 1:
            value_mismatches.append((section, identity, per_folder))
    if value_mismatches:
        found = True
        print(f"Total (non-version) mismatches ({len(value_mismatches)}):")
        for section, identity, per_folder in sorted(value_mismatches):
            print(f"  [{section}] {identity}")
            for folder in all_folders:
                specs = sorted(per_folder.get(folder, []))
                print(f"    {folder}: {' | '.join(specs) if specs else '(absent)'}")
        print()

    # 3. Unparseable entries - no '@' or '#' version for the registry to read
    unparseable = {}
    for e in raw_entries:
        if not e.has_version:
            unparseable.setdefault((e.section, e.identity), set()).add(e.source_folder)
    if unparseable:
        found = True
        print(f"Unparseable entries, no version ({len(unparseable)}):")
        for (section, identity), fs in sorted(unparseable.items()):
            print(f"  [{section}] {identity} - in {', '.join(sorted(fs))}")
        print()

    return found


def build_registry(all_entries):
    """
    Build a registry keyed by (section, lib_name) that tracks the highest and
    lowest version seen across all files, along with their version_prefix and folder.
    """
    reg = {}
    for e in all_entries:
        key = (e.section, e.lib_name)
        vt = version_tuple(e.version_str)
        if key not in reg:
            reg[key] = {
                "section":     e.section,
                "lib_name":    e.lib_name,
                "highest":     (e.version_str, e.version_prefix, e.source_folder),
                "lowest":      (e.version_str, e.version_prefix, e.source_folder),
                "occurrences": [e],
            }
        else:
            r = reg[key]
            r["occurrences"].append(e)
            if vt > version_tuple(r["highest"][0]):
                r["highest"] = (e.version_str, e.version_prefix, e.source_folder)
            if vt < version_tuple(r["lowest"][0]):
                r["lowest"] = (e.version_str, e.version_prefix, e.source_folder)
    return reg


def run_version_audit():
    patchable = find_patchable_files()
    if not patchable:
        print("No patchable platformio.ini files found.")
        return

    print(f"Scanning {len(patchable)} patchable file(s):")
    for p in patchable:
        print(f"  {p.parent.name}/platformio.ini")
    print()

    # Load files and collect both the parsed entries and the raw inventory
    file_lines = {}
    all_entries = []
    all_raw = []
    for p in patchable:
        lines = p.read_text(encoding="utf-8").splitlines(keepends=True)
        file_lines[p] = lines
        all_entries.extend(parse_lib_entries(p, p.parent.name, lines))
        all_raw.extend(parse_library_inventory(p, p.parent.name, lines))

    folders = [p.parent.name for p in patchable]
    modified = set()

    # ── Version drift (interactive) ─────────────────────────────────────────
    reg = build_registry(all_entries)
    version_found = False

    for section_label in ("lib_deps", "library"):
        mismatches = {
            k: v for k, v in sorted(reg.items())
            if k[0] == section_label and v["highest"][0] != v["lowest"][0]
        }
        for key, r in mismatches.items():
            version_found = True
            hv, hp, hf = r["highest"]
            lv, lp, lf = r["lowest"]
            print(
                f"[{section_label}] Newer for {r['lib_name']} found in {hf}: "
                f"{hp}{hv}  Oldest: {lp}{lv} in {lf}"
            )
            if prompt_yn("Replace in all? [Y/N]: "):
                for e in r["occurrences"]:
                    old = file_lines[e.file_path][e.line_idx]
                    new = update_version_in_line(old, hv)
                    if new != old:
                        file_lines[e.file_path][e.line_idx] = new
                        modified.add(e.file_path)
                print(f"  → Updated to {hv} in all files.")
            print()

    if not version_found:
        print("No version mismatches found.")
        print()

    # ── Coverage audit (report-only) ────────────────────────────────────────
    coverage_found = report_coverage(all_raw, folders)

    if not version_found and not coverage_found:
        print("No mismatches found.")
        return

    for p in modified:
        p.write_text("".join(file_lines[p]), encoding="utf-8")
        print(f"Saved: {p.parent.name}/platformio.ini")

    if modified:
        print(f"\nDone. Updated {len(modified)} file(s).")
    else:
        print("No files were changed.")


# ─────────────────────────────────────────────────────────────────────────────
# Mode 2 — Section Sync (folder arg)
# ─────────────────────────────────────────────────────────────────────────────

def parse_segments(lines):
    """
    Parse a list of raw lines into alternating gap / section segments.

    Each section segment contains the header line + all body lines up to and
    including the last non-blank, non-comment line.  Trailing blank lines and
    ';' comment lines are trimmed from the section and become part of the next
    gap segment.  This preserves inter-section decorative comment blocks while
    keeping blank lines that appear *within* a section's body intact.

    Returns a list of dicts:
      {"type": "gap",     "lines": [...]}
      {"type": "section", "name": "ehradio", "lines": [...]}
    """
    segments = []
    gap = []
    i = 0

    while i < len(lines):
        s = lines[i].strip()

        if s.startswith("[") and "]" in s:
            # ── Flush accumulated gap ────────────────────────────────────────
            if gap:
                segments.append({"type": "gap", "lines": gap})
                gap = []

            # ── Extract section name ─────────────────────────────────────────
            m = re.match(r'^\[([^\]]+)\]', s)
            sec_name = m.group(1) if m else s.strip("[]")

            # ── Collect header + body until next section header or EOF ───────
            block = [lines[i]]
            i += 1
            while i < len(lines):
                ns = lines[i].strip()
                if ns.startswith("[") and "]" in ns:
                    break
                block.append(lines[i])
                i += 1

            # ── Trim trailing blank / ';' comment lines from block ───────────
            # Scan backwards; index 0 is always kept (the section header).
            last = 0
            for j in range(len(block) - 1, 0, -1):
                bs = block[j].strip()
                if bs and not bs.startswith(";"):
                    last = j
                    break

            segments.append({"type": "section", "name": sec_name, "lines": block[:last + 1]})
            gap = block[last + 1:]  # trailing blank/comment lines become next gap

        else:
            gap.append(lines[i])
            i += 1

    if gap:
        segments.append({"type": "gap", "lines": gap})

    return segments


def segments_to_text(segments):
    """Flatten segments back to a single string."""
    return "".join(line for seg in segments for line in seg["lines"])


def run_section_sync(master_folder):
    master_ini = SCRIPT_DIR / master_folder / "platformio.ini"
    if not master_ini.exists():
        print(f"Error: {master_ini} does not exist.")
        sys.exit(1)

    master_text = master_ini.read_text(encoding="utf-8")
    if PATCH_SENTINEL not in master_text:
        print(f"Error: {master_ini} does not contain the patch sentinel comment.")
        sys.exit(1)

    master_segs = parse_segments(master_text.splitlines(keepends=True))

    # Index master sections we care about
    master_content = {}
    for seg in master_segs:
        if seg["type"] == "section" and seg["name"] in SYNC_SECTIONS:
            master_content[seg["name"]] = seg["lines"]

    missing = [s for s in SYNC_SECTIONS if s not in master_content]
    if missing:
        print(f"Warning: Master file is missing section(s): {missing}")

    if not master_content:
        print("Error: No syncable sections found in master file.")
        sys.exit(1)

    # Find destination files (all patchable files except the master itself)
    patchable = [p for p in find_patchable_files() if p.parent.name != master_folder]
    if not patchable:
        print("No other patchable platformio.ini files to patch.")
        return

    print(f"Master file: {master_folder}/platformio.ini")
    print()

    patched = 0
    for ini_path in patchable:
        folder = ini_path.parent.name
        lines = ini_path.read_text(encoding="utf-8").splitlines(keepends=True)
        segs = parse_segments(lines)
        changed = False

        # Report any missing sections in the destination
        dest_section_names = {seg["name"] for seg in segs if seg["type"] == "section"}
        for sname in master_content:
            if sname not in dest_section_names:
                print(f"  Warning: [{sname}] not found in {folder}/platformio.ini — skipped.")

        for seg in segs:
            if seg["type"] != "section" or seg["name"] not in master_content:
                continue
            new_lines = master_content[seg["name"]]
            if seg["lines"] != new_lines:
                seg["lines"] = new_lines
                changed = True

        if changed:
            ini_path.write_text(segments_to_text(segs), encoding="utf-8")
            print(f"  Patched: {folder}/platformio.ini")
            patched += 1
        else:
            print(f"  No changes needed: {folder}/platformio.ini")

    print(f"\nDone. Patched {patched} file(s).")


# ─────────────────────────────────────────────────────────────────────────────
# Entry Point
# ─────────────────────────────────────────────────────────────────────────────

def main():
    args = sys.argv[1:]
    if not args:
        run_version_audit()
    elif len(args) == 1:
        run_section_sync(args[0])
    else:
        print(__doc__)
        sys.exit(1)


if __name__ == "__main__":
    main()
