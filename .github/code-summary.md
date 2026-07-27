# ehRadio Code Summary (Human + AI Operational Map)

## Mandatory Maintenance Directive

If a change affects code interactions (how files/modules interact), storage keys, WebUI contracts, locale/build/dependency behavior, or other external contracts,`.github/code-summary.md` MUST be updated in the same change set; bug fixes that restore expected behavior are exempt unless they also change code interactions or external behavior/contracts.

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
- `.github/workflows/build-release-firmware.yml` verifies generated contributor release artifacts by re-running `builds/fix_releases.py` on CI and diff-checking `builds/releases/` (contains `firmware.txt`, `releases.md`, and `web_assets/`) against what was committed.
- `.github/workflows/build-deploy-page.yml` deploys Pages on `release.published`, and on branch/manual runs it preserves the currently published `firmware-info.json` and manifests instead of overwriting them from `builds/*/web_assets`.
- `.github/workflows/update-timezones.json-automatically.yml` checks out using `DEPLOY_KEY` and pushes over SSH to `dev`, enabling ruleset bypass configured for Deploy keys.

### Runtime chain
- `src/main.cpp` bootstraps system: config -> display -> player -> network -> server/telnet/controls.
- WebUI/WebSocket input path: `netserver` -> `commandhandler` -> `config/player/display/network`.
- State output path: `requestOnChange(...)` in `netserver` -> WebSocket JSON to browser.
- Settings persistence path: `config.saveValue(...)` -> ESP Preferences namespace `"ehradio"`.
- Web-stream resume path: `Config::setLastStationUrl(...)` -> debounced SPIFFS file `/data/laststation.url` -> `player.resumeLastWebSource()` for smartstart/reconnect/direct-URL resume.

### Logging chain (serial + telnet)
- `src/core/logging.h` / `src/core/logging.cpp` now define the common log path for runtime diagnostics.
- The public usage pattern remains uppercase macros at call sites, but they now wrap function-backed implementations (`serialLog`, `functionLog`, `bootLog`, `bootLogX`, `errorLog`, `serialLogDot`, `audioLog`) so formatting happens in one backend instead of nested macro layers.
- Core macros:
  - `SERIALLOG(...)`: writes one formatted line to both serial and telnet sinks.
  - `SERIALLOGX(...)`: writes one formatted line to both serial and telnet sinks without line-ending (useful after `BOOTLOGX`).
  - `FUNCTIONLOG(category, ...)`: category-tagged wrapper over `SERIALLOG`.
  - `BOOTLOG(...)` / `BOOTLOGX(...)`: boot-sequence logging helpers. `BOOTLOG` ends with a line-wrapper, `BOOTLOGX` does not.
  - `ERRORLOG(...)`: error-category wrapper.
  - `SERIALLOGDOT()`: progress-dot helper for long-running loops.
  - `AUDIOLOG(category, ...)`: callback-safe logging wrapper for stack-sensitive audio callback contexts.
- Contract detail:
  - `Telnet::printf(...)` is telnet-only transport and no longer mirrors to serial.
  - Normal logs no longer route through `Telnet::printf(...)`; `logging.cpp` uses `Telnet::logLine(...)` / `Telnet::logRaw(...)` so the shared logging path avoids the prompt-aware telnet formatter and its extra stack use.
  - Logs should use macros above; direct `Serial.print*`/`telnet.printf` is reserved for explicit transport-specific behavior (for example client-targeted telnet responses and OTA progress streaming).

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
- Risk:
  - Wrong env can compile without required modules because files are source-filtered.

### `myoptions.h`
- Board/profile selector and hardware wiring table.
- Sets many user defaults that flow into `config_t` via macros.
- Enables/disables many runtime features by compile-time macro presence.

### `src/core/options.h`
- Canonical fallback defaults and compile flags.
- Includes `myoptions.h` when present.
- **Owns all compile-time guardrails** inline, right next to each respective define.
- Owns shared buffer sizing macros under `/* Maximum lengths of character buffers */`, including `MQTT_URL_SIZE` for stream/artwork URL buffers used by `player`, `audiohandlers`, and MQTT status payload sizing.
- Defines:
  - hardware defaults (pins, feature gates)
  - updater URLs (`FILESURL`, `UPDATEURL`, `CHECKUPDATEURL`) unless disabled
  - weather defaults and thresholds
  - battery defaults/curve/thresholds
  - WebUI and localization defaults
  - curated list defaults
  - **Exception**: locale and language options are handled by locale.h
  - **SPI architecture — Named Bus System** (auto-derived internals — do NOT define `SPI_BUS_SECONDARY`, `SPIA`, or `VS1053_SPIBUS` in `myoptions.h`):
  - **Bus A** = `SPIA` (alias for `&SPI`, the default ESP32 SPI instance). Pins configured by `SPI.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI)` in `Config::init()` when `SPIA_SCK` is defined; otherwise `SPI.begin()` uses hardware defaults.
  - **Bus B** = `SPIB` (`SPIClass SPIB(SPI_BUS_SECONDARY)` declared and initialized in `config.cpp`). Only exists when `SPIB_SCK` is defined. `SPI_BUS_SECONDARY` uses symbolic constants: `VSPI` (ESP32) / `FSPI` (ESP32-S3/C3) for the primary bus, `HSPI` for the secondary bus on all targets.
  - **Bus pin defines** (set in `myoptions.h`): `SPIA_SCK/MISO/MOSI` manually, or use shorthands: `SPIA_DEFAULT` (chip default pins), `SPIA_DEFAULT_XMISO` (chip default SCK/MOSI, MISO=255 — for display-only Bus A). `SPIB_SCK/MISO/MOSI` manually, or `SPIB_DEFAULT` (chip default secondary-bus pins). `SPI.begin()` / `SPIB.begin()` are only called when the respective SCK is defined and `!= 255`; I2C-only builds skip SPI init entirely.
  - **Per-peripheral bus assignment** (char literals `'A'` or `'B'` — NOT strings): `SD_SPI`, `TS_SPI`, `VS1053_SPI`. `VS1053_SPI` resolves `VS1053_SCK/MISO/MOSI` from the matching bus pins in `options.h` (soft — does not override direct pin defines). SD and TS use the bus *object* directly (`SPIA`/`SPIB`) — no separate SCK/MISO/MOSI derivation needed.
  - **`VSPI FSPI` shim** — defined when target is not ESP32 (`!CONFIG_IDF_TARGET_ESP32`) for third-party library compatibility.
  - **VS1053 bus assignment** — `VS1053_SPIBUS` macro auto-derived in `options.h`. `VS1053_CS != 255` requires `VS1053_SCK` to be set (via `VS1053_SPI` or directly); `#error` if missing. Resolves to `SPIB` if `VS1053_SCK == SPIB_SCK`, otherwise `SPIA`. Used as `&VS1053_SPIBUS` in the `Audio` constructor in `player.cpp`.
  - **SD bus selection** — `SDREALSPI` macro in `sdmanager.cpp`: `SPIB` when `SD_SPI == 'B'` and `SPIB_SCK` is defined, otherwise `SPIA`.
  - **Touchscreen bus selection** — inline in `touchscreen.cpp` `init()`: `SPIB` when `TS_SPI == 'B'` and `SPIB_SCK` is defined; `SPIA` when `TS_SPI == 'A'`; `ts.begin()` (default `&SPI`) otherwise.
- **SD card defines** (set in `myoptions.h`, fallback `255` = disabled in `options.h`):
  - `SD_CS` — chip-select pin; `255` disables SD entirely.
  - `SD_SPI 'A'/'B'` — assigns SD to Bus A or B; SD uses the bus object directly, no per-pin derivation needed.
  - `USE_SD` — feature presence macro, derived from `SD_CS!=255`.
