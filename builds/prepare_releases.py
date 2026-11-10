#!/usr/bin/env python3
"""
prepare_releases.py

Prepares everything a release needs, and refuses to prepare a tree that does not build.
Four phases, stopping at the first failure. Nothing under builds/releases/ is written until
the build has passed.

  1. naming     FIRMWARE must be "<contributor folder>_<env>.bin" for an [env:...] declared
                in that folder's platformio.ini, the contributor field must prefix it, and
                board_build.filesystem must be littlefs
  2. configs    preprocessor balance in every contributor myoptions.h - a leftover #endif
                closes the include guard early and then fails every translation unit
  3. build      every contributor folder, every [env:...] it declares, in a scratch copy
  4. generate   builds/releases/firmware.txt
                builds/releases/releases.md (patched in-place)
                builds/releases/web_assets/firmware-info.json
                builds/releases/web_assets/manifests/*.json

Usage:
  python builds/prepare_releases.py             # all four phases; asks before compiling
  python builds/prepare_releases.py --yes       # same, without the prompt (CI)
  python builds/prepare_releases.py --check     # phases 1-2: no build, no writes
  python builds/prepare_releases.py --no-build  # phases 1, 2, 4 (release workflows only)

WHY A SCRATCH COPY
  Root platformio.ini / myoptions.h are the live files that use_build.py swaps and that the
  VS Code File Watcher syncs into builds/<folder>/. Writing them here would fight both the
  lock files and the watcher, so the tree is copied to .build_all/ and the copy is built
  instead. The copy is deleted on the way out, pass or fail.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent          # builds/
ROOT       = SCRIPT_DIR.parent              # workspace root
SCRATCH    = ROOT / '.build_all'
LOG_LINES_ON_FAIL = 12

# Not needed to compile, and large: the build copies only what the compiler reads.
COPY_IGNORE = {
    '.git', '.pio', '.build_all', '.vscode', '__pycache__',
    'images', 'docs', 'site', 'binaries', 'firmware', 'upload_pkg', 'www_release',
}

# A contributor swap left in place means builds/<folder> may hold swapped content.
SWAP_MARKERS = (
    SCRIPT_DIR / 'use_build.lck',
    ROOT / 'use_build.lck',
    ROOT / 'platformio.ini._bk',
    ROOT / 'myoptions.h._bk',
)

COND_OPEN = {'if', 'ifdef', 'ifndef'}
COND_MID  = {'else', 'elif'}


def _strip_comments(content):
    """Remove block comments and lines starting with // so commented-out #defines are ignored."""
    # Remove /* ... */ block comments (including multi-line)
    content = re.sub(r'/\*.*?\*/', '', content, flags=re.DOTALL)
    # Remove lines whose first non-whitespace chars are //
    lines = [line for line in content.splitlines(keepends=True)
             if not line.lstrip().startswith('//')]
    return ''.join(lines)


