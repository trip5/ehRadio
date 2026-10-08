# ehRadio Code Summary (Human + AI Operational Map)

## Mandatory Maintenance Directive

If a change affects code interactions (how files/modules interact), storage keys, WebUI contracts,
locale/build/dependency behavior, or other external contracts,`.github/code-summary.md` MUST be updated in the same
change set; bug fixes that restore expected behavior are exempt unless they also change code interactions or external
behavior/contracts.

This file exists to reduce re-analysis cost for humans and AI agents.

It will get lengthy as more is added to it.

---

## Scope and Intent

This document is intentionally per-file focused for:
- `src/main.cpp`
- `src/core/*`
- `data/www/*`
- `src/locale/*`

Grouped (not one-by-one deep explained) areas:
- `src/displays/display*.cpp/.h` (drivers follow similar shape)
- `src/displays/conf/*.h` (widget placement/config pattern files)
- `src/displays/clockfonts/*` (font assets)

---

## Fast Architecture Overview

### Build-time chain
- `platformio.ini` selects environment, included libraries, and included source files.
- `myoptions.h` selects hardware profile + defaults.
- `src/core/options.h` resolves all defaults/fallbacks and feature flags.
- `.github/workflows/build-release-firmware.yml` verifies generated contributor release artifacts by re-running
  `builds/fix_releases.py` on CI and diff-checking `builds/releases/` (contains `firmware.txt`, `releases.md`, and
  `web_assets/`) against what was committed.
- `.github/workflows/build-deploy-page.yml` deploys Pages on `release.published`, and on branch/manual runs it preserves
  the currently published `firmware-info.json` and manifests instead of overwriting them from `builds/*/web_assets`.
- `.github/workflows/update-timezones.json-automatically.yml` checks out using `DEPLOY_KEY` and pushes over SSH to
  `dev`, enabling ruleset bypass configured for Deploy keys.

### Runtime chain
- `src/main.cpp` bootstraps system: config -> display -> player -> network -> server/telnet/controls.
- WebUI/WebSocket input path: `netserver` -> `commandhandler` -> `config/player/display/network`.
- State output path: `requestOnChange(...)` in `netserver` -> WebSocket JSON to browser.
- Settings persistence path: `config.saveValue(...)` -> ESP Preferences namespace `"ehradio"`.
- Web-stream resume path: `Config::setLastStationUrl(...)` -> debounced LittleFS file `/data/laststation.url` ->
  `player.resumeLastWebSource()` for smartstart/reconnect/direct-URL resume.

### Logging chain (serial + telnet)
- `src/core/logging.h` / `src/core/logging.cpp` define the common log path. Call sites keep the uppercase macros, which
  now wrap function-backed implementations (`serialLog`, `functionLog`, `bootLog`, `bootLogX`, `errorLog`, `serialLogDot`,
  `audioLog`) so formatting happens in one backend.
- Core macros:
  - `SERIALLOG(...)`: writes one formatted line to both serial and telnet sinks.
  - `SERIALLOGX(...)`: writes one formatted line to both serial and telnet sinks without line-ending (useful after
    `BOOTLOGX`).
  - `FUNCTIONLOG(category, ...)`: category-tagged wrapper over `SERIALLOG`.
  - `BOOTLOG(...)` / `BOOTLOGX(...)`: boot-sequence logging helpers. `BOOTLOG` ends with a line-wrapper, `BOOTLOGX` does
    not.
  - `ERRORLOG(...)`: error-category wrapper.
  - `SERIALLOGDOT()`: progress-dot helper for long-running loops.
  - `AUDIOLOG(category, ...)`: callback-safe logging wrapper for stack-sensitive audio callback contexts.
  - `BOOTLOG_TIME` (defaulted in `options.h` under `ALL_DEBUG_LOGS`, set in `myoptions.h` for debug builds) makes every
    `BOOTLOG` line carry the time since boot, zero-padded to five digits, between the 16-character category field and
    the message: `[BOOT]          00600ms: <message>`. `logging.cpp` must include `options.h` or the `#ifdef` is false
    in that one file alone while the rest of the build has the macro - a silent feature loss with a green build. Only
    `bootLog()` stamps, selected by category plus newline (`appendNewline && category == "BOOT"`), because `bootLogX()`
    writes no newline and a stamp there would land mid-line; the value is `millis()` at emit time and self-widens past
    100 s, and telnet receives the same composed line. Boot logs are the boot-timing measurement: consecutive deltas
    attribute a slow boot to its stages. It is also the switch for the three stage timing logs (`BOOTTIMELOG`,
    `LITTLEFSTIMELOG`, `CONFIGTIMELOG`): undefined, their bodies compile to nothing, so the build has no stage lines and
    loses the `[BOOT] ...ms:` prefixes too. Measured: 216 bytes, 52 of them call sites, which stay as empty functions in
    the off build.
  - `ESPFILEUPDATER_VERBOSE` is derived in `options.h`, immediately after `ESPFILEUPDATER_DEBUG` is settled there, not
    in `logging.h`. It lived in `logging.h` once, where the library files that include that header without `options.h`
    (`FT6336.cpp`, `es8311.cpp`, the four `audioVS1053Ex.cpp` variants) resolved it to `false` while the core resolved
    it to `true` - one macro with two values in one build. All six call sites pass it to
    `ESPFileUpdater::checkAndUpdate()` and include `options.h`; a seventh that forgets now fails to compile rather than
    quietly logging nothing.
  - Boot stage attribution: `BOOTTIMELOG(name)` closes each block of `setup()` with the stage's cost, e.g.
    `[BOOT]          01950ms: checkLittleFSandVer                    377ms` - the timestamp is the running total to the end
    of that stage and the message is that stage's share. The init paths announce completion with `BOOTLOGX` progress
    lines, which carry no stamp by design (it would land mid-line before the dots), leaving the whole tail of `setup()`
    unmeasured: about 4.7 s of a 23.4 s boot on the 128x64 VS1053 build. Add a call after every block in `setup()` that
    can block; the first measures from power-on. `LITTLEFSTIMELOG` and `CONFIGTIMELOG` do the same for the LittleFS and
    config paths, all three in `logging.cpp` behind declarations and macros in `logging.h`. Each keeps its own stamp, so
    a marker measures against the previous marker of its own kind: the config and LittleFS stages that run inside a
    `setup()` block would otherwise disturb the `setup()` deltas, and a shared stamp would redefine every number.
    `LITTLEFSTIMELOGRESET()` and `CONFIGTIMELOGRESET()` restart the measurement at the entry to the functions whose
    first stage would otherwise be measured from the previous marker (`checkLittleFSandVer()`, `Config::init()`,
    `loadPreferences()`, `initPlaylistMode()`). The functions are declared whatever the build and compile to empty bodies
    without `BOOTLOG_TIME`; the `verifyLittleFS` summary line's `, NNNms` is gated the same way.
  - The log write path is shaped by `Serial` here being USB CDC, not a UART: `HWCDC::write()` takes its TX lock with a
    timeout, blocks on the ring buffer, then waits 1 ms at a time for the host up to its TX timeout (100 ms default;
    USBCDC 250 ms), after which it declares the host gone and clears `connected`. So one log line could stall the boot
    for up to a timeout, twice over. Two changes: `emitLogMessage()` composes the CRLF into the buffer and issues a
    single `Serial.write()`, and `setup()` applies `BOOTLOG_TX_TIMEOUT_MS` (options.h, default 5 ms) through
    `Serial.setTxTimeoutMs()` under `#if ARDUINO_USB_CDC_ON_BOOT`. Not zero: at zero the first failed ring-buffer send
    gives up immediately and `tries` hits zero, so a burst such as the 40-line config dump comes out truncated rather
    than merely bounded. Measured cost before the change: about 4.6 ms per line, 186 ms for the config dump alone.
  - **File logging (`SAVE_LOGS_TO_FS`, live in `logging.h`/`logging.cpp`; it began as `logring.*`, which is why the
    identifiers still read `logRing*`).** Switched on by an uncommented `#define SAVE_LOGS_TO_FS` in `myoptions.h` (as
    in `builds/trip5/myoptions.h`). Every test is `#ifdef`, so the define needs no value - and, for the same reason,
    `#define SAVE_LOGS_TO_FS 0` would still switch it on: to turn it off, comment the line out. When on,
    `emitLogMessage()` takes **both** of its branches to `logRingWrite(text, complete, critical)`: a complete line goes
    straight out, and a **fragment** - the `X` variants' partial text, and the progress dots now that `serialLogDot()`
    routes through the funnel - accumulates in a pending buffer and rides out joined to the line that closes the run, so
    the file reads the way serial did (`netserver.begin....done`). The run carries the stamp taken when its first
    fragment arrived, so the timestamp marks when the work started rather than when the newline came. Lines whose
    category is `ERROR`, `Network`, `Player` or `Services` flush to flash immediately (the last lines before a reboot
    are the ones that explain it); everything else rides a RAM ring (16 KB in PSRAM, 2 KB of internal heap when there is
    none) drained by `logRingFlush()` from `loop()`, outside the loop's stage attribution, and written through a static
    internal-DRAM staging buffer because a flash write runs with the cache disabled and must never be handed a PSRAM
    pointer. `FS_REQUIRED_FREE_SPACE` (options.h) is the ring's reserve as well as netserver's download floor: the ten
    files share `total - reserve`, so each caps at about 335 KB here and the ring as a whole is the filesystem minus
    that reserve - 440 KB/h with `ALL_DEBUG_LOGS` and `CORE_MONITOR` (86% of it Core Monitor) is about 7.5 h of history.
    One `File::write()` can trigger an uninterruptible sector erase, so `Max Main Loop Time` reads ~118 ms with the ring
    on against ~3 ms without, and the 15 ms flush budget cannot preempt the single write it is inside. Serving is
    snapshot-based: `logRingSnapshot()` fixes the file list and each size for one request (so anything logged during the
    download cannot shift an offset) and `logRingReadAt()` re-reads it file by file, keeping its `File` open across
    chunk callbacks rather than open/seek/close per chunk - 730 opens is what made a 1.1 MB `/log` take 21 s to load and
    to save. While such a handle is open `flush()` writes nothing at all, because littlefs can relocate the tail of the
    newest file when it compacts a directory; the RAM ring absorbs the pause, and a request that stops reading is
    treated as abandoned after `SERVE_IDLE_MS` (5 s) and its handle dropped. Layout, rotation, sizing, the reboot-safe
    `/logs/idx` state and the `/log` reader are documented here rather than in a page of their own: the feature is
    normally built out, so it does not warrant one.
  - The ring has two URIs. `/log` (and its `/log.txt` spelling, which is what curls and old bookmarks carry) streams the
    whole ring as one text file, chunked, with the layout snapshotted per request; `/logclear` wipes it and answers with
    how much was there. Clearing is a URI of its own rather than `?clear=1` on the reader, so that a bookmark, a history
    entry or a reload button can never destroy the log whose name it carries.
  - **One owner per file handle, and one lock (`_ioLock`) for the ring's flash I/O.** The download used to keep its
    `File` open between chunk callbacks while `flush()`, `logRingSnapshot()` and `logRingClear()` closed that handle from
    other tasks, so a seek landed on a handle littlefs no longer had open and the device aborted with
    `assert failed: lfs_file_seek lfs.c:... (lfs_mlist_isopen(...))`. `logRingReadAt()` opens, seeks, reads and closes
    inside the one call, and every flash touch in the ring (`flush()`, `currentSize()`, `ensureFile()`, `saveIdx()`, the
    callers of `rotate()`, `logRingSnapshot()`, `logRingClear()`, `logRingInit()`) runs under `_ioLock` - a real mutex,
    never a critical section, because a flash write must not run with interrupts disabled. `flush()` takes it with a
    zero-tick try and skips when busy (two flushes could interleave their appends and each advance `_cur`), and never
    rotates while a download is live, because a rotate deletes the oldest slot a snapshot starts from.
  - One download at a time: while `logRingServeBusy()` is true a second `/log` gets 409, and `/logclear` refuses rather
    than wiping files out from under it. A download that reads nothing for `SERVE_IDLE_MS` (5 s) counts as abandoned, so
    a client that walks away cannot lock the ring closed. `logRingClear()` returns the bytes it removed, so the
    confirmation text no longer snapshots first (that snapshot rewrote a live download's layout). `logRingShutdown()`
    (called by the danger-zone `format` command before `LittleFS.format()`) flushes and disables the ring for the rest
    of the boot.
- Contract detail:
  - `Telnet::printf(...)` is telnet-only transport and no longer mirrors to serial.
  - Normal logs no longer route through `Telnet::printf(...)`; `logging.cpp` uses `Telnet::logLine(...)` /
    `Telnet::logRaw(...)` so the shared logging path avoids the prompt-aware telnet formatter and its extra stack use.
  - Logs should use macros above; direct `Serial.print*`/`telnet.printf` is reserved for explicit transport-specific
    behavior (for example client-targeted telnet responses and OTA progress streaming).

### Crash report (the core dump, read back on the next boot)
- A panic cannot be written into the ring (nothing of ours runs after one, and the ring lives in PSRAM), but a core dump
  is recorded on every crash: `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` with `DATA_FORMAT_ELF=y` is built into the
  framework's prebuilt ESP32-S3 libraries, and every partition table in `builds/partitions/` ends with a 64 KB
  `coredump` partition. `src/core/crashreport.h` / `crashreport.cpp` only read it back.
- `crashDumpAvailable()` is `esp_core_dump_image_check() == ESP_OK`; `crashDumpReport()` calls
  `esp_core_dump_get_summary()` and BOOTLOGs the crashing task, `exc_pc`, the exception cause and vaddr, up to 16
  on-device backtrace PCs, and the crashing app's ELF SHA256 - all either paste-ready for
  `xtensa-esp32s3-elf-addr2line -pfiaC -e firmware.elf <pc>` or needed to know which build those addresses belong to.
  It then erases the dump, or every later boot would report the same crash again. `COREDUMP_KEEP_DUMP` (options.h)
  leaves it in flash for `esp-coredump`, and `COREDUMP_SUMMARY_AT_BOOT` (on by default) is the switch for the feature;
  both it and `CONFIG_ESP_COREDUMP_*` must hold or the file compiles to empty stubs.
- `main.cpp` calls it right after the 1 second pre-log delay, inside the same `CORE_DEBUG_LEVEL`/`ALL_DEBUG_LOGS` guard,
  and that delay also fires when a dump is waiting (after a crash `esp_reset_reason()` is `ESP_RST_PANIC` or a WDT
  reason, not `ESP_RST_POWERON`). Because the block runs before `logRingInit()`, its lines sit in the RAM ring and are
  appended once the ring is ready - the crash lands at the end of the log, after the lines that led up to it.
- The summary API exists only in the ELF core-dump build of the IDF libraries, so `crashreport.cpp` is guarded by `#if
  defined(COREDUMP_SUMMARY_AT_BOOT) && CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH && CONFIG_ESP_COREDUMP_DATA_FORMAT_ELF`; both
  sdkconfig macros are 1 here, and the map file confirms `esp_core_dump_get_summary` links in from
  `libespcoredump.a(core_dump_elf.c.obj)`.

### Primary shared state objects
- `config` (`Config` singleton): persistent store + station/theme/runtime state.
- `player` (`Player` singleton): audio control and playback state.
- `display` (`Display` singleton): display mode and render queue.
- `network` (`MyNetwork` singleton): connectivity/time/weather state.
- `netserver` (`NetServer` singleton): HTTP/WS server and outbound state queue.

---

## Build and Configuration Files

### `platformio.ini`
- Core environment and dependency declaration.
- Important behavior:
  - `build_src_filter` excludes all by default then re-includes selected folders/files.
  - Board environments add display/audio library includes.
  - `extra_scripts` are used for localization/font replacement and gzip workflow.
  - `[ehradio] build_flags` carries `-include src/core/options_overrides.h`, so that file is placed at the top of
    **every** translation unit - our own sources, every managed library, and the framework's `.c` files. It exists
    because libraries never include `options.h` (see its subsection below).
- Risk:
  - Wrong env can compile without required modules because files are source-filtered.
  - Editing `options_overrides.h` may not rebuild library objects: when the header was introduced, only `src/` TUs were
    recompiled and every `libXXX` object was left alone. A changed *value* can therefore stay inert until the affected
    env is cleaned (`pio run -t clean -e <env>`).

### `myoptions.h`
- Board/profile selector and hardware wiring table.
- Sets many user defaults that flow into `config_t` via macros.
- Enables/disables many runtime features by compile-time macro presence.

### `src/core/options.h`
- Canonical fallback defaults and compile flags.
- Includes `myoptions.h` when present.
- **Owns all compile-time guardrails** inline, right next to each respective define.
- Owns shared buffer sizing macros under `/* Maximum lengths of character buffers */`, including `MQTT_URL_SIZE` for
  stream/artwork URL buffers used by `player`, `audiohandlers`, and MQTT status payload sizing, plus
  `MQTT_STATUS_SETTLE_MS` (how long an MQTT status change must remain unchanged before it is published).
- Defines:
  - hardware defaults (pins, feature gates)
  - updater URLs (`FILESURL`, `UPDATEURL`, `CHECKUPDATEURL`) unless disabled
  - weather defaults and thresholds
  - battery defaults/curve/thresholds
  - WebUI and localization defaults
  - curated list defaults
  - **Exception**: locale and language options are handled by locale.h
  - **SPI architecture — Named Bus System** (auto-derived internals — do NOT define `SPI_BUS_SECONDARY`, `SPIA`, or
    `VS1053_SPIBUS` in `myoptions.h`):
  - **Bus A** = `SPIA` (alias for `&SPI`, the default ESP32 SPI instance). Pins configured by `SPI.begin(SPIA_SCK,
    SPIA_MISO, SPIA_MOSI)` in `Config::init()` when `SPIA_SCK` is defined; otherwise `SPI.begin()` uses hardware
    defaults.
  - **Bus B** = `SPIB` (`SPIClass SPIB(SPI_BUS_SECONDARY)` declared and initialized in `config.cpp`). Only exists when
    `SPIB_SCK` is defined. `SPI_BUS_SECONDARY` uses symbolic constants: `VSPI` (ESP32) / `FSPI` (ESP32-S3/C3) for the
    primary bus, `HSPI` for the secondary bus on all targets.
  - **Bus pin defines** (set in `myoptions.h`): `SPIA_SCK/MISO/MOSI` manually, or use shorthands: `SPIA_DEFAULT` (chip
    default pins), `SPIA_DEFAULT_XMISO` (chip default SCK/MOSI, MISO=255 — for display-only Bus A). `SPIB_SCK/MISO/MOSI`
    manually, or `SPIB_DEFAULT` (chip default secondary-bus pins). `SPI.begin()` / `SPIB.begin()` are only called when
    the respective SCK is defined and `!= 255`; I2C-only builds skip SPI init entirely.
  - **Per-peripheral bus assignment** (char literals `'A'` or `'B'` — NOT strings): `SD_SPI`, `TS_SPI`, `VS1053_SPI`.
    `VS1053_SPI` resolves `VS1053_SCK/MISO/MOSI` from the matching bus pins in `options.h` (soft — does not override
    direct pin defines). SD and TS use the bus *object* directly (`SPIA`/`SPIB`) — no separate SCK/MISO/MOSI derivation
    needed.
  - **`VSPI FSPI` shim** — defined when target is not ESP32 (`!CONFIG_IDF_TARGET_ESP32`) for third-party library
    compatibility.
  - **VS1053 bus assignment** — `VS1053_SPIBUS` macro auto-derived in `options.h`. `VS1053_CS != 255` requires
    `VS1053_SCK` to be set (via `VS1053_SPI` or directly); `#error` if missing. Resolves to `SPIB` if `VS1053_SCK ==
    SPIB_SCK`, otherwise `SPIA`. Used as `&VS1053_SPIBUS` in the `Audio` constructor in `player.cpp`.
  - **SD bus selection** — `SDREALSPI` macro in `sdmanager.cpp` (SPI transport only, i.e. when `SD_USE_MMC` is not
    defined): `SPIB` when `SD_SPI == 'B'` and `SPIB_SCK` is defined, otherwise `SPIA`.
  - **Touchscreen bus selection** — inline in `touchscreen.cpp` `init()`: `SPIB` when `TS_SPI == 'B'` and `SPIB_SCK` is
    defined; `SPIA` when `TS_SPI == 'A'`; `ts.begin()` (default `&SPI`) otherwise.
- **SD card defines** (set in `myoptions.h`, fallback `255` = disabled in `options.h`):
  - `SD_CS` — chip-select pin; `255` disables SD entirely. **`254` is a sentinel meaning "SD present but on SDMMC"**,
    set automatically by `options.h` when SDMMC pins are defined, so every pre-existing `SD_CS!=255` "SD exists" test
    keeps working unchanged.
  - `SD_SPI 'A'/'B'` — assigns SD to Bus A or B; SD uses the bus object directly, no per-pin derivation needed. Not
    required when `SD_USE_MMC` is set.
  - `USE_SD` — feature presence macro, derived from `SD_CS!=255`.
- **SDMMC transport (ESP32-S3 only)** — defining any of
  `SDMMC_CLK`/`SDMMC_CMD`/`SDMMC_D0`/`SDMMC_D1`/`SDMMC_D2`/`SDMMC_D3` in `myoptions.h` defines `SD_USE_MMC` and switches
  the card from SPI to the native SD host. `SDMMC_CLK`/`SDMMC_CMD`/`SDMMC_D0` are required; adding
  `SDMMC_D1`/`SDMMC_D2`/`SDMMC_D3` selects 4-bit mode. Only all-six or first-three is accepted — a partial set, or any
  non-S3 target with SDMMC pins, is a compile `#error`. `SDMMC_FREQ` (0 = driver default 40MHz high speed) overrides the
  mount frequency. The header comment records that SD modules carrying a 74LVC125A/level-shifter cannot work, because
  CMD must be bidirectional.
- **I2S internal DAC**:
  - `USE_AUDIO_ESP32_DAC` — defined directly in `myoptions.h` to use the ESP32 internal DAC (ESP32 only, not S3/C3).
    `I2S_INTERNAL` boolean is removed.
- **Guardrail conventions** (maintain these when adding new options):
  - `#error` for hard-invalid values: wrong board type, mutually exclusive decoders, enum/constant out of range (e.g.
    `TS_MODEL`, `RTC_MODULE`), bad logical cross-constraints (e.g. `BTN_PRESS_TICKS <= BTN_CLICK_TICKS`,
    `BATTERY_CRITICAL_THRESHOLD >= BATTERY_LOW_THRESHOLD`).
  - `#warning` + `#undef` for out-of-range tunables in `/* USER DEFAULTS */` section (e.g. `SOUND_VOLUME`,
    `SCREEN_BRIGHTNESS`): reverts silently to default but now emits a visible warning in the build log.
  - `static_assert` with `__builtin_strcmp` for enumerated string options (e.g. `WEATHER_API`,
    `WEATHER_WIND_SPEED_UNITS`). **Update the `static_assert` whenever a new provider/value is added.**
  - `/* PREVENT BOARD-DEFINED PIN RE-USE */` section lives after the `/* ESP DEVBOARD */` LED block (requires `LED_PIN`
    and `ESP_S3C3` to be defined first). Covers LED vs RST pin conflicts only — keep it narrowly scoped.
- **What is intentionally NOT guarded**: booleans (compiler error is obvious), pin numbers (board-dependent range),
  free-form strings (`AP_SSID`, `MQTT_*`, URLs), color macros (R,G,B triplets), `AUTOBACKLIGHT(x)` (C macro function),
  `BATTERY_CURVE_MV/PCT` (already has `static_assert` in `battery.cpp`).
- **PSRAM buffer sizing** — `PSRAM_BUFSIZE` controls the audio input buffer and FLAC reserved buffer sizes; the default
  differs by board.
- Buffer bar visual mapping:
  - `BUFFERBAR_VISUAL_FULL_PERCENT` controls where input-buffer fill is rendered as visually full.
  - default `82` means 82% raw fill maps to 100% bar width; set `100` to keep direct 1:1 mapping.

### `src/core/options_overrides.h` (the force-included config)
- **Why it exists**: third-party libraries are their own translation units and never include `options.h`, so a value
  defined only there never reaches them. `AsyncTCP.h` guards each macro with `#ifndef`, so anything defined before it is
  first included wins; without this header the library's own defaults (`CONFIG_ASYNC_TCP_RUNNING_CORE -1`,
  `CONFIG_ASYNC_TCP_USE_WDT 1`, `CONFIG_ASYNC_TCP_QUEUE_SIZE 64`) were what ran.
- **`options.h` must never be force-included**: it is not library-safe. It `#include <SPI.h>` (a library TU's compile
  line lacks the SPI include path: `options.h:273:10: fatal error: SPI.h: No such file or directory`, the
  `Wire.cpp`/`libf41` failure) and uses C++-only `static_assert` while `build_flags` also apply to `.c` TUs.
- **Rules for the file**: preprocessor only, no framework/library includes, no types, no C++ syntax, and only values a
  library must agree with us about. `es3c28p` compiles the framework's `esp32-hal-*.c` files through it, which proves it
  stays C-safe.
- **Contents**: the `myoptions.h` include (first, so user values beat every default here), the `VS1053_CS 255` default
  (the core rule needs it; `myoptions.h` defines it only for VS1053 builds), the `NETWORK_CORE` tree, and the three
  `CONFIG_ASYNC_TCP_*` overrides.
- **The `NETWORK_CORE` tree is written to be evaluated twice**, with no marker macro: this header is compiled first, so
  `options.h` then meets the same lines with `NETWORK_CORE` already set. The dual-core branch is guarded by its own
  `#ifndef`; the unicore branch's `#error` tests the value (`#if NETWORK_CORE!=0`, which is 0 at that point, made by
  this header not the user) - a plain `#ifdef`/`#error` would fire on our own definition on any single-core build. Keep
  the two trees identical.
- **`myoptions.h` is the user channel**: values set there (e.g. `CONFIG_ASYNC_TCP_USE_WDT 0`) reach the libraries because
  it is read here before any default - only in builds carrying the force-include (root `platformio.ini` and
  `builds/trip5/platformio.ini`; the other `builds/*` templates do not, and there the classic `options.h` copy is live).
- sdkconfig macros (`CONFIG_FREERTOS_UNICORE`, `CONFIG_IDF_TARGET_ESP32S3`) are defined at force-include time (a clean
  S3 build proved the unicore branch live; no board-macro proxy is needed).
- **The AsyncTCP core is pinned, not locked**: `CONFIG_ASYNC_TCP_RUNNING_CORE` defaults to `NETWORK_CORE` but both may be
  overridden. Its task runs at priority 10, above every task we create, so which core it sits on decides what it can
  preempt - on a VS1053 build it shares core 0 with the audio decode.

## Compile-Time Modularity and Build Variants (`#if` / `#ifdef` behavior)

This codebase is strongly compile-time modular. Runtime behavior can differ significantly even with the same source,
depending on the selected PlatformIO environment and macro definitions.

### Where behavior is selected
- `platformio.ini`:
  - determines which board/env is built
  - controls source inclusion via `build_src_filter`
  - injects build flags that enable/disable subsystems
- `myoptions.h`:
  - hardware profile pins and feature toggles
  - indirectly controls which code paths in `src/core/*` and `src/displays/*` compile
- `src/core/options.h`:
  - fallback defaults and many `#ifndef` guards
  - central place where missing user macros are filled

### What commonly changes between builds
- Audio backend and related controls:
  - I2S audio path vs VS1053 path are mutually exclusive — enforced via `#error` in `options.h`.
- Display backend:
  - selected display model changes driver implementation and capabilities.
  - **Resolution/interface system**: `DSP_MODEL` = controller chip, `DSP_WIDTH`/`DSP_HEIGHT` = panel resolution, SPI or
    I2C type may be detected by `I2C_SDA` and `I2C_SCL` pins defines. Resolution defaults per DSP_MODEL in `options.h`,
    overridable in `myoptions.h`.
  - **dspcore.h**: one `#elif` per DSP_MODEL, sets the feature flags (`PSFBUFFER` for the TFT class, `DSP_OLED` for the
    monochrome class).
  - **dspfont.h** (new): selects the bootlogo by resolution, and holds the system-font and clock-font
    tables (see "Display fonts/assets" below — the clock font and its size are runtime values, not
    resolution-derived ones).
  - **dspconf.h** (new): selects `conf/display*conf.h` by resolution × display category.
  - Display `.h` files now delegate conf/font/bootlogo to these central files — no per-file branches for resolution.
  - Conf files no longer define DSP_WIDTH/DSP_HEIGHT (set upstream in options.h).
  - **Removed enum values** (collapsed into resolution variants): DSP_ST7789_240, DSP_ST7789_76, DSP_SSD1306x32,
    DSP_SSD1305I2C, DSP_SSD1327_64. I2C variants detected by `I2C_SDA` and `I2C_SCL`.
  - ST7735 DTYPE still required for library; resolution auto-derived from DTYPE in displayST7735.h.
- Network/update features:
  - some online update and service behavior is compiled out by feature flags.
- Touch, RTC, SD, battery helper behavior:
  - each has compile gates that can remove handlers/routes or no-op logic.
- MQTT is the exception: it is always compiled and gated at runtime by the `mqttenable` setting.

### Build-variant risk pattern
- A fix validated in one env may not compile or behave in another env because:
  - different source files are included
  - different `#ifdef` branches are active
  - defaults from `options.h` may mask missing `myoptions.h` values

### Practical checklist before merging a change
1. Confirm which env(s) the change targets in `platformio.ini`.
2. Check affected macro guards in touched files.
3. Verify any new setting has safe defaults in `options.h`.
4. Ensure mutually-exclusive hardware blocks still compile (audio/display especially).
5. If possible, do at least one alternate-env compile sanity check.

---

## Boot and Control Flow

### `src/main.cpp`
- `setup()` major sequence:
  1. serial + LED + RGB + battery init
  2. `config.init()`
  3. `backlightControls.init()`
  4. `display.init()`
  5. `player.init()`
  6. `battery.bootStatus()`
  7. `network.begin()`
  8. if no connectivity: start minimal server + controls + display start and return
  9. if connectivity:
     - `config.initPlaylistMode()`
     - `netserver.begin()`
     - `telnet.begin()`
     - controls init
     - display start
     - MQTT init when enabled in settings
     - optional smart-start playback
     - `startup.startupServices()`
     - `netserver.setBootReady(true)` only after setup work is actually complete
- `loop()`:
  - AP mode: Improv + captive DNS
  - normal: telnet loop
  - RGB loop
  - `battery.loop()`
  - player loop (connected/SD ready)
  - controls loop

---

## Core Folder Per-File Map (`src/core`)

### Module Convention
All modules in `src/core/` follow the **class + global instance** pattern:
- The header declares a `class Foo` with the full public interface and private members/methods.
- The `.cpp` defines all `Foo::` methods and declares the single global instance: `Foo foo;`
- The header provides `extern Foo foo;` so callers can use `foo.method()`.
- Hardware-conditional modules use a real class in the `#if` branch and a no-op stub class in the `#else` branch;
  `extern Foo foo;` is placed after the guard.
- **C-style free-function modules are not acceptable in `src/core/`.**

### Naming Style
- **camelCase** for all identifiers: private members, local variables, private methods (e.g. `inferredCharging`,
  `lastVoltageMv`, `readAndUpdate`).
- No underscore-prefixed names (e.g. `_myVar`, `_myMethod`) — use plain camelCase instead.
- Public API methods follow existing verb-noun camelCase: `init()`, `getStatus()`, `setEncAcceleration()`.
- File-scope `static const` constants also use camelCase (e.g. `pctSampleMax`, `emaAlphaQ`).
- `ALL_CAPS` applies only to `#define` macros and hardware pin constants inherited from the config cascade.

## `src/core/common.h`
- Shared enums and structs used across modules (display modes, requests, control events, etc.).
- Coupling:
  - Imported by display, controls, and command paths.

## `src/core/options.h`
- See earlier build section.
- `DSP_INVERT_TITLE` macro removed — replaced by runtime `config.store.inverttitle` boolean.
- `DSP_TFT` added to `dspcore.h` for all TFT display models — used to gate color-theme reloading (monochrome displays
  skip it).
- VU visualiser tunables live here: `VU_REFRESH_MS`, `VU_FADE_MS`, `VU_PEAK_FREEZE_MS`, `VU_PEAK_FADE_DIV`,
  `VU_PEAK_THICKNESS_MILLI`, `VU_SPECTRUM_MIN_PX`, `VU_SPECTRUM_SPACE_PX`, `VU_SPECTRUM_DB_FLOOR`,
  `VU_SPECTRUM_MAX_CHANNELS`, `VU_HISTORY_MIN_PX`, `VU_CAPTURE_SAMPLES`, `VU_DUTY_FACTOR` and `VU_STYLE_DEFAULT`.
  **`VU_CAPTURE_SAMPLES` must stay a power of two** — it is both the rolling sample window and the FFT size.
  `VU_REFRESH_MS` is a *floor* on the redraw interval, i.e. the ceiling on the frame rate, and `VU_DUTY_FACTOR`
  stretches that interval by the frame's measured draw cost (see the VU Widget Rendering section).

## `src/core/config.h`
- Defines persistent struct `config_t store`.
- `theme_t` no longer includes `theme.heap`; runtime input-buffer rendering uses `theme.buffer`.
- New fields: `uint8_t themeId`, `bool inverttitle`. `color565()` method removed.
- Defines station/theme structs and config API.
- Defines key constants for LittleFS paths and data file locations.
- `theme_t` now includes screensaver-specific clock palette fields (`clockss`, `clockbgss`, `secondsss`, `dowss`,
  `datess`) so screensaver clock/date elements can use colors independent from normal PLAYER-mode clock colors.
- `station_t` fields (`name`, `url`, `title`) are sized by `STATION_FIELD_LENGTH` (default 170, defined in `options.h`).
  These are RAM-only fields — not NVS-stored. `BUFLEN` has been retired; use `STATION_FIELD_LENGTH` for station metadata
  buffers across the codebase.
- `SD_PATH_LENGTH` (256, defined in `sdmanager.h`) is used for SD filesystem path buffers where paths may exceed 170
  bytes.
- `Config::keyMap` declaration controls Preferences key mapping.
- IR remote codes use a separate named store: `struct irstore_t` with one `uint64_t[3]` field per button (`power`,
  `mute`, `up`, `down`, `prev`, `next`, `play`, `mode`, `hash`, `n0`…`n9`). It is persisted in its own NVS namespace
  (`ehradioir`) through a dedicated key map in `config.cpp`; buttons are addressed by name, never by array position. The
  old positional `ircodes_t` blob was removed.
- `Config::saveValue(...)` API now has two simple overloads only:
  - typed: `saveValue(T* field, const T& value)`
  - string: `saveValue(char* field, const char* value)`
- `Config` also owns a separate RAM-backed `lastStationUrl` resume buffer that is intentionally *not* part of `config_t`
  / Preferences; it is persisted through `/data/laststation.url` with a dedicated debounce path because previews/direct
  URLs can change more often than normal prefs.
- `config.store.dspon` is RAM-only runtime state by design and is deliberately **not** in `Config::keyMap`. The display
  must always be on after power-on, so a standby/blank `false` must never be restored from NVS; persisting it would
  present as a "bricked" (blank) device to a normal user. It is still reported to the WebUI through `GETSCREEN` and
  drives the MQTT off-state token, but those read the live runtime value only.
- Legacy compatibility parameters (`commit`, `force`, and string `size_t N`) were removed.
- String saves now normalize into a zero-filled fixed-size buffer before compare/write to avoid reading beyond short
  source strings.
- Both overloads share a single internal write-if-changed path (`missing key` OR `size mismatch` OR `content changed`)
  before calling `prefs.putBytes(...)`.
- Save-path writes now emit telnet+serial config logs by key name; sensitive keys (`mqttpass`, `weatherkey`) are masked
  as `*`.

## `src/core/config.cpp`
- Persistent storage/defaults/hardware bootstrap center.
- Main responsibilities:
  - load and validate Preferences (`cfgset` marker)
  - defaults/init logic
  - LittleFS mount and Config-owned required-file checks used during playlist-mode initialization
  - version marker management (`/data/ehradio.ver`)
  - load/save the debounced Web-stream resume hint (`/data/laststation.url`)
  - playlist-mode initialization and file-presence checks before delegating playlist indexing/load helpers to `utility`
  - canonical LittleFS asset allowlists (`Config::wwwFiles[]`, `Config::dataFiles[]`) used by startup recovery and
    file-maintenance flows
  - reset section handlers (`defaultSettings(...)`)
  - named IR code storage in the dedicated `ehradioir` NVS namespace (`IR_MAGIC` 1812 stored under key `irset`, one key
    per button via `irKeyMap[]`); helpers `loadIR()`, `saveIR()` / `saveIR(button)`, `irCodes()`, `clearIR()`,
    `clearDuplicateIR()`, `irButtonByName()`, `irButtonCount()`, `irButtonKey()`, `irAction()`
  - `deleteOldKeys()` also drops the legacy `ircodes` key and the former persisted `dspon` key from the `ehradio`
    namespace (`dspon` is now runtime-only state and must not be restored on boot)
- SPI bus initialization: `Config::init()` calls `SPI.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI)` only when `SPIA_SCK` is
  defined and `!= 255`, and `SPIB.begin(SPIB_SCK, SPIB_MISO, SPIB_MOSI)` only when `SPIB_SCK` is defined and `!= 255`.
  I2C-only builds skip SPI init entirely. Both buses are initialized before `_initHW()` and before `display.init()` /
  `player.init()`. Both SPI buses are fully configured before any peripheral uses them. `SPIClass
  SPIB(SPI_BUS_SECONDARY)` is declared at file scope in `config.cpp`; extern declared in `config.h`.
- **A safe-mode boot pays a deliberate 1 s before anything else.** `setup()` holds `if (!config.store.bootStableMarker)
  delay(1000)` and then dumps the config (`bootInfo`, ~196 ms at 115200), so the early stage measures ~1586 ms in safe
  mode against ~590 ms settled - the NVS path itself costs ~412 ms and needs no optimisation. `bootStableMarker` is only
  set ~10 s after the startup services finish, so rebooting the device within ~20 s of a boot puts the next one into
  safe mode with that delay (worth knowing when timing boots).
- SD-specific behavior:
  - `_initHW()` configures `SD_CARD_DETECT_PIN` as `INPUT_PULLUP` when available
  - `initPlaylistMode()` short-circuits to `PM_WEB` without calling `sdman.start()` when `SD_CARD_DETECT_PIN` reports
    slot-empty during SD boot
  - `changeMode()` short-circuits SD mode switches the same way, avoiding the SPI retry path when the slot is empty
- Key interaction:
  - almost every module reads/writes through `config`.
- Theme loading note:
  - `Config::loadTheme()` now uses `memcpy_P(&theme, &_themes[config.store.themeId], sizeof(ThemeData))` — loads from
    PROGMEM `_themes[]` array at runtime. No longer uses compile-time `COLOR_*` macros.
  - `applyInvertTitle()` removed from Config (moved to Display).

