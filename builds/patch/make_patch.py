#!/usr/bin/env python3
"""
make_patch.py - rebuild ESP-IDF archives with a few sdkconfig values changed.

Two changes live here: liblwip.a (the TCP window and friends) and the three mbedTLS archives (record
buffers and context from PSRAM instead of internal DRAM).  Both are on by default, because both are what
this codebase actually needs on espressif32 8.0.0; --no-mbedtls drops the second one.

Usage:
  python builds/patch/make_patch.py                             # every installed libs package, both changes
  python builds/patch/make_patch.py --target esp32s3             # only that MCU
  python builds/patch/make_patch.py --no-mbedtls                 # liblwip.a only
  python builds/patch/make_patch.py --idf-path D:/esp/esp-idf-v5.5.5 --keep-temp

WHY A REBUILD IS THE ONLY WAY

  TCP_WND is a compile-time constant. platformio-build.py links the precompiled
  framework-arduinoespressif32-<mcu>-libs archive, and lwIP's window is baked into it at
  `CONFIG_LWIP_TCP_WND_DEFAULT=5760`. setsockopt(SO_RCVBUF) cannot substitute: lwIP says of
  recv_bufsize "not used for TCP: adjust TCP_WND instead!" (lwip/src/include/lwip/api.h) and
  TCP_WND_MAX(pcb) is literally TCP_WND while window scaling is off.

WHAT THIS DOES

  1. Finds the installed libs package for each target and reads the IDF version from its
     versions.txt - that file is written by whoever built the package, so it is the only
     honest answer to "which IDF are we matching".
  2. Fetches ESP-IDF at that tag (or reuses --idf-path), with submodules: components/lwip/lwip
     is one of them.
  3. Makes a throwaway venv in the temp dir (pip: requirements.core.txt, cmake, ninja) and
     reuses the already installed PlatformIO xtensa toolchain rather than downloading a
     second copy of the same compiler.
  4. Copies the package's own sdkconfig as the base, applies ONLY the whitelist below,
     asserts the blacklist is untouched, configures a two-file project, then builds a single
     ninja output - esp-idf/lwip/liblwip.a - instead of a whole IDF tree.
  5. Copies the archives to builds/patch/<target>/ and writes patch-info.txt beside them.
     post_patch.py picks them up from there; this script never touches the packages tree.

WHAT IT DOES NOT DO

  It does not modify, move or back up anything under the PlatformIO packages directory, and
  it does not build the rest of ESP-IDF. ABI safety rests on the whitelist: every entry in it
  changes a value that lwIP keeps in its own heap objects or in a u16_t that is unchanged
  while CONFIG_LWIP_WND_SCALE is off.
"""

import argparse
import hashlib
import os
import re
import shlex
import shutil
import stat
import subprocess
import sys
import tempfile
from datetime import datetime, timezone
from pathlib import Path

# --- The only sdkconfig entries this script is allowed to change --------------------------
WHITELIST = {
    # The window and the mailbox move together, and that pair is what was measured on hardware: 32 KB of
    # window is 23 segments at MSS 1436, so a 16-message mailbox would only queue 23 KB of it.
    # assert_parameters_sane() warns when the two disagree.
    "CONFIG_LWIP_TCP_WND_DEFAULT": 32768,     # was 5760
    "CONFIG_LWIP_TCP_RECVMBOX_SIZE": 32,      # was 6 (~8.6 KB at MSS 1436 - the real cap)
    "CONFIG_LWIP_TCP_SND_BUF_DEFAULT": 8192,  # was 5744
    "CONFIG_LWIP_MAX_ACTIVE_TCP": 32,         # was 16
}

# --- Entries that would change a type other precompiled archives also compile against ------
BLACKLIST = (
    "CONFIG_LWIP_WND_SCALE",        # tcpwnd_size_t u16_t -> u32_t: corrupts every tcp_pcb
    "CONFIG_LWIP_MAX_SOCKETS",      # sizes lwIP's fd_set
    "CONFIG_LWIP_PBUF_LINK_HLEN",   # pbuf layout
    "CONFIG_LWIP_IPV6",
    "CONFIG_LWIP_SO_REUSE",
)