def parse_myoptions_h(myoptions_path):
    """
    Parse myoptions.h to extract firmware metadata.
    Commented-out lines and block comments are ignored.

    Format expected:
      #define FIRMWARE "filename.bin" // "board_env", "chip_family", "Contributor"
      #define FIRMWARE_NAME "Friendly Name" // "optional_url"

        Returns:
            firmwares          - list of 5-tuples (board_env, chip_family, fw_env, friendly_name, contributor)
            url_map            - dict mapping fw_env -> url
            missing_name_envs  - list of fw_env values missing FIRMWARE_NAME in this file
    """
    content = _strip_comments(Path(myoptions_path).read_text(encoding='utf-8'))

    firmwares = []
    url_map   = {}

    fw_pattern        = re.compile(r'#define\s+FIRMWARE\s+"([^"]+)"\s*//\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"')
    name_pattern      = re.compile(r'#define\s+FIRMWARE_NAME\s+"([^"]+)"(?:\s*//\s*"([^"]*)")?')
    missing_name_envs = []

    fw_matches = list(fw_pattern.finditer(content))

    for idx, m in enumerate(fw_matches):
        filename    = m.group(1)
        board_env   = m.group(2)
        chip_family = m.group(3)
        contributor = m.group(4)

        # Skip board_ entries (bare boards without full config)
        if filename.startswith('board_'):
            continue

        fw_env = filename[:-4] if filename.endswith('.bin') else filename

        # Look for FIRMWARE_NAME within this FIRMWARE block.
        # Bound the search to before the next FIRMWARE define so very long
        # URLs are supported without leaking into the next entry.
        next_start = fw_matches[idx + 1].start() if idx + 1 < len(fw_matches) else len(content)
        after = content[m.end():next_start]
        nm       = name_pattern.search(after)
        if nm:
            friendly_name = nm.group(1)
            url = nm.group(2) or ''
            if url:
                url_map[fw_env] = url
        else:
            friendly_name = fw_env
            missing_name_envs.append(fw_env)

        firmwares.append((board_env, chip_family, fw_env, friendly_name, contributor))

    return firmwares, url_map, missing_name_envs


ENV_PATTERN = re.compile(r'^\s*\[env:([^\]]+)\]', re.MULTILINE)


def normalize_name(value):
    """Normalize a name so 'No Display', 'no_display' and 'no-display' compare equal."""
    return re.sub(r'[\s_\-]+', '_', (value or '').strip().lower()).strip('_')


def read_pio_envs(platformio_path):
    """Return the [env:NAME] names declared in a platformio.ini (empty when unreadable)."""
    platformio_path = Path(platformio_path)
    if not platformio_path.is_file():
        return []
    text = platformio_path.read_text(encoding='utf-8', errors='ignore')
    return ENV_PATTERN.findall(text)


def contributor_folders(script_dir):
    """Subfolders of builds/ that carry a myoptions.h and a usable name."""
    folders = []
    for subfolder in sorted(script_dir.iterdir()):
        if not subfolder.is_dir():
            continue
        if subfolder.name == 'releases':
            continue
        if not re.match(r'^[a-zA-Z0-9_-]+$', subfolder.name):
            continue
        if (subfolder / 'myoptions.h').is_file():
            folders.append(subfolder)
    return folders


def parse_ini_sections(ini_path):
    """Minimal INI reader: {section: {key: value}}, with ';' comments stripped."""
    sections = {}
    ini_path = Path(ini_path)
    if not ini_path.is_file():
        return sections
    current = None
    for raw in ini_path.read_text(encoding='utf-8', errors='ignore').splitlines():
        line = raw.split(';', 1)[0].strip()
        if not line:
            continue
        header = re.match(r'^\[([^\]]+)\]$', line)
        if header:
            current = header.group(1).strip()
            sections.setdefault(current, {})
            continue
        if current is None or '=' not in line:
            continue
        key, value = line.split('=', 1)
        sections[current][key.strip()] = value.strip()
    return sections


def board_partition_tables(ini_path):
    """
    Map each board environment to the partition CSV it declares.

    A contributor ini looks like [env:taipan_streamer_1] extends esp32_s3_n16r8, and
    board_build.partitions lives on the board block, so the extends chain is walked
    until a section actually carries the key.
    """
    sections = parse_ini_sections(ini_path)
    tables = {}
    for name in sections:
        if not name.startswith('env:'):
            continue
        board = sections[name].get('extends', '')
        seen = set()
        while board and board not in seen:
            seen.add(board)
            table = sections.get(board, {}).get('board_build.partitions')
            if table:
                tables[board] = table
                break
            board = sections.get(board, {}).get('extends', '')
    return tables


