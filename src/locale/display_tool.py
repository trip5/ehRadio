#!/usr/bin/env python3
"""
Manage display locale JSON files in src/locale/display/ — compare against master JSON.

NOTE:
    Check .md files for how to install full translation support

USAGE:
    python display_tool.py <target> [mode] [options]
    python display_tool.py <master> <target> [mode] [options]
    python display_tool.py * [mode] [options]
    python display_tool.py <target> --merge <file.json> [options]
    python display_tool.py <target|*> --newkeys [file.json] [options]
    python display_tool.py <target> --extract [file]

TARGET:
    <target>         One locale → check against en_US.json (default master)
    <master> <target> Explicit master + target pair
    *                All locales → all .json files EXCEPT the master

MODES:
    (default)        Interactive mode - prompts for missing keys, ask about cleanup, ask about sort
    --fast, -f       Add all missing keys at once using master text, skip individual edits
    --every, -e      Prompt to review every single key using master text (detailed proofreading)
    --diff, -d       Only prompt when target text differs from master (to find changed translations)
    --ndiff, -n      Only prompt when target text is same as master (to fix untranslated text)
    --merge, -m FILE Merge a partial locale JSON into ONE locale file
    --extract, -x [FILE]
                     Print the locale's values for the keys the --newkeys template lists, as a labelled sheet for a
                     native-speaker check.  With no FILE the sheet goes to the console; with a FILE it is APPENDED
                     to that file - which is never read back or rewritten, so a display check and a www check can
                     share one sheet.  Nothing else has a function in this mode.

OPTIONS:
    --translate, -t  Translate master text (can't use with --diff).  An unchanged or failed translation asks
                     [y]es / [a]lways for this key / [n]o - stop, in --fast; the interactive modes ask yes/no
    --clean, -c      Auto-delete extra keys not in master (no prompt; never touches master)
    --sort, -s       Auto-sort keys to match master's key order (no prompt)
    --newkeys, -k    Write the keys the master has and the target(s) lack into a template file, keyed to the
                     master text, for a translator to fill in and send back for --merge
    --key NAME       Work on one key only, across every locale: NAME key must be in master and is written
                     even where the locale already has it, and no other key is examined


EXAMPLES:
    # Interactive check of one file (en_US is default master)
    py display_tool.py fr_FR

    # Explicit master + target with translation
    py display_tool.py en_US fr_FR --translate

    # Fast mode WITH translation (auto-translate all missing keys in all files, put new keys in display_newkeys.json)
    py display_tool.py * --translate --fast --clean --sort --newkeys

    # Diff mode (find keys where target differs from master)
    py display_tool.py fr_FR --diff

    # Merge a contributor's partial file (only their keys), then tidy the file
    py display_tool.py ro_RO --merge changes.json --clean --sort

    # Collect every key the locales still lack into a template for the translators
    py display_tool.py * --newkeys --sort

    # Show what one locale currently says for the recently added keys, to paste into a forum post
    py display_tool.py ru_RU --extract

    # The same, appended to a sheet beside this script (a www check can share the same file)
    py display_tool.py ru_RU --extract RU_check.txt

    # Redo one key everywhere, after its text in the master changed
    py display_tool.py * --translate --fast --clean --sort --key msg_open
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

# Translation service configuration (shared with www_tool.py)
_translation_service = None
_translation_check_done = False
_translation_input_locale = "en_US"

_translation_lang_cache = {}
_translation_error_shown = False

META_KEYS = ('locale_code', 'locale', 'locale_en')  # tuple — order matters for sort_keys_by_master

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
DISPLAY_DIR = os.path.join(SCRIPT_DIR, 'display')
DEFAULT_MASTER = 'en_US'


def detect_translation_service():
    """
    Detect available translation service by scanning for trans_*.key files.
    Returns: service name (e.g., 'deepl', 'google') or None if none available
    """
    global _translation_service, _translation_check_done

    if _translation_check_done:
        return _translation_service

    _translation_check_done = True

    # Scan for any trans_*.key files
    key_files = glob.glob(os.path.join(SCRIPT_DIR, 'trans_*.key'))

    for key_file in sorted(key_files):
        basename = os.path.basename(key_file)
        service_name = basename[6:-4]  # Remove 'trans_' prefix and '.key' suffix

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

        script_file = os.path.join(SCRIPT_DIR, f'trans_{service_name}.py')
        if os.path.exists(script_file):
            _translation_service = service_name
            return _translation_service

    _translation_service = None
    return _translation_service


def translate_text(text, source_locale=None, target_locale=None):
    """
    Translate text using available translation service.
    Returns translated text or None if translation failed.
    """
    global _translation_lang_cache, _translation_input_locale, _translation_error_shown

    if source_locale is None:
        source_locale = _translation_input_locale

    service = detect_translation_service()
    if not service or not target_locale:
        return None

    if target_locale in _translation_lang_cache and not _translation_lang_cache[target_locale]:
        return None

    if service:
        script_path = os.path.join(SCRIPT_DIR, f'trans_{service}.py')

        try:
            result = subprocess.run(
                [sys.executable, script_path, source_locale, target_locale, text],
                capture_output=True,
                timeout=30,
                text=True,
                encoding='utf-8',
                errors='replace'
            )

            if result.returncode == 0:
                translated = result.stdout.strip()
                if translated:
                    _translation_lang_cache[target_locale] = True
                    return translated

            if result.stderr and not _translation_error_shown:
                error_msg = result.stderr.strip()
                if error_msg:
                    print(f"\n⚠ Translation error: {error_msg}")
                    print("  Source text requires confirmation per key.\n")
                    _translation_error_shown = True

            _translation_lang_cache[target_locale] = False
            return None

        except subprocess.TimeoutExpired:
            if not _translation_error_shown:
                print(f"\n⚠ Translation timeout (>30s) for {target_locale}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except (UnicodeDecodeError, UnicodeError) as e:
            if not _translation_error_shown:
                print(f"\n⚠ Translation encoding error for {target_locale}: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None
        except Exception as e:
            if not _translation_error_shown:
                print(f"\n⚠ Translation error: {e}")
                print("  Source text requires confirmation per key.\n")
                _translation_error_shown = True
            _translation_lang_cache[target_locale] = False
            return None

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
    print()

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
                if ch == b'\r':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':
                    print("[ESC - skipping this key]")
                    return None
                elif ch == b'\x08':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:
                ch = get_key()
                if ch == '\r' or ch == '\n':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':
                    print("[ESC - skipping this key]")
                    return None
                elif ch == '\x7f':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)

    else:  # 'all', 'diff', or 'ndiff' mode
        print(f"[{filename}] {key}")
        print(f"[Found] {found_text}")

        if translated_text:
            print(f"[Translation] {translated_text}")

        if json_text is not None:
            print(f"[JSON] {json_text}")

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
                if ch == b'\r':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == b'\x1b':
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == b'\x08':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in (b'\x03', b'\x04'):
                    print()
                    sys.exit(0)
                else:
                    try:
                        char = ch.decode('utf-8')
                        user_input += char
                        print(char, end='', flush=True)
                    except:
                        pass
            else:
                ch = get_key()
                if ch == '\r' or ch == '\n':
                    result = user_input if user_input else default_text
                    if user_input:
                        print()
                    else:
                        source = "Translation" if translated_text else "Found"
                        print(f"[ENTER - using {source} text: {result}]")
                    return result
                elif ch == '\x1b':
                    result = json_text if json_text is not None else default_text
                    print(f"[ESC - keeping JSON text: {result}]")
                    return result
                elif ch == '\x7f':
                    if user_input:
                        user_input = user_input[:-1]
                        print('\b \b', end='', flush=True)
                elif ch in ('\x03', '\x04'):
                    print()
                    sys.exit(0)
                else:
                    user_input += ch
                    print(ch, end='', flush=True)


def sort_keys_by_master(master_data, target_data):
    """
    Sort target JSON keys to match master's key order.
    New keys in target that don't exist in master are appended at the end (alphabetically).
    Metadata keys (locale_code, locale, locale_en) always come first.
    """
    master_keys = list(master_data.keys())

    # Start with meta keys in order
    result = {}
    for mk in META_KEYS:
        if mk in target_data:
            result[mk] = target_data[mk]

    # Follow master's order for translation keys
    for key in master_keys:
        if key in target_data and key not in META_KEYS:
            result[key] = target_data[key]

    # Append any keys in target that don't exist in master (alphabetically)
    extra_keys = sorted(k for k in target_data if k not in result and k not in META_KEYS)
    for key in extra_keys:
        result[key] = target_data[key]

    return result


def load_json_safe(path):
    """Load JSON with automatic trailing comma fix."""
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
        else:
            print(f"\nError: Invalid JSON in {path}")
            print(f"  {e}")
            return None


def get_translation_keys(data):
    """Return list of translation keys (excludes meta keys), preserving order."""
    return [k for k in data if k not in META_KEYS]


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


def merge_partial_file(locale_code, master_code, master_data, merge_path, auto_clean, auto_sort):
    """
    Merge a partial locale JSON into one display locale file.

    The partial is upserted: the keys it carries are updated, the keys the target lacks are added, and equal values
    are left alone.  Every key must exist in the master, because make_dsplocale.py treats a missing key and an extra
    key alike - as an error - so a mistyped key in a contributor's file has to be caught here rather than at build
    time.  Unlike the normal pass this never prompts: clean and sort happen only when they are asked for.
    """
    print(f"\n{'='*60}")
    print(f"Merging into: {locale_code}.json  (master: {master_code}.json)")
    print(f"{'='*60}")

    json_path = os.path.join(DISPLAY_DIR, f'{locale_code}.json')
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

    master_keys = get_translation_keys(master_data)
    print(f"Master {master_code}.json has {len(master_keys)} translation keys")
    print(f"Loaded {len(get_translation_keys(locale_data))} translation keys from {locale_code}.json")
    print(f"Loaded {len(partial)} keys from {os.path.basename(merge_path)}")

    # locale_code follows the filename, which make_dsplocale.py validates, so a partial may not overwrite it.
    if 'locale_code' in partial and partial['locale_code'] != locale_code:
        print(f"  Ignoring locale_code '{partial['locale_code']}' from the merge file: the filename is the authority")

    unknown = sorted(k for k in partial if k not in master_data and k not in META_KEYS)
    if unknown:
        print(f"\n{'='*60}")
        print(f"Refused: {len(unknown)} key(s) in {os.path.basename(merge_path)} are not in the master")
        print(f"{'='*60}")
        for key in unknown:
            print(f"  {key}")
        print("\nEither the key name is wrong, or the key is new and belongs in the master and its source first.")
        print("Nothing was written.")
        return False

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
    missing = [k for k in master_keys if k not in locale_data]
    empty = sorted(k for k in locale_data
                   if k not in META_KEYS and isinstance(locale_data[k], str) and not locale_data[k].strip())
    for label, keys in (("not in this file", missing), ("present but empty", empty)):
        if keys:
            shown = ', '.join(keys[:20]) + (" ..." if len(keys) > 20 else "")
            print(f"⚠ {len(keys)} master key(s) {label}: {shown}")

    # Clean and sort only when asked - a merge never prompts, see the docstring.
    if auto_clean:
        dropped = [k for k in sorted(locale_data) if k not in master_data and k not in META_KEYS]
        for key in dropped:
            del locale_data[key]
        print(f"✓ Auto-deleted {len(dropped)} extra key(s)" if dropped else "✓ Nothing to clean")

    if auto_sort:
        locale_data = sort_keys_by_master(master_data, locale_data)
        print("✓ Auto-sorted keys to match master order")

    temp_path = json_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(locale_data, f, ensure_ascii=False, indent=2)
    os.replace(temp_path, json_path)
    print(f"\n✓ Saved {json_path}")

    return True


DEFAULT_NEWKEYS_PATH = os.path.join(SCRIPT_DIR, 'display_newkeys.json')


def extract_keys(json_path, template_path, out_path, label):
    """
    Print or append this locale's values for the keys the template lists, as a sheet for a native-speaker check.

    The template is what --newkeys wrote: the keys that were recently added, still holding their MASTER text.  This
    mode answers the next question - what does this locale currently say for them - so a translator, or a forum, can
    check the wording in place.  Keys the locale does not have yet are SKIPPED: there is no value to check, and an
    empty string would read as a translation of nothing.  The count is reported instead, so a short sheet is never
    mistaken for the whole template.

    The output is a SHEET rather than a file the tools will read back - a label naming the tool and the locale, then a
    blank line, then the JSON block: the shape that goes into a forum post.  It is deliberately never read, parsed or
    rewritten, and with a filename it is only ever APPENDED to, so a display check and a www check can end up in one
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
        # APPEND ONLY.  The file may already hold a www or a display check - or a translator's edits - so it is never
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