# --- mbedTLS, applied only with --mbedtls ----------------------------------------------------------------
# Two booleans: EXTERNAL moves mbedtls_calloc into PSRAM, INTERNAL is the opposite it replaces.  Only mbedTLS's own
# allocator changes, so no type shared with the other precompiled archives moves.  Worth it because the internal heap
# on 8.0.0 reaches steady state with a 30 KB largest block while mbedTLS needs ~40 KB contiguous for a TLS handshake
# (measured).  None as a value means "emit as `# KEY is not set`".
MBEDTLS_WHITELIST = {
    "CONFIG_MBEDTLS_EXTERNAL_MEM_ALLOC": "y",
    "CONFIG_MBEDTLS_INTERNAL_MEM_ALLOC": None,
}

# Extra archives, by ninja output path relative to the build directory.
EXTRA_ARCHIVES = {
    "esp_netif": "esp-idf/esp_netif/libesp_netif.a",
    "mbedcrypto": "esp-idf/mbedtls/mbedtls/library/libmbedcrypto.a",
    "mbedx509": "esp-idf/mbedtls/mbedtls/library/libmbedx509.a",
    "mbedtls": "esp-idf/mbedtls/mbedtls/library/libmbedtls.a",
}

# MCU names that can have a framework-arduinoespressif32-<mcu>-libs package installed.  With no --target the
# script patches every one it finds, in this order, so a checkout with both esp32 and esp32s3 does both in one
# run and shares one ESP-IDF clone between them.
KNOWN_MCUS = ("esp32", "esp32s2", "esp32s3", "esp32c3", "esp32c6", "esp32h2", "esp32p4")
IDF_REPO = "https://github.com/espressif/esp-idf.git"
IDF_MSS = 1436

SCRIPT_DIR = Path(__file__).resolve().parent          # builds/patch
REPO_ROOT = SCRIPT_DIR.parent.parent                  # repo root


# -----------------------------------------------------------------------------
# helpers
# -----------------------------------------------------------------------------

def say(step, message):
    print(f"[{step}] {message}")


def die(message):
    print(f"\nERROR: {message}\n")
    raise SystemExit(1)


def run(cmd, cwd=None, env=None, capture=False):
    printable = " ".join(shlex.quote(str(part)) for part in cmd)
    print(f"    > {printable}")
    result = subprocess.run(
        [str(part) for part in cmd],
        cwd=str(cwd) if cwd else None,
        env=env,
        text=True,
        capture_output=capture,
    )
    if result.returncode != 0:
        if capture and result.stderr:
            print(result.stderr)
        die(f"command failed with exit code {result.returncode}")
    return result.stdout if capture else ""


def rmtree(path):
    """shutil.rmtree with the read-only bits cleared.

    A git clone leaves read-only object files behind, and on Windows rmtree then removes
    everything except the directory it cannot empty - which makes the next `git clone` fail
    with "destination path already exists". The retry callback handles both the modern
    (`onexc`) and the older (`onerror`) signature.
    """
    def _retry(func, target, _unused):
        try:
            os.chmod(target, stat.S_IWRITE)
            func(target)
        except Exception:
            pass

    if not path.exists():
        return
    if sys.version_info >= (3, 12):
        shutil.rmtree(path, onexc=_retry)
    else:
        shutil.rmtree(path, onerror=_retry)


def sha256_of(path, length=64):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()[:length]


def version_value(text, key):
    """Read `key = value` for an `# key is not set` aware sdkconfig/sdkconfig.h reader."""
    match = re.search(r"^%s=(.*)$" % re.escape(key), text, re.MULTILINE)
    if match:
        return match.group(1).strip()
    if re.search(r"^# %s is not set$" % re.escape(key), text, re.MULTILINE):
        return None
    return "<missing>"


# -----------------------------------------------------------------------------
# discovery
# -----------------------------------------------------------------------------

