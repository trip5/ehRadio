#!/usr/bin/env python3
"""
Scan data/www/*.html and *.js files and src/core/netserver.h for translation keys and compare with locale JSON.

NOTE:
    Check .md files for how to install full translation support

USAGE:
    python www_tool.py <locale> [mode] [options]
    python www_tool.py * [mode] [options]
    python www_tool.py <locale> --merge <file.json> [options]
    python www_tool.py <locale|*> --newkeys [file.json] [options]
    python www_tool.py <locale> --extract [file]

TARGET:
    <locale>         One locale → .json file in the www folder (en_US → en_US.json)
    *                All locales → all .json files

MODES:
    (default)        Interactive mode - prompts for missing keys, ask about cleanup, ask about sort
    --fast, -f       Add all missing keys at once using HTML Found text, skip individual edits (no prompt unless translation fails)
    --every, -e      Prompt to review every single key using HTML Found text (detailed proofreading)
    --diff, -d       Only prompt when HTML text differs from JSON (to compare hardcoded)
    --ndiff, -n      Only prompt when HTML text is same as JSON (to fix untranslated text)
    --merge, -m FILE Merge a partial locale JSON into ONE locale file
    --extract, -x [FILE]
                     Print the locale's values for the keys the --newkeys template lists, as a labelled sheet for a
                     native-speaker check.  With no FILE the sheet goes to the console; with a FILE it is APPENDED
                     to that file - which is never read back or rewritten, so a www check and a display check can
                     share one sheet.  Nothing else has a function in this mode.

OPTIONS:
    --translate, -t  Translate HTML Found text (can't use with --diff).  An unchanged or failed translation asks
                     [y]es / [a]lways for this key / [n]o - stop, in --fast; the interactive modes ask yes/no
    --clean, -c      Auto-delete unused keys (no prompt)
    --sort, -s       Auto-sort keys hierarchically at end (no prompt)
    --newkeys, -k    Write the keys that are in the source but not yet in the locale(s) into a template file
                     for a translator to fill in and send back for --merge
    --key NAME       Work on one key only, across every locale: NAME key must be in HTML/JS files and is written
                     even where the locale already has it, and no other key is examined

EXAMPLES:
    # Interactive check of one file
    py www_tool.py fr_FR

    # Fast mode WITH translation (auto-translate all missing keys in all files, put new keys in www_newkeys.json)
    py www_tool.py * --translate --fast --clean --sort --newkeys

    # Diff mode (never uses translation, useful for checking that hard-coded text and locale file are same)
    py www_tool.py en_US --diff

    # Ndiff mode (prompt only when text matches - to find/fix untranslated text with translation)
    py www_tool.py de_DE --ndiff --translate --clean --sort

    # Merge a contributor's partial file (only their keys), then tidy the file
    py www_tool.py ru_RU --merge changes.json --clean --sort

    # Collect every key the locales still lack into a template for the translators
    py www_tool.py * --newkeys --sort

    # Show what one locale currently says for the recently added keys, to paste into a forum post
    py www_tool.py ru_RU --extract

    # The same, appended to a sheet beside this script (a display check can follow it into the same file)
    py www_tool.py ru_RU --extract RU_check.txt

    # Redo one key everywhere, after its text in the page changed
    py www_tool.py * --translate --fast --clean --sort --key msg_sd_manager_closed
"""

import os
import sys
import json
import re
import argparse
import glob
import subprocess
import shlex
try:
    import msvcrt  # Windows
    WINDOWS = True
except ImportError:
    import termios
    import tty
    WINDOWS = False

# Translation service configuration
_translation_service = None  # None, 'deepl', or other future services
_translation_check_done = False
_translation_input_locale = "en_US"  # Default source language for HTML/JS text

# Cache for per-language translation support: locale_code -> True (works) / False (failed)
_translation_lang_cache = {}
# Track if we've shown a translation error (to avoid spam)
_translation_error_shown = False


def detect_translation_service():
    """
    Detect available translation service by scanning for trans_*.key files.
    Returns: service name (e.g., 'deepl', 'google') or None if none available
    """
    global _translation_service, _translation_check_done
    
    if _translation_check_done:
        return _translation_service
    
    _translation_check_done = True
    
    # Get script directory
    script_dir = os.path.dirname(os.path.abspath(__file__))
    
    # Scan for any trans_*.key files
    key_files = glob.glob(os.path.join(script_dir, 'trans_*.key'))
    
    for key_file in sorted(key_files):  # Sorted for consistent order
        # Extract service name from filename: trans_deepl.key -> deepl
        basename = os.path.basename(key_file)
        service_name = basename[6:-4]  # Remove 'trans_' prefix and '.key' suffix
        
        # Check if key file has content (not just comments)
        has_key = False
        try:
            with open(key_file, 'r', encoding='utf-8') as f:
                for line in f:
                    line = line.strip()
                    if line and not line.startswith('#'):
                        has_key = True
                        break
        except Exception:
            continue
        
        if not has_key:
            continue
        
        # Check if matching .py script exists
        script_file = os.path.join(script_dir, f'trans_{service_name}.py')
        if os.path.exists(script_file):
            # Found a valid translation service!
            _translation_service = service_name
            return _translation_service
    
    # No translation service available
    _translation_service = None
    return _translation_service


