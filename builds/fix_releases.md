# Firmware Build Tool

## `fix_releases.py`

This script automatically generates firmware metadata and web assets by scanning all contributor subfolders under `builds/` for `myoptions.h` files and merging them into a single set of outputs under `builds/releases/`.

### Files Generated

- `releases/firmware.txt`: Machine-readable list of all firmware variants
- `releases/web_assets/manifests/*.json`: ESP Web Tools manifest files for each firmware variant
- `releases/web_assets/firmware-info.json`: Summary file for the web flashers (`docs/flasher.html` / `docs/firmware.html`)

### File Patched

- `releases/releases.md`: Human-readable release notes for GitHub releases

### Usage

Run the script from the `builds/` folder:

```bash
cd builds/
python fix_releases.py
```

To validate contributor naming only (writes nothing, exits non-zero on problems):

```bash
python builds/fix_releases.py --check
```

The script will:
1. Scan all immediate subfolders of `builds/` (e.g. `trip5/`, `kasperaitis/`) for a `myoptions.h`
2. Parse each `myoptions.h` to find firmware definitions (commented-out `#define` lines are ignored)
3. Skip duplicate `fw_env` names with a `Duplicate!` warning
4. Generate `releases/firmware.txt` in the format: `board_env|chip_family|fw_env|friendly_name`
5. Generate ESP Web Tools manifests for each firmware in `releases/web_assets/manifests/`
6. Generate `releases/web_assets/firmware-info.json` with all variant information, sorted alphabetically by name
7. Patch `releases/releases.md` with organized firmware listings per contributor

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

Comparison ignores case and treats spaces, hyphens and underscores as equivalent, so `"No Display"`/`no_display` and `"Trip5"`/`trip5` are accepted. A define that breaks either rule still compiles, but the binary is never copied into the release — `fix_releases.py` rejects it both when generating and with `--check`, and that check runs in the PR workflow and in STAGE 0 of `add-builds.py`.

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
4. Run `python builds/fix_releases.py --check` to confirm the naming rules pass
5. Run the script from `builds/` — your entries will be merged into `releases/`

### Automation

The GitHub workflow runs this script during the release process and diffs the output against the committed `builds/releases/` files. If anything has changed, the workflow fails — you must run the script locally, commit the updated files, and push before releasing.