def find_packages_dir(override):
    if override:
        candidate = Path(override).expanduser()
        if not candidate.is_dir():
            die(f"--packages-dir {candidate} is not a directory")
        return candidate

    candidates = []
    if os.environ.get("PLATFORMIO_CORE_DIR"):
        candidates.append(Path(os.environ["PLATFORMIO_CORE_DIR"]) / "packages")
    candidates.append(Path.home() / ".platformio" / "packages")

    for candidate in candidates:
        if (candidate / "framework-arduinoespressif32").is_dir():
            return candidate

    # Last resort: no usable core dir, but there may be a libs package we can reason about.
    die(
        "could not find PlatformIO's packages directory.\n"
        "  Pass --packages-dir (the one containing framework-arduinoespressif32-<mcu>-libs).\n"
        f"  Tried: {', '.join(str(c) for c in candidates)}"
    )


def libs_package_for(packages_dir, target):
    package = packages_dir / f"framework-arduinoespressif32-{target}-libs"
    if not package.is_dir():
        return None
    return package


def detect_targets(packages_dir):
    """Every MCU that has a usable libs package installed, in KNOWN_MCUS order.

    A package only counts when it carries both versions.txt (the IDF it was built from) and sdkconfig (the
    base this script patches).  Anything else would fail later with a worse message.
    """
    found = []
    prefix, suffix = "framework-arduinoespressif32-", "-libs"
    for package in packages_dir.glob(f"{prefix}*{suffix}"):
        mcu = package.name[len(prefix):-len(suffix)]
        if not mcu or mcu == "libs":
            continue
        if not (package / "versions.txt").is_file() or not (package / "sdkconfig").is_file():
            continue
        found.append(mcu)

    ordered = [mcu for mcu in KNOWN_MCUS if mcu in found]
    ordered += sorted(mcu for mcu in found if mcu not in KNOWN_MCUS)
    if not ordered:
        die(
            f"no {prefix}<mcu>{suffix} package in {packages_dir}.\n"
            "  Build once for the platform so PlatformIO installs it, or pass --target explicitly."
        )
    return ordered


def read_idf_version(libs_package):
    versions = libs_package / "versions.txt"
    if not versions.is_file():
        die(f"{versions} is missing - cannot tell which ESP-IDF this package was built from")

    for line in versions.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.lower().startswith("esp-idf:"):
            value = line.split(":", 1)[1].strip()
            parts = value.split()
            if not parts:
                break
            return parts[0], (parts[1] if len(parts) > 1 else "")
    die(f"no `esp-idf:` line in {versions}")


def find_toolchain(packages_dir):
    bin_dir = packages_dir / "toolchain-xtensa-esp-elf" / "bin"
    if not bin_dir.is_dir():
        die(
            f"{bin_dir} is not present.\n"
            "  Install the espressif32 platform once (any build does it) so the toolchain "
            "is there, or add the toolchain to PATH and rerun."
        )
    return bin_dir


# -----------------------------------------------------------------------------
# sdkconfig
# -----------------------------------------------------------------------------

def patch_sdkconfig(text, wanted):
    changes = []
    for key, value in wanted.items():
        pattern = re.compile(r"^%s=.*$" % re.escape(key), re.MULTILINE)
        not_set = f"# {key} is not set"
        current = version_value(text, key)

        if value is None:                     # a boolean being switched off: write the "is not set" form
            if current is None:
                continue
            if current == "<missing>":
                text = text + ("" if text.endswith("\n") else "\n") + not_set + "\n"
                changes.append((key, "absent", "not set"))
            else:
                text = pattern.sub(not_set, text, count=1)
                changes.append((key, current, "not set"))
            continue

        if current == "<missing>":
            die(
                f"{key} is not in the package sdkconfig.\n"
                "  The package layout probably changed; re-read patch.md in this folder."
            )
        if current is None:                   # present as `# KEY is not set`: switching it on
            text = re.sub(r"^# %s is not set$" % re.escape(key), f"{key}={value}", text,
                          count=1, flags=re.MULTILINE)
            changes.append((key, "not set", str(value)))
            continue
        if current != str(value):
            text = pattern.sub(f"{key}={value}", text, count=1)
            changes.append((key, current, str(value)))
    return text, changes


