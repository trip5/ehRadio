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
  8. Adding, renaming or removing a command in `commandhandler.cpp`? → `Commands.md` gets the entry in the **same block and the same position**, and its *Blocked in HTTP/MQTT/Telnet* column has to match `isBlockedForSource()` and nothing else. The file's order is its value: an audit is a walk down the two lists. The `CONFIG_KEY_ENTRY` names in `config.cpp` are store keys, not commands
  9. Adding a field to `theme_t`? → it lands in **four** places, or the build or the tool is quietly wrong: `theme_t` in `config.h`, `ThemeData` in `themes.h` (the two are `memcpy_P`'d, so their shapes must match exactly), every hand-overridden reduced palette (`displays/tools/oledcolorfix.h`, `displaySSD1322.cpp`), and **`importtheme.py`** - `FIELD_ORDER` plus its derivation rule, with the matching rows in `importtheme.md`. A field missing from `FIELD_ORDER` is dropped from every imported theme without a word. A new *screensaver* colour belongs in the "derived" list, not the computed one, if a stated rule produces it

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
- **A display label is not a wire format, and a control offers only the values the hardware can hold.** The theme editor's element column shows a human label and keeps the `.h` key one click away: the table in `themes.h` pairs them one per line, so a label can be reworded without touching a theme file, and it is deliberately not two parallel tables - a key added without its label would shift every entry after it while a count-based check still passed. The colour fields step in 565 levels (8 / 4 / 8, maximum 248 / 252) because 0-255 in steps of 1 offered 256 choices for 32 outcomes, where 248 to 255 are one red - `r8 >> 3` is 31 for all of them. `themes.h` writes the same level-times-step numbers, and `importtheme.py` forces them onto anything it writes, so one colour has one spelling in the editor, in the palette and in the tool. Both are one principle: the interface speaks the storage's own terms.
- **The control that changed is the one that fired the event, never `document.activeElement`.** The theme editor captures each field's row index when it attaches the listener; the earlier version looked the row up from the focused element instead, and clicking a number field's spinner arrows fires `input` **without focusing that field** - so no row was found, nothing was painted locally, and the panel was still sent the new colour. The symptom was oddly specific and worth recognising: the display flashes while the swatch and the readout keep the old value. If only one of the two happened, or they disagree, the update is being inferred rather than carried.
- **A restore must not be able to destroy what it is not restoring.** The theme backup is a set of themes with no placeholders and no slot numbers inside it, and a bulk import writes only into **free** slots, checking capacity *before* the first write so a file that does not fit changes nothing at all. The single-theme import is the deliberate exception, and it can only touch the one slot the user pointed at. Anything that overwrites while calling itself a restore is really a merge: an older file would silently destroy newer work, and the file would have to carry a full layout to be safe. The same reasoning is why a reader that skips an empty entry must be the design, not an accident - `null` meaning "erase" is a trap one careless line away.

- **A repaint optimisation whose correctness depends on state that is not visible on the screen is a bug magnet.** The VU bars briefly drew incrementally - remembering where each channel's tip and peak marker were last frame (`_prevMeas*` / `_prevPk*`), repainting only the span between the old and the new value, and sending only that span with a dirty-rectangle blit. Nothing on the panel proved `_prev*` still described the canvas, so every way of breaking that assumption (the vspace gaps and the inter-channel strip left un-erased, a peak marker surviving in them, a crossed-over per-channel pairing, a partially undone clear) surfaced as a cosmetic artefact - in four rounds. It is removed: the bar family now full-wipes and full-repaints every frame, and the dirty rectangle, the partial blit, the `_prev*` fields and the `Widget::_invalidate()` hook that kept the belief fresh are gone with it. The reason the shortcut existed had also gone - the screensaver box is budget-capped (`VU_MAX_WIDTH x VU_MAX_HEIGHT`), so a full repaint of it is a fraction of the 99 ms it once cost. When a repaint shortcut keys off remembered state, ask what keeps that state true; if the answer is "every erase path has to be right", prefer the full repaint.

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
- **State shared between a web handler and the display task belongs to the display task: the handler stages it and queues a request.** The theme editor's live preview is the worked example - the handler parses into `themeStaging()` and queues `THEMEPREVIEW`, and the display task is what copies the staging theme into `config.theme` before re-initialising the widgets. The handler never writes the live theme itself. Keeping it that way is also what makes the preview safe: it changes nothing the user did not ask for, because the widget re-init runs in place instead of through `_switchMode()`.
- **When a task stages something and another task applies it, the applying side must not re-read the live source over the top of it.** `_applyState()` takes the *staged* theme when it is repainting in place and re-reads the stored one otherwise, and that branch is not an optimisation: re-reading `config.store.themeId` after the handler had staged new colours replaced them with the colours already on screen, so the preview repainted exactly what was there and looked like it did nothing at all.
- **A widget re-init paints, so whoever runs one has to put the current page back afterwards.** `_reinitWidgets()` does not only rebuild the objects - it re-applies their layout visibility, and those calls paint as they go. The page that happens to be on screen therefore ends up with fresh player and dialog widgets drawn over it: on the playlist that showed as RSSI and the buffer bar appearing over a half-drawn list that was never repainted. The theme preview is the worked example of the repair - it re-asserts the *current mode's* page after the re-init (the player page plus the IP line that `setPage()` wipes, the playlist page through `_drawPlaylist()`), and leaves modes whose widgets the re-init just repainted alone. Two more things fall out of the same job: while asleep it rebuilds the screensaver directly instead of queueing it, because a queued rebuild leaves those paints on the panel for a pass and that reads as a flash of the main screen; and it never calls `_switchMode()`, which is what keeps the countdowns and any open overlay untouched.
- **Coalescing repeated requests belongs in `Display::putRequest()`, not in the caller.** It is the only place that knows whether the send actually landed. A "is one already queued" flag set *before* the send would be left stuck by a request the saturated queue silently drops, and the feature it guards would then be blocked for good - `_previewPending` is set from the send result and cleared where the request is handled and in `resetQueue()`.

## Crash Reporting

- A panic cannot be logged by us - no application code runs afterwards, and the log ring is in RAM. Instead ESP-IDF writes a core dump to the reserved `coredump` partition (core-dump-to-flash and ELF format are already enabled in the prebuilt S3 libraries, and every partition table reserves the partition). `src/core/crashreport.cpp` reads it back on the NEXT boot and `BOOTLOG`s the summary, so `/log.txt` carries the crash next to the lines that caused it.
- The report prints the crashing task, the `exc_pc`, the exception cause, up to 16 on-device backtrace PCs and the crashing ELF's SHA256. Symbolise those PCs against the ELF whose SHA256 was printed - addresses from a different build resolve to the wrong functions.
- The dump is erased after being reported, or every later boot would repeat the same crash. `COREDUMP_KEEP_DUMP` in `options.h` keeps it for `esp-coredump` instead; `COREDUMP_SUMMARY_AT_BOOT` switches the whole feature off.
- Reading a core dump requires the ELF-format build of the IDF libraries (`CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`); the code is `#if`-guarded so a target without it still builds, it just reports nothing.

## Failure Handling and Logging

- **A recovery rung must not depend on an allocation.** The stream ladder is the only thing that clears a wedged network stack, and it is a FreeRTOS task needing 8 KB of contiguous heap in a system whose heap is lowest exactly when the stack is wedged (the decoder and the failed connection objects are still held). The failure mode that produces is not a crash but a loop: the watchdog logged "a resume is still owed, starting a new one" every 5 s for minutes while nothing could be started at all. So the rung that matters - the link reset - is reachable from the 1 s timer as well as from the task, because `WiFi.disconnect(true, false)` needs no task, no heap and no scheduler. When a recovery step is designed, ask what it costs to *run* it, not only what it does.
- **A cap with nothing behind it is a stop, not a policy.** Three link resets per outage was correct - a reset costs a scan, a reassociation and DHCP, so it should be spent only on evidence - but past those three the ladder retried a state that only a reset can clear, at 15 s and then 60 s, forever. Every ceiling needs either a rung above it or a written reason why the remaining retries are useful; if the answer is "nothing can fix this from software", the last rung is a restart. The restart must not set `bootStableMarker` (the intentional-reboot paths do, so the next boot is not safe mode) and must be counted in RTC memory, because a wedge that recurs on every boot otherwise reboots forever and safe mode will not catch it - each boot proves itself stable before the network fails.
- **Only `FUNCTIONLOG` takes a category.** `SERIALLOG`, `SERIALLOGX`, `BOOTLOG`, `BOOTLOGX` and `ERRORLOG` are `#define X(fmt, ...)`, so `ERRORLOG("Network", "streamRetry task could not be created (%u bytes, free heap %u)", ...)` passes the category as the format and throws the message away - the log prints `[ERROR] Network` and nothing else. That is exactly what the two task-create failures in `network.cpp` did, which is how a 8 KB-versus-7 KB heap starvation stayed invisible for minutes. When adding a log call, copy the shape of the macro you are calling.
- **A task that is parked is not a task that has finished - and "created" is not "running".** The startup services are created in `setup()` and can then sit in `while (cardInUse())` for the whole boot, so `SVC_WILL_RUN` outlives the decision its name suggests. The WebUI is held back until those TLS downloads are done, and the check that starts it has to treat a parked task as "not going to compete" - wait for `SVC_DONE` alone and SD mode loses its WebUI, which is exactly when it is needed. The same reading is what the boot-stable decision already needed. When a flag means "created", ask what it means when the thing it created never runs, and never make a resource wait on a state that may never arrive.