def translate_text(text, source_locale=None, target_locale=None):
    """
    Translate text using available translation service.
    Returns translated text or None if translation failed.
    Auto-discovers language support and caches results.
    
    Args:
        text: Text to translate
        source_locale: Source language code (default: uses _translation_input_locale)
        target_locale: Target language code (e.g., de_DE, hr_HR)
    """
    global _translation_lang_cache, _translation_input_locale, _translation_error_shown
    
    # Use global default if not specified
    if source_locale is None:
        source_locale = _translation_input_locale
    
    # Check if translation service is available
    service = detect_translation_service()
    if not service or not target_locale:
        return None
    
    # Check cache - if we already know this language doesn't work, skip
    if target_locale in _translation_lang_cache and not _translation_lang_cache[target_locale]:
        return None
    
    # Call external translation script (generic for any service)
    if service:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        script_path = os.path.join(script_dir, f'trans_{service}.py')
        
        try:
            # Pass: source_lang target_lang "text"
            result = subprocess.run(
                [sys.executable, script_path, source_locale, target_locale, text],
                capture_output=True,
                timeout=30,
                text=True,
                encoding='utf-8',
                errors='replace'  # Replace invalid UTF-8 with ? instead of crashing
            )
            
            # Check if translation succeeded
            if result.returncode == 0:
                translated = result.stdout.strip()
                if translated:
                    # Success! Cache this language as working
                    _translation_lang_cache[target_locale] = True
                    return translated
            
            # Failed - show error from stderr on first failure
            if result.stderr and not _translation_error_shown:
                error_msg = result.stderr.strip()
                if error_msg:
                    print(f"\n⚠ Translation error: {error_msg}")
                    print("  Source text requires confirmation per key.\n")
                    _translation_error_shown = True
            
            # Cache as not supported
            _translation_lang_cache[target_locale] = False
            return None
            
        except subprocess.TimeoutExpired:
            # Timeout - show warning on first occurrence
            if not _translation_error_shown:
                print(f"\n⚠ Translation timeout (>30s) for {target_locale}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except (UnicodeDecodeError, UnicodeError) as e:
            # Unicode error from subprocess
            if not _translation_error_shown:
                print(f"\n⚠ Translation encoding error for {target_locale}: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except Exception as e:
            # Other error - show on first occurrence
            if not _translation_error_shown:
                print(f"\n⚠ Translation error: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
    
    # Unknown service
    return None


def get_key():
    """Get a single keypress (cross-platform)."""
    if WINDOWS:
        return msvcrt.getch()
    else:
        fd = sys.stdin.fileno()
        old_settings = termios.tcgetattr(fd)
        try:
            tty.setraw(sys.stdin.fileno())
            ch = sys.stdin.read(1)
        finally:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        return ch


def extract_keys_from_html_js(file_path):
    """Extract translation keys and their display text from HTML/JS files."""
    keys_found = {}
    
    with open(file_path, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Pattern 1a: data-i18n="key" with text content between tags
    for match in re.finditer(r'data-i18n=["\']([^"\']+)["\'](?:[^>]*>([^<]+)<)?', content):
        key = match.group(1)
        text = match.group(2).strip() if match.group(2) else ""
        if key not in keys_found and text:
            keys_found[key] = text
    
    # Pattern 1b: data-i18n="key" with placeholder attribute (for inputs)
    for match in re.finditer(r'data-i18n=["\']([^"\']+)["\'][^>]*placeholder=["\']([^"\']+)["\']', content):
        key = match.group(1)
        text = match.group(2).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1c: placeholder first, then data-i18n (reversed order)
    for match in re.finditer(r'placeholder=["\']([^"\']+)["\'][^>]*data-i18n=["\']([^"\']+)["\']', content):
        key = match.group(2)
        text = match.group(1).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1d: data-i18n="key" with value attribute (for input buttons)
    for match in re.finditer(r'data-i18n=["\']([^"\']+)["\'][^>]*value=["\']([^"\']+)["\']', content):
        key = match.group(1)
        text = match.group(2).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1e: value first, then data-i18n (reversed order)
    for match in re.finditer(r'value=["\']([^"\']+)["\'][^>]*data-i18n=["\']([^"\']+)["\']', content):
        key = match.group(2)
        text = match.group(1).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1f: data-i18n="key" with title attribute (for tooltips)
    for match in re.finditer(r'data-i18n=["\']([^"\']+)["\'][^>]*title=["\']([^"\']+)["\']', content):
        key = match.group(1)
        text = match.group(2).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1g: title first, then data-i18n (reversed order)
    for match in re.finditer(r'title=["\']([^"\']+)["\'][^>]*data-i18n=["\']([^"\']+)["\']', content):
        key = match.group(2)
        text = match.group(1).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1h: data-i18n="key" with alt attribute (for images)
    for match in re.finditer(r'data-i18n=["\']([^"\']+)["\'][^>]*alt=["\']([^"\']+)["\']', content):
        key = match.group(1)
        text = match.group(2).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 1i: alt first, then data-i18n (reversed order)
    for match in re.finditer(r'alt=["\']([^"\']+)["\'][^>]*data-i18n=["\']([^"\']+)["\']', content):
        key = match.group(2)
        text = match.group(1).strip()
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 2: t('key', 'fallback text', ...) - with optional additional parameters
    for match in re.finditer(r'\bt\(["\']([^"\']+)["\'],\s*["\']([^"\']+)["\'](?:\s*,\s*[^)]+)?\)', content):
        key = match.group(1)
        text = match.group(2)
        if key not in keys_found:
            keys_found[key] = text
    
    # Pattern 3: t('key') - single argument form (skip if already found in pattern 2)
    for match in re.finditer(r'\bt\(["\']([^"\']+)["\']\)', content):
        key = match.group(1)
        if key not in keys_found:
            keys_found[key] = ""
    
    return keys_found


def scan_www_folder(www_path):
    """Scan all .html and .js files in www folder, plus netserver.h for PROGMEM HTML."""
    all_keys = {}
    
    for filename in os.listdir(www_path):
        if filename.endswith('.html') or filename.endswith('.js'):
            file_path = os.path.join(www_path, filename)
            keys = extract_keys_from_html_js(file_path)
            
            for key, text in keys.items():
                if key not in all_keys:
                    all_keys[key] = {'text': text, 'files': [filename]}
                else:
                    if filename not in all_keys[key]['files']:
                        all_keys[key]['files'].append(filename)
    
    # Also scan netserver.h for data-i18n keys in emptyfs_html PROGMEM string
    netserver_h = os.path.join(www_path, '..', '..', 'src', 'core', 'netserver.h')
    netserver_h = os.path.abspath(netserver_h)
    if os.path.exists(netserver_h):
        keys = extract_keys_from_html_js(netserver_h)
        for key, text in keys.items():
            if key not in all_keys:
                all_keys[key] = {'text': text, 'files': ['netserver.h']}
            else:
                if 'netserver.h' not in all_keys[key]['files']:
                    all_keys[key]['files'].append('netserver.h')
    
    return all_keys


# Keys the user answered "always" for during an automatic translation pass: their source text is used for the rest of
# the run without asking again.  A run over * asks about the same key once per locale, and an unchanged translation is
# usually a property of the key itself, so one answer has to cover the whole run.
_source_text_always = set()


def confirm_source_text_use(key, source_text, reason, keep_existing=False, auto_pass=False):
    """Ask before writing source text when translation is missing or unchanged.

    The automatic pass offers [a]lways, which settles that key for the rest of the run, and treats [n]o as "stop": a
    translation that keeps failing needs the user, and quietly skipping the key in every remaining locale is the one
    outcome nobody wants.  The interactive caller keeps the plain yes/no it always had - there [n]o means "not this
    key", and answering yes hands the key to the normal edit prompt anyway, so stopping would be wrong.
    """
    if auto_pass and key in _source_text_always:
        return True
    print(f"\n⚠ {reason}: {key}")
    print(f"[Source] {source_text}")
    if auto_pass:
        print("Use source text anyway? [y]es / [a]lways for this key / [n]o - stop and check: ", end='', flush=True)
    else:
        suffix = "(n keeps JSON)" if keep_existing else "(n skips key)"
        print(f"Use source text anyway? [y/n] {suffix}: ", end='', flush=True)
    answer = input().strip().lower()
    if auto_pass and answer == 'a':
        _source_text_always.add(key)
        print(f"  → always: the source text is used for {key} for the rest of this run")
        return True
    if answer == 'y':
        return True
    if auto_pass:
        print(f"\nStopped: no usable translation for {key}.")
        print("  Two things worth checking before running the command again:")
        print("    - the key's source text: a string of symbols or punctuation often comes back unchanged, and may")
        print("      simply need translating by hand;")
        print("    - the translation service: it may be down or rate-limited, in which case waiting a little helps.")
        print("  Locales finished before this point are saved; the one in progress is not written.")
        sys.exit(1)
    return False


def prompt_for_key(key, found_text, json_text=None, filename=None, mode='missing', locale_code=None, use_translate=False):
    """Prompt user for translation text."""
    print()  # Blank line before prompt
    
    # Try to get translation if flag is set
    translated_text = None
    if use_translate and locale_code and locale_code != 'en_US':
        translated_text = translate_text(found_text, target_locale=locale_code)
        failed_or_same = (not translated_text) or translated_text == found_text
        if failed_or_same:
            reason = "Translation failed" if not translated_text else "Translation returned unchanged source text"
            keep_existing = mode != 'missing'
            if not confirm_source_text_use(key, found_text, reason, keep_existing=keep_existing):
                return json_text if keep_existing else None
            translated_text = None
    
    if mode == 'missing':
        print(f"[{filename}] {key}")
        print(f"[Found] {found_text}")
        
        # Show translation if available
        if translated_text:
            print(f"[Translation] {translated_text}")
            default_text = translated_text
            prompt_msg = "Enter new text (ENTER accepts Translation / type to edit / ESC skip): "
        else:
            default_text = found_text
            prompt_msg = "Enter new text (ENTER accepts Found / type to edit / ESC skip): "
        
        print(prompt_msg, end='', flush=True)
        
        user_input = ""
        while True:
            if WINDOWS:
                ch = msvcrt.getch()
                if ch == b'\r':  # Enter
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':  # ESC
                    print("[ESC - skipping this key]")
                    return None  # Skip this key
                elif ch == b'\x08':  # Backspace
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):  # Ctrl+C or Ctrl+D
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:  # Unix/Linux
                ch = get_key()
                if ch == '\r' or ch == '\n':  # Enter
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':  # ESC
                    print("[ESC - skipping this key]")
                    return None  # Skip this key
                elif ch == '\x7f':  # Backspace
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):  # Ctrl+C or Ctrl+D
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)
    
    else:  # 'all', 'diff', or 'ndiff' mode
        print(f"[{filename}] {key}")
        print(f"[Found] {found_text}")
        
        # Show translation if available
        if translated_text:
            print(f"[Translation] {translated_text}")
        
        if json_text is not None:
            print(f"[JSON] {json_text}")
        
        # Determine default text priority: Translation > Found
        if translated_text:
            default_text = translated_text
            prompt_msg = "Enter new text (ENTER accepts Translation / type to edit / ESC keeps JSON): "
        else:
            default_text = found_text
            prompt_msg = "Enter new text (ENTER accepts Found / type to edit / ESC keeps JSON): "
        
        print(prompt_msg, end='', flush=True)
        
        user_input = ""
        while True:
            if WINDOWS:
                ch = msvcrt.getch()
                if ch == b'\r':  # Enter
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':  # ESC
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == b'\x08':  # Backspace
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):  # Ctrl+C or Ctrl+D
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:  # Unix/Linux
                ch = get_key()
                if ch == '\r' or ch == '\n':  # Enter
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':  # ESC
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == '\x7f':  # Backspace
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):  # Ctrl+C or Ctrl+D
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)