## `src/core/startup.h` / `startup.cpp`
- Boot-only orchestration module following the standard core `class + global instance` pattern (`Startup startup;`).
- **Boot stability is a state, not a timer.** `Startup::_services` is `SVC_NONE` / `SVC_WILL_RUN` / `SVC_DONE`, and
  `setup()` has already settled which one applies by the time `loop()` can run, because `startupServices()` has exactly
  one call site — [`main.cpp:142`](src/main.cpp:142), inside `setup()`. `loop()` then has three exits: `SVC_NONE` proves
  the boot over `BOOT_STABLE_TIME` from power-on (nothing risky will run, but an early crash must still trip Safe Mode
  on the next boot); `SVC_WILL_RUN` **while the card is still in use** proves it after `STARTUP_ASYNC_SERVICES_DELAY +
  BOOT_STABLE_TIME`, with the reason `startup services were suspended`; and `SVC_DONE` marks it stable
  `BOOT_STABLE_TIME` after the downloads finish. Two of those count from power-on, each safe for its own reason:
  `SVC_NONE` because no risky work is scheduled at all, and the parked case because the services delay is waited out
  first, so a task that was only slow to start has started by then.
- **The three boot outcomes, and why the state can always be settled before `loop()`:** *SD offline*
  (`network.offlineMode || config.store.SDoffline`) never calls `checkSafeMode()`, so `_bootStablePending` stays false
  and the marker is not touched at all — that is the long-standing workaround and it is deliberate. *No WiFi / soft AP*
  returns early from `setup()` before the services call, leaving `SVC_NONE`, and the boot then proves itself over
  `BOOT_STABLE_TIME` from power-on. *Connected* records `SVC_WILL_RUN` before the task is created, and the task sets
  `SVC_DONE`, so a download that finishes quickly cannot be missed.
- **SD playback suspends the services, and the boot is still proven.** The task parks in `while (cardInUse())` until the
  card is free, and `loop()` marks the boot stable once `STARTUP_ASYNC_SERVICES_DELAY + BOOT_STABLE_TIME` has passed
  with the task still parked — `Boot stable after ~20000 ms - 10 s after the startup services were suspended`. The first
  version waited indefinitely for the park to end, and its consequence was the opposite of what it looked like: a device
  that booted into SD playback never proved its boot, so the next boot came up in Safe Mode once, and safe mode forces
  web mode — meaning **SD mode could not survive a restart at all**, and smartstart for SD was effectively ignored every
  time. Nothing risky runs in a parked boot, which is what justifies the mark; the delay is waited out so that a task
  merely slow to start is not mistaken for a parked one.
- **Two earlier designs were replaced:** one fell back to the power-on count when `_servicesDoneMs == 0`, marking the
  boot stable exactly as the services started; another kept a `BOOT_STABLE_BACKSTOP_MULT` timer that could call a boot
  stable while the risky window was still open. Both are gone.
- The startup services are a known high-risk path: three concurrent TLS sessions against ~75 KB of internal heap, which
  has been observed to exhaust it and crash the boot. See `.github/code-issues.md` section 7.
- Two accessors publish that state: `servicesBusy()` is true only inside the task body (the VU limiter and `mqtt.cpp`
  want exactly that), while `servicesPending()` is `SVC_WILL_RUN` and so also covers the park and the countdown. The
  wider one is what to hold another job back with - `netserver.cpp` holds its radio-browser click on it.
- Owns startup-time helpers that were previously mixed into `config.cpp`:
  - boot-time version marker and required LittleFS/WebUI file verification (`checkLittleFSandVer()`) — the verification
    itself lives in `Utility::verifyLittleFS()`, see the LittleFS notes below
  - **The partition-size lookup penalty belonged to SPIFFS; the migration to LittleFS removed it** (this supersedes the
    old "prefer the 8 MB table" conclusion). Under SPIFFS on sh1106_vs1053_3buttons: a missing-name probe cost ~187 ms on
    the 3.38 MB partition in `builds/partitions/default_16MB.csv` and ~87 ms on the 1.5 MB `default_8MB.csv`;
    `SPIFFS.begin()` mount was 229 → 103 ms; the `netserver.begin()` file cache 797 → 382 ms - one 2.25x factor, three
    measurements, because SPIFFS proved a name by walking the partition's lookup structures. LittleFS resolves a name by
    walking the directory's entry chain and mounts from a superblock, neither of which scales with partition size (the
    measured mount win, 229 → 27 ms, is exactly that). **Partition-table choice is therefore no longer a
    filesystem-speed decision.** Size still governs free-block headroom and wear rotation (1.5 MB is ~384 blocks of 4 KB,
    3.38 MB ~864), so the larger table is mildly better for wear and has more room for the ~300 KB peak demand
    (`code-issues.md` section 3.2). The only size-sensitive call left is space accounting (`usedBytes()`/`totalBytes()`
    traverse the filesystem rather than counting) - measure rather than assume if that matters.
  - **Web-file verification is one listing pass, not N lookups.** `Utility::verifyLittleFS()` in `utility.cpp` lists
    `/www` once, matching every listed name against `Config::wwwFiles` as it goes, so it stores no names at all — only
    two bitmasks (plain seen, `.gz` seen) — and returns whether each required file is present in either form. Its single
    write is the duplicate rule: a plain file whose `.gz` twin exists is removed. It replaced two identical per-file
    probe loops (`requiredWebFilesExist()` in `startup.cpp` and `Config::_wwwFilesExist()`), which together cost **7.48
    s of a 23.2 s boot** — 33 lookups each, run twice, because the second one re-answered a question
    `checkLittleFSandVer()` had already answered. `config.wwwFilesExist` is now computed once there and reused by
    `initPlaylistMode()`.
  - **The filesystem is LittleFS, and four of its differences from SPIFFS are load-bearing** (migrated from SPIFFS; full
    reasoning in `plans/spiffs-to-littlefs.md`):
    - **The partition label is `littlefs` but the partition SubType stays `spiffs`.** LittleFS resolves its partition
      through `esp_vfs_littlefs_register()`'s `partition_label`, so `FS_PARTITION_LABEL` / `FS_MOUNT_POINT` in
      `config.h` are passed explicitly to `LittleFS.begin()` — the Arduino default is still the legacy label `spiffs`.
      The CSV SubType cannot follow: `gen_esp32part.py` rejects `littlefs`, `esp_partition_subtype_t` has no `LITTLEFS`
      value, and PlatformIO matches only the literal strings `spiffs`/`fat`/`littlefs` when locating the FS offset for
      `buildfs`/`uploadfs`. Keeping `spiffs` (0x82) also keeps `Update.begin(size, U_SPIFFS)` resolving, so the
      filesystem-image upload needs no custom `esp_partition` code.
    - **Directories are real, so `/www` and `/data` must exist.** `checkLittleFSandVer()` calls `LittleFS.mkdir("/www")`
      and `mkdir("/data")` on the common path taken by both a successful mount and a fresh format, because every
      `/data/*` write (playlist, wifi, version marker) and every `/www/*` write needs its parent to exist. SPIFFS
      inferred the directory from a path prefix; LittleFS does not. The built image happens to supply `/data` (the
      source tree ships `data/data/wifi.csv` and `playlist.csv`), but the `format` command erases both directories,
      hence the mkdir.
    - **A root listing yields directory entries, not files.** `LittleFS.open("/")` returns only `www` and `data`, so
      `verifyLittleFS()` lists `/www` directly and `pruneLittleFS()` walks `/www` and `/data` explicitly, comparing
      basenames. The old "walk `/` once" shape would have reported every www file missing on every boot, and would have
      pruned nothing.
    - **`rename()` no longer overwrites an existing target.** Every call site removes the destination first
      (`importWifi()`/`saveWifi()` and `cleanPlaylist()` in `utility.cpp`), and `ESPFileUpdater` does
      `exists`→`remove`→`rename` internally, so the invariant holds — but any new rename must preserve it.
    - Names that changed with it: `clearspiffs` → `clearfs`, the `updatetarget` value `spiffs` → `littlefs`,
      `checkSpiffsandVer`/`verifySpiffs`/`pruneSpiffs` → `checkLittleFSandVer`/`verifyLittleFS`/`pruneLittleFS`,
      `SPIFFSTIMELOG` → `LITTLEFSTIMELOG`. The emergency form in `netserver.h` deliberately stays **firmware-only** and
      keeps its hidden `updatetarget=fw`: the `/update` handler can also target the FS partition via `U_SPIFFS`, but
      that writes raw flash under a mounted LittleFS with no `LittleFS.end()` first and no image-size or partition-table
      validation, so it is not offered on a recovery page. The hidden field also matters because `getParam()` returns
      nullptr when the parameter is absent, which is why that dereference is now guarded. Build side,
      `board_build.filesystem = littlefs` selects `mklittlefs`, and both gzip `extra_scripts` key off
      `$BUILD_DIR/littlefs.bin` — a stale `spiffs.bin` reference there silently disables the compress/restore step.
    - **There is no SPIFFS → LittleFS upgrade path.** A full flash (bootloader + partitions + firmware) is required; an
      OTA firmware-only update leaves an unmountable partition because the in-flash partition table still carries the
      old label. Saved `playlist.csv` / `wifi.csv` do not survive.
    - **Boot-time FS cost, and the two fixes that followed the measurement.** Measured on the SH1106/VS1053 build,
      LittleFS is 8.5x faster to mount (229 → 27 ms) and much faster on lookup-miss-heavy paths, but roughly 4–25x more
      expensive per small file operation, so total boot regressed 15402 → 15747 ms. Two changes claw that back, and both
      help under either filesystem: `StaticFileCache::loadOne()` no longer calls `exists()` before `open()` (open() is
      the existence test, and a `size > 0` guard covers the core quirk the FS health check documents), and the FS health
      check now runs only when `bootStableMarker` says the previous boot did **not** prove stable — the same condition
      `checkSafeMode()` uses to enter Safe Mode. The single biggest regression was `netserver.begin` (the 17-file PSRAM
      cache load) at 798 → 1284 ms.
    - **The verify pass and the update churn are deliberate, not oversights.** `checkLittleFSandVer()` lists all 17 www
      files via `verifyLittleFS()`, and then `netserver.begin()` opens them again in `loadAll()`. The second pass is
      knowingly kept: presence must be known at `main.cpp:73`, before `initPlaylistMode()` reads `config.wwwFilesExist`
      at `main.cpp:108` and before `loadAll()` runs at `main.cpp:110`, and the early `pruneLittleFS()` / version-marker
      write inside `checkLittleFSandVer()` depends on it as well. Folding the two passes would mean reordering boot
      stages across `main.cpp`, `startup.cpp`, `config.cpp` and `netserver.cpp` — reviewed and declined. Cost is 385 ms
      under SPIFFS and 422–555 ms under LittleFS.
    - **`ESPFileUpdater`'s `.meta` sidecars and the churn they cause are load-bearing.** Each updated file costs a
      `.tmp` create plus appends, then `exists`→`remove`→`rename`, then a `writeMeta()` of `localPath + ".meta"` — and
      the `NOT_MODIFIED` path still rewrites that `.meta` just to bump its timestamp, so **every boot rewrites one
      `.meta` per checked URL even when nothing changed**. That is the intended mechanism for deciding whether a
      download is needed, so it stays. LittleFS pays it synchronously (a metadata commit, often with a block erase)
      where SPIFFS deferred the cost to GC — which is the whole of the perceived "slower file operations" difference.
    - **`pruneLittleFS()` is an update-scoped reset, not a periodic cleaner — and deleting the `.meta` sidecars is part
      of that by design.** Its trigger is `!config.wwwFilesExist || !LittleFS.exists(VERSION_PATH)` at
      `startup.cpp:252`, so it runs in exactly three situations: a **firmware version change** (a mismatch sets
      `wwwFilesExist = false`), a **new install** (no version file present), or **a missing www file while the version
      still matches** (the version-match branch sets `wwwFilesExist = verifyLittleFS()`, so one absent asset alone is
      enough). It never runs on a healthy boot — all 17 files verify present and the version file exists, making the
      condition false, which the boot logs confirm. Because the version marker is rewritten on every prune, the
      version-change case fires once per firmware change and not repeatedly. Clearing everything derived at that moment
      is deliberate: besides the `.meta` sidecars it also removes `/data/index.dat`, `/www/searchresults.json`,
      `/www/search.txt`, `/www/curated.json`, `/www/pl_import.json` and `/data/new_ver.txt`, all of which are
      rebuildable — so a firmware change starts from a clean slate instead of mixing stale bookkeeping with new assets.
      The only cost is that intact assets get re-fetched on that one boot. The third trigger is self-limiting for the
      same reason: prune rewrites the marker with the same version, the files come back, and the next boot verifies
      clean.
    - **LittleFS has no fsck and needs none, and its one maintenance knob is unreachable.** Consistency is resolved
      atomically at mount (the force-consistency step inside `LittleFS.begin()`), and reclamation is continuous, so
      there is no repair or defrag pass to schedule — which is precisely why `begin(false)` falling back to
      `begin(true)` is the complete recovery path, and why gating the read/write health check behind `bootStableMarker`
      is safe. The closest thing the design has to periodic self-maintenance is `block_cycles`, the wear-levelling
      relocation threshold, and it cannot be set from here: arduino-esp32 3.2's `LittleFSFS::begin()` exposes only
      `formatOnFail`, `basePath`, `maxOpenFiles` and `partitionLabel`. The real ongoing risk is **free space**, not
      corruption — see `.github/code-issues.md` section 3.2.
  - **`pruneLittleFS()` is a repair, not a verifier, and must never run on the boot path.** It deletes everything
    outside the `wwwFiles`/`dataFiles` keep lists — which includes `/www/searchresults.json`, `/www/search.txt`,
    `/www/curated.json`, `/www/pl_import.json` and `/data/new_ver.txt` — so it stays on the missing-files and
    version-mismatch paths (and the `clearfs` command). It was formerly `cleanupSpiffs()`, renamed so the pair reads as
    what they are: one verifies, one prunes.
  - loading saved SSIDs from `/data/wifi.csv` into `config.ssids`
  - stale search-result cleanup under `/www/searchresults.*`
  - required WebUI asset download and recovery flow
  - version-file parsing for online-update detection
  - startup background update scheduling (`startupServicesAsync`) — spawned as a FreeRTOS task on `NETWORK_CORE` at low
    priority:
    - waits `STARTUP_SERVICES_DELAY` seconds before any work, letting audio buffer fill first
    - verifies WebUI locale JSON file; downloads if missing
    - checks for new firmware version via `new_ver.txt`; triggers OTA if `autoupdate` is enabled
    - downloads default `playlist.csv` from `PLAYLIST_DEFAULT_URL` if file is missing
    - updates `timezones.json.gz` and `rb_srvrs.json` from online sources
    - cleans stale search results older than 24 hours
    - deletes the `ESPFileUpdater` param and self-terminates via `vTaskDelete(NULL)`
  - `deassertCsPins()` — called from `main.cpp` `setup()` before any device init. Sets all known SPI CS pins
    (`VS1053_CS`, `SD_CS`, `TFT_CS`, `TS_CS`) to `OUTPUT` + `HIGH` to prevent floating CS from causing bus contention
    during peripheral detection.
  - safe mode boot crash-loop detection (`checkSafeMode`, `markBootStable`, `loop`): reads NVS key `bootstablemark` at
    boot — if the previous boot did not complete successfully it sets `Startup::_safeMode` for this session, which
    suppresses the automatic version-check/autoupdate and the automatic smartstart playback, so the device does not
    auto-reconnect to a crash-causing stream. The stored `smartstart`/`autoupdate` values are deliberately **not**
    overwritten: they feed the WebUI (`GETCONTROLS`, `GETSYSTEM`) and **Note the marker is now set by the state machine
    above, not by a fixed uptime**: `BOOT_STABLE_TIME` after the startup services finish when they will run, or over
    `BOOT_STABLE_TIME` from power-on when they will not run.
  - `icon()` — the boot-mode glyph for the boot dots line: the SD pair (`\030\031`) when `network.offlineMode` or
    `SDoffline`, PAUSE (`\034`) when the previous boot never proved itself, PLAY (`\035`) for smart start, VOL_75
    (`\026`) otherwise. The boot screen is its **only** consumer, and the sampling rule still applies:
    `Display::_bootScreen()` reads it while the screen is built, because `display.init()` in `setup()` runs *before*
    `checkSafeMode()`, which is what clears `bootStableMarker` — a later read would report PAUSE on every boot. The
    widget keeps the returned literal for the session, which is why the glyphs here must stay string literals.
- Coupling:
  - drives `utility` for shared update/download helpers
  - reads Config-owned asset allowlists during required-file recovery
  - updates `netserver.newVersion` / `newVersionAvailable`
  - stops playback and drives `display` during required-file recovery
  - `main.cpp` calls `startup.startupServices()` after network/server startup
  - `network.cpp` calls `startup.initNetwork()` during WiFi credential load

## `src/core/network.h` / `network.cpp`
- `network.h` declares `MyNetwork` state and API; states: `CONNECTED`, `SOFT_AP`, `FAILED`, `SDREADY`.
- Connectivity, periodic scheduling (`ticks`), weather provider logic, and time sync.
- Main responsibilities:
  - STA connect + optional strongest RSSI/BSSID mode
  - AP fallback with DNS captive portal
  - Improv provisioning flow
  - WiFi reconnect/disconnect handlers
  - periodic ticker logic for:
    - time sync interval
    - weather sync interval
    - screensaver timing
    - RSSI updates
    - SD card hot-insert detection (when `SD_AUTOPLAY && SD_CARD_DETECT_PIN!=255`): polls `SD_CARD_DETECT_PIN` every ~2
      s in `divrssi` block; calls `config.changeMode(PM_SDCARD)` on insertion
  - weather provider dispatch:
    - `OM1` Open-Meteo
    - `OW25` OpenWeather 2.5
    - `OW30` OpenWeather 3.0
  - weather cache and formatting logic
  - centralized runtime logging for reconnect/weather/boot progress/time-sync via `FUNCTIONLOG`/`SERIALLOG`/`BOOTLOGX`
  - web-stream reconnect now resumes through `player.resumeLastWebSource()` so direct URL sources can recover via
    `/data/laststation.url` instead of always falling back to `lastStation`
- **The boot Wi-Fi path (`network.begin()` → `wifiBegin()`).** With `config.store.wifiscanbest` it runs one synchronous
  `WiFi.scanNetworks()` and orders the candidates by RSSI, pinning the BSSID; otherwise it walks the saved SSIDs in
  order. Each candidate gets a **time-based** ceiling of `WIFI_ATTEMPTS * 500` ms (8 s) while polling `WiFi.status()`
  every `WIFI_CONNECT_POLL_MS` and printing a dot. That ceiling has to cover association **and** the lease, because
  `WL_CONNECTED` is raised only on `ARDUINO_EVENT_WIFI_STA_GOT_IP` — 0.5 s or 4.8 s after association on this hardware.
  Association is therefore logged separately, from `WiFi.RSSI()` going non-zero (`Associated after Nms (RSSI x) -
  waiting for the address`), and a candidate that hits the ceiling is retried **once** in place (`WiFi.disconnect(false,
  false)`, wait for RSSI to clear up to `WIFI_SETTLE_MS`, `WiFi.begin()` again) before it is abandoned. Exhausting the
  list returns false and `network.begin()` raises the SoftAP.
  - **What the scan costs.** `WIFI_SCAN_DWELL_MS` (options.h, default **120**) is passed as `max_ms_per_chan`, but
    `WiFiScan.cpp` hardcodes `scan_time.active.min = 100`, so it is a **ceiling**: per-channel cost is dominated by
    fixed overhead (~320 ms of it, ~441 ms per channel measured at 120), which makes a 13-channel scan ~5.7 s.
    `wifiBegin()` scans a **second** time at the IDF default 300 only when the configured dwell finds nothing, so the
    old behaviour stays the last resort and only a failure there reaches the SoftAP — which is what makes a lower dwell
    safe.
  - **`WIFI_CONNECT_POLL_MS`** (options.h, default **100**) replaces the old hardcoded 500 ms poll, which quantised
    every join measurement to half a second. The per-candidate ceiling is time-based rather than attempt-based precisely
    so a shorter poll cannot silently shrink the 8 s budget.
  - `WiFi.setSleep(false)` is applied in `setWifiParams()` **after** a successful connect; nothing clears the
    modem-sleep policy before a connect.
  - The `SCANNINGWIFI` message is sent with `display.putRequestDelayed(..., DSP_BOOTMSG_DELAY_MS)` (**2000** ms) so the
    firmware version on the boot line stays readable instead of being replaced the moment the scan starts.
  - **WARNING - do not re-add an asynchronous scan.** `WiFi.scanNetworks(true)` returns `WIFI_SCAN_FAILED` (-2) after
    2.5-6 s on this build and leaves the radio unable to scan at all afterwards: every later synchronous scan returns 0
    networks in 2-12 ms, and `scanDelete()` does not clear it, so the fallback cannot recover and the boot lands in the
    SoftAP. Three variants failed identically - no driver call during the scan, dwell 120 or 300, and leaving the
    modem-sleep policy alone. The ~860 ms it would save is not worth a radio that stops scanning.
  - **WARNING - a pinned static address was tried on hardware and removed; do not re-attempt it.** The feature (an
    `SMART_STATIC_IP` `#ifdef` in `network.cpp`) remembered the DHCP lease once a boot had proved itself, applied it
    before the join and judged the join on the wire instead of waiting for `WL_CONNECTED`. It worked and it was fast
    (~4.6-4.8 s to Ready against ~8.7 s on plain DHCP), but **this stack cannot prove an address is free**: lwIP here
    has no ACD (`LWIP_ACD` / `LWIP_DHCP_DOES_ACD_CHECK` are absent from every target's `sdkconfig`, so it cannot be
    switched on without rebuilding the prebuilt IDF libraries), and the hand-rolled ARP probe is defeated by lwIP itself
    — **an ARP reply whose sender IP equals the netif's own address is treated as us**, so no foreign entry is ever
    cached under our address and `etharp_find_addr()` can never see the competing host. With a PC holding that exact
    address, first manually and then as a DHCP reservation, the radio still logged "nobody else holds it" and used it.
    What the probe does prove is that the address is bound and the gateway answers; it does not prove uniqueness, and
    the failure mode is a **silent LAN collision**, not a slow boot. An address is only safe as a router reservation by
    MAC or one outside the DHCP pool, and the firmware cannot verify that — which is why it is gone rather than shipped
    with a caveat. Facts the work left behind: `WL_CONNECTED` is set in exactly one place, on
    `ARDUINO_EVENT_WIFI_STA_GOT_IP` (`WiFiGeneric.cpp:1095`); a `WiFi.config()` issued before the connect only fills
    esp_netif's *stored* copy while the lwIP netif still reads `0.0.0.0`; and a **down** netif cannot transmit at all
    (`etharp_request()` returning `ERR_OK` means "accepted for transmission", not "sent"). Also not to be re-tried:
    re-applying the address after association, "kicking" the DHCP client to raise the netif, and priming every connect
    attempt.
  - **The join budget: two attempts per candidate, and the second is twice as big.** `WIFI_ATTEMPTS * 500` = 8 s has to
    cover association *and* the lease, because `WL_CONNECTED` is GOT_IP and the late-GOT_IP mode measures 4.8 s on its
    own — that mode is one lost DISCOVER, as lwIP retransmits at about +0, +4 and +12 s, so a window can be spent on
    nothing but lost DISCOVERs and still be called a failure. Each candidate therefore gets exactly two windows: 8 s as
    before, then a retry of `WIFI_ATTEMPTS * 500 * WIFI_RETRY_SCALE` (options.h, default **WIFI_RETRY_SCALE 2** = 16 s).
    Only after the second failure is the next candidate tried, and only after that the SoftAP — worst case ~25 s per
    candidate against ~17 s before. The retry is a **real restart, not a re-association**: the shared helper
    `wifiRestartForRetry()` does `WiFi.disconnect(true, false)` (radio off, stored credentials kept), waits for the
    teardown (RSSI back to 0, bounded by `WIFI_SETTLE_MS`), then re-begins the same AP with the same channel/BSSID. That
    matters because `WiFi.disconnect(false, false)` leaves the radio up, so the STA netif can stay up, esp_netif never
    stops the DHCP client, and the retry inherits the accumulated DISCOVER backoff (next retransmit 16-32 s away)
    instead of starting a fresh ladder. Both connect paths call the one helper so they cannot drift. The association
    line (`Associated after Nms (RSSI x) - waiting for the address`) splits association from lease, and the retry logs
    `No address after Nms - restarting the network and trying <ssid> once more`. The deferred boot-line message cannot
    land on the AP screen — it fires at 2 s, long before any of this.
  - **SoftAP fallback, and its reboot timer.** `raiseSoftAP()` brings up the AP, the DNS captive portal and Improv; the
    optional auto-reboot is `if (SOFTAP_REBOOT_DELAY > 0) rtimer.once((uint32_t)SOFTAP_REBOOT_DELAY * 60, rebootTime);`,
    in minutes. `SOFTAP_REBOOT_DELAY` (options.h, default **0** = never) is the only control: the WebUI field, the
    stored `softapdelay` key (purged by `deleteOldKeys()`), the `softap` command and the `"softr"` field of the
    netserver GETSYSTEM payload were all removed, so re-enabling the reboot means editing options.h/myoptions.h.
  - **Build/size.** `sh1106_vs1053_3buttons` (8 MB partition) builds clean at **1,832,333** bytes flash and **72,236**
    bytes RAM with `BOOTLOG_TIME` off — 308 bytes *smaller* than the version before the retry rework, because the shared
    `wifiRestartForRetry()` helper replaced two inlined copies of the restart code. A build that dies at the `.bin` step
    (e.g. the uploader holding `firmware.bin`) leaves a stale sconsign, so the next build can link stale objects and
    report a plausible-looking size; delete `.pio\build\<env>\src\*.o` and rebuild rather than trusting it.
  - `retryStreamConnection` task (40 fast attempts, then a slow infinite tail) is cancelled through
    `MyNetwork::cancelStreamRetry()`, the single owner of `streamRetryTaskHandle` (called by commandhandler on
    playback-changing commands, by `player.prev()`/`next()`/`toggle()`, and by `utility.startStandby()`); the task also
    cleans itself up when conditions change (user stops, WiFi drops, or playback resumes)
- Coupling:
  - pushes display updates (`display.putRequest(...)`)
  - calls player/netserver hooks
  - reads/writes `config.store`
- Successful connect handling now stays internal to `network.cpp`; there is no remaining app-level weak
  `network_on_connect` callback.

## Network Recovery (the stall, the reset ladder, and the boot-stable marker)

Everything follows from one measured fact: **`Audio::connecttohost()` runs on the main task**, so a failing
stream connect blocks `loop()` for its whole timeout and the WebUI looks dead meanwhile.

**Two routines, not one.** `wifiReconnectionTask()` owns the *link* (`WIFI_STA_DISCONNECTED`, gated by
`network.beginReconnect`, cadence 5/10/30 then 60 s capped); `retryStreamConnection()` owns the *stream*
(stream died with the link up, gated by `network.lostPlaying`, cadence 1/3/6/12/15 s for 40 attempts then
60 s forever). Neither gives up, and a wedged stack raises no disconnect event - so the Wi-Fi routine never
runs, the stream task can only replay the same failing connect, and that is why the forced reset exists.

**The traps around that reset.** It is `WiFi.disconnect(true, false)` - never `eraseap = true` - and its budget
must be a **file-static**, because `WiFiReconnected` recreates the task and a local gives each instance a fresh
budget (four back-to-back teardowns, measured). The handle must be cleared before `vTaskDelete`, and
`WiFiLostConnection` must use `network.lostPlaying || player.isRunning()`, never an assignment: during a reset
nothing is playing, so an assignment wrote `false` and the Wi-Fi returned to a silent radio.

**One spawner, and a ladder that cannot lose its task.** Everything that starts `retryStreamConnection` - the
arm gate in `player.loop()`, `WiFiReconnected`, and now the self-heal - goes through **`spawnStreamRetry()`**
(`network.cpp`). The check-and-claim is a critical section, because two callers could otherwise win the race and
leave a stray task running with the handle already cleared; more importantly the `xTaskCreatePinnedToCore` result
is checked, because `player.loop()` sets `network.lostPlaying = true` **before** creating the task and nothing
else clears that flag except a fresh Wi-Fi reconnect event - so a silent failure closed the arm gate until a
reboot. `ticks()` (the 1 ms Ticker heartbeat) self-heals the same way: if `lostPlaying` is set, no task is
servicing it, `!beginReconnect` and the link is up, it re-spawns, rate limited by `STREAM_RETRY_RESPAWN_MS`
(options.h). The `wifiReconnectionTask` spawn is checked and logged too, but has no equivalent fallback: the AP is
otherwise only re-found on the next disconnect event.

**Why the VS1053 build depended on this ladder more than the I2S build.** Both libraries retry a dead stream
themselves every 5 s (`streamDetection()` -> `connecttohost(m_lastHost)`), but only the VS1053 failure branch
erased the host (`m_lastHost[0] = '\0'`), which turned every later library-level retry into `connecttohost("")` -
a guaranteed failure for the rest of the session - while the I2S backend kept the URL and kept retrying it. I2S
therefore had two independent recovery paths and VS1053 had **one**, which is why a stall here only ever showed on
the VS1053 build as "the stream will not come back and other streams fail until a reboot". The erase is gone and
`streamDetection()` now returns early when the host is empty or NULL.

**Refusal vs wedge is decided by duration.** At or past the connect bound means a stale path; far below it means
the peer refused and the link has just proved itself. Only the ambiguous case probes, and only
`NET_STACK_WEDGED` resets: no link means the Wi-Fi routine owns it, a gateway TCP answer means the stack is fine
and the host refused, and both probes silent while the driver reports connected means **WEDGED - reset at the
first failure**. DNS is not part of it - it goes through the same stack, so it is the same evidence.

**Manual Wi-Fi and the boot marker.** `captureCurrentAp()` runs at GOT_IP *and* at the end of
`MyNetwork::begin()` (the handlers register after the boot connect), letting `wifiBeginFast()` reconnect by
BSSID/channel with no scan and giving up after `WIFI_FAST_ATTEMPTS`, since a moved AP needs the scan and a reset
does not. `WIFI_SETTLE_MS` applies only when `WiFi.mode()` reports a change, and an empty scan is retried once.
`Startup::deferBootStable()` re-stamps the countdown while unproven, and no-ops once settled.

**Instruments, all unconditional.** `MAIN_LOOP_STALL_MS` names the worst stage of a slow `loop()` (this is what
identified `player+saves`), `NVS_SLOW_WRITE_MS` logs a slow NVS commit, `[Heap]` gives internal free **and the
largest contiguous block** (what a TLS handshake needs), recovery logs under `Network`, and `Telnet::loop()`
must never block. Every cadence and threshold above is a macro in `options.h`, **except the connect bound**:
`m_connectTimeout_ms` / `_ssl` in `src/libraries/I2S_Audio/Audio.h` (1200 ms), separate from
`CONNECT_HTTP_HTTPS_TIMEOUT`, which doubles as the socket READ timeouts and must stay patient for slow streams.

**The reload storm is the trigger, and the guard was withdrawn.** A reload opens a new websocket before the old
closes (22 clients, 3 KB heap); the `MIN_MALLOC` guard in `onWsEvent()` went away because that websocket carries
every setting to `script.js` (`setupElement`), so a refused client shows the page its defaults and the user
thinks the radio forgot everything. The loader-side wait (`plans/network-recovery.md` §F11) was not built.

## `src/core/player.h` / `player.cpp`
- `player.h` declares player command queue, playback API, and status.
- Audio engine integration and playback sequencing.
- Main responsibilities:
  - initialize codec/I2S/VS1053
  - queue command handling (`PR_PLAY`, `PR_STOP`, `PR_VOL`, etc.)
  - station play/stop/toggle/next/prev flow
  - exact-match-first URL playback routing for `playurl` / preview resume (`queueResolvedUrl`, `resumeLastWebSource`)
  - volume conversion (`volToI2S`) including ES8311 path
  - SD/web mode specific playback behavior
  - error reporting and display/net updates
  - command queue depth: `xQueueCreate(10, ...)` — increased from 5 to prevent queue overflow during rapid mode-switch
    sequences (SD→web transitions) where multiple commands (PR_STOP, PR_PLAY, PR_VUTONUS) arrive before the first
    finishes processing.
  - direct playback lifecycle side effects for `rgbled` and `backlightControls` (start/stop + initial stopped-state
    sync)
  - `mute()`: volume-0 toggle backed by the private `_muteVol` member; uses raw `getVolume()`/`setVolume()` so
    `config.store.volume` and the displayed volume are deliberately untouched. Shared by the physical mute buttons, the
    `mute` command, and the IR mute button — the `DSP_DUMMY` suppression lives only at the physical-button call site.
  - `prev()` / `next()` / `toggle()` delegate retry cancellation to `network.cancelStreamRetry()` (previously duplicated
    inline).
  - **`setReady()`: the text a mode shows before anything has started playing in it - the one place a mode entry decides
    the title lines.** Called from `Config::changeMode()` (both directions, beside the `NEWSTATION`/`NEWTITLE` requests
    it already sends), from `SDManager::trySdRemount()`, and from the SD manager's close. It clears the player error and
    then writes one of three things: the no-SD-card text when the boot is SD-offline with no card mounted, `[ready]`
    when `config.station.url` is non-empty, and **nothing** when there is no station at all - an emptied playlist or an
    empty card keeps the blank title lines of the "ehRadio and nothing else" state, because `[ready]` there would
    promise a start that cannot come. Each mode had grown its own copy of this idea (the pager build in `display.cpp`,
    two in `sdmanager.cpp`, the stop branch in `commandhandler.cpp`) and the mode switch had none, so a switch kept
    whatever the mode being left had written: a failed card file leaves `Error connecting to <file>`, a web stream
    leaves its song title. The two remaining copies are deliberate - `display.cpp`'s runs before the playlist is loaded
    (the url test would be meaningless) and `commandhandler.cpp`'s is a LOST/PLAYER state decision, not a mode entry.
- VS1053 SPI: `Player::Player()` constructor passes `&VS1053_SPIBUS` to the `Audio(CS, DCS, DREQ, SPIClass*)`
  constructor. `VS1053_SPIBUS` is the `SPIB` or `SPIA` object resolved by `options.h`. No `SPIClass` declared in
  `player.cpp` or `player.h`.
- Coupling:
  - updates display queue and websocket state
  - uses `config` station and mode state
  - interacts with radio-browser click reporting (clicks are gated on `!network.lostPlaying` so automatic retry
    reconnections do not fire clicks; only explicit user-initiated plays do)
  - calls `rgbled` and `backlightControls` directly during playback start/stop

## `src/core/audiohandlers.h` / `audiohandlers.cpp`
- Callback bridge used by the audio libraries.
- `audiohandlers.h` now declares the `AudioHandlers` module and the required free `audio_*` callback symbols;
  `audiohandlers.cpp` owns the implementation as a normal core translation unit.
- Converts decoder callbacks into:
  - metadata updates
  - title/station updates
  - bitrate/codec updates
  - error updates
  - SD EOF behavior
- Important for title/bitrate side effects to WebUI and display.
- Owns runtime artwork state and policy:
  - parses `StreamUrl='...'` from `audio_info(...)`
  - accepts `audio_icylogo(...)` as a fallback image source
  - exposes the filtered `image_url` to MQTT through `audioHandlers` getters
- Uses shared utility helpers for string normalization instead of keeping those helpers in `config.cpp`.
- Audio info/bitrate/ID3 notifications are emitted through shared logging macros so telnet+serial output stays
  consistent with the rest of the firmware log contract.

## `src/core/display.h` / `display.cpp`
- `display.h` declares Display class and display mode/change API.
- The screensaver's two extra widgets are `Display::_ssvu` and `Display::_sstext` - the only widgets no layout
  provides. `_screensaverWidgets()` brings them up on **every** entry into `SCREENSAVER`, not once at boot,
  because the meter's segmentation comes from the active layout's `bandsConf` and the info line's size from
  `metaConf`, and the WebUI stays reachable while the device is asleep. While the meter is up the clock is
  `lock()`ed (both `ClockWidget::draw()` and `_draw()` bail on `_locked`), so `_time()` also skips its random
  walk. `_ssContentH()` is the panel less the line's strip - one text row plus 1% of the panel height, rounded
  up, above and below it - and that figure is the floor the walking clock is clamped to. `_sstext` is created
  and re-init'd with the `"*"` separator, so its wrap-around joiner reads `" * "` like `_screensaverLine()`'s own
  joins; the separator is per widget (built as `" %c "` in `ScrollWidget::init()`), and the weather widget is the
  one that keeps `"~"`, matching the `" ~ "` inside the weather string. See
  `plans/screensaver-panel-ownership.md`.
- **The screensaver owns the panel**: `_weatherHidden()` and `_clockHidden()` both answer for
  `config.isScreensaver` before anything else, because the WebUI is reachable while the device is asleep and
  both predicates are consulted from `_applyState()` (which runs its lock lines before its isScreensaver early
  return) and from `_layoutChange()` via `SHOWVUMETER`. The weather widget belongs to the player page alone and
  must never be drawn over the screensaver - `_screensaverLine()` is what carries it there. That method is
  guarded on `_ssStripH()` first and on the lock second, because a *refused* strip is locked rather than inactive
  and `ScrollWidget::setText()` paints on `_active` alone - so a new metadata string (`NEWTITLE` -> `_title()`) or
  a weather refresh would otherwise paint the line the strip was refused for, leaving the content area sized as if
  the strip were gone. The clock's answer while asleep is the recorded `Display::_ssMeterUp`, set by
  `_screensaverWidgets()` on the one path that locks the clock for the meter, so the two cannot disagree; a
  derived predicate was rejected because the canvas-allocation failure path - where the clock is what the
  screensaver shows - is precisely the case a derived answer gets wrong.
- `SHOWWEATHER` and `NEWWEATHER` recompose the screensaver rather than the player page while
  `config.isScreensaver`: `SHOWWEATHER` queues `SSREBUILD` only when the strip's existence changes with the
  flag (otherwise it refreshes only the line, so a toggle costs no panel clear), and `NEWWEATHER` rebuilds when
  there is no strip yet but `_ssStripH()` has become true - nothing playing and no weather cached at entry,
  where only a rebuild can create the widget - and otherwise updates the line as before.
- Render queue + display task + widget/page orchestration.
- **Theme/layout/invert runtime switching** — major refactor:
  - `_applyState()` — central state function: loads layout from PROGMEM, loads theme from PROGMEM (TFT only), applies
    invert (color swap + `metaBGConf_ptr` switch), reinitializes widgets, redraws. Called by all three commands
    (`theme`, `layout`, `inverttitle`).
  - `_setLayoutPointers()` — extracted 30-pointer setup shared by init and runtime paths.
  - `applyLayout()`, `applyTheme()`, `applyInvertTitle()` — thin wrappers that set config then call `_applyState()`.
  - `inverttitle` is runtime boolean (`config.store.inverttitle`). On TFT: swaps meta↔metabg colors + switches to
    `metaBGConfInv` layout. On OLED: color swap only. Uses conditional rendering at widget init time — no theme
    mutation.
  - `getThemeListJson()` / `getLayoutListJson()` — serve dropdown data to WebUI from PROGMEM `_themeNames[]` /
    `_layoutNames[]`.
  - `#ifndef HIDE_*` compile guards removed from `_reinitWidgets()` and `_buildPager()` — all widget init now uses
    runtime null-checks.
  - `_buildPager()` no longer calls `_reinitWidgets()` — state application is handled by `_applyState()`.
  - `_start()` calls `_buildPager()` then `_applyState()` — widgets created then initialized with correct state.
  - `DSP_INVERT_TITLE` compile-time macro eliminated.