- **I2S internal DAC**:
  - `USE_AUDIO_ESP32_DAC` — defined directly in `myoptions.h` to use the ESP32 internal DAC (ESP32 only, not S3/C3). `I2S_INTERNAL` boolean is removed.
- **Guardrail conventions** (maintain these when adding new options):
  - `#error` for hard-invalid values: wrong board type, mutually exclusive decoders, enum/constant out of range (e.g. `TS_MODEL`, `RTC_MODULE`), bad logical cross-constraints (e.g. `BTN_PRESS_TICKS <= BTN_CLICK_TICKS`, `BATTERY_CRITICAL_THRESHOLD >= BATTERY_LOW_THRESHOLD`).
  - `#warning` + `#undef` for out-of-range tunables in `/* USER DEFAULTS */` section (e.g. `SOUND_VOLUME`, `SCREEN_BRIGHTNESS`): reverts silently to default but now emits a visible warning in the build log.
  - `static_assert` with `__builtin_strcmp` for enumerated string options (e.g. `WEATHER_API`, `WEATHER_WIND_SPEED_UNITS`). **Update the `static_assert` whenever a new provider/value is added.**
  - `/* PREVENT BOARD-DEFINED PIN RE-USE */` section lives after the `/* ESP DEVBOARD */` LED block (requires `LED_PIN` and `ESP_S3C3` to be defined first). Covers LED vs RST pin conflicts only — keep it narrowly scoped.
- **What is intentionally NOT guarded**: booleans (compiler error is obvious), pin numbers (board-dependent range), free-form strings (`AP_SSID`, `MQTT_*`, URLs), color macros (R,G,B triplets), `AUTOBACKLIGHT(x)` (C macro function), `BATTERY_CURVE_MV/PCT` (already has `static_assert` in `battery.cpp`).
- **PSRAM buffer sizing** — `PSRAM_BUFSIZE` macro controls the audio input buffer and FLAC reserved buffer sizes. Defaults differ by board
- Buffer bar visual mapping:
  - `BUFFERBAR_VISUAL_FULL_PERCENT` controls where input-buffer fill is rendered as visually full.
  - default `82` means 82% raw fill maps to 100% bar width; set `100` to keep direct 1:1 mapping.

## Compile-Time Modularity and Build Variants (`#if` / `#ifdef` behavior)

This codebase is strongly compile-time modular. Runtime behavior can differ significantly even with the same source, depending on the selected PlatformIO environment and macro definitions.

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
  - **Resolution/interface system**: `DSP_MODEL` = controller chip, `DSP_WIDTH`/`DSP_HEIGHT` = panel resolution, SPI or I2C type may be detected by `I2C_SDA` and `I2C_SCL` pins defines. Resolution defaults per DSP_MODEL in `options.h`, overridable in `myoptions.h`.
  - **dspcore.h**: one `#elif` per DSP_MODEL, sets feature flags (`PSFBUFFER`, `DSP_OLED`, `DSP_LCD`).
  - **dspfont.h** (new): selects bootlogo, clock font, TIME_SIZE by resolution.
  - **dspconf.h** (new): selects `conf/display*conf.h` by resolution × display category.
  - Display `.h` files now delegate conf/font/bootlogo to these central files — no per-file branches for resolution.
  - Conf files no longer define DSP_WIDTH/DSP_HEIGHT (set upstream in options.h).
  - **Removed enum values** (collapsed into resolution variants): DSP_ST7789_240, DSP_ST7789_76, DSP_SSD1306x32, DSP_SSD1305I2C, DSP_1602I2C, DSP_2004I2C, DSP_SSD1327_64. I2C variants detected by `I2C_SDA` and `I2C_SCL`.
  - ST7735 DTYPE still required for library; resolution auto-derived from DTYPE in displayST7735.h.
- Network/update features:
  - some online update and service behavior is compiled out by feature flags.
- MQTT, touch, RTC, SD, battery helper behavior:
  - each has compile gates that can remove handlers/routes or no-op logic.

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
     - optional MQTT init
     - optional smart-start playback
     - `startup.startupServices()`
     - `netserver.setBootReady(true)` only after setup work is actually complete
- `loop()`:
  - AP mode: Improv + captive DNS
  - normal: telnet loop
  - RGB loop
  - `battery.loop()` + `battery.applyPowerPolicy()`
  - player loop (connected/SD ready)
  - controls loop

---

## Core Folder Per-File Map (`src/core`)

### Module Convention
All modules in `src/core/` follow the **class + global instance** pattern:
- The header declares a `class Foo` with the full public interface and private members/methods.
- The `.cpp` defines all `Foo::` methods and declares the single global instance: `Foo foo;`
- The header provides `extern Foo foo;` so callers can use `foo.method()`.
- Hardware-conditional modules use a real class in the `#if` branch and a no-op stub class in the `#else` branch; `extern Foo foo;` is placed after the guard.
- **C-style free-function modules are not acceptable in `src/core/`.**

### Naming Style
- **camelCase** for all identifiers: private members, local variables, private methods (e.g. `inferredCharging`, `lastVoltageMv`, `readAndUpdate`).
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
- `DSP_TFT` added to `dspcore.h` for all TFT display models — used to gate color-theme reloading (monochrome displays skip it).

## `src/core/config.h`
- Defines persistent struct `config_t store`.
- `theme_t` no longer includes `theme.heap`; runtime input-buffer rendering uses `theme.buffer`.
- New fields: `uint8_t themeId`, `bool inverttitle`. `color565()` method removed.
- Defines station/theme structs and config API.
- Defines key constants for SPIFFS paths and data file locations.
- `theme_t` now includes screensaver-specific clock palette fields (`clockss`, `clockbgss`, `secondsss`, `dowss`, `datess`) so screensaver clock/date elements can use colors independent from normal PLAYER-mode clock colors.
- `station_t` fields (`name`, `url`, `title`) are sized by `STATION_FIELD_LENGTH` (default 170, defined in `options.h`). These are RAM-only fields — not NVS-stored. `BUFLEN` has been retired; use `STATION_FIELD_LENGTH` for station metadata buffers across the codebase.
- `SD_PATH_LENGTH` (256, defined in `sdmanager.h`) is used for SD filesystem path buffers where paths may exceed 170 bytes.
- `Config::keyMap` declaration controls Preferences key mapping.
- `Config::saveValue(...)` API now has two simple overloads only:
  - typed: `saveValue(T* field, const T& value)`
  - string: `saveValue(char* field, const char* value)`
- `Config` also owns a separate RAM-backed `lastStationUrl` resume buffer that is intentionally *not* part of `config_t` / Preferences; it is persisted through `/data/laststation.url` with a dedicated debounce path because previews/direct URLs can change more often than normal prefs.
- Legacy compatibility parameters (`commit`, `force`, and string `size_t N`) were removed.
- String saves now normalize into a zero-filled fixed-size buffer before compare/write to avoid reading beyond short source strings.
- Both overloads share a single internal write-if-changed path (`missing key` OR `size mismatch` OR `content changed`) before calling `prefs.putBytes(...)`.
- Save-path writes now emit telnet+serial config logs by key name; sensitive keys (`mqttpass`, `weatherkey`) are masked as `*`.

## `src/core/config.cpp`
- Persistent storage/defaults/hardware bootstrap center.
- Main responsibilities:
  - load and validate Preferences (`cfgset` marker)
  - defaults/init logic
  - SPIFFS mount and Config-owned required-file checks used during playlist-mode initialization
  - version marker management (`/data/ehradio.ver`)
  - load/save the debounced Web-stream resume hint (`/data/laststation.url`)
  - playlist-mode initialization and file-presence checks before delegating playlist indexing/load helpers to `utility`
  - canonical SPIFFS asset allowlists (`Config::wwwFiles[]`, `Config::dataFiles[]`) used by startup recovery and file-maintenance flows
  - reset section handlers (`defaultSettings(...)`)
