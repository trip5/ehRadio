"""
post_patch_lwip.py - let the build link our liblwip.a without editing the package.

The Arduino builder resolves ESP-IDF archives by name (-llwip) through $LIBPATH, whose first
entry is normally <libs package>/lib. Prepending our own directory puts the patched archive
ahead of it and nothing in the PlatformIO packages tree is ever read-write. If the archive is
missing this script does nothing, so an unpatched checkout builds exactly as before.

The post-link line exists so a build log always states which lwIP went into the firmware - the
question "did the patch actually link?" should never need a guess. See
plans/lwip-rebuild-patch.md.
"""
Import("env")

import hashlib
from pathlib import Path

PREFIX = "LWIP patch"
PATCH_ROOT = Path(env.subst("$PROJECT_DIR")) / "builds" / "patch"


def _sha256(path, length=12):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()[:length]


def _mcu():
    return env.BoardConfig().get("build.mcu", "esp32")


def _announce_after_link(archive_sha):
    def _report(source, target, env):
        print(f"{PREFIX} linked: liblwip.a {archive_sha}")

    try:
        # The display string matters: without it SCons echoes the whole function repr and the
        # one useful line drowns in a list of object files.
        env.AddPostAction(
            "$BUILD_DIR/${PROGNAME}.elf",
            env.Action(_report, f"{PREFIX}: confirming the linked liblwip.a"),
        )
    except Exception as exc:  # only if a platform someday renames the elf target
        print(f"{PREFIX}: could not hook the link step ({exc})")


archive = PATCH_ROOT / _mcu() / "liblwip.a"
if archive.is_file():
    env.Prepend(LIBPATH=[str(archive.parent)])
    _announce_after_link(_sha256(archive))