def get_sort_priority(key):
    """
    Return a tuple for sorting priority:
    - First element: category priority (lower = earlier)
    - Second element: the key itself for alphabetical sorting within category
    """
    # Special handling for exact metadata keys (always at top)
    if key in ('locale_code', 'locale', 'locale_en'):
        metadata_order = {'locale_code': 0, 'locale': 1, 'locale_en': 2}
        return (-1, metadata_order[key])  # -1 ensures these come before all prefixes
    
    prefixes = [
        'locale_',   # 0
        'ttl_',      # 1
        'lbl_',      # 2
        'btn_',      # 3
        'msg_',      # 4
        'unit_',     # 5
        'z_',        # 6 — netserver emptyfs keys, always at bottom
    ]
    
    for i, prefix in enumerate(prefixes):
        if key.startswith(prefix):
            return (i, key)
    
    # Default: alphabetical at the end
    return (999, key)


def sort_json_data(data):
    """Sort JSON data dict by hierarchical key ordering."""
    sorted_keys = sorted(data.keys(), key=get_sort_priority)
    return {key: data[key] for key in sorted_keys}


def load_json_safe(path):
    """Load JSON, attempting to repair a trailing comma error before giving up."""
    try:
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)
    except json.JSONDecodeError as e:
        if 'trailing comma' in str(e).lower() or 'illegal trailing comma' in str(e).lower():
            print(f"⚠ Found trailing comma error in JSON file - attempting to fix...")
            with open(path, 'r', encoding='utf-8') as f:
                json_text = f.read()
            fixed_json = re.sub(r',(\s*[}\]])', r'\1', json_text)
            try:
                data = json.loads(fixed_json)
                with open(path, 'w', encoding='utf-8') as f:
                    json.dump(data, f, ensure_ascii=False, indent=2)
                print(f"✓ Automatically fixed and saved {os.path.basename(path)}")
                return data
            except json.JSONDecodeError as e2:
                print(f"\nError: Could not parse JSON file even after fixing trailing commas")
                print(f"  {e2}")
                return None
        print(f"\nError: Invalid JSON in {path}")
        print(f"  {e}")
        return None


