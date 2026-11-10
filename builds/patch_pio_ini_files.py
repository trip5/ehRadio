#!/usr/bin/env python3
"""
patch_pio_ini_files.py - keep builds/*/platformio.ini in step with the master build.

Usage:
  python patch_pio_ini_files.py           # master is builds/trip5/platformio.ini
  python patch_pio_ini_files.py <folder>  # use builds/<folder> as the master instead

One run does two things, in order:

  1. AUDIT against the master. Three things are reported, nothing is changed yet:
       - a library the contributor has at a NEWER version than the master
       - a library the master does not have at all ("unknown")
       - a file that cannot be parsed
     Older or missing libraries are deliberately not reported: the sync pass fixes
     those, so listing them here would only add noise.

  2. SYNC, once you answer the prompt.
       - [platformio], [ehradio] and [library] are replaced wholesale from the master
       - board_build.filesystem is copied per board section, key by key. This one matters
         because the firmware mounts LittleFS: a contributor ini that asks for spiffs
         produces an image that can never mount, and prepare_releases.py refuses to run.

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
DEFAULT_MASTER = "trip5"

# Board-section keys copied individually rather than by replacing the whole section: a
# board block also carries build_flags, and those legitimately differ between contributors.
SYNC_BOARD_KEYS = ("board_build.filesystem",)


# -----------------------------------------------------------------------------
# Data
# -----------------------------------------------------------------------------

@dataclass
class LibLine:
    section: str            # "lib_deps" or "library"
    identity: str           # [library] alias key, else the library name before @ or #
    spec: str               # value without any trailing "; comment"
    prefix: str             # "^", "~", ">=", "#v", "" - kept so the report matches the file
    version: str            # "1.5.0", or "" when the version cannot be read
    folder: str             # contributor folder this line came from
    line_no: int            # 1-based line number in that file

    @property
    def label(self):
        return "[library]" if self.section == "library" else "[lib_deps]"

    @property
    def version_text(self):
        return f"{self.prefix}{self.version}" if self.version else self.spec


# -----------------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------------

def find_patchable_files():
    """Return sorted builds/*/platformio.ini paths that contain the sentinel."""
    result = []
    for path in sorted(SCRIPT_DIR.glob("*/platformio.ini")):
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        if PATCH_SENTINEL in text:
            result.append(path)
    return result


def version_tuple(value):
    """Convert "7.4.3" to (7, 4, 3). Anything else becomes (value,) so compares stay total."""
    try:
        return tuple(int(part) for part in value.split("."))
    except (ValueError, AttributeError):
        return (value,)


def parse_lib_version(value):
    """
    Split "bblanchon/ArduinoJson@7.4.3 ; comment" into (name, prefix, version, trailing).

    '@' is the version delimiter; a '#' git ref is accepted too, so ".git#v1.4" is read
    with prefix "#v". Returns None when there is no delimiter or no version digits.
    """
    at = value.find("@")
    delim = at if at != -1 else value.find("#")
    if delim == -1:
        return None
    name = value[:delim].strip()
    rest = value[delim + 1:]
    match = re.match(r'^([^0-9]*)([0-9][0-9.]*)(.*)', rest)
    if not match:
        return None
    return name, match.group(1), match.group(2), match.group(3)


def strip_lib_comment(value):
    """Drop a trailing '; comment' from a library value."""
    semi = value.find(";")
    return (value[:semi] if semi != -1 else value).strip()


def prompt_yn(message):
    """Prompt for y/N. Blank or unrecognised input re-prompts; end-of-input counts as no."""
    while True:
        try:
            answer = input(message).strip().lower()
        except EOFError:
            print()
            return False
        if answer in ("y", "yes"):
            return True
        if answer in ("n", "no", ""):
            return False


def _section_name(line):
    match = re.match(r'^\[([^\]]+)\]', line)
    return match.group(1).strip() if match else None


# -----------------------------------------------------------------------------
# Parse one platformio.ini into library lines
# -----------------------------------------------------------------------------

