# Firmware Build Tool

## `prepare_releases.py`

This script proves the tree is releasable and then generates the release metadata. It scans every contributor subfolder under `builds/` for `myoptions.h`, validates the configs, **builds every firmware variant**, and only then merges the results into a single set of outputs under `builds/releases/`.

Nothing under `releases/` is written until the build has passed, so a tree that does not compile can never produce release metadata that looks finished.

### Files Generated

- `releases/firmware.txt`: Machine-readable list of all firmware variants
- `releases/web_assets/manifests/*.json`: ESP Web Tools manifest files for each firmware variant
- `releases/web_assets/firmware-info.json`: Summary file for the web flashers (`docs/flasher.html` / `docs/firmware.html`)

### File Patched

- `releases/releases.md`: Human-readable release notes for GitHub releases

### Usage

```bash
python builds/prepare_releases.py             # validate, build every variant, then generate
python builds/prepare_releases.py --yes       # same, without the confirmation prompt (CI)
python builds/prepare_releases.py --check     # validate only: no build, no writes
python builds/prepare_releases.py --no-build  # skip the build (release workflows only)
python builds/prepare_releases.py --verbose   # stream compiler output instead of capturing it
```

The default run asks `Are you ready to compile all builds? [y/N]` before it starts compiling. With no terminal and no `--yes` it cancels rather than hanging, which is what CI relies on.

**`--check`** runs the two validation phases and stops: contributor naming/filesystem rules plus preprocessor balance. It writes nothing and compiles nothing, so it is the fast gate used by `add_builds.py` STAGE 0 and the `Config check` PR job.

**`--no-build`** skips only the compile. The metadata is still validated and regenerated, so this cannot produce a false pass: the `Build all firmware` job on the PR into `main` runs the script *without* this flag, and compiles everything regardless of what was done locally. It exists so the two release workflows, whose next step compiles the same environments, do not build them twice.

The script runs four phases, stopping at the first failure:
1. **Naming**: scan all immediate subfolders of `builds/` (e.g. `trip5/`, `no_display/`) for a `myoptions.h`, and check every `FIRMWARE` name and filesystem type (see the rules below)
2. **Configs**: check preprocessor balance in every contributor `myoptions.h` - one `#if`/`#ifdef`/`#ifndef` per `#endif`. A leftover `#endif` closes the include guard early, and because `options_overrides.h` includes that file in every translation unit, a single orphan line fails the entire build
3. **Build**: copy the working tree into `.build_all/`, install each contributor's `platformio.ini` + `myoptions.h` there, and run `pio run -e <env>` for every `[env:...]` it declares. The scratch copy is deleted afterwards, so the live root config, `use_build.py`'s lock files and the VS Code File Watcher are never touched. Build logs go to a temp directory, kept only when something fails
4. **Generate**: parse each `myoptions.h` for firmware definitions (commented-out `#define` lines are ignored), skip duplicate `fw_env` names with a `Duplicate!` warning, then write `releases/firmware.txt` in the format `board_env|chip_family|fw_env|friendly_name`, generate the ESP Web Tools manifests in `releases/web_assets/manifests/`, generate `releases/web_assets/firmware-info.json` sorted alphabetically by name, and patch `releases/releases.md` with the per-contributor listings

The `releases/` subfolder itself is always skipped. Folders with no `myoptions.h`, incomplete firmware definitions, or names containing spaces/special characters are silently skipped.

### Requirements

Each contributor's `myoptions.h` must have firmware definitions in this format:

```cpp
#elif defined(ESP32_S3_YOURBOARD)
  #define FIRMWARE "yourusername_your_board.bin" // "esp32_s3_n16r8", "ESP32-S3", "YourUserName"
  #define FIRMWARE_NAME "Your Friendly Name" // "https://optional-url.com"
  #define ARDUINO_ESP32S3_DEV
```

The comments after `FIRMWARE` are critical:
- First field: board environment from `platformio.ini` (e.g., `"esp32_s3_n16r8"`)
- Second field: chip family (e.g., `"ESP32-S3"`, `"ESP32"`, `"ESP32-C3"`)
- Third field: contributor name, which must agree with the contributor folder (e.g., `"Trip5"`, `"Kasperaitis"`)

### Naming rules (enforced)

`fw_env` is the FIRMWARE filename minus `.bin` and is used verbatim for the release asset, the manifest filename, and `firmware-info.json`. The release workflows derive every asset name as `<contributor folder>_<env>.bin`, so:

1. `FIRMWARE` must be `"<contributor folder>_<env>.bin"`, where `<env>` is an `[env:...]` name declared in that folder's `platformio.ini`.
2. The contributor field must match that folder name, so the credit and the asset name cannot drift apart.

Comparison ignores case and treats spaces, hyphens and underscores as equivalent, so `"No Display"`/`no_display` and `"Trip5"`/`trip5` are accepted. A define that breaks either rule still compiles, but the binary is never copied into the release — `prepare_releases.py` rejects it in both modes, and that check runs in the PR workflow and in STAGE 0 of `add_builds.py`.

Entries whose filename starts with `board_` are skipped (bare board binaries, not full firmware).

### `releases.md` patching behavior

- Existing `### X Firmware` sections: entries are replaced inline; if X has no firmware data the section is removed entirely
- New contributors not yet in `releases.md`: a new section is inserted before `### Add Yours`, sorted alphabetically
- Entry format: `` [`fw_env.bin`](url) `` if a URL is provided in the `FIRMWARE_NAME` comment, else `` `fw_env.bin` ``

### `firmware-info.json` naming

Variant names are formatted as `Contributor FriendlyName` (e.g. `Trip5 Color Screen with 1-Button`) and sorted alphabetically, so contributors with the same friendly name (e.g. `ES3C28P`) remain distinct.

### Adding a New Contributor

1. Create a subfolder under `builds/` using your GitHub username (letters, digits, `_`, `-` only)
2. Add a `myoptions.h` with your firmware definitions following the format above, naming every `FIRMWARE` as `<folder>_<env>.bin`
3. Ensure `releases/releases.md` exists with a `### YourName Firmware` section (or let the script add one automatically before `### Add Yours`)
4. Run `python builds/prepare_releases.py --check` to confirm the naming and config rules pass
5. Run `python builds/prepare_releases.py` — it builds your firmware, then merges your entries into `releases/`. Commit the regenerated files

### Automation

Three CI touch points, all running this one script:

- `pr-check.yml` **Config check** (`--check`) on PRs into `dev` and `main`: naming, filesystem and preprocessor balance. Seconds, no compiler.
- `pr-check.yml` **Build all firmware** on PRs into `main`: the full run, every contributor's every environment, then a `git status` check on `builds/releases/`. A folder that no longer compiles, or metadata that was not regenerated, blocks the merge to `main`.
- The two release workflows run it with `--no-build` in their verify step, because their next step compiles the same environments, and fail if `git status` shows any change under `builds/releases/`.
