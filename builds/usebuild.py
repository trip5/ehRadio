#!/usr/bin/env python3
"""
usebuild.py - Safely swap platformio.ini, myoptions.h, and mytheme.h for testing a contributor's config.

Usage:
  python usebuild.py <subfolder>   # Activate: install subfolder's config files
  python usebuild.py               # Restore: put the originals back

How the .vscode/settings.json patch works:
  The filewatcher extension syncs root platformio.ini/myoptions.h/mytheme.h → builds/<contributor>/.
  On activate all three filewatcher destinations are redirected (e.g. trip5 → test) so any
  subsequent edits are automatically synced to the test folder instead.
  The original contributor name is read from settings.json at activate time and saved to
  usebuild.lck in the workspace root so restore can always find it.
  If this ever breaks, look for \\builds\\<name>\\platformio.ini in .vscode/settings.json.

  A lock file (usebuild.lck) is created in builds/ to prevent double-swapping and store the build name.
  A second lock file (usebuild.lck) is created in the workspace root to store the original contributor name.

  platformio.ini and myoptions.h are both mandatory in the builds subfolder; mytheme.h is optional.

Note:
  This absolutely depends on File Watcher Extension to automatically sync swapped config
  files back to the correct folder in builds/ - without it, you will have to manually copy the
  platformio.ini and options.h (and optionally mytheme.h) files or contents.

  Of note, this has only been tested with "event": "onFileChange" and no other events.
"""

import sys
import re
import shutil
from pathlib import Path

SCRIPT_DIR     = Path(__file__).parent          # builds/
ROOT           = SCRIPT_DIR.parent              # workspace root
PLATFORMIO_INI = ROOT / "platformio.ini"
PLATFORMIO_BK  = ROOT / "platformio.ini._bk"
MYOPTIONS_H    = ROOT / "myoptions.h"
MYOPTIONS_BK   = ROOT / "myoptions.h._bk"
MYTHEME_H      = ROOT / "mytheme.h"
MYTHEME_BK     = ROOT / "mytheme.h._bk"
LOCK_FILE      = SCRIPT_DIR / "usebuild.lck"   # contains the active subfolder name
ORIGIN_FILE    = ROOT / "usebuild.lck"          # contains the original contributor name
SETTINGS_JSON  = ROOT / ".vscode" / "settings.json"


def _read_settings():
    if not SETTINGS_JSON.exists():
        print(f"⚠️  {SETTINGS_JSON} not found, skipping settings patch")
        return None
    return SETTINGS_JSON.read_text(encoding="utf-8")


def _write_settings(text):
    SETTINGS_JSON.write_text(text, encoding="utf-8")
    print("✅ Patched .vscode/settings.json")


def _change_destination(text, new_folder):
    """Replace \\builds\\<any>\\<file> with \\builds\\<new_folder>\\<file> for all filewatcher destinations."""
    return re.sub(
        r'(\\\\builds\\\\)[^\\]+(\\\\(?:platformio\.ini|myoptions\.h|mytheme\.h))',
        lambda m: m.group(1) + new_folder + m.group(2),
        text
    )


def _read_contributor_from_settings(text):
    """Read the current contributor folder name from the platformio.ini filewatcher line."""
    m = re.search(r'\\\\builds\\\\([^\\]+)\\\\platformio\.ini', text)
    return m.group(1) if m else None


def activate(subfolder_name: str) -> int:
    subfolder = SCRIPT_DIR / subfolder_name
    src_ini   = subfolder / "platformio.ini"
    src_opts  = subfolder / "myoptions.h"
    src_theme = subfolder / "mytheme.h"

    if LOCK_FILE.exists() or PLATFORMIO_BK.exists():
        print("❌ Don't try to use another build until you've restored (run this script with no parameter).")
        return 1

    if not subfolder.is_dir() or not src_ini.exists() or not src_opts.exists():
        print(f"❌ '{subfolder_name}/platformio.ini' and '{subfolder_name}/myoptions.h' must both exist!")
        return 1

    # Read and save original contributor name before modifying settings.json
    text = _read_settings()
    original = _read_contributor_from_settings(text) if text else None
    if not original:
        print("⚠️  Could not determine original contributor from settings.json")
        original = ""
    ORIGIN_FILE.write_text(original, encoding="utf-8")

    # Create lock file
    LOCK_FILE.write_text(subfolder_name, encoding="utf-8")

    # Backup root config files (mytheme.h always backed up even if subfolder has none)
    shutil.copy2(PLATFORMIO_INI, PLATFORMIO_BK)
    shutil.copy2(MYOPTIONS_H, MYOPTIONS_BK)
    try:
        shutil.copy2(MYTHEME_H, MYTHEME_BK)
    except Exception as e:
        print(f"⚠️  Could not back up mytheme.h (may not exist): {e}")

    # Redirect all filewatcher destinations to the new subfolder
    if text:
        text = _change_destination(text, subfolder_name)
        _write_settings(text)

    # Install the test config files
    shutil.copy2(src_ini, PLATFORMIO_INI)
    shutil.copy2(src_opts, MYOPTIONS_H)
    if src_theme.exists():
        shutil.copy2(src_theme, MYTHEME_H)
    else:
        # Subfolder has no mytheme.h — remove it from root so the build sees none
        try:
            MYTHEME_H.unlink(missing_ok=True)
        except Exception as e:
            print(f"⚠️  Could not remove mytheme.h from root: {e}")

    print(f"Using '{subfolder_name}' builds for platformio.ini, myoptions.h, mytheme.h")
    print(f"   Run 'python usebuild.py' (no argument) when done testing to restore.")

    # Nudge each installed file twice so VS Code notices the change and reloads the editor
    nudge_files = [PLATFORMIO_INI, MYOPTIONS_H]
    if src_theme.exists():
        nudge_files.append(MYTHEME_H)
    for f in nudge_files:
        for _ in range(2):
            content = f.read_bytes()
            f.write_bytes(content[:-1])
            f.write_bytes(content)

    return 0