- SPI bus initialization: `Config::init()` calls `SPI.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI)` only when `SPIA_SCK` is defined and `!= 255`, and `SPIB.begin(SPIB_SCK, SPIB_MISO, SPIB_MOSI)` only when `SPIB_SCK` is defined and `!= 255`. I2C-only builds skip SPI init entirely. Both buses are initialized before `_initHW()` and before `display.init()` / `player.init()`. Both SPI buses are fully configured before any peripheral uses them. `SPIClass SPIB(SPI_BUS_SECONDARY)` is declared at file scope in `config.cpp`; extern declared in `config.h`.
- SD-specific behavior:
  - `_initHW()` configures `SD_CARD_DETECT_PIN` as `INPUT_PULLUP` when available
  - `initPlaylistMode()` short-circuits to `PM_WEB` without calling `sdman.start()` when `SD_CARD_DETECT_PIN` reports slot-empty during SD boot
  - `changeMode()` short-circuits SD mode switches the same way, avoiding the SPI retry path when the slot is empty
- Key interaction:
  - almost every module reads/writes through `config`.
- Theme loading note:
  - `Config::loadTheme()` now uses `memcpy_P(&theme, &_themes[config.store.themeId], sizeof(ThemeData))` — loads from PROGMEM `_themes[]` array at runtime. No longer uses compile-time `COLOR_*` macros.
  - `applyInvertTitle()` removed from Config (moved to Display).

## `src/core/startup.h` / `startup.cpp`
- Boot-only orchestration module following the standard core `class + global instance` pattern (`Startup startup;`).
- Owns startup-time helpers that were previously mixed into `config.cpp`:
  - boot-time version marker and required SPIFFS/WebUI file verification (`checkVerAndSpiffs()`)
  - loading saved SSIDs from `/data/wifi.csv` into `config.ssids`
  - stale search-result cleanup under `/www/searchresults.*`
  - required WebUI asset download and recovery flow
  - version-file parsing for online-update detection
  - startup background update scheduling (`startupServicesAsync`) — spawned as a FreeRTOS task on `NETWORK_CORE` at low priority:
    - waits `STARTUP_SERVICES_DELAY` seconds before any work, letting audio buffer fill first
    - verifies WebUI locale JSON file; downloads if missing
    - checks for new firmware version via `new_ver.txt`; triggers OTA if `autoupdate` is enabled
    - downloads default `playlist.csv` from `PLAYLIST_DEFAULT_URL` if file is missing
    - updates `timezones.json.gz` and `rb_srvrs.json` from online sources
    - cleans stale search results older than 24 hours
    - deletes the `ESPFileUpdater` param and self-terminates via `vTaskDelete(NULL)`
  - `deassertCsPins()` — called from `main.cpp` `setup()` before any device init. Sets all known SPI CS pins (`VS1053_CS`, `SD_CS`, `TFT_CS`, `TS_CS`) to `OUTPUT` + `HIGH` to prevent floating CS from causing bus contention during peripheral detection.
  - safe mode boot crash-loop detection (`checkSafeMode`, `bootInSafeMode`, `markBootStable`, `loop`): reads NVS key `lastbootgood` at boot — if previous boot did not complete successfully, disables `smartstart` and `autoupdate` in memory only for this session so the device does not auto-reconnect to a crash-causing stream; marks boot stable after `BOOT_STABLE_TIME` seconds of uptime
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
    - SD card hot-insert detection (when `SD_AUTOPLAY && SD_CARD_DETECT_PIN!=255`): polls `SD_CARD_DETECT_PIN` every ~2 s in `divrssi` block; calls `config.changeMode(PM_SDCARD)` on insertion
  - weather provider dispatch:
    - `OM1` Open-Meteo
    - `OW25` OpenWeather 2.5
    - `OW30` OpenWeather 3.0
  - weather cache and formatting logic
  - centralized runtime logging for reconnect/weather/boot progress/time-sync via `FUNCTIONLOG`/`SERIALLOG`/`BOOTLOGX`
  - web-stream reconnect now resumes through `player.resumeLastWebSource()` so direct URL sources can recover via `/data/laststation.url` instead of always falling back to `lastStation`
  - `retryStreamConnection` task (40 attempts × 15 s) can be externally cancelled by `commandhandler.cpp` `cancelStreamRetry()` when the user issues any playback-changing command; the task also cleans itself up when conditions change (user stops, WiFi drops, or playback resumes)
- Coupling:
  - pushes display updates (`display.putRequest(...)`)
  - calls player/netserver hooks
  - reads/writes `config.store`
- Successful connect handling now stays internal to `network.cpp`; there is no remaining app-level weak `network_on_connect` callback.

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
  - command queue depth: `xQueueCreate(10, ...)` — increased from 5 to prevent queue overflow during rapid mode-switch sequences (SD→web transitions) where multiple commands (PR_STOP, PR_PLAY, PR_VUTONUS) arrive before the first finishes processing.
  - direct playback lifecycle side effects for `rgbled` and `backlightControls` (start/stop + initial stopped-state sync)
- VS1053 SPI: `Player::Player()` constructor passes `&VS1053_SPIBUS` to the `Audio(CS, DCS, DREQ, SPIClass*)` constructor. `VS1053_SPIBUS` is the `SPIB` or `SPIA` object resolved by `options.h`. No `SPIClass` declared in `player.cpp` or `player.h`.
- Coupling:
  - updates display queue and websocket state
  - uses `config` station and mode state
  - interacts with radio-browser click reporting (clicks are gated on `!network.lostPlaying` so automatic retry reconnections do not fire clicks; only explicit user-initiated plays do)
  - calls `rgbled` and `backlightControls` directly during playback start/stop

## `src/core/audiohandlers.h` / `audiohandlers.cpp`
- Callback bridge used by the audio libraries.
- `audiohandlers.h` now declares the `AudioHandlers` module and the required free `audio_*` callback symbols; `audiohandlers.cpp` owns the implementation as a normal core translation unit.
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
- Audio info/bitrate/ID3 notifications are emitted through shared logging macros so telnet+serial output stays consistent with the rest of the firmware log contract.

## `src/core/display.h` / `display.cpp`
- `display.h` declares Display class and display mode/change API.
- Render queue + display task + widget/page orchestration.
- **Theme/layout/invert runtime switching** — major refactor:
  - `_applyState()` — central state function: loads layout from PROGMEM, loads theme from PROGMEM (TFT only), applies invert (color swap + `metaBGConf_ptr` switch), reinitializes widgets, redraws. Called by all three commands (`theme`, `layout`, `inverttitle`).
  - `_setLayoutPointers()` — extracted 30-pointer setup shared by init and runtime paths.
  - `applyLayout()`, `applyTheme()`, `applyInvertTitle()` — thin wrappers that set config then call `_applyState()`.
  - `inverttitle` is runtime boolean (`config.store.inverttitle`). On TFT: swaps meta↔metabg colors + switches to `metaBGConfInv` layout. On OLED: color swap only. Uses conditional rendering at widget init time — no theme mutation.
  - `getThemeListJson()` / `getLayoutListJson()` — serve dropdown data to WebUI from PROGMEM `_themeNames[]` / `_layoutNames[]`.
  - `#ifndef HIDE_*` compile guards removed from `_reinitWidgets()` and `_buildPager()` — all widget init now uses runtime null-checks.
  - `_buildPager()` no longer calls `_reinitWidgets()` — state application is handled by `_applyState()`.
  - `_start()` calls `_buildPager()` then `_applyState()` — widgets created then initialized with correct state.
  - `DSP_INVERT_TITLE` compile-time macro eliminated.
