"""
pre_patch.py - complain early, and only when something is actually wrong.

Runs before the framework script. It writes nothing and changes nothing: its whole job is to
notice that builds/patch/<mcu>/liblwip.a was built from a different ESP-IDF or a different
package sdkconfig than the one now installed, because that is the failure mode that links
without a single warning and then misbehaves on the device.

Silent when no patch exists, so a fresh clone and every build of an unpatched tree log
nothing at all. See patch.md in this folder.
"""
Import("env")

import hashlib
import re
from pathlib import Path

PREFIX = "Patch"
PATCH_ROOT = Path(env.subst("$PROJECT_DIR")) / "builds" / "patch"


def _warn(message):
    print(f"\n{PREFIX} WARNING: {message}\n")


def _sha256(path, length=64):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()[:length]


def _mcu():
    return env.BoardConfig().get("build.mcu", "esp32")


def _read_info(path):
    info = {}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, value = line.split("=", 1)
        info[key.strip()] = value.strip()
    return info


def _package_dir(mcu):
    name = f"framework-arduinoespressif32-{mcu}-libs"
    try:
        return Path(env.PioPlatform().get_package_dir(name)), name
    except Exception:
        return None, name


def _package_idf_version(package):
    versions = package / "versions.txt"
    if not versions.is_file():
        return ""
    for line in versions.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.lower().startswith("esp-idf:"):
            return line.split(":", 1)[1].strip()
    return ""


def main():
    mcu = _mcu()
    archive = PATCH_ROOT / mcu / "liblwip.a"
    info_path = PATCH_ROOT / mcu / "patch-info.txt"

    if not archive.is_file() and not info_path.is_file():
        return  # stock lwIP, nothing to say

    if not info_path.is_file():
        _warn(
            f"{archive} exists but {info_path.name} does not, so it cannot be checked "
            f"against the installed ESP-IDF.\n"
            f"  Re-run: python builds/patch/make_patch.py"
        )
        return

    info = _read_info(info_path)
    if not archive.is_file():
        _warn(
            f"{info_path} exists but {archive} does not; the build will link the package's "
            f"own lwIP.\n"
            f"  Either restore the archive or delete {info_path.parent}."
        )
        return

    problems = []
    package, package_name = _package_dir(mcu)
    if info.get("libs_package") and info["libs_package"] != package_name:
        problems.append(
            f"built for {info['libs_package']}, this build uses {package_name}"
        )

    if package is not None:
        package_version = _package_idf_version(package)
        recorded = info.get("idf_version", "")
        if package_version and recorded and not recorded.startswith(package_version.split()[0]):
            problems.append(f"built against ESP-IDF {recorded}, package now has {package_version}")

        sdkconfig = package / "sdkconfig"
        recorded_sha = info.get("sdkconfig_sha256")
        if sdkconfig.is_file() and recorded_sha and recorded_sha != _sha256(sdkconfig):
            problems.append(
                "the package's sdkconfig changed since the archive was built, so the "
                "archive no longer matches the rest of the precompiled libraries"
            )

    recorded_sha = info.get("lwip_sha256")
    actual_sha = _sha256(archive)
    if recorded_sha and recorded_sha != actual_sha:
        problems.append(
            f"liblwip.a itself is not the archive that was recorded "
            f"({actual_sha[:12]} instead of {recorded_sha[:12]})"
        )

    if problems:
        _warn(
            "the patched liblwip.a is stale:\n  - "
            + "\n  - ".join(problems)
            + "\n  Re-run: python builds/patch/make_patch.py"
        )
    else:
        print(f"{PREFIX}: {mcu} liblwip.a {actual_sha[:12]} ({info.get('changes', 'no changes')})")


main()
