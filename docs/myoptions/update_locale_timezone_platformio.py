#!/usr/bin/env python3
# update_json_locale_timezone.py
#
# Regenerates timezones.json and locale.json in the docs/myoptions folder by
# pulling data from elsewhere in the repository and transforming it.
# Copies platformio.ini from builds/generator folder as well -
# expected that patch_pio_ini_files.py was run and patched already.
#
# DEPENDENCIES: Python standard library only (json, pathlib, re).
#   No pip install needed.
#
# USAGE:
#   python docs/myoptions/update_json_locale_timezone.py
#   -- OR --  (from inside docs/myoptions/)
#   python update_json_locale_timezone.py
#
# ==============================================================================
# SOURCE / DESTINATION PATHS
# Edit these variables if files are ever relocated inside the repository.
# All paths are relative to THIS script file.
# ==============================================================================

import json
import pathlib
import re
import shutil

SCRIPT_DIR = pathlib.Path(__file__).parent.resolve()

# Timezone source: flat JSON object  { "Continent/City": "POSIX_string", ... }
TIMEZONE_SRC = SCRIPT_DIR / '../../data/www/timezones.json'

# Webui locale JSON files (one per locale, each contains locale_code / locale / locale_en)
WEBUI_LOCALE_DIR = SCRIPT_DIR / '../../src/locale/webui'

# Directory containing displayL10n_*.h files - used to filter available locales
LOCALE_H_DIR = SCRIPT_DIR / '../../src/locale'

# Output files (written next to this script, i.e. into docs/myoptions/)
TIMEZONE_OUT = SCRIPT_DIR / 'timezones.json'
LOCALE_OUT   = SCRIPT_DIR / 'locale.json'

# This will just do a straight copy from the builds folder
PLATFORMIO_SRC = SCRIPT_DIR / '../../builds/generator/platformio.ini'
PLATFORMIO_OUT = SCRIPT_DIR / 'platformio.ini'

# ==============================================================================
# OPERATION 1 - timezones.json
# Simple text-level transform: prepend ["Time Zone", and append ]
# ==============================================================================

def update_timezones():
    src = TIMEZONE_SRC.resolve()
    if not src.exists():
        print(f'ERROR: timezone source not found:\n  {src}')
        return False

    raw = src.read_text(encoding='utf-8').rstrip()

    # Wrap the existing JSON object inside an array with a header string
    out_text = '["Time Zone",\n' + raw + '\n]\n'

    TIMEZONE_OUT.write_text(out_text, encoding='utf-8')
    print(f'[timezones] Written -> {TIMEZONE_OUT.name}')
    return True


# ==============================================================================
# OPERATION 2 - locale.json
# Reads every *.json from WEBUI_LOCALE_DIR, extracts locale_code / locale /
# locale_en, then keeps only entries where a matching displayL10n_<code>.h
# file exists in LOCALE_H_DIR.  Outputs in the same array format as the
# original locale.json.
# ==============================================================================

def update_locale():
    webui_dir = WEBUI_LOCALE_DIR.resolve()
    h_dir     = LOCALE_H_DIR.resolve()

    if not webui_dir.exists():
        print(f'ERROR: webui locale directory not found:\n  {webui_dir}')
        return False
    if not h_dir.exists():
        print(f'ERROR: locale .h directory not found:\n  {h_dir}')
        return False

    # Collect all .h filenames directly inside LOCALE_H_DIR (not subdirs)
    h_filenames = {
        f.name
        for f in h_dir.iterdir()
        if f.is_file() and f.suffix == '.h'
    }

    entries = []
    skipped = []

    # Process webui JSON files in alphabetical order (gives a sorted output)
    for json_file in sorted(webui_dir.glob('*.json')):
        raw_text = json_file.read_text(encoding='utf-8')
        data = None
        try:
            data = json.loads(raw_text)
        except Exception:
            # The file has invalid escape sequences deeper in the file.
            # We only need the first three fields (always at the top), so
            # extract them with a simple regex rather than parsing the whole file.
            def _extract(key):
                m = re.search(r'"' + key + r'"\s*:\s*"((?:[^"\\]|\\.)*)"', raw_text)
                return m.group(1) if m else ''
            data = {
                'locale_code': _extract('locale_code'),
                'locale':      _extract('locale'),
                'locale_en':   _extract('locale_en'),
            }

        code = data.get('locale_code', '').strip()
        if not code:
            print(f'  WARNING: no locale_code in {json_file.name}, skipping')
            continue

        # Only include this locale if a .h filename in LOCALE_H_DIR contains
        # the locale code string anywhere in the filename.
        # e.g. "be_BY" matches "displayL10n_be_BY.h"
        has_h = any(code in fname for fname in h_filenames)
        if not has_h:
            skipped.append(code)
            continue

        entries.append({
            'locale_code': code,
            'locale':      data.get('locale', ''),
            'locale_en':   data.get('locale_en', '')
        })

    if skipped:
        print(f'  Skipped (no .h file found): {", ".join(skipped)}')

    # Build output text that exactly matches the original locale.json format:
    #
    # ["Locale",
    # {
    #   "locale_code": "be_BY",
    #   "locale": "беларуская мова",
    #   "locale_en": "Belarusian"
    # },
    # ...
    # ]

    parts = ['["Locale",']
    for i, entry in enumerate(entries):
        comma = ',' if i < len(entries) - 1 else ''
        parts.append('{')
        parts.append('  "locale_code": ' + json.dumps(entry['locale_code'], ensure_ascii=False) + ',')
        parts.append('  "locale": '      + json.dumps(entry['locale'],      ensure_ascii=False) + ',')
        parts.append('  "locale_en": '   + json.dumps(entry['locale_en'],   ensure_ascii=False))
        parts.append('}' + comma)
    parts.append(']')

    out_text = '\n'.join(parts) + '\n'
    LOCALE_OUT.write_text(out_text, encoding='utf-8')
    print(f'[locale]    Written -> {LOCALE_OUT.name}  ({len(entries)} locales included)')
    return True


# ==============================================================================
# OPERATION 3 - platformio.ini
# Expected that patch_pio_ini_files.py was run and patched
# builds/generator/platformio.ini already... so just copy
# ==============================================================================

def update_platformio():
    src = PLATFORMIO_SRC.resolve()
    dst = PLATFORMIO_OUT.resolve()

    if not src.exists():
        print(f'ERROR: platformio source not found:\n  {src}')
        return False

    shutil.copyfile(src, dst)
    print(f'[platformio] Written -> {PLATFORMIO_OUT.name}')
    return True


# ==============================================================================

if __name__ == '__main__':
    print('=== update_json_locale_timezone.py ===')
    print()
    update_timezones()
    print()
    update_locale()
    print()
    update_platformio()
    print()
    print('Done.')
