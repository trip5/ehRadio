# ehRadio AI Coding Guide

## Note

- **This file is a short rule sheet, not documentation.** Edit it only when the message you are working on asks for it (Rule #3), and keep any addition to a line or two: the detail belongs in `.github/code-summary.md`.
- If you wish to NOT use this file, feel free to add it to your own private exclusions (`.git/info/exclude`).  Please do not try to sync your own version of this file back in a PR to `ehRadio:dev`.
- **code-summary.md**: The ehRadio `Bible` may be hard for a human to read but provides a good (if lengthy) summary of the codebase to be more easily digested by an AI / LM Copilot.

## About

- **ehRadio**: A fork of [yoRadio](https://github.com/e2002/yoradio) with added Web UI functionality, improved Web UI, usability, and customization.
- **Arduino**: Trip5 hosts firmware files, flashing tools, and Releases on the [Github Repo](https://github.com/trip5/ehRadio) and [Github Page](https://trip5.github.io/ehRadio/)
- **Online Flasher**: Releases are also available through the [ESP Web Tool](https://trip5.github.io/ehRadio/firmware.html)
- **Online Updating**: Files on a running device are updated using ESPFileUpdater from the Releases on the Github Repo.

## ⚠️ Critical Rules

> **These rules are HARD CONSTRAINTS, not guidelines. Violating any rule is an error.**
> Before making ANY edit, running ANY command or suggesting code, mentally validate against ALL 9 rules. If any rule would be violated, **STOP** and inform the user. When in doubt, ASK.

**Rule #0**: **Never Commit or Touch the Remote Without Explicit Permission**
- Do **NOT** run `git push` in any form (including `--force`), create or edit a **release**, **tag**, **pull request** or issue, or delete a remote branch, unless the user has explicitly asked for **that** action in the message you are working on.
- **Do NOT commit either - not even locally - unless the user asks for a commit in the message you are working on.** The user reviews every change as a diff before it becomes a commit, and a commit takes the change out of that review: VS Code lists committed-but-unpushed work as read-only *Outgoing Changes*, which cannot be edited where the user found it. A pause on committing is not lifted by finishing a stage, by a stage boundary, or by a plan that says to commit per stage.
- Permission is per action and never inherited: a push mentioned while planning, or "we will push this later", is not permission, and permission for one push is not permission for the next. The repo is public and a push is not quietly undone - say what you would push and to which branch, then **wait**.
- **When the user asks you to commit, do it the way this repo does**: the subject is the date, `YYYY.MM.DD` (`2026.10.06`), with **no body**. The why belongs in `code-summary.md` (Rule #4) and the plan file (Rule #7), not in the commit.
- **A local commit is not needed to undo a mistake, so it is never a valid reason to commit.** Use `git stash create` instead: it prints a SHA that snapshots the tracked tree **without touching the index or the files**, so an invasive step can be rolled back with `git checkout <sha> -- <path>` while history stays clean. Say that a snapshot is being taken and report the SHA.
- This rule outranks every rule below it. When an instruction is ambiguous about the remote, **STOP** and ask.

**Rule #1**: **Plan Mode for Large Changes**
- For changes spanning **more than 50 lines** (summed across ALL files in the change), you **MUST STOP** and inform the user:  
  > "This change is large. Please switch to Plan mode so we can review a plan before making edits."  
- Only proceed if the user explicitly confirms or switches to Plan mode.
- In Plan mode, once the user states "start implementation" (or equivalent), **implement exactly as agreed**. Do **NOT re-confirm or reinterpret** the scope — follow the finalized plan.
- If you are uncertain about the total line count, **ASK** before proceeding.

**Rule #2**: **One File/Simple Changes Exception**
- This is the **ONLY** exception to Rule #1. There is no "small multi-file" loophole. You must satisfy **ALL 3 points**:
  - Changes are **less than or equal to 50 lines** (summed across all files).
  - The changes affect **only one file or one logical set of files** (e.g., a `.cpp` file and its matching `.h` file).
  - The user has given **explicit instructions** on what to modify.
- **Self-check**: Before every edit, ask yourself: *"Am I touching exactly one file (or one .cpp+.h pair)?"* If not, Plan mode is required regardless of line count.

**Rule #3**: **Restricted Files**
- Do **NOT** edit or even **suggest** edits to the following files unless the user provides **explicit confirmation**:
  - `myoptions.h`
  - `src/core/options.h`
  - `platformio.ini`
- **`.github/ai-coding-guide.md` (this file) is a short rule sheet and is edited only when the message you are working on asks for it.** It is not a place for explanation, history or subsystem detail - that is what `code-summary.md` is for, and one or two lines is the whole budget for a new rule.
- **`.github/code-issues.md` records open issues that need investigation or are blocked** (for example, waiting on hardware the maintainer does not own). It is not a changelog: never add, edit or remove an entry unless asked. Do read it before editing an affected area, and say so if your change fixes or alters a described entry.

**Rule #4**: **Code Interaction Documentation**
- If your change affects **code interactions**, external APIs, storage keys, or external contracts:
  - Ensure `.github/code-summary.md` is updated in the **same change set**.
- **Bug fixes** do not require updates unless they modify external contracts.

**Rule #5**: **Pre-edit Research Required**
- For any non-trivial change (beyond an isolated one-line fix), first determine whether it touches the firmware:
  - **Firmware files** = any `*.c`, `*.cpp`, `*.h` file anywhere in the repo, plus anything under `src/`, `libraries/`, or `data/`.
  - **If the change touches ANY firmware file**: You **MUST READ** `.github/code-summary.md` before writing code. Also check `.github/code-issues.md` for known issues in the affected area.
  - **Search `plans/` for a plan covering the area**, whatever kind of file it touches. A previous plan carries the exact detail - the numbers, the API quirk, the ordering rule - that `code-summary.md` deliberately does not (Rule #7).
  - **If the change touches ONLY non-firmware files** (workflows, docs, build scripts, config generators, Home Assistant, images, etc.): `code-summary.md` review is **NOT** required. Still check `code-issues.md` if relevant.
  - **One-line/trivial fixes** (typos, formatting) are exempt regardless of file type.

**Rule #6**: **Comments State Facts, Briefly**
- **`//` is the default comment.** A block gets a 1-3 line summary above it; a call gets a short trailing `//` when its name does not already say what happened.
- **`/* */` has exactly three uses**: a section TITLE (`/* ==== Upload ==== */`), a header saying what the next long block does, and code kept but not used (tested and switched off).
- **The variable name documents itself.** Most declarations need no comment at all, and the ones that do want a trailing note rather than a block above them.
- **A `.h` file almost never needs a comment**, especially where the `.cpp` already says the same thing: the declaration and its implementation are read together, so a second copy is noise that can drift.
- **No narration, no history, no measurements.** Why something is the way it is belongs in `.github/code-summary.md`; what was measured, what was tried and what was rejected belong in the plan file for that change (Rule #7). If it is not in the code, it does not belong in the summary.
- **Never document what is not in the code.** Deleted code is not commented about, and a replaced mechanism is not described; a retired mechanism worth not repeating gets at most two lines - what was tried, and the result. If the name says it, delete the comment.
- **Three lines is the ceiling.** A comment that runs past three lines is either a reason for `code-summary.md` or four shorter comments.
- **Example** - this:
  ```c
  /* PSRAM usage tracking - set by subsystems, consumed by Core Monitor */
  size_t psramFrameBufferBytes = 0;
  ```
  is this:
  ```c
  size_t psramFrameBufferBytes = 0;   // set by subsystems, read by Core Monitor
  ```
  and a twenty-line account of a failed experiment is one line here plus the account in `code-summary.md`:
  ```c
  // Sector alignment was tried and failed: the unit that matters is the CLUSTER, not the sector.
  ```

**Rule #7**: **Plan Files Are Working Memory**
- A conversation can be compacted; `plans/` cannot. For any change large enough to need a plan (Rule #1), write it to `plans/<topic>.md` **before** the first edit, and keep it updated **as you implement** - not afterwards.
- Record what the code actually turned out to be, not only what was intended: the symptom that started it, the options rejected and why, the measurements, the traps, the files and line anchors. That is the detail which will not fit in a comment (Rule #6) and does not belong in `code-summary.md` (Rule #4).
- Where the implementation proves the plan wrong, **correct the plan**. A plan still describing the design that was intended, after the code went another way, is worse than no plan - the next session will trust it. Corrections are dated; history is not rewritten.
- Mark the plan implemented when it is, with the state of the verification (built, flashed, retested by the user) and what remains open.
- The split is: **comments** say what the code does, beside it; **`code-summary.md`** says what the code *is*; **`plans/`** says why this change went the way it did and what was tried. Do not duplicate - link.
- **Check `plans/` during any code inquiry** - before investigating, answering or proposing - and read the plan that covers the area first. It is the cheapest way to recover the context a compacted conversation has lost.
- A change that qualifies under Rule #2 needs no plan file.

**Rule #8**: **Localization Files and Tools Are the Maintainer's**
- Do **NOT** edit any `src/locale/**/*.json` file, do **NOT** run any localization tool (`www_tool.py`,
  `display_tool.py`, `make_wwwlocale.py`, `make_dsplocale.py`, `locale_js_to_h.py`), and do **NOT** touch the
  generated `wwwlocale.h`, `dsplocale.h` or `locale_js.h`.
- **WebUI strings** (what a `data/www/*` page shows): put the key and its English in the **HTML/JS only** -
  `t('key', 'English')`, or `data-i18n="key"` with the English standing in the markup. The page falls back to that
  English, so the work is complete and testable without any locale edit. The maintainer runs
  `www_tool.py * --translate --fast --clean --sort --newkeys` from `src/locale` when it suits them; it scans the
  sources and fills the JSONs. Never write English into the JSONs as a placeholder - it is indistinguishable from a
  finished translation and has to be found and redone.
- **Display strings** (anything the panel shows through `l10n(...)`): a new one needs a key in the display master
  `src/locale/display/en_US.json`, the translations, and a rebuilt `dsplocale.h` **before the firmware compiles** -
  so the sequence is deliberate, not incidental:
  1. Propose the key name, the exact string, and **where it should sit in `en_US.json`** - that file is ordered the
     maintainer's way and not alphabetically - then **STOP and wait for confirmation**.
  2. Once confirmed, add it to `en_US.json` and say so, so the maintainer can reword it or move it.
  3. **Confirm again before running the tools**: `display_tool.py * --translate --fast --clean --sort --newkeys`
     from `src/locale` (it can take a while, and `--newkeys` writes the file for outside collaborators), then
     `make_dsplocale.py` to rebuild `dsplocale.h`.
- **Never hardcode a display string.** Code refers to one only as `l10n(L10N_XXX)`.
- When a change looks like it needs any locale edit or tool run, **STOP and ask** - for both toolchains.

**Enforcement and AI Behavior**
- Always validate proposed changes against these rules **before every action**.
- **Rule #0 is absolute, and it covers the local history as well as the remote.** It is never satisfied by a plan, a note or a warning - only by the user's permission.
- For Rule #1 and Rule #2 violations: Explain which rule is violated and wait for user confirmation.
- Any violations of Rules #2–#6 and #8 **must be flagged explicitly** to the user before edits.
- **Pre-action checklist** — mentally answer before writing code:
  0. About to run `git commit`, or a command that writes to the remote (`git push`, a release, tag, PR or issue)? → **STOP and ask first** (Rule #0)
  1. Total lines across all files > 50? → Plan mode required (Rule #1)
  2. More than 1 file (not a .cpp/.h pair)? → Plan mode required (Rule #2)
  3. Touching `myoptions.h`, `options.h`, `platformio.ini`, this guide, or `code-issues.md`? → Get explicit confirmation / a request in this message (Rule #3)
  4. Affecting external contracts/APIs/storage keys? → Update `code-summary.md` (Rule #4)
  5. Touching any firmware file (`*.c`, `*.cpp`, `*.h`, `*.ino`, `src/`, `libraries/`, `data/`)? → Read `code-summary.md` first, and search `plans/` for the area (Rule #5)
  6. Writing a comment longer than three lines? → It belongs in `code-summary.md`, or it is four shorter comments (Rule #6)
  7. Planning a change that needs more than a one-file edit? → Write it to `plans/` first, and keep it current as the code lands (Rule #7)
  8. Adding, renaming or removing a command in `commandhandler.cpp`? → `Commands.md` gets the entry in the **same block and the same position**, and its *Blocked in HTTP/MQTT/Telnet* column has to match `isBlockedForSource()` and nothing else. The file's order is its value: an audit is a walk down the two lists. The `CONFIG_KEY_ENTRY` names in `config.cpp` are store keys, not commands
  9. Adding a field to `theme_t`? → it lands in **four** places, or the build or the tool is quietly wrong: `theme_t` in `config.h`, `ThemeData` in `themes.h` (the two are `memcpy_P`'d, so their shapes must match exactly), every hand-overridden reduced palette (`displays/tools/oledcolorfix.h`, `displaySSD1322.cpp`), and **`importtheme.py`** - `FIELD_ORDER` plus its derivation rule, with the matching rows in `importtheme.md`. A field missing from `FIELD_ORDER` is dropped from every imported theme without a word. A new *screensaver* colour belongs in the "derived" list, not the computed one, if a stated rule produces it
  10. Patching `I2S_Audio` or `VS1053_Audio`? → `src/libraries/i2s-frankenstein-surgery-notes.md` or `src/libraries/vs1053-frankenstein-surgery-notes.md` - the matching one, not both - gets a new `## Post-Graft Patch Fixes (YYYY.MM.DD)` section: the file changed, and why. Same change set.
  11. About to touch a `src/locale/**/*.json` file, or run a localization tool? → **STOP**; the maintainer owns both (Rule #8)

## Project Structure

- **Config Cascade**: `platformio.ini` (env `#define`) → `myoptions.h` (hardware profile, user defaults) → `options.h` (fallback for anything undefined). Libraries never see it; the few values they must agree with us about live in `src/core/options_overrides.h`, which `platformio.ini` force-includes into every TU.
- **Core logic**: `src/core/` (Player, Display, Network, Config, Controls).
- **UI**: widgets in `src/displays/widgets/`, drivers in `src/displays/`.
- **Libraries**: software codecs `libraries/I2S_Audio/`, `libraries/ES8311_Audio`; hardware decoder `libraries/VS1053_Audio/`; the other folders are drivers for other displays, touchscreens and hardware.
- **Plugins**: the yoRadio plugin hook system is gone - add behaviour in the owning core module instead.
- **Web UI**: most files in `data/www` are served with headers in `src/core/netserver.h`; `search.html` and `curated.html` are not.
- **WebUI palette**: every colour is a custom property in the `:root` block of `data/www/style.css`, and a theme is one `:root[data-theme="N"]` block there (`N` = the id in `themes.h`; `/variables.js` publishes it). Never a second stylesheet, and never a colour literal in a rule. The 62 declarations are grouped in nine families whose head holds one literal (`--surfaces`, `--edges`, `--fills`, `--outlines`, `--highlights`, `--actives`, `--labels`, `--infotexts`, `--maintexts`); a head is painted by no rule, but a theme block may declare one to move the family in a single line. 53 of the declarations are roles: the generator derives 50 of them, and `--page-bg` (the window around the app column), `--list-bars` and `--playlist-hover` are hand-owned - no display theme supplies them - though a block may set any of the three. Blocks come from `themes.h` via `src/displays/webuitheme.py` (`--inflate`/`--deflate`/`--update`); the family lists, the split rules and the tool are in `code-summary.md`.
- **`dsplocale.h` defines its tables inside the header**, so every TU that includes it embeds another copy (~48 KB). Never add that include to a new file: call `l10n(...)` from one that already has it, or expose an accessor the way `Player::isConnecting()` does.

## Functionality

- **Hardware**: the build is defined by the Config Cascade, not by `#if` scattered through the code. `#if defined` / `#ifndef` are for hardware only, never for configuration.
- **Software**: extend behaviour through `options.h` and the Cascade; they exist to configure it, not to limit it.
- **Granular Control in Web UI**: anything not hardware-related should be changeable in the Web UI rather than by a `#define`.

## Force-Included Config (`src/core/options_overrides.h`)

- `[ehradio] build_flags` passes `-include src/core/options_overrides.h`, so it is compiled first in **every** TU - ours, every managed library and the framework's own `.c` files.
- **Never force-include `options.h`.** It includes `SPI.h` (which a library TU cannot find) and uses C++-only `static_assert`; this header holds the few library-visible values instead.
- **Keep it dependency-free**: preprocessor only - no includes, no types, no C++ syntax. C compatibility is a hard requirement.
- **`myoptions.h` stays the user channel**: the header includes it first, so a user value beats every default here. That holds only in builds carrying the force-include (root `platformio.ini`, `builds/trip5/platformio.ini`).
- **The `NETWORK_CORE` tree is duplicated here and in `options.h` on purpose**, and both copies must survive being evaluated twice. Keep them identical.
- **A value change here may not rebuild libraries**: use `pio run -t clean -e <env>` when it must take effect.
- **Run one `pio run` at a time.** The post-build step gzips `data/www` and two builds sharing it race into a spurious `FAILED`; re-run that environment alone before believing one.
- **A long card operation reachable from a web handler must feed the watchdog** (`sdFeedWatchdog()`), because handlers run in AsyncTCP's task, which is subscribed (`CONFIG_ASYNC_TCP_USE_WDT 1`).

## File Handles and Cross-Task State

- **A `File` has exactly one owning task.** Never close or reassign a handle another task may use: the failure is not a compile error but `assert failed: lfs_file_seek lfs.c:NNNN (lfs_mlist_isopen(...))`. Do not cache a `File` across callbacks - open, use and close it inside the one call; if a second task touches the same files, take a mutex.
- **Never do flash or SD I/O inside a critical section** (`portENTER_CRITICAL`). Use a FreeRTOS mutex: a critical section disables interrupts, and a flash write already runs with the cache disabled.
- **One long operation at a time, and say so.** A request that finds another in flight is refused with 409 plus a reason rather than sharing the state - a shared handle or layout is what let two requests of the same kind corrupt each other.
- **A subsystem that owns a filesystem must be shut down before that filesystem is erased.** `LittleFS.format()` takes every open handle with it.
- **State shared with another task belongs to that task**: the web handler stages it and queues a request, and the owning task applies it without re-reading the live source over the top of the staging area.
- **Coalescing repeated requests belongs in `Display::putRequest()`**, not in the caller - it is the only place that knows whether the send landed.

## Logging and Recovery

- **Only `FUNCTIONLOG` takes a category.** `SERIALLOG`, `SERIALLOGX`, `BOOTLOG`, `BOOTLOGX` and `ERRORLOG` are `#define X(fmt, ...)`, so passing a category passes it as the format and throws the message away. Copy the shape of the macro you are calling.
- **`ERRORLOG` is for a loss** - a failure with nothing recoverable and no partial kept. A fully recovered failure is an ordinary `FUNCTIONLOG` line, or a batch of them reads as a list of dead files.
- **A recovery rung must not depend on an allocation**, and every ceiling needs a rung above it or a written reason why the remaining retries help. When the last rung is a restart, it must not set `bootStableMarker` and must be counted in RTC memory.