def assert_blacklist_untouched(before, after):
    for key in BLACKLIST:
        old, new = version_value(before, key), version_value(after, key)
        if old != new:
            die(f"{key} changed ({old} -> {new}); that is not ABI safe and not allowed here")


def assert_parameters_sane(wanted):
    if "CONFIG_LWIP_TCP_WND_DEFAULT" not in wanted:
        return
    wnd = wanted["CONFIG_LWIP_TCP_WND_DEFAULT"]
    box = wanted["CONFIG_LWIP_TCP_RECVMBOX_SIZE"]
    if wnd > 65535:
        die("window > 65535 needs CONFIG_LWIP_WND_SCALE, which breaks ABI - stay <= 65535")
    capacity = box * IDF_MSS
    if capacity < wnd:
        print(
            f"    WARNING: {box} mailbox messages hold ~{capacity} bytes, less than the "
            f"{wnd}-byte window; the mailbox, not the window, will be the ceiling"
        )


# -----------------------------------------------------------------------------
# ESP-IDF
# -----------------------------------------------------------------------------

def ensure_idf(idf_path, tag, commit, temp_root):
    if idf_path:
        idf_dir = Path(idf_path).expanduser()
        if not (idf_dir / "tools" / "idf.py").is_file():
            die(f"--idf-path {idf_dir} does not look like an ESP-IDF checkout")
        say("idf", f"using existing checkout {idf_dir}")
    else:
        idf_dir = temp_root / "esp-idf"
        complete = (
            (idf_dir / "tools" / "idf.py").is_file()
            and (idf_dir / "components" / "lwip" / "lwip" / "src").is_dir()
        )
        if complete:
            say("idf", f"reusing the checkout already in {temp_root}")
        else:
            if idf_dir.exists():
                say("idf", "the existing checkout is incomplete; starting over")
                rmtree(idf_dir)
                if idf_dir.exists():
                    die(f"could not remove {idf_dir}; delete it by hand and rerun")
            say("idf", f"cloning ESP-IDF {tag} with submodules (this is the slow step)")
            run([
                "git", "clone", "--depth", "1", "--branch", tag,
                "--recurse-submodules", "--shallow-submodules", IDF_REPO, str(idf_dir),
            ])

    head = run(["git", "-C", str(idf_dir), "rev-parse", "--short", "HEAD"], capture=True).strip()
    if commit and not head.startswith(commit[:7]):
        print(f"    WARNING: checkout is {head}, package was built from {commit}")
    else:
        say("idf", f"checkout {head} matches versions.txt")
    return idf_dir


def ensure_venv(venv_dir, idf_dir):
    python = venv_dir / ("Scripts" if os.name == "nt" else "bin") / ("python.exe" if os.name == "nt" else "python")
    if not python.is_file():
        say("venv", f"creating {venv_dir}")
        run([sys.executable, "-m", "venv", str(venv_dir)])

    requirements = idf_dir / "tools" / "requirements" / "requirements.core.txt"
    if not requirements.is_file():
        die(f"{requirements} is missing - IDF checkout is incomplete")

    say("venv", "installing IDF requirements and the cmake/ninja pair")
    run([python, "-m", "pip", "install", "--disable-pip-version-check", "--quiet",
         "--upgrade", "pip"])
    run([python, "-m", "pip", "install", "--disable-pip-version-check", "--quiet",
         "-r", str(requirements)])
    run([python, "-m", "pip", "install", "--disable-pip-version-check", "--quiet",
         "cmake", "ninja"])
    return python


def build_env(python, toolchain_bin, idf_dir, target):
    venv_bin = Path(python).parent
    environment = os.environ.copy()
    environment["PATH"] = os.pathsep.join([
        str(venv_bin),
        str(toolchain_bin),
        environment.get("PATH", ""),
    ])
    environment["IDF_PATH"] = str(idf_dir)
    environment["IDF_TARGET"] = target
    return environment