DEFAULT_NEWKEYS_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'www_newkeys.json')


def newkeys_www(locale_paths, www_path, out_path, auto_clean, auto_sort):
    """
    Write the keys that are in the source but not yet in one or more locale files.

    The result is a template to hand out: key -> current source text, so a translator can see what they are
    translating, and send the file back for --merge.  Nothing here touches a locale file - a missing key stays
    missing until someone merges a filled-in template.  With several locales the missing keys are unioned and the
    template is written once, because a key absent from one locale is almost always absent from the rest.

    An existing template is never rebuilt: keys it already has keep their values, which may be a translator's work
    in progress, and only the keys it lacks are added.  It lives beside the tools rather than in the locale folders,
    which both generators glob as locale files.
    """
    print(f"\n{'='*60}")
    print(f"Collecting new keys into {os.path.basename(out_path)}")
    print(f"{'='*60}")

    print(f"Reading master keys from {www_path}...")
    found_keys = scan_www_folder(www_path)
    print(f"Found {len(found_keys)} master keys in HTML/JS files")

    missing = {}   # key -> source text, unioned over every locale given
    for code, json_path in locale_paths:
        if not os.path.exists(json_path):
            print(f"  {code}: file not found, skipped")
            continue
        data = load_json_safe(json_path)
        if data is None:
            return False
        gone = [k for k in found_keys if k not in data]
        print(f"  {code}: {len(gone)} of {len(found_keys)} key(s) missing")
        for key in gone:
            missing.setdefault(key, found_keys[key]['text'])

    print(f"\nUnion across {len(locale_paths)} locale(s): {len(missing)} key(s)")

    template = {}
    if os.path.exists(out_path):
        template = load_json_safe(out_path)
        if template is None:
            return False
        if not isinstance(template, dict):
            print(f"Error: {out_path} does not contain a JSON object")
            return False
        print(f"Existing template holds {len(template)} key(s); their values are kept as they are")

    added = 0
    for key in sorted(missing):
        if key not in template:
            template[key] = missing[key]
            added += 1
    print(f"✓ Template: {added} added, {len(missing) - added} already present")

    # The only thing --clean can mean here: a template key the source no longer knows, left over from an earlier run.
    if auto_clean:
        dropped = [k for k in sorted(template) if k not in found_keys]
        for key in dropped:
            del template[key]
        print(f"✓ Auto-deleted {len(dropped)} retired key(s)" if dropped else "✓ Nothing to clean")

    if auto_sort:
        template = sort_json_data(template)
        print("✓ Auto-sorted keys hierarchically")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    temp_path = out_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(template, f, ensure_ascii=False, indent=2)
    os.replace(temp_path, out_path)
    print(f"\n✓ Saved {out_path} ({len(template)} key(s))")
    print("  Fill it in, send it back, then apply it with:  --merge <file>")
    return True


def extract_keys(json_path, template_path, out_path, label):
    """
    Print or append this locale's values for the keys the template lists, as a sheet for a native-speaker check.

    The template is what --newkeys wrote: the keys that were recently added, still holding their SOURCE text.  This
    mode answers the next question - what does this locale currently say for them - so a translator, or a forum, can
    check the wording in place.  Keys the locale does not have yet are SKIPPED: there is no value to check, and an
    empty string would read as a translation of nothing.  The count is reported instead, so a short sheet is never
    mistaken for the whole template.

    The output is a SHEET rather than a file the tools will read back - a label naming the tool and the locale, then a
    blank line, then the JSON block: the shape that goes into a forum post.  It is deliberately never read, parsed or
    rewritten, and with a filename it is only ever APPENDED to, so a www check and a display check can end up in one
    document.  The block uses the locale files' own formatting (real characters, two-space indent) so it can be edited
    in place and then clipped by hand into an input file for --merge.
    """
    if not os.path.exists(template_path):
        print(f"Error: template not found: {template_path}")
        print("       --newkeys writes it, and it holds the keys this mode reads - run that first.")
        return False
    if not os.path.exists(json_path):
        print(f"Error: locale file not found: {json_path}")
        return False

    template = load_json_safe(template_path)
    if template is None:
        return False
    if not isinstance(template, dict):
        print(f"Error: {template_path} does not contain a JSON object")
        return False
    data = load_json_safe(json_path)
    if data is None:
        return False

    # Template order, so the sheet reads in the order the keys were added rather than the locale's own order.
    picked = {key: data[key] for key in template if key in data}
    skipped = [key for key in template if key not in data]
    block = label + "\n\n" + json.dumps(picked, ensure_ascii=False, indent=2) + "\n"

    if out_path is None:
        print(block)
    else:
        # APPEND ONLY.  The file may already hold a display or a www check - or a translator's edits - so it is never
        # opened for reading, and a block that follows another starts after a blank line.
        empty = (not os.path.exists(out_path)) or os.path.getsize(out_path) == 0
        with open(out_path, 'a', encoding='utf-8') as f:
            if not empty:
                f.write("\n")
            f.write(block)
        print(f"Appended to {out_path} ({len(picked)} key(s))")

    if skipped:
        print(f"Note: {len(picked)} of {len(template)} template key(s) are in this locale; "
              f"skipped {', '.join(skipped)}")
    return True