def parse_library_lines(ini_path, folder):
    """
    Collect every library line from [ehradio] lib_deps and from [library].

    Comment-only lines, blank lines and ${...} interpolation lines are skipped; the last
    are deliberate aliases (st7789 = ${library.st7735}), not libraries of their own.

    Returns (lines, ok) where ok is False when the file has no [ehradio] section at all,
    which is what "cannot be parsed" means for this tool.
    """
    try:
        raw_lines = ini_path.read_text(encoding="utf-8").splitlines()
    except OSError:
        return [], False

    found_ehradio = False
    lines = []
    section = None          # "ehradio" | "library" | None
    in_lib_deps = False

    for index, raw_line in enumerate(raw_lines):
        stripped = raw_line.strip()

        header = _section_name(stripped)
        if header is not None:
            if header == "ehradio":
                found_ehradio = True
            section = header if header in ("ehradio", "library") else None
            in_lib_deps = False
            continue

        if section is None:
            continue
        if stripped == "" or stripped.startswith(";"):
            continue
        if "${" in stripped:
            continue

        value = None
        alias_key = None

        if section == "ehradio":
            if re.match(r'^lib_deps\s*=', stripped):
                in_lib_deps = True
                same_line = re.sub(r'^lib_deps\s*=\s*', '', stripped).strip()
                if same_line and not same_line.startswith(";"):
                    value = same_line
            elif in_lib_deps:
                # A non-indented line ends the lib_deps block.
                if not raw_line.startswith((" ", "\t")):
                    in_lib_deps = False
                    continue
                value = stripped

        elif "=" in stripped:
            equals = stripped.index("=")
            alias_key = stripped[:equals].strip()
            candidate = stripped[equals + 1:].strip()
            if candidate and not candidate.startswith("${"):
                value = candidate

        if not value:
            continue

        spec = strip_lib_comment(value)
        if not spec:
            continue

        parsed = parse_lib_version(spec)
        if parsed:
            name, prefix, version, _trailing = parsed
            identity = alias_key if section == "library" else name
        else:
            identity = alias_key if section == "library" else spec
            prefix, version = "", ""

        lines.append(LibLine(
            section="library" if section == "library" else "lib_deps",
            identity=identity,
            spec=spec,
            prefix=prefix,
            version=version,
            folder=folder,
            line_no=index + 1,
        ))

    return lines, found_ehradio


def inventory_for(ini_path, folder):
    """Convenience wrapper: the library lines of one file, keyed by (section, identity)."""
    lines, ok = parse_library_lines(ini_path, folder)
    return {(line.section, line.identity): line for line in lines}, ok


# -----------------------------------------------------------------------------
# Phase 1 - audit against the master
# -----------------------------------------------------------------------------

def audit(master_folder, patchable):
    """
    Report newer-than-master libraries, libraries the master does not have, and files that
    cannot be parsed. Returns True when something was reported.
    """
    master_ini = SCRIPT_DIR / master_folder / "platformio.ini"
    master_lines, master_ok = inventory_for(master_ini, master_folder)
    if not master_ok:
        print(f"{master_folder}/platformio.ini cannot be parsed, so there is nothing to audit against.")
        return True

    reported = False
    for ini_path in patchable:
        folder = ini_path.parent.name
        lines, ok = inventory_for(ini_path, folder)

        if not ok:
            print()
            print(f"{folder}/platformio.ini cannot be parsed.")
            reported = True
            continue

        for key, line in lines.items():
            master = master_lines.get(key)
            if master is None:
                print()
                print(f"{line.label} Unknown {line.identity} found in {folder}: {line.version_text}")
                reported = True
                continue
            if not line.version or not master.version:
                continue
            if version_tuple(line.version) > version_tuple(master.version):
                print()
                print(f"{line.label} Newer for {line.identity} found in {folder}: {line.version_text}"
                      f"   master {master_folder}: {master.version_text}")
                reported = True

    if not reported:
        print("Audit: no newer or unknown libraries found.")
    return reported


# -----------------------------------------------------------------------------
# Phase 2a - whole-section sync
# -----------------------------------------------------------------------------

def parse_segments(lines):
    """
    Split raw lines into alternating gap / section segments.

    A section segment is its header plus body, trimmed of trailing blank and ';' lines.
    Those trimmed lines become the start of the following gap, which preserves the
    decorative comment blocks between sections.
    """
    segments = []
    gap = []
    index = 0

    while index < len(lines):
        stripped = lines[index].strip()

        if stripped.startswith("[") and "]" in stripped:
            if gap:
                segments.append({"type": "gap", "lines": gap})
                gap = []

            name = _section_name(stripped) or stripped.strip("[]")

            block = [lines[index]]
            index += 1
            while index < len(lines):
                if lines[index].strip().startswith("[") and "]" in lines[index]:
                    break
                block.append(lines[index])
                index += 1

            last = 0
            for position in range(len(block) - 1, 0, -1):
                body = block[position].strip()
                if body and not body.startswith(";"):
                    last = position
                    break

            segments.append({"type": "section", "name": name, "lines": block[:last + 1]})
            gap = block[last + 1:]

        else:
            gap.append(lines[index])
            index += 1

    if gap:
        segments.append({"type": "gap", "lines": gap})

    return segments


def segments_to_text(segments):
    return "".join(line for segment in segments for line in segment["lines"])