def scaffold_project(project_dir, sdkconfig_text):
    project_dir.mkdir(parents=True, exist_ok=True)
    (project_dir / "main").mkdir(exist_ok=True)
    (project_dir / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.16)\n"
        "include($ENV{IDF_PATH}/tools/cmake/project.cmake)\n"
        "project(ehradio_lwip_patch)\n",
        encoding="utf-8",
    )
    (project_dir / "main" / "CMakeLists.txt").write_text(
        'idf_component_register(SRCS "main.c")\n',
        encoding="utf-8",
    )
    (project_dir / "main" / "main.c").write_text(
        "/* nothing to run; this project exists to make one archive */\n"
        "void app_main(void) {}\n",
        encoding="utf-8",
    )
    (project_dir / "sdkconfig").write_text(sdkconfig_text, encoding="utf-8")


def build_archive(python, idf_dir, project_dir, environment, jobs, extra_archives):
    venv_bin = Path(python).parent
    cmake = venv_bin / ("cmake.exe" if os.name == "nt" else "cmake")
    ninja = venv_bin / ("ninja.exe" if os.name == "nt" else "ninja")
    build_dir = project_dir / "build"

    # idf.py is deliberately not used. It refuses to run unless IDF's own install script has
    # created a python_env and downloaded a constraints file into IDF_TOOLS_PATH, and none of
    # that is needed to compile one component. PYTHON_DEPS_CHECKED is the switch IDF's
    # project.cmake itself offers to skip that probe - it is what idf.py sets after checking
    # once. Everything else (toolchain, python, target) is supplied explicitly here.
    say("cmake", f"configuring {project_dir.name}")
    run([
        cmake, "-S", project_dir, "-B", build_dir, "-G", "Ninja",
        f"-DIDF_TARGET={environment['IDF_TARGET']}",
        f"-DCMAKE_TOOLCHAIN_FILE="
        f"{idf_dir / 'tools' / 'cmake' / ('toolchain-%s.cmake' % environment['IDF_TARGET'])}",
        f"-DPYTHON={python}",
        "-DPYTHON_DEPS_CHECKED=1",
    ], env=environment)

    outputs = [build_dir / "esp-idf" / "lwip" / "liblwip.a"]
    outputs += [build_dir / EXTRA_ARCHIVES[name] for name in sorted(extra_archives)]

    say("ninja", "building only the archive(s) we need")
    for output in outputs:
        relative = output.relative_to(build_dir)
        run([ninja, "-C", build_dir, "-j", str(jobs), str(relative)], env=environment)
        if not output.is_file():
            die(
                f"{output} was not produced.\n"
                "  Ninja target names are stable in IDF 5.x; if this is not, fall back to a "
                "full `idf.py build` and copy build/esp-idf/lwip/liblwip.a."
            )
    return outputs, build_dir


def read_generated_config(build_dir, wanted):
    """Read back what the compiler was actually given, not what we asked for.

    The sdkconfig the build generated is the only honest source: a value that silently failed to apply would
    otherwise be discovered on hardware - as a window that did not grow, or a TLS allocator that did not move.
    Booleans are compared in both written forms, so an option we deliberately switched off has to appear as
    `# KEY is not set` rather than merely going missing.
    """
    generated = build_dir / "config" / "sdkconfig"
    if not generated.is_file():
        return {}
    text = generated.read_text(encoding="utf-8", errors="replace")
    actual = {}
    for key, expected in wanted.items():
        value = version_value(text, key)
        actual[key] = "n" if value is None else value
        want = "n" if expected is None else str(expected)
        if actual[key] != want:
            die(f"{generated} has {key} {actual[key]}, expected {want}")
        say("config", f"  verified in generated config: {key} {actual[key]}")
    return actual


# -----------------------------------------------------------------------------
# harvest
# -----------------------------------------------------------------------------