- Buffer bar terminology was aligned to actual behavior:
  - `_heapbar` -> `_bufferbar`
  - `HIDE_HEAPBAR` -> `HIDE_BUFFERBAR`
  - `heapbarConf` -> `bufferbarConf`
- Input-buffer bar values still come from `player.inBufferFilled()`; only visual normalization changed via `BUFFERBAR_VISUAL_FULL_PERCENT`.
- Battery widget support now follows the same config-file contract: feasible display configs define `batteryConf` plus `batteryRangeLowFmt` / `batteryRangeMidFmt` / `batteryRangeHighFmt`, while tiny or LCD-only layouts use `HIDE_BATTERY` to keep `display.cpp` compile-safe.
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
  - static file serving from PSRAM cache (via `StaticFileCache`) — no SPIFFS reads during HTTP serving
  - fallback to SPIFFS for dynamic files not in cache (`searchresults.json`, `curated.json`, etc.)
  - `handleNotFound`: checks PSRAM cache first for GET requests, cache-busting `?v=` stripping removed (max-age=60 handles freshness)
  - `handleIndex`: serves `player.html` from PSRAM cache for `GET /`
  - route handlers (`/`, `/search`, `/update`, `/locale.json`, `/ready`, etc.)
  - `fileCache.loadAll()` called in `begin()` before `webserver.begin()`
  - `invalidateCache()` public method exposed for `utility.cpp` runtime updates
  - `Cache-Control: max-age=60` for all cached static files (was 3600)
  - `/settings.html`, `/update.html`, `/ir.html` no longer served via `index_html[]` — handled by PSRAM cache fallthrough
  - websocket command parsing and outbound updates
  - state request queue processing (`GETSYSTEM`, `GETSCREEN`, `GETLOCALE`, etc.)
  - online update check/start tasks
  - radio-browser search and curated task management
  - exact-match-first preview/add handling on `/search`; unmatched preview now uses the same direct URL playback path as `playurl` instead of a mutating playlist scan
  - centralized logging for search/curated/playback/radio-browser-click/update/not-found paths via `FUNCTIONLOG`
- Coupling:
  - uses `cmd.exec(...)` from commandhandler
  - emits JSON consumed by `data/www/script.js`
  - `GETSCREEN` now also carries `invtitle`, `layout`, `theme` fields for WebUI dropdown state
  - Virtual endpoints `/themes.json` and `/layouts.json` serve dropdown data from PROGMEM arrays
- Readiness detail:
  - `/ready` returns `{"ready":true}` only when `netserver.bootReady` is true, required web files exist, and network state is stable (`CONNECTED` + `WL_CONNECTED`, or `SDREADY`).
- OTA note:
  - OTA start/end/error callbacks use `FUNCTIONLOG`.
  - OTA progress now uses `FUNCTIONLOG` (line-oriented output, no raw `\r` streaming path).

## `src/core/commandhandler.h` / `commandhandler.cpp`
- `commandhandler.h` declares command execution API for command strings.
- Central command router for WS, URL params, MQTT, and telnet fallback paths.
- Main responsibilities:
  - map `key=value` commands into config/player/display/network actions
  - request websocket state snapshots
  - persist settings with `config.saveValue(...)`
  - source-aware command policy (`WebSocket`, `HttpUrl`, `Mqtt`, `Telnet`) and shared non-WebUI blocklist checks for HTTP/MQTT/Telnet ingress
  - own shared command aliases across ingress channels (`playstation`/`play`, `boot`/`reboot`, `vol+`/`volup`, `dim`/`brightness`, `dspon`/`screenon`)
  - player-command parity helpers (including exact-match-first direct URL playback command routing for `playurl` / `burl`)
  - trigger curated operations and locale update tasks
  - cancel the stream retry task (`cancelStreamRetry()`) before executing user-initiated playback commands (`stop`, `playstation`, `prev`, `next`, `toggle`, `turnoff`, `burl`, `mode`, `submitplaylist`) so explicit user actions always interrupt automatic reconnection loops
- Critical coupling file for setting changes.
- New commands: `theme` (theme switching), `layout` (layout switching), `inverttitle` (invert title toggle). All persist via `saveValue` and trigger `display._applyState()`.

## `src/displays/themes.h`
- **NEW FILE** — Runtime theme switching data.
- `ThemeData` struct: 33 `uint16_t` color fields + `playlist[5]` — mirrors `config.h` `theme_t`.
- `const ThemeData _themes[] PROGMEM` — array of theme presets (default + imported).
- `const char _themeNames[][32] PROGMEM` — display names for WebUI dropdown.
- `RGB(r,g,b)` macro packs 8-bit RGB into RGB565 `uint16_t`.
- Populated by `importtheme.py` script.

## `src/displays/importtheme.py`
- **NEW SCRIPT** — imports old-style `#define COLOR_*` theme files into `themes.h`.
- Handles `#ifdef`/`#ifndef` branching to generate multiple theme variants (permutation).
- Smart fallbacks for missing colors (e.g., `.dow` ← `.date`, `.battery` ← `.rssi`).
- Meta fallback for everything else (uses `.meta` color instead of zeros).
- Name truncation at 31 chars for `_themeNames[][32]`.
- `--dry-run` writes to `.new.h`.

## `src/core/controls.h` / `controls.cpp`
- `controls.h` declares `class Controls` with public interface: `init()`, `loop()`, `setEncAcceleration()`, `setIRTolerance()`, `flipTS()`, `controlsEvent()`.
- `extern Controls controls;` provides the global instance; callers use `controls.init()`, `controls.loop()`, etc.
- All internal helpers (`onBtnClick`, `encodersLoop`, `irLoop`, etc.) are private class methods.
- Static trampoline methods (`_btnClickCb`, etc.) used for `OneButton` callbacks (function-pointer API; cannot capture `this`).
- `readEncoderISR` / `readEncoder2ISR` remain free functions with `IRAM_ATTR` (ISR constraint; access file-scope `encoder`/`encoder2` directly).
- Physical controls integration:
  - OneButton
  - rotary encoders
  - touchscreen gestures
  - IR remote decoding
- Converts hardware input events into same core actions used by WebUI (`controlsEvent`, player commands, display mode changes).
- `Controls::loop()` now calls `backlightControls.controlsLoop()` directly for non-PLAYER backlight wake behavior.
- IR record debug text now routes through centralized logging macros.
- Screensaver wake hardening:
  - `controlsEvent()` now flushes pending display requests (`display.resetQueue()`) and zeroes screensaver tick counters before queueing `NEWMODE, PLAYER` when waking from `SCREENSAVER`/`SCREENBLANK`, preventing one-detent rotary wake races where a stale queued screensaver mode request could immediately re-apply.

## `src/core/telnet.h` / `telnet.cpp`
- Telnet and serial command handling.
- Responsibilities:
  - manage client sessions
  - read input lines with CR/LF-pair handling so Enter submits immediately across CR/LF client variants and empty Enter events are preserved
  - apply explicit 2000 ms stream timeout configuration for serial and per-client telnet streams
  - normalize command strings (`key=value`, `key value`, `key(value)`) plus minimal payload-shape handling (`play` value-shape handling)
  - route commands through `cmd.exec(...)` with source `Telnet`
  - keep command handling output-minimal (no telnet-specific reporting command table)
- Important:
  - acts as secondary control channel parallel to WebUI
  - command handling is intentionally kept near-parity with MQTT/HTTP routes
  - `Telnet::printf(...)` is transport-only and no longer echoes to serial
  - `help`, `quit`, and `bye` are handled locally in telnet before commandhandler dispatch
  - `quit` / `bye` silently disconnect only the issuing Telnet client
  - empty input lines now re-show prompt (`> `), aligning interactive UX with common telnet clients