def resolve_merge_path(given, locale_dir):
    """Find the merge file as given, then inside the locale folder. Returns None when it is nowhere."""
    if os.path.exists(given):
        return os.path.abspath(given)
    candidate = os.path.join(locale_dir, given)
    if os.path.exists(candidate):
        return os.path.abspath(candidate)
    print(f"Error: merge file not found: {given}")
    print(f"       Also looked in {locale_dir}")
    return None


def merge_partial_file(locale_code, www_path, json_path, merge_path, auto_clean, auto_sort):
    """
    Merge a partial locale JSON into one locale file.

    The partial is upserted: the keys it carries are updated, the keys the target lacks are added, and equal values
    are left alone.  A key the master does not know is skipped and named, not written: a translator working from an
    older copy of the file will hand back keys that have since been retired, and that is no reason to throw away the
    rest of their work - while writing such a key is exactly what has to be avoided, since a key nothing else uses
    would sit here dead and the generators reject extra keys.  Unlike the normal pass this never prompts, so a
    contributor's file can be merged unattended - clean and sort happen only when they are asked for.
    """
    print(f"\n{'='*60}")
    print(f"Merging into: {locale_code}.json")
    print(f"{'='*60}")

    if not os.path.exists(json_path):
        print(f"Error: JSON file not found at {json_path}")
        print("       A merge fills a locale that is already there: copy the master, or another locale, to this")
        print("       name and translate it first, or name a locale that exists.")
        return False

    partial = load_json_safe(merge_path)
    if partial is None:
        return False
    if not isinstance(partial, dict):
        print(f"Error: {merge_path} does not contain a JSON object")
        return False

    locale_data = load_json_safe(json_path)
    if locale_data is None:
        return False

    print(f"Reading master keys from {www_path}...")
    found_keys = scan_www_folder(www_path)
    print(f"Found {len(found_keys)} master keys in HTML/JS files")
    print(f"Loaded {len(locale_data)} keys from {locale_code}.json")
    print(f"Loaded {len(partial)} keys from {os.path.basename(merge_path)}")

    # locale_code follows the filename, which make_dsplocale.py validates, so a partial may not overwrite it.
    if 'locale_code' in partial and partial['locale_code'] != locale_code:
        print(f"  Ignoring locale_code '{partial['locale_code']}' from the merge file: the filename is the authority")

    unknown = sorted(k for k in partial
                     if k not in found_keys and k not in ('locale_code', 'locale', 'locale_en'))
    if unknown:
        print(f"\n{'='*60}")
        print(f"Ignored: {len(unknown)} key(s) in {os.path.basename(merge_path)} are not in the master key set")
        print(f"{'='*60}")
        for key in unknown:
            print(f"  {key}")
        print("\nEither the key name is wrong, the key is old, or is new and belongs in the source and en_US.json first.")
        print("Everything else in the file is merged as usual.")
        for key in unknown:
            del partial[key]

    added = updated = unchanged = 0
    for key, value in partial.items():
        if key == 'locale_code':
            continue
        if key not in locale_data:
            added += 1
        elif locale_data[key] != value:
            updated += 1
        else:
            unchanged += 1
        locale_data[key] = value

    print(f"\n✓ Merged {added + updated + unchanged} key(s): {added} added, {updated} updated, {unchanged} unchanged")

    # What the file still owes, which is the part that decides whether the page reads in this language at all.
    missing = [k for k in found_keys if k not in locale_data]
    empty = [k for k in sorted(locale_data)
             if k not in ('locale_code', 'locale', 'locale_en')
             and isinstance(locale_data[k], str) and not locale_data[k].strip()]
    for label, keys in (("not in this file", missing), ("present but empty", empty)):
        if keys:
            shown = ', '.join(keys[:20]) + (" ..." if len(keys) > 20 else "")
            print(f"⚠ {len(keys)} master key(s) {label}: {shown}")

    # Clean and sort only when asked - a merge never prompts, see the docstring.
    if auto_clean:
        dropped = [k for k in sorted(locale_data)
                   if k not in found_keys and k not in ('locale_code', 'locale', 'locale_en')]
        for key in dropped:
            del locale_data[key]
        print(f"✓ Auto-deleted {len(dropped)} unused key(s)" if dropped else "✓ Nothing to clean")

    if auto_sort:
        locale_data = sort_json_data(locale_data)
        print("✓ Auto-sorted keys hierarchically")

    temp_path = json_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(locale_data, f, ensure_ascii=False, indent=2)
    os.replace(temp_path, json_path)
    print(f"\n✓ Saved {json_path}")

    return True