def harvest(outputs, target, libs_package, idf_version, commit, changes, toolchain_bin,
            local_version, generated, extra_archives):
    out_dir = SCRIPT_DIR / target
    out_dir.mkdir(parents=True, exist_ok=True)

    written = []
    for output in outputs:
        destination = out_dir / output.name
        shutil.copy2(output, destination)
        written.append(destination)
        say("harvest", f"{destination.relative_to(REPO_ROOT)}  {destination.stat().st_size:,} bytes")

    main_archive = out_dir / "liblwip.a"
    info = [
        "# generated by builds/patch/make_patch.py",
        "# read back by builds/patch/pre_patch.py to detect a stale patch",
        f"created = {datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ')}",
        f"target = {target}",
        f"idf_version = {idf_version} {commit}".strip(),
        f"libs_package = {libs_package.name}",
        f"sdkconfig_sha256 = {sha256_of(libs_package / 'sdkconfig')}",
        f"lwip_sha256 = {sha256_of(main_archive)}",
        f"lwip_bytes = {main_archive.stat().st_size}",
        "params = " + ", ".join(f"{key}={value}" for key, value in generated.items()),
        "changes = " + "; ".join(f"{key} {old} -> {new}" for key, old, new in changes),
        "generated = " + ", ".join(f"{key}={value}" for key, value in generated.items()),
        f"toolchain = {toolchain_bin.parent.name} {local_version}",
        f"host = {os.name}",
    ]
    if extra_archives:
        info.append("extra_archives = " + ", ".join(sorted(extra_archives)))
    (out_dir / "patch-info.txt").write_text("\n".join(info) + "\n", encoding="utf-8")
    say("harvest", f"wrote {(out_dir / 'patch-info.txt').relative_to(REPO_ROOT)}")
    return written


# -----------------------------------------------------------------------------
# main
# -----------------------------------------------------------------------------

def parse_args():
    parser = argparse.ArgumentParser(
        description="Rebuild ESP-IDF archives with a few sdkconfig values changed: liblwip.a always, and the three "
                    "mbedTLS archives unless --no-mbedtls. Every installed libs package is patched unless --target "
                    "narrows it.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("--target", action="append", dest="targets", metavar="MCU",
                        help="MCU to patch, e.g. esp32 or esp32s3; repeatable. "
                             "Default: every libs package that is installed")
    parser.add_argument("--wnd", type=int, default=WHITELIST["CONFIG_LWIP_TCP_WND_DEFAULT"],
                        help="CONFIG_LWIP_TCP_WND_DEFAULT (default: %(default)s)")
    parser.add_argument("--recvmbox", type=int, default=WHITELIST["CONFIG_LWIP_TCP_RECVMBOX_SIZE"],
                        help="CONFIG_LWIP_TCP_RECVMBOX_SIZE (default: %(default)s)")
    parser.add_argument("--sndbuf", type=int, default=WHITELIST["CONFIG_LWIP_TCP_SND_BUF_DEFAULT"],
                        help="CONFIG_LWIP_TCP_SND_BUF_DEFAULT (default: %(default)s)")
    parser.add_argument("--max-active", type=int, default=WHITELIST["CONFIG_LWIP_MAX_ACTIVE_TCP"],
                        help="CONFIG_LWIP_MAX_ACTIVE_TCP (default: %(default)s)")
    parser.add_argument("--packages-dir", help="PlatformIO packages directory")
    parser.add_argument("--idf-path", help="reuse this ESP-IDF checkout instead of cloning")
    parser.add_argument("--temp-dir", help="where the throwaway trees go (default: system temp)")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="ninja -j")
    parser.add_argument("--with-netif", action="store_true",
                        help="also rebuild libesp_netif.a (not needed for a window change)")
    parser.add_argument("--mbedtls", action="store_true", default=True,
                        help="also rebuild the three mbedTLS archives with MBEDTLS_EXTERNAL_MEM_ALLOC, so TLS "
                             "buffers come from PSRAM instead of the internal heap (default: on, because that is "
                             "what this codebase needs)")
    parser.add_argument("--no-mbedtls", action="store_false", dest="mbedtls",
                        help="build liblwip.a only.  Any mbedTLS archives already in the folder are left alone, "
                             "but a patch-info.txt rewritten this way stops naming them")
    parser.add_argument("--keep-temp", action="store_true",
                        help="keep the ESP-IDF clone and build tree for inspection")
    return parser.parse_args()