## `src/core/mqtt.h` / `mqtt.cpp`
- MQTT integration if `MQTT_ENABLE` compile flag exists.
- `mqtt.h` declares `class Mqtt` with `init()`, `loop()`, `publishStatus()`, `publishVolume()`, `publishPlaylist()`.
- `extern Mqtt mqtt;` (inside `#ifdef MQTT_ENABLE`) provides the global instance.
- Private static callback methods (`_connectCb`, `_onConnect`, `_onDisconnect`, `_onMessage`) used for AsyncMqttClient API (static required by library callback interface).
- Responsibilities:
  - connection lifecycle
  - subscribe to `.../command`
  - publish status/playlist/volume
  - status payload now includes `image_url` (HTTP/S image-only artwork URL used by Home Assistant)
  - parse command payload forms (`key=value`, `key value`, `key(value)`, raw URL)
  - apply minimal payload-shape normalization (`play` value-shape handling) then dispatch through `cmd.exec(...)` with source `Mqtt`
  - apply explicit non-WebUI blocklist rejections for unsupported MQTT-origin commands
- Coupling:
  - command behavior is now primarily centralized in commandhandler.
  - `ARTWORK` queue events in `netserver` trigger MQTT status republishes even without a WebSocket payload.
  - artwork payload data is read from `audioHandlers`, not from `config.station`.
- Status buffer sizing now derives from `STATION_FIELD_LENGTH` plus `MQTT_URL_SIZE`, replacing the older duplicated browse-URL size macro.

## `src/core/utility.h` / `utility.cpp`
- Shared helper module following the standard core `class + global instance` pattern (`Utility utility;`).
- Current responsibilities:
  - `stripWhitespace(char*)`
  - `stripWrappingQuotes(char*)`
  - `ipToStr(...)`
  - `escapeQuotes(...)`
  - playlist CSV parsing and station lookup/load helpers
  - WiFi credential parse/save/import helpers
  - deep-sleep entrypoints (`doSleepW`, `sleepForAfter`)
  - SPIFFS file-maintenance helpers shared with startup and WebUI update paths:
    - `cleanupSpiffs()`
    - `deleteMainwwwFile()`
    - `updateFile(...)`
    - `updateLocaleFile()`
    - `updateLocaleFileAsync(...)`
- Holds small reusable scratch/state buffers (`ipBuf`, `stationBuf`) plus the sleep duration state and sleep `Ticker`; it still does not own playback/artwork runtime state.
- Current consumers include `audiohandlers.cpp`, `battery.cpp`, `commandhandler.cpp`, `config.cpp`, `display.cpp`, `netserver.cpp`, `network.cpp`, `player.cpp`, and startup/update flows.

## `src/core/battery.h` / `battery.cpp`
- `battery.h` declares `class Battery` (real class under hardware guard; no-op stub in `#else`); `extern Battery battery;` provides the global instance.
- Public interface: `init()`, `bootStatus()`, `isInitialized()`, `getStatus()`, `formatStatusLine()`, `loop()`, `applyPowerPolicy()`, `recalcNow()`, `calibrate()`.
- All ADC/inference state and helpers are private members/methods.
- Battery monitoring/calibration/inference implementation.
- Responsibilities:
  - ADC sampling and filtering
  - battery presence detection
  - charge/discharge inference with candidate windows
  - threshold state (`low`, `critical`) tracking
  - battery-driven brightness reduction / recovery and critical deep-sleep policy
  - status formatting for telnet/WebUI
  - triggers display and websocket updates
- Logging note:
  - battery status/debug/inference messages now use centralized logging macros (including `BATTERY_DEBUG` paths), replacing direct serial/telnet prints.

## `src/core/backlightcontrols.h` / `backlightcontrols.cpp`
- `backlightcontrols.h` declares `class BacklightControls` (real class under `BRIGHTNESS_PIN` + `DSP_DIMMING_ENABLED` guards; no-op stub in `#else`); `extern BacklightControls backlightControls;` provides the global instance.
- Public interface: `init()`, `restart()`, `controlsLoop()`.
- Responsibilities:
  - own persisted idle-dimming timer state and non-blocking brightness ramp
  - use `config.store.dimmingEnabled`, `dimmingTimeout`, and `dimmingBrightness` instead of board-only compile-time dimming thresholds
  - clamp the dim target to the current screen brightness, restore configured brightness, and restart the idle timer on explicit activity/settings events
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
- `rgbled.h` declares `class RgbLed` (real class under `RGB_LED_PIN` guard; no-op stub in `#else`); `extern RgbLed rgbled;` provides the global instance.
- Public interface: `init()`, `isInitialized()`, `set()`, `playing()`, `stopped()`, `trackChange()`, `loop()`.
- Optional RGB LED state machine:
  - playing/stopped colors
  - track-change flashing
  - optional cycle behavior

## `src/core/sdmanager.h` / `sdmanager.cpp`
- `sdmanager.h` declares SD manager API and FS integration wrapper.
- SD lifecycle and SD playlist indexing.
- Responsibilities:
  - mount/retry/unmount — `start()` attempts up to 4 `SD.begin(SD_CS, ...)` calls, early-returning on success (delays only between retries, not after success)
  - card-present checks
  - recursive scan and media file playlist/index creation
  - scan/index progress and errors now use centralized logging macros (`SERIALLOGDOT`, `ERRORLOG`)
- SPI bus: `SDREALSPI` macro resolved at compile time — `SPIB` when `SD_SPI == 'B'` and `SPIB_SCK` defined, otherwise `SPIA`. Both buses are initialized in `Config::init()` before `SDManager::start()` runs. No `SPIClass` declared in `sdmanager.cpp`.
- SD CS pin is `SD_CS`. Guard macro: `#if SD_CS!=255`.
- Coupling:
  - consumed by config/player for SD mode.

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
- Reboot/update redirect nuance:
  - `redirectWhenReady(...)` only redirects after it has observed at least one not-ready state, preventing a false-positive redirect against the still-running pre-reboot instance.
  - `/ready` probes now use a short client-side fetch timeout so reboot/reset flows do not stall waiting on a dead device connection during restart.
  - reboot and update redirect calls now explicitly apply a 1-second post-ready grace in JavaScript before navigation.
  - manual upload completion now uses a 60 second fallback, while OTA still uses 180 seconds; both can redirect early as soon as `/ready` reports true.
  - mDNS rename (`restartmdns`) calls `MDNS.end()` + `MDNS.begin()` at runtime via `NetServer::restartMdns()` — no reboot. Browser-side: sends `mdnsname=`, swaps the button row for a status message, then polls the new `.local` host with `redirectWhenReady` (8s timeout, 500ms post-ready grace). If mdnsValue is empty, saves silently without redirect.
- consolidated with `data/www/dragpl.js`
  - Playlist drag-and-drop reorder behavior.

## `data/www/options.js`
- Settings page behavior.
- Responsibilities:
  - timezone JSON loading and dropdown population
  - locale list loading and locale switch logic
  - weather provider field visibility logic
  - **theme/layout dropdown loading** — fetches `/themes.json` and `/layouts.json`, populates `#themeId`/`#layoutId` dropdowns, sends `websocket.send("theme=N")` / `websocket.send("layout=N")` on change
  - `afterSetupElement` hook restores current `themeId`/`layoutId` from WebSocket data
  - apply handlers for locale/weather/mqtt/wifi
  - reboot/reset/format status screen with per-action behavior:
    - reboot/reset use ready-aware return-to-root with shorter fallback (15s)
    - format SPIFFS shows reboot status but skips automatic reload

## `data/www/player.html`
- Player page structure (playlist, controls, sliders, status elements).

## `data/www/settings.html`
- Settings page structure with grouped sections and `data-command` bindings.
- Contains element IDs expected by websocket payload mapping.

## `data/www/update.html`
- Update page layout (manual upload + online update controls).