def process_locale_file(locale_code, www_path, json_path, mode, auto_clean, auto_sort, use_translate=False, only_key=None):
    """Process a single locale file."""
    print(f"\n{'='*60}")
    print(f"Processing: {locale_code}.json")
    print(f"{'='*60}")
    
    if not os.path.exists(json_path):
        print(f"Error: JSON file not found at {json_path}")
        return False
    
    # Load JSON with automatic trailing comma fix
    try:
        with open(json_path, 'r', encoding='utf-8') as f:
            locale_data = json.load(f)
    except json.JSONDecodeError as e:
        # Check if it's a trailing comma error
        if 'trailing comma' in str(e).lower() or 'illegal trailing comma' in str(e).lower():
            print(f"⚠ Found trailing comma error in JSON file - attempting to fix...")
            
            # Read the file and remove trailing commas
            with open(json_path, 'r', encoding='utf-8') as f:
                json_text = f.read()
            
            # Remove trailing commas before closing braces/brackets
            # Pattern: comma followed by optional whitespace and then } or ]
            fixed_json = re.sub(r',(\s*[}\]])', r'\1', json_text)
            
            # Try to parse the fixed JSON
            try:
                locale_data = json.loads(fixed_json)
                
                # Save the fixed JSON back to file
                with open(json_path, 'w', encoding='utf-8') as f:
                    json.dump(locale_data, f, ensure_ascii=False, indent=2)
                
                print(f"✓ Automatically fixed and saved {locale_code}.json")
                
            except json.JSONDecodeError as e2:
                print(f"\nError: Could not parse JSON file even after fixing trailing commas")
                print(f"  {e2}")
                print(f"  Please manually fix {json_path}")
                return False
        else:
            # Different JSON error - show helpful message
            print(f"\nError: Invalid JSON in {json_path}")
            print(f"  {e}")
            print(f"  Please fix the JSON syntax errors manually")
            return False
    
    # Scan www files
    print(f"Scanning {www_path} for translation keys...")
    found_keys = scan_www_folder(www_path)
    print(f"Found {len(found_keys)} unique keys in HTML/JS files")
    print(f"Loaded {len(locale_data)} keys from {locale_code}.json")
    
    # Count excluded locale* keys
    excluded_keys = [k for k in locale_data.keys() if k in ('locale_code', 'locale', 'locale_en')]
    if excluded_keys:
        print(f"Ignoring {len(excluded_keys)} locale* metadata key(s)")
    
    # Extract locale info for headers
    locale_native = locale_data.get('locale', '')
    locale_english = locale_data.get('locale_en', '')
    locale_display = f" ({locale_native} / {locale_english})" if locale_native and locale_english else ""
    
    # Show missing keys summary if in missing or fast mode
    if mode in ('missing', 'fast'):
        if only_key is not None:
            print("\n" + "="*60)
            print(f"Redoing one key in {locale_code}{locale_display}:")
            print("="*60)
            if only_key in found_keys:
                print(f"  {only_key} = {found_keys[only_key]['text']}")
            else:
                print(f"  {only_key} is not used by the sources - nothing to do")
            print("=" * 60)
        else:
            missing_keys = [(key, data) for key, data in found_keys.items() if locale_data.get(key) is None]
            if missing_keys:
                print("\n" + "="*60)
                print(f"Keys in HTML/JS files not found in JSON{locale_display}:")
                print("="*60)
                for key, data in missing_keys:
                    print(f"  {key} = {data['text']}")
                print(f"\nTotal: {len(missing_keys)} missing key(s)")
                print("=" * 60)
    
    # Process keys
    updates = {}
    processed_count = 0
    
    if mode == 'fast':
        # Fast mode: add all missing keys at once.  With --key it is the one target instead, and it is written
        # whether or not the locale already has it - that is what redoing a key means.  Translation and the
        # unchanged-text prompt behave exactly as they do for a key that was missing.
        if only_key is not None:
            missing_keys = [(only_key, found_keys[only_key])] if only_key in found_keys else []
        else:
            missing_keys = [(key, data) for key, data in found_keys.items() if locale_data.get(key) is None]
        if missing_keys:
            # Prepare translations/text FIRST (show progress)
            pending_updates = {}
            
            if use_translate and locale_code != 'en_US':
                # Auto-translate all missing keys and show progress
                print(f"\nAuto-translating {len(missing_keys)} missing keys...")
                print("  (✓ = translated, → = source text used, n stops the run)\n")
                
                for key, data in missing_keys:
                    found_text = data['text']
                    # Try translation first
                    translated_text = translate_text(found_text, target_locale=locale_code)
                    
                    # Use translation if available, otherwise fallback to Found text
                    if translated_text and translated_text != found_text:
                        pending_updates[key] = translated_text
                        print(f"  ✓ {key}: {translated_text}")
                    else:
                        reason = "Translation failed" if not translated_text else "Translation returned unchanged source text"
                        # This can only return yes or end the run, so the key is always recorded afterwards.
                        confirm_source_text_use(key, found_text, reason, auto_pass=True)
                        pending_updates[key] = found_text
                        print(f"  → {key}: {found_text}")
            else:
                # No translation: just prepare hardcoded text
                for key, data in missing_keys:
                    pending_updates[key] = data['text']

            updates.update(pending_updates)
            processed_count = len(pending_updates)
            # A --key run replaces a value that was already there, so "Added" would read wrong for it.
            if only_key is not None:
                print(f"\n✓ Wrote {processed_count} key(s) to JSON")
            else:
                print(f"\n✓ Added {processed_count} key(s) to JSON")
    
    elif mode in ('missing', 'every', 'diff', 'ndiff'):
        # --key narrows this loop to one entry, and to nothing at all if the sources have dropped the key.
        if only_key is not None:
            keys_to_process = [(only_key, found_keys[only_key])] if only_key in found_keys else []
        else:
            keys_to_process = list(found_keys.items())
        for key, data in keys_to_process:
            found_text = data['text']
            json_text = locale_data.get(key)
            filename = data['files'][0] if data['files'] else 'unknown'
            
            if mode == 'missing':
                # A key the locale already has is normally left alone.  With --key it is the whole point, so the
                # prompt happens anyway - through the 'all' layout when a value exists, which shows it as [JSON]
                # and lets ESC keep it, instead of the source text replacing it without being seen.
                if json_text is not None and only_key is None:
                    continue
                prompt_mode = 'all' if (only_key is not None and json_text is not None) else 'missing'
                new_text = prompt_for_key(key, found_text, json_text, filename, mode=prompt_mode, locale_code=locale_code, use_translate=use_translate)
                if new_text is not None and new_text != json_text:
                    updates[key] = new_text
                    processed_count += 1
            
            elif mode == 'every':
                new_text = prompt_for_key(key, found_text, json_text, filename, mode='all', locale_code=locale_code, use_translate=use_translate)
                if new_text != json_text:
                    updates[key] = new_text
                    processed_count += 1
            
            elif mode == 'diff':
                if json_text is None or (found_text and json_text != found_text):
                    # Note: diff mode never uses translation per user request
                    new_text = prompt_for_key(key, found_text, json_text, filename, mode='diff', locale_code=locale_code, use_translate=False)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1
            
            elif mode == 'ndiff':
                # Only prompt when texts are the same (to fix untranslated text)
                if json_text is not None and found_text and json_text == found_text:
                    new_text = prompt_for_key(key, found_text, json_text, filename, mode='ndiff', locale_code=locale_code, use_translate=use_translate)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1
    
    # Update JSON if changes were made
    if updates:
        print(f"\n{len(updates)} key(s) updated")
        locale_data.update(updates)
    else:
        print("\nNo changes made")
    
    # Handle unused keys
    unused = []
    for key in sorted(locale_data.keys()):
        if key not in found_keys and key not in ('locale_code', 'locale', 'locale_en'):
            unused.append(key)
    
    # Show unused keys
    print("\n" + "="*60)
    print(f"Keys in JSON not found in HTML/JS files{locale_display}:")
    print("="*60)
    
    if unused:
        for key in unused:
            print(f"  {key} = {locale_data[key]}")
        print(f"\nTotal: {len(unused)} unused key(s)")
        
        # Delete unused keys
        if auto_clean:
            for key in unused:
                del locale_data[key]
            print(f"✓ Auto-deleted {len(unused)} unused key(s)")
        else:
            print("\nWould you like to delete these keys from the JSON? [y/n]: ", end='', flush=True)
            response = input().strip().lower()
            if response == 'y':
                for key in unused:
                    del locale_data[key]
                print(f"✓ Deleted {len(unused)} unused key(s)")
            else:
                print("No keys deleted")
    else:
        print("  (none)")
    
    # Sort JSON
    if auto_sort:
        locale_data = sort_json_data(locale_data)
        print("\n✓ Auto-sorted keys hierarchically")
    elif (updates or unused) and mode != 'fast':  # Only ask if something changed
        print("\nWould you like to sort the keys in the JSON? [y/n]: ", end='', flush=True)
        response = input().strip().lower()
        if response == 'y':
            locale_data = sort_json_data(locale_data)
            print("✓ Sorted keys hierarchically")
    
    # Write updated JSON
    temp_path = json_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(locale_data, f, ensure_ascii=False, indent=2)
    
    # Replace original
    os.replace(temp_path, json_path)
    print(f"\n✓ Saved {json_path}")
    
    return True