def newkeys_display(master_code, master_data, target_codes, out_path, auto_clean, auto_sort):
    """
    Write the keys the master has and one or more targets lack.

    The result is a template to hand out: key -> master text, so a translator can see what they are translating, and
    send the file back for --merge.  Nothing here touches a locale file - a missing key stays missing until someone
    merges a filled-in template.  With several targets the missing keys are unioned and the template is written once,
    because a key absent from one locale is almost always absent from the rest.

    An existing template is never rebuilt: keys it already has keep their values, which may be a translator's work in
    progress, and only the keys it lacks are added.  It lives beside the tools rather than in the locale folder,
    which make_dsplocale.py globs and validates as locale files.
    """
    print(f"\n{'='*60}")
    print(f"Collecting new keys into {os.path.basename(out_path)}")
    print(f"{'='*60}")

    master_keys = get_translation_keys(master_data)
    print(f"Master {master_code}.json has {len(master_keys)} translation keys")

    missing = {}   # key -> master text, unioned over every target given
    for code in target_codes:
        json_path = os.path.join(DISPLAY_DIR, f'{code}.json')
        if not os.path.exists(json_path):
            print(f"  {code}: file not found, skipped")
            continue
        data = load_json_safe(json_path)
        if data is None:
            return False
        gone = [k for k in master_keys if k not in data]
        print(f"  {code}: {len(gone)} of {len(master_keys)} key(s) missing")
        for key in gone:
            missing.setdefault(key, master_data.get(key, ''))

    print(f"\nUnion across {len(target_codes)} locale(s): {len(missing)} key(s)")

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
    for key in master_keys:          # master order, so the file reads like the master even without --sort
        if key in missing and key not in template:
            template[key] = missing[key]
            added += 1
    print(f"✓ Template: {added} added, {len(missing) - added} already present")

    # The only thing --clean can mean here: a template key the master no longer knows, left over from an earlier run.
    if auto_clean:
        dropped = [k for k in sorted(template) if k not in master_data]
        for key in dropped:
            del template[key]
        print(f"✓ Auto-deleted {len(dropped)} retired key(s)" if dropped else "✓ Nothing to clean")

    if auto_sort:
        template = sort_keys_by_master(master_data, template)
        print("✓ Auto-sorted keys to match master order")

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    temp_path = out_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(template, f, ensure_ascii=False, indent=2)
    os.replace(temp_path, out_path)
    print(f"\n✓ Saved {out_path} ({len(template)} key(s))")
    print("  Fill it in, send it back, then apply it with:  --merge <file>")
    return True