## `data/www/ir.html`
- IR recording and assignment UI.

## `data/www/search.html`
- Search UI for radio-browser integration.

## `data/www/curated.html`
- Curated list browsing/import page.

## `data/www/script2.js`
- Consolidated helper script loaded by main shell and standalone search/curated pages.
- Contains logic previously in `ir.js`, `updform.js`, `playstation.js`
  - station preview/play helper (`sendStationAction`)
  - online update check/start UI helpers
  - IR setup/learn interactions (`initControls`, `checkSelect`, `irClear`, `backRecord`)
- also consolidated with `data/www/locale.js`
  - i18n runtime helper (`t(...)`) and translation application (`applyI18n`).
  - Applies key-based translations to DOM and fallback behavior.

## `data/www/search.js`
- Search page API calls, pagination, result actions, and import hooks.

## `data/www/curated.js`
- Curated list fetch/load/import page logic.

## `data/www/style.css`
- Primary stylesheet.

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
  - `SliderWidget` buffer/volume bar rendering now repaints the full inner area each update to avoid stale pixels after page/mode transitions.
- `src/displays/widgets/pages.h`, `pages.cpp`
  - page and pager composition framework.

## Display driver files (`src/displays/display*.h/.cpp`)
- Similar pattern:
  - init hardware
  - draw primitives/text/pages
  - sleep/wake/flip/invert where supported
- Files include:
  - `displayST7735*`, `displayST7789*`, `displayST7796*`
  - `displayILI9341*`, `displayILI9488*`, `displayILI9225*`
  - `displaySSD1306*`, `displaySSD1305*`, `displaySH1106*`, `displaySSD1327*`, `displaySSD1322*`
  - `displayN5110*`, `displayGC9A01A*`, `displayGC9106*`, `displayST7920*`, `displayLC1602*`

## Display config files (`src/displays/conf/*.h`)
- Mostly widget coordinates/sizing/visibility for each panel class.
- Treated as layout maps rather than logic-heavy files.
- Consolidated canonical conf files now route multiple models by panel type/dimensions:
  - `displayOLED128x64conf.h` for SH1106/SH1107/SSD1305/SSD1306 128x64 OLED class.
  - `displayTFT480x320conf.h` for ILI9488/ST7796 class.
  - `displayTFT320x240conf.h` for ILI9341/ST7789 class.
- The canonical files are intentionally deduplicated masters without per-model `#if DSP_MODEL...` branches; model-specific legacy deltas were removed in favor of one shared layout baseline per family.
- Display driver headers now include conf files directly; custom fallback include blocks using `__has_include("conf/*_custom.h")` were removed.
- `metaBGConf` / `metaBGConfInv` — runtime-switchable station-name background (full bar vs thin line on TFT; bar vs no-bar on OLED). Controlled by `config.store.inverttitle`.
- OLED configs: `metaBGConf` may be `{ }` (normal mode no bar), `metaBGConfInv` has the bar (inverted mode). `importlayout.py` auto-swaps when importing into OLED targets.
- Buffer bar layout/visibility symbols were renamed:
  - `heapbarConf` -> `bufferbarConf`
  - `HIDE_HEAPBAR` -> `HIDE_BUFFERBAR`
- Battery widget layout now exists on the feasible non-LCD configs; unsupported small/LCD panels explicitly define `HIDE_BATTERY`.

## Display tools
- `src/displays/tools/utf8To.*`
- `src/displays/tools/utf8_common.*`
- `src/displays/tools/utf8Latin.*`
- `src/displays/tools/utf8Cyrillic.*`
- `src/displays/tools/commongfx.h`
- `src/displays/tools/psframebuffer.h`
- `src/displays/tools/oledcolorfix.h` — OLED monochrome color initialization. `DSP_INVERT_TITLE` ifdef removed; `metafill` corrected to `TFT_FG` (was `TFT_BG` — invisible on black screen).
- `src/displays/tools/tftinverttitle.h` — **DEPRECATED**, no longer included. Replaced by runtime `inverttitle` in `_applyState()`.

Purpose:
- text normalization/transliteration and glyph handling
- display utility support

## Display fonts/assets
- `src/displays/clockfonts/*` for digit/font assets.
- `font15.h` is wired for 128x64 mono-OLED clock paths (SH1106/SH1107, SSD1306 128x64, SSD1305). Clock font dispatch headers now live under `src/displays/clockfonts/` (`font15.h`, `font19.h`, `font35.h`, `font52.h`, `font70.h`), and DS_DIGI/Chunky6 families also live there (`src/displays/clockfonts/DS_DIGI/`, `src/displays/clockfonts/Chunky6/`). `font15.h` maps CHUNKY6 modes directly to Chunky6_15 variants; `font19.h` is retained for possible future use. For clock rendering fallback, widgets now force `CLOCKFONT5x7` when `TIME_SIZE<15`, or when `TIME_SIZE==15` with `CLOCKFONT` set to `YO_MONO`.

## GFXfont Rendering Pipeline (`src/displays/tools/commongfx.h`, `psframebuffer.h`)

`DspCore::_writeGlyph(uint16_t cp)` is the central glyph renderer replacing the legacy 256-slot glcdfont system. Key behaviors:

- **Dispatch:** When `gfxFont != NULL && gfxFont != &DisplayFont` (a special clock font is active), delegates to `Adafruit_GFX::write()` which uses the font's native `drawChar`. Otherwise renders via `&DisplayFont` (the configured Unicode GFXfont).
- **Icon rendering:** Codepoints `0x01-0x1F` map to `ICON_TABLE[]`; rendered with `startWrite()`/`endWrite()` wrapping (needed for Adafruit SPI TFT drivers).
- **Font glyph rendering:** Similarly wrapped in `startWrite()`/`endWrite()`. The inner loop iterates all `width` bitmap columns but only renders columns where `(xOffset + xx) < xAdvance` — prevents glyph bleed beyond the cell boundary (fixes colon/digit overlap on SH1106 YO_MONO). Bleed columns have their bits consumed from the bitmap stream but are not rendered.
- **Space handler:** Advances cursor by the font's first-glyph `xAdvance` without drawing pixels.
- **Unrenderable fallback:** Falls through `foldAccent()`; if still unmapped, advances cursor by the font's `xAdvance` (blank space).
- **`psframebuffer.h`** mirrors the same `_writeGlyph` logic for PSRAM-framebuffer TFT displays.

Important rendering invariants:
- Never call `startWrite()`/`endWrite()` in `write()` or `writePixel`/`writeFillRect` overrides — SPI nesting causes hangs on Adafruit TFT drivers.
- The `gfxFont == NULL` case (YO_MONO / display font) runs through `_writeGlyph` using DisplayFont, NOT the built-in glcdfont. This means glyph metrics (yAdvance, yOffset, xAdvance) come from DisplayFont.

## Screen Rendering Fixes (Session: SH1106 YO_MONO)

### ClockWidget colon blink (widgets.cpp)
- The seconds-area `fillRect` at line 813 is guarded by `if (Clock_GFXfontPtr != NULL)` — its Y-position formula (`_top() - _timeheight + _space`) is only correct for special clock fonts; YO_MONO renders at `_top()` directly and the fillRect would clear into title rows.

### Screensaver clock boundaries (display.cpp, widgets.h)
- `minTop = max(TFT_FRAMEWDT, _clock->timeHeight())` — for framebuffer displays, `_config.top - _timeheight` must stay >= 0 to avoid framebuffer clipping above the display.
- `maxTop = dsp.height() - clockH - TFT_FRAMEWDT` — was missing the `-clockH` term, allowing the clock to render partially off-screen.
- Movement interval now uses `% SCREENSAVERMOVE` (default 5 seconds) instead of hardcoded `% 60`.
- `ClockWidget::timeHeight()` accessor added to expose `_timeheight`.