def read_filesystem_partition(csv_path):
    """Return {'offset': int, 'size': int} for the LittleFS row of a partition CSV."""
    csv_path = Path(csv_path)
    if not csv_path.is_file():
        return None
    for raw in csv_path.read_text(encoding='utf-8', errors='ignore').splitlines():
        line = raw.strip()
        if not line or line.startswith('#'):
            continue
        columns = [column.strip() for column in line.split(',')]
        if len(columns) < 5:
            continue
        name, ptype, subtype, offset, size = (
            columns[0].lower(), columns[1].lower(), columns[2].lower(), columns[3], columns[4])
        # The subtype has to stay "spiffs" for the toolchain, so the row is found by its
        # name first and only then by the data/spiffs combination.
        if name.startswith('littlefs') or (
                ptype == 'data' and subtype == 'spiffs'
                and name not in ('nvs', 'otadata', 'coredump')):
            try:
                return {'offset': int(offset, 0), 'size': int(size, 0)}
            except ValueError:
                return None
    return None


def collect_filesystem_images(script_dir, repo_root):
    """
    Work out, per board environment, which filesystem image the flasher should write and
    where. Content is identical for every variant on the same partition layout, so one
    image per board is enough and the name follows the bootloader/partitions convention.
    """
    images = {}
    for folder in contributor_folders(script_dir):
        for board_env, table in board_partition_tables(folder / 'platformio.ini').items():
            if board_env in images:
                continue
            partition = read_filesystem_partition(repo_root / table)
            if not partition:
                print(f'  [!] {board_env}: no LittleFS row found in {table}; no filesystem image will be listed')
                continue
            images[board_env] = {
                'path': f'../firmware/{board_env}_filesystem.bin',
                'offset': partition['offset'],
                'size': partition['size'],
                'source': table,
            }
    return images


def validate_build_naming(script_dir):
    """
    The release workflows derive every asset name as '<contributor folder>_<env>.bin',
    so a FIRMWARE define that ignores that pattern compiles fine but produces a binary
    that is never copied into the release. Checks, per contributor folder:

      1. FIRMWARE "x.bin" must equal "<folder>_<env>.bin" for an [env:...] declared in
         that folder's platformio.ini.
      2. The contributor field of the FIRMWARE comment must prefix the filename, so the
         credited name and the asset name cannot drift apart.
      3. platformio.ini must ask for littlefs. The firmware mounts LittleFS, so a
         SPIFFS-packed image can never mount, and buildfs would leave spiffs.bin where
         the release pipeline expects littlefs.bin.

    Comparison ignores case and treats space, hyphen and underscore as equivalent, so
    'No Display'/'no_display' and 'Trip5'/'trip5' do not raise false positives.

    Returns a list of human-readable error strings (empty when everything is fine).
    """
    errors = []
    for folder in contributor_folders(script_dir):
        # Filesystem type first: a SPIFFS ini breaks the filesystem image the flasher writes,
        # and the fix is a different tool, so it is reported before any naming problem.
        for section, values in parse_ini_sections(folder / 'platformio.ini').items():
            filesystem = values.get('board_build.filesystem')
            if filesystem is None or filesystem.strip().lower() == 'littlefs':
                continue
            errors.append(
                f'builds/{folder.name}/platformio.ini: board_build.filesystem = {filesystem} '
                f'in [{section}]\n'
                f'      the firmware mounts LittleFS, so a {filesystem.upper()}-packed image cannot mount,\n'
                f'      and buildfs would leave {filesystem.lower()}.bin where littlefs.bin is expected.\n'
                f'      Run:  python builds/patch_pio_ini_files.py   (no arguments), then re-run this check.\n'
                f'      That tool is what keeps the per-board sections in step with the master template.'
            )

        firmwares, _url_map, _missing = parse_myoptions_h(folder / 'myoptions.h')
        if not firmwares:
            continue

        pio_envs = read_pio_envs(folder / 'platformio.ini')
        expected = [f'{folder.name}_{env}.bin' for env in pio_envs]
        location = f'builds/{folder.name}/myoptions.h'

        for _board_env, _chip_family, fw_env, _friendly_name, contributor in firmwares:
            filename = f'{fw_env}.bin'
            if filename not in expected:
                errors.append(
                    f'{location}: FIRMWARE "{filename}" is not "<folder>_<env>.bin"\n'
                    f'      expected one of: '
                    + (', '.join(expected) if expected
                       else f'(no [env:...] found in builds/{folder.name}/platformio.ini)')
                )
            if not normalize_name(fw_env).startswith(normalize_name(contributor) + '_'):
                errors.append(
                    f'{location}: contributor "{contributor}" does not prefix FIRMWARE "{filename}"\n'
                    f'      expected FIRMWARE "{folder.name}_<env>.bin" // ..., "{contributor}"'
                )
    return errors


