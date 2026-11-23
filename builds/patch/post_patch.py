"""
post_patch.py - let the build link our rebuilt archives without editing the package.

The Arduino builder resolves ESP-IDF archives by name (-llwip) through $LIBPATH, whose first
entry is normally <libs package>/lib. Prepending our own directory puts the patched archive
ahead of it and nothing in the PlatformIO packages tree is ever read-write. If the archive is
missing this script does nothing, so an unpatched checkout builds exactly as before.

The post-link line exists so a build log always states which archives went into the firmware - the
question "did the patch actually link?" should never need a guess.  It names the count as well, because
the folder can hold more than liblwip.a (the three mbedTLS archives, when --mbedtls was used). See
patch.md in this folder.
"""
Import("env")

import hashlib
from pathlib import Path

from SCons.Script import COMMAND_LINE_TARGETS

PREFIX = "Patch"
PATCH_ROOT = Path(env.subst("$PROJECT_DIR")) / "builds" / "patch"
FILESYSTEM_TARGETS = {"buildfs", "uploadfs"}


def _filesystem_only_build():
    # A run that only packs LittleFS never links, so the patch cannot matter.
    targets = list(COMMAND_LINE_TARGETS)
    return bool(targets) and all(target in FILESYSTEM_TARGETS for target in targets)


def _sha256(path, length=12):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()[:length]


def _mcu():
    return env.BoardConfig().get("build.mcu", "esp32")


def _announce_after_link(archive_sha, mcu, archives):
    def _report(source, target, env):
        print(f"{PREFIX} linked: liblwip.a {archive_sha} "
              f"({len(archives)} archive(s) from builds/patch/{mcu}: {', '.join(archives)})")

    try:
        # The display string matters: without it SCons echoes the whole function repr and the
        # one useful line drowns in a list of object files.
        env.AddPostAction(
            "$BUILD_DIR/${PROGNAME}.elf",
            env.Action(_report, f"{PREFIX}: confirming the linked liblwip.a"),
        )
    except Exception as exc:  # only if a platform someday renames the elf target
        print(f"{PREFIX}: could not hook the link step ({exc})")


if not _filesystem_only_build():
    mcu = _mcu()
    archive = PATCH_ROOT / mcu / "liblwip.a"
    if archive.is_file():
        env.Prepend(LIBPATH=[str(archive.parent)])
        _announce_after_link(_sha256(archive), mcu, sorted(path.name for path in archive.parent.glob("*.a")))