- Buffer bar terminology was aligned to actual behavior:
  - `_heapbar` -> `_bufferbar`
  - `heapbarConf` -> `bufferbarConf`
- Input-buffer bar values still come from `player.inBufferFilled()`; only visual normalization changed via
  `BUFFERBAR_VISUAL_FULL_PERCENT`.
- Battery widget support is runtime-guarded: `_battery` null-check at every call site. Display configs that don't
  support battery leave `batteryConf` zeroed (height=0) — no compile-time guard needed.
- **Which confs may be empty, and what happens then.** `height == 0` is a valid "none" for `apTitleBGConf` (the boot
  band, so `{ }` is how a panel declines one — `displayOLED128x32conf.h` does) and for the two reference lines
  `underLineConf` / `overLineConf`: **no widget is made for an empty conf**, and `_syncLineRule()` in `display.cpp` can
  still create it later, when a layout switch brings one in. That is what `Page::addWidgetFirst()` is for — a lazily
  created under line would otherwise be appended and painted over the text it exists to sit beneath. By contrast
  `metaBGConf`, `metaBGConfInv` and `playlBGConf` are allocated unconditionally on purpose: their confs are re-pointed
  behind the widget's back (`_applyMetaInvert()`, and `_plbackground`'s per-mode `setHeight()`), so a zeroed conf there
  is a state rather than an absence. Everywhere else zeros simply draw nothing, a zero rect being one cheap `fillRect`.
- **The meta fill pair belongs to `LayoutData`; the boot band to `BootData`.** `metaBGConf` / `metaBGConfInv` are
  alternatives selected by *invert title* (`Display::_applyMetaInvert()`, `display.cpp`), never both, and with **no
  fallback**: an empty `metaBGConfInv` means invert mode draws no bar, which is how a layout declines one. Both are
  plain fills drawn in `config.theme.metafill`, except that **TFT only** replaces `metafill` with `theme.div` first when
  inverted; OLED has no such override and its drivers point `metafill` at a foreground shade (`oledcolorfix.h`,
  `displaySSD1322.cpp`), so a band there reads as a solid block rather than a tint — hence the OLED confs carry a
  hairline or `{ }` in `metaBGConf` and the bar in `metaBGConfInv`, the reverse of a yoRadio conf. `_apScreen()` and
  `_sdmanScreen()` draw `_bootConfig.apTitleBGConf` and ignore *invert title*, so a setup screen is independent of the
  selected layout (its text geometry already came from `_bootConfig`). `conf_tool.py` normalises an imported pair by
  height for an OLED target, derives `.apTitleBGConf` from it, and asks OLED-or-TFT only when it is creating a new conf,
  since a source file cannot say which family it was written for. **`metaBGConf`/`metaBGConfInv` are deliberately absent
  from `BootData` and `apTitleBGConf` absent from `LayoutData`**: same-named fields in both structs would make
  `emit_import_entry()` treat them as boot-only and blank them in every imported layout.
- **The big-number screen is one code path, and it owns the panel while it is up.** `VOL`, `NUMBERS` and `SDCHANGE`
  all draw through `Display::_showNumbers(header, value, fmt, dialogPage)`, which also performs the shared-row tidy-up
  the volume screen has always done: it locks and clears the weather (that line shares the bottom row with the IP text
  on several layouts), hides the battery and re-activates the RSSI (the other shared row), and repoints the IP line.
  `dialogPage` selects the variant - true switches to `PG_DIALOG`, where the panel wipe leaves the number alone with
  the header on the meta line; false is the volume overlay, drawn over the live player page. A negative value blanks
  the number widget, which is how `SDCHANGE` starts before `SDFILEINDEX` delivers a count. Checking a layout's number
  geometry from the volume screen therefore covers all three.
  `_ownScreen()` is what makes that safe: it is true for `SDCHANGE`, `NUMBERS` and `VOL` with `volumepage` on, plus
  the active file manager, because a page switch only stops the *page pass* - the clock is drawn directly by
  `_layoutChange()`/`redrawIfVisible()`, and the self-drawing requests write to widgets that live on `PG_PLAYER`, so
  neither obeys the page system. The volume *overlay* is deliberately excluded: it shares a live player page, so its
  title, RSSI and battery have to keep updating, and the row tidy-up is what keeps the number legible there.
  One request is exempt from that drop, and the exemption lives in `_drawsOverOwnScreen()` rather than in the list:
  `DRAWVOL` *is* the volume screen's content - `_volume()` updates the volume bar, the footer text and the number
  itself - so it passes while `VOL` or `NUMBERS` is up and stays dropped on the manager and the card-change screen,
  where the player's volume row would paint over them. Dropping it for the volume page was a real regression: the
  number and the bar froze for as long as that page was up, because the only thing that refreshes them is that
  request. That is also why the predicate is a member function now - it has to know which screen is up.
  One request is exempt from that drop, and the exemption lives in `_drawsOverOwnScreen()` rather than in the list:
  `DRAWVOL` *is* the volume screen's content - `_volume()` updates the volume bar, the footer text and the number
  itself - so it passes while `VOL` or `NUMBERS` is up and stays dropped on the manager and the card-change screen,
  where the player's volume row would paint over them. Dropping it for the volume page was a real regression: the
  number and the bar froze for as long as that page was up, because the only thing that refreshes them is that
  request. That is also why the predicate is a member function now - it has to know which screen is up.
- **A page switch does not stop a widget drawing itself.** `Pager::setPage()` fills the panel with the background and
  activates one page, which stops the *pager* drawing the others - but a widget asked to draw directly paints wherever
  it is, because nothing tells it that its page went inactive. The SD File Manager is the case that exposes it:
  `enter()` stops the player, and that stop (`PSTOP`/`SHOWVUMETER` → `_layoutChange()`) re-evaluated `_clockHidden()`,
  which hid a yielded Big-VU clock only while `player.isRunning()` was true; the clock was therefore unlocked and
  redrawn over the manager's page, and then never ticked again, `case CLOCK` being gated to `PLAYER`/`SCREENSAVER`.
  The manager therefore *owns* the screen: `_clockHidden()` and `_weatherHidden()` return true while
  `filemanager.active()`, `drawsOverManagerScreen()` in `Display::loop()` drops `PSTART`, `PSTOP`, `SHOWVUMETER`,
  `SHOWWEATHER`, `NEWWEATHER`, `NEWTITLE`, `NEWSTATION`, `DRAWVOL`, `SHOWBUFFERBAR`, `DSPRSSI`, `DSPBATTERY` and
  `NEWIP` while it does, and `_switchMode(PLAYER)` calls `_layoutChange(player.isRunning())` after its page switch so
  the dropped state is re-derived from the live player on the way back. The card-change wait screen (`SDCHANGE`) needs
  the same treatment for the same reason - it is a holding pattern whose only changing content is the index counter -
  so both screens are now decided in one place, `Display::_ownScreen()` (`_mode == SDCHANGE || filemanager.active()`),
  and `drawsOverManagerScreen()` became `drawsOverOwnScreen()`. `_ownScreen()` drives that drop and the hiding of both
  the clock and the weather, which matters because their refresh paths are not mode-gated (the title, the IP line and
  the RSSI/battery icons have their own paths too), and neither the clock nor the weather is advanced outside
  `PLAYER`/`SCREENSAVER`: a widget left visible would be painted once by the page pass and then sit frozen for the
  whole wait - including the common case where the index is valid and no counter ever appears. Leaving the mode
  redraws the clock in full, because `_layoutChange()` prints it when a widget comes back from hidden.
- Main responsibilities:
  - initialize rendering task and widgets
  - mode switching (`PLAYER`, `VOL`, `STATIONS`, `LOST`, `UPDATING`, screensaver)
  - draw station/title/weather/clock/VU/bitrate/playlist
  - update progress bar during update flow
  - battery indicator rendering
- Coupling:
  - depends on `config.store` for many visual toggles
  - reads `network` time/weather, `player` status
  - title changes now directly trigger `rgbled.trackChange()` and `backlightControls.restart()` instead of a weak hook

## `src/core/netserver.h`
- Declares request enums, websocket/server globals, `StaticFileCache` class, `CachedFile` struct, and NetServer API.
- `StaticFileCache` — PSRAM-backed cache for static WebUI files (files from `Config::wwwFiles[]`).
- `CachedFile` — per-file entry: URL path, plain data pointer, gzipped data pointer, content-type.
- Contains embedded fallback HTML templates (`emptyfs_html`, `index_html`, `emergency_form`).

## `src/core/netserver.cpp`
- HTTP + WebSocket + upload/update + search/curated orchestration.
- Main responsibilities:
  - static file serving from PSRAM cache (via `StaticFileCache`) — no LittleFS reads during HTTP serving
  - fallback to LittleFS for dynamic files not in cache (`searchresults.json`, `curated.json`, etc.)
  - `handleNotFound`: checks PSRAM cache first for GET requests, cache-busting `?v=` stripping removed (max-age=60
    handles freshness)
  - `handleIndex`: serves `player.html` from PSRAM cache for `GET /`
  - route handlers (`/`, `/search`, `/update`, `/locale.json`, `/ready`, etc.)
  - `/visuals.json` — the VU style list, built **at request time** from one static `{ id, label, needsPcm }` table, so
    the WebUI select can only offer what the running build can draw: waveform and Lissajous are omitted on a VS1053
    because they need PCM, and the labels for every other style are identical on both backends. The ids are `vuStyle_e`,
    the same numbers that travel in `vustyle=<n>` and come back in `GETSCREEN`. There is deliberately **no box-based
    filtering**
  - `fileCache.loadAll()` called in `begin()` before `webserver.begin()`
  - `invalidateCache()` public method exposed for `utility.cpp` runtime updates
  - `Cache-Control: max-age=60` for all cached static files (was 3600)
  - `/settings.html`, `/update.html`, `/ir.html` no longer served via `index_html[]` — handled by PSRAM cache
    fallthrough
  - websocket command parsing and outbound updates
  - state request queue processing (`GETSYSTEM`, `GETSCREEN`, `GETLOCALE`, etc.)
  - IR websocket helpers: `irToWs()` (protocol + code) and `irValsToWs()` (the active button's 3 codes, read through
    `config.irCodes()`)
  - online update check/start tasks
  - radio-browser search and curated task management
  - exact-match-first preview/add handling on `/search`; unmatched preview now uses the same direct URL playback path as
    `playurl` instead of a mutating playlist scan
  - centralized logging for search/curated/playback/radio-browser-click/update/not-found paths via `FUNCTIONLOG`
  - radio-browser click reporting holds the queued click while the startup services are pending instead of letting the
    heap check drop it. `processRadioBrowserClick()` leaves `clickDelayActive` set and re-tests every pass, capped at
    `RADIO_BROWSER_SEND_CLICK_DELAY + rbClickServicesWaitMs` (60 s, the same protection `Mqtt::servicesWaitLimitMs`
    gives its re-apply). The services run on this very core and hold the DRAM, so a spawn inside that window was
    refused as `low heap, refusing rb click task spawn` on every boot that played a radio-browser station. The click is
    still dropped outright while the card is off limits (`PM_SDCARD`, or the SD File Manager) - the older
    `Abandoning click` rule, extended to the manager.
  - the RSSI payload carries the raw dBm (`rssi`, kept for the bars' tooltip) and the bar count (`rssibars`, from
    `rssiLevel()`), so the WebUI holds no scale of its own
  - `GETBATTERY` sends `volt: NmV, percentage: N%`. When `BATTERY_FORCE_DISPLAY` is defined it synthesises that same
    string instead of reading `Battery` - percentage clamped, voltage interpolated across the presence window - so the
    WebUI battery row can be laid out with no battery fitted
- Coupling:
  - uses `cmd.exec(...)` from commandhandler
  - emits JSON consumed by `data/www/script.js`
  - `GETSCREEN` now also carries `invtitle`, `layout`, `theme` **and `vustyle`** fields for WebUI dropdown state
  - Virtual endpoints `/themes.json` and `/layouts.json` serve dropdown data from PROGMEM arrays; `/visuals.json` is the
    first *runtime* one (built per request, not from PROGMEM), and the WebUI loads it through `loadVisuals()` +
    `populateNamedDropdown('vustyle', data)`
- Readiness detail:
  - `/ready` returns `{"ready":true}` only when `netserver.bootReady` is true, required web files exist, and network
    state is stable (`CONNECTED` + `WL_CONNECTED`, or `SDREADY`).
- OTA note:
  - OTA start/end/error callbacks use `FUNCTIONLOG`.
  - OTA progress now uses `FUNCTIONLOG` (line-oriented output, no raw `\r` streaming path).

