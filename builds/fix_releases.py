#!/usr/bin/env python3
"""
fix_releases.py

Scans all builds/*/myoptions.h files, generates:
  builds/releases/firmware.txt
  builds/releases/releases.md (patched in-place)
  builds/releases/web_assets/firmware-info.json
  builds/releases/web_assets/manifests/*.json

Run from the builds/ directory:
  python fix_releases.py
"""

import re
import json
from pathlib import Path


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
      firmwares  - list of 5-tuples (board_env, chip_family, fw_env, friendly_name, contributor)
      url_map    - dict mapping fw_env -> url
    """
    content = _strip_comments(Path(myoptions_path).read_text(encoding='utf-8'))

    firmwares = []
    url_map   = {}

    fw_pattern   = re.compile(r'#define\s+FIRMWARE\s+"([^"]+)"\s*//\s*"([^"]+)",\s*"([^"]+)",\s*"([^"]+)"')
    name_pattern = re.compile(r'#define\s+FIRMWARE_NAME\s+"([^"]+)"(?:\s*//\s*"([^"]*)")?')

    for m in fw_pattern.finditer(content):
        filename    = m.group(1)
        board_env   = m.group(2)
        chip_family = m.group(3)
        contributor = m.group(4)

        # Skip board_ entries (bare boards without full config)
        if filename.startswith('board_'):
            continue

        fw_env = filename[:-4] if filename.endswith('.bin') else filename

        # Look for FIRMWARE_NAME within next 500 chars
        after    = content[m.end():m.end() + 500]
        nm       = name_pattern.search(after)
        if nm:
            friendly_name = nm.group(1)
            url = nm.group(2) or ''
            if url:
                url_map[fw_env] = url
        else:
            friendly_name = fw_env

        firmwares.append((board_env, chip_family, fw_env, friendly_name, contributor))

    return firmwares, url_map


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
    print(f'✅ Generated {output_path} with {len(firmwares)} firmware entries')


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
        print(f'⚠️  {output_path} not found, skipping releases.md update')
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
                    print(f'✅ Added new section: {disp} Firmware ({len(by_contributor[key])} entries)')
            new_lines.append(line)
            i += 1
            continue

        # Contributor section header
        m = re.match(r'^###\s+(\w+)\s+Firmware\s*$', line)
        if m:
            key = m.group(1).lower()
            seen_contributors.add(key)
            i += 1
            # Skip existing entry lines
            while i < len(lines) and lines[i].strip().startswith('- '):
                i += 1
            if key in by_contributor:
                disp = display_name[key]
                new_lines.append(f'### {disp} Firmware\n')
                for fw_env, _ in by_contributor[key]:
                    filename = f'{fw_env}.bin'
                    if fw_env in url_map:
                        new_lines.append(f'  - [`{filename}`]({url_map[fw_env]})\n')
                    else:
                        new_lines.append(f'  - `{filename}`\n')
                print(f'✅ Updated {disp} Firmware section ({len(by_contributor[key])} entries)')
            else:
                print(f'⚠️  No firmware for {m.group(1)} — section removed')
            continue

        new_lines.append(line)
        i += 1

    output_path.write_text(''.join(new_lines), encoding='utf-8')
    print(f'✅ Patched {output_path}')


def generate_esp_web_tools_manifests(firmwares, output_dir, version=''):
    """Generate ESP Web Tools manifest JSON files. Cleans manifests/ before writing."""
    
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
        bl_offset = 0 if chip_family in ('ESP32-S3', 'ESP32-C3') else 4096
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
        manifest_file = manifests_dir / f'{fw_env}-manifest.json'
        with open(manifest_file, 'w', encoding='utf-8', newline='\n') as f:
            json.dump(manifest, f, indent=2)
            f.write('\n')

    print(f'✅ Generated {len(firmwares)} ESP Web Tools manifests in {manifests_dir}')


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
    with open(output_path, 'w', encoding='utf-8', newline='\n') as f:
        json.dump(firmware_info, f, indent=2)
        f.write('\n')

    print(f'✅ Generated {output_path} with {len(variants)} firmware variants')


def main():
    script_dir    = Path(__file__).parent  # builds/
    releases_dir  = script_dir / 'releases'
    all_firmwares = []
    all_url_map   = {}
    seen_fw_envs  = set()

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

        firmwares, url_map = parse_myoptions_h(myoptions_path)
        if not firmwares:
            continue

        print(f'{subfolder.name}/')

        for entry in firmwares:
            fw_env = entry[2]
            if fw_env in seen_fw_envs:
                print(f'  {fw_env} - Duplicate!')
                continue
            seen_fw_envs.add(fw_env)
            all_firmwares.append(entry)
            if fw_env in url_map:
                all_url_map[fw_env] = url_map[fw_env]
            print(f'  {fw_env} - OK')

    if not all_firmwares:
        print('\n⚠️  No firmware definitions found in any subfolder')
        return 1

    print(f'\n✅ Total: {len(all_firmwares)} firmware definitions')

    releases_dir.mkdir(parents=True, exist_ok=True)

    generate_firmware_txt(all_firmwares,              releases_dir / 'firmware.txt')
    generate_releases_md(all_firmwares, all_url_map,  releases_dir / 'releases.md')
    generate_esp_web_tools_manifests(all_firmwares,   releases_dir / 'web_assets')
    generate_firmware_info_json(all_firmwares,        releases_dir / 'web_assets' / 'firmware-info.json')

    return 0


if __name__ == '__main__':
    exit(main())