### NumWidget volume-digit clearing (widgets.cpp)
- YO_MONO clear height: `realth = _textheight * CHARHEIGHT` (was OLED-only; now applies to all displays with `Clock_GFXfontPtr == NULL`).
- Special-font clear height: `realth = _textHeight() + 1` — uses the font's actual glyph height + 1px margin, matching `_clearClock()`'s pattern.

### Font directory renamed
- `src/displays/ehfonts/` → `src/displays/clockfonts/`
- `YO_CLASSIC` removed as a clock font option (variable-width variant of YO_MONO that broke layout assumptions).

---

## CPU Core Assignments & Stack Sizes

The ESP32 has two hardware cores: **Core 0** (PRO_CPU) and **Core 1** (APP_CPU). The ESP32 Arduino framework runs `setup()` and `loop()` on Core 1. Audio decoding is isolated on Core 0; all other application tasks run on Core 1.

### Compile-time Core Macros (`src/core/options.h`)

Two macros control core assignment across the codebase:

| Macro | Default | Valid override | Purpose |
|---|---|---|---|
| `AUDIO_CORE` | `0` | `1` | Core for audio decode task |
| `NETWORK_CORE` | `1` | `0` | Core for netserver and all network/utility tasks |
| `DSP_TASK_CORE_ID` | `1` | `0` | Core for the display loop task (independent of `NETWORK_CORE`) |

On single-core ESP32-C3 (`CONFIG_FREERTOS_UNICORE`), all three macros are forced to `0` automatically; defining any of them manually on a unicore build is a compile-time `#error`. `CONFIG_ASYNC_TCP_RUNNING_CORE` is tied to `NETWORK_CORE` so the AsyncTCP internal event task follows automatically.

### Board Stack Multiplier (`STACK_MULTIPLIER`)

A board-aware multiplier scales all five user-configurable FreeRTOS task stacks automatically:

| Board | `STACK_MULTIPLIER` | Effect |
|---|---|---|
| ESP32-S3 | 2 | All base stack sizes doubled |
| ESP32 | 1 | Base sizes unchanged |
| ESP32-C3 | 1 | Base sizes unchanged (C3 has *less* RAM than base ESP32) |

- Defined automatically after the board guard in `src/core/options.h`. Override in `myoptions.h` with `#define STACK_MULTIPLIER 1` or `2` if needed.
- Only values `1` and `2` are accepted — a compile-time `#error` fires otherwise.
- Per-task manual overrides (e.g. `#define DSP_TASK_STACK_SIZE 6`) bypass the multiplier; a `#elif` range guard validates the manually-set value.
- The multiplier applies **only** to the five configurable task stacks. Fixed-stack tasks (HTTPS workers, OTA) are unaffected.
- `SEARCHRESULTS_BUFFER` and `CONFIG_ASYNC_TCP_QUEUE_SIZE` use separate per-board explicit values (not `STACK_MULTIPLIER`) since they scale differently.

### Core 0 — Audio

#### `src/libraries/I2S_Audio/Audio.cpp` + `src/libraries/VS1053_Audio/audioVS1053Ex.cpp`
- Both audio libraries pin their `PeriodicTask` (audio decode loop) to `m_audioTaskCoreId`.
- `src/core/player.cpp` `Player::init()` calls `setAudioTaskCore(AUDIO_CORE)` to set this explicitly (defaults to Core 0).

### Core 1 — Everything Else

#### `src/core/display.cpp`
- `loopDspTask` ("DspTask") is pinned to `DSP_TASK_CORE_ID` (default `1`) via `xTaskCreatePinnedToCore`.
- This task calls `display.loop()` only. `netserver.loop()` was moved to its own dedicated task (see `netserverLoopTask` below).

#### `src/core/network.cpp`
- `doSync` (time/weather sync task) is pinned to `NETWORK_CORE`.
- `searchWiFi` (WiFi connection/retry loop) is pinned to `NETWORK_CORE`.
- `retryStreamConnection` (post-disconnect reconnect) is pinned to `NETWORK_CORE`.


#### `src/core/netserver.cpp` — `netserverLoopTask` + all utility tasks pinned to `NETWORK_CORE`
- `netserverLoopTask` (started by `NetServer::startLoopTask()`, called from `main.cpp` after each `netserver.begin()`) is pinned to `NETWORK_CORE`. It is the sole caller of `netserver.loop()`.
- All formerly scheduler-assigned (`xTaskCreate`) utility tasks are explicitly pinned to `NETWORK_CORE` via `xTaskCreatePinnedToCore`:
  `vTaskSearchRadioBrowser`, playback task (lambda), radio-browser click task (lambda), `checkForOnlineUpdateTask` (lambda), `startOnlineUpdateTask` (lambda)

#### `src/core/startup.cpp`, `src/core/utility.cpp`, `src/core/commandhandler.cpp` — pinned to `NETWORK_CORE`
- `src/core/startup.cpp`: `startupServicesAsync`
- `src/core/utility.cpp`: `updateLocaleFileAsyncWrapper`
- `src/core/commandhandler.cpp`: `vTaskFetchCuratedIndex`, `vTaskFetchCuratedPlaylist`

#### Arduino `loop()` — implicit Core 1
- All calls from `src/main.cpp` `loop()` run on Core 1: `telnet.loop()`, `battery.loop()`, `player.loop()`, `controls.loop()`.
- `netserver.loop()` is **not** called from `loop()` or from DspTask; it runs exclusively in `netserverLoopTask` pinned to `NETWORK_CORE`.

### FreeRTOS Task Reference

Stack sizes and priorities are controlled by macros in `src/core/options.h` (`/* Tweaks for Core Processes */`). Higher priority = more CPU; Arduino `loop()` runs at priority 1. Priority 0 is idle-level (starved) and is never used. Per-task local conversion macros (`_BYTES`) are defined at the top of each `.cpp` file (except `SET_LOOP_TASK_STACK_SIZE` which is an `Arduino.h` macro). Stack defaults scale with `STACK_MULTIPLIER` — values shown as ESP32/C3 (1x) / S3 (2x).

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
| `updateLocaleFileAsyncWrapper` | utility.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |
| `startupServicesAsync` | startup.cpp | 8192 fixed | `LOW_TASK_PRIORITY` (1) | HTTPS — stack hardcoded |

### CORE_MONITOR debug feature (opt-in)

Enable by adding `#define CORE_MONITOR` to `myoptions.h`. Zero impact on binary when not defined.

When active, emits a `FUNCTIONLOG("Core Monitor", ...)` line to serial+telnet every 5 seconds:
- **Dual-core output**: `Core0(+Audio) loops/s: 82 (12.15ms/loop) | Core1(Main+Net+TCP+Disp) loops/s: 15327 (0.07ms/loop) | MaxMainLoopUs: 5197 | Heap: 163732`
  - The labels in parentheses are built at compile time via `CORE_0` / `CORE_1` string macros defined in `options.h`. Each macro concatenates component tokens (`+Audio`, `+Net`, `+TCP`, `+Disp`) conditioned on where `AUDIO_CORE`, `NETWORK_CORE`, `CONFIG_ASYNC_TCP_RUNNING_CORE`, and `DSP_TASK_CORE_ID` are assigned. These macros are only defined when both `CORE_MONITOR` and `!CONFIG_FREERTOS_UNICORE` are true.
- **Unicore C3 output**: `Core0 loops/5s: N (worst: N) | Core0(Main) loops/5s: N (worst: N) | MaxMainLoopUs: N | Heap: N` — `CORE_0`/`CORE_1` are not available on unicore; labels are static strings
- **Also shows SPIFFS information**: `Used: N / N bytes, Free: N bytes`