def get_version_from_options_h():
    options_h = Path(__file__).parent.parent / 'src' / 'core' / 'options.h'
    if options_h.exists():
        for line in options_h.read_text(encoding='utf-8').splitlines():
            if '#define RADIOVERSION' in line:
                m = re.search(r'"([^"]+)"', line)
                if m:
                    return m.group(1)
    return ''


def generate_firmware_txt(firmwares, output_path):
    """Format: board_env|chip_family|fw_env|friendly_name"""
    with open(output_path, 'w', encoding='utf-8') as f:
        for board_env, chip_family, fw_env, friendly_name, _ in firmwares:
            f.write(f'{board_env}|{chip_family}|{fw_env}|{friendly_name}\n')
    print(f'[*] Generated {output_path} with {len(firmwares)} firmware entries')


def generate_releases_md(firmwares, url_map, output_path):
    """
    Patch releases.md in-place:
      - Walk existing lines; when ### X Firmware is found:
          - If X has firmware data → keep header, replace entry lines
          - If X has no firmware data → drop header and its entries
      - Before ### Add Yours: insert new contributor sections (sorted alphabetically)
        for contributors not already present in the file
    """
    # Group by contributor (key = lowercase), preserve display name
    by_contributor  = {}  # contributor_lower -> list of (fw_env, friendly_name)
    display_name    = {}  # contributor_lower -> display name as found in data
    for _, _, fw_env, friendly_name, contributor in firmwares:
        key = contributor.lower()
        by_contributor.setdefault(key, []).append((fw_env, friendly_name))
        display_name[key] = contributor

    if not output_path.exists():
        print(f'[!] {output_path} not found, skipping releases.md update')
        return

    lines = output_path.read_text(encoding='utf-8').splitlines(keepends=True)
    new_lines        = []
    seen_contributors = set()

    i = 0
    while i < len(lines):
        line = lines[i]

        # Sentinel: insert any new contributors before ### Add Yours
        if re.match(r'^###\s+Add Yours', line):
            for key in sorted(by_contributor):
                if key not in seen_contributors:
                    disp = display_name[key]
                    new_lines.append(f'### {disp} Firmware\n')
                    for fw_env, _ in by_contributor[key]:
                        filename = f'{fw_env}.bin'
                        if fw_env in url_map:
                            new_lines.append(f'  - [`{filename}`]({url_map[fw_env]})\n')
                        else:
                            new_lines.append(f'  - `{filename}`\n')
                    seen_contributors.add(key)
                    print(f'[OK] Added new section: {disp} Firmware ({len(by_contributor[key])} entries)')
            new_lines.append(line)
            i += 1
            continue

        # Contributor section header
        m = re.match(r'^###\s+(.+?)\s+Firmware\s*$', line)
        if m:
            key = m.group(1).lower()
            i += 1
            # Skip existing entry lines
            while i < len(lines) and lines[i].strip().startswith('- '):
                i += 1
            if key not in seen_contributors:
                seen_contributors.add(key)
                if key in by_contributor:
                    disp = display_name[key]
                    new_lines.append(f'### {disp} Firmware\n')
                    for fw_env, _ in by_contributor[key]:
                        filename = f'{fw_env}.bin'
                        if fw_env in url_map:
                            new_lines.append(f'  - [`{filename}`]({url_map[fw_env]})\n')
                        else:
                            new_lines.append(f'  - `{filename}`\n')
                    print(f'[OK] Updated {disp} Firmware section ({len(by_contributor[key])} entries)')
                else:
                    print(f'[!] No firmware for {m.group(1)} -- section removed')
            continue

        new_lines.append(line)
        i += 1

    output_path.write_text(''.join(new_lines), encoding='utf-8')
    print(f'[*] Patched {output_path}')


