#!/usr/bin/env python3
"""
Scan data/www/*.html and *.js files and src/core/netserver.h for translation keys and compare with locale JSON.

NOTE:
    Check .md files for how to install full translation support

USAGE:
    python www_tool.py <locale> [mode] [options]
    python www_tool.py * [mode] [options]

TARGET:
    <locale>         One locale → .json file in the www folder (en_US → en_US.json)
    *                All locales → all .json files

MODES:
    (default)        Interactive mode - prompts for missing keys, ask about cleanup, ask about sort
    --fast, -f       Add all missing keys at once using HTML Found text, skip individual edits (no prompt unless translation fails)
    --every, -e      Prompt to review every single key using HTML Found text (detailed proofreading)
    --diff, -d       Only prompt when HTML text differs from JSON (to compare hardcoded)
    --ndiff, -n      Only prompt when HTML text is same as JSON (to fix untranslated text)

OPTIONS:
    --translate, -t  Translate HTML Found text (can't use with --diff)
    --clean, -c      Auto-delete unused keys (no prompt)
    --sort, -s       Auto-sort keys hierarchically at end (no prompt)
    --create         Auto-create missing locale JSON from en_US.json with empty values

EXAMPLES:
    # Interactive check of one file
    py www_tool.py fr_FR

    # Interactive check of each key with translation (cleaned & sorted file)
    py www_tool.py * --translate --clean --sort

    # Fast mode WITH translation (auto-translate all missing keys in all files)
    py www_tool.py * --translate --fast --clean --sort

    # Diff mode (never uses translation, useful for checking that hard-coded text and locale file are same)
    py www_tool.py en_US --diff

    # Ndiff mode (prompt only when text matches - to find/fix untranslated text with translation)
    py www_tool.py de_DE --ndiff --translate --clean --sort
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


def confirm_source_text_use(key, source_text, reason, keep_existing=False):
    """Ask before writing source text when translation is missing/unchanged."""
    print(f"\n⚠ {reason}: {key}")
    print(f"[Source] {source_text}")
    suffix = "(n keeps JSON)" if keep_existing else "(n skips key)"
    print(f"Use source text anyway? [y/n] {suffix}: ", end='', flush=True)
    return input().strip().lower() == 'y'


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


def process_locale_file(locale_code, www_path, json_path, mode, auto_clean, auto_sort, use_translate=False, auto_create=False):
    """Process a single locale file."""
    print(f"\n{'='*60}")
    print(f"Processing: {locale_code}.json")
    print(f"{'='*60}")
    
    if not os.path.exists(json_path):
        if auto_create and locale_code != 'en_US':
            script_dir = os.path.dirname(os.path.abspath(__file__))
            master_path = os.path.join(script_dir, 'www', 'en_US.json')
            if os.path.exists(master_path):
                with open(master_path, 'r', encoding='utf-8') as f:
                    master_data = json.load(f)
                new_data = {}
                for key, value in master_data.items():
                    if key == 'locale_code':
                        new_data[key] = locale_code
                    elif key in ('locale', 'locale_en'):
                        new_data[key] = ''
                    elif key.startswith('ttl_') or key.startswith('lbl_') or key.startswith('btn_') or key.startswith('msg_') or key.startswith('unit_'):
                        new_data[key] = ''
                    else:
                        new_data[key] = value  # preserve locale_* metadata values (empty)
                # Ensure directory exists
                os.makedirs(os.path.dirname(json_path), exist_ok=True)
                with open(json_path, 'w', encoding='utf-8') as f:
                    json.dump(new_data, f, ensure_ascii=False, indent=2)
                print(f"✓ Created {locale_code}.json from en_US.json with empty values")
            else:
                print(f"Error: Master en_US.json not found at {master_path}")
                return False
        else:
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
        # Fast mode: add all missing keys at once
        missing_keys = [(key, data) for key, data in found_keys.items() if locale_data.get(key) is None]
        if missing_keys:
            # Prepare translations/text FIRST (show progress)
            pending_updates = {}
            
            if use_translate and locale_code != 'en_US':
                # Auto-translate all missing keys and show progress
                print(f"\nAuto-translating {len(missing_keys)} missing keys...")
                print("  (✓ = translated, → = source confirmed, - = source skipped)\n")
                
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
                        if confirm_source_text_use(key, found_text, reason):
                            pending_updates[key] = found_text
                            print(f"  → {key}: {found_text}")
                        else:
                            print(f"  - {key}: skipped")
            else:
                # No translation: just prepare hardcoded text
                for key, data in missing_keys:
                    pending_updates[key] = data['text']

            updates.update(pending_updates)
            processed_count = len(pending_updates)
            print(f"\n✓ Added {processed_count} key(s) to JSON")
    
    elif mode in ('missing', 'every', 'diff', 'ndiff'):
        for key, data in found_keys.items():
            found_text = data['text']
            json_text = locale_data.get(key)
            filename = data['files'][0] if data['files'] else 'unknown'
            
            if mode == 'missing':
                if json_text is not None:
                    continue
                new_text = prompt_for_key(key, found_text, json_text, filename, mode='missing', locale_code=locale_code, use_translate=use_translate)
                if new_text is not None:
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
    parser.add_argument('--create', action='store_true', help='Auto-create missing locale JSON from en_US.json with empty values')
    args = parser.parse_args()
    
    # Validate argument combinations
    mode_count = sum([args.fast, args.every, args.diff, args.ndiff])
    if mode_count > 1:
        print("Error: Only one mode can be specified (--fast, --every, --diff, --ndiff)")
        sys.exit(1)
    
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
    
    # Blank line for readability
    print()
    
    # Check translation service availability BEFORE any file work
    service = detect_translation_service()
    
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
        elif mode == 'missing':
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
            if process_locale_file(locale_code, www_path, json_path, mode, args.clean, args.sort, args.translate, args.create):
                success_count += 1
        
        print(f"\n{'='*60}")
        print(f"Processed {success_count}/{len(json_files)} locale file(s) successfully")
        print(f"{'='*60}")
    
    else:
        # Process single locale file
        json_path = os.path.join(script_dir, 'www', f'{args.locale}.json')
        process_locale_file(args.locale, www_path, json_path, mode, args.clean, args.sort, args.translate, args.create)


if __name__ == '__main__':
    main()