Implementation:
- `src/core/display.cpp`: `volatile uint32_t cmDspLoopCount` incremented each `loopDspTask` iteration
- `src/main.cpp`: `extern` reference to `cmDspLoopCount` + per-loop timing via `micros()`; worst-case counters are all-time minimums (never reset between windows)

---

## Hardware-Specific Notes (High-Risk Paths)

This section calls out hardware implementations that diverge from the common code path and are more likely to regress.

- Separate serial protocol parser and command emitter (`^...$` framed messages).
- Uses dedicated queue and task loop; does not behave like generic TFT/OLED widget drivers.
- Includes an explicit maintainer warning in file header about potential breakage.
- Risk notes:
  - page/component names are hardcoded strings, so HMI/editor changes can silently break firmware integration.
  - parser uses fixed-size buffers and repeated `sscanf` patterns; malformed serial payloads can cause fragile behavior.
  - parity drift risk vs WebUI commandhandler (settings logic duplicated in another channel).

## Audio backend variants (`src/core/player.cpp` + `src/libraries/*`)
- Two major audio stacks are used depending on macros/hardware:
  - I2S audio library path
  - VS1053 external decoder path
- `options.h` enforces that both are not enabled together.
- Risk notes:
  - behavior differences between backends (metadata timing, codec handling, volume behavior) can create env-specific bugs.
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

These are **not** third-party packages installable via PlatformIO's registry. They are custom or heavily-modified libraries embedded directly in the repository, mostly inherited from yoRadio and extended for ehRadio. Consult `src/libraries/libraries-note.md` for the origin, upstream source, and modification status of each library.

### Display driver libraries
- `Adafruit_GC9106Ex/` — GC9106 TFT driver (not a real Adafruit library; adapted from prenticedavid)
- `Adafruit_ST7796S/` — ST7796S TFT driver (same origin)
- `ILI9225Fix/` — ILI9225 TFT driver (heavily modified)
- `ILI9488/` — ILI9486/ILI9488 SPI driver (modified from ZinggJM)
- `LiquidCrystalI2C/` — I2C LCD driver (slightly modified from johnrickman)
- `SSD1322/` — SSD1322 OLED driver (slightly modified from JamesHagerman)
- `ST7920/` — ST7920 GLCD driver

### Audio decoder libraries
- `I2S_Audio/` — software I2S audio decoder (adapted from schreibfaul1/ESP32-audioI2S via Maleksm's yoRadio mod). PSRAM buffer size now configurable via `PSRAM_BUFSIZE` macro.
- `VS1053_Audio/` — VS1053 hardware decoder driver (adapted from schreibfaul1/ESP32-vs1053_ext via Maleksm's yoRadio mod). PSRAM buffer size now configurable via `PSRAM_BUFSIZE` macro. `stopSong()` SM_CANCEL sequence now guarded by `if(m_f_running)` — prevents permanently stuck CANCEL bit when stop is called during init with no song playing. `VS_PATCH_ENABLE` forced `false` on this hardware — FLAC patches produce audio silence on this VS1053 variant.
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
- Library `.cpp` files that reference project defines begin with `#include "../../core/options.h"` as the **first line** (before any `#if` guard), then gate all remaining includes and code behind the relevant `#if` condition (e.g., `#if DSP_MODEL==DSP_ST7920`, `#if defined(USE_AUDIO_I2S) || defined(USE_AUDIO_ESP32_DAC)`, `#if defined(USE_AUDIO_VS1053)`). This pattern is acceptable and intentional.
- Library `.h` files do not include `options.h`; they are self-contained and guarded with `#ifndef`/`#pragma once`.

---

## Locale and Translation Map (`src/locale`)

## `src/core/locale.h` (selector)
- Thin include wrapper: includes `dsplocale.h` and defines `WEBUI_LOCALE` from `DSP_LOCALE` if not overridden.
- `_activeLocale` runtime index (set from `config.store.locale_display`), `l10n()` / `l10n_dow()` / `l10n_month()` / `l10n_wind()` helpers.
- `l10n_findLocale()` resolves locale code to array index at runtime.

## Display locale files (`src/locale/display/*.json`)
- 36 JSON source files (one per locale), compiled via `make_dsplocale.py` into `dsplocale.h` PROGMEM mega-header.
- Master key set defined by `en_US.json` (67 keys: days, months, wind, weather, status labels).
- `static_assert` validates `DSP_LOCALE` at compile time against known locale codes.
- `dsplocale_index` PROGMEM string served at `/dsplocale.json` for WebUI dropdown.

## WebUI locale files (`src/locale/www/*.json`)
- 50 JSON source files, compiled via `make_wwwlocale.py` into `wwwlocale.h` PROGMEM (gzip-compressed byte arrays).
- Served from PROGMEM at `/locale.json` (with `Content-Encoding: gzip`) and `/wwwlocale.json` (index).
- No SPIFFS files needed — all locale data is compile-time embedded.

## Locale build tools
- `src/locale/make_dsplocale.py`: validates display JSONs, generates `dsplocale.h` with PROGMEM string tables + enum.
- `src/locale/make_wwwlocale.py`: validates webui JSONs, gzip-compresses into `wwwlocale.h` PROGMEM byte arrays.
- `src/locale/hardcode_locale_to_webui.py`: bake locale text into WebUI assets (for `HARDCODED_WEBUI_LOCALE`).

## Locale maintenance tools
- `src/locale/www_tool.py` (was `scan_www_check_json.py`): scan HTML/JS for i18n keys, check/add/translate/sort www locale JSONs. Supports `--create`.
- `src/locale/display_tool.py` (NEW): manage display JSONs against master. Sort uses master key order (never alphabetizes). Clean never touches master. Supports `--create`.
- `src/locale/trans_deepl.py` (was `scan_trans_deepl.py`): DeepL translation module. Uses `trans_*.key` discovery pattern (was `scan_trans_*.key`).
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
   - element in `data/www/settings.html` with id and `data-command`.
   - fallback label text + `data-i18n` key.
   - add i18n key in `src/locale/www/en_US.json` (and optionally others).
8. Ensure websocket UI apply path exists in `data/www/script.js`:
   - `setupElement(...)` supports element type/id.
   - incoming `GET*` payload key matches DOM element id or custom handler.
9. If setting is locale/time/weather related, update `data/www/settings.js` apply handlers too.
10. Telnet command handling is thin-dispatch by default: update `src/core/commandhandler.cpp` first, and only extend `src/core/telnet.cpp` if protocol normalization needs a new alias/form.
11. If setting affects startup behavior, check `main.cpp`, `config.init()`, and `startup.startupServices()`.
12. Update this `code-summary.md`.
13. Update `Commands.md`.

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
- MQTT: normalized payload parser -> `commandhandler.cpp` (with source blocklist)
- Physical controls: `controls.cpp`

Implication:
- For universal control behavior, update `commandhandler.cpp` first; then adjust only channel-specific parser aliases/blocklists.

## Matrix C: Playlist actions

- Edit/import in browser -> upload to `/webboard` -> SPIFFS write
- `netserver` triggers `PLAYLISTSAVED`
- `config.indexPlaylist()/initPlaylist()` refresh index
- `player` and display refresh via request queue events

---

## Telnet Section Interactions (Explicit)

`telnet.cpp` primarily acts as a command ingress path now. Most command behavior is owned by `commandhandler.cpp` and shared with MQTT/HTTP paths.

Input-line handling is delimiter-based (`\r` or `\n`) with explicit 2000 ms timeouts, which avoids delayed command execution on clients that submit CR without LF.

If you add a setting command in `commandhandler.cpp`, telnet and MQTT generally inherit it automatically unless blocked by the shared HTTP/MQTT/Telnet non-WebUI policy.

---

> Tracked issues and risk notes are in `.github/code-issues.md`.