def sync_sections(master_folder, patchable):
    """Replace SYNC_SECTIONS in every patchable file with the master's copy."""
    master_ini = SCRIPT_DIR / master_folder / "platformio.ini"
    master_text = master_ini.read_text(encoding="utf-8")
    if PATCH_SENTINEL not in master_text:
        print(f"Error: {master_ini} does not contain the patch sentinel comment.")
        sys.exit(1)

    master_content = {}
    for segment in parse_segments(master_text.splitlines(keepends=True)):
        if segment["type"] == "section" and segment["name"] in SYNC_SECTIONS:
            master_content[segment["name"]] = segment["lines"]

    for name in SYNC_SECTIONS:
        if name not in master_content:
            print(f"Warning: master file is missing [{name}]")

    patched = 0
    for ini_path in patchable:
        folder = ini_path.parent.name
        segments = parse_segments(ini_path.read_text(encoding="utf-8").splitlines(keepends=True))
        present = {segment["name"] for segment in segments if segment["type"] == "section"}

        for name in master_content:
            if name not in present:
                print(f"  Warning: [{name}] not found in {folder}/platformio.ini - skipped.")

        changed = False
        for segment in segments:
            if segment["type"] != "section" or segment["name"] not in master_content:
                continue
            wanted = master_content[segment["name"]]
            if segment["lines"] != wanted:
                segment["lines"] = wanted
                changed = True

        if changed:
            ini_path.write_text(segments_to_text(segments), encoding="utf-8")
            print(f"  Patched sections: {folder}/platformio.ini")
            patched += 1

    print(f"Section sync: {patched} file(s) changed.")
    return patched


# -----------------------------------------------------------------------------
# Phase 2b - board key sync
# -----------------------------------------------------------------------------

def board_key_values(ini_path):
    """Map {section: {key: value}} for the board keys this tool is allowed to copy."""
    values = {}
    section = None
    for raw_line in ini_path.read_text(encoding="utf-8").splitlines():
        stripped = raw_line.split(";", 1)[0].strip()
        if not stripped:
            continue
        header = _section_name(stripped)
        if header is not None:
            section = header
            continue
        if section is None or "=" not in stripped:
            continue
        key, value = stripped.split("=", 1)
        key = key.strip()
        if key in SYNC_BOARD_KEYS:
            values.setdefault(section, {})[key] = value.strip()
    return values


def sync_board_keys(master_folder, patchable):
    """
    Copy SYNC_BOARD_KEYS between matching board sections.

    Only sections that exist in both files are touched, and only the listed keys, so
    build_flags and anything else contributor-specific is left exactly as it is.
    """
    master_ini = SCRIPT_DIR / master_folder / "platformio.ini"
    master_values = board_key_values(master_ini)

    patched = 0
    for ini_path in patchable:
        folder = ini_path.parent.name
        text = ini_path.read_text(encoding="utf-8")
        lines = text.splitlines(keepends=True)
        section = None
        changed = False

        for index, raw_line in enumerate(lines):
            stripped = raw_line.split(";", 1)[0].strip()
            header = _section_name(stripped)
            if header is not None:
                section = header
                continue
            if section is None or "=" not in stripped:
                continue
            key = stripped.split("=", 1)[0].strip()
            if key not in SYNC_BOARD_KEYS:
                continue
            wanted = master_values.get(section, {}).get(key)
            if wanted is None:
                continue
            current = stripped.split("=", 1)[1].strip()
            if current == wanted:
                continue
            lines[index] = f"{key} = {wanted}\n"
            changed = True
            print(f"  Patched key: {folder}/platformio.ini [{section}] {key} = {wanted}")

        if changed:
            ini_path.write_text("".join(lines), encoding="utf-8")
            patched += 1

    print(f"Board key sync: {patched} file(s) changed.")
    return patched


# -----------------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------------

def main():
    args = sys.argv[1:]
    if len(args) > 1 or (args and args[0].startswith("-")):
        if args and args[0].startswith("-"):
            # Otherwise "--check" is taken as a master folder and reported as a missing one.
            print(f"Unknown option: {args[0]}")
            print("This tool takes no options. Run it with no arguments, or name a folder to use")
            print("as the master instead, for example: python builds/patch_pio_ini_files.py trip5")
            print()
        print(__doc__)
        return 1

    master_folder = args[0] if args else DEFAULT_MASTER
    if not (SCRIPT_DIR / master_folder / "platformio.ini").exists():
        print(f"Error: builds/{master_folder}/platformio.ini does not exist.")
        return 1

    patchable = [p for p in find_patchable_files() if p.parent.name != master_folder]
    if not patchable:
        print("No other patchable platformio.ini files found.")
        return 0

    print(f"Master: builds/{master_folder}/platformio.ini")
    print(f"Files:  {', '.join(path.parent.name for path in patchable)}")

    print()
    reported = audit(master_folder, patchable)

    if reported:
        print()
        if not prompt_yn("Do you wish to continue patching? [y/N] "):
            print("Stopped. Nothing was changed.")
            return 0

    print()
    sync_sections(master_folder, patchable)
    sync_board_keys(master_folder, patchable)
    print("\nDone.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