def main():
    args = parse_args()
    wanted = {
        "CONFIG_LWIP_TCP_WND_DEFAULT": args.wnd,
        "CONFIG_LWIP_TCP_RECVMBOX_SIZE": args.recvmbox,
        "CONFIG_LWIP_TCP_SND_BUF_DEFAULT": args.sndbuf,
        "CONFIG_LWIP_MAX_ACTIVE_TCP": args.max_active,
    }
    if args.mbedtls:
        wanted.update(MBEDTLS_WHITELIST)

    packages_dir = find_packages_dir(args.packages_dir)
    say("find", f"packages in {packages_dir}")

    # No --target means "everything installed", decided here because the packages dir has to be known first.
    targets = args.targets or detect_targets(packages_dir)
    say("find", f"targets: {', '.join(targets)}" + ("" if args.targets else " (auto-detected)"))

    resolved = []
    for target in targets:
        libs_package = libs_package_for(packages_dir, target)
        if libs_package is None:
            die(f"no framework-arduinoespressif32-{target}-libs in {packages_dir}")
        tag, commit = read_idf_version(libs_package)
        resolved.append((target, libs_package, tag, commit))
        say("find", f"{target}: ESP-IDF {tag} {commit}")

    if len({(tag, commit) for _, _, tag, commit in resolved}) > 1:
        die("targets disagree on the IDF version; run them in separate invocations")

    tag, commit = resolved[0][2], resolved[0][3]
    assert_parameters_sane(wanted)

    toolchain_bin = find_toolchain(packages_dir)
    toolchain_version = run(
        [toolchain_bin / ("xtensa-esp-elf-gcc.exe" if os.name == "nt" else "xtensa-esp-elf-gcc"),
         "-dumpfullversion"],
        capture=True,
    ).strip()
    say("tool", f"toolchain {toolchain_version} from {toolchain_bin.parent.name}")

    temp_candidate = args.temp_dir or tempfile.gettempdir()
    temp_root = Path(temp_candidate).resolve() / "ehradio-lwip"
    temp_root.mkdir(parents=True, exist_ok=True)
    say("temp", f"working in {temp_root}")

    try:
        idf_dir = ensure_idf(args.idf_path, tag, commit, temp_root)
        python = ensure_venv(temp_root / "venv", idf_dir)

        for target, libs_package, _, _ in resolved:
            say("config", f"{target}: base sdkconfig from {libs_package.name}")
            original = (libs_package / "sdkconfig").read_text(encoding="utf-8", errors="replace")
            patched, changes = patch_sdkconfig(original, wanted)
            assert_blacklist_untouched(original, patched)
            for key, old, new in changes:
                say("config", f"  {key}: {old} -> {new}")
            if not changes:
                say("config", "  nothing to change; this is a stock rebuild")

            environment = build_env(python, toolchain_bin, idf_dir, target)
            project_dir = temp_root / f"project-{target}"
            if project_dir.exists():
                rmtree(project_dir)
            scaffold_project(project_dir, patched)

            extra = {"esp_netif"} if args.with_netif else set()
            if args.mbedtls:
                extra |= {"mbedcrypto", "mbedx509", "mbedtls"}
            outputs, build_dir = build_archive(
                python, idf_dir, project_dir, environment, args.jobs, extra
            )
            generated = read_generated_config(build_dir, wanted)
            harvest(outputs, target, libs_package, tag, commit, changes, toolchain_bin,
                    toolchain_version, generated, extra)

        print(
            "\nDone. The build now links these archives instead of the package's own; "
            "check the next build log for `LWIP patch linked: ...`.\n"
            "Verify with:\n"
            "  pio run -e ili9488_pcm_1button_full\n"
        )
    finally:
        if args.keep_temp:
            say("temp", f"kept {temp_root}")
        else:
            rmtree(temp_root)
            say("temp", "removed the throwaway tree")


if __name__ == "__main__":
    main()