def main():
    # Reconfigure stdout for the Windows cp949 terminal.  Every locale this tool handles is Cyrillic, Greek, Thai and
    # so on, and the console is not UTF-8 by default there, so printing any of it raises UnicodeEncodeError - the same
    # fault that once crashed a run on a mere "✓" in a progress line.
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

    # Show help if no arguments provided
    if len(sys.argv) == 1:
        print(__doc__)
        sys.exit(0)
    
    parser = argparse.ArgumentParser(
        description='Scan www files for translation keys and manage locale JSON files',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('locale', help='Locale code (e.g., en_US) or * to process all locales')
    parser.add_argument('--fast', '-f', action='store_true', help='Add all missing keys at once (one prompt)')
    parser.add_argument('--translate', '-t', action='store_true', help='Use translation service for missing/new keys')
    parser.add_argument('--every', '-e', action='store_true', help='Prompt to review every single key')
    parser.add_argument('--diff', '-d', action='store_true', help='Only prompt when text differs')
    parser.add_argument('--ndiff', '-n', action='store_true', help='Only prompt when text is same (to fix untranslated)')
    parser.add_argument('--clean', '-c', action='store_true', help='Auto-delete unused keys (no prompt)')
    parser.add_argument('--sort', '-s', action='store_true', help='Auto-sort keys hierarchically (no prompt)')
    parser.add_argument('--key', metavar='NAME', default=None, help='Work on one key only, in every locale: write it even where it exists, ignore every other key (refused with --merge/--newkeys)')
    parser.add_argument('--merge', '-m', metavar='FILE', default=None, help='Merge a partial locale JSON into one locale file (upsert, no prompts)')
    parser.add_argument('--newkeys', '-k', nargs='?', const=DEFAULT_NEWKEYS_PATH, default=None, metavar='FILE', help='Write the keys the locale(s) lack into a template file (default: www_newkeys.json), keyed to the source text')
    parser.add_argument('--extract', '-x', nargs='?', const='', default=None, metavar='FILE', help='Print this locale\'s values for the keys the --newkeys template lists, as a labelled sheet for a native-speaker check; with FILE, append the sheet to it instead (no other option applies in this mode)')
    args = parser.parse_args()
    
    # Validate argument combinations
    mode_count = sum([args.fast, args.every, args.diff, args.ndiff])
    if mode_count > 1:
        print("Error: Only one mode can be specified (--fast, --every, --diff, --ndiff)")
        sys.exit(1)

    if args.merge:
        # A partial file is written for one language, so the wildcard has nothing to mean here.
        if args.locale == '*':
            print("Error: --merge works on one locale at a time - name the locale, never *")
            sys.exit(1)
        if mode_count:
            print("Error: --merge cannot be combined with --fast, --every, --diff, or --ndiff")
            sys.exit(1)
        if args.translate:
            print("Error: --merge cannot be combined with --translate - the values are already written")
            sys.exit(1)
        if args.key:
            print("Error: --merge writes a whole partial file, so --key has nothing to select")
            sys.exit(1)

    if args.newkeys is not None:
        # The wildcard is the point here: one pass, the union of what every locale lacks, one file written.
        # It composes with the pass below on purpose - the collection runs first, while the keys are still missing,
        # and the pass then fills the locales, so one command yields both.
        if args.merge is not None:
            print("Error: --newkeys and --merge are opposite directions - collect, or apply, not both")
            sys.exit(1)
        if args.key is not None:
            print("Error: --newkeys collects the keys the locales lack, --key redoes one key they already have - use one or the other")
            sys.exit(1)
    
    if args.extract is not None:
        # A sheet is built from ONE locale file, so the wildcard has nothing to mean here; --merge would write the
        # very file being read, which is the opposite direction.  Everything else is inert BY DESIGN - this mode only
        # reads the template and the locale and writes a sheet - so any that were typed are named rather than
        # silently ignored, which is how a stray --translate would otherwise look like it had done something.
        if args.locale == '*':
            print("Error: --extract works on one locale at a time - name the locale, never *")
            sys.exit(1)
        if args.merge is not None:
            print("Error: --extract reads a locale to build a sheet, --merge writes one - use one or the other")
            sys.exit(1)
        inert = [name for name, given in (('--translate', args.translate), ('--clean', args.clean),
                                          ('--sort', args.sort), ('--fast', args.fast), ('--every', args.every),
                                          ('--diff', args.diff), ('--ndiff', args.ndiff),
                                          ('--newkeys', args.newkeys is not None),
                                          ('--key', args.key is not None)) if given]
        if inert:
            print(f"Note: {' '.join(inert)} ignored in --extract mode")

        # Resolved here rather than below, because this mode leaves before the interactive machinery is set up.
        script_dir = os.path.dirname(os.path.abspath(__file__))
        json_path = os.path.join(script_dir, 'www', f'{args.locale}.json')
        out_path = None
        if args.extract:
            # A bare name lands beside this script and the template, so a sheet is never a mystery path.
            out_path = args.extract if os.path.isabs(args.extract) else os.path.join(script_dir, args.extract)
            out_path = os.path.abspath(out_path)
        print()
        ok = extract_keys(json_path, DEFAULT_NEWKEYS_PATH, out_path, f"New www keys: {args.locale}")
        sys.exit(0 if ok else 1)

    if args.translate and args.diff:
        print("Error: --translate cannot be used with --diff mode")
        sys.exit(1)
    
    if args.locale == '*' and (args.every or args.diff or args.ndiff):
        print("Error: * (all locales) cannot be combined with --every, --diff, or --ndiff")
        sys.exit(1)
    
    # Determine mode
    if args.fast:
        mode = 'fast'
    elif args.every:
        mode = 'every'
    elif args.diff:
        mode = 'diff'
    elif args.ndiff:
        mode = 'ndiff'
    else:
        mode = 'missing'

    # A collect-only run (--newkeys with no mode) writes the template and stops.  It must not fall through into
    # the interactive pass, which would prompt; with a mode given, the same run collects first and then continues.
    collect_only = args.newkeys is not None and mode == 'missing'
    
    # Blank line for readability
    print()
    
    # Check translation service availability BEFORE any file work.  A merge never prompts and never translates, so
    # it skips the detection outright - otherwise mode 'missing' would stop and ask about the service before the
    # merge even starts.
    # Skipped for a collect-only run so it cannot stop and ask about the service; if --translate was given anyway,
    # detection still runs so the answer is honest rather than a misleading "unavailable".
    service = None if (args.merge or (collect_only and not args.translate)) else detect_translation_service()
    
    if service:
        if not args.translate:
            # Service available but user didn't specify --translate flag
            # Only ask interactively in default/missing mode
            if mode == 'missing':
                print(f"{'='*60}")
                print(f"✓ Translation service available: {service.upper()}")
                print(f"{'='*60}")
                print("Use translation service for missing/new keys? [y/n]: ", end='', flush=True)
                response = input().strip().lower()
                if response == 'y':
                    args.translate = True
                    print("✓ Translation enabled for this session\n")
                else:
                    print("→ Translation disabled, will use hardcoded text\n")
            # else: in fast/every/diff mode without --translate flag, just proceed without translation
        else:
            # User explicitly requested translation
            print(f"{'='*60}")
            print(f"✓ Translation service: {service.upper()}")
            print(f"{'='*60}\n")
    else:
        # No translation service available
        if args.translate:
            # User requested translation but it's unavailable
            print(f"{'='*60}")
            print("⚠ Error: --translate flag is set but translation is unavailable!")
            print("⚠ No translation service (add API key to trans_<service>.key)")
            print(f"{'='*60}")
            sys.exit(1)
        elif mode == 'missing' and not args.merge and not collect_only:
            # Interactive mode without translation service - show helpful warning
            print(f"{'='*60}")
            print("⚠ Warning: No translation service found.")
            print("⚠ This tool is more powerful with a translation service!")
            print(f"{'='*60}\n")
        # else: no service in fast/every/diff mode, just continue silently
    
    # Paths
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(script_dir, '..', '..'))
    www_path = os.path.join(project_root, 'data', 'www')
    
    if not os.path.exists(www_path):
        print(f"Error: www folder not found at {www_path}")
        sys.exit(1)

    # Check the targeted key against the sources before touching any locale, so a typo stops the run once here
    # instead of printing the same complaint for every locale file.  The HTML/JS sources are this tool's master,
    # so that is where the key has to exist.
    if args.key is not None:
        if args.key not in scan_www_folder(www_path):
            print(f"Error: key '{args.key}' does not appear in any HTML/JS file under {www_path}")
            print("       The sources are the master for this tool, so a key nothing uses cannot be redone.")
            sys.exit(1)
        print(f"Single key: {args.key}\n")
    
    # --newkeys: collect first, so the template holds the keys that were missing when this run started - a mode
    # given below then fills the locales in the same run.  A collect-only run stops here.
    if args.newkeys is not None:
        www_locale_dir = os.path.join(script_dir, 'www')
        if args.locale == '*':
            locale_paths = [(os.path.splitext(os.path.basename(p))[0], p)
                            for p in sorted(glob.glob(os.path.join(www_locale_dir, '*.json')))]
        else:
            locale_paths = [(args.locale, os.path.join(www_locale_dir, f'{args.locale}.json'))]
        if not newkeys_www(locale_paths, www_path, args.newkeys, args.clean, args.sort):
            sys.exit(1)
        if collect_only:
            return

    # Process file(s)
    if args.locale == '*':
        # Process all locale files
        www_locale_path = os.path.join(script_dir, 'www')
        json_files = glob.glob(os.path.join(www_locale_path, '*.json'))
        
        if not json_files:
            print(f"Error: No JSON files found in {www_locale_path}")
            sys.exit(1)
        
        print(f"Found {len(json_files)} locale file(s) to process")
        
        success_count = 0
        for json_path in sorted(json_files):
            locale_code = os.path.splitext(os.path.basename(json_path))[0]
            if process_locale_file(locale_code, www_path, json_path, mode, args.clean, args.sort, args.translate, args.key):
                success_count += 1
        
        print(f"\n{'='*60}")
        print(f"Processed {success_count}/{len(json_files)} locale file(s) successfully")
        print(f"{'='*60}")
    
    else:
        # Process single locale file
        json_path = os.path.join(script_dir, 'www', f'{args.locale}.json')

        if args.merge:
            merge_path = resolve_merge_path(args.merge, os.path.join(script_dir, 'www'))
            if merge_path is None:
                sys.exit(1)
            if not merge_partial_file(args.locale, www_path, json_path, merge_path, args.clean, args.sort):
                sys.exit(1)
            return

        process_locale_file(args.locale, www_path, json_path, mode, args.clean, args.sort, args.translate, args.key)


if __name__ == '__main__':
    main()