def process_display_locale(locale_code, master_code, master_data, mode, auto_clean, auto_sort, use_translate=False, only_key=None):
    """Process a single display locale file against the master."""
    print(f"\n{'='*60}")
    print(f"Processing: {locale_code}.json  (master: {master_code}.json)")
    print(f"{'='*60}")

    json_path = os.path.join(DISPLAY_DIR, f'{locale_code}.json')

    if not os.path.exists(json_path):
        print(f"Error: JSON file not found at {json_path}")
        return False

    locale_data = load_json_safe(json_path)
    if locale_data is None:
        return False

    master_keys = get_translation_keys(master_data)
    target_keys = get_translation_keys(locale_data)

    print(f"Master {master_code}.json has {len(master_keys)} translation keys")
    print(f"Loaded {len(target_keys)} translation keys from {locale_code}.json")

    # Extract locale info for headers
    locale_native = locale_data.get('locale', '')
    locale_english = locale_data.get('locale_en', '')
    locale_display = f" ({locale_native} / {locale_english})" if locale_native and locale_english else ""

    # Show missing keys summary
    if mode in ('missing', 'fast'):
        if only_key is not None:
            print("\n" + "=" * 60)
            print(f"Redoing one key in {locale_code}{locale_display}:")
            print("=" * 60)
            print(f"  {only_key} = {master_data.get(only_key, '')}")
            print("=" * 60)
        else:
            missing_keys = [k for k in master_keys if locale_data.get(k) is None]
            if missing_keys:
                print("\n" + "=" * 60)
                print(f"Keys in master not found in {locale_code}{locale_display}:")
                print("=" * 60)
                for key in missing_keys:
                    print(f"  {key} = {master_data.get(key, '')}")
                print(f"\nTotal: {len(missing_keys)} missing key(s)")
                print("=" * 60)

    # Process keys
    updates = {}
    processed_count = 0

    if mode == 'fast':
        # With --key it is the one target instead, and it is written whether or not the locale already has it -
        # that is what redoing a key means.  Translation and the unchanged-text prompt behave exactly as they do
        # for a key that was missing.
        if only_key is not None:
            missing_keys = [(only_key, master_data.get(only_key, ''))]
        else:
            missing_keys = [(k, master_data.get(k, '')) for k in master_keys if locale_data.get(k) is None]
        if missing_keys:
            pending_updates = {}

            if use_translate and locale_code != 'en_US':
                print(f"\nAuto-translating {len(missing_keys)} missing keys...")
                print("  (✓ = translated, → = source text used, n stops the run)\n")

                for key, found_text in missing_keys:
                    translated_text = translate_text(found_text, target_locale=locale_code)

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
                for key, found_text in missing_keys:
                    pending_updates[key] = found_text

            updates.update(pending_updates)
            processed_count = len(pending_updates)
            # A --key run replaces a value that was already there, so "Added" would read wrong for it.
            if only_key is not None:
                print(f"\n✓ Wrote {processed_count} key(s) to JSON")
            else:
                print(f"\n✓ Added {processed_count} key(s) to JSON")

    elif mode in ('missing', 'every', 'diff', 'ndiff'):
        # --key narrows this loop to one entry.
        keys_to_process = [only_key] if only_key is not None else master_keys
        for key in keys_to_process:
            found_text = master_data.get(key, '')
            json_text = locale_data.get(key)

            if mode == 'missing':
                # A key the locale already has is normally left alone.  With --key it is the whole point, so the
                # prompt happens anyway - through the 'all' layout when a value exists, which shows it as [JSON]
                # and lets ESC keep it, instead of the master text replacing it without being seen.
                if json_text is not None and only_key is None:
                    continue
                prompt_mode = 'all' if (only_key is not None and json_text is not None) else 'missing'
                new_text = prompt_for_key(key, found_text, json_text, locale_code, mode=prompt_mode,
                                          locale_code=locale_code, use_translate=use_translate)
                if new_text is not None and new_text != json_text:
                    updates[key] = new_text
                    processed_count += 1

            elif mode == 'every':
                new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='all',
                                          locale_code=locale_code, use_translate=use_translate)
                if new_text != json_text:
                    updates[key] = new_text
                    processed_count += 1

            elif mode == 'diff':
                if json_text is None or (found_text and json_text != found_text):
                    new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='diff',
                                              locale_code=locale_code, use_translate=False)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1

            elif mode == 'ndiff':
                if json_text is not None and found_text and json_text == found_text:
                    new_text = prompt_for_key(key, found_text, json_text, locale_code, mode='ndiff',
                                              locale_code=locale_code, use_translate=use_translate)
                    if new_text != json_text:
                        updates[key] = new_text
                        processed_count += 1

    if updates:
        print(f"\n{len(updates)} key(s) updated")
        locale_data.update(updates)
    else:
        print("\nNo changes made")

    # Handle extra keys (keys in target not in master)
    extra = []
    for key in sorted(locale_data.keys()):
        if key not in master_data and key not in META_KEYS:
            extra.append(key)

    print("\n" + "=" * 60)
    print(f"Keys in {locale_code}.json not found in master {locale_display}:")
    print("=" * 60)

    if extra:
        for key in extra:
            print(f"  {key} = {locale_data[key]}")
        print(f"\nTotal: {len(extra)} extra key(s)")

        if auto_clean:
            for key in extra:
                del locale_data[key]
            print(f"✓ Auto-deleted {len(extra)} extra key(s)")
        else:
            print("\nWould you like to delete these keys from the JSON? [y/n]: ", end='', flush=True)
            response = input().strip().lower()
            if response == 'y':
                for key in extra:
                    del locale_data[key]
                print(f"✓ Deleted {len(extra)} extra key(s)")
            else:
                print("No keys deleted")
    else:
        print("  (none)")

    # Sort JSON (master key order)
    if auto_sort:
        locale_data = sort_keys_by_master(master_data, locale_data)
        print("\n✓ Auto-sorted keys to match master order")
    elif (updates or extra) and mode != 'fast':
        print("\nWould you like to sort keys to match master order? [y/n]: ", end='', flush=True)
        response = input().strip().lower()
        if response == 'y':
            locale_data = sort_keys_by_master(master_data, locale_data)
            print("✓ Sorted keys to match master order")

    # Write updated JSON
    temp_path = json_path + '.tmp'
    with open(temp_path, 'w', encoding='utf-8') as f:
        json.dump(locale_data, f, ensure_ascii=False, indent=2)

    os.replace(temp_path, json_path)
    print(f"\n✓ Saved {json_path}")

    return True