def generate_esp_web_tools_manifests(firmwares, output_dir, version='', filesystem_images=None):
    """
    Generate ESP Web Tools manifest JSON files. Cleans manifests/ before writing.

    Each manifest gains a non-standard "filesystem" entry naming the shared board image
    and its offset. ESP Web Tools ignores the key; docs/flasher.html uses it to offer a
    full reinstall when "Erase the whole flash" is ticked.
    """
    filesystem_images = filesystem_images or {}
    
    if not version:
        version = get_version_from_options_h()
    
    # Create manifests directory
    manifests_dir = output_dir / 'manifests'

    # Clean existing manifests before regenerating
    if manifests_dir.exists():
        for f in manifests_dir.glob('*.json'):
            f.unlink()

    manifests_dir.mkdir(parents=True, exist_ok=True)

    if not version:
        version = get_version_from_options_h()

    for board_env, chip_family, fw_env, friendly_name, _ in firmwares:
        bl_offset = 0 if chip_family == 'ESP32-S3' else 4096
        manifest = {
            'name': f'ehRadio - {friendly_name}',
            'version': version,
            'funding_url': 'https://github.com/sponsors/trip5',
            'builds': [
                {
                    'chipFamily': chip_family,
                    'parts': [
                        {'path': f'../firmware/{board_env}_bootloader.bin', 'offset': bl_offset},
                        {'path': f'../firmware/{board_env}_partitions.bin', 'offset': 32768},
                        {'path': f'../firmware/{fw_env}.bin',               'offset': 65536},
                    ],
                }
            ],
        }
        filesystem = filesystem_images.get(board_env)
        if filesystem:
            manifest['filesystem'] = {
                'path': filesystem['path'],
                'offset': filesystem['offset'],
                'size': filesystem['size'],
            }
        manifest_file = manifests_dir / f'{fw_env}-manifest.json'
        with open(manifest_file, 'w', encoding='utf-8') as f:
            json.dump(manifest, f, indent=2)
            f.write('\n')

    print(f'[*] Generated {len(firmwares)} ESP Web Tools manifests in {manifests_dir}')


def generate_firmware_info_json(firmwares, output_path, version=''):
    if not version:
        version = get_version_from_options_h()

    variants = sorted(
        [
            {
                'name': f'{contributor} {friendly_name}',
                'manifest': f'manifests/{fw_env}-manifest.json',
                'description': f'ehRadio - {fw_env}',
            }
            for _, _, fw_env, friendly_name, contributor in firmwares
        ],
        key=lambda v: v['name'],
    )

    firmware_info = {
        'project': 'ehRadio',
        'version': version,
        'variants': variants,
    }

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with open(output_path, 'w', encoding='utf-8') as f:
        json.dump(firmware_info, f, indent=2)
        f.write('\n')

    print(f'[*] Generated {output_path} with {len(variants)} firmware variants')