def restore() -> int:
    if not LOCK_FILE.exists():
        print("⚠️  No lock file found (usebuild.lck does not exist), nothing to restore")
        return 0

    subfolder_name = LOCK_FILE.read_text(encoding="utf-8").strip()
    original = ORIGIN_FILE.read_text(encoding="utf-8").strip() if ORIGIN_FILE.exists() else ""
    print("Restoring default config files")

    # Patch settings.json: restore all filewatcher destinations to original contributor
    text = _read_settings()
    if text and original:
        text = _change_destination(text, original)
        _write_settings(text)
    elif not original:
        print("⚠️  Could not determine original contributor, skipping settings.json restore")

    # Restore root config files from backups
    try:
        shutil.copy2(PLATFORMIO_BK, PLATFORMIO_INI)
    except Exception as e:
        print(f"⚠️  Could not restore platformio.ini: {e}")

    try:
        shutil.copy2(MYOPTIONS_BK, MYOPTIONS_H)
    except Exception as e:
        print(f"⚠️  Could not restore myoptions.h: {e}")

    if MYTHEME_BK.exists():
        try:
            shutil.copy2(MYTHEME_BK, MYTHEME_H)
        except Exception as e:
            print(f"⚠️  Could not restore mytheme.h: {e}")

    # Delete backups
    for bk in (PLATFORMIO_BK, MYOPTIONS_BK, MYTHEME_BK):
        try:
            bk.unlink(missing_ok=True)
        except Exception as e:
            print(f"⚠️  Could not delete {bk.name}: {e}")

    # Delete lock files
    for lk in (LOCK_FILE, ORIGIN_FILE):
        try:
            lk.unlink(missing_ok=True)
        except Exception as e:
            print(f"⚠️  Could not delete {lk.name}: {e}")

    # Offer to clean up .pio/*/<env> directories not in the restored platformio.ini
    pio_dir = ROOT / ".pio"
    try:
        ini_text = PLATFORMIO_INI.read_text(encoding="utf-8")
        keep_envs = set(re.findall(r'(?<=\[env:)[^\]]+', ini_text))
    except Exception:
        keep_envs = set()

    # Collect all per-env folders across every .pio/ subdirectory (build, libdeps, etc.)
    stale = []
    if pio_dir.is_dir():
        for pio_sub in sorted(pio_dir.iterdir()):
            if pio_sub.is_dir():
                for env_dir in sorted(pio_sub.iterdir()):
                    if env_dir.is_dir() and env_dir.name not in keep_envs:
                        stale.append(env_dir)

    if stale:
        print(f"\n{len(stale)} stale .pio folder(s) not in current platformio.ini:")
        for d in stale:
            print(f"  .pio/{d.parent.name}/{d.name}")
        answer = input("Delete them? [Y/N] ").strip().upper()
        if answer == "Y":
            for d in stale:
                try:
                    shutil.rmtree(d)
                    print(f"  Deleted .pio/{d.parent.name}/{d.name}")
                except Exception as e:
                    print(f"⚠️  Could not delete {d}: {e}")

    return 0


def main() -> int:
    if len(sys.argv) == 1:
        return restore()
    if len(sys.argv) == 2:
        return activate(sys.argv[1])
    print("Usage:")
    print("  python usebuild.py <subfolder>   # activate test config")
    print("  python usebuild.py               # restore original")
    return 1


if __name__ == "__main__":
    exit(main())