def main():
    # Reconfigure stdout for Windows cp949 terminal
    if sys.platform == 'win32':
        import io
        sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8', errors='replace')
        sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8', errors='replace')

    if len(sys.argv) == 1:
        print(__doc__)
        sys.exit(0)

    parser = argparse.ArgumentParser(
        description='Manage display locale JSON files against master JSON',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__
    )
    parser.add_argument('target', help='Locale code (e.g., fr_FR), or * for all non-master locales, or master code when two positional args')
    parser.add_argument('target2', nargs='?', default=None, help='Target locale code when first arg is the master')
    parser.add_argument('--fast', '-f', action='store_true', help='Add all missing keys at once (one prompt)')
    parser.add_argument('--translate', '-t', action='store_true', help='Use translation service for missing/new keys')
    parser.add_argument('--every', '-e', action='store_true', help='Prompt to review every single key')
    parser.add_argument('--diff', '-d', action='store_true', help='Only prompt when text differs')
    parser.add_argument('--ndiff', '-n', action='store_true', help='Only prompt when text is same (to fix untranslated)')
    parser.add_argument('--clean', '-c', action='store_true', help='Auto-delete extra keys not in master (no prompt; never touches master)')
    parser.add_argument('--sort', '-s', action='store_true', help='Auto-sort keys to match master order (no prompt)')
    parser.add_argument('--key', metavar='NAME', default=None, help='Work on one key only, in every locale: write it even where it exists, ignore every other key (refused with --merge/--newkeys)')
    parser.add_argument('--merge', '-m', metavar='FILE', default=None, help='Merge a partial locale JSON into one locale file (upsert, no prompts)')
    parser.add_argument('--newkeys', '-k', nargs='?', const=DEFAULT_NEWKEYS_PATH, default=None, metavar='FILE', help='Write the keys the target(s) lack into a template file (default: display_newkeys.json), keyed to the master text')
    parser.add_argument('--extract', '-x', nargs='?', const='', default=None, metavar='FILE', help='Print this locale\'s values for the keys the --newkeys template lists, as a labelled sheet for a native-speaker check; with FILE, append the sheet to it instead (no other option applies in this mode)')
    args = parser.parse_args()

    # Determine master and target(s)
    if args.target2 is not None:
        # Two positional args: first is master, second is target
        master_code = args.target
        target_code = args.target2
        targets = [target_code]
    elif args.target == '*':
        # Wildcard: all files except the default master
        master_code = DEFAULT_MASTER
        targets = []
        for f in sorted(glob.glob(os.path.join(DISPLAY_DIR, '*.json'))):
            code = os.path.splitext(os.path.basename(f))[0]
            if code != master_code:
                targets.append(code)
        if not targets:
            print(f"Error: No locale files found in {DISPLAY_DIR} (excluding {master_code})")
            sys.exit(1)
    else:
        # Single arg: it's the target, master defaults to en_US
        master_code = DEFAULT_MASTER
        target_code = args.target
        targets = [target_code]

    # Validate argument combinations
    mode_count = sum([args.fast, args.every, args.diff, args.ndiff])
    if mode_count > 1:
        print("Error: Only one mode can be specified (--fast, --every, --diff, --ndiff)")
        sys.exit(1)

    if args.merge:
        # A partial file is written for one language, so the wildcard has nothing to mean here.
        if args.target == '*':
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
        if args.target == '*':
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
        json_path = os.path.join(DISPLAY_DIR, f'{target_code}.json')
        out_path = None
        if args.extract:
            # A bare name lands beside this script and the template, so a sheet is never a mystery path.
            out_path = args.extract if os.path.isabs(args.extract) else os.path.join(SCRIPT_DIR, args.extract)
            out_path = os.path.abspath(out_path)
        print()
        ok = extract_keys(json_path, DEFAULT_NEWKEYS_PATH, out_path, f"New display keys: {target_code}")
        sys.exit(0 if ok else 1)

    if args.translate and args.diff:
        print("Error: --translate cannot be used with --diff mode")
        sys.exit(1)

    if args.target == '*' and (args.every or args.diff or args.ndiff):
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

    print()

    # Check translation service availability.  A merge never prompts and never translates, so it skips the
    # detection outright - otherwise mode 'missing' would stop and ask about the service before the merge starts.
    # Skipped for a collect-only run so it cannot stop and ask about the service; if --translate was given anyway,
    # detection still runs so the answer is honest rather than a misleading "unavailable".
    service = None if (args.merge or (collect_only and not args.translate)) else detect_translation_service()

    if service:
        if not args.translate:
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
                    print("→ Translation disabled, will use master text\n")
        else:
            print(f"{'='*60}")
            print(f"✓ Translation service: {service.upper()}")
            print(f"{'='*60}\n")
    else:
        if args.translate:
            print(f"{'='*60}")
            print("⚠ Error: --translate flag is set but translation is unavailable!")
            print("⚠ No translation service (add API key to trans_<service>.key)")
            print(f"{'='*60}")
            sys.exit(1)
        elif mode == 'missing' and not args.merge and not collect_only:
            print(f"{'='*60}")
            print("⚠ Warning: No translation service found.")
            print("⚠ This tool is more powerful with a translation service!")
            print(f"{'='*60}\n")

    # Validate master exists
    master_path = os.path.join(DISPLAY_DIR, f'{master_code}.json')
    if not os.path.exists(master_path):
        print(f"Error: Master locale file not found at {master_path}")
        sys.exit(1)

    master_data = load_json_safe(master_path)
    if master_data is None:
        sys.exit(1)

    print(f"Master: {master_code}.json ({len(get_translation_keys(master_data))} keys)")

    # Check the targeted key against the master before touching any locale, so a typo stops the run once here
    # instead of printing the same complaint for every locale file.
    if args.key is not None:
        if args.key not in get_translation_keys(master_data):
            print(f"Error: key '{args.key}' is not in {master_code}.json")
            sys.exit(1)
        print(f"Single key: {args.key}")

    if args.target == '*':
        print(f"Found {len(targets)} locale file(s) to process (excluding master: {master_code})")

    # Merge mode fills one locale that already exists and never prompts; everything below is the pass that scans
    # for missing keys and asks about them.
    if args.merge:
        merge_path = resolve_merge_path(args.merge, DISPLAY_DIR)
        if merge_path is None:
            sys.exit(1)
        tcode = targets[0]
        json_path = os.path.join(DISPLAY_DIR, f'{tcode}.json')
        if not merge_partial_file(tcode, master_code, master_data, merge_path, args.clean, args.sort):
            sys.exit(1)
        return

    # --newkeys: collect first, so the template holds the keys that were missing when this run started - a mode
    # given below then fills the locales in the same run.  A collect-only run stops here.
    if args.newkeys is not None:
        if not newkeys_display(master_code, master_data, targets, args.newkeys, args.clean, args.sort):
            sys.exit(1)
        if collect_only:
            return

    # Process all targets
    success_count = 0
    for tcode in targets:
        # Never allow clean/sort on master when using wildcard
        use_clean = args.clean and tcode != master_code
        use_sort = args.sort
        if process_display_locale(tcode, master_code, master_data, mode, use_clean, use_sort, args.translate, args.key):
            success_count += 1

    if len(targets) > 1:
        print(f"\n{'='*60}")
        print(f"Processed {success_count}/{len(targets)} locale file(s) successfully")
        print(f"{'='*60}")


if __name__ == '__main__':
    main()