## `src/core/commandhandler.h` / `commandhandler.cpp`
- `commandhandler.h` declares command execution API for command strings.
- The three screensaver-composition commands (`screensavertext`, `screensavervu`, `screensavervustyle`) are the
  only members of the screensaver family that also ask for `GETACTIVE` again, because they change which rows
  the Screen section shows. `GETACTIVE` sends `group_vu_ss` unconditionally inside the VU `#if` (the VU
  Screensaver checkbox, which needs no VU box in the layout), `group_vu_ss_style` only while `screensaverVU`
  is on (the style picker *and* the meter's own peaks/axis switch, which share the group because both belong
  to the screensaver's meter), and withdraws `group_full_time` while it is on; `group_vu` keeps its own
  meaning, "the active layout has a VU box". `GETSCREEN` carries `scrtext`, `scrvu`, `scrstyle` and
  `scrpeak`.
- `screensavervupeak` is deliberately not `vupeaks`: `screensaverVUpeak` is a store of its own, and the
  widget reads it through `VuWidget::_vupeak()` whenever the instance is the pinned one - the same split
  `_style()` makes for `screensaverVUStyle` against `vustyle`. It owns the peak markers and every axis or
  reference line in every style, which is why the label is "VU Meter Peaks / Axis" on both rows.
- A screensaver setting is applied where it is visible. Every `screensaver*` command shares one helper that,
  while the device is already in `SCREENSAVER` or `SCREENBLANK`, queues an `SSREBUILD` carrying the mode the
  blank switches now describe - the same choice [`network.cpp`](../src/core/network.cpp) makes when the
  countdown fires - instead of waking the device to the player. `SSREBUILD` calls
  `Display::_enterScreensaver()` directly rather than going through `_switchMode()`, which cannot serve it:
  the first guard on that function's fifth line drops a request for the mode the device is already in, and the
  second drops any request while the network is transient, and either one leaves the old picture running.
  `_enterScreensaver()` holds what `_switchMode()`'s screensaver branch used to do, including lighting the
  panel again so a rebuild out of `SCREENBLANK` shows the picture rather than a dark screen. The two enable
  switches (`screensaverenabled`, `screensaverplayingenabled`) still return to the player, because with the
  governing one off the screensaver should not be up at all - and that exit is the route that resets
  `screensaverTicks`/`screensaverPlayingTicks`. The cost is the panel clear a rebuild goes through, one full
  blit of black before the rebuilt picture; that clear is what owns the whole panel when a box shrinks.
- Central command router for WS, URL params, MQTT, and telnet fallback paths.
- Main responsibilities:
  - map `key=value` commands into config/player/display/network actions
  - request websocket state snapshots
  - persist settings with `config.saveValue(...)`
  - source-aware command policy (`WebSocket`, `HttpUrl`, `Mqtt`, `Telnet`) and shared non-WebUI blocklist checks for
    HTTP/MQTT/Telnet ingress
  - `Commands.md` at the repository root is this file's own reference, deliberately **in the same order and the same
    blocks** - that promise is what makes an audit a walk down two lists. `isBlockedForSource()` is the only
    authority for its *Blocked in HTTP/MQTT/Telnet* column. Beware the names that are not commands:
    `config.cpp`'s `CONFIG_KEY_ENTRY` keys are what a setting is stored under, and `vumeter` / `vupeak` happen to
    exist as both. Six commands had gone undocumented before the 2026-10-07 audit
    (`plans/commands-md-audit.md`), which is why the procedure below ends with them.
  - own shared command aliases across ingress channels (`playstation`/`play`, `boot`/`reboot`, `vol+`/`volup`,
    `dim`/`brightness`, `dspon`/`screenon`)
  - player-command parity helpers (including exact-match-first direct URL playback command routing for `playurl` /
    `burl`)
  - trigger curated operations and locale update tasks
  - cancel the stream retry task (`network.cancelStreamRetry()`) before executing user-initiated playback commands
    (`stop`, `playstation`, `prev`, `next`, `toggle`, `startstandby`, `burl`, `mode`, `submitplaylist`) so explicit user
    actions always interrupt automatic reconnection loops
  - `stopstandby` / `startstandby` delegate to `utility.stopStandby()` / `utility.startStandby()`; the `mute` command
    maps to `player.mute()`
  - IR recorder commands: `irbtn` resolves a button **name** via `config.irButtonByName()` (`-1` stops recording and
    saves), `chkid` selects the slot, and `irclr` clears a slot through `config.clearIR()`
- Critical coupling file for setting changes.
- New commands: `theme` (theme switching), `layout` (layout switching), `inverttitle` (invert title toggle). All persist
  via `saveValue` and trigger `display._applyState()`.

## `src/displays/conf/conf_tool.py`
- **One tool, two modes** (it replaced `importlayout.py`, which is gone): `--clean` repairs every `display*conf.h` in
  its own directory; `--import <file> --name "X" [--target file.h]` converts a community yoRadio conf file, appending a
  layout or creating the file when the target does not exist; `--dry-run` in both. There is deliberately no bare
  invocation - no mode prints a two-line pointer and exits 2, and `-h`/`--help` prints the module docstring, which is
  the only copy of the help.
- **The master is `widgetsconfig.h`.** `struct LayoutData` and `struct BootData` supply the field order, each field's
  type (which decides `{ }` versus `false`), the section headers, and the per-field comments. The tool keeps no list of
  its own, so a field added to the struct reaches every conf on the next run. The hand-maintained `LAYOUT_FIELDS` in the
  old importer was already stale - no `underLineConf`, `overLineConf` or `rotateVU` - and that is exactly how the files
  drifted.
- **The conf-file invariant, now enforced:** `_layoutNames[]` and `_layouts[]` are the same length and order (the index
  is the layout's position in both, and is persisted in `config.store.layoutId`); every layout entry writes every
  `LayoutData` field and `_bootConfig` every `BootData` field, booleans as `false` and structs as `{ }`; fields are in
  declaration order, which the compiler requires of a designated initialiser; and the section headers (`SCROLLS`,
  `SLIDER BARS`, `LINES + RECTANGLES`, `WIDGETS`, `CODEC BADGE`, `VU BANDS`, `MOVES`, `TRANSFORMS`) come from the
  master. The five booleans are per layout and always written, so only their value carries meaning.
- **Comments survive the rewrite.** Trailing comments are kept verbatim (`// unused`, `// <--------- NEEDS EDITING!`,
  `// clock disappears when VU is on`); a commented-out alternative stays on the side of its field - the `//
  .bufferbarConf = {...}` under `{ }, // unused` and the `//.clockConf` above the live line are both in the shipped
  files; a note above a field travels with the field below it unless a blank line ends the group, in which case it
  belongs to the field above; a comment naming a field travels with that field; `// ??? (Unused by ehRadio) ...` travels
  with the field it followed.
- **A missing `metaConf` or `playlistConf` is never repaired.** Dialogs write into the meta line and the playlist page
  is built on `playlistConf`, so the tool warns loudly, leaves that entry byte-identical, and lists the file as NEEDS
  HAND EDITING. No layout is ever removed: that would shift every index after it and the index is persisted.
- **Writes are two-stage.** Both modes write `<file>.new.h` (the `.h` extension so it reads in the editor), print a
  per-file report and a summary, and install only on `Update ...? [y/N]`; declining offers to delete the temps;
  `--dry-run` writes the same temps but never installs. `--clean` is idempotent - a second run reports every file
  unchanged.
- Import mode keeps the old format's behaviour: `BOOMBOX_STYLE` yields two entries with `.boomboxVU = true` on the
  BoomBox one; `HIDE_*` zeroes a config and keeps the original commented beneath it; `HIDE_IP_ONLY_MAIN_SCREEN` and
  `RSSI_DIGIT` become `true` switches; the `BITRATE_FULL`/`TITLE_FIX` block is stripped and substituted; an obsolete
  `fadespeed` sixth value is dropped from `bandsConf` (a sixth plain integer only, so an expression is never lost); OLED
  targets get `metaBGConf`/`metaBGConfInv` swapped; and anything the source did not provide is written as `{ }` with `//
  <--------- NEEDS EDITING!`.
- Docs: [`conf_tool.md`](../src/displays/conf/conf_tool.md) covers both modes,
  [`layout_key.md`](../src/displays/conf/layout_key.md) documents every field and now states the write-every-field and
  keep-the-headers rules.

## `src/displays/themes.h`
- Runtime theme switching data, loaded from PROGMEM at runtime.
- `ThemeData` struct: 35 `uint16_t` color fields + `playlist[5]` — mirrors `config.h` `theme_t`.
- `const ThemeData _themes[] PROGMEM` — array of theme presets (default + imported).
- `const char _themeNames[][32] PROGMEM` — display names for WebUI dropdown.
- `RGB(r,g,b)` macro packs 8-bit RGB into RGB565 `uint16_t`.
- Populated by `importtheme.py` script.
- **`.line` and `.vuaxis`: the two entries with no old-format source, both derived from `.div`.** `line` sits between
  `.div` and `.weather`, `vuaxis` between `.weather` and `.vupeak`, and their place in the entry is fixed because the
  entries are designated initialisers. The rules are part of the palette now, not values to eyeball: **`.line` =
  `.div`** (the divider's ink) and **`.vuaxis` = `.div` × 0.25** (truncated per channel, like every other computed
  colour).
  - `.vuaxis` was originally set to `.clockbg` in all 13 themes; that is what the merged tree carried, and it is wrong
    in principle and in fact: the axis is a *reference line* and `.clockbg` is a *background* shade, so the copy left
    the axis nearly invisible in Graphite (10,10,10), UltraPerfect (0,0,0), White and Black (229,229,229 on a 255
    background), Ocean (0,0,62 on 0,0,91) and vip-cxema (29,29,0). A quarter of the divider keeps contrast on black and
    on white alike (63 for a white divider, 22 for Graphite's 91).
  - Consumers, which is why both had to be wired as well as recalculated: `.vuaxis` is the seventh argument of
    `VuWidget` and colours every reference line it draws (centre cross, bar baseline, histogram line, Spectrum divider
    and mirror divider) — `display.cpp` was constructing and re-initialising the widget with **six** arguments, which
    does not compile against the seven-parameter signature in `widget_vu.h`, and the colour reached nothing. `.line`
    colours `_underline` / `_overline`, the two `FillWidget`s built from a layout's `underLineConf` / `overLineConf` in
    `_buildPager()` and re-initialised on a layout switch beside the other fills; the merge declared the members and
    added them to the player page but never created them, so they were always null.
  - Reduced palettes override the rule, deliberately: `displaySSD1322.cpp` sets `.line = GRAY_9` (equal to its `.div`)
    and `.vuaxis = GRAY_3`, since a four-shade grey palette has no quarter of `GRAY_9`, and `oledcolorfix.h` sets both
    to `TFT_FG` because a one-bit panel has a single ink.
  - **The two lines are one widget type at two depths, and the depth is where each one is added.** `underLineConf` and
    `overLineConf` are `FillConfig`s rendered by `FillWidget` in `theme.line`, and either can be a frame rather than a
    fill (they honour `outlined`). A page draws in two stages: `Page::setActive()` walks its own `_widgets` then its
    `_pages` in insertion order (`pages.cpp:103`). So the **under line** is added with `addWidgetFirst()` (the first
    widget of `PG_PLAYER`, behind the meta band and the text), while the **over line** goes to `_overLinePage`, attached
    to `PG_PLAYER` after `_footer`, which makes it the last thing that page draws, footer row included. Consequences:
    the footer cannot cross the over line, and a widget `_reinitWidgets()` creates on a layout switch (VU, title2,
    weather, volbar, bufferbar) lands below the over line too. `_overLinePage` is a child of `PG_PLAYER`, not of
    `_footer`, because the footer is shared with `PG_DIALOG`. `Page::loop()` descends only into `_widgets`, never into
    `_pages`, so nothing in a sub-page can be self-updating - which is why the footer holds only bars and text.
  - **`outlined` in `FillConfig` now means the shape for a fill, and still means "frame plus inset" for the two
    sliders.** `FillWidget::_draw()` (`widgets.cpp:31`) reads it and draws `drawRect` instead of `fillRect`, in the same
    colour the widget was constructed with - `theme.line` for the two reference lines, `theme.metafill` for the meta
    band and the boot band, `theme.plcurrentfill` for the playlist highlight - and leaves the interior alone on purpose,
    because a frame that cleared its inside would erase the widgets it exists to enclose. `false` is what every conf in
    the tree writes, so no existing layout changed; and since a rectangle one pixel thick is its own outline, `outlined`
    does nothing visible to a hairline, which is the one thing to tell a layout author. `SliderWidget::init`
    (`widgets.cpp:418`) keeps its own reading, used twice: the `drawRect` frame in `_draw()` and the inset `innerWidth =
    _width - _outlined * 2` in `_drawslider()`. `SLIDER BARS` is exactly those two fields, and they take two theme
    colours (`volbarin`/`volbarout`) where a fill takes one. The master's section comment in `widgetsconfig.h` - both
    copies, the `BootData` one and the `LayoutData` one - now reads `outlined` instead of `false`, as do all 37
    occurrences across the 14 conf files; the tool copies the master's headers verbatim, so a newly imported conf gets
    the corrected comment without any change to `conf_tool.py`.
  - **`metaBGConfInv` is consumed by a pointer swap, not by a second widget.** When `config.store.inverttitle` is on,
    `_applyState()` re-points `metaBGConf_ptr` at `metaBGConfInv` (falling back to `metaBGConf` when the inv conf is
    empty) and, on TFT, sets `config.theme.metafill = config.theme.div` first - so the single band widget is
    re-initialised from the inv geometry and drawn in the divider ink, which makes it read as the rule under the station
    name instead of a band. What is dead is the exported `metaBGConfInv_ptr`: it is assigned in three places (the
    initialiser, the null block for builds without a display, and `_setLayoutPointers()`) and never read, because the
    swap goes through `metaBGConf_ptr`. Recorded, not fixed.

## `src/displays/importtheme.py`
- Imports old-style `#define COLOR_*` theme files into `themes.h`.
- Handles `#ifdef`/`#ifndef` branching to generate multiple theme variants (permutation).
- Smart fallbacks for missing colors (e.g., `.dow` ← `.date`, `.battery` ← `.rssi`).
- Meta fallback for everything else (uses `.meta` color instead of zeros).
- `FIELD_ORDER` carries `line` (after `div`) and `vuaxis` (after `weather`), matching the struct: the emitted entries
  are designated initialisers, so the order is load-bearing.
- `COMPUTED_FALLBACK` derives the two new entries from `div` — `line` at 100%, `vuaxis` at 25% — and both are listed in
  `DERIVED_RULES`, which exempts them from the `// needs fixing?` marker that every other computed fallback keeps. They
  are rules, not guesses; the rest are still eyeballed.
- Name truncation at 31 chars for `_themeNames[][32]`.
- `--dry-run` writes to `.new.h`.

## `src/core/controls.h` / `controls.cpp`
- `controls.h` declares `class Controls` with public interface: `init()`, `loop()`, `setEncAcceleration()`,
  `setIRTolerance()`, `flipTS()`, `controlsEvent()`.
- `extern Controls controls;` provides the global instance; callers use `controls.init()`, `controls.loop()`, etc.
- All internal helpers (`onBtnClick`, `encodersLoop`, `irLoop`, etc.) are private class methods.
- Static trampoline methods (`_btnClickCb`, etc.) used for `OneButton` callbacks (function-pointer API; cannot capture
  `this`).
- `readEncoderISR` / `readEncoder2ISR` remain free functions with `IRAM_ATTR` (ISR constraint; access file-scope
  `encoder`/`encoder2` directly).
- Physical controls integration:
  - OneButton
  - rotary encoders
  - touchscreen gestures
  - IR remote decoding
- Converts hardware input events into same core actions used by WebUI (`controlsEvent`, player commands, display mode
  changes).
- `Controls::loop()` now calls `backlightControls.controlsLoop()` directly for non-PLAYER backlight wake behavior.
- IR record debug text now routes through centralized logging macros.
- IR dispatch is name-based: `irLoop()` iterates `config.irButtonCount()`, matches codes from `config.irCodes(button)`,
  then switches on the behaviour id from `config.irAction(button)` (`IRACT_POWER`, `IRACT_MUTE`, `IRACT_UP`,
  `IRACT_DOWN`, `IRACT_PREV`, `IRACT_NEXT`, `IRACT_PLAY`, `IRACT_MODE`, `IRACT_HASH`, `IRACT_DIGIT`). Digit buttons
  derive their value from the `n0`…`n9` key. The old positional `IR_UP`…`IR_HASH` enum is gone. Power/mute/mode are
  local actions and are allowed while offline or showing `LOST`.
- Physical mute (`EVT_ENC2_SW` / `EVT_BTN_MODE` double-click) calls `player.mute()` and keeps the `DSP_MODEL ==
  DSP_DUMMY` no-op guard at the call site.
- Learning a new IR code calls `config.clearDuplicateIR()`, which zeroes that same code in every other button and slot
  so only the newest copy survives (a code can no longer be shadowed by an earlier duplicate at match time). Empty slots
  hold `0` and are never matched. The clear happens in RAM; it is persisted by the existing `saveIR()` on recording
  stop, and `irValsToWs()` is re-sent only when something was actually cleared.
- Screensaver wake hardening:
  - `controlsEvent()` now flushes pending display requests (`display.resetQueue()`) and zeroes screensaver tick counters
    before queueing `NEWMODE, PLAYER` when waking from `SCREENSAVER`/`SCREENBLANK`, preventing one-detent rotary wake
    races where a stale queued screensaver mode request could immediately re-apply.

## `src/core/telnet.h` / `telnet.cpp`
- Telnet and serial command handling.
- Responsibilities:
  - manage client sessions
  - read input lines with CR/LF-pair handling so Enter submits immediately across CR/LF client variants and empty Enter
    events are preserved
  - apply explicit 2000 ms stream timeout configuration for serial and per-client telnet streams
  - parse command strings through `utility.parseCommandLine(...)` (`key=value`, `key value`, `key(value)`, bare URL,
    bare key), which also applies the trimming, one layer of wrapping quotes and the `play` value-shape handling
  - route commands through `cmd.exec(...)` with source `Telnet`
  - keep command handling output-minimal (no telnet-specific reporting command table)
- Important:
  - acts as secondary control channel parallel to WebUI
  - command handling is intentionally kept near-parity with MQTT/HTTP routes
  - `Telnet::printf(...)` is transport-only and no longer echoes to serial
  - `help`, `quit`, and `bye` are handled locally in telnet before commandhandler dispatch
  - `quit` / `bye` silently disconnect only the issuing Telnet client
  - empty input lines now re-show prompt (`> `), aligning interactive UX with common telnet clients
  - `Telnet::printf(...)` normalizes line endings through `utility.normalizeToCRLF(...)` (the old file-local
    `normalize_to_crlf`)
  - telnet no longer defines any private string helper: the local `normalize_to_crlf`, `trimInPlace`,
    `stripWrappingQuotes`, `startsWithHttp` and `parseTelnetCommand` were duplicates of `utility.*` and are gone. The
    one genuinely telnet-only rule - command `mode` with value `2` is rewritten to `-1`, because telnet's help text
    calls 2 "cycle" - stays at the call site immediately after the shared parse rather than leaking into the shared
    helper

## `src/core/mqtt.h` / `mqtt.cpp`
- MQTT is always compiled in and gated at runtime by `config.store.mqttenable` (WebUI Settings > MQTT). The old
  `MQTT_ENABLE` compile *gate* is gone - nothing in the firmware is conditional on it. The macro of that name now only
  supplies the *default value* of that setting (`options.h`: `#ifndef MQTT_ENABLE` / `#define MQTT_ENABLE false`, beside
  the `MQTT_HOST/PORT/USER/PASS/TOPIC` defaults), so a `myoptions.h` can have MQTT on for a factory-fresh device without
  touching code.
  - It reaches the stored value from `config.h` (`bool mqttenable = MQTT_ENABLE;`), i.e. it only applies when NVS has no
    `mqttenable` key yet. An already-provisioned radio keeps whatever the WebUI saved, which is the same behaviour as
    the other `MQTT_*` defaults.
  - Known wart: `Config::defaultSettings("mqtt")` in `config.cpp` still writes `saveValue(&store.mqttenable, false)`
    explicitly, so a WebUI "reset mqtt" turns MQTT off regardless of the macro. Every neighbour in that function uses
    its macro (`EHDP`, `MQTT_HOST`, `MQTT_PORT`, ...), so that one call should be `(bool)MQTT_ENABLE` to agree with
    `config.h`.
- Client library: `elims/PsychicMqttClient` (`platformio.ini`, `^0.2.4`), which wraps the ESP-IDF `esp_mqtt_client`. The
  first attempt used `bambo1543/MqttClientBinary`, a fork of it that had not been updated in two years; the upstream
  tree is the maintained one and its API is the closer match to the `AsyncMqttClient` this replaced. Reconnects are
  owned by the library, so there is no FreeRTOS reconnect timer.
- `mqtt.h` declares `class Mqtt` with `init()`, `publishPlaylist()` and `loop()` public; every publisher is private
  because only `loop()` calls them. `publishPlaylist()` is the one request another module raises, because a playlist's
  contents change while its topic stays the same.
- `extern Mqtt mqtt;` provides the global instance.
- **Publishing is state-driven and settled, not request-driven.** `loop()` compares the live values (station id, the
  derived state token, name, title, artwork URL) against two stored `StatusSnapshot`s - `reported` (what the broker
  holds) and `candidate` (the current combination, waiting to settle) - and publishes the candidate only once it has
  stayed unchanged for `MQTT_STATUS_SETTLE_MS` (`options.h`, 1500 ms). A station switch mutates those inputs at
  different moments - the index moves in `utility.loadStation()`, the title becomes `[connecting]`, the stream's ICY
  headers overwrite the name, artwork and title land last - so publishing every combination would tell consumers stories
  that were never true for longer than a moment (the classic one: the new station name carrying the previous station's
  title). The state token is derived from `dspon`, the player status and the connecting placeholder and lives *inside*
  that snapshot, so a play/pause or a standby change restarts the same single window instead of needing a settle
  mechanism of its own; `mode`, `ip`, `volume` and the playlist revision are tracked separately and publish as soon as
  they differ. Force requests skip the wait: `_onConnect` requests a status refresh and raises `rearmRequested`, which
  `loop()` answers with `resetReported()`, so a reconnect republishes everything the broker may have lost. This replaced
  the old model where a publish only happened because some other subsystem raised a
  `TITLE`/`ARTWORK`/`STATION`/`MODE`/`VOLUME` request - that left the retained topics stuck on a transient snapshot
  whenever the settled state produced no further request, and it republished identical payloads repeatedly.
  `resetReported()` also runs when MQTT is disabled, so a later enable republishes everything.
- `publishRetained(payload, qos, async)` is the only caller of the library's `publish()`: retained, explicit payload
  length, and it refuses an empty payload. An empty *retained* message means "delete the retained topic", so publishing
  one would wipe the state every consumer sees (Home Assistant loses the track name and artwork). This also covers the
  `length = 0` trap - the library forwards that to `esp_mqtt_client_enqueue()`, which then produces a zero-byte body.
  Everything publishes QoS 0 asynchronously except availability, which needs an acknowledgement.
- **Availability is its own topic with a last will.** `applySettings()` arms `setWill(<root>availability, 1, true,
  "offline", ...)` next to `setServer()`, and `_onConnect` requests the retained `online`, so a power cut, a crash or
  deep sleep makes the broker publish `offline` by itself. A deliberate disconnect cannot rely on that, because a clean
  DISCONNECT suppresses the will, so the disable and reconfigure paths call `publishOfflineBeforeDisconnect()` first -
  QoS 1 with `async = false`, so it is acknowledged before the client is stopped beneath it. `availabilityTopic` is a
  member for the same reason as `serverUri`: the library keeps the pointer. `loop()` also announces `online` whenever
  the session is connected and `lastReportedAvailability != 1`, so the announcement cannot be lost to a missed request:
  the first build of this shipped without that request in `_onConnect` at all, which left the previous session's
  retained `offline` in place forever while the device was plainly online, and Home Assistant dutifully showed it as
  Unavailable.
- The published topic set is one fact per topic, all retained, each published only when its value changes: `status` (the
  JSON attributes: `station`, `name`, `title`, `image_url`, `max_volume`), `state` (the bare token: `off`, `playing`,
  `idle`, `buffering`), `mode` (0 web radio / 1 SD card), `ip`, `volume`, `playlist` (a CRC32 revision of the active
  playlist file) and `availability`. The `status` and `on` numbers that used to sit inside the JSON are gone now that
  `state` exists, and `title` deliberately keeps the display text including its translations - the token is what a
  consumer or an automation binds to.
- `publishPlaylistNow()` is the only publisher that touches the filesystem (`config.SDPLFS()->open(REAL_PLAYL)` +
  `fileCRC32()`), so it runs on the loop task when `playlistRequested` is set rather than inside the request, which
  arrives from the AsyncTCP task. It publishes `none` when the file is missing. The state token asks
  `player.isConnecting()` rather than comparing the localized "connecting" placeholder here, because `dsplocale.h`
  defines its tables inside the header and including it in one more translation unit embeds another full copy of every
  locale - that mistake cost 48 KB of flash before it was moved behind the player.
- An empty title is treated as *no news* rather than as a value: at publish time `wouldEraseTitle()` holds the update
  when it would replace a title already reported for the same station with an empty one (`holdStatus()` consumes the
  change via `markReported()` so it is not re-evaluated every pass). The device blanks its title for an instant around a
  (re)connect - the play path in `player.cpp` re-runs `setTitle("")` after the first metadata has landed - and a display
  redraws a moment later, but a retained topic would keep that blank until the next song. The settle window already
  hides most of these; this covers a blank that lasts.
- **Task model (important)**: the entry points other modules call - `init()` and `publishPlaylist()`, plus the private
  publish helpers that `loop()` itself uses - only record a request (`applyRequested` plus one flag per published
  topic), because their callers are the AsyncTCP task (`async_tcp` runs the WebSocket/HTTP handlers and is enrolled in
  the task watchdog) and the WiFi event task. `Mqtt::loop()` is the single owner of the client library and is serviced
  by `netserverLoopTask` via `NetServer::loop()` (priority 2, core 0, 1 ms cadence, not watchdog-enrolled). Blocking
  `async_tcp` past the 5 s watchdog window panics the board, so `init()` must never connect inline.
- Settings changes take effect live: the `mqtt*` option commands (`mqttenable`, host, port, user, pass, topic) and the
  WiFi-reconnect path only request a re-apply - no reboot needed. The debounce is measured from the **last request**
  (`applyDebounceMs`, 500 ms), so the six commands of one WebUI "Apply Changes" collapse into a single reconfiguration
  with the final values. Measuring it from the last apply instead connected with the values from before the rest arrived
  and then connected again with the right ones, and that second `connect()` hit an already-started client - which is the
  library's `ESP_ERROR_CHECK_WITHOUT_ABORT ... esp_mqtt_client_start` line. An apply whose URI and credentials are
  unchanged against a session that is already up is skipped outright; since `connected()` is still false while a client
  is coming up, `clientStarted` is tracked separately so a reconfigure always stops the old client first. The apply also
  waits while `Startup::servicesBusy()` is true (bounded by `servicesWaitLimitMs`, 60 s): the startup downloads are
  three concurrent TLS sessions, and a TLS handshake needs a large contiguous block of internal RAM, so creating the
  MQTT task stack and its client buffer in the middle of them competes with the boot path the Network Recovery notes
  call the riskiest one. Booting is unaffected - `main.cpp` requests MQTT before the services start, so the allocations
  land in the baseline heap rather than interrupting a download.
- IDF transport logging is silenced unless `MQTT_DEBUG` is defined in `myoptions.h`: the `MQTT_CLIENT`,
  `TRANSPORT_BASE`, `TRANSPORT_SSL` and `esp-tls` tags are set to `ESP_LOG_NONE` on the first apply. A broker that is
  unreachable or on the wrong port otherwise logs an error burst on every retry attempt, while the `[MQTT]` lines
  already say what is happening - `Connecting to mqtt://host:port` names the target (that is how a wrong port is
  spotted) and `Connected`/`Disconnected` report the session. The same silencing also hides raw `esp-tls` detail from
  the stream and the updater, which still report their own failures.
- Library-specific details that shape this code:
  - `setServer()` takes a URI, not host+port, and keeps the pointer, so the `serverUri` member builds `mqtt://host:port`
    (or passes `mqtthost` through unchanged when it already contains `://`); an empty host means "do not connect".
  - `_onMessage` is registered through `onMessage(char* topic, char* payload, int retain, int qos, bool dup)`. The
    fork's `onMessageBinary(...)` overload does not exist upstream, and upstream already reassembles multipart messages
    before dispatching.
  - The payload arrives NUL-terminated but with no length, so `_onMessage` measures it with `strlen()` (a zero-length
    message returns immediately), rejects it if it exceeds `MQTT_URL_SIZE`, and otherwise copies `len + 1` bytes into
    the single-slot mailbox (`incomingPayload` + `messagePending`) and returns; a payload arriving while one is still
    queued is dropped, and the library's own client task is never held up. The copy has to happen inside the callback
    because for a multipart message the library frees its reassembly buffer as soon as the callbacks return.
  - Callbacks live in `std::vector`s, so `applySettings()` binds them only once (`callbacksBound`) - it runs again on
    every `mqttenable` change.
  - Because the library only resubscribes topics registered through `onTopic()`, `_onConnect` subscribes `.../command`
    (QoS 2) itself on every (re)connect, through a local topic buffer because `topic[]`/`status[]` belong to `loop()`.
  - `applySettings()` disconnects when MQTT is disabled or the host is empty, and reconfigures plus reconnects
    otherwise; the library's `disconnect()` waits for its own client task, which is another reason only `loop()` may
    call it.
  - `setBufferSize` is sized from the status/topic buffers so the status payload is not split, the library's client task
    keeps its default 6144-byte stack at `NET_TASK_PRIORITY` (`setTaskStackAndPriority(6144, NET_TASK_PRIORITY)`), and
    every `publish()` gets an explicit payload length. The stack is not enlarged because nothing but the library's
    callbacks runs there - commands execute on the netserver loop - and the priority stays in the network tier instead
    of above it, so a busy MQTT client cannot preempt the stream or display tasks during their TLS work.
- `reported`/`candidate` live in the object and are compared field by field (`matchesCandidate()`,
  `differsFromReported()`), never built as a local: `loop()` runs on the netserver loop task, whose stack is 4 KB on the
  non-S3 builds and is also shared with the queue work and deferred command execution. The `StatusSnapshot` local it
  replaced cost that task 812 bytes of high-water mark, which `[Core.monitor]` showed directly (5856 -> 5044 after MQTT
  was enabled).
- Private static callback methods (`_onConnect`, `_onDisconnect`, `_onMessage`) are required by the library callback
  interface; they must return immediately.
- Responsibilities:
  - connection lifecycle
  - subscribe to `.../command`
  - publish the retained topics from `loop()`: status attributes, state, mode, ip, volume, playlist revision and
    availability
  - status attributes include `image_url` (HTTP/S image-only artwork URL used by Home Assistant) and `max_volume`, so a
    consumer can scale the bare volume topic without being configured by hand
  - parse command payload forms (`key=value`, `key value`, `key(value)`, bare URL) with `utility.stripWhitespace(...)` +
    `utility.parseCommandLine(...)`, the same shared helper the telnet path uses, so `mqtt.cpp` defines no parser of its
    own (`trimInPlace`, `stripWrappingQuotes`, `startsWithHttp`, `parsePayloadToCommand` and its anonymous namespace are
    gone, and `<ctype.h>` is no longer included)
  - dispatch through `cmd.exec(...)` with source `Mqtt`, in `processPendingMessage()` on the netserver loop task
  - apply explicit non-WebUI blocklist rejections for unsupported MQTT-origin commands
- Coupling:
  - command behavior is primarily centralized in commandhandler.
  - `netserver` no longer triggers MQTT republishes from its request stream (the ARTWORK/TITLE/STATION/ITEM/MODE/VOLUME
    hooks were removed); `Mqtt::loop()` owns that decision, so the state reported to MQTT cannot depend on queue entries
    that may be dropped or coalesced.
  - artwork payload data is read from `audioHandlers`, not from `config.station`.
  - `NetServer::triggerMqttPlaylistSync()` existed only to wrap the Ticker that deferred the playlist publish, so it was
    removed along with the Ticker and the `mqttplaylistblock` wait the playlist download spun on inside the AsyncTCP
    task; a playlist import now calls `mqtt.publishPlaylist()` directly from commandhandler. `netserver.cpp`'s only MQTT
    interaction is the `mqtt.loop()` call in its loop, and `netserver.h` no longer mentions MQTT at all.
  - `HA/custom_components/ehradio/media_player.py` is the consumer shipped with the firmware: it subscribes to `status`,
    `state`, `volume`, `mode`, `ip`, `playlist` and `availability`, maps the `off` token to `MediaPlayerState.STANDBY`
    because the radio has no true power switch, and builds the station-list URL from `ip` + `mode` so the source list
    follows the SD card. Firmware and component versions move together from here on, since this release changed the
    topic contract.
- Status buffer sizing derives from `STATION_FIELD_LENGTH` plus `MQTT_URL_SIZE`, replacing the older duplicated
  browse-URL size macro.

## `src/core/utility.h` / `utility.cpp`
- Shared helper module following the standard core `class + global instance` pattern (`Utility utility;`).
- Current responsibilities:
  - `stripWhitespace(char*)` - trim leading/trailing whitespace plus `\r`/`\n` in place
  - `stripWrappingQuotes(char*)` - remove one layer of matching `"` or `'` quotes
  - `ipToStr(...)`
  - `escapeQuotes(...)`
  - shared command-ingress helpers, so the MQTT and Telnet paths own no parser of their own:
    - `normalizeToCRLF(const char* input, char* output, size_t outputSize)` - expand every lone `\n` to CRLF (was the
      file-local `normalize_to_crlf` in `telnet.cpp`, the only snake_case helper left in the core)
    - `isHttpUrl(const char* text)` - starts with `http://` or `https://`
    - `parseCommandLine(const char* input, char* command, size_t commandSize, char* value, size_t valueSize)` - the
      shared command-line parser: bare URL, `key=value`, `key(value)`, `key value` or a bare key, plus trimming, one
      layer of wrapping quotes, and the `play` value-shape handling (`play` alone becomes `start`, `play <url>` becomes
      `burl`)
  - playlist CSV parsing and station lookup/load helpers
  - WiFi credential parse/save/import helpers
  - deep-sleep entrypoints (`doSleepW`, `sleepForAfter`)
  - standby helpers `stopStandby()`, `startStandby()` and `toggleStandby()` (shared by the `stopstandby`, `startstandby`
    and `togglestandby` commands and the IR power button); `startStandby()` also calls `network.cancelStreamRetry()`
  - LittleFS file-maintenance helpers shared with startup and WebUI update paths:
    - `pruneLittleFS()`
    - `deleteMainwwwFile()`
    - `updateFile(...)`
- `parseWsCommand(...)` deliberately stays separate from `parseCommandLine(...)` and stays strict: the WebSocket
  settings path only ever receives `key=value` and must reject anything else rather than execute a bare command, so the
  two are not merged.
- `rssiLevel(int)` is the one reader of `RSSI_STEPS` (`options.h`): it returns 0-4, and `display.cpp` maps that level to
  its glyph pair while `netserver.cpp` sends it to the WebUI as `rssibars`. Changing the scale in `options.h` therefore
  moves the on-screen widget and the WebUI bars together.
- Holds small reusable scratch/state buffers (`ipBuf`, `stationBuf`) plus the sleep duration state and sleep `Ticker`;
  it still does not own playback/artwork runtime state.
- Current consumers include `audiohandlers.cpp`, `battery.cpp`, `commandhandler.cpp`, `config.cpp`, `display.cpp`,
  `mqtt.cpp`, `netserver.cpp`, `network.cpp`, `player.cpp`, `telnet.cpp`, and startup/update flows.

## `src/core/battery.h` / `battery.cpp`
- `battery.h` declares `class Battery` (real class under hardware guard; no-op stub in `#else`); `extern Battery
  battery;` provides the global instance.
- Public interface: `init()`, `bootStatus()`, `isInitialized()`, `getStatus()`, `recalcNow()`, `loop()`, `calibrate()`.
- All ADC state and helpers are private members/methods.
- Responsibilities:
  - ADC sampling (median of several reads), EMA voltage smoothing, percentage from the discharge curve
  - battery presence detection inside `BATTERY_PRESENT_MIN_MV`..`BATTERY_PRESENT_MAX_MV`, clamped the same way in
    `init()`
  - pushes `DSPBATTERY` to the display and `GETBATTERY` to the WebUI when the percentage changes
  - `calibrate()` turns a multimeter reading into a corrected `battery_adc_ref_mv`
- **There is no charge/discharge inference and no charge-status reporting.** The device cannot tell charging from
  discharging - there is no charge pin and no trend analysis - so `BatteryStatus` is percentage/voltage/present only,
  and
  the battery payload is `volt: NmV, percentage: N%` with nothing else. `Battery::formatStatusLine()` and the WebUI's
  three status spans (`battery_charging` / `battery_discharging` / `battery_idle`) are gone; the older description of
  "charge/discharge inference with candidate windows" and `applyPowerPolicy()` never existed in this code.
- Logging note:
  - battery status/debug/inference messages now use centralized logging macros (including `BATTERY_DEBUG` paths),
    replacing direct serial/telnet prints.

## `src/core/backlightcontrols.h` / `backlightcontrols.cpp`
- `backlightcontrols.h` declares `class BacklightControls` (real class under `BRIGHTNESS_PIN` + `DSP_DIMMING_ENABLED`
  guards; no-op stub in `#else`); `extern BacklightControls backlightControls;` provides the global instance.
- Public interface: `init()`, `restart()`, `controlsLoop()`.
- Responsibilities:
  - own persisted idle-dimming timer state and non-blocking brightness ramp
  - use `config.store.dimmingEnabled`, `dimmingTimeout`, and `dimmingBrightness` instead of board-only compile-time
    dimming thresholds
  - clamp the dim target to the current screen brightness, restore configured brightness, and restart the idle timer on
    explicit activity/settings events
  - centralize the former `main.cpp` backlight code without moving it into the display task owner
- Explicit call sites:
  - `main.cpp` after `config.init()`
  - `config.cpp` screen-default reset path
  - `commandhandler.cpp` brightness / dimming / display-on commands
  - `player.cpp` playback start/stop paths
  - `display.cpp` title-change path
  - `controls.cpp` loop path
  - `battery.cpp` low-battery recovery / restore path when the dimmer feature is enabled

## `src/core/rgbled.h` / `rgbled.cpp`
- `rgbled.h` declares `class RgbLed` (real class under `RGB_LED_PIN` guard; no-op stub in `#else`); `extern RgbLed
  rgbled;` provides the global instance.
- Public interface: `init()`, `isInitialized()`, `set()`, `playing()`, `stopped()`, `trackChange()`, `loop()`.
- Optional RGB LED state machine:
  - playing/stopped colors
  - track-change flashing
  - optional cycle behavior

## `src/core/sdmanager.h` / `sdmanager.cpp`
- `sdmanager.h` declares SD manager API and FS integration wrapper.
- SD lifecycle and SD playlist indexing.
- Responsibilities:
  - mount/retry/unmount — `start()` attempts up to 4 mount calls, early-returning on success (delays only between
    retries, not after success)
  - card-present checks
  - recursive scan and media file playlist/index creation
  - scan/index progress and errors now use centralized logging macros (`SERIALLOGDOT`, `ERRORLOG`)
  - **An index is only ever written complete, or not at all - and its existence means "a walk finished".** The pair is
    built as `/data/plsd.csv.tmp` and `/data/idxsd.dat.tmp` and renamed into place as the very last act of
    `indexSDPlaylist()`, so an unfinished build leaves no index at all: there is nothing to detect, nothing to clean up,
    and the next entry sees no index and rebuilds, exactly as it does on a card that has never been indexed. A pair
    already on the card is untouched, and stays usable, until that swap; the only inconsistent moment is a reset between
    the two renames, and the reader's checks catch that mismatch. `listSD()` returns `bool` and reports every way a walk
    can stop early - a directory that will not open, a failed allocation, a short write to either file, a failing
    recursion - because it cannot otherwise tell a card error from the end of a directory (`getNextFileName()` returns
    empty for both); the swap happens only when the walk finished, both files are open and the index holds exactly one
    4-byte offset per row (`index.size() == _sdFCount * 4`). Entering the SD File Manager mid-walk is what interrupts a
    build (its listing walks the card from the AsyncTCP task while this walk is on the loop task), and before all this
    it left a half-built index stamped with a footer that `playlistLength()` - index size minus the 8-byte footer,
    divided by 4 - read as a real station count, so SD mode played a truncated list. On the reader side
    `Config::initSDPlaylist()` demands `storedCount * 4 + 8 == size` in addition to the magic and the file-count
    comparison. And a build that fails is owed another pass, not forgotten: `FileManager::loop()` re-arms the debt when
    the index is still missing, after a 4 s delay and at most twice, resetting the count on a build that writes an index
    - the same "no index means build one" predicate the mode entry uses.
  - **`_endsWith()` compares the whole suffix.** It used to compare `strlen(str) - 1` characters, dropping the final
    one, so `.mp4` passed as `.mp3` (`.mp` = `.mp`) and `.ogv` as `.ogg`, `.m4b` as `.m4a`, `.flaX` as `.flac` - video
    files were indexed as tracks, which is what made a walk fail halfway with `SD write failed at .../...mp4` on a card
    holding both.
- **Two transports, one object.** `SDManager`'s base class is `fs::SDMMCFS` when `SD_USE_MMC` is defined and `fs::SDFS`
  otherwise (`SDMAN_FS_BASE` in `sdmanager.h`). Both derive from `fs::FS`, so `sdman` is still consumed as `fs::FS&` by
  `Config::SDPLFS()` and `Audio::connecttoFS()` — no caller changes and no `_SDplaylistFS` changes.
  - SPI branch (unchanged behaviour): `SDREALSPI` macro resolved at compile time — `SPIB` when `SD_SPI == 'B'` and
    `SPIB_SCK` defined, otherwise `SPIA`. Both buses are initialized in `Config::init()` before `SDManager::start()`
    runs. No `SPIClass` declared in `sdmanager.cpp`.
  - SDMMC branch: `setPins()` is called with 3 pins (1-bit) or 6 pins (4-bit, chosen from `SDMMC_D1==255`), then
    `begin("/sdcard", mode1bit, false, freq)`. `mode1bit` must match the pin count or the framework rejects the config.
    No SPI bus is touched; `ERRORLOG("SDMMC mount failed")` fires if all retries fail.
  - **Never name an `fs` type unqualified here.** `FSImplPtr` is `fs::FSImplPtr`, declared inside `namespace fs` in
    `FS.h`, which re-exports only `FS`, `File` and `SeekMode` to the global namespace. The unqualified form used to
    compile purely because `SD.h` ends with a bare `using namespace fs;` — and that line is its own, not the framework's
    convention: `SD_MMC.h` closes its namespace and declares `extern fs::SDMMCFS SD_MMC;` with no using-directive, so
    `SDManager(FSImplPtr impl)` failed only on the MMC branch, as `expected ')' before 'impl'` on the parameter (the
    base class above it resolved fine, which is what makes the message look misplaced). The constructor parameter and
    the `sdman` definition are qualified, and `sdmanager.h` includes `<FS.h>` itself rather than inheriting it from
    whichever transport the branch picks. Adding `vfs_api.h` to the header would also "fix" it, but only via that
    header's own `using namespace fs;`, at the cost of leaking it plus `FSImpl.h` and the POSIX headers into every TU
    that includes `sdmanager.h`.
- `cardPresent()` is transport-specific: SPI probes `sectorSize()`/`readRAW()` (via `diskio_impl.h`, now wrapped in `#if
  !defined(SD_USE_MMC)`), while SDMMC uses `cardSize() > 0` because `SDMMCFS` exposes no raw-sector API.
- `cardPresentStable()` is the DEBOUNCED probe and the ONE owner of the strike count: one failed raw read is a busy
  card, not a removal, so it reports the card gone only after `SDMAN_CARD_GONE_STRIKES` (3) consecutive failures and
  resets the count on any success. The probe is rate-limited to `SDMAN_CARD_PROBE_MS` (1 s) inside SDManager, so the
  player's `PR_CHECKSD` (every 2 s from `network.ticks()`) and the manager's `loop()` cannot double-count one busy
  second. `grantPresenceGrace()` resets the count and holds the probe off for `SDMAN_CARD_CHECK_GRACE_MS` (3 s):
  `filemanager`'s `leave()` calls it, because the card has just been written to and remounted and the first raw read is
  the one that used to fail. `presenceProbeAllowed()` gates the player's check on that window.
- **`player.cpp`'s `PR_CHECKSD` used to act on ONE un-debounced probe**, calling `sdman.stop(); config.changeMode(PM_WEB);`.
  A card still busy after a delete batch (or the remount `leave()` does at `SDSPISPEED`) timed out that single read, so
  pressing Done in the SD File Manager dropped the whole mode to web (`playmode: 0` then `sdman.stop` then
  `initPlaylistMode`, one after the other in the log). It now asks the shared `cardPresentStable()`.
- The three probe macros live in `sdmanager.h`, not `filemanager.h`, because SDManager owns the probe.
- SD CS pin is `SD_CS`. Guard macro: `#if SD_CS!=255`. The value `254` is the SDMMC sentinel and must never be handed to
  a GPIO call (`Startup::deassertCsPins()` skips it, and `Config::bootInfo()` logs SDMMC pins/mode instead of
  `SD_SPI`/`SD_CS`).
- Coupling:
  - consumed by config/player for SD mode.

## `src/core/filemanager.h` / `filemanager.cpp` (SD card manager mode)
- Runtime-only browse/edit mode for the SD card, compiled under `#ifdef USE_SD`. The header owns `SDMAN_AUTO_EXIT_MS`
  (default 180000), guarded by `#ifndef` there rather than added to `options.h` (Rule #3), so a `myoptions.h` can still
  override it; the card-probe macros belong to `sdmanager.h`, which owns the probe.
- `FileManager filemanager;` is the global instance. `enter()` returns bool and stops the player, mounts the card if
  needed, sets the active flag and asks the display for `SDMAN`; `leave(bool resumeAudio = true)` clears the flag, asks
  for `PLAYER`, grants the presence grace and - under SmartStart - gives the audio back; `loop()` (called from
  `main.cpp`) asks the shared debounced probe, redraws the countdown once a second and runs the idle timeout.
- **A mount from scratch refuses the mode when there is no card.** `enter()` returns false, logs one line and does
  nothing else - no stop, no display change, no `_active` - so the player keeps playing and the display stays put.
  `hEnterApi()` answers 409 `no_card` for that. The mount is attempted BEFORE the stop only on this path: an already
  mounted card must not be remounted until the player is stopped, because the step-down discards every handle.
- **The mode hands playback back on exit, but only what it took.** `enter()` records `player.isRunning()` in
  `_wasPlaying`, **on the transition into the mode only**, because SmartStart must not turn the user's own stop into
  playback - the manager is not a play button. The transition-only part is not tidiness: loading the page more than
  once (a reload, a second tab) calls `/sdman/enter` again, and doing the stop and the capture on every call broke the
  resume twice over. That used to be guaranteed rather than merely possible - the page handed itself over to "/" on
  load and called this again from there - and the page now keeps its own address instead, so the rule survives as the
  guard for the reload case. The second capture read the player the first call had
  already stopped, so `_wasPlaying` became false and `leave()` did nothing at all; and the second `PR_STOP` ran
  `_stop()` again, which overwrote `config.sdResumePos` with the position of a stopped player. Everything else in
  `enter()` (the on-demand mount and the clock refresh) stays idempotent on purpose.
  `leave()` then reissues `player.resumeLastWebSource()` for `PM_WEB` or `{PR_PLAY, config.lastStation()}` otherwise,
  the same two branches as `stopStandby()` and the smartstart block in `setup()`. The card needs no extra state to
  continue where it left off: `_stop()` saves the byte offset in `config.sdResumePos` and the play path consumes it, and
  an offset that no longer lands after an edit is left to the player's self-healing (failed connect, index rebuild).
  Done and the idle timeout both resume; only the card-gone exit calls `leave(false)`, because the media it would play
  from is what vanished.
- **A changed card never resumes.** `_cardChanged`, set by every mutation through `markCardChanged()`, suppresses the
  **card** resume in `leave()` only. A station NUMBER indexes `playlistsd.csv`, so after a delete it points at a
  different file - and the re-index that follows picks a station at random (`initSDPlaylist()` calls
  `_randomStation()`),
  which makes an automatic card resume meaningless either way. A web stream is untouched by card edits, so SmartStart
  still hands that back, and the log line names the new condition (`card changed 1`) alongside the old three.
- **Mutations owe one re-index, and it is paid after the mode closes.** `dropSdDerivedFiles()` deletes **both** derived
  files - `playlistsd.csv` (the rows) and `indexsd.dat` (one offset per row, plus a footer count) - and sets the bit;
  nothing walks the card while the manager is open. They are one object in two files, and dropping only the index is how
  a valid index came to sit beside a truncated playlist, which SD mode then read as an empty card; the index has to go
  too for the rebuild to trigger, because its absence is the test `initSDPlaylist()` runs first. `FileManager::loop()` -
  which `main.cpp` calls whether or not the mode is open - then does the walk: `SDCHANGE` with the same
  wait-for-the-mode handshake `changeMode()` uses, a forced `config.initSDPlaylist(true)`, a `PLAYLIST` notification,
  back to `PLAYER`. A session of a hundred deletes therefore costs one walk instead of a hundred, the walk is visible on
  the counting screen, and no request handler blocks the AsyncTCP task on it. If the card left the slot in the meantime
  the bit is simply dropped.
- **The walk announces itself, and that announcement is a boolean - the player page never has to guess.**
  `PLAYLISTREADY`
  used to mean only "the list is settled"; it now carries `filemanager.rebuilding()`, so one message covers both ends of
  a build. `FileManager::loop()` sets `_rebuilding = true` and sends the request *before* the first flash write and
  clears it with a second request at the end of the block - so the page blanks the list, spins and locks on the first
  and unlocks and fetches on the second. The client-connect notice hands out the same value from the same flag, which is
  what makes a tab that connects mid-walk behave like the tab that started it. A build that runs while the page is
  open can no longer be watched by playing from the list, because the list is not there to play from; and the request
  at the end is sent even when the build *failed*, because a page must never wait for a build to succeed.
- **The pair is validated together on the way in.** `initSDPlaylist()` already compares the index's footer count with
  `sdman.countAudioFiles()`; it now also looks at the playlist beside it, because a valid index with a truncated or
  empty
  CSV is exactly what a mutation - or a re-index interrupted by a mode close - used to leave behind. A row is written as
  `<name>\t<path>\t0\r\n` (see `SDManager::listSD`), so the shortest possible one, `a.mp3` at the card root, is 16
  bytes,
  and the CSV can never be smaller than 16 bytes per track it claims to hold; when it is smaller, the entry forces a
  re-index. An empty card is deliberately not that case - its count is 0 and a 0-byte playlist is the correct file
  there.
- **Long operations in this module refresh the idle clock, not only the watchdog.** `removeRecursive()` stamps
  `filemanager.touch()` in both of its loops (the name collection and the delete recursion) beside `sdFeedWatchdog()`,
  and [`hDelete()`] stamps both at the top of its selection loop too: a selection is N FATFS deletes in one handler,
  and a *file* never reaches those two loops inside `removeRecursive()` - it returns at the `isDirectory()` false
  branch - so the batch loop is the only place a many-file delete can be fed. One delete request can run for minutes
  and the idle branch would otherwise close the mode mid-delete - the same race the card-gone debounce guards against,
  except this one hands the player back while FATFS is still writing. `touch()` also clears the screensaver counters,
  so the display's countdown and the device's idle logic stay in step. **Rule for new code**: a long card operation
  reachable from a web handler feeds the watchdog *and* stamps the clock. An abort here is not a neutral failure either:
  a watchdog abort lands mid-write, leaves the mount dirty, and the next boot can fail to switch to the card until a
  cold start - observed on a whole-card delete through the manager.
- **A card with nothing playable is a state, not an error.** Three things agree on that now. `Utility::loadStation()`
  returns the result of the entry read instead of an unconditional `true` (it used to report success for a read that
  failed, so `_play()`'s `if (!utility.loadStation(...)) return;` guard never fired and the audio library was handed
  the mount point as a track - `Reading file: "/"` followed by a garbage format name). `_play()` also refuses a
  card-mode play whose url is empty, with `nothing to play: station N has no file`. And
  `Config::initPlaylistMode()` installs the `ehRadio` placeholder when there is no station it can *load* - the test
  is `_lastStation > 0 && utility.loadStation(_lastStation)`, deliberately not a count. `Utility::playlistLength()`
  derives the SD count from the index file's size, and an index holding no entries is still large enough to report
  one, so an empty card resolved to a station that does not exist and kept the previous station's name on the
  display; a failed read leaves the station table untouched by design, so the placeholder has to be applied over it.
  `initSDPlaylist()` applies the same state when a re-index leaves the list empty - the "delete everything, then
  Done" path, which never goes through `initPlaylistMode()`. Both call one helper, `setNoStationState()`: url
  cleared, `name = "ehRadio"`, and `config.setTitle("")`.
  **The three text lines are derived, and none of them repaints on its own**: `_station()` renders the meta line from
  `config.station.name`, while `_title()` renders *both* title lines from `config.station.title`, split on `" - "`.
  That is why a placeholder which set the name and left the title alone let a web stream's title survive a card swap
  and a switch back to an empty card. So anything that rewrites those two fields must ask for the repaint:
  `initPlaylistMode()` sends both at its end (it is the only place the *boot* path passes through, and an unpainted
  meta widget is simply blank), `changeMode()` sends `NEWTITLE` beside its `NEWSTATION`, and the FileManager's
  post-exit re-index sends both *after* it switches back to `PLAYER` - a screen we own drops them, which is the
  reason the requests sit after those page switches rather than beside the state changes.
  The other half of the empty-card picture is that the state must not be overwritten by a stop: `Player::_stop()`
  skips its `L10N_MSG_STOPPED` title when `config.station.url[0] == '\0'`, because an empty url *is* the "nothing to
  play" state, and a switch into SD mode sends `PR_STOP` right before the placeholder runs - so the stop used to
  announce "[stopped]" over the title the placeholder had just cleared.
- Page and API are split on purpose: the page is `data/www/sdmanager.html` and **keeps that address for the whole
  session**. While the mode is open `handleIndex` redirects "/" to it, so a bookmark, a second tab, a captive-portal
  redirect or a Home Assistant link still lands on the manager rather than on a player UI whose buttons are all
  refused - which is the reason the root was hijacked in the first place. A redirect and not the page body at "/":
  the manager then has ONE address, so its own reloads never depend on the device's mode at that instant, and the
  root response stays a constant. `handleNotFound` no longer redirects `/sdmanager.html` anywhere (it is served like
  any other WebUI page) and the page no longer calls `location.replace('/')`.
- **A running operation makes the whole button row inert.** `busy(on, text)` and `busyUpload()` toggle a `busy` class
  on `<body>` alongside the overlay, and the stylesheet dims every `.fb` and `input` under it and sets
  `pointer-events: none` - so New folder, both Upload labels, Delete and Done behave like the controls inside the
  protected folder, which is where that look comes from. `busy()` also calls `armDelete(false)`, because the Delete
  button's "Delete 3?" wording belongs to the press that armed it: without that it stayed armed for the whole request
  and read "Delete 1?" behind the spinner.
  The device has the matching guard, because the page can only speak for itself: `hDelete()` sets `markBusy(true)` for
  the length of the batch (the selection is handled inside one request, so it is the one operation the mode must not
  be closed in the middle of), `hDone()` refuses with 409 `busy` while it is set - same rule and same inertness as the
  upload refusal, and the case it really covers is a *second* tab or device - and `leave()` clears the flag as a
  backstop so a handler that died mid-operation cannot lock the mode closed.
- **The page's status line lives in state, not in the element.** `notice()` writes one slot (`state.notice`) and
  `paintNotice()` renders it; the listing's success path and `render()` repaint it rather than clearing it. What does
  empty it: a new message, leaving the folder, or a press on any button under the list. That last one is a
  capture-phase listener on the button row on purpose - it has to run *before* the button's own handler, or a handler
  reporting synchronously would have its message wiped by the clear. Both the listing's success path and `render()`
  used to clear the slot instead, so a report written just before the follow-up listing was erased within
  milliseconds: the upload's skipped list, every mutation's answer and a delete's failures all "flashed by" that way.
  Deliberately no timer, because a message that expires while it is being read is the bug this exists to fix. The
  protected-folder banner is the one derived message, flagged `sticky`, so a render drops it once the folder is no
  longer locked - and the buttons that clear the slot are dimmed while it applies, so there is nothing stale to clear.
  It is also flagged as an error, because the red border `.pleditorwarning` gets recoloured to is the same thing to
  the reader: this folder refuses every change.
- **An upload batch reports once, at the end.** One slot cannot hold a line per file, and the per-file errors and
  notices were overwriting each other - three failed files showed only the third - so nothing is reported per file and
  `uploadSummary()` composes the lot: `msg_files_upload_failed` with a count, `msg_files_upload_skipped` with a count,
  the last failure's reason appended, and the following listing no longer clears it. Counts, never filenames: the
  names made a message that grew with the batch. The same key serves any count, so there is no singular form to keep
  in step - "1 files failed to upload." is the accepted price. `xhr.onerror` is a transport failure rather than a
  refusal, and it too counts itself into that summary instead of carrying a key of its own; it shows the summary
  *without* the reload `done()` does, because a listing request would fail as well and its error would replace it.
  The overlay for that batch has two shapes: `busy()` is the single body-size line a rename, delete or mkdir gets, and
  `busyUpload()` stacks three under the spinner - "Uploading 4/5" and the percentage at twice the body size, the file
  name between them as a detail - so the progress callback writes only the percentage while the transfer runs.
  `msg_files_upload_failed` and `msg_files_upload_skipped` are new keys: the 50 locale files do not have them yet, so
  other languages fall back to the inline English until they are translated.
- **The player header's SD badge is the UI entry point.** `player.html` wraps that badge in its own
  `div.gb.nb.local[data-command="sdfilemanager"]`, and `script.js` handles the command exactly like `search` - a plain
  navigation to `/sdmanager.html`, nothing else, because the page opens the mode itself and stays on that address.
  Two details are load-bearing. (1) The id (`sdmanbtn`) has to sit on the **wrapper**, not on the glyph: the playermode
  handler shows and hides that id, and a wrapper left in the DOM would be an invisible 54px hot spot over the playlist
  glyph, swallowing the toggle's own click in web mode. (2) `#toggleplaylist.sd-mode { pointer-events: none; }` turns
  the toggle's hit-testing off in the one state where the badge is shown, so
  `#toggleplaylist.sd-mode .gb { pointer-events: auto; }` restores it; the wrapper is a `.gb`, so its hover ring is the
  ordinary one, the same the search glyph gets. No `sdinit` gate is needed - the badge only exists in SD mode, and a
  non-SD build never sets `modesd`.
- **There is no route at bare `/sdman`.** `AsyncURIMatcher::matches()` treats a plain URI as an exact path *or* as a
  prefix followed by "/", so a handler registered at `/sdman` also answered `/sdman/list` and every sibling route;
  the page then parsed that route's redirect as JSON. `/sdman/enter` is the only way in.
- Routes: `/sdman/enter` (GET), `/sdman/done` (POST), `/sdman/info`, `/sdman/list`, `/sdman/mkdir`, `/sdman/rename`,
  `/sdman/move`, `/sdman/delete` (POST, selection in the body), `/sdman/download`, `/sdman/upload` (POST with its own
  chunk handler). Every handler except `enter`/`done` calls `requireActive()`, which answers 409 `not_active` when the
  session ended, or 409 `no_card` when no card is mounted - the page re-enters on the first and leaves the manager on
  the second.
- **The manager's page follows the device over the WebSocket, so nothing polls it.** `SDMANACTIVE` (a new
  `requestType_e`) carries `{"sdmanactive":0|1}` built from `filemanager.active()`: `enter()` broadcasts it once
  `_active` is true and `leave()` once the exit is real, so EVERY close is covered by one line in one place - Done,
  the idle timeout, the card leaving the slot, a close driven from another tab or from the box. A client that
  connects is sent the current value in `onWsEvent` beside `GETBATTERY`, which is what arms a page that opened while
  the manager was already up. `sdmanager.html` opens `ws://<host>/ws` and returns to "/" on the 1 -> 0 transition
  ONLY: the connect-time push reports the state as it is, so a page told only 0 - its socket came up a moment before
  its own `/sdman/enter` - never acts, and a user who opened the manager while the radio was on a web stream is not
  bounced out. `playermode` was deliberately not reused for this: the play mode is not the manager's state, and
  `leave()` even forces the play mode back to `PM_SDCARD`, so the page could not tell the two apart. Polling was
  rejected outright: `/sdman/info` touches the idle clock (that is what `keepAlive()` is for), so a timer on it would
  hold the manager open for ever and defeat `SDMAN_AUTO_EXIT_MS`. The `no_card` request paths stay as the fallback
  for a socket that never comes up.
- Mutations are POST-only and each one calls `invalidateSdIndex()` (removes `INDEX_SD_PATH`), so playlists and the SD
  index are rebuilt rather than trusted after an edit.
- `/` and `/data` (and anything under it) are protected: no create, rename, move, delete or upload there; browsing
  stays allowed and the page dims the controls. Error codes are stable strings the page maps in `errText()`:
  `protected`, `exists`, `no_space` (507), `not_found`, `no_card`, `bad_name`, `no_dir`, `not_dir`, `not_active`,
  and `failed` as the fallback.
- **Upload** is one file per request, multipart, streamed straight to the card with no RAM buffer. The `path` and
  `name` args choose the target and `name` wins over the multipart filename. Replace (no `skip` - the default) opens
  with `FILE_WRITE`, which truncates, so an existing name is overwritten; there is deliberately no `exists()` test on
  that branch. Skip Existing (`skip=1`) leaves an existing name untouched and answers `{"ok":true,"skipped":true}`
  without touching the SD index. Free space is measured *after* the truncating open, because the open is what
  releases the room the replacement needs; a write that would exceed it fails with 507 and the partial file is
  removed so a failed upload cannot leave a truncated track behind.
- **The abandoned open handle is the counter-intuitive half of that.** There is no `UPLOAD_FILE_ABORTED`
  notification to react to, so an upload cut off mid-stream leaves the handle open and the file half-written; the
  next request closes it at `index == 0`, before opening anything new, which is what the missing callback would
  otherwise have had to do. `FILE_WRITE` is also what truncates, so the room an overwrite needs is released by the
  open itself rather than by the close - the reason the free-space figure is taken afterwards. And deleting while
  walking a directory advances the position under the removal and skips entries, so child names are collected
  before anything is removed.
- **A short chunk is never mistaken for a whole file, and a cut-off upload is named.** `onUploadChunk()` now checks
  the result of `_upFile.write(data, len)` and answers 500 `write_failed` when fewer bytes landed than the request
  carried, so a full card is reported instead of a silently truncated track being called uploaded. It also stamps
  `filemanager.touch()` **and** `sdFeedWatchdog()` on every chunk, because this handler runs in the AsyncTCP task and
  that task is subscribed to the watchdog: a slow SPI-SD write was able to reset the device mid-file with no
  application line to say why. The cut-off case keeps its own evidence - when a new file begins while `_upFile` is
  still open, the log names the previous target and the byte count it stopped at before closing it. `hUploadDone()`
  closes a still-open handle the same way and answers 409 `cut_off` rather than reporting success for a multipart
  stream that ended early. On the page side (`sdmanager.html`) a network-level failure on one file is retried once and
  the batch continues, because the old `xhr.onerror` abandoned every remaining file in the selection.
- **A write's RETURN VALUE is how many bytes the stream ACCEPTED, not how many reached the card - so the upload is
  measured after the fact.** On FATFS a write lands in a newlib stream buffer and reports its full length, and the
  flush behind it can fail later, silently. That is the mechanism behind three separate looking symptoms in one
  session: an upload that reported `uploaded (490496 bytes)` for a file that was never on the card, `short write at
  ... (46 of 1436 bytes)` when a buffer boundary happened to flush mid-`fwrite`, and `indexing did not finish
  (250 files, 0 index bytes)` - 250 rows whose four writes each returned their full length, and an index file of zero
  bytes, because `listSD()` counts a file only when all four writes report, and none of them had reached the medium.
  `hUploadDone()` therefore re-opens the target by name, compares `size()` against `_upBytes`, and answers 500
  `write_failed` with the file removed when they differ. **Rule for new code**: never report a write as done on the
  strength of its return value alone; measure the file.
- **OBSERVED ON HARDWARE: mid-write `errno 5` (EIO) - and it follows the UPLOAD PATH, not the card.** The first
  reading of this was wrong, so the evidence is recorded in full. `errno 5` is `EIO`, not `ENOSPC` (28) or `EBADF`
  (9), and beneath it is FATFS's `FR_DISK_ERR`; it arrives in 5-8 ms, so it is a reject, not a slow card.
  - Old card: `short write at ... (0 / 276 / 178 of 1436 bytes, errno 5, after 6-8 ms)`, and one file died after
    **3537920 bytes**.
  - **A brand new Sandisk High Endurance 8 GB failed the same way** - `96` and `361` of 1436 bytes - while one file
    in the same batch uploaded **8673949 bytes with `0 chunk(s) retried`** and passed the landed-size check. A card
    that can take 8.6 MB cleanly is not a failing card, and two very different cards cannot share a card fault.
  - Mount, listing and `usedBytes` are perfect on both.
  - **Every new-card failure is PARTIAL** (`0 < wrote < len`): the transport stops mid-sector. That is the shape of
    interference during a write, not of a medium error.
- **SUSPECT FOUND (and fixed): the presence probe was hammering the SD bus during uploads.** `FileManager::loop()`
  called `sdman.cardPresent()` on every main-loop iteration - about 60 times a second - and the SPI implementation
  of that probe issues a **raw `readRAW()` sector read** through `diskio_impl`, bypassing FATFS. So the main loop
  was sending card commands continuously while AsyncTCP wrote a file to the same card, and through every delete.
  Nothing was left of the three-strike debounce either: at 60 Hz the three strikes were spent inside 50 ms.
  The probe is now skipped entirely while an upload is open or a delete batch is running (a transfer in progress is
  itself the proof that the card is present) and is otherwise limited to **once a second** by `_lastCardCheckMs`.
  **Rule for new code**: the SD bus is shared, so anything that touches the card from outside the operation that
  owns it must be gated on that operation and rate-limited, however cheap the call looks.
- **The card probe is still the fallback suspect if `errno 5` returns.** If mid-write EIO survives this change on a
  known-good card, the next levers are the SPI clock (`SDSPISPEED`) and the transport itself - SDMMC on the S3 -
  not more code in this module.
- **A transfer is verified every `SDMAN_UPLOAD_VERIFY_BYTES` (64 KB), not only at the end.** `flush()` then `size()` is
  the only pair that tells the truth about the medium; checking once at close let a card that accepts into the stream
  buffer and discards it surface megabytes later (**4815872 bytes** into one file). Early verification turns that into a
  failure within a cluster.
- **Rescues are budgeted by SIZE, and a BURST of refusals is treated as a different failure entirely.** A flat count
  conflated two things the field showed in the same week, and it cost a real upload: a 10 MB file reached
  **10029827 bytes - 24225 short of its 10054052 - and the ninth isolated refusal met a flat cap of 8**, so a file
  that was 99.76% written was thrown away by a constant rather than by the card. Meanwhile a card that was genuinely
  failing refused 17 chunks in a row. So there are two rules now:
  - `SDMAN_UPLOAD_RETRY_BURST` (4) consecutive refusals with no successful chunk between them fails the file at
    once - a card that is not coming back, which no budget can help;
  - otherwise the budget grows with the transfer: `SDMAN_UPLOAD_RETRY_FLOOR` (8) plus one per
    `SDMAN_UPLOAD_RETRY_PER_BYTES` (**64 KB**) written - deliberately generous, so this is a sanity ceiling and never
    the thing that ends a transfer. The divisor was 512 KB until the 14-file batch showed Stars wanting 11 rescues in
    2.08 MB (one per 190 KB) where the 10 MB file wanted one per 1.25 MB: no divisor is right for both, and a
    transfer that is landing, however grudgingly, should be allowed to land.
  Both kinds of refusal are named in the log line (`burst - the card is not recovering` / `rescue budget reached`),
  because they mean opposite things. Only the FIRST rescue is logged per file and the rest are counted: a card that
  is merely flaky rescues many chunks, and a line each flooded the log badly enough that logging had to be switched
  off to read anything.
- **A retry waits `SDMAN_UPLOAD_RETRY_DELAY_MS` (2 ms) first.** The refusal being answered is
  `0 of 1436 bytes, errno 5, after 0ms` - zero bytes accepted and no bus time at all, which is the card declining
  the next write while it is still programming the previous block. That is a timing failure and time is the remedy;
  retrying instantly only asks the same question again. It also makes `SDSPISPEED` a *mechanism* rather than a guess:
  a slower clock is the same remedy by another route, so it is the next thing to try if the refusal rate is
  unchanged, and SDMMC (which bypasses sdspi altogether) after that.
- **The upload stall window depends on whether the transfer can still succeed.** `SDMAN_UPLOAD_STALL_MS` (120 s) applies
  while it can; `SDMAN_UPLOAD_STALL_FAILED_MS` (1 s) applies once a chunk has failed, because the file is lost and
  nothing is being waited for, so the manager - and the whole WebUI - must not stay locked for the healthy window.
- **Both failure shapes are rescued, and the PARTIAL one is the interesting case.** `wrote == 0` means nothing was
  accepted, so the stream has not moved and re-issuing the same bytes cannot duplicate or skip any - and with the
  field evidence above, discarding a finished multi-megabyte file over one 8 ms reject is the wrong trade.
  A short write of `0 < wrote < len` used to be fatal on the grounds that "how many bytes reached the medium is
  unknowable". That was wrong: after `flush()`, `size()` IS the medium, and when it has not fallen behind `_upBytes`
  the missing bytes are the TAIL of the current chunk, still in `data`. So the stream is `seek()`ed to the card's own
  figure and that tail is re-sent. That is safer than failing the file, not merely kinder - the position comes from
  the card rather than from our bookkeeping, so the two cannot drift apart, and nothing is re-sent that might
  duplicate or skip a byte. Only when the shortfall reaches back past `_upBytes` are those bytes gone from our side
  too, and only then is the file failed. Both counters (`_upRetries` for refused chunks, `_upPartialRescues` for
  repaired ones) are named in the per-request line, so a file that landed because of rescues says so.
- **The 14-file batch that settled the rest: 9 landed, and the 5 failures split 3 partial / 2 budget.** Worth
  keeping because it is counter-intuitive - **every file that landed needed ZERO rescues**, not one of the nine,
  while the failures were the files that hit refusals over and over (Stars spent 11 rescues in 2.08 MB, one per
  190 KB, against the 10 MB file's one per 1.25 MB). The condition is per-transfer, not a steady rate: a file either
  flows or it fights. That is why no per-size budget can be tuned right, why the divisor is now generous enough to
  never be the limiter, and why the burst rule is the only correct give-up test.
- **A "rescued" write is not a rescued write: the medium is the only truth.** The follow-up batch showed the rescue
  counter lying. Infinity logged **26 rescues**, Stars **53**, and both failed - and `_upPartialRescues` stayed **0**,
  which is what gave it away: the partial-write recovery needs `onCard >= _upBytes`, so a zero meant the card was
  BEHIND our count, not level with it. The retry line reads `the retry wrote it (1436 bytes in 0ms)`, and 1436 bytes
  of SPI in 0 ms is not a card write - it went into the stream buffer. So `_upBytes` counts bytes the stream
  *accepted*, the buffer hides the loss, and the count drifts ahead of the medium until a flush fails and the
  shortfall reaches back past bytes we no longer hold. **Rule**: never treat a write's return value as evidence that
  data landed; `flush()` then `size()` is the only instrument here, and it is worth running often.
- **When the card falls behind, the transfer is over - fail it at once and let the browser send the file again.**
  The periodic `size()` check now runs every `SDMAN_UPLOAD_VERIFY_BYTES` (64 KB, was 256 KB) and a shortfall fails
  the file immediately with its own reason, **`card_lagging`**, logging how far behind the card was. Grinding 53
  rescues into a transfer that has already lost bytes helps nobody: it costs the card's time and it buries the
  cause. The reason is distinct because of what the PAGE does with it: `card_lagging` (with `write_failed` and
  `cut_off`) is retryable, and the page retries the file - resuming from the offset the device vouched for when it has
  one, else from the start - under its attempt ceiling (`MAX_ATTEMPTS` floor, grown by `attemptCeiling()`, with
  `MAX_STALLED_TRIES` as the size-independent no-progress guard), naming the attempt on screen. That is the only
  recovery that can work here - **the device cannot
  re-supply bytes it no longer holds, but the browser still has them**, and the failures are per-transfer (nine
  files landed with zero rescues while three needed 26, 53 and 26), so a fresh attempt has a real chance where
  patching bytes does not.
- **Both ends of the SPI clock have now been tested and it is not the variable.** The last two batches ran at
  10 MHz and behaved exactly like the earlier ones at 20 MHz: the same per-transfer refusals, the same one-per-47-KB
  rate on a bad transfer. It is back at `SDSPISPEED 20000000` for speed. Do not spend another round on the
  frequency; the remaining levers are the transport itself (SDMMC, which takes `sdspi` out of the path) and, if the
  rate is ever worth attacking again, coalescing reads/writes - **and note the C library already buffers stream
  writes into multi-chunk flushes** (the partial returns of 46-404 bytes are the proof), so a hand-rolled 16 KB
  buffer would buy a few times fewer card writes, not the eleven times it looks like, and it would mean rewriting
  the most delicate function in `filemanager.cpp`.
  **SUPERSEDED for the manager session - see "THE CLOCK *IS* THE VARIABLE, AND THE DISPLAY BOARD IS THE REASON".**
  The two batches above were both run on a unit whose display does not compete for the SPI bus. The conclusion still
  holds for throughput ("the clock buys no speed") and still holds as "do not tune `SDSPISPEED` globally"; what it
  missed is the ERROR RATE, which is a different variable and which does depend on the clock when the display shares
  the bus. Session entry now lowers the clock to `SDSPISPEED_MANAGER` and exits restore `SDSPISPEED`.
- **RESULT: 14 of 14 uploaded. Ten landed first time and four failed and landed on the second attempt** - the
  whole-file retry is what closed this out, and the four failures are the ones that used to be lost files. Infinity,
  the 10 MB file that was 24225 bytes short two rounds earlier, went up **complete at 10054052 bytes** on its second
  attempt; Hot Like Fire was caught by `card_lagging` (the card fell 49511 bytes behind) instead of being ground
  down, and landed on the retry. Every final upload reports `0 refused chunk(s) rescued, 0 partial(s) recovered` -
  the first attempts are the only refusals, and they no longer cost a file.
- **What a retry COSTS, measured: it restarts the file from byte 0, and that is about 18% overhead on a batch.**
  `abortUpload()` removes the partial on purpose, so the browser re-sends everything. In that batch: 96 MB of files,
  17.7 MB of discarded partials and four full re-sends, so roughly 114 MB moved for 96 MB of audio. Cheap in
  aggregate; **expensive for the file it happens to** - Night Time reached **97.9%** (6817792 of 6965338 bytes,
  discarded) and then cost about twice its size, and Infinity threw away 3.6 MB of a 10 MB file. Worth knowing
  before reaching for a bigger batch or a slower link.
- **Resumable retry is the fix for that cost, and it is deliberately NOT built yet.** Three levels, recorded so the
  choice is explicit rather than accidental:
  1. **Resume on retry (the one to build if it is ever needed).** The verification already knows a *trusted*
     position - the last 64 KB checkpoint - so truncate the partial to it instead of deleting it, return `landed: N`
     with the failure, and have the page re-send only `f.slice(N)` with an `offset=N`; the device appends and refuses
     an offset that does not match its verified size. Night Time would have sent its last 147546 bytes rather than
     6.97 MB. No continuous chunking, so the happy path stays one fast stream.
  2. **General browser-driven chunking** - many small verified pieces, a failed piece retried alone. More robust,
     more machinery, and it slows the path that currently works.
  3. **Do nothing.** ~18% overhead and the batch completes.
  The ordering matters: run the **40 MHz** test first. If a faster clock reduces the refusal count then resume is
  complexity for nothing, and if refusals rise, this 18% figure is what says whether Level 1 earns its keep.
- **A retry notice must be CLEARED when the file lands.** The message slot holds its text until another message
  replaces it, and the batch summary only speaks up when something FAILED - so `sending it again: <file>` sat on
  screen describing a file that was already on the card, which made a clean 14-of-14 batch look like it had gone
  wrong. The notice is now informational rather than error-styled, and `finish()` clears it when a retried file
  lands. **Rule**: any message that describes a state must be withdrawn when that state ends, or it becomes a lie.
- **BUG FOUND AND FIXED: nothing created `/data` ON THE SD CARD, so a card without it could never build an index.**
  `startup.cpp`'s `LittleFS.mkdir("/data")` is for the FLASH filesystem - a different filesystem with the same
  folder name - and FATFS cannot create a file inside a directory that does not exist. `indexSDPlaylist()` opened
  `/data/plsd.csv.tmp`, failed, and **returned without a word**, so the index was never built and SD mode reported
  `no playable station (playlist length 0)` for ever. Uploads kept working throughout, because they only need the
  folder they write into, which is exactly what hid it. `indexSDPlaylist()` now creates the folder, says so when it
  does, and logs plainly if it cannot - the folder name is one `static const char SD_DATA_DIR[]` so the planned
  rename to `/ehradio.data` has a single place to change.
- **A diagnostic must never read through a handle it has already decided is invalid.** The same line printed
  `playlist open 0 with 1495441367 bytes` - a 1.5 GB "size" for a handle the `?:` had just called invalid, on a
  7.9 GB card - and it sent a whole round of diagnosis in the wrong direction. The line now names WHICH condition
  failed (`the index file could not be created` / `the walk stopped at a refused write` / `the row count does not
  match the file count`) and reads sizes only from handles known to be open.
- **Per-file transfer timing is logged on success and on failure.** `uploaded <file> (N bytes on the card in Mms,
  K KB/s)`, and the refusal line carries its own time and byte count. The point is comparison - 20000000 against
  40000000, or SPI against SDMMC - decided on throughput rather than on a feeling, and **the failure cost is
  included because that is where the minutes actually go**. A clock can be good for reading and bad for writing, so
  this is read together with the listing's own `walked N entries in M ms`.
- **THE SPI CLOCK DOES NOT AFFECT THROUGHPUT - measured, three ways.** A 14-file, ~96 MB batch at **20000000,
  30000000 and 40000000** all landed at about **135 KB/s** (131-161, 113-139, 123-190), so 20 MHz is if anything the
  best of the three and doubling the clock buys nothing. **Do not tune `SDSPISPEED` again**: it has no advantage to
  trade against an error rate. 135 KB/s works out at roughly **4 ms per 512-byte sector**, far slower than 20 MHz SPI
  could write a sector, which points at the card's own programming latency (or the network path) rather than the bus
  - so the ~20 minutes a batch takes was never winnable through the clock either.  (Throughput only: the ERROR rate is
  the clock's business after all - see "THE CLOCK *IS* THE VARIABLE, AND THE DISPLAY BOARD IS THE REASON".)
- **THE CLOCK *IS* THE VARIABLE, AND THE DISPLAY BOARD IS THE REASON - and it is a per-SESSION clock, not a global
  one.** The 2x2 that found this used two boards and two cards, same firmware:
    - `sh1106_vs1053_3buttons` + the good card: excellent, every file first attempt.
    - `sh1106_vs1053_3buttons` + the budget card: the usual refusals, rescue, ~4 KB behind, resume.
    - `ili9488_pcm_1button_full` + the good card: about FIFTY failed transfers on a batch the sh1106 unit landed clean.
    - `ili9488_pcm_1button_full` + the budget card: the worst of the four.
  Both boards mount the SD card on `SD_SPI 'B'` (checked in `myoptions.h`), so the BUS is identical; the only
  difference is the DISPLAY - a 128x64 I2C OLED against a 480x320 SPI panel. The old theory ("the 1 s release window
  caused it") was wrong and was withdrawn: the same firmware is clean on one unit and dirty on the other, which points
  at TIMING MARGIN - the panel's SPI traffic takes margin away from `sdspi`, and a budget card has the least margin to
  lose. Lowering the clock gives it back.
  - **T1, the good card on the ili9488 unit: `SDSPISPEED 10000000` fixed it COMPLETELY** - 10 of 10 files first
    attempt, zero refusals, and **170-238 KB/s, i.e. exactly as fast as the best 20 MHz runs** (226 KB/s measured at
    10 MHz). That is the second time the card has been shown to be the throughput limit at ANY clock, so nothing is
    traded away for the reliability. Below 2.5 MHz the BUS would finally become the limit and uploads really would
    slow down.
  - **The budget card still fails at 10 MHz** (refusal, rescue, ~4 KB behind, resume, at 15-76 KB/s), so halving
    helps but does not finish the job - which is why the step-down below exists rather than a single fixed number.
  - **The mechanism is a REMOUNT, because the frequency is a MOUNT PARAMETER.** `SDManager::start(freq)` now takes the
    clock, `ensureSpeed(freq)` remounts only when the card is not already at that clock (`SD.end()` + `begin(cs, spi,
    freq)`), and `mountedFreq()` reports it. **A remount discards every open handle**, so it may ONLY happen at a mode
    boundary or from the main loop - never inside a request handler. That is why `enter()` sets it and `loop()` steps
    it, and nothing else touches it.
  - **`SDSPISPEED_MANAGER` (10000000) is the session clock; `SDSPISPEED` is the player's.** `enter()` applies the
    manager clock; `leave()` puts `SDSPISPEED` back before the card is handed to playback, so the player's reads keep
    the high clock. **The manager runs at that ONE clock for the whole session** - browsing, listing, deletes and
    uploads - because a speed change means a REMOUNT (the frequency is a mount parameter) and a remount discards every
    open handle. Listing is therefore slower than it would be at `SDSPISPEED`; that is accepted: safety over speed.
  - **THE AUTOMATIC STEP-DOWN WAS BUILT, TESTED AND REMOVED - do not rebuild it.** It halved the session clock on
    every transfer that needed resuming, down to a 156250 Hz floor, on the theory that the refusals were a
    time-between-writes problem a slower bus would cure. The field test killed it outright: a budget card froze at the
    SAME byte offset with the SAME ~5 KB deficit at 10, 5, 2.5, 1.25, 0.625, 0.3125 AND 0.15625 MHz - a **64x range
    with no change in behaviour** - while a 10 MB file went through untouched at 10 MHz in the same session. The
    clock is not the variable, and every step it took cost a remount plus a permanently slower session. The macros
    `SDMAN_SPEED_FLOOR` and `SDMAN_SPEED_STRIKES` went with it. Its three defects are still worth remembering, because
    they were real faults and two of the lessons stayed:
    1. Its strike was raised in only ONE of the two paths that can end an attempt. It lived in `hUploadDone()`, but
       most failures are released by the STALL branch (`abortUpload()`), after which the page aborts and
       `hUploadDone()` never runs - so a session whose files needed five and seven resumes never moved the clock.
    2. Applying it from `loop()` RACED the request handler. `ensureSpeed()` is a `stop()`/`start()` pair and the
       page's next request arrives on the AsyncTCP task, so seen mid-remount it read `ready == false` and answered
       `no_card` at `0 bytes in 0.00s`, abandoning a 5.8 MB partial. **A mid-session remount is not safe here** -
       which is exactly why the manager now has only one, at the mode boundary.
    3. A failed remount left the card UNMOUNTED, and then every path answered `no_card`. `ensureSpeed()` now falls
       back to the clock that DID work and keeps the card mounted, logging the fallback. That guard is KEPT.
  - **A STALLING CARD COULD NOT CONVERGE, AND THAT IS WHAT ACTUALLY LOST FILES.** On `card_lagging` the retry resumed
    from `_upVerified` - the last CHECKPOINT that passed - which can be far behind what the card holds: the field
    resumed a 6.9 MB file at **766 bytes** when the card held **29696**, so every attempt re-sent the same ~29 KB into
    the same wall, never advanced, spent the retry budget and was discarded (`Discarded the abandoned partial ...`).
    The card's own `size()` after `flush()` is a LOWER bound on what is committed - the same instrument the verify
    already trusts - so `_upVerified` is now set to that figure (floored to a sector by `resumeFloor()`), and the
    retry continues from where the card truly stopped.
  - **The manager ALWAYS hands back to SD mode.** `leave()` used to pick its resume target from `config.getMode()`,
    i.e. whatever the device was last in, so a session opened while a web stream was playing resumed the WEB stream
    on exit. It now forces `play_mode = PM_SDCARD` + `syncSDFS()` (when the card is mounted) BEFORE the resume
    decision, and the web branch is gone: this mode exists to edit the CARD, so it returns to the card.
  - **The answer was the card's ALLOCATION UNIT SIZE, which supersedes every theory in this section.** A cheap 16 GB
    card that had failed here for months, at every clock, was **reformatted with 512-byte allocation units** and then
    took everything: 14 files, 1 attempt each, 0 refusals, 0 partials, 186-198 KB/s - and then a deliberate fragmentation
    stress run (upload, delete, re-upload, a 12 MB `.exe` and a 12 MB `.zip`) with the same result. The unit that broke
    it - **8 KB, sixteen sectors** - is what every formatter, including the "SD card Formatter" utility, picks by default
    for a card that size.
    **It is a weak-card malady, not a law about large units.** The unit does not break a card by itself; it decides
    whether a MARGINAL card can cope. On the same cheap card: **16 KB** landed every file but slowly (18, 11 and 13
    attempts, 73-86 KB/s; without resume-on-retry those are three lost files), **8 KB** lost files outright, **512 B** was
    clean at 90-95 KB/s. A branded Sandisk at **16 KB** was flawless - every file, 1 attempt, up to 199 KB/s even
    fragmented. The ladder is a property of the CARD as much as of the format, so a large unit costs a marginal card
    SPEED or files and a good card nothing.
    The mechanism fits every symptom: with an 8 KB unit a stream of ~1436-byte chunks makes FATFS rewrite the same
    cluster and the same FAT and directory sectors over and over, and this card refuses those read-modify-write patterns
    (`errno 5`, the size a few KB short and never catching up, eventually a byte offset that cannot be passed). It also
    explains the shape we kept noticing - **the first file after a format always lands and the ones after it do not**,
    because only the first write goes into clean, contiguous, never-programmed space.
    **So before blaming a card, a reader, a clock or this firmware, CHECK THE ALLOCATION UNIT.** The manager prints it in
    its `Open (...)` line (`Allocation unit size: 512 bytes`); the SDMMC transport tolerated the large unit on this same
    card (every file, 1 attempt, 0 refusals at 109-196 KB/s).
  - **What the refusal actually is.** The card accepts 0-307 of 1436 bytes and refuses in 0-13 ms with no bus time at
    all, i.e. it is declining the NEXT command while it is still programming the previous block.  That looked exactly
    like a time-between-writes problem, which is why a slower bus was tried - and the **64x clock test REFUTED that
    cure**.  The real cause turned out to be the allocation unit above, so the SHAPE was right and the remedy was
    wrong: whatever the card is waiting for, it is not more time between commands, and it is not the display either
    (its own bus, and the same board with a good card is flawless).
  - **The refuted 512-byte write-alignment experiment aligned to the wrong unit.** `SDMAN_WRITE_SECTOR` (since removed)
    rounded every write up to a whole SECTOR - while the test card's allocation unit was **8 KB, sixteen sectors**, so
    it could not change how FATFS programs a cluster; the "no benefit" reading was inevitable and void. **The unit that
    matters is the CLUSTER**, not the sector.
  - **The allocation unit is REPORTED, never acted on.** `SDManager::allocationUnit()` reads `fs->csize` (in SECTORS,
    so bytes = `csize * ssize`) from the volume `f_getfree()` hands back, once per mount; `0` is reported as `unknown`
    rather than guessed. The manager prints it in its `Open (...)` line on BOTH transports - which is why `ff.h` is
    included unconditionally rather than only on the SPI path (the MMC path does not pull in `diskio_impl.h`, which is
    what carries it there) - and `/sdman/info` carries the same figure.
  - A large allocation unit costs SPEED, not correctness: a mis-formatted card still uploads (measured 164-181 KB/s at
    2 KB, ~90 KB/s at 512 B, 176-199 KB/s at 64 KB), so nothing warns about it or blocks over it.
  - **WHY THE FIRST FILE ALWAYS LANDS AND THE ONES AFTER IT DO NOT.** After a format the first file goes into a clean,
    contiguous, never-written region: almost no FAT work, and its directory entry is written once, at close.  Every
    file after it makes the filesystem REWRITE sectors it has already programmed - the same FAT sector and the same
    directory sector, read-modify-write, over and over.  **A card that fails on rewrites of already-programmed sectors
    therefore passes the first file perfectly and fails everything after it**, which is exactly the observed shape,
    and it is why a format "fixes" it for precisely one file.  (This was written as a hypothesis and is now the
    mechanism confirmed by the allocation-unit finding above: the rewrite granularity IS the cluster size.)
  - **A HOPELESS FILE NOW ENDS THE BATCH, deliberately.** The page allows a few no-progress retries
    (`MAX_STALLED_TRIES` is 3) and then, when the reason blames the CARD (`CARD_FAULT`), abandons the whole batch with a
    message that the
    card looks faulty. A card that cannot finish one file will not finish the next thirteen, and the field runs showed
    precisely that - file after file, each burning its budget and discarding a partial, with nothing gained. A refusal
    the page caused itself (a bad name, a protected path) is NOT in `CARD_FAULT`: those are skipped and the batch
    carries on.
  - **THE SD CARD'S data FOLDER IS `/ehradio.data`, and it is NOT the LittleFS `/data`.** Two different filesystems
    used the same folder name, which made them impossible to tell apart in a log, a listing or a bug report.
    `SD_DATA_DIR` in `config.h` is now the ONE place the SD name is written, and `PLAYLIST_SD_PATH`, `INDEX_SD_PATH`
    and their `.tmp` twins are derived from it by string concatenation. **The five LittleFS paths beside them
    (`PLAYLIST_PATH`, `SSIDS_PATH`, `VERSION_PATH`, `TMP_PATH`, `INDEX_PATH`) did NOT move**: they are the web
    endpoints the browser fetches, and changing them would break every one of those URLs. An existing card keeps its
    old `/data` until the new folder is created and the index rebuilt - **nothing migrates it and nothing deletes it,
    and `/data` is NOT protected**: it is ordinary clutter the user may clear from the UI, and the log line that
    announced the new folder used to add a parenthetical about the old one, which only invited the question of what
    happens to it. The PAGE has its own copy of the name (`sdmanager.html`'s lock test) and it does not move with the
    macro.
  - **The per-card SPI upload marker is gone**, removed with the premise it rested on: the cluster assembly disproved
    that a large-unit card cannot be uploaded to. Taken out: `SDMAN_SPI_MARKER`, `spiUploadBlocked()`, the `spi_blocked`
    refusal, the `hSpiMarker` route, `spi`/`spimarker` in `/sdman/info`, and on the page the marker notice and its
    `state.spiBlocked` dimming. A batch abandoned for a card-side reason now reports the plain write failure only.
  - **NEVER REMOUNT THE CARD WHILE ANYTHING CAN STILL HOLD A HANDLE FROM IT - and stopping the player is part of
    that.** `FileManager::enter()` QUEUED a `PR_STOP` and remounted the card one line later, because the session clock
    is `SDSPISPEED_MANAGER` and the player's is `SDSPISPEED`. A queued stop only ASKS the player task to stop, so the
    audio's file handle outlived the filesystem it came from and closing it afterwards produced
    `CORRUPT HEAP: Bad head at 0x... / assert failed: multi_heap_free ... (head != NULL)` and a reboot. It happened
    EVERY time the manager was opened while PLAYING FROM THE CARD, and never from a web stream - a web stream holds no
    SD handle, which is the tell. The fix is one call: `player.stopSync()`, which is `_stop()` -> `stopSong()` and
    closes the audio file there and then. **This is the pattern the codebase already used**: `config.cpp`'s
    `changeMode()` unmounts the card with `player.stopSync()` and THEN `sdman.stop()`, with a comment saying exactly
    why. `_wasPlaying` must be read BEFORE the stop, because the stop clears it.
    The same rule applies on the way OUT: `leave()` now closes `_listDir` before it remounts back to `SDSPISPEED`,
    because a listing keeps its directory open across the whole walk and a listing the user abandoned can still be
    open when the mode closes. **Rule: a remount is a mode boundary, and every handle the boundary's OWN side owns
    must be closed before it - the ones it does not own (the player's) must be closed by the owner, synchronously.**
  - **A REFUSED MODE SWITCH NOW ENDS THE WAIT.** Switching to SD mode with **no card** used to leave the page locked
    on its spinner for the whole 3-minute backstop: `config.changeMode()` logs `SD card not found`, sends a
    `GETPLAYERMODE` report and returns WITHOUT changing the mode, and the page only acted on a mode report it could
    see had CHANGED. It now also acts on the report where it did NOT change, but only while a switch is outstanding
    (`_switching`): that combination IS the refusal. The FIRST report after a page load is the one that must not be
    mistaken for it, which is what `_modeKnown` is for. The wait ends, the lock is dropped and the list is refetched.
    It used to raise an alert ("No SD card is mounted.") for the switch the page had ASKED for (`_switchWanted`), and
    that pop-up is gone: the device has already shown why it refused, and the SD File Manager now refuses to open at
    all without a card (`no_card` -> the page leaves), so the alert only repeated what the display and the manager page
    both say. `_switchWanted` went with it; a no-op switch (asking for the mode we are already in) is handled the same
    way as a refusal.
  - **THREE SMALL UI FIXES, and the rule they follow.** (1) A plain `input[type=checkbox]` - the only control the
    browser still drew for itself, used by the SD manager's Select All and the editor's per-row flags - is themed with
    one `accent-color` rule, which colours the box AND the tick and follows the theme because it reads the same custom
    property everything else does. (2) A list NOTICE is ONE ROW: `.sdnotice` carried 18px of padding and `.plloader`
    40px around a 40px spinner, so an empty-folder message or the listing spinner measured about three rows and pushed
    the row after it out of step. (3) The listing spinner is now the page's own spinner scaled to a list row rather
    than a size of its own, so there is one spinner treatment in the UI, not two.  SCALING IT DOWN REQUIRES DECLARING
    ITS PADDING, and that was a follow-up field bug: the manager's loader is spans INSIDE a `.pleitem`, so it inherits
    `.pleitem span`'s `padding: 0 8px` and `--odd-bg-color` box, and with the universal `box-sizing: border-box` an
    18px box cannot hold 16px of padding plus a 6px ring - the browser widens the border-box to about 22px while the
    height stays 18px, so the ring renders as an ELLIPSE (round at the player's 40px, oval here, from identical rules).
    The manager's markup is the difference: player.html's loader is a DIV outside any `.pleitem` (`script.js`), the
    manager's is spans in an `li.pleitem`.  The notice row is now `background: transparent` with no wrapper border, and
    the ring declares its own padding and full border shorthand.  **The general rule: anything inside a `.pleitem`
    inherits a 40px-wide, 8px-padded, 1px-bordered span box - override every one of those or the element will not be
    the size that was asked for.**
  - **THE WEB FILES ARE NOT IN THE FIRMWARE BINARY.** `board_build.filesystem = littlefs` and the gzip scripts run
    ONLY for filesystem targets, so `pio run -e <env>` builds the FIRMWARE and nothing else - an edit to
    `data/www/*.html`, `*.js` or `*.css` is invisible until `-t buildfs` (and `uploadfs` to flash it). The two are a
    pair and are flashed separately. A build that reports no compilation and an unchanged flash size after a web edit
    is telling you the web edit is not in it.
  - **A retry cadence note, measured, so it is not re-derived:** the log's `No chunks for ~1005ms - releasing` is
    `SDMAN_UPLOAD_STALL_FAILED_MS` (1000 ms), but it is NOT what sets the time between attempts. The page's upload
    watcher runs every **2000 ms** and `waitForRelease()` polls `/sdman/info?up=1` every **1000 ms**, so the PAGE
    gates the cycle; shortening the device's window to 250 ms would buy nothing (it was tried on paper and dropped).
    If the retry cycle ever needs to be faster, the levers are the watcher's 2000 ms interval and `waitForRelease`'s
    1000 ms - and both add cardless `/sdman/info?up=1` requests to the SAME AsyncTCP task that writes the chunks, so
    that is a trade to measure, not to assume.
- **EVERY RETRY HAS SUCCEEDED, in every run (4/4, 3/3, 1/1, 5/5, 3/3).** Combined with the refusals arriving in
  0-1 ms with almost nothing accepted, the shape is a card declining a command while it is busy and being perfectly
  happy a moment later. **Zero FAILED FILES is therefore achievable; zero FAILURES is not something we can promise
  without the mechanism**, and the clock cannot deliver it. Absorbing the failures is the lever that removes them
  from the outcome.
- **RESUME ON RETRY - the protocol, and why it is small.** The device keeps a partial file when the CARD ended the
  transfer (`write_failed`, `card_lagging`, `stalled`) and answers with **`landed: N`**, where N is the last position
  the 64 KB verification PROVED is on the card - never the raw accepted byte count, because that is the number a
  buffered write lies with. The page remembers `landed` per file and sends **`f.slice(landed)`** with
  **`offset=landed`** on the next attempt. The device accepts the offset only when it equals the value it advertised
  for the same path, opens with **`r+` (no truncate)**, seeks there and continues, letting the incoming remainder
  overwrite the stale tail. A mismatch is refused and the file restarts - slower, never wrong.
  - That is the whole protocol: **one request parameter and one response field**, because `Blob.slice()` happens in
    the browser. The file still goes up as ONE request - no piece boundaries, no ordering, no reassembly. (The
    general chunked-upload design that would need all three was considered and rejected as a far bigger protocol
    for no extra benefit.)
  - What it saves: a file that fails at 97.9% costs its last **147546 bytes** instead of re-sending 6.97 MB, and a
    10 MB file no longer throws away 3.6 MB. Before this, one batch moved 114 MB for 96 MB of files and the worst
    single file cost about twice its size.
  - **A partial is kept only when a verified position exists.** Anything else - no space, a protected name, the mode
    closing - removes it, because a truncated track under a name the page has listed is worse than no track. `leave()`
    clears the record too: nothing may resume into a mode that has ended.
- **RESUME WORKS, and the first run of it landed all 14.** Resumed attempts took 5830 ms, 9478 ms, 9715 ms and
  13931 ms against 35-50 s for a full re-send - that is where the minutes come back.
- **BUG IT EXPOSED: `_upBytes` starts at the resume offset, so it is the FILE POSITION, not the bytes moved.** The
  timing line divided the whole file size by the time the tail took and reported **1086 KB/s** for a transfer that
  actually ran at 172. Any figure used for comparison has to measure the same thing on both sides, so the line now
  reports `_upBytes - _upResumedFrom` and says `bytes moved`. It also prints **seconds** (`33.25s`) - every figure
  here is read by eye, and `33253ms` takes work to read.
- **BUG IT EXPOSED: a kept partial was LEFT ON THE CARD when the page gave up.** The device keeps a partial so the
  page can resume, but nothing removed it if the page never came back - and a field run left a **9.25 MB truncated
  file** on the card after the page gave up on the file, which the listing then counted as a track. That is
  precisely what the no-truncated-track rule exists to prevent. `discardResume()` now forgets the record **and
  deletes the file**, and it is called when a different file starts and from `leave()`. **Rule**: a file kept for a
  resume is a loan, not a gift - it must be reclaimed on every path where the page will not come back for it.
- **KEEPING A FILE FOR A RESUME CHANGES WHAT `exists()` MEANS FOR THAT NAME, so every check that treats "already
  there" as a reason to refuse has to know about it.** `Skip Existing` did not, and it cost **five of fourteen files
  in one batch**: the partial kept for the resume IS the file that "already exists", so each retry was answered with
  `upload skipped, name already present` and then `discardResume()` deleted the partial - the file lost twice over,
  to a rule that was working exactly as written. A resume the page offers now **takes precedence** over Skip
  Existing on both sides: `canResume` is computed before the refusal chain in `onUploadChunk` and gates the skip
  branch, and the page's own pre-check is bypassed for a file it holds a `landed` offset for. **Rule for new code**:
  when this module keeps a file on purpose, list every other test that asks whether the name is taken and decide what
  each one should do about it.
- **The countdown is drawn only below `SDMAN_COUNTDOWN_FROM_MS` (2 minutes), and `idleRemainingMs()` is ONE clock
  again - the user's.** It had been answering with the upload's own deadline while a transfer was open, which put a
  number on the display nobody asked for, and because every chunk restamps the clock that number sat still: first a
  frozen 2:59, then a frozen 1:59. The deadline is still ENFORCED by the stall branch in `loop()`; whether it is
  worth SHOWING is decided where the showing happens. During an upload or a delete the clock stays near the top of
  its range and the line is **blank**, so what remains reads as what it is: a two-minute no-activity timer.
- **CORRECTION - the SPI sweep measured WRITES only.** 20, 30 and 40 MHz all write at ~135 KB/s, but the clock does
  affect READS, and high-bitrate FLAC streaming needs that headroom. So do not lock in 20000000: **resume is what
  makes a higher write-error rate affordable**, which is the trade to make - keep 40 MHz if playback needs the read
  speed and let the resume path absorb the write failures.
- **The stall branch prints the quiet time it TESTED, and `abortUpload()` never overwrites a reason that is already
  set.** Both were field bugs. Reading `uploadIdleMs()` again inside the `FUNCTIONLOG` let a chunk arriving from the
  AsyncTCP task between the test and the print turn 30 s of silence into `upload quiet for 0ms`, a log line denying
  the action it had just taken; and because `abortUpload()` assigned `_upReason` unconditionally, the resulting
  `stalled` replaced the `write_failed` that was the actual cause, so every stalled transfer had really been a short
  write first. `SDMAN_UPLOAD_STALL_MS` also went from 30 s to 120 s: it had been firing the instant a chunk
  arrived, which proves the browser had merely paused mid-file and the device abandoned a live transfer.
- **Delete** takes the whole selection as a newline-separated body and walks it once. Refusals (protected or in use)
  and failures (missing, rmdir failed) share one `failed` counter in the response
  (`{"ok":<bool>,"deleted":N,"failed":M}`) because the page has a single message for "this stayed"; the serial log
  still tells the two apart. Child names are collected before anything is removed, because deleting during a
  directory walk advances the position under the removal and skips entries.
- The `isPlaying()` refusals in rename, delete and upload stay as a backstop even though entering the mode stops the
  player and `Player::_play()` refuses new playback: nothing may touch a file the player is reading.
- **An upload HOLDS the mode open, and a STALLED upload is the only thing that can release it early.** The idle
  branch in `FileManager::loop()` is guarded by `!uploadOpen()`, so the 180 s user clock cannot close over a live
  transfer, and a second branch - `uploadOpen() && uploadIdleMs() >= SDMAN_UPLOAD_STALL_MS` - abandons a transfer
  which has gone quiet (`filemanager.h`: 120 s while it can still succeed, 3 s once a chunk has failed, overridable).
  Before this the 180 s user clock was what closed a *live* upload: the mode went at the moment a chunk was slow to
  return, `leave()` removed the half-written file by design, and every remaining file of the batch answered
  `not_active` - a batch destroyed by our own timer, with the log naming an idle timeout that had not happened.
  `hDone()` had always refused to close over an upload; the timer did not, and that asymmetry was the bug.
- **The one close allowed over an open upload is guarded by `_upInWrite`.** Closing a `File` another task is inside
  is the cross-task hazard the log ring was fixed for, so the stall branch does nothing while a write is executing -
  it marks the reason (`stalled`) so the transfer ends deterministically at its next step, and a genuinely wedged
  write is left to the task watchdog, which `sdFeedWatchdog()` feeds *outside* the write and which will therefore
  reset the device by itself. A locked manager is the better failure there; a corrupted handle is not.
- **`abortUpload()` is the single owner of an upload that will not finish** - the stall branch, `leave()`, and any
  later path all call it, so the reason is always set before the close and the partial file is always removed. Its
  line names the byte count, because `490496 bytes` and `0 bytes` are the same failure until you can see how far the
  transfer got.
- **Log lines must print the values they TESTED, not re-read them while formatting.** The old idle line read
  `millis()` and `_lastActivity` in its argument list, so a `touch()` from the AsyncTCP task landing between the test
  and the print produced `idle for 180000ms, closing (now 94441, base 94440, elapsed 1)` - a report of a timeout that
  branch had not seen, and the reason a stalled upload's close was misread as an idle timeout for two rounds of
  diagnosis. The values are now copied out first.
- **A hidden widget's blank has to be RE-ASSERTED, not issued once.** `TextWidget::setText()` returns early when the
  string it is handed equals the one already up, and that early return is what makes it cheap - but the SD manager's
  countdown blanks to an empty string and then stays empty for minutes, so the one clear it performed on the way
  there was the only one it ever got: from then on the comparison matched, nothing was painted again, and the digits
  sat at their last value (a field report of a line "stuck" at 0:29 that reappeared as 2:00 the moment the string
  changed). `TextWidget::repaint()` paints the current text unconditionally and `Display::sdmanCountdown()` calls it
  every tick while the line is hidden; the two transitions are logged with the remaining time, which is what tells
  "the value stopped changing" apart from "the paint was never re-issued".
- **The page is TOLD the moment a transfer is lost, instead of finding out when it ends.** `/sdman/info?up=1`
  answers `open`, `lost`, `up` and `landed`, and the uploader's own 2-second progress watchdog polls it while a file
  is going up. It is a separate shape from the figures body because that one walks the FAT for `usedBytes()` - card
  reads issued while a chunk is being written - and this one touches no card at all. `lost` is tied to an OPEN
  handle rather than to the reason, because a reason outlives the transfer it describes and a stale `lost` would
  abort the next file's request.
- **The retry WAITS for the device to let go, and that wait is what makes the resume legal.** A failed transfer keeps
  its handle until the stall branch closes it, and closing it is also when `abortUpload()` records `_upResumeAt`. A
  retry sent before that is taken as a FRESH file: the target is truncated and the page's tail slice is written at
  position zero. So the page asks `open` and sends nothing until it is false - which is also what let
  `SDMAN_UPLOAD_STALL_FAILED_MS` drop from 10 s to 3 s, since the batch no longer waits out a window written for a
  browser that was never coming back. `hUploadDone()` had to learn to LEAVE that record alone as well: the aborted
  request can outlive the release, and its failure path used to zero `_upResumeAt`/`_upResumePath`, refusing the very
  retry the partial had been kept for.
- **An offset the device cannot vouch for is REFUSED (`resume_refused`), never written.** A request carrying
  `offset=N` is a slice, not a file: the old fallback ("starting it again") truncated the target and put the tail at
  position zero, producing a corrupt track reported as a successful upload. The page treats it as retryable, drops
  its figure and sends the whole file.
- **The upload bar counts the FILE, not the request, and the per-file notices are gone.** A resumed attempt is a
  slice, so its own `loaded/total` starts at nothing while the file is already part way there: progress is
  `from + loaded` over `f.size`, and a device-detected loss freezes it at `landed / f.size` - the figure the retry
  then continues from - so the bar never lies about what the card holds. Nothing is written to the message slot
  during a batch for a failure that is about to be resumed: the file name and the bar carry that, and the slot keeps
  its one line for the batch summary.
- **WHOLE-SECTOR WRITES: TRIED, MEASURED, REFUTED - and left in place as an off-by-default path.** The theory was
  that a 1436-byte chunk (a TCP segment, not a multiple of the card's sector) made the file system read-modify-write
  a PARTIAL SECTOR through the driver programming the previous block, which is a plausible mechanism for the `errno 5`
  refusals that arrive 4 ms in after the stream buffer has accepted a few hundred bytes.  The same 14-file, 96 MB
  batch on the same card says no: **aligned 12 then 17 failed transfers at about 7.3 minutes, as-it-arrives 7 then 7
  at about 5.0-5.8 minutes.**  The trend points the OPPOSITE way - a longer unbroken burst of sector programs is
  refused more readily - so a 4096-byte payload was never worth trying.  The machinery itself works exactly as
  designed (every resumed offset in the aligned run is a multiple of 512, so `resumeFloor()` and the assembly both
  did their jobs); it is the hypothesis underneath that failed. `SDMAN_WRITE_SECTOR` has since been removed with the
  experiment; the cluster assembly replaced it, and the tail flush it needed survives in `sdFlushTail()` at the end of
  the stream and before the final size check (`_upBytes` follows the WRITE, not the arrival).
- **A short FINAL size check now keeps the file, exactly as `card_lagging` does.** It is the same loss reaching us by
  another route - this check reads the file, that one reads it at a checkpoint - so it reports `landed` from the last
  verified position and leaves the partial in place.  It used to remove the file and send no figure, so the retry sent
  the whole thing: Teardrops landed 7332954 of 7381237 bytes and the next attempt sent all 7381237 again, when 48 KB
  would have finished it.  **Rule**: every path where the card came up short is a resume opportunity, and only a path
  with nothing verified may delete anything.
- **A DIAGNOSTIC THAT ASKS A CLOSED HANDLE IS A DIAGNOSTIC THAT LIES.** `indexSDPlaylist()`'s failure line asked
  `index ? "open" : "NOT open"` AFTER it had closed the handle, so it blamed "the index file could not be created" on
  every failure whatever the truth was - and printed `rows 1936875874` and `playlist 1070244108 bytes`, memory
  contents rather than sizes, from handles that had already been committed.  The open result is now captured at the
  open, each failure carries its `errno`, the walk counts what it SAW (`_walkEntries`, `_walkDirs`) beside the files
  it counted, and both temp files are re-opened by name for a size that came from the card (`sdFileSize()`) - the
  only second opinion there is.  The one real signal in that log was `of 0`: `_sdFCount * 4`, so the walk counted no
  files while `walked` said yes and neither "Failed to open directory" nor "SD write failed" appeared - a folder
  skipped or a listing that came back empty, with nothing to say which.  **Rule**: a closed handle answers false, so
  capture what is needed while it is open, and never print a number that did not come from a handle known to be
  valid.  This is the second time this module has been misled that way; the 1.5 GB "sizes" figure was the first.
- **PROVE THE STREAM RIGHT AFTER ANY RESCUE, not at the next checkpoint.** Both rescue paths - the partial-write repair
  and the refused-chunk rescue - now set `_upNextCheck = _upBytes + wrote`, so the existing flush-plus-`size()` check
  runs on the chunk that needed rescuing instead of waiting for the next 64 KB boundary.  The field pattern was
  refusal, rescue accepted, and then the card silently SWALLOWING the whole next window: `fell 28765 bytes behind`,
  `fell 49553`, `fell 53760` - one checkpoint's worth every time, so every byte written between the rescue and the
  next check was written for nothing and the transfer was failed 30 to 66 KB later than it had to be.  Measured
  effect: the reported shortfalls became **4303** and **4437** bytes, and the batch's recovery cost fell from 466 KB
  to **114 KB, 0.12 percent of a 96 MB set**, with each failure ending sooner and `_upVerified` left as high as it can
  be.  A rescue is the moment a transfer is most likely to be already lost, and that is where the proof belongs.
  Note the distinction this settled: a REFUSAL (`errno 5`, nothing accepted) is repaired in 0 ms and costs one chunk
  of tail, while the SWALLOWING (writes accepted, file not growing) is the expensive one - and it is caught by the
  checkpoint, which is why the checkpoint cadence, not the write granularity, is the knob that matters.
- **A BUDGET CARD BREAKS THE DESIGN'S ASSUMPTIONS, and the lag check now separates the two possible truths.** A cheap
  16 GB card refuses one chunk in about 180 (one per ~256 KB) where a good card refuses essentially never, so a file
  needs 20 to 40 attempts: the resume path held - recovery stayed at ~4 KB per failure - but the PAGE abandoned at a
  flat 8 attempts with megabytes still to send (Infinity at 2.78 MB, Shelter at 3.04, Intro at 4.04, and 7/14 landed),
  and every failure also cost the 3 s failed-stall window.  So: the attempt ceiling now SCALES with the file
  (`max(8, size / 256 KB)`, bounded by a hard 200) while `MAX_STALLED_TRIES` stays the size-independent guard;
  `SDMAN_UPLOAD_STALL_FAILED_MS` is 1 s; and the retry pause GROWS with consecutive refusals (2, 4, 8, 16, 32, 50 ms)
  instead of a flat 2 ms, because a card that is still programming needs time rather than the same question again.
  **The lag check asks whether the card is behind or the READING IS EARLY**: the size is read, the card is given
  `SDMAN_UPLOAD_LAG_SETTLE_MS` (50 ms) to settle, and it is read AGAIN - if it catches up the transfer continues and
  one line per file says so; only a shortfall that survives the pause fails the file, with both readings in the line.
  That single measurement decides whether a budget card's failures are real (a lost write - so back off in the write
  path) or an artefact of measuring mid-program (so read twice and never fail on the first number).
- **PER-FILE ACCOUNTING, because an attempt is not a file.** The page sends one file over several requests, so every
  figure in the per-attempt lines belonged to the LAST attempt: a file that took minutes and thirty attempts reported
  "657001 bytes moved in 3.23s, 172 KB/s, 0 rescued".  The device now accumulates `_filePath`, `_fileStartedMs`,
  `_fileMoved`, `_fileAttempts`, `_fileRetries`, `_filePartialRescues` across attempts - folding each attempt in when
  it ends, resetting when the path changes - and the success line is
  `Finished Upload <file> (N bytes moved in T, R KB/s, A attempt(s), X refused chunk(s) rescued, Y partial(s)
  recovered)`: the file's wall clock and totals, which is what a comparison run is actually comparing.  Renamed from
  `Uploaded` so it does not read like the `Upload ...` lines beside it.
- **AN ATTEMPT CAN END THREE WAYS, SO IT IS FOLDED FROM BOTH ENDS.** A response (`hUploadDone`), a stall release
  (`abortUpload`), or a cut-off - and two of them DO run for the same attempt. Folding only in `hUploadDone` missed
  every attempt the stall branch released, and the field caught it: `Finished Upload ... 1465995 bytes moved in
  85.33s, 16 KB/s` on a file that had run over 8.6 MB. `fileFoldAttempt()` is one guarded static (`_fileFolded`)
  called at the TOP of both, while `_upPath` is still set (which is what tells a retry of the same file from the
  start of a different one), and `_upBytes` is deliberately NOT zeroed by `abortUpload()` for the same reason.
- **The upload bar and its percentage only move FORWARD, per file.** A retry resumes from the figure the device
  vouched for, which is at or below the highest the bar already drew - so drawing it walked the bar backwards and
  read as lost progress. The device-poll path used to do exactly that deliberately; the page now keeps a per-NAME
  high-water mark and a lower figure is simply not drawn, so the figure freezes at the last good value and carries on
  when the retry climbs past it. Reset by `busy()` and whenever the file name changes, so the next file starts at 0.
- **TRANSPORT VERDICT: SPI and SDMMC write at the same speed; only SPI refuses.** Same card, same 14-file 96 MB batch:
  SPI at 20/30/40 MHz landed 5 to 8 files per batch with 114-466 KB of recovery; **4-bit SDMMC and 1-bit SDMMC both
  landed 14 of 14 with ZERO refusals, zero partial writes and nothing to recover** (167-255 KB/s per file, ~7.65
  minutes for the batch; SPI's fastest clean session was ~6.5 min, its slowest ~7.9).  The spread between sessions is
  wider than the difference between the transports, so the throughput figures do not choose between them - what
  chooses is that `errno 5` never appears on SDMMC.  That settles what the SPI logs kept implying: the refusals are
  the SPI TRANSPORT's (sdspi issues a command round trip per sector), not the card's and not the write granularity's.
  **(QUALIFIED LATER - see the allocation-unit entry above: the WRITE GRANULARITY does matter, and it is what decides
  whether those per-sector round trips can cope.  The transport verdict stands as measured on cards formatted the
  same way; what it must not be read as is "the format is irrelevant".)**
  Recommendation for a board with a choice: **SDMMC if the pins can be dedicated** (4-bit where the slot provides it,
  1-bit otherwise - same write performance, no refusals, and it frees the SPI bus for display/audio), with SPI kept as
  the universal fallback whose refusals now cost 0.12 percent of a batch.  Caveat measured on the es3c28p: SDMMC
  sessions report main-loop stalls of 628-1896 ms that the SPI build does not, worst stage sometimes `controls`, so
  investigate that before recommending it without qualification.  The one intermittent index-build failure we saw was
  on the SPI transport right after a session full of refusals; both SDMMC sessions built the index normally.
- **THE MAIN-LOOP STALLS ARE *NOT* THE ALLOCATION UNIT, and the field data says so three ways.**  The tempting theory
  was that a large unit makes the card queue programmed data before finalising a cluster, so the bus goes quiet and
  starves the loop.  It does not survive the measurements: the stalls appear on a card formatted at **512 B** as well;
  they are ABSENT from the cheap card's **16 KB** run, where every file was slow; and they are PRESENT in the good
  Sandisk's 16 KB run, which landed everything perfectly.  What that last run also had, and what the clean runs lacked,
  was `ESPFileUpdater` writing `timezones.json`/`rb_srvrs.json` to LittleFS flash DURING an upload - a flash write and
  a card write overlapping.  The stalls were also first recorded as a delete-burst symptom (1.2-2.4 s, with a
  `playerQueue overflow`), i.e. with no upload running at all, so the shape that fits everything is **contention on the
  shared flash/write paths, not the card's format** (the Part 1 "H2" line of investigation).  NOT FIXED - recorded so
  nobody re-derives that the unit is innocent here.  The fix shape already exists in this codebase: defer a flash write
  while an upload is open, the way the manager already defers its re-index.
- **`playerQueue overflow` DURING A DELETE BURST IS THE SAME STARVATION, and `hDelete` now drains the queue.**
  `Player::sendCommand()` drops a command and logs its type when the 10-slot `playerQueue` is still full after
  `PLQ_SEND_DELAY` (100 ms); the number printed is `request.type` (1 PLAY, 2 STOP, 3 PREV, 4 NEXT, 5 VOL, 6 CHECKSD,
  7 VUTONUS, 8 BURL, 9 TOGGLE).  The ONLY drain is `player.loop()` in `loop()`, gated on
  `status == CONNECTED || SDOFFLINE`, plus the two opportunistic calls in `SDManager::listSD()`.  The producers are the
  1 Hz `ctimer.attach(1, ticks)` (the "1ms" comment in `startup.cpp` is stale - `attach()` takes SECONDS) with `divrssi`
  halving it, so `PR_CHECKSD` and `PR_VUTONUS` arrive about once per two seconds; a whole-card burst that blocks the
  main loop on the shared bus for seconds per file therefore fills the queue and starts dropping ticks.  The delete
  batch loop now calls `player.loop()` once per item, exactly as the index walk does, and the commands execute in that
  task exactly as they already do there.  **Still open and NOT fixed: `SOFT_AP` and `FAILED` are outside the drain
  gate, while `ticks()` is not gated on status at all - so in AP mode commands accumulate, get dropped, and a stale one
  can sit in the queue until the status changes to `CONNECTED` and then fire.  `config.changeMode()`'s
  `player.resetQueue()` is the existing answer to that hazard.**
- **The upload writes ONE CLUSTER per file system call, via CMD25.** A multipart chunk is ~1436 bytes (a TCP segment,
  never a multiple of the 512-byte sector), so writing each as it arrives misaligns the file position for ever and FatFs
  falls back to its per-file sector window: one CMD24 per 512 bytes, each with its own select, command, status check and
  deselect. Instead filemanager.cpp holds the bytes until a whole allocation unit is ready and hands it over in one
  write. FatFs's direct multi-sector path runs because the write starts on a sector boundary and is several sectors
  long, and FatFs clips it to the current cluster - so a cluster-aligned write reaches the driver as ONE
  WRITE_MULTIPLE_BLOCK (CMD25) with CMD23 declaring the count, instead of 128 CMD24s on a 64 KB card. The driver's own
  partial-burst recovery (SEND_NUM_WR_BLOCKS) covers a burst that breaks. A fragmented file is fine: the clip is per
  cluster, so one call is still one cluster.
  - **The unit is the card's own cluster size**, read at mount (`SDManager::allocationUnit()`), capped at
    `SDMAN_ASM_UNIT_MAX` (64 KB, the largest FAT32 cluster with 512-byte sectors). A 512-byte card stands aside and says
    so once per session; a large-unit card gets whole-cluster writes.
  - **One buffer of `unit + SDMAN_ASM_SLACK`.** Two buffers (a tail array plus a payload array, the old experiment's
    shape) would be about 130 KB at 64 KB, and this board's largest heap block is 127888 bytes. The unwritten remainder
    is compacted to the front only when room runs out - in the steady state nothing moves. It is taken when an upload
    opens (a refused or skipped file never pays), reused across a batch, and released when the manager closes; not
    `.bss`, because a 64 KB static buffer would cost that RAM on every board whether uploads happen or not.
  - **PSRAM first, internal heap second, logged either way.** The SD write path uses no DMA (`sdWriteBytes()` sends 512
    bytes through `spiTransferBytes()`, which drives the FIFO in 64-byte pieces), so the buffer has no DMA-capability
    requirement and PSRAM is legal. The choice is logged (`Cluster assembly: 65536-byte unit, 69632-byte buffer from
    PSRAM`), which also shows whether a board's PSRAM is really enabled. If neither allocation succeeds the upload runs
    as before with a line saying so - a held cluster must never be a condition of uploading at all.
  - **The accounting follows the WRITE, not the arrival.** `_upBytes`, `_upVerified` and every check built on them count
    what the card accepted, so a held cluster is not counted yet - which is why `resumeFloor()` rounds to the assembly
    unit (a resume that began mid-cluster would misalign every write after it) and why the tail flush runs at the end of
    the stream and again before the final size check.
  - **The instrument is the log**: `Cluster writes for <file>: N write(s) of up to 65536 bytes, 3..8ms each (avg 5ms,
    245 KB/s inside them)` - one line per attempt, only when the assembly wrote something. A failed burst prints the
    driver's own `Card Failed! cmd: 0x19` (CMD25).
  - **Measured in the field, and it works.** A 64 KB card that had managed 75-95 KB/s with refusals took a 10 MB file in
    ONE attempt with ZERO refusals and ZERO partial writes: `154 write(s) of up to 65536 bytes, 29ms..73ms each (avg
    64ms, 1000 KB/s inside them)` beside `Finished Upload ... 10054052 bytes moved in 55.48s, 176 KB/s, 1 attempt(s), 0
    refused chunk(s) rescued, 0 partial(s) recovered`. 64 ms for 128 sectors is ~0.5 ms per sector, against the ~2.7 ms
    per sector the single-sector path cost at 95 KB/s - a real, roughly five-fold CMD25 effect.
  - **The gap between those figures is the next bottleneck, and it is not the card.** 154 writes is only ~10 s of the
    55 s wall clock: one cluster is 46 chunks of 1436 bytes, which took ~296 ms to arrive (~220 KB/s over the wire). The
    upload is now DELIVERY-bound, with roughly 5x headroom on the card. More speed means the network/HTTP path (and
    possibly the display's SPI work, which competes for the CPU) - not the card and not this assembly.
  - **Reformatting was the WORKAROUND; this is the CURE.** A 512-byte unit made the per-file sector window do one sector
    per write, which by luck is one unit - so reformatting "fixed" the symptom by making the accidental burst size
    correct. The assembly does that on purpose whatever the card's unit is.
  - **It reduces refusals, it does not eliminate them, and the recovery path makes that harmless.** On the same 64 KB
    card, a 500 MB MP4 into a deliberately fragmented file system refused once in 1257 cluster writes (82 MB in): the
    rescue was refused too, the write was reported short, the transfer was released, and the resume finished it - one
    round trip, not the file. Consequence: keep the whole recovery path (refusal rescue with backoff and budget,
    short-write repair, verify checkpoints, stall release, resume protocol) - it is now the difference between one round
    trip and 82 MB re-sent.
  - **The 512-byte card is the regression baseline and is unchanged** - the assembly stands aside
    (`Cluster assembly off: the allocation unit is 512 bytes, so one write is already one unit`), and 24 files (a 1 MB
    text log, an 11.8 MB exe, a 9.4 MB exe, a 12.4 MB zip among them) all landed on the first attempt with zero rescues
    at 82-95 KB/s.
  - **The 8 KB-cluster card is the OTHER end, and it is why the correction chain exists.** A field run of a 2.1 MB zip
    (9 attempts over 55 s, 37 KB/s overall) refused a chunk on nearly every transfer - `errno 5`, the rescue of it
    refused too, `0 of 8192` written - and every attempt was salvaged by the chain around it: the verify caught the card
    64 KB behind once, the stall release handed the transfer back, and each retry sent only the remainder (`Upload
    stopped at N bytes with M verified ... Resuming at M`).  The unit decides SPEED, not correctness, and a card that
    refuses writes needs this chain, not a formatter.
  - **The throughput ladder:** 512 B ~90 KB/s; 2 KB 164-181 KB/s (409-550 KB/s inside the writes); 64 KB ~190 KB/s
    overall (~1000 KB/s inside). Neither large unit refuses, so the difference is the COMMAND RATE (one CMD24 per 512
    bytes against one CMD25 per cluster), and the unit decides SPEED, not correctness. At 2 KB and above the card is not
    what limits an upload (the wire is, at ~220 KB/s); below it, it is. A formatter's default (4-16 KB) is comfortably
    fine, so no advice to reformat is needed.
- **The SPI upload marker and the reformat advice were removed** (the removed symbols are listed above); `au` stays in
  the info payload and on the Open line, being the diagnostic that found all of this. The locale keys
  `msg_spi_upload_marker`, `msg_format_au_512` and `msg_format_au_512_advice` are now unused (pruned by `* --clean
  --sort`), and `cardFault()` reports the plain failure (`msg_write_failed`) instead of a verdict.
- **THE DERIVED PAIR IS DROPPED ONLY WHEN THE PLAYABLE SET CAN HAVE CHANGED.**  `playlistsd.csv` and `indexsd.dat` are
  one object in two files: the rows, and 4 bytes per AUDIO file (an offset into them) plus an 8-byte footer
  `[magic 0x1867][count]`.  `initSDPlaylist()` validates the magic, `size == count*4 + 8`, and
  `storedCount == sdman.countAudioFiles()` - it RE-COUNTS the card's audio files - so a change that cannot alter that
  count cannot invalidate the pair, and dropping it was pure waste: an extra full walk plus the SDCHANGE screen every
  time a document or a photo was uploaded.  `dropSdDerivedFiles(bool audioChanged)` now decides, and when it keeps the
  pair it does NOT call `markCardChanged()`, which is what spares the close-time re-index, and it says NOTHING - keeping
  them is the ordinary case and a line per upload told nobody anything, while the drop line prints ONCE per session
  because it only fires when the pair really existed.  The four callers:
  upload asks `sdman.isAudioName(target)`; delete asks it of each path and treats a DIRECTORY as unknown; rename asks it
  of both names and likewise; a new folder is never audio-affecting.
  - **The extension test is now ONE function, `SDManager::isAudioName()`**, because the conditional drop must agree
    with the counter or a newly added track would hide until the next re-index.  The list (`.mp3 .m4a .aac .wav .flac
    .ogg .opus`) was written out TWICE, in `listSD()` and `_countAudioFilesRecursive()`, each with a `strlwr()` that
    lowercased the caller's buffer in place; both now call the one predicate, and the dead `_endsWith()` helper went
    with them.
- **The FAT-walk figure is now `Disk used: <used> of <total> bytes (<ms> ms walk)`, and it is SAID ONLY WHEN IT HAS
  SOMETHING TO SAY** - when the figure changes, or when the walk takes longer than `SDMAN_INFO_SLOW_MS` (20 ms).  The
  handler runs on every load, click and idle poll, so a line each time was noise; a changed figure is the only way a
  wrong count shows up, and a slow walk is a card answering slowly.
- **THE PAGE RECOVERS FROM A CLOSED MANAGER NOW, in both directions.**  The mode lives only while requests arrive (a
  180 s idle clock), so a batch begun after a quiet spell meets a closed manager: `xhr.onload` used to end the whole
  batch with "SD File Manager was closed." on the first such refusal, while the LISTING had always recovered by asking
  for the mode again.  `startUpload()` now pokes `/sdman/enter` before the batch (the common case, gone), and a
  `not_active` upload asks for the mode once and re-sends the same file - nothing of it was written, so nothing is
  lost.  A second refusal is real and still ends the batch.  A refusal that is instead `no_card` is terminal: the same
  answer reaches the page from `/sdman/enter`, `/sdman/list`, `/sdman/info` and the upload, and each one calls
  `leaveManager()`, so a card pulled from the slot takes the tab back to "/" instead of reporting a failure it cannot
  fix.
- **Log conventions in `filemanager.cpp`, settled in the field.** (1) `ERRORLOG` is for a LOSS only - a failure with
  nothing vouchable and no partial kept - because that is the only thing the user has to act on; every recoverable
  failure is an ordinary `FUNCTIONLOG` line, which is what stopped a batch of fully-recovered transfers reading as a
  list of failed files.  (2) ONE line per upload: the request-finished line prints only when it says something the
  outcome cannot (the handle still open, or a transfer already failing), with its counters otherwise folded into
  `Uploaded <file> (N bytes moved in T, R KB/s, A rescued, B recovered)` - the pair of them made a 14-file batch 28
  lines.  (3) ONE line per close: `leave()` takes a `why` string and names the reason beside the four conditions,
  where the idle timeout and the Done button each printed their own line above and below it.  (4) The first word is
  capitalised on every line, so the left edge can be scanned.  (5) A line that reports a measurement reads the value
  ONCE and prints what it tested - and if that value is the subtraction of two clocks, it must be CLAMPED:
  `millis() - stamp` wraps to 4294967289 ms when the stamp is written by the AsyncTCP task between the two reads, and
  the stall branch read that as forty-nine days of silence and released a live transfer with 6.4 MB in - which also
  opens the door to a retry writing the same file as the request it superseded.
- **A superseded request's chunks are DROPPED, by multipart index.** The index restarts at 0 for every request, so a
  chunk whose index is below the highest this transfer has seen cannot belong to it: it is the tail of a request the
  page already gave up on, still in flight.  Writing one would splice two transfers together, and reporting one gave
  lines about an empty path and a card that was never asked anything (`the rescue of a refused chunk at  was refused
  too`).  Dropped before the clock is stamped - a dead request is not activity.
- **THE RETRY BUDGET COUNTS ATTEMPTS THAT WENT NOWHERE, NOT ATTEMPTS - resume changed what a failure means.** An
  attempt that advanced the verified position did real work and the next one continues from there, so failing the
  file at that point throws the progress away. A field run lost the last file of a batch exactly there: 2.85 MB, then
  3.43 MB, then 6.47 MB of a 6,965,338-byte file - the third attempt ending 490 KB from the end - and that third
  failure spent a budget of two retries which says nothing about how far the file got. Teardrops survived the same
  shape of run only by having had one fewer retry. The page now counts per name: `PROGRESS_BYTES` (32 KB) of forward
  movement makes a retry FREE, less than that spends one of `MAX_STALLED_TRIES` (2) - the file that cannot get off
  the starting blocks - and `MAX_ATTEMPTS` (8) is the ceiling over both. A failure arriving with no `landed` at all
  (a dropped socket) counts as having gone nowhere. **Rule for new code**: a budget on a path that RESUME makes
  incremental must be spent by lack of progress, or it will fail the files that are nearly done.
- **What the wasted tail costs, measured from the 40 MHz run.** A failure ends the transfer but not the request: the
  browser pushes the rest of the file while the device drops every byte of it. Comparing the bytes the device
  actually moved against the file sizes gives ~19.4 MB wasted across the 14-file set at 20 MHz against ~55.5 MB at
  40 MHz - roughly 7.8 against 10.1 minutes of batch time - which is why the abort was worth building before
  settling `SDSPISPEED` on throughput alone.
- **One walker at a time, and the handlers that would start a second one refuse instead.** `/sdman/list` builds its
  answer with a chunked response whose filler (`sdmanListFiller`) keeps the directory handle in module-static state
  between callbacks, but a second request over the same handler starts the walk again and closes the handle the first
  one is still using - the 7-14 s walks and the "listing failed" flakiness after the first one. `hList()` therefore
  answers 409 `busy` while a listing is in flight (`_listDir` open, `_listDone` clear, and within
  `SDMAN_LIST_ABANDON_MS` = 5000 ms of its start) and 409 `uploading` while an upload holds `_upFile`, and the page
  retries those quietly instead of showing an error - that is what makes a refresh during an upload wait for the upload
  rather than tear it down. The walk is also fed to the watchdog every eight entries, and the driver's own
  `_listT0`/`_listDone` guard is the reason a torn-down response does not wedge the next one.
- **Entry works in AP mode.** The `network.status != CONNECTED` guard was removed because it protected nothing:
  `_switchMode()` refuses every mode change while the status is neither `CONNECTED` nor `SDOFFLINE`, so the AP screen
  (SSID, password, address) is never taken over, and the card stays reachable as plain storage on a device with no
  network. SD-offline needs no test of its own - `NetServer::begin()` returns before the web server starts.
- Coupling: `controls.cpp` returns early in every physical input path while the mode is open - both encoder loops, the
  IR loop and IR number entry, and the click, double-click, long-press-start/stop and during-long-press callbacks, plus
  `controlsEvent()` underneath them; `display.cpp` `_switchMode()` refuses any mode
  other than `PLAYER`/`SDMAN` while the mode is open, `commandhandler.cpp` drops commands, `Player::_play()`
  refuses playback, and `startup.cpp`'s async services task parks while the mode is open - the same park SD playback
  already used for its DRAM/decoding reasons, since the manager is rewriting the card and a download starting on the
  network core is the last thing it wants. All five exist so the manager is the only thing touching the card while it
  is open. The park is released by itself when the manager closes, Done button or idle timeout alike, within one
  2-second poll, and while it is held `Startup::loop()` marks the boot stable after
  `STARTUP_ASYNC_SERVICES_DELAY + BOOT_STABLE_TIME` with the reason `startup services were suspended` — the same
  rule that now covers SD playback, see the startup section.
- The encoder guards drain rather than only returning: `encoderChanged()` is a delta-since-last-call, so skipping it for
  a whole session delivers every detent turned meanwhile as one `int8_t` delta on the first loop after the mode closes,
  i.e. a volume slam or a phantom station step. The long-press-stop guard also clears `lpId`, because `Controls::loop()`
  keeps calling `onBtnDuringLongPress()` for as long as that latch is set.

- **One idle clock, shared by every client, and the transients are silent.** `_lastActivity` is a single value on the
  device, so any request from any tab or browser resets it and the on-screen countdown is that one number. `enter()`
  refreshes it **before** its own `if (_active) return`, so a second tab loading the page silently restarts the three
  minutes - the `open (...)` line prints only on the transition into the mode, which is why a phantom reset has nothing
  in the log. The page's own copy of the deadline (`scheduleIdleReload()`) is armed only by responses carrying `idle`,
  and an expired tab navigates to "/", which `handleIndex` redirects back to `/sdmanager.html` while the mode is open,
  so that tab re-enters the mode and resets the device clock as well. The reset a user can drive on purpose is the
  ping: `keepAlive()` on a document click (1000 ms floor) and on keydown (15000 ms), both calling `/sdman/info`, which
  touches the clock. A click is the *only* signal a file picker leaves - the dialog is native, so neither a chosen file
  nor a cancel reaches the page, and the countdown simply keeps running while it is open; the click that opened it
  bought the 180 s, and clicking again is how a cancelled picker is recovered.
- **The idle redirect is judged when it FIRES, and held for the whole batch.** `scheduleIdleReload()` tested
  `state.uploading` only at the moment it armed the timer - and the delay it arms is the device's entire idle
  window, so a timer set on the page's first `/sdman/info` could expire in the middle of a running batch and
  navigate to "/", aborting the upload in flight. The device sees that as a perfectly healthy upload going silent
  for its whole stall window, which is exactly what a field log showed, and it is why a batch ended after one file.
  The test now happens inside the timer callback, against a batch-level flag (`state.batch`) rather than the
  per-file `state.uploading` that `finish()` clears between files; `keepAlive()` respects it too, and only `done()`
  clears it.
- **The page watches upload PROGRESS, because nothing else can see a transfer that has stopped moving.** There is no
  event for it: the device answers nothing until the last byte, so `xhr.timeout` cannot fire, and a browser that
  stops sending produces no callback at all. A 20 s interval (`UPLOAD_STALL_MS`) aborts a file whose
  `xhr.upload.onprogress` has not moved, and `onabort` gives it the same single retry as a dropped connection before
  dropping it and carrying on with the batch. That is the difference between one bad file and a lost batch: the old
  behaviour left the device waiting out its own deadline for a browser that had stopped sending.
- **The on-screen countdown shows the upload's deadline only once a chunk has FAILED.** In that state the file is
  already lost and the mode is about to be released, so the number is counting down to something real (the 10 s
  `SDMAN_UPLOAD_STALL_FAILED_MS`). While the transfer is healthy the same number would be restamped by every chunk
  and sit at a frozen 1:59, which reads as a stopped clock - so the ordinary idle clock is shown instead. The
  deadline is still enforced by the stall branch in `loop()`; the display simply stops advertising it.
- **Every close now names its cause, and the Done button is refused mid-upload.** Three things end the mode and
  `leave()` alone cannot tell them apart, so each says so before calling it: `hDone()` logs `closing on request
  (Done button)`, the idle branch logs `idle for <N>ms, closing (now, base, elapsed)`, and the card-gone branch
  already logged `card gone for N checks`. The numbers in the idle line exist because `idleRemainingMs()` answers
  zero for `!_active` as well as for an expired timeout: a `leave()` from the network task landing between the
  `_active` guard at the top of `loop()` and the idle check below it used to be reported as a 180-second timeout,
  which is a line naming a cause that branch had not caused - and it is how a close the user asked for was
  mis-read as the browser going quiet. The branch therefore re-tests `_active`, and a base that is not what the
  chunk touches wrote would now be visible in the line. `hDone()` also refuses with 409 `uploading` while
  `uploadOpen()` is true, because `leave()` deletes the half-written file on purpose: a Done press from another
  tab or another device would otherwise throw away an upload nobody finished watching. Between the files of a
  batch `_upFile` is closed, so this only stops a close that would destroy something. The refusal is deliberately
  inert - it does not `touch()`, so a Done press cannot be used to keep a stalled upload's mode alive - and it
  logs `Done refused, upload in progress`, so a cross-tab refusal is visible instead of silent. The page reads that
  answer
  instead of navigating blindly - a 200 (the redirect the device sends, already followed by `fetch`) means
  closed, anything else means refused, and the reason is shown with `msg_upload_in_progress`, a new key the
  locale files do not carry yet.
- **A slow card costs time; it does not abort the device.** Card work is slow by nature and it runs on whichever task
  asked for it, which for the WebUI is AsyncTCP's task - and AsyncTCP subscribes that task to the task watchdog, so a
  single multi-second operation aborts the firmware with `task_wdt: async_tcp` instead of merely delaying the page.
  That is exactly what a 15 GB card did on entering the manager while a 7.5 GB one was fine. `sdFeedWatchdog()` in
  `sdmanager.h` (`esp_task_wdt_reset()` plus one tick, since a tick also lets the other tasks run) is now called from
  every long card walk: the listing filler every eight entries, the free-space figure either side of the FAT walk, and
  the two index walks (`listSD()`, `_countAudioFilesRecursive()`), which already yielded per entry with a bare
  `vTaskDelay(2)` but never fed the watchdog. Where the caller is not subscribed the reset reports
  `ESP_ERR_NOT_FOUND` and is harmless.
  - `removeRecursive()` was the last unfed walk and is now fed in both of its loops (the name collection and the delete
    recursion), so deleting a folder holding a hundred entries on a slow card cannot abort the device - it is the same
    multi-second walk in the same task.
  - **Rule for new code**: any long card operation reachable from a web handler must feed the watchdog, because that
    handler runs in the AsyncTCP task and that task is subscribed.
  - **The subscription is deliberate** (`CONFIG_ASYNC_TCP_USE_WDT 1` in `options_overrides.h`): it turns a handler
    wedged on a stalled SD call into an automatic reboot instead of a device that needs a power cycle. The cost is the
    library's own 33-200 us per TCP event. Setting it to `0` in `myoptions.h` is the supported way to switch it off, and
    it reaches the library through the force-include.
- **The figures are recomputed on every request, never cached** - deliberately: a stale tree or a stale free-space
  figure would be worse than a slow one, and the answer to a slow card is co-operation, not staleness. What
  `/sdman/info` no longer does is pay for the *same* walk twice: `usedBytes()` is the walk, so the total comes from
  `cardSize()` (the CSD the card reported at init, instant) and only the used bytes are walked, once. The cost is
  reported rather than hidden - `figures: used X of Y, N ms` from `hInfo()` and `walked N entries in M ms` from the
  listing - so a slow card shows up as a number in the log instead of a watchdog backtrace.

## `src/core/touchscreen.h` / `touchscreen.cpp`
- Touch controllers:
  - XPT2046
  - GT911
  - FT6336
- Responsibilities:
  - init and orientation/flip
  - swipe and tap/long press mapping to control events
  - touch debug coordinates now route through centralized logging macros

## `src/core/rtcsupport.h` / `rtcsupport.cpp`
- RTC init/get/set wrappers for DS3231/DS1307 when configured; `rtcsupport.h` has compile guards.

---

## WebUI Per-File Map (`data/www`)
- **Keep a script inside its page when only that page uses it.** Every file in `Config::wwwFiles[]` is fetched
  separately by the OTA updater and each fetch pays its own HTTPS handshake, so fewer files means a faster update for
  every device in the field - the byte count of the file barely matters beside that. `search.html`, `curated.html` and
  `sdmanager.html` therefore carry their own `<script>` block inline, while `script.js`, `script2.js`, `variables.js`
  and `curated_variables.js` stay separate because more than one page loads them. Removing a file from `wwwFiles[]` and
  from `data/www` must happen together, and firmware plus filesystem must then ship together: older firmware still lists
  the name and reports a missing file.

## `data/www/script.js`
- Main runtime script.
- Responsibilities:
  - websocket connect/reconnect
  - parse inbound JSON payloads
  - dynamic page loading (`player`, `settings`, `update`, `ir`)
  - shared page bootstrap helpers for logo/version/i18n application
  - safe lazy fallback for script2-exposed functions (`ensureFunctionLoaded`)
  - control dispatch from DOM (`data-command`)
  - playlist editor/import/export logic and curated integration
  - online update UI progress handling
  - shared ready-aware redirect helper (`redirectWhenReady`) used by update and reboot flows
  - mode switching (`changeMode`) and its guard state (`setModeSwitching`)
- **A mode switch blanks the page and locks it, and the page never has to guess when it is over.** `changeMode(el)`
  flips the two mode icons, calls `setModeSwitching(true)` and only then sends `newmode=`, so the icon, the blanked
  list, the loader and the lock all happen in the same tick as the press; `setModeSwitching(true)` puts the `plloader`
  spinner into `#playlist` (the same markup `generatePlaylist` uses), empties `#pleditorcontent` so the editor cannot
  offer the mode being left, and sets `body.switching`, which `style.css` turns into a dim plus `pointer-events: none`
  over every `.gb` control and over the playlist. Two signals then come back, with distinct meanings: `playermode`
  **with a changed mode** is the START of a switch (sent at the start of `Config::changeMode()`, before `sdman.start()`
  and before the card is verified or indexed) and the page only locks - it does not fetch, because an answer now is
  either the list being left behind or one still being rewritten; `playlistready` (request type 35) is the END, sent at
  the end of `changeMode()` and at the end of the deferred rebuild in `FileManager::loop()`, and that is what unlocks
  and fetches. It is sent even when a build produced nothing, deliberately: the page waits for the device to STOP
  TRYING, never for a build to succeed, or a card that refuses a write would leave it locked behind the loader. It is
  also sent when a client connects, so a page opened mid-switch still ends up with a list, and the first `playermode`
  after a load only establishes which mode we are on (`_modeKnown`) rather than counting as a switch. A backstop timeout
  releases the lock if the device never answers. The two branches of the icon flip were the wrong way round for a long
  time - each hid the icon just pressed and then showed that same icon again - so the flip never happened locally and
  only the device's message ever did it, which is after the work.
- Reboot/update redirect nuance:
  - `redirectWhenReady(...)` only redirects after it has observed at least one not-ready state, preventing a
    false-positive redirect against the still-running pre-reboot instance.
  - `/ready` probes now use a short client-side fetch timeout so reboot/reset flows do not stall waiting on a dead
    device connection during restart.
  - reboot and update redirect calls now explicitly apply a 1-second post-ready grace in JavaScript before navigation.
  - manual upload completion now uses a 60 second fallback, while OTA still uses 180 seconds; both can redirect early as
    soon as `/ready` reports true.
  - mDNS rename (`restartmdns`) calls `MDNS.end()` + `MDNS.begin()` at runtime via `NetServer::restartMdns()` — no
    reboot. Browser-side: sends `mdnsname=`, swaps the button row for a status message, then polls the new `.local` host
    with `redirectWhenReady` (8s timeout, 500ms post-ready grace). If mdnsValue is empty, saves silently without
    redirect.
- consolidated with `data/www/dragpl.js`
  - Playlist drag-and-drop reorder behavior.

## `data/www/options.js`
- Settings page behavior.
- Responsibilities:
  - timezone JSON loading and dropdown population
  - locale list loading and locale switch logic
  - weather provider field visibility logic
  - MQTT credentials visibility: `setupMqttToggle()` collapses `#mqttsettings` while `#mqttenable` is off, synced on
    click and from the `getmqtt` payload; unlike the layout-driven groups this is not a device-reported `act` token
  - **theme/layout dropdown loading** — fetches `/themes.json` and `/layouts.json`, populates `#themeId`/`#layoutId`
    dropdowns, sends `websocket.send("theme=N")` / `websocket.send("layout=N")` on change
  - `afterSetupElement` hook restores current `themeId`/`layoutId` from WebSocket data
  - apply handlers for locale/weather/mqtt/wifi
  - reboot/reset/format status screen with per-action behavior:
    - reboot/reset use ready-aware return-to-root with shorter fallback (15s)
    - format LittleFS shows reboot status but skips automatic reload

## `data/www/player.html`
- Player page structure (playlist, controls, sliders, status elements).
- The `#info` row is one line by construction: `nowrap`, no per-item minimum widths, and only `#bitinfo` flexes (it
  ellipsises) while volume, battery, shuffle and the RSSI bars keep their natural width. `#batteryinfo` is part of that
  row - it is not a row of its own - and carries the label from `ttl_battery` plus the percentage.
- RSSI is a graphic, not a number: `#rssibars` holds a four-path SVG (weakest first) sized in `em` so it tracks the info
  text at every breakpoint, with a `<title id="rssititle">` giving `RSSI: -29dBm` as the tooltip. Bars past the
  reported level take the `.off` class and are painted `--main-bg-color` rather than hidden, so the shape never resizes;
  a level of 0 leaves every bar unlit, mirroring the empty glyph pair the display widget draws below the last step.
- `#shuffle` is a `.gb` button inside `#playernav`, wedged between next and the mode switch, so it is the same circle as
  its neighbours at every breakpoint and inherits the `.active` states from that family instead of carrying rules of its
  own. The `playermode` branch in `script.js` is still what shows it in the SD player only, so the info row is left with
  volume, codec, battery and the bars.

## `data/www/settings.html`
- Settings page structure with grouped sections and `data-command` bindings.
- Contains element IDs expected by websocket payload mapping.

## `data/www/update.html`
- Update page layout (manual upload + online update controls).

## `data/www/ir.html`
- IR recording and assignment UI.
- Every `.irbutton` carries a `data-irid` name (`power`, `mute`, `up`, `down`, `prev`, `next`, `play`, `mode`, `number`,
  `n0`…`n9`) that maps 1:1 to the `irstore` field / NVS key, so DOM order is irrelevant.
- The shell loads the page body from `irrecord.html`; the `/ir.html` route itself is handled by the PSRAM cache
  fallthrough.

## `data/www/search.html`
- Search UI for radio-browser integration.
- **Carries its own script inline** (formerly `data/www/search.js`, now the `<script>` block at the foot of the page):
  websocket search progress, results fetch with a 404 retry, pagination, quick searches and the import hooks.

## `data/www/curated.html`
- Curated list browsing/import page.
- **Carries its own script inline** (formerly `data/www/curated.js`): index fetch, playlist list fetch, and the
  replace/merge import into the playlist.

## `data/www/script2.js`
- Consolidated helper script loaded by main shell and standalone search/curated pages.
- Contains logic previously in `ir.js`, `updform.js`, `playstation.js`
  - station preview/play helper (`sendStationAction`)
  - online update check/start UI helpers
  - IR setup/learn interactions (`initControls`, `checkSelect`, `irClear`, `backRecord`); `irbuttonClick()` sends the
    button's `data-irid` name as `irbtn=<name>` and `irbtn=-1` on deselect
- also consolidated with `data/www/locale.js`
  - i18n runtime helper (`t(...)`) and translation application (`applyI18n`).
  - Applies key-based translations to DOM and fallback behavior.

## `data/www/style.css`
- Primary stylesheet.
- `#info` is a one-line flex row by construction: `nowrap`, no per-item minimum widths, and `#bitinfo` the only item
  that
  flexes (centred, ellipsising). The old `min-width: 110px` per item plus `flex-wrap: wrap` is what used to push an item
  onto a second row; `#batteryinfo` no longer has `width: 100%` or an `order`, so the battery shares the row.
- Sizes that must track the text use `em`: the RSSI bars are a `1em` square, which rides the 12/14/18/21px info font at
  the four breakpoints without a rule per breakpoint.
- The playlist editor input carries the **same ladder as `#playlist li span.text`** (20px base, 16px at ≤375, 25px at
  600-899, 30px at 900-1200), so a name reads the same in the list and in the editor. It is not tied to the SD manager's
  `.sdname` any more; the editor's own spans and the playlist's count column already agreed at 14/12/18/21.

## `data/www/theme.css`
- Theme override variables/colors.

## `data/www/locales.json`
- Locale-code to display-name mapping for selector.

## `data/www/timezones.json`
- Timezone label -> POSIX tz mapping used by settings UI.

## `data/www/rb_srvrs.json`
- Radio-browser server source list used by search task fallback and randomization.

---

## Displays Folder Map (`src/displays`) - Grouped

## Core display abstractions
- `src/displays/dspcore.h`
  - common display core wrapper and API layer used by `core/display.cpp`.
- `src/displays/widgets/widgets.h`, `widgets.cpp`, `widgetsconfig.h`
  - widget classes (scroll, text, bars, VU, clock, playlist, etc.).
  - **Boot-line messages** (what the `_bootstring` line says while the boot is blocked): one request type per message in
    `displayRequestType_e` (`common.h`) plus a case in `Display::draw()` that does
    `_bootstring->setText(l10n(L10N_MSG_...))`. The three are `WAITFORSD`, `FORMATTING` and `SCANNINGWIFI` — the last
    for the boot scan, which blocks the radio for ~5.7 s with nothing else to show, and it is sent from `wifiBegin()`
    just before the scan. **Senders must include a short `delay()` after `display.putRequest(...)`**: it only queues,
    and the blocking work that follows can otherwise start before the display task has drawn the line (the `FORMATTING`
    sender does the same). The strings live in the generated `src/locale/dsplocale.h`, so a new message needs its key
    added to all 36 `src/locale/display/*.json` files and that header regenerated; measured cost of one new key across
    the 36 locales is **~5 KB of flash**, which is the number to weigh before adding one.
  - `TextWidget::setText()` is **virtual** (all three overloads), with `ScrollWidget` and `NumWidget` marking their
    matching overloads `override` so the compiler enforces the match. This matters because a subclass can legitimately
    be held in a `TextWidget*`: the boot line is exactly that case (`Display::_bootstring` is a `TextWidget*` over a
    `ScrollWidget`). Bound non-virtually, a call through that pointer runs the **base** version, which never sets
    `_doscroll`/`_x` and measures with the base `_charWidth` — the boot line then could not scroll and, for a string
    wider than the window, was painted at an underflowed centre offset, which reads as "nothing drawn". `_draw()` was
    already virtual, so this removes a static/dynamic mismatch rather than adding a mechanism.
  - `TextWidget::_realLeft()` **clamps** rather than underflowing: when `_textwidth` is not smaller than the available
    width it returns `0` (the widget's own left edge) instead of wrapping the subtraction to ~65500 and painting
    off-panel. Only reachable for text wider than its space — a `ScrollWidget` scrolls instead and a plain `TextWidget`
    is conf-sized — so the clamp is insurance against the invisible-rather-than-misplaced failure mode.
  - The boot text line (`Display::_bootstring`, built in `Display::_bootScreen()`) is a `ScrollWidget` fed from the
    conf's plain `bootstrConf` `WidgetConfig`, so a string that fits is drawn statically at the conf's align (WA_CENTER
    still rules) and a longer one parks at the edge and scrolls. Its window and buffer are derived in code (`width =
    MAX_WIDTH`, `buffsize = BOOTSTR_LEN` 128, uppercase), and its **scroll cadence is borrowed from the panel's own
    `apSettConf`** — `startscrolldelay`, `scrolldelta` and `scrolltime` only, never that entry's
    `left/top/width/buffsize/fontsize`, which belong to its own line and differ on the round TFT (`left =
    TFT_FRAMEWDT+32`, `width = MAX_WIDTH-64`) and the 220x176 (`width = DSP_WIDTH+10`). That inherits each panel's tuned
    step: 1px on the OLEDs and small TFTs, 2px on the 220x176 and every panel 240px and up, 4px on the 428x142,
    `apSettConf.widget.textsize > 0` is the guard if the entry is `{ }`, which zero-initialises it and would leave the
    boot line standing still — the fallback is `SCROLLTIME`.
  - `ProgressWidget` is the boot screen's animated dots line, with exactly one instantiation (`Display::_bootScreen()`),
    so its behaviour is the boot screen's. The line is `speaker + runway + boot glyph`, hard against both glyphs, and
    `ProgressConfig` is `{ frame interval ms, line character width, blob elements }` — every conf carries that header
    above the field, and `conf_tool.py` emits it for generated confs. `width` is the **whole line budget in
    characters**: `init()` derives the runway as `width - 2`, counting each glyph as **one character regardless of its
    byte length** (the SD pair renders one column wider than the rest, which is invisible on a single row of pixels).
    **There are two paint paths, and that split is what keeps a TFT flicker-free**: `_draw()` is the full paint —
    speaker, the runway with the blob where it belongs, boot glyph — reached only from activation, layout changes and
    screensaver restarts, so the two static glyphs are drawn once and then never touched; `_progress()` is the
    animation, and it paints **only the cells that differ from the previous frame**, at most two of them (the cell the
    blob left, the cell it entered), against a whole-line erase plus fourteen glyph writes in the single-path version.
    `_fieldX` records the field's x origin at the last full paint, and `_painted` guards the first tick so a delta can
    never be painted against a picture the widget did not put on the panel; both paths place column `c` at `_realLeft()
    + (frameChars + c) * _charWidth`, which is why a full paint cannot shift the dots. The reason is the panel rather
    than the animation: an SH1106 is a framebuffer whose refresh hides an erase, while an ILI9488 shows every write, so
    the same code flickers on one and not the other. The blob grows in at the speaker, slides right one column per frame
    and has its head eaten at the far end, which is what makes the dots read as vanishing into the boot glyph; the cycle
    is `runway + blob` frames (the 128x64 OLED conf's `{ 90, 14, 4 }` is a twelve-column runway and sixteen frames, 1.44
    s), and the character count is identical on every frame so the centred position never moves. A blob at or above the
    runway never reaches the sliding phase — it just grows and snaps back. **The blob element is an icon codepoint,
    `\026` (VOL_75) from `icons.h`, not a font glyph**: that is what makes it line up, because icon codepoints all share
    the same seven-row grid as the speaker and the boot glyph, whereas a font bullet carries its own vertical metrics
    and sits off centre between them. **The text buffer is sized in BYTES rather than characters, for a reason worth
    keeping**: with the earlier two-byte U+00B7 dot, a character-count buffer truncated the line mid-dot, a dangling
    `0xC2` made the renderer take the terminator as its second byte and draw past the NUL (a glyph no font knows), and
    the changed character count moved the centred x every frame. `_buffsize` is `frame bytes + runway + one extra byte
    per blob element + 1`, which reduces to `frame bytes + runway + 1` while the element stays one byte. Nothing here
    uses `Widget::_width`, which `Widget::init()` zeroes and `moveTo()` rewrites — sizing from that drew a completely
    blank line. `_scrolldelay` is set in `init()`; it was previously never initialised.
  - `SliderWidget` buffer/volume bar rendering now repaints the full inner area each update to avoid stale pixels after
    page/mode transitions.
- `src/displays/widgets/pages.h`, `pages.cpp`
  - page and pager composition framework.

## Display driver files (`src/displays/display*.h/.cpp`)
- Similar pattern:
  - init hardware
  - draw primitives/text/pages
  - sleep/wake/flip/invert where supported
- `flip()` maps `config.store.flipscreen` to `setRotation(...)`. Rotation values follow the Adafruit GFX convention
  (0=0°, 1=90°, 2=180°, 3=270°) relative to each controller's native orientation. `ROTATE_90` (optional, for square
  panels) is applied only when `DSP_WIDTH==DSP_HEIGHT` and adds 90° to that controller's normal base rotation.
- Files include:
  - `displayST7735*`, `displayST7789*`, `displayST7796*`
  - `displayILI9341*`, `displayILI9488*`, `displayILI9225*`
  - `displaySSD1306*`, `displaySSD1305*`, `displaySH1106*`, `displaySSD1327*`, `displaySSD1322*`
  - `displayGC9A01A*`, `displayGC9106*`, `displayNV3007*`

## Display config files (`src/displays/conf/*.h`)
- Mostly widget coordinates/sizing/visibility for each panel class.
- Treated as layout maps rather than logic-heavy files.
- Consolidated canonical conf files now route multiple models by panel type/dimensions:
  - `displayOLED128x64conf.h` for SH1106/SH1107/SSD1305/SSD1306 128x64 OLED class.
  - `displayTFT480x320conf.h` for ILI9488/ST7796 class.
  - `displayTFT320x240conf.h` for ILI9341/ST7789 class.
- The canonical files are intentionally deduplicated masters without per-model `#if DSP_MODEL...` branches;
  model-specific legacy deltas were removed in favor of one shared layout baseline per family.
- Display driver headers now include conf files directly; custom fallback include blocks using
  `__has_include("conf/*_custom.h")` were removed.
- `metaBGConf` / `metaBGConfInv` — runtime-switchable station-name background (full bar vs thin line on TFT; bar vs
  no-bar on OLED). Controlled by `config.store.inverttitle`.
- OLED configs: `metaBGConf` may be `{ }` (normal mode no bar), `metaBGConfInv` has the bar (inverted mode).
  `conf_tool.py` auto-swaps when importing into OLED targets.
- All conf files must define every `WidgetConfig` / `FillConfig` / `ScrollConfig` / `ProgressConfig` / `VUBandsConfig` /
  `MoveConfig` field that `display.cpp` references — nothing is optional at link time. Disabling a feature is done by
  zeroing the relevant struct (e.g. `height=0`, `buffsize=0`, `dimension=0`); `display.cpp` checks those sentinel values
  at runtime and skips the widget. This eliminates the old `HIDE_*` compile-time macro system entirely.

## Display tools
- `src/displays/tools/pretext.h` / `pretext.cpp` — `preText()` font-aware text resolver (keep / fold / replace),
  `allCaps()`, `foldToBase()`, `glyphAvailable()`, `utf8_strlen()`, `utf8_offset()`.
- `src/displays/tools/dspstats.h` — Core Monitor counters (`cmGlyphCount`, `cmPreTextCalls`, `cmPreTextHits`,
  `cmFillCount`, `cmPushCount`, `cmDspCore`) with increment helpers that vanish unless `CORE_MONITOR` is defined.
  Definitions live in `core/display.cpp`; the fields are printed and documented under CORE_MONITOR below.
- `src/displays/tools/gen_fold_table.py` → `pretext_fold.h` — generator for the stepping tables, plus the generated
  header (3,624 entries, 14,496 bytes, emitted in two halves so supplementary keys still fit 4-byte entries). `--stats`
  reports coverage per Unicode block.
- `src/displays/tools/commongfx.h`
- `src/displays/tools/psframebuffer.h`
- The old `utf8To.*`, `utf8_common.*`, `utf8Latin.*` and `utf8Cyrillic.*` files are **gone** (they were replaced by the
  Unicode GFXfont pipeline); `localization-guide.md` still described them as "retained for reference".
- `src/displays/tools/oledcolorfix.h` — OLED monochrome color initialization. `DSP_INVERT_TITLE` ifdef removed;
  `metafill` corrected to `TFT_FG` (was `TFT_BG` — invisible on black screen).

Purpose:
- text normalization/transliteration and glyph handling
- display utility support

## Display fonts/assets

**Both fonts are runtime choices now.  There is no `TIME_SIZE`, no `CLOCKFONT5x7`, no `Clock_GFXfont`
and no `fontNN.h`: the whole compile-time font selection is gone.**  `plans/font-overhaul.md` is the
design record; this is the shape of it.

- **System fonts** — one folder each, `src/displays/fonts/<Name>/`, holding the `.bdf` and the header
  `bdf2adafruit3.py` generates from it.  All of them are compiled in.  `dspfont.h` declares them
  `extern` and holds `_systemFontNames[]` / `_systemFonts[]`; **`fonts/fonts.cpp` is the only
  translation unit that includes the headers** — a namespace-scope `const` has internal linkage, so
  including them from `dspfont.h` put the whole set in flash once per TU that reached it (measured:
  39 KB on sh1106/es3c28p, 78 KB on ili9488).  `displayFont()` returns
  `_systemFonts[activeSystemFontId]`.
  **Every system font must stay on the 6x8 metric class** (`xAdvance == 6`, `yAdvance == 8`) — layout
  comes from `CHARWIDTH`/`CHARHEIGHT`, not from the font — and that is validated at boot.
- **Clock fonts** — one folder per STYLE, `src/displays/clockfonts/<Style>/`, holding
  `{8,15,21,28,35,52,70}.png`.  `py makefont.py <Style>` converts the whole folder into one
  `clockfonts/<Style>.h` and **aborts, naming every missing size, if the set is not complete**.  The
  generated header defines the seven `GFXfont`s, the two glow strings and a `ClockFontStyle`; the
  three styles are declared in `dspfont.h` as `_clockFontStyles[]` and defined in
  `clockfonts/clockfonts.cpp`, which is the only TU that includes them.  `clockFontStyle()` is the
  accessor, driven by `config.store.clockFontId`.
- **The clock size is a LAYOUT value** — `clockConf.textsize`, a size index: `0` = the system font
  (all the 128x32 panel does), `1..4` = 15, 35, 52, 70 px.  It used to be `TIME_SIZE`, one number for
  the whole build derived from `DSP_HEIGHT`; every shipped conf now carries the index that rule
  produced for its panel.  `numConf.textsize` is the same index for the number page, and the two are
  independent — the widgets resolve their own `ClockFontSel`, which is why there is no global clock
  font pointer any more.
- **The seconds** are drawn with that style's `8/15/21/28` sibling at the same ink top the built-in
  cell used, falling back to the built-in font when the style has no sibling at that index.  Their
  glow uses `theme.secondsbg` / `theme.secondsbgss`.
- **`fullClock` and `seconds` are per-layout booleans** (`LayoutData`, first two of the TRANSFORMS
  group), replacing "the size is 52 or 70, or the model is an ILI9225" and "this size has a seconds
  font".  The ILI9225 model hack is gone.
- **`clockglow` is a stored setting** (`config.store.clockglow`), not the `CLOCKFONT`-derived macro.
- **`YO_MONO` and `DS_DIGI/` are deleted.**  `LED` took YO_MONO's id slot (`0`); the ids are the index
  into `_clockFontStyles[]`, so they must never be renumbered.
- `/fonts.json` and `/clockfonts.json` are built from those two tables by `Display::_buildJsonCache()`,
  which is why the WebUI dropdowns cannot drift from what is compiled in.

**Flash cost, measured, one copy each** (the point of the single defining TU):

| item | bytes |
|---|---|
| `MatrixLight8x6` / `MatrixChunky8x6` | 12,752 each |
| `UnixX11_6x9` (`Fixed`) | 13,696 |
| one clock STYLE (all seven sizes, bitmaps + glyph tables) | 13,404 |

So the two extra system fonts are +13,696 and the three clock styles +40,212: measured deltas of
+41,220 (sh1106, 128x64), +39,032 (es3c28p) and +36,308 (ili9488, which had already been paying for
one 70 px font).  **Adding a font now costs its own size and nothing more** — before the linkage fix
every one of them was in the image twice (three times on the ILI9488 family).

## GFXfont Rendering Pipeline (`src/displays/tools/commongfx.h`, `psframebuffer.h`)

`DspCore::_writeGlyph(uint16_t cp)` is the central glyph renderer replacing the legacy 256-slot glcdfont system. Key
behaviors:

- **Dispatch:** When `gfxFont != NULL && gfxFont != &DisplayFont` (a special clock font is active), delegates to
  `Adafruit_GFX::write()` which uses the font's native `drawChar`. This check now runs **before** `preText()`, so the
  resolver is never asked about a font other than the one that will draw the glyph.
- **Icon rendering:** Codepoints `0x01-0x1F` map to `ICON_TABLE[]`; rendered with `startWrite()`/`endWrite()` wrapping
  (needed for Adafruit SPI TFT drivers).
- **Font glyph rendering:** Similarly wrapped in `startWrite()`/`endWrite()`. The inner loop iterates all `width` bitmap
  columns but only renders columns where `(xOffset + xx) < xAdvance` — prevents glyph bleed beyond the cell boundary
  (fixes colon/digit overlap on SH1106 YO_MONO). Bleed columns have their bits consumed from the bitmap stream but are
  not rendered.
- **Space handler:** Advances cursor by the font's first-glyph `xAdvance` without drawing pixels. Space is handled
  *before* `preText()`, because resolving it would turn the gap into the substitute character.
- **Resolution (`preText`) is a chain, not a single mapping:** keep the glyph when the font has it, else walk
  `preTextFoldStep()` -- which folds within the script first (U+1F00 → U+03B1) and then to an ASCII lookalike (→ `a`) --
  re-testing the font after every step, and substitute `_` only when the chain runs out. One table therefore serves
  fonts of different coverage. The generator's rule is overrides → canonical base → first ASCII of the NFKD form → name
  lookalike, and a non-ASCII base counts as a step only when it can itself progress (otherwise Hangul would claim 11,172
  dead-end Jamo steps).
- **The one-in-one-out contract still holds** (`utf8_strlen(text) * charWidth` is computed on the string as stored), and
  never 0. Codepoints are 32-bit through `preText()`/`glyphAvailable()`/`_writeGlyph()` because four-byte sequences
  would otherwise be truncated into a different glyph; the **result** is always BMP.
- **`preTextString()` resolves a whole string once, at ingress** — `TextWidget::setText`, `ScrollWidget::setText`,
  `NumWidget::setText` and the scroll separator. A scrolling label re-prints its window every scroll step, so this
  removes the repetition and makes measurement and drawing use the same bytes. Invisible codepoints (combining marks,
  zero-width, variation selectors) are dropped there and only there; the per-glyph path substitutes for them instead.
- **Unrenderable codepoints are memoised** in a 128-slot direct-mapped cache keyed on the codepoint *and the font
  pointer*, so a font change invalidates it for free. `preTextInvalidateCache()` is for a font whose data changes in
  place.
- **Forced fold:** `PRETEXT_FOLDACCENT` / `PRETEXT_FOLDCYRILLIC` no longer enable the fallback (it is unconditional);
  they now force the fold even where the glyph exists. `PRETEXT_ALLCAPS` is unchanged in intent.
- **Cursor advance** in the two glyph branches is now defensive only (font swapped mid-frame); the old post-`preText`
  fallback and its `_writeGlyph()` recursion are gone, so `preText()` is the single place that decides what is drawn.
- **`psframebuffer.h`** mirrors the same `_writeGlyph` logic for PSRAM-framebuffer TFT displays — any change to one must
  be applied to both.

Important rendering invariants:
- Never call `startWrite()`/`endWrite()` in `write()` or `writePixel`/`writeFillRect` overrides — SPI nesting causes
  hangs on Adafruit TFT drivers.
- The `gfxFont == NULL` case (YO_MONO / display font) runs through `_writeGlyph` using DisplayFont, NOT the built-in
  glcdfont. This means glyph metrics (yAdvance, yOffset, xAdvance) come from DisplayFont.

## Display Widget Memory Ownership

The widget classes mix two allocation conventions; release must match allocation:

| Member | Owner | Allocated with | Released with |
|---|---|---|---|
| `_text`, `_oldtext` | `TextWidget` (and `NumWidget`, which duplicates it) | `malloc` | `free` |
| `_sep`, `_window` | `ScrollWidget` | `malloc` | `free` |
| `_fb` | `ScrollWidget`, `ClockWidget` | `new psFrameBuffer` | `delete` |
| `_canvas` | `VuWidget` | `new Canvas` | `delete` |
| `_caps` *(removed)* | `VuWidget` | was an in-object array for the spectrum's per-band caps | — the caps were cut on review, so the widget carries no per-band state |

Rules:
- Never `free()` a `new`-allocated object: `free()` skips the destructor and leaks the internal buffer.
- `init()` is re-called on every layout/theme change, so it must release its previous buffers first.
- Resize an existing `psFrameBuffer` with `freeBuffer()` then `begin()`, never by allocating a second one.

### Optional widget guard fields

Each config type has its own field that makes a widget meaningful, and that is what the creation guard in `display.cpp`
must test — never a coordinate, since `{0,0}` is a legitimate position:

| Config type | Fields | Guard field |
|---|---|---|
| `WidgetConfig` | `left`, `top`, `textsize`, `align` | `textsize > 0` |
| `ScrollConfig` | `widget`, `buffsize`, `uppercase`, `width`, ... | `buffsize > 0` |
| `FillConfig` | `widget`, `width`, `height`, `outlined` | `height > 0` |
| `BitrateConfig` | `widget`, `dimension` | `dimension > 0` |

`_fullbitrate` and `_bitrate` are **independent**, not alternatives: the badge is drawn when
`fullbitrateConf.dimension > 0`, the text when `bitrateConf.textsize > 0`, both when a layout fills both, neither
when it fills neither. Neither is ever deleted - `_reinitWidgets()` hides the one a layout does not ask for with
`hideByLayout()`, like every other optional widget - and `Page::removeWidget()` deletes the widget it is handed, so a
caller that removes one and then deletes it too is a **double free**. That pair was the last widget still deleted on
a switch, and it is why switching between a `.bitrateConf` layout and a `.fullbitrateConf` one rebooted the board
(`CORRUPT HEAP` / `multi_heap_free` assert). `BitrateWidget::_draw()` also returns early on `!_active` now: a hidden
badge still receives `setBitrate()`/`setFormat()` on every `DBITRATE`, and its `_draw()` opens with an ungated
`_clear()` that would otherwise paint a background square where the badge would have been.

## VU Widget Rendering (TFT vs OLED, and the runtime style)

`VuWidget` lives in **`src/displays/widgets/widget_vu.h` / `widget_vu.cpp`** — it moved out of `widgets.h`/`widgets.cpp`
because it is now seven draw paths over one box. `widget_vu.h` includes `widgets.h`; **`widgets.h` must not include
`widget_vu.h`** (that is the include cycle) — `display.cpp` includes it explicitly instead. It is compiled for every
display.

`_draw()` is a **dispatcher**: it resolves the area once into `_len`/`_thk`/`_cw`/`_ch`, calls `_levels()`, fills the
whole area with the background, then switches on `config.store.vustyle` and blits once at the end. The styles are
`vuStyle_e` from `widgetsconfig.h` — **seven of them**: Bars (0), Digital LED (1), History (2), Spectrum Reflect (3),
Waveform (4), Lissajous (5), Spectrum Mirror (6). The ids are persisted in `config.store.vustyle` and are the keys in
`/visuals.json`, so they must never be renumbered again (Spectrum A was cut, and the ids shifted, before the first
release) — Spectrum Mirror is id 6 rather than id 4 for exactly that reason, and `/visuals.json` lists it beside
Spectrum Reflect so the two still sit together in the dropdown. `_fillLocal()` is the only helper that knows the pixel
surface:

| | TFT (`DSP_TFT`) | OLED (`DSP_OLED`) |
|---|---|---|
| Buffer | `Canvas *_canvas`, 16-bit (`GFXcanvas16`) | none — the driver owns the framebuffer |
| `init()` | allocates the canvas | no allocation |
| Fills | `_canvas->fillRect` at widget-local coords | `dsp.fillRect` at `_config.left/top` + local coords |
| Transfer | one `startWrite`/`setAddrWindow`/`writePixels`/`endWrite` | none — `DspCore::loop()` calls `display()` |

The shared box fill in `_draw()` is the clear for **every** style, including the tail-clear bar family: every fill and
`_clear()` stops at `len`, so the final (clamped) segment is only clean because the box was cleared first. Do not
reintroduce `drawRGBBitmap` here — the manual blit is deliberate (see the memory-ownership notes above).

**Two kinds of layout, one drawing rule.** The **bar family** (styles 0-1) draws two strips where `bandsConf` says they
are, so `rotateVU`, `align` and `boomboxVU` all matter to it, exactly as they always have. **Every other style** (2-6)
draws into one area, `_cw` x `_ch`: time or frequency runs along its **width**, and the two channels split its
**height** — except Spectrum Mirror, which splits its **width** — with `boomboxVU` ignored. Nothing transposes - the
OLED, the rotated TFT box, the portrait TFT box and the BoomBox ribbon get the same picture. The booleans are read in
`_draw()` for both cases, but only to size the area: `_rotate` picks `_cw = bandsConf.height` with `_ch =
bandsConf.width * 2 + bandsConf.space`, otherwise it is the other way round. `_fillLocal()` is the only pixel helper;
the non-bar painters never touch the per-channel geometry. A first cut transposed the non-bar styles onto the area's
*long* axis, which put the portrait box's spectrum on its side and needed per-family special cases - all of that is
gone.

**The sample styles need audio the VS1053 does not have.** `player.getWaveform(int16_t* out, uint16_t n)` returns `n`
interleaved L/R pairs (oldest first) and `player.getSpectrum(uint8_t* bands, uint8_t n)` returns `2 * n` band values in
0..255 (L's bands then R's), or `false` when there is nothing to give. On the I2S path the audio task keeps a
`VU_CAPTURE_SAMPLES` (512) rolling window, rotated into a published copy on the `f_vu` tick with a sequence counter
around the copy; a reader that sees the sequence move drops the frame rather than drawing a torn trace. On the VS1053
both methods are stubs returning `false`, which is why the spectrum there is synthesised from the level in the widget -
a fact the dropdown no longer states, since the label is the same on every backend. **The transform only exists where it
is used**: `getSpectrum()` is dead code on a VS1053 build, so the linker drops its twiddle table, Hann table and FFT
scratch entirely.

**The spectrum's scale is two factors that are easy to lose.** `getSpectrum()` returns 0..255 where 255 is a full-height
bar, and it gets there as `sqrt(mean power) * (2/N) * (2/32768) * gain`. The first two are the transform's own
normalisation; the third takes the int16 sample range down to 0..1 *and* gives back the Hann window's 0.5 coherent gain;
and `gain` is **the meter's reference**, `clamp(256 / config.vuThreshold, 1, 32)`, so the loudest content the level
meter has seen sits at full scale. That is what makes the I2S spectrum behave like the level bar beside it and like the
simulated source on the VS1053, which is fed that same calibrated level. Dropping the `2/32768` pins every band at 255
and hot — that is exactly what the first hardware build did. The dB window is `VU_SPECTRUM_DB_FLOOR` (60) in
`options.h`. The result is also **held**: a band rises at once and falls over `VU_SPECTRUM_FALL_MS` (400), because one
512-sample window is a noisy estimate - a band swings about 10 dB window to window, and a quiet window dims the whole
display. That is the same hold that fixed a hardware flicker: the seqlock drops a frame whenever the publisher moves
during the staging copy - **7% of frames**, ~1.8 a second, measured - and `getSpectrum()` used to report it as `false`,
which a painter reads as "this backend has no transform" and answers by switching to its SIMULATED source for one
refresh. On a sparse real spectrum that reads as the whole display suddenly filling in. A dropped frame now repeats the
held frame; the strict check is what the waveform needs, where a spliced window is visible.

**Spectrum and trace layout details.** The inter-bar gap is `VU_SPECTRUM_SPACE_PX`, never `bandsConf.space` (that
separates the two *meter strips* — 4 px on one TFT layout, 17 px on another, which left only 7 bars on the second).
Because `barW` is floored, the bar block is **centred** — under the baseline in the stacked Spectrum, and inside each
half in Spectrum Mirror, which is why its two low ends stop short of the divider, Spectrum Mirror reserving `th + 2` px
more there so the line gets daylight on both sides instead of sitting flush against the first bar. And the Lissajous
joins each sample pair to the previous one with a stepped line — one fill per pixel of the longer axis — because
isolated dots read as a scatter rather than as a vectorscope trace.

**The sample styles use the same reference as everything else.** `_waveGain()` is `65536 / config.vuThreshold` in 8.8
fixed point (256 = unity, clamped to 0.25x..32x), applied to the waveform's amplitude and the Lissajous's X and Y, with
the hot colouring measured *after* it. Without it both styles were absolute readings of raw int16: a trace that hugged
the middle of the area and could never reach the hot zone. An uncalibrated meter (threshold 0) maps to unity. The
Lissajous hot test is proportional on whichever axis the beam is furthest out on — a single `min(halfW, halfH)` scale
can never go hot at the left or right edge of a wide box.

**The `[PSRAM]` core-monitor line is the memory diagnostic, and its fields are a contract.** `Used / total: Framebuffer:
X, VU FFT: X, WebUI Cache: X, Audio buffered: X, Contiguous Free: X`. `VU FFT` is `vuPsramBytes` (declared in
`config.h`, defined in `config.cpp`), written by the audio library's scratch allocator, and it reads 0 until a spectrum
frame has been drawn because that allocation is lazy. `Contiguous Free` is
`heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)` — the largest single free block, which is the figure that reveals
fragmentation, since total free can look healthy while the biggest block shrinks. Add new fields at the end rather than
reordering.

**Pacing is adaptive now, not a fixed divisor.** `VuWidget::loop()` times `_draw()` with `micros()`, smooths the cost
(`_drawUs`, a quarter of each sample) and sets `interval = max(VU_REFRESH_MS, cost * VU_DUTY_FACTOR)`, both macros in
`options.h` — at the default factor of 3, a frame spends at most a third of its own interval drawing. `VU_REFRESH_MS` is
the floor and stays the number the level bars care about, so the limiter can only ever slow a style **below** the rate
the audio core publishes levels at. `_redrawMs` (the limiter's timestamp) is deliberately **not** `_lastMs` (the fade
maths' time base): while they were shared, a skipped frame showed up as a doubled `dt` in the fade.
`VU_SAMPLE_REFRESH_DIV` is gone — it was a fixed guess at which styles were heavy and by how much, and it covered only
two of them. On TFT the blit is inside `_draw()` and counted; on OLED the panel flush is paid afterwards by the display
task, so the figure measured there is the widget's own cost. **While the startup services are downloading** the floor is
multiplied by `VU_STARTUP_SERVICES_DIV` (4), so every style redraws a quarter as often for that window — the updater
holds up to three TLS sessions on the network core and the audio stream is usually already up. The gate is
[`startup.servicesBusy()`](src/core/startup.h:19), set by the services task around its own downloads: **not**
`SVC_WILL_RUN`, which also covers the SD-mode park and the 10 s countdown, where the display must stay fast.

**History is the one style a variable redraw rate breaks**, because its x axis *is* time, so `_drawHistory()` advances
by wall clock: it pushes however many columns the elapsed time owes (one per `VU_REFRESH_MS`), keeping the sub-tick
remainder in `_histMs` so the strip does not drift, and a gap longer than the strip fills it in one go rather than
replaying it. It also draws **two rects per column, not four** — the tick at each level is already inside either the
fill (switch off) or the joined run (switch on), so only the first column, which has nothing to join to, needs bare
ticks. The waveform and the Lissajous need neither fix: they plot the whole capture window every frame, so only their
refresh rate moves.

**The draw-cost report is the measuring instrument.** With `WIDGET_DEBUG` defined, `loop()` accumulates frames, fills
and draw time and prints them every 5 s like the Core Monitor: `[VU.Widget] Box 404x7, style 3: 28.1 FPS, draw 1.05ms
avg / 2.31ms peak, interval 33ms, 12.0 fills/frame, free heap 93000`. `fills/frame` counts `_fillLocal()` and
`_drawBand()` calls (not the blit) and is what shows whether a style is drawing rects it does not need; the draw time
shows whether the box is simply too big for the panel. The members and the log are both behind `#ifdef WIDGET_DEBUG`, so
a normal build pays nothing for either. The `WIDGET_DEBUG` line in `options.h`'s `ALL_DEBUG_LOGS` block used to read
`#ifdef WIDGET_DEBUG`, a define that could never take effect; it is now `#ifndef`, matching its siblings.

### Colours on OLED

`config.theme.vumax` / `vumin` are set per driver, not via `dspcolors.h`:

- 1-bit panels (`tools/oledcolorfix.h`): both `TFT_FG`. A single bit cannot express two colours, so level reads from
  geometry alone.
- `OLED_GREYSCALE` with `core/options.h`, default `false` selects the driver's grayscale palette (instead of 1-bit
  mono), and is only valid for `SSD1322` (support removed from`SSD1327`)

`dspcolors.h` is deliberately minimal: only `BOOT_PRG_COLOR`, `BOOT_TXT_COLOR`, `TFT_BG`, `TFT_FG`. Panel-specific
palettes live with their own drivers.

## Screen Rendering Fixes (Session: SH1106 YO_MONO)

### The clock only prints a time the device can stand behind (utility.cpp, widgets.cpp)
- `ClockWidget::_printClock()` is the single function that puts the time on the glass, and it has two doors into it:
  `draw()` (the tick-driven path, which also formats through `_getTime()`) and `_draw()`/`forceDraw()`, which the widget
  base class calls itself from `setActive()` and from the layout show/hide helpers - so `Pager::setPage()` and every
  layout change repaint the clock. Gating `Display::_time()` does NOT cover those: that only handles the tick updates,
  which is why a gate there changed nothing at all.
- `clockTrustworthy()` (`utility.cpp`, declared in `utility.h`) is asked at the top of `_printClock()`. It refuses two
  states. First, nothing has set the time yet: `network.timeinfo.tm_year + 1900 <= 1901` is the zeroed struct, which is
  what the no-RTC radio showed as `00:00` until the first `doSync()` (1901 rather than a modern floor, so a device that
  never syncs can never cross it). Second, an RTC that disagrees with an established system clock by more than a minute,
  or in the year - because `RTC::getTime()`/`setTime()` carry whatever zone the chip happens to hold, so a chip set by
  another tool prints a plausible-looking UTC, right year and right minute, which no plausibility test can catch.
- **The comparison needs an established system clock, and that is a separate condition** (`now > 31536000`, one year of
  epoch - far above any uptime, far below any real date). `localtime()` only becomes local once `configTzTime()` has
  run, which happens at the WiFi-connect event, so before that a UTC chip agrees with `localtime()` perfectly and gets
  printed as local. That was the RTC radio's remaining UTC step after the first attempt at this gate. With the clock
  unset the chip is trusted only when `network.status == SDOFFLINE` (the chip is the only source there will ever be);
  online it is blocked until SNTP and `doSync()` have spoken.
- The first genuine paint needs no help from the gate: `ticks()` (RTC) and `doSync()` (network) both request `CLOCK` the
  moment they have something to show, and `_getTime()` reports a change because `_timebuffer` starts at `"00:00"`.

### ClockWidget colon blink (widgets.cpp)
- The seconds-area `fillRect` at line 813 is guarded by `if (Clock_GFXfontPtr != NULL)` — its Y-position formula
  (`_top() - _timeheight + _space`) is only correct for special clock fonts; YO_MONO renders at `_top()` directly and
  the fillRect would clear into title rows.

### Screensaver clock boundaries (display.cpp, widgets.h)
- `minTop = max(TFT_FRAMEWDT, _clock->timeHeight())` — for framebuffer displays, `_config.top - _timeheight` must stay
  >= 0 to avoid framebuffer clipping above the display.
- `maxTop = dsp.height() - clockH - TFT_FRAMEWDT` — was missing the `-clockH` term, allowing the clock to render
  partially off-screen.
- Movement interval now uses `% SCREENSAVERMOVE` (default 5 seconds) instead of hardcoded `% 60`.
- `ClockWidget::timeHeight()` accessor added to expose `_timeheight`.

### NumWidget volume-digit clearing (widgets.cpp)
- YO_MONO clear height: `realth = _textheight * CHARHEIGHT` (was OLED-only; now applies to all displays with
  `Clock_GFXfontPtr == NULL`).
- Special-font clear height: `realth = _textHeight() + 1` — uses the font's actual glyph height + 1px margin, matching
  `_clearClock()`'s pattern.

### Font directory renamed
- `src/displays/ehfonts/` → `src/displays/clockfonts/`
- `YO_CLASSIC` removed as a clock font option (variable-width variant of YO_MONO that broke layout assumptions).

---

## Screen Rendering Fixes (Session: VU Rotated Layout)

- **New layout flag** `LayoutData::rotateVU` (exposed via `rotateVU_ptr`), treated exactly like `boomboxStyle` — absent
  means false. `VuWidget::_rotate` is read from `rotateVU_ptr` in `init()`.
- **Layout ordering** in `displayTFT480x320conf.h`: `_layoutNames` is now `Default`, `Default (VU Rotated)`, `BoomBox
  (VaraiTamas)`. The rotated layout is layout #2 (`bandsConf = { 32, 130, 4, 2, 10, 3 }`, `.rotateVU = true`); BoomBox
  moved to #3.
- **Blit choice**: `VuWidget::_draw()` uses the manual `startWrite()` / `setAddrWindow()` / `writePixels()` /
  `endWrite()` sequence for all three modes. `drawRGBBitmap()` was deliberately removed from the widget layer — the
  manual path depends only on `setAddrWindow` and `writePixels`, which every TFT driver is guaranteed to implement, and
  it issues a single bulk transfer rather than one `writePixels` call per scanline. Do not switch this back.
- **Direction**: the rotated VU fills left-to-right with `_vumaxcolor` at the right end.

## CPU Core Assignments & Stack Sizes

The ESP32 has two hardware cores: **Core 0** (PRO_CPU) and **Core 1** (APP_CPU). The ESP32 Arduino framework runs
`setup()` and `loop()` on Core 1. Audio decoding is isolated on Core 0; all other application tasks run on Core 1.

### Compile-time Core Macros (`src/core/options.h`)

Two macros control core assignment across the codebase:

| Macro | Default | Valid override | Purpose |
|---|---|---|---|
| `AUDIO_CORE` | `0` | `1` | Core for audio decode task |
| `NETWORK_CORE` | `1` | `0` | Core for netserver and all network/utility tasks |
| `DSP_TASK_CORE_ID` | `1` | `0` | Core for the display loop task (independent of `NETWORK_CORE`) |

On single-core ESP32-C3 (`CONFIG_FREERTOS_UNICORE`), all three macros are forced to `0` automatically; defining any of
them manually on a unicore build is a compile-time `#error`.

`NETWORK_CORE` is derived in **`options_overrides.h`** as well as here - a deliberate duplicate (the one place in this
codebase where the same rule lives twice, because the value must reach AsyncTCP, a translation unit that never sees
`options.h`). That header is force-included and compiled first, so it owns the value and the copy here is a no-op in
those builds; in builds without the force-include (the other `builds/*` templates) the copy here is what defines it.
Both copies are written to survive being evaluated twice: the dual-core branch is `#ifndef NETWORK_CORE`, and the
unicore `#error` fires only when the value it finds is not 0, so it cannot mistake our own definition for one a user
set. `CONFIG_ASYNC_TCP_RUNNING_CORE` defaults to `NETWORK_CORE` so the AsyncTCP internal event task follows
automatically, and both are deliberately overridable in `myoptions.h`.

The health of the force-include channel is observable: the Done-button panic in the SD manager (`LoadProhibited` on core
1) has not recurred since the override landed, although that is a single observation - correlation, not a proven
mechanism. The unpinned `async_tcp` task floating onto core 1 during a mode switch is the plausible link.

### Board Stack Multiplier (`STACK_MULTIPLIER`)

A board-aware multiplier scales all five user-configurable FreeRTOS task stacks automatically:

| Board | `STACK_MULTIPLIER` | Effect |
|---|---|---|
| ESP32-S3 | 2 | All base stack sizes doubled |
| ESP32 | 1 | Base sizes unchanged |
| ESP32-C3 | 1 | Base sizes unchanged (C3 has *less* RAM than base ESP32) |

- Defined automatically after the board guard in `src/core/options.h`. Override in `myoptions.h` with `#define
  STACK_MULTIPLIER 1` or `2` if needed.
- Only values `1` and `2` are accepted — a compile-time `#error` fires otherwise.
- Per-task manual overrides (e.g. `#define DSP_TASK_STACK_SIZE 6`) bypass the multiplier; a `#elif` range guard
  validates the manually-set value.
- The multiplier applies **only** to the five configurable task stacks. Fixed-stack tasks (HTTPS workers, OTA) are
  unaffected.
- `SEARCHRESULTS_BUFFER` and `CONFIG_ASYNC_TCP_QUEUE_SIZE` use separate per-board explicit values (not
  `STACK_MULTIPLIER`) since they scale differently.

### Core 0 — Audio

#### `src/libraries/I2S_Audio/Audio.cpp` + `src/libraries/VS1053_Audio/audioVS1053Ex.cpp`
- Both audio libraries pin their `PeriodicTask` (audio decode loop) to `m_audioTaskCoreId`.
- `src/core/player.cpp` `Player::init()` calls `setAudioTaskCore(AUDIO_CORE)` to set this explicitly (defaults to Core
  0).

### Core 1 — Everything Else

#### `src/core/display.cpp`
- `loopDspTask` ("DspTask") is pinned to `DSP_TASK_CORE_ID` (default `1`) via `xTaskCreatePinnedToCore`.
- This task calls `display.loop()` only. `netserver.loop()` was moved to its own dedicated task (see `netserverLoopTask`
  below).

#### `src/core/network.cpp`
- `doSync` (time/weather sync task) is pinned to `NETWORK_CORE`.
- `searchWiFi` (WiFi connection/retry loop) is pinned to `NETWORK_CORE`.
- `retryStreamConnection` (post-disconnect reconnect) is pinned to `NETWORK_CORE`.


#### `src/core/netserver.cpp` — `netserverLoopTask` + all utility tasks pinned to `NETWORK_CORE`
- `netserverLoopTask` (started by `NetServer::startLoopTask()`, called from `main.cpp` after each `netserver.begin()`)
  is pinned to `NETWORK_CORE`. It is the sole caller of `netserver.loop()`.
- All formerly scheduler-assigned (`xTaskCreate`) utility tasks are explicitly pinned to `NETWORK_CORE` via
  `xTaskCreatePinnedToCore`:
  `vTaskSearchRadioBrowser`, playback task (lambda), radio-browser click task (lambda), `checkForOnlineUpdateTask`
  (lambda), `startOnlineUpdateTask` (lambda)

#### `src/core/startup.cpp`, `src/core/commandhandler.cpp` — pinned to `NETWORK_CORE`
- `src/core/startup.cpp`: `startupServicesAsync`
- `src/core/commandhandler.cpp`: `vTaskFetchCuratedIndex`, `vTaskFetchCuratedPlaylist`

#### Arduino `loop()` — implicit Core 1
- All calls from `src/main.cpp` `loop()` run on Core 1: `telnet.loop()`, `battery.loop()`, `player.loop()`,
  `controls.loop()`.
- `netserver.loop()` is **not** called from `loop()` or from DspTask; it runs exclusively in `netserverLoopTask` pinned
  to `NETWORK_CORE`.

### FreeRTOS Task Reference

Stack sizes and priorities are controlled by macros in `src/core/options.h` (`/* Tweaks for Core Processes */`). Higher
priority = more CPU; Arduino `loop()` runs at priority 1. Priority 0 is idle-level (starved) and is never used. Per-task
local conversion macros (`_BYTES`) are defined at the top of each `.cpp` file (except `SET_LOOP_TASK_STACK_SIZE` which
is an `Arduino.h` macro). Stack defaults scale with `STACK_MULTIPLIER` — values shown as ESP32/C3 (1x) / S3 (2x).

| Task | File | Stack macro (default ESP32 / S3) | Priority macro (default) | Notes |
|---|---|---|---|---|
| `loopTask` | main.cpp | `LOOP_TASK_STACK_SIZE` KB (8 / 16) | 1 (framework) | Arduino loop(); `SET_LOOP_TASK_STACK_SIZE()` applies at boot |
| `DspTask` | display.cpp | `DSP_TASK_STACK_SIZE` KB (4 / 8) | `DSP_TASK_PRIORITY` (2) | — |
| `netserverLoopTask` | netserver.cpp | `NETSERVER_TASK_STACK_SIZE` KB (4 / 8) | `NETSERVER_TASK_PRIORITY` (2) | — |
| `doSync` | network.cpp | `NETWORK_TASK_STACK_SIZE` KB (4 / 8) | `LOW_TASK_PRIORITY` (1) | Time/weather sync |
| `searchWiFi` ×2 | network.cpp | `NETWORK_TASK_STACK_SIZE` KB (4 / 8) | `NET_TASK_PRIORITY` (3) | — |
| `retryStreamConnection` | network.cpp | `NETWORK_TASK_STACK_SIZE` KB (4 / 8) | `NET_TASK_PRIORITY` (3) | Post-disconnect reconnect |
| `retryStreamConnection` | player.cpp | `NETWORK_TASK_STACK_SIZE` KB (4 / 8) | `NET_TASK_PRIORITY` (3) | Stream drop reconnect; was hardcoded to Core 0 (bug) |
| `vTaskFetchCuratedIndex/Playlist` | commandhandler.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| `vTaskSearchRadioBrowser` | netserver.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| `playbackTask` (lambda) | netserver.cpp | 4096 http / 8192 https | `PLAYBACK_TASK_PRIORITY` (3) | Dynamic stack based on URL scheme |
| `rbClickTask` (lambda) | netserver.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| `checkForOnlineUpdateTask` (lambda) | netserver.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| `startOnlineUpdateTask` (lambda) | netserver.cpp | 16384 fixed | `NET_TASK_PRIORITY` (3) | OTA — stack hardcoded |
| `startupServicesAsync` | startup.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| MQTT client task (library-owned) | PsychicMqttClient | 6144 (library default) | `NET_TASK_PRIORITY` (3) | Plain TCP; `Mqtt::loop()` runs on `netserverLoopTask`, not here. Apply is held while the startup downloads are running |

### CORE_MONITOR debug feature (opt-in)

Enabled by `ALL_DEBUG_LOGS`, or directly with `#define CORE_MONITOR` in `myoptions.h` — `options.h` only defines it
inside the `ALL_DEBUG_LOGS` block. The counters and helpers in `src/displays/tools/dspstats.h` compile to nothing when
it is not defined, so a build without the monitor pays nothing for them.

Emits to serial + telnet every 5 seconds:

- **Task rates**: `DspTask(core1) loops/s: 77 (12.99ms/loop), Main(core1) loops/s: 824 (1.21ms/loop), Max Main Loop
  Time: 12.580ms, Free Heap: 139880`
  - The two figures are per **task**, not per core. Field 1 is the display task (`cmDspLoopCount`, incremented in
    `loopDspTask`), field 2 is the Arduino `loop()` (`cmMainCount`). Each is labelled with the core it actually runs on:
    the display task reports its own core via `cmDspCore`, the main loop via `xPortGetCoreID()`. **Do not read field 1
    as "core 0"** — in the common configuration both tasks are pinned to core 1, so the old literal `Core0`/`Core1`
    prefixes attached core 0's subsystem names to the display task's counter and a display-task change looked like a
    core-0 regression.
  - The display task sleeps `DSP_TASK_DELAY` (10 ms) after every iteration, so its period is work + 10 ms and its
    ceiling is ~100 loops/s: a figure at the ceiling means the display was idle, below it means it was drawing.
  - Rates are per **measured** second (the `millis()` delta between prints, which includes the time the printing itself
    takes). Dividing by a hardcoded 5 inflated every figure by 5-7% - that is what made a display task on its 10 ms
    floor report 107 loops/s.
- **Display work**: `DspTask work/s: glyphs 1830, fills 240, preText 1830 (hit 99%), fb flushes 77 (1.0/loop)`
  - `glyphs` is counted at the top of `_writeGlyph()` in both `commongfx.h` and `psframebuffer.h`. `fills` is counted in
    `DspCore::writeFillRect()` after its clip test, so it is the class-agnostic "how much rectangle drawing reached the
    panel" figure - every `dsp.fillRect()` on either class passes through it.
  - `preText` calls and hits live in `pretext.cpp`; a hit is an answer that did not need the fold chain. On real station
    names the hit rate sits at **100%**, because the display font carries every codepoint they use: the fold table is a
    safety net, not a hot path.
  - `fb flushes` counts OLED `display()` calls and TFT **PSRAM region blits only**. On a TFT build only `ScrollWidget`
    and `ClockWidget` draw through a `psFrameBuffer`; every other widget writes straight to the driver over SPI, so this
    is not "the panel was written" and it can legitimately read 0 while the screen is visibly updating - scrolling the
    playlist is exactly that case.
  - `glyphs` exceeds `preText` calls by the number of spaces and control codepoints, which `preText()` answers before it
    starts counting.
- **Core layout**: `Core layout: core0 (Audio+Net+TCP), core1 (Main+Disp)` — built at compile time from the `CORE_0` /
  `CORE_1` string macros in `options.h`, which concatenate `+Audio`, `+Net`, `+TCP` and `+Disp` according to where
  `AUDIO_CORE`, `NETWORK_CORE`, `CONFIG_ASYNC_TCP_RUNNING_CORE` and `DSP_TASK_CORE_ID` are assigned. Only defined on a
  dual-core build.
- **Stack high water marks**: `Main`, `Display`, `Netserver` minima, never reset between windows.
- **LittleFS, Heap and PSRAM lines**, rate-limited by `CORE_MONITOR_ETC_LOOPS` (default 5 cycles = 25 s).

- **The monitor does not measure audio.** It counts the display task and the Arduino `loop()`; on the common builds both
  are pinned to core 1, so a `Core layout: core0 (Audio)` line means core 0 carries no counter at all and its load has
  to be read from the `[PSRAM] Audio buffered` figure instead.
- **Each group ends with a blank line** via `SERIALLOGLF()` - a prefix-free `serialLog("%s", "")` in `core/logging.cpp`,
  so serial and telnet both get a bare CRLF. That keeps consecutive reports separable, including when the LittleFS, Heap
  and PSRAM lines print alongside them.

Implementation:
- `src/displays/tools/dspstats.h` — counter declarations plus the increment helpers, which are no-ops when
  `CORE_MONITOR` is undefined
- `src/core/display.cpp` — defines the counters beside `cmDspLoopCount`; the display task reports its own core on every
  iteration
- `src/main.cpp` — reads and resets them, times the main loop with `micros()`, and prints the lines above plus the
  trailing blank line

---

## Hardware-Specific Notes (High-Risk Paths)

This section calls out hardware implementations that diverge from the common code path and are more likely to regress.

## Audio backend variants (`src/core/player.cpp` + `src/libraries/*`)
- **The I2S audio task's stack lives in the library header, not in `options.h`**: `AUDIO_STACK_SIZE` in
  `I2S_Audio/Audio.h` is a plain `static const size_t` (3500 words since it was raised from 3300 for a station that
  overflowed it - `MPEG-2.5, Layer I` on `echoesofbluemars.org`, panic shape `Stack canary watchpoint triggered
  (PeriodicTask)` on core 0). It is not scaled by `STACK_MULTIPLIER` and cannot be tuned from `myoptions.h`; the
  header has to be edited. The full write-up is in `src/libraries/i2s-frankenstein-surgery-notes.md`, Addendum 2 -
  including why it was *not* the task watchdog and *not* the AsyncTCP priority or core. The VS1053 library keeps its
  own `AUDIO_STACK_SIZE` (`audioVS1053Ex.h`); it was raised to the same 3500 for parity, which is alignment rather
  than a fix, since that backend hands the bitstream to a hardware decoder and never reaches the software decoder's
  depth (`vs1053-frankenstein-surgery-notes.md`, "Stack Size").
- Two major audio stacks are used depending on macros/hardware:
  - I2S audio library path
  - VS1053 external decoder path
- `options.h` enforces that both are not enabled together.
- Risk notes:
  - behavior differences between backends (metadata timing, codec handling, volume behavior) can create env-specific
    bugs.
  - some callback/metadata handling is shared while low-level decoder behavior is not.

## Display driver families (`src/displays/display*.cpp/.h`)
- Most drivers implement similar APIs but capability differences exist:
  - sleep/wake/invert support varies
  - color depth and text rendering differ
  - touch coupling only exists for certain panel combinations
- Risk notes:
  - UI assumptions tested on one controller may fail on another due to geometry, fonts, or refresh behavior.
  - conf-layout headers can hide clipping/overlap issues until specific display targets are built.

## Touch controllers (`src/core/touchscreen.cpp`)
- Multiple controller backends (XPT2046, GT911, FT6336) with shared gesture mapping.
- Risk notes:
  - orientation/flip and calibration behavior can diverge by controller.
  - long-press/swipe thresholds can feel different across hardware even with same app logic.

---

## Custom Libraries (`src/libraries`)

These are **not** third-party packages installable via PlatformIO's registry. They are custom or heavily-modified
libraries embedded directly in the repository, mostly inherited from yoRadio and extended for ehRadio. Consult
`src/libraries/libraries-note.md` for the origin, upstream source, and modification status of each library.

### Display driver libraries
- `Adafruit_GC9106Ex/` — GC9106 TFT driver (not a real Adafruit library; adapted from prenticedavid)
- `Adafruit_ST7796S/` — ST7796S TFT driver (same origin)
- `ILI9225Fix/` — ILI9225 TFT driver (heavily modified)
- `ILI9488/` — ILI9486/ILI9488 SPI driver (modified from ZinggJM)
- `SSD1322/` — SSD1322 OLED driver (slightly modified from JamesHagerman)

### Audio decoder libraries
- `I2S_Audio/` — software I2S audio decoder (adapted from schreibfaul1/ESP32-audioI2S via Maleksm's yoRadio mod). PSRAM
  buffer size now configurable via `PSRAM_BUFSIZE` macro.
- `VS1053_Audio/` — VS1053 hardware decoder driver (adapted from schreibfaul1/ESP32-vs1053_ext via Maleksm's yoRadio
  mod). PSRAM buffer size now configurable via `PSRAM_BUFSIZE` macro. `stopSong()` SM_CANCEL sequence now guarded by
  `if(m_f_running)` — prevents permanently stuck CANCEL bit when stop is called during init with no song playing.
  `VS_PATCH_ENABLE` forced `false` on this hardware — FLAC patches produce audio silence on this VS1053 variant.
- `ES8311_Audio/` — ES8311 codec driver (written for ehRadio by kasperaitis)

### Touchscreen library
- `FT6336_Touchscreen/` — FT6336 capacitive touch driver (written for ehRadio by kasperaitis)

### Logging integration in custom libraries
- Selected library-level diagnostic prints now use centralized logging macros for consistency with core logs:
  - `VS1053_Audio/audioVS1053Ex.cpp` (VU meter status/error)
  - `ES8311_Audio/es8311.cpp` (register dump helper)
  - `FT6336_Touchscreen/FT6336.cpp` (startup probe log)
  - `ILI9225Fix/TFT_22_ILI9225Fix.cpp` (`DEBUG` macro print path)

### Include conventions in library files
- Library `.cpp` files that reference project defines begin with `#include "../../core/options.h"` as the **first line**
  (before any `#if` guard), then gate all remaining includes and code behind the relevant `#if` condition (e.g., `#if
  DSP_MODEL==DSP_ILI9488`, `#if defined(USE_AUDIO_I2S) || defined(USE_AUDIO_ESP32_DAC)`, `#if
  defined(USE_AUDIO_VS1053)`). This pattern is acceptable and intentional.
- Library `.h` files do not include `options.h`; they are self-contained and guarded with `#ifndef`/`#pragma once`.

---

## Locale and Translation Map (`src/locale`)

## `src/core/locale.h` (selector)
- Thin include wrapper: includes `dsplocale.h` and defines `WEBUI_LOCALE` from `DSP_LOCALE` if not overridden.
- `_activeLocale` runtime index (set from `config.store.locale_display`), `l10n()` / `l10n_dow()` / `l10n_month()` /
  `l10n_wind()` helpers.
- `l10n_findLocale()` resolves locale code to array index at runtime.

## Display locale files (`src/locale/display/*.json`)
- 36 JSON source files (one per locale), compiled via `make_dsplocale.py` into `dsplocale.h` PROGMEM mega-header.
- Master key set defined by `en_US.json` (67 keys: days, months, wind, weather, status labels).
- `static_assert` validates `DSP_LOCALE` at compile time against known locale codes.
- `dsplocale_index` PROGMEM string served at `/dsplocale.json` for WebUI dropdown.

## WebUI locale files (`src/locale/www/*.json`)
- 50 JSON source files, compiled via `make_wwwlocale.py` into `wwwlocale.h` PROGMEM (gzip-compressed byte arrays).
- Served from PROGMEM at `/locale.json` (with `Content-Encoding: gzip`) and `/wwwlocale.json` (index).
- **English is served like every other locale.** `handleDynamicLocale()` used to answer 404 when the requested code
  matched `HARDCODED_WEBUI_LOCALE` ("en_US"), on the reasoning that the HTML already holds that text - which cost a
  failed load and a console warning in devtools and nothing else. Both the check and the define are gone:
  `src/locale/www/en_US.json` is the master `www_tool.py` diffs the other locales against, and the pages are its
  source, so the HTML is English and there is no build-time or runtime switch that makes it anything else.
- No LittleFS files needed — all locale data is compile-time embedded. **Nothing is deployed into `data/www` for a
  locale, and nothing is cleaned up afterwards.** `builds/platformio_pre_gzip_www.py` used to scan `myoptions.h` and
  `CPPDEFINES` for `WEBUI_LANGUAGE_xx_XX` / `DSP_LANGUAGE_xx_XX` and copy `src/locale/webui/{code}.json` - a directory
  that does not exist - into `data/www`, and `builds/platformio_post_gzip_www.py` then deleted `xx_XX.json` again
  after packaging. Both halves are gone, and the `_LANGUAGE_` macros were never read by the firmware anyway: it
  selects on `DSP_LOCALE` and `WEBUI_LOCALE`, so that removed build step was their only reader in the repo.

## Locale build tools
- `src/locale/make_dsplocale.py`: validates display JSONs, generates `dsplocale.h` with PROGMEM string tables + enum.
- `src/locale/make_wwwlocale.py`: validates webui JSONs, gzip-compresses into `wwwlocale.h` PROGMEM byte arrays.
- `src/locale/hardcode_locale_to_webui.py` (removed): it rewrote the `data/www` files in another language for the
  `HARDCODED_WEBUI_LOCALE` build, so it went with that define.

## Locale maintenance tools
- `src/locale/www_tool.py` (was `scan_www_check_json.py`): scan HTML/JS for i18n keys, check/add/translate/sort www
  locale JSONs.
  - **`--merge <file.json>` (added): upsert a partial locale JSON into one locale.** `www_tool.py de_DE --merge
    changes.json [--clean] [--sort]`. One locale only - `*`, the prompt modes (`--fast/--every/--diff/--ndiff`) and
    `--translate` are each refused with their own message. A merge never prompts, which includes the translation-service
    question: it is skipped rather than asked before the merge starts. The partial may carry any subset of keys;
    `locale_code` in it is ignored (the filename is the authority) while `locale`/`locale_en` merge normally. The run
    reports added/updated/unchanged counts, then how many master keys are missing from the file and how many are present
    but empty - the list a translator still owes.
  - **A key the master does not know is skipped and named, and never written** (exit 0, the rest of the partial merges
    as usual). The master key set is the `data/www` scan for www, the master JSON for display. The report is `Ignored: N
    key(s) in <file> are not in the master key set`, and the message names the three readings: the name is wrong, the
    key is old, or it is new and belongs in the master first. The first version refused the whole file and wrote
    nothing, which was wrong about the common case: a translator working from an older copy hands back keys that have
    since been retired, and the rest of their work should not be lost over that. What stays a hard rule is that such a
    key is never written, because `make_dsplocale.py` treats an extra key exactly as it treats a missing one, as a build
    error.
  - **`--newkeys`/`-k [FILE]` (added): write the keys the locale(s) lack into a template** - key -> master text, for a
    translator to fill in and hand back for `--merge`. **It is a pre-pass, not an alternative to the normal run**: with
    a mode given it collects first, while the keys are still missing, and the pass then fills the locales - so `*
    --translate --fast --clean --sort --newkeys` yields both the filled files and the list to send out. With no mode it
    collects and stops, because the pass would start prompting. It composes with `*` and with `--clean`/`--sort` (which
    apply to the template), and refuses only `--merge`, being the opposite direction. `*` unions the missing keys across
    every locale and writes **once**, because a key absent from one locale is almost always absent from the rest. An
    existing template is never rebuilt: keys it already has keep their values, which may be someone's work in progress,
    and only new keys are appended; `--clean` then means "drop template keys the master no longer has".
    - **The template lives beside the tools, not in the locale folders** (`src/locale/www_newkeys.json`,
      `src/locale/display_newkeys.json`, overridable with `-k FILE`). Both generators glob their own folder as locale
      files - `make_dsplocale.py` validates `locale_code` and the key set, `make_wwwlocale.py` requires
      `locale`/`locale_en` - so a template dropped into `www/` or `display/` would fail validation and be embedded in
      `wwwlocale.h` as a bogus locale. Verified after the fact: `make_wwwlocale.py --verify` still counts 50 files with
      both templates present.
  - **How it collects keys, and the rule that follows from it.** It matches `data-i18n` attributes (patterns 1a-1i:
    element text, `placeholder`, `value`, `title`, `alt`) and literal `t('key', 'default')` calls (patterns 2 and 3),
    across `data/www/*.html`, `data/www/*.js` and `netserver.h` (for `emptyfs_html`). **Never wrap `t()` in a helper
    function, and never keep keys in a table of bare strings.** `tr('key', 'text')` does not match `\bt\(` - the
    character after the `t` is `r` - and `{ 'code': 'err_key' }` is not a call at all, so both forms are invisible to
    the scanner, and a `--clean` run then deletes those keys from every locale as unused. There is nothing to guard
    against either: `locale.js` is compiled in from `locale_js.h` and cannot fail to load, which is the only reason a
    wrapper was ever wanted. Where a message is selected at runtime each arm must still be a real `t(...)` call -
    `errText()` in `data/www/sdmanager.html` is a `switch` of literal calls for exactly this reason.
  - **The unchanged-translation prompt differs between the automatic pass and the interactive one.**
    `confirm_source_text_use()` asks before writing the source text when the service failed or returned it unchanged. In
    `--fast` it offers `y` / `a` (always for this key, held in the module-level `_source_text_always` set for the rest
    of the run - the same key is asked once per locale under `*`) / `n`, and **`n` ends the run** with a message naming
    both causes and saying that finished locales are saved and the one in progress is not. `prompt_for_key()` calls it
    with `auto_pass` left false and keeps plain `y/n`: there `n` means "not this key" (keep the JSON, or skip it in the
    missing mode), and `y` hands the key to the normal edit prompt anyway.
    - **`a` suppresses the question, never the attempt.** `translate_text()` runs first for every key, and the set is
      consulted only when the result was empty or identical to the source text, so a key settled with `a` still receives
      a real translation in any later locale that produces one. The set is per key, not per run: a service failing
      everywhere still asks once per key, and `n` - not `a` - is the answer to that.
  - Run it as `py src/locale/www_tool.py * --fast` to add every newly found key to all 50 locales using the master text;
    `--clean` and `--sort` are the same pass with the tidy-ups. On Windows it fails with a `cp949` encoding error when
    it reaches the non-Latin locales unless `PYTHONIOENCODING=utf-8` is set - `display_tool.py` does this for itself and
    this one does not.
  - **The two ways this collector invents keys from prose or from concatenation.** (1) Never write the call form out in
    a comment, even when the comment is explaining the scanner itself: pattern 2 cannot tell prose from code, and the
    header comment of `data/www/sdmanager.html` plus the comment above its `errText()` switch each produced a bogus
    `key` entry that then spread to all 50 locales. (2) Do not put `data-i18n` on a tag whose markup is assembled by
    string concatenation: pattern 1a reads from the attribute to the next `>`, then the element text to the next `<`,
    and in `'... data-i18n="k" title="' + esc(t('k','text')) + '">' + SVG + '</span>'` the next `>` is the one closing
    the attribute - so the tool recorded the following concatenation fragment (`' + SVG_UP + '`) as that key's source
    text in every locale. Build such tooltips with `t()` as the row is rendered and leave `data-i18n` off. Check any
    page after editing it with
    `python -c "import sys;sys.path.insert(0,'src/locale');import www_tool as
    w;print(w.extract_keys_from_html_js('data/www/<page>.html'))"`.
- `src/locale/display_tool.py` (NEW): manage display JSONs against master. Sort uses master key order (never
  alphabetizes). Clean never touches master. Supports `--merge <file.json>` and `--newkeys`/`-k [FILE]` with the same
  upsert, unknown-key-skip and template rules as `www_tool.py`, except that the master key set comes from the master
  JSON and the sort uses master key order.
- **`--create` was removed from both tools.** It laid down a locale file holding every key with an empty value, which
  turned out to be useless: a translator starts from a real file - the master, or a language they can read - and copies
  it. The option, the `auto_create` plumbing and `create_locale_from_master()`/`create_locale_file_from_master()` are
  gone, so both tools now only ever edit a locale that already exists, and a merge into a missing file says so and names
  the fix.
- **`--key NAME` was added to both tools: redo exactly one key across every locale.** The name is looked up in that
  tool's own master - the `data/www` scan for `www_tool.py`, the master JSON for `display_tool.py` - once, before any
  locale file is opened, so a typo exits 1 with a single message instead of one per locale in a `*` run. Both `missing`
  and `fast` normally skip a key the locale already has, which is exactly the case the option exists for, so it writes
  the key where it exists as well as where it does not; it is orthogonal to the modes and to
  `--translate`/`--clean`/`--sort`, so `* --translate --fast --clean --sort --key X` re-translates one key everywhere.
  In the interactive modes the key is still prompted when a value exists, but through the `all` prompt layout, so the
  current text appears as `[JSON]` and ESC keeps it rather than letting the source text replace it unseen. Refused, with
  its own message: `--merge` (it writes a whole partial file, so there is nothing to select) and `--newkeys` (the
  opposite direction - it collects the keys the locales lack). One key per run; a sweep for many is still `--ndiff`.
  Verified on a scratch copy of `fr_FR.json` (deleted afterwards): the pass wrote 1 key and `fc` against the original
  showed exactly one differing line.
- `src/locale/trans_deepl.py` (was `scan_trans_deepl.py`): DeepL translation module. Uses `trans_*.key` discovery
  pattern (was `scan_trans_*.key`).
- `src/locale/trans_deepl.md` (was `scan_trans_deepl.md`): DeepL setup + usage docs.

---

## WebUI <-> `config.store` Integration Playbook (Critical Section)

This section is specifically for adding/removing settings and avoiding missed linkage points.

## When adding a new runtime setting field

1. Add macro default in `src/core/options.h` (and optionally override in `myoptions.h`).
2. Add field in `config_t` in `src/core/config.h`.
3. Add key mapping in `Config::keyMap` in `src/core/config.cpp`.
4. Add reset behavior in `Config::defaultSettings(...)` branch (the right group).
5. Add getter payload in `netserver.processQueue()`:
   - whichever `GET*` JSON block should include it (`GETSYSTEM`, `GETSCREEN`, etc.).
6. Add command handling in `src/core/commandhandler.cpp`:
   - parse command key
   - persist with `saveValue(...)`
   - trigger display/network side effects and request updates as needed.
7. Add WebUI wiring:
   - element in `data/www/options.html` with id and `data-command`.
   - fallback label text + `data-i18n` key.
   - add i18n key in `src/locale/www/en_US.json` (and optionally others).
8. Ensure websocket UI apply path exists in `data/www/script.js`:
   - `setupElement(...)` supports element type/id.
   - incoming `GET*` payload key matches DOM element id or custom handler.
9. If setting is locale/time/weather related, update `data/www/options.js` apply handlers too.
10. Telnet command handling is thin-dispatch by default: update `src/core/commandhandler.cpp` first, and only extend
    `src/core/telnet.cpp` if protocol normalization needs a new alias/form.
11. If setting affects startup behavior, check `main.cpp`, `config.init()`, and `startup.startupServices()`.
12. Update this `code-summary.md`.
13. Update `Commands.md`: the entry goes in the same block and at the same position as its `cmdIs(...)`
    branch, because the file's whole value is that walking it and this router together is mechanical. If
    the command is refused for HTTP/MQTT/Telnet, that comes from `isBlockedForSource()` and the doc's
    *Blocked* column has to match it - nothing else is authoritative for that column.

## When removing a setting field

1. Remove/disable command usage in `commandhandler.cpp`.
2. Remove from `GET*` payload in `netserver.cpp`.
3. Remove UI controls and JS references.
4. Remove from `config_t` + `keyMap`.
5. Add removed key to `Config::deleteOldKeys()` if old persisted value should be cleaned.
6. Remove locale keys from `src/locale/www/en_US.json` (and regenerate/check).
7. Check telnet/mqtt code paths for orphan logic.
8. Update this file.

## Why changes are often missed

Frequent miss points:
- `Config::defaultSettings(...)` sections
- `Config::keyMap` update
- `GET*` outbound payloads in `netserver`
- DOM id mismatch vs websocket payload key
- locale keys missing from `en_US.json` and `data-i18n`
- telnet parity for advanced settings

---

## Cross-Link Matrices

## Matrix A: Settings request/response paths

- Browser asks for settings:
  - `script.js` sends `getsystem=1` etc.
  - `commandhandler.cpp` -> `netserver.requestOnChange(GETSYSTEM, cid)`
  - `netserver.cpp` builds JSON from `config.store`
  - `script.js` maps keys to DOM by id

- Browser applies settings:
  - UI emits `key=value` over websocket
  - `netserver.onWsMessage()` -> `cmd.exec()`
  - `cmd.exec()` updates `config.store` and side effects
  - server emits follow-up updates where needed

## Matrix B: Same behavior surface via different channels

- WebUI: `commandhandler.cpp`
- HTTP URL params: `netserver.cpp` `handleIndex()` multi-param loop -> `commandhandler.cpp` (with source blocklist)
- Telnet/serial: normalized parser -> `commandhandler.cpp` (minimal local handling)
- MQTT: payload parsing stays in `mqtt.cpp` (`processPendingMessage()` on the netserver loop task) ->
  `commandhandler.cpp` dispatch with the source blocklist
- Physical controls: `controls.cpp`

Implication:
- For universal control behavior, update `commandhandler.cpp` first; then adjust only channel-specific parser
  aliases/blocklists.

## Matrix C: Playlist actions

- Edit/import in browser -> upload to `/webboard` -> LittleFS write
- `netserver` triggers `PLAYLISTSAVED`
- `config.indexPlaylist()/initPlaylist()` refresh index
- `player` and display refresh via request queue events

---

## Telnet Section Interactions (Explicit)

`telnet.cpp` primarily acts as a command ingress path now. Most command behavior is owned by `commandhandler.cpp` and
shared with MQTT/HTTP paths.

Input-line handling is delimiter-based (`\r` or `\n`) with explicit 2000 ms timeouts, which avoids delayed command
execution on clients that submit CR without LF.

The command line itself is parsed by `Utility::parseCommandLine(...)`, shared with the MQTT ingress path; telnet's only
private rule is the `mode 2` -> `-1` alias applied immediately after that call.

If you add a setting command in `commandhandler.cpp`, telnet and MQTT generally inherit it automatically unless blocked
by the shared HTTP/MQTT/Telnet non-WebUI policy.

---

> Tracked issues and risk notes are in `.github/code-issues.md`.


