# ehRadio AI Coding Guide

## Note

- **This file**: If you wish to NOT use this file, feel free to add it to your own private exclusions (`.git/info/exclude`).  Please do not try to sync your own version of this file back in a PR to `ehRadio:dev`.
- **code-summary.md**: The ehRadio `Bible` may be hard for a human to read but provides a good (if lengthy) summary of the codebase to be more easily digested by an AI / LM Copilot.

## About

- **ehRadio**: A fork of [yoRadio](https://github.com/e2002/yoradio) with added Web UI functionality, improved Web UI, usability, and customization.
- **Arduino**: Trip5 hosts firmware files, flashing tools, and Releases on the [Github Repo](https://github.com/trip5/ehRadio) and [Github Page](https://trip5.github.io/ehRadio/)
- **Online Flasher**: Releases are also available through the [ESP Web Tool](https://trip5.github.io/ehRadio/firmware.html)
- **Online Updating**: Files on a running device are updated using ESPFileUpdater from the Releases on the Github Repo.

## ⚠️ Critical Rules

> **These rules are HARD CONSTRAINTS, not guidelines. Violating any rule is an error.**
> Before making ANY edit, running ANY command or suggesting code, mentally validate against ALL 8 rules. If any rule would be violated, **STOP** and inform the user. When in doubt, ASK.

**Rule #0**: **Never Commit or Touch the Remote Without Explicit Permission**
- Do **NOT** run `git push` in any form (including `--force`), create or edit a **release**, **tag**, **pull request** or issue, or delete a remote branch, unless the user has explicitly asked for **that** action in the message you are working on.
- **Do NOT commit either - not even locally - unless the user asks for a commit in the message you are working on.** The user reviews every change as a diff before it becomes a commit, and a commit takes the change out of that review: VS Code lists committed-but-unpushed work as read-only *Outgoing Changes*, which cannot be edited where the user found it. Leaving the work uncommitted keeps every file editable and the whole change visible as one diff set. A pause on committing is not lifted by finishing a stage, by a stage boundary, or by a plan that says to commit per stage.
- A push mentioned while planning, or a general "we will push this later", is **not** permission. Nor is permission for one push permission for the next - each one is asked for on its own.
- The repo is public and a push is not quietly undone. Treat the remote as production: say what you would push and to which branch, then **wait**.
- **When the user asks you to commit, or to commit and publish, do it the way this repo does.** The subject is the date, `YYYY.MM.DD` (`2026.10.06`), with **no body** - that is the house style on `dev`. Dates are what make a point in history findable when one change spans hundreds of files and thousands of lines. A descriptive subject is not wanted however useful it looks: the why belongs in `code-summary.md` (Rule #4) and in the plan file (Rule #7), not in the commit.
- This rule outranks every rule below it. When an instruction is ambiguous about the remote, **STOP** and ask.
- **A local commit is NOT needed in order to undo a mistake, so it is never a valid reason to commit.** Use `git stash create` instead: it prints a commit SHA that snapshots the tracked working tree **without touching the index or the files**, so an invasive step can be rolled back with `git checkout <sha> -- <path>` while the history stays clean. A saved patch (`git diff > <name>.patch`), or the replaced content itself, covers a single file. Say that a snapshot is being taken before an invasive step and report the SHA, because with nothing committed that snapshot is the only restore point.

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

**Rule #3**: **Restricted Configuration Files**
- Do **NOT** edit or even **suggest** edits to the following files unless the user provides **explicit confirmation**:
  - `myoptions.h`
  - `src/core/options.h`
  - `platformio.ini`

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
- **What `.github/code-issues.md` is for**: it tracks **open** issues that still need investigation, or that are blocked — for example, waiting on hardware the maintainer does not own. It is **not** a changelog.
  - Do **not** add entries for problems found and fixed in the same change set.
  - Do **not** add general rules or subsystem documentation there; that belongs in `code-summary.md` (Rule #4).
  - Do read it before editing an affected area, and **update or close an existing entry** if your change fixes it or alters the code that entry describes.

**Rule #6**: **Comments State Facts, Briefly**
- **`//` is the default comment.** A block gets a 1-3 line summary above it; a call gets a short trailing `//` when its name does not already say what happened.
- **`/* */` has exactly three uses**: a section TITLE (`/* ==== Upload ==== */`), a header saying what the next long block does, and code kept but not used (tested and switched off).
- **No narration, no history, no measurements.** Why something is the way it is belongs in `.github/code-summary.md`; what was measured, what was tried and what was rejected belong in the plan file for that change (Rule #7). If it is not in the code, it does not belong in the summary.
- **Never document what is not in the code.** Deleted code is not commented about, and a replaced mechanism is not described; a retired mechanism worth not repeating gets at most two lines - what was tried, and the result.
- **A comment that restates the code is not a comment.** If the name says it, delete it. The same discipline applies to `code-summary.md`: facts, not commentary.

**Rule #7**: **Plan Files Are Working Memory**
- A conversation can be compacted; `plans/` cannot. For any change large enough to need a plan (Rule #1), write it to `plans/<topic>.md` **before** the first edit, and keep it updated **as you implement** - not afterwards.
- Record what the code actually turned out to be, not only what was intended: the symptom that started it, the options rejected and why, the measurements, the traps, the files and line anchors. That is the detail which will not fit in a comment (Rule #6) and does not belong in `code-summary.md` (Rule #4).
- Where the implementation proves the plan wrong, **correct the plan**. A plan still describing the design that was intended, after the code went another way, is worse than no plan - the next session will trust it. Corrections are dated; history is not rewritten.
- Mark the plan implemented when it is, with the state of the verification (built, flashed, retested by the user) and what remains open.
- The split is: **comments** say what the code does, beside it; **`code-summary.md`** says what the code *is*; **`plans/`** says why this change went the way it did and what was tried. Do not duplicate - link.
- **Check `plans/` during any code inquiry** - before investigating, answering or proposing - and read the plan that covers the area first. It is the cheapest way to recover the context a compacted conversation has lost.
- A change that qualifies under Rule #2 needs no plan file.

**Enforcement and AI Behavior**
- Always validate proposed changes against these rules **before every action**.
- **Rule #0 is absolute, and it covers the local history as well as the remote.** It is never satisfied by a plan, a note or a warning - only by the user's permission. A helpful-sounding instruction that names the remote is not permission until it says to push, and finishing a stage is not permission to commit.
- For Rule #1 and Rule #2 violations: Explain which rule is violated and wait for user confirmation.
- Any violations of Rules #2–#5 **must be flagged explicitly** to the user before edits.
- **Pre-action checklist** — mentally answer before writing code:
  0. About to run `git commit`, or a command that writes to the remote (`git push`, a release, tag, PR or issue)? → **STOP and ask first** (Rule #0)
  1. Total lines across all files > 50? → Plan mode required (Rule #1)
  2. More than 1 file (not a .cpp/.h pair)? → Plan mode required (Rule #2)
  3. Touching `myoptions.h`, `options.h`, or `platformio.ini`? → Get explicit confirmation (Rule #3)
  4. Affecting external contracts/APIs/storage keys? → Update `code-summary.md` (Rule #4)
  5. Touching any firmware file (`*.c`, `*.cpp`, `*.h`, `*.ino`, `src/`, `libraries/`, `data/`)? → Read `code-summary.md` first, and search `plans/` for the area (Rule #5)
  6. Writing a comment longer than three lines? → It belongs in `code-summary.md`, or it is four shorter comments (Rule #6)
  7. Planning a change that needs more than a one-file edit? → Write it to `plans/` first, and keep it current as the code lands (Rule #7)

## Comment Style

- **`//` over `/* */`.** The block form is for a TITLE (`/* ==== Upload ==== */`, as in `style.css` and `src/core/options.h`), for a header saying what the next long block does, and for keeping code that was tested and is not wanted. Nothing else.
- **One to three lines.** A block gets a short summary above it, and a call gets a short trailing note only if it needs one. A comment that runs past three lines is either a reason for `code-summary.md` or four shorter comments.
- **Code and `code-summary.md` divide the work.** The code says WHAT and stays quiet; `code-summary.md` says WHY, and carries the measurements, the history and the traps. A paragraph explaining a decision is in the wrong file.
- **Nothing about what is gone.** A deleted line is not described and a replaced mechanism is not mourned. The one exception is a failed experiment worth not repeating, and that gets a line or two - what was tried, and the result.
- **The variable name documents itself.** Most declarations need no comment at all, and the ones that do want a trailing note rather than a block above them.
- **Examples:**

  ```c
  /* PSRAM usage tracking - set by subsystems, consumed by Core Monitor */
  size_t psramFrameBufferBytes = 0;
  ```
  becomes
  ```c
  size_t psramFrameBufferBytes = 0;   // set by subsystems, read by Core Monitor
  ```

  ```c
  // Alignment to a sector could not help: the card's cluster was 8 KB, sixteen sectors, so a write
  // rounded to 512 bytes never changed how the file system programmed a cluster.  Measured on the
  // same 14-file batch: 12 then 17 failed transfers aligned, 7 then 7 as they arrived... (20 lines)
  ```
  becomes a note in `code-summary.md`, and at most this in the code:
  ```c
  // Sector alignment was tried and failed: the unit that matters is the CLUSTER, not the sector.
  ```

## Project Structure
- **Config Cascade**: `platformio.ini` (env #define) → `myoptions.h` (hardware profile, user defaults) → `options.h` (fallback defaults for anything undefined). Third-party libraries are their own translation units and never see this cascade: the few values they must agree with us about live in `src/core/options_overrides.h`, which `platformio.ini` force-includes into every TU.
- **Core logic**: `src/core/` (Player, Display, Network, Config, Controls).
- **Headers that own data**: `src/locale/dsplocale.h` defines its locale tables (`l10n_strings[36][77]` and every translated string) as namespace-scope `const` arrays *inside the header*, so each translation unit that includes it embeds another complete copy - roughly 48 KB of flash. Do not add that include to a new file: call `l10n(...)` from a file that already has it, or expose a small accessor as `Player::isConnecting()` does for the "connecting" placeholder check. The same rule works in reverse: a `const` array has internal linkage, so a TU whose last `l10n()` call is removed stops emitting its copy entirely. Removing `sdmanager.cpp`'s two title placeholders (they became `player.setReady()`) dropped about 55 KB from both build environments with no other change.
- **Libraries path**: Software codecs: `libraries/I2S_Audio/`, `libraries/ES8311_Audio` / Hardware decoder: `libraries/VS1053_Audio/` (Hardware chip), other folders are custom drivers for other display, touchscreen, and other hardware.
- **UI**: Widgets in `src/displays/widgets/`, drivers in `src/displays/`.
- **Plugins**: The former yoRadio plugin hook system has been removed. Add new behavior in the owning core module instead of reviving plugin-style hooks.
- **Web UI**: Most files in `data/www` are served with headers in `src/core/netserver.h`. `search.html` and `curated.html` are not.

## Functionality
- **Hardware**: The firmware is built according to the hardware that is connected to it and users who will use it.  These are defined by files listed in Config Cascade.
- **Software**: `src/core/options.h` and the Config Cascade should be used to extend functionality, not limit it. `#if defined` and `#ifndef` should not be used in the code for configuration not related to hardware.
- **Granular Control in Web UI**: If not hardware-related, functionality should be changeable in the Web UI, not controlled by a `#define` in Config Cascade.

## Force-Included Config (`src/core/options_overrides.h`)

`[ehradio] build_flags` in `platformio.ini` passes `-include src/core/options_overrides.h`, so that header is compiled first in **every** translation unit: our sources, every managed library (AsyncTCP, ESPAsyncWebServer, Adafruit, ...), and the framework's own `.c` files.

- **Never force-include `options.h`.** It is not a library-safe header: it does `#include <SPI.h>`, and a library TU's compile line does not carry the SPI include path - `Wire.cpp` fails with `options.h:273:10: fatal error: SPI.h: No such file or directory` - and it uses C++-only `static_assert` while `build_flags` also apply to `.c` files. `options_overrides.h` exists to hold the few library-visible values instead.
- **Keep it dependency-free**: preprocessor only, no framework or library includes, no types, no C++ syntax, and only values a library must agree with us about. The framework's `.c` files compile through it, so C compatibility is a hard requirement.
- **`myoptions.h` stays the user channel**: the header includes it first, so a value set there beats every default in the header - that is the supported way to set `CONFIG_ASYNC_TCP_USE_WDT`, `CONFIG_ASYNC_TCP_RUNNING_CORE`, and so on. This holds only in builds that carry the force-include: root `platformio.ini` and `builds/trip5/platformio.ini` do, the other `builds/*` templates do not.
- **The `NETWORK_CORE` tree is duplicated here and in `options.h` on purpose, and both copies must survive being evaluated twice** - this header is compiled first, so `options.h` always meets the value already set. That needs no marker macro: the dual-core branch is wrapped in `#ifndef NETWORK_CORE`, and the unicore branch only errors when the value it finds is not 0, so it cannot mistake our definition for a user's. Keep the two trees identical when editing either.
- **A value change here may not rebuild libraries**: after the header was introduced, only `src/` objects were recompiled. Use `pio run -t clean -e <env>` when a value change must be guaranteed to take effect.
- **Any long card operation reachable from a web handler must feed the task watchdog** (`sdFeedWatchdog()`), because web handlers run in AsyncTCP's task and that task is subscribed (`CONFIG_ASYNC_TCP_USE_WDT 1`). The subscription is kept on deliberately: it turns a handler wedged on a stalled SD call into an automatic reboot instead of a device that needs a power cycle.

## File Handles and Cross-Task State

- **A `File` has exactly one owning task.** Never close or reassign a handle another task may be using: `File::close()` closes the underlying filesystem file, and the failure this produces is not a compile error but `assert failed: lfs_file_seek lfs.c:NNNN (lfs_mlist_isopen(...))` - a stale handle used after someone else closed it. Do not cache a `File` across callbacks; open, use and close it inside the one call. If a second task touches the same files, take a mutex.
- **Never do flash or SD I/O inside a critical section** (`portENTER_CRITICAL`). Use a FreeRTOS mutex. A critical section disables interrupts, and a flash write already runs with the cache disabled.
- **One long operation at a time, and say so.** A request that finds another operation in flight is refused with 409 plus a reason string rather than sharing the state - the SD card manager's delete batch and the `/log` download both work this way. A shared handle or shared layout is what let two requests of the same kind corrupt each other.
- **A subsystem that owns a filesystem must be shut down before that filesystem is erased.** `LittleFS.format()` (the danger-zone command) takes every open handle with it, so the log ring is flushed and disabled first (`logRingShutdown()`).
- **A boot-time delay that exists to make the serial log readable must also fire after a crash.** `esp_reset_reason()` is `ESP_RST_PANIC` (or a WDT reason), not `ESP_RST_POWERON`, after a reset caused by a crash.

## Crash Reporting

- A panic cannot be logged by us - no application code runs afterwards, and the log ring is in RAM. Instead ESP-IDF writes a core dump to the reserved `coredump` partition (core-dump-to-flash and ELF format are already enabled in the prebuilt S3 libraries, and every partition table reserves the partition). `src/core/crashreport.cpp` reads it back on the NEXT boot and `BOOTLOG`s the summary, so `/log.txt` carries the crash next to the lines that caused it.
- The report prints the crashing task, the `exc_pc`, the exception cause, up to 16 on-device backtrace PCs and the crashing ELF's SHA256. Symbolise those PCs against the ELF whose SHA256 was printed - addresses from a different build resolve to the wrong functions.
- The dump is erased after being reported, or every later boot would repeat the same crash. `COREDUMP_KEEP_DUMP` in `options.h` keeps it for `esp-coredump` instead; `COREDUMP_SUMMARY_AT_BOOT` switches the whole feature off.
- Reading a core dump requires the ELF-format build of the IDF libraries (`CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`); the code is `#if`-guarded so a target without it still builds, it just reports nothing.