def unbalanced_directives(path):
    """Return [(line_no, reason)] for unbalanced #if/#else/#endif in one file."""
    text = path.read_text(encoding='utf-8', errors='replace')
    # Blank the block comments but keep their newlines: line numbers must not shift.
    text = re.sub(r'/\*.*?\*/', lambda m: '\n' * m.group(0).count('\n'),
                  text, flags=re.DOTALL)

    problems = []
    open_lines = []
    for number, raw in enumerate(text.splitlines(), start=1):
        line = raw.split('//', 1)[0].strip()
        match = re.match(r'#\s*(\w+)', line)
        if not match:
            continue
        directive = match.group(1)
        if directive in COND_OPEN:
            open_lines.append(number)
        elif directive in COND_MID:
            if not open_lines:
                problems.append((number, f'#{directive} without #if'))
        elif directive == 'endif':
            if open_lines:
                open_lines.pop()
            else:
                problems.append((number, '#endif without #if'))
    for number in open_lines:
        problems.append((number, '#if never closed'))
    return problems


def check_configs(folders):
    """Pre-pass over every contributor config. Returns a list of error strings."""
    errors = []
    for folder in folders:
        ini = folder / 'platformio.ini'
        if not ini.is_file():
            errors.append(f'{rel(ini)}: missing')
        options = folder / 'myoptions.h'
        for number, reason in unbalanced_directives(options):
            errors.append(f'{rel(options)}:{number}  {reason}')
    return errors


def rel(path):
    try:
        return str(path.relative_to(ROOT)).replace('\\', '/')
    except ValueError:
        return str(path)


def make_scratch():
    """Copy the working tree - not git HEAD: root platformio.ini is gitignored."""
    if SCRATCH.exists():
        shutil.rmtree(SCRATCH)
    shutil.copytree(
        ROOT, SCRATCH,
        ignore=lambda _directory, names: [n for n in names if n in COPY_IGNORE],
        symlinks=False,
    )


def find_pio():
    for candidate in ('pio', 'platformio'):
        found = shutil.which(candidate)
        if found:
            return [found]
    try:
        subprocess.run([sys.executable, '-m', 'platformio', '--version'],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        return [sys.executable, '-m', 'platformio']
    except Exception:
        return None


def build_env(pio, folder, env, log_path, verbose):
    """Run one environment in the scratch tree. Returns (ok, tail_lines)."""
    shutil.copy2(folder / 'platformio.ini', SCRATCH / 'platformio.ini')
    shutil.copy2(folder / 'myoptions.h', SCRATCH / 'myoptions.h')

    command = pio + (['run', '-e', env] if env else ['run'])
    tail = []
    with open(log_path, 'w', encoding='utf-8', errors='replace') as log:
        log.write(f'$ {" ".join(command)}   (cwd {SCRATCH})\n\n')
        if verbose:
            process = subprocess.Popen(command, cwd=SCRATCH, stdout=subprocess.PIPE,
                                       stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                                       errors='replace')
            for line in process.stdout:
                print('    ' + line.rstrip())
                log.write(line)
                tail = (tail + [line.rstrip()])[-LOG_LINES_ON_FAIL:]
            ok = process.wait() == 0
        else:
            done = subprocess.run(command, cwd=SCRATCH, stdout=subprocess.PIPE,
                                  stderr=subprocess.STDOUT, text=True, encoding='utf-8',
                                  errors='replace')
            log.write(done.stdout)
            tail = done.stdout.splitlines()[-LOG_LINES_ON_FAIL:]
            ok = done.returncode == 0
    return ok, tail


def describe_envs(envs):
    if not envs:
        return 'every env'
    if len(envs) <= 3:
        return ', '.join(envs)
    return f'{len(envs)} envs'


def build_all(folders, verbose):
    """Build every folder's every env in the scratch copy. 0 ok, 1 failed, 2 refused."""
    pio = find_pio()
    if not pio:
        print('[X] Refusing to build: neither pio nor platformio is on PATH.')
        return 2

    log_dir = Path(tempfile.mkdtemp(prefix='ehradio-buildall-'))
    results = []
    try:
        make_scratch()
        for folder in folders:
            envs = read_pio_envs(folder / 'platformio.ini')
            started = time.time()
            folder_ok = True
            # One env at a time: that is what makes a failure attributable to an environment
            # rather than to "somewhere in this folder".
            for env in (envs or [None]):
                log_path = log_dir / f'{folder.name}__{env or "all"}.log'
                ok, tail = build_env(pio, folder, env, log_path, verbose)
                if ok:
                    continue
                folder_ok = False
                print(f'  [FAIL] {folder.name:<20} {env or "every env"}')
                for line in tail:
                    print(f'         | {line}')
                print(f'         log: {log_path}')
            if folder_ok:
                elapsed = time.time() - started
                print(f'  [OK]   {folder.name:<20} {describe_envs(envs)}  ({elapsed:.0f}s)')
            results.append((folder.name, folder_ok))
    finally:
        shutil.rmtree(SCRATCH, ignore_errors=True)

    print('')
    failed = [name for name, ok in results if not ok]
    if not failed:
        shutil.rmtree(log_dir, ignore_errors=True)
        print(f'  {len(results)} ok -> every contributor builds')
        return 0
    print(f'  {len(results) - len(failed)} ok, {len(failed)} failed ({", ".join(failed)})')
    print(f'  Build logs kept in: {log_dir}')
    print('  To dig into one folder:')
    print(f'    set PYTHONIOENCODING=utf-8 && python builds/use_build.py {failed[0]}')
    print('  Release files were NOT written.')
    return 1


def parse_args():
    parser = argparse.ArgumentParser(
        description='Build every contributor firmware, then generate the release metadata.')
    parser.add_argument('--yes', action='store_true',
                        help='skip the confirmation prompt (needed in CI)')
    parser.add_argument('--check', action='store_true',
                        help='phases 1-2 only: validate configs, build nothing, write nothing')
    parser.add_argument('--no-build', action='store_true',
                        help='skip the build phase (release workflows only: they compile next)')
    parser.add_argument('--verbose', action='store_true',
                        help='stream compiler output instead of capturing it')
    return parser.parse_args()


def confirm_build():
    """Ask before compiling. False means cancel - including when there is no terminal."""
    if not sys.stdin.isatty():
        print('[X] Refusing to build without confirmation (no terminal). Re-run with --yes.')
        return False
    try:
        answer = input('Are you ready to compile all builds? [y/N] ')
    except (EOFError, KeyboardInterrupt):
        # A tty that gives no answer (closed, piped from nul, Ctrl-C) must cancel cleanly.
        print('\nCancelled.')
        return False
    if answer.strip().lower() not in ('y', 'yes'):
        print('Cancelled.')
        return False
    return True


def main():
    args = parse_args()

    script_dir   = SCRIPT_DIR
    releases_dir = script_dir / 'releases'

    # --- Phase 1: naming and filesystem ---
    naming_errors = validate_build_naming(script_dir)
    if naming_errors:
        print(f'[X] Contributor build check failed ({len(naming_errors)} problem(s)):')
        for error in naming_errors:
            print(f'  - {error}')
        print('')
        print('    Rule: the FIRMWARE filename must be "<contributor folder>_<env>.bin" and the')
        print('    contributor field of its comment must match that folder name. The release')
        print('    workflows build "<folder>_<env>.bin", so any other name yields no asset.')
        print('    A contributor platformio.ini must also ask for littlefs, because the firmware')
        print('    mounts LittleFS and the flasher writes a littlefs image into that partition.')
        return 1
    print('[OK] Contributor build check passed')

    folders = contributor_folders(script_dir)
    if not folders:
        print('[!] No contributor folders found in builds/ (need platformio.ini + myoptions.h).')
        return 1

    # --- Phase 2: preprocessor balance ---
    config_errors = check_configs(folders)
    if config_errors:
        print(f'[X] Config pre-pass failed ({len(config_errors)} problem(s)):')
        for error in config_errors:
            print(f'  - {error}')
        print('')
        print('    An #endif with no open #if closes the include guard early, and')
        print('    options_overrides.h includes myoptions.h in every translation unit, so every')
        print('    file then fails to compile. Fix the config, do not work around the build.')
        return 1
    print('[OK] Config pre-pass passed')

    if args.check:
        print(f'[OK] {len(folders)} contributor folder(s) validated - nothing built, nothing written')
        return 0

    active = [marker.name for marker in SWAP_MARKERS if marker.exists()]
    if active:
        print(f'[X] Refusing to run: a contributor swap is still active ({", ".join(active)}).')
        print('    Restore first:  python builds/use_build.py')
        return 2

    # --- Phase 3: build everything ---
    if args.no_build:
        if not os.environ.get('CI'):
            print('[!] Skipping the compile. The dev-to-main PR check still builds every')
            print('    environment, so a build failure will surface there instead of here.')
    else:
        print('Testing by building all firmwares in builds/')
        if not args.yes and not confirm_build():
            return 2
        build_rc = build_all(folders, args.verbose)
        if build_rc:
            return build_rc

    all_firmwares = []
    all_url_map   = {}
    seen_fw_envs  = set()
    skipped_defs  = 0

    print('Building firmware.txt and manifests...')

    for subfolder in sorted(script_dir.iterdir()):
        if not subfolder.is_dir():
            continue
        if subfolder.name == 'releases':
            continue
        if not re.match(r'^[a-zA-Z0-9_-]+$', subfolder.name):
            continue

        myoptions_path = subfolder / 'myoptions.h'
        if not myoptions_path.exists():
            continue

        firmwares, url_map, missing_name_envs = parse_myoptions_h(myoptions_path)
        if not firmwares:
            continue

        print(f'{subfolder.name}/')
        for fw_env in missing_name_envs:
            print(f'  [!] Missing FIRMWARE_NAME for {fw_env} in {myoptions_path}')
        skipped_defs += len(missing_name_envs)

        for entry in firmwares:
            fw_env = entry[2]
            if fw_env in seen_fw_envs:
                print(f'  {fw_env} - Duplicate!')
                skipped_defs += 1
                continue
            seen_fw_envs.add(fw_env)
            all_firmwares.append(entry)
            if fw_env in url_map:
                all_url_map[fw_env] = url_map[fw_env]
            print(f'  {fw_env} - OK')

    if not all_firmwares:
        print('\n[!] No firmware definitions found in any subfolder')
        return 1

    print(f'\n[X] Skipped: {skipped_defs} firmware definitions')
    print(f'[OK] Total: {len(all_firmwares)} firmware definitions')

    releases_dir.mkdir(parents=True, exist_ok=True)

    filesystem_images = collect_filesystem_images(script_dir, script_dir.parent)
    # Only boards that a firmware actually uses need an image: an unused board section
    # (one that no FIRMWARE entry points at) would otherwise build a file nobody flashes.
    used_boards = {entry[0] for entry in all_firmwares}
    filesystem_images = {board: info for board, info in filesystem_images.items()
                         if board in used_boards}
    if filesystem_images:
        print('')
        print('[*] Filesystem images the release must carry:')
        for board_env, info in sorted(filesystem_images.items()):
            print(f"  {board_env}_filesystem.bin  offset 0x{info['offset']:x}  "
                  f"{info['size'] // 1024} KB  (from {info['source']})")
    else:
        print('[!] No filesystem images resolved; manifests will not offer a full reinstall')

    generate_firmware_txt(all_firmwares,              releases_dir / 'firmware.txt')
    generate_releases_md(all_firmwares, all_url_map,  releases_dir / 'releases.md')
    generate_esp_web_tools_manifests(all_firmwares,   releases_dir / 'web_assets',
                                     filesystem_images=filesystem_images)
    generate_firmware_info_json(all_firmwares,        releases_dir / 'web_assets' / 'firmware-info.json')

    return 0


if __name__ == '__main__':
    exit(main())

