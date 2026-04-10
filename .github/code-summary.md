# ehRadio Code Summary (Human + AI Operational Map)

## Mandatory Maintenance Directive

If any code change affects behavior, data flow, build configuration, platform targets, WebUI functionality, stored settings, networking, display logic, localization, update flow, or dependency usage, this file **must** be updated in the same change set.

This file exists to reduce re-analysis cost for humans and AI agents.

It will get lengthy as more is added to it.

---

## Scope and Intent

This document is intentionally per-file focused for:
- `src/main.cpp`
- `src/core/*`
- `src/pluginsManager/*`
- `data/www/*`
- `src/locale/*`

Grouped (not one-by-one deep explained) areas:
- `src/displays/display*.cpp/.h` (drivers follow similar shape)
- `src/displays/conf/*.h` (widget placement/config pattern files)
- `src/displays/fonts/*` (font assets)

---

## Fast Architecture Overview

### Build-time chain
- `platformio.ini` selects environment, included libraries, and included source files.
- `myoptions.h` selects hardware profile + defaults.
- `src/core/options.h` resolves all defaults/fallbacks and feature flags.
- `.github/workflows/build-release-firmware.yml` verifies generated contributor release artifacts and now diff-checks `web_assets/` in addition to `firmware.txt` and `releases.md`.
- `.github/workflows/build-deploy-page.yml` deploys Pages on `release.published`, and on branch/manual runs it preserves the currently published `firmware-info.json` and manifests instead of overwriting them from `builds/*/web_assets`.

### Runtime chain
- `src/main.cpp` bootstraps system: config -> display -> player -> network -> server/telnet/controls.
- WebUI/WebSocket input path: `netserver` -> `commandhandler` -> `config/player/display/network`.
- State output path: `requestOnChange(...)` in `netserver` -> WebSocket JSON to browser.
- Settings persistence path: `config.saveValue(...)` -> ESP Preferences namespace `"ehradio"`.

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
- Includes `myoptions.h`, `mytheme.h`, `mqttoptions.h` when present.
- **Owns all compile-time guardrails** inline, right next to each respective define.
- Defines:
  - hardware defaults (pins, feature gates)
  - updater URLs (`FILESURL`, `UPDATEURL`, `CHECKUPDATEURL`) unless disabled
  - weather defaults and thresholds
  - battery defaults/curve/thresholds
  - WebUI and localization defaults
  - curated list defaults
  - **Exception**: locale and language options are handled by locale.h
- **Guardrail conventions** (maintain these when adding new options):
  - `#error` for hard-invalid values: wrong board type, mutually exclusive decoders, enum/constant out of range (e.g. `TS_MODEL`, `RTC_MODULE`), bad logical cross-constraints (e.g. `BTN_PRESS_TICKS <= BTN_CLICK_TICKS`, `BATTERY_CRITICAL_THRESHOLD >= BATTERY_LOW_THRESHOLD`).
  - `#warning` + `#undef` for out-of-range tunables in `/* USER DEFAULTS */` section (e.g. `SOUND_VOLUME`, `SCREEN_BRIGHTNESS`): reverts silently to default but now emits a visible warning in the build log.
  - `static_assert` with `__builtin_strcmp` for enumerated string options (e.g. `WEATHER_API`, `WEATHER_WIND_SPEED_UNITS`). **Update the `static_assert` whenever a new provider/value is added.**
  - `/* PREVENT BOARD-DEFINED PIN RE-USE */` section lives after the `/* ESP DEVBOARD */` LED block (requires `REAL_LEDBUILTIN` and `ESP_S3C3` to be defined first). Covers LED vs RST pin conflicts only — keep it narrowly scoped.
- **What is intentionally NOT guarded**: booleans (compiler error is obvious), pin numbers (board-dependent range), free-form strings (`AP_SSID`, `MQTT_*`, URLs), color macros (R,G,B triplets), `AUTOBACKLIGHT(x)` (C macro function), `BATTERY_CURVE_MV/PCT` (already has `static_assert` in `battery.cpp`).

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
  - Nextion is a special path (`src/displays/nextion.cpp`) with its own command protocol and UI assumptions.
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
  2. plugin setup hooks (`pm.on_setup()`)
  3. `config.init()`
  4. `display.init()`
  5. `player.init()`
  6. `network.begin()`
  7. if no connectivity: start minimal server + controls + display start and return
  8. if connectivity:
     - `config.initPlaylistMode()`
     - `netserver.begin()`
     - `telnet.begin()`
     - controls init
     - display start
     - optional MQTT init
     - optional smart-start playback
     - `config.startupServices()`
     - plugin end-setup hook
    - `netserver.setBootReady(true)` only after setup work is actually complete
- `loop()`:
  - AP mode: Improv + captive DNS
  - normal: telnet loop
  - RGB loop + battery loop + battery dim/sleep policy
  - player loop (connected/SD ready)
  - controls loop
  - netserver loop

---

## Core Folder Per-File Map (`src/core`)

## `src/core/common.h`
- Shared enums and structs used across modules (display modes, requests, control events, etc.).
- Coupling:
  - Imported by display, controls, plugins manager, and command paths.

## `src/core/options.h`
- See earlier build section.

## `src/core/config.h`
- Defines persistent struct `config_t store`.
- Defines station/theme structs and config API.
- Defines key constants for SPIFFS paths and data file locations.
- `Config::keyMap` declaration controls Preferences key mapping.
- `Config::saveValue(...)` API now has two simple overloads only:
  - typed: `saveValue(T* field, const T& value)`
  - string: `saveValue(char* field, const char* value)`
- Legacy compatibility parameters (`commit`, `force`, and string `size_t N`) were removed.
- String saves now normalize into a zero-filled fixed-size buffer before compare/write to avoid reading beyond short source strings.
- Both overloads share a single internal write-if-changed path (`missing key` OR `size mismatch` OR `content changed`) before calling `prefs.putBytes(...)`.

## `src/core/config.cpp`
- Startup/storage/file-integrity center.
- Main responsibilities:
  - load and validate Preferences (`cfgset` marker)
  - defaults/init logic
  - SPIFFS mount and required file checks
  - version marker management (`/data/ehradio.ver`)
  - playlist indexing and station loading
  - locale/update helper routines
  - startup update services and online file maintenance
  - reset section handlers (`defaultSettings(...)`)
- SD-specific behavior:
  - `_initHW()` configures `SD_CARD_DETECT_PIN` as `INPUT_PULLUP` when available
  - `initPlaylistMode()` short-circuits to `PM_WEB` without calling `sdman.start()` when `SD_CARD_DETECT_PIN` reports slot-empty during SD boot
  - `changeMode()` short-circuits SD mode switches the same way, avoiding the SPI retry path when the slot is empty
- Key interaction:
  - almost every module reads/writes through `config`.

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
- Coupling:
  - pushes display updates (`display.putRequest(...)`)
  - calls player/netserver hooks
  - reads/writes `config.store`

## `src/core/player.h` / `player.cpp`
- `player.h` declares player command queue, playback API, and status.
- Audio engine integration and playback sequencing.
- Main responsibilities:
  - initialize codec/I2S/VS1053
  - queue command handling (`PR_PLAY`, `PR_STOP`, `PR_VOL`, etc.)
  - station play/stop/toggle/next/prev flow
  - volume conversion (`volToI2S`) including ES8311 path
  - SD/web mode specific playback behavior
  - error reporting and display/net updates
- Coupling:
  - updates display queue and websocket state
  - uses `config` station and mode state
  - interacts with radio-browser click reporting

## `src/core/audiohandlers.h`
- Callback bridge used by audio library.
- Converts decoder callbacks into:
  - metadata updates
  - title/station updates
  - bitrate/codec updates
  - error updates
  - SD EOF behavior
- Important for title/bitrate side effects to WebUI and display.

## `src/core/display.h` / `display.cpp`
- `display.h` declares Display class and display mode/change API.
- Render queue + display task + widget/page orchestration.
- Main responsibilities:
  - initialize rendering task and widgets
  - mode switching (`PLAYER`, `VOL`, `STATIONS`, `LOST`, `UPDATING`, screensaver)
  - draw station/title/weather/clock/VU/bitrate/playlist
  - update progress bar during update flow
  - battery indicator rendering
- Coupling:
  - depends on `config.store` for many visual toggles
  - reads `network` time/weather, `player` status

## `src/core/netserver.h`
- Declares request enums, websocket/server globals, and NetServer API.
- Contains embedded fallback HTML templates (`emptyfs_html`, `index_html`, `emergency_form`).

## `src/core/netserver.cpp`
- HTTP + WebSocket + upload/update + search/curated orchestration.
- Main responsibilities:
  - static file serving from SPIFFS `/www`
  - route handlers (`/`, `/search`, `/update`, `/locale.json`, `/ready`, etc.)
  - websocket command parsing and outbound updates
  - state request queue processing (`GETSYSTEM`, `GETSCREEN`, `GETLOCALE`, etc.)
  - online update check/start tasks
  - radio-browser search and curated task management
  - playback launch helper for preview URL
- Coupling:
  - uses `cmd.exec(...)` from commandhandler
  - emits JSON consumed by `data/www/script.js`
- Readiness detail:
  - `/ready` returns `{"ready":true}` only when `netserver.bootReady` is true, required web files exist, and network state is stable (`CONNECTED` + `WL_CONNECTED`, or `SDREADY`).

## `src/core/commandhandler.h` / `commandhandler.cpp`
- `commandhandler.h` declares command execution API for command strings.
- Central command router for WS and URL command paths.
- Main responsibilities:
  - map `key=value` commands into config/player/display/network actions
  - request websocket state snapshots
  - persist settings with `config.saveValue(...)`
  - trigger curated operations and locale update tasks
- Critical coupling file for setting changes.

## `src/core/controls.h` / `controls.cpp`
- `controls.h` declares controls init/loop and helper functions for encoders/IR/touch.
- Physical controls integration:
  - OneButton
  - rotary encoders
  - touchscreen gestures
  - IR remote decoding
- Converts hardware input events into same core actions used by WebUI (`controlsEvent`, player commands, display mode changes).

## `src/core/telnet.h` / `telnet.cpp`
- Telnet and serial command handling.
- Responsibilities:
  - manage client sessions
  - parse command strings for playback/system/wifi/time/battery
  - direct integration with config/player/network
- Important:
  - acts as secondary control channel parallel to WebUI
  - overlaps command surface with commandhandler but is separate code path

## `src/core/mqtt.h` / `mqtt.cpp`
- MQTT integration if `MQTT_ENABLE` compile flag exists.
- Responsibilities:
  - connection lifecycle
  - subscribe to `.../command`
  - publish status/playlist/volume
  - parse command payload into player/system actions
- Coupling:
  - mirrors subset of telnet/commandhandler behavior.

## `src/core/battery.h` / `battery.cpp`
- `battery.h` declares battery status struct and API.
- Battery monitoring/calibration/inference implementation.
- Responsibilities:
  - ADC sampling and filtering
  - battery presence detection
  - charge/discharge inference with candidate windows
  - threshold state (`low`, `critical`) tracking
  - status formatting for telnet/WebUI
  - triggers display and websocket updates

## `src/core/rgbled.h` / `rgbled.cpp`
- Optional RGB LED state machine:
  - playing/stopped colors
  - track-change flashing
  - optional cycle behavior

## `src/core/sdmanager.h` / `sdmanager.cpp`
- `sdmanager.h` declares SD manager API and FS integration wrapper.
- SD lifecycle and SD playlist indexing.
- Responsibilities:
  - mount/retry/unmount — `start()` attempts up to 4 `SD.begin()` calls, early-returning on success (delays only between retries, not after success)
  - card-present checks
  - recursive scan and media file playlist/index creation
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

## `src/core/rtcsupport.h` / `rtcsupport.cpp`
- RTC init/get/set wrappers for DS3231/DS1307 when configured; `rtcsupport.h` has compile guards.

---

## Plugins Manager (`src/pluginsManager`)

## `src/pluginsManager/pluginsManager.h` / `pluginsManager.cpp`
- `pluginsManager.h` defines plugin base class and hook surface (setup/start/stop/ticker/button/display queue events).
- `pluginsManager.cpp` registers plugins and dispatches hook calls.

Notes:
- Current workspace has no `src/plugins/*` plugin implementation files.
- Hook points are still used by core code (`pm.on_*` calls).

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
  - mDNS rename (`rebootmdns`) now reuses the same reboot status screen as a normal reboot, then uses browser-side ready polling against the new `.local` host; backend no longer sends WebSocket redirect JSON before reboot.

## `data/www/options.js`
- Settings page behavior.
- Responsibilities:
  - timezone JSON loading and dropdown population
  - locale list loading and locale switch logic
  - weather provider field visibility logic
  - apply handlers for locale/weather/mqtt/wifi
  - reboot/reset/format status screen with per-action behavior:
    - reboot/reset use ready-aware return-to-root with shorter fallback (15s)
    - format SPIFFS shows reboot status but skips automatic reload

## `data/www/locale.js`
- i18n runtime helper (`t(...)`) and translation application (`applyI18n`).
- Applies key-based translations to DOM and fallback behavior.

## `data/www/player.html`
- Player page structure (playlist, controls, sliders, status elements).

## `data/www/options.html`
- Settings page structure with grouped sections and `data-command` bindings.
- Contains element IDs expected by websocket payload mapping.

## `data/www/updform.html`
- Update page layout (manual upload + online update controls).

## `data/www/irrecord.html`
- IR recording and assignment UI.

## `data/www/search.html`
- Search UI for radio-browser integration.

## `data/www/curated.html`
- Curated list browsing/import page.

## `data/www/dragpl.js`
- Playlist drag-and-drop reorder behavior.

## `data/www/script2.js`
- Consolidated helper script loaded by main shell and standalone search/curated pages.
- Contains logic previously in `ir.js`, `updform.js`, and `playstation.js`:
  - station preview/play helper (`sendStationAction`)
  - online update check/start UI helpers
  - IR setup/learn interactions (`initControls`, `checkSelect`, `irClear`, `backRecord`)

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
- `src/displays/widgets/pages.h`, `pages.cpp`
  - page and pager composition framework.
- `src/displays/nextion.h`, `nextion.cpp`
  - Nextion display integration path.

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

## Display tools
- `src/displays/tools/utf8To.*`
- `src/displays/tools/utf8_common.*`
- `src/displays/tools/utf8Latin.*`
- `src/displays/tools/utf8Cyrillic.*`
- `src/displays/tools/commongfx.h`
- `src/displays/tools/psframebuffer.h`
- `src/displays/tools/oledcolorfix.h`
- `src/displays/tools/tftinverttitle.h`

Purpose:
- text normalization/transliteration and glyph handling
- display utility support

## Display fonts/assets
- `src/displays/fonts/*` for boot logos and digit/font assets.

---

## Hardware-Specific Notes (High-Risk Paths)

This section calls out hardware implementations that diverge from the common code path and are more likely to regress.

## Nextion (`src/displays/nextion.cpp/.h`)
- Separate serial protocol parser and command emitter (`^...$` framed messages).
- Uses dedicated queue and task loop; does not behave like generic TFT/OLED widget drivers.
- Has direct config mutation paths (volume/EQ/timezone/wifi writes) from Nextion events.
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
- `I2S_Audio/` — software I2S audio decoder (adapted from schreibfaul1/ESP32-audioI2S via Maleksm's yoRadio mod)
- `VS1053_Audio/` — VS1053 hardware decoder driver (adapted from schreibfaul1/ESP32-vs1053_ext via Maleksm's yoRadio mod)
- `ES8311_Audio/` — ES8311 codec driver (written for ehRadio by kasperaitis)

### Touchscreen library
- `FT6336_Touchscreen/` — FT6336 capacitive touch driver (written for ehRadio by kasperaitis)

### Include conventions in library files
- Library `.cpp` files that reference project defines begin with `#include "../../core/options.h"` as the **first line** (before any `#if` guard), then gate all remaining includes and code behind the relevant `#if` condition (e.g., `#if DSP_MODEL==DSP_ST7920`, `#if I2S_DOUT!=255 || I2S_INTERNAL`, `#if VS1053_CS!=255`). This pattern is acceptable and intentional.
- Library `.h` files do not include `options.h`; they are self-contained and guarded with `#ifndef`/`#pragma once`.

---

## Locale and Translation Map (`src/locale`)

## `src/core/locale.h` (selector)
- compile-time locale selection and weather language fallback mapping.
- `WEBUI_LOCALE` default behavior and hardcoded locale fallback constants.

## Display locale files (`src/locale/displayL10n_*.h`)
- one file per display locale string set.
- used via `LANG::` namespace throughout display/system text.

## WebUI locale files (`src/locale/webui/*.json`)
- one JSON per WebUI language.
- consumed by `data/www/locale.js`.

## Locale docs/scripts
- `src/locale/l10n.md`: locale listing matrix.
- `src/locale/localization-guide.md`: full localization/font pipeline doc.
- `src/locale/make_data_www_locales_json.py`: regenerate `locales.json`.
- `src/locale/scan_www_check_json.py`: check i18n key consistency.
- `src/locale/hardcode_locale_to_webui.py`: bake locale text into WebUI assets.
- `src/locale/scan_trans_deepl.py` and notes: translation helper.

## GLCD font files
- `src/locale/glcdfont/glcdfont_Latin.c` / `glcdfont_Cyrillic.c` / docs.
- `glcdfont-lib.c` for base/reference.
- glyph tool scripts under `src/locale/glcdfont/glyph_scripts`.

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
   - add i18n key in `src/locale/webui/en_US.json` (and optionally others).
8. Ensure websocket UI apply path exists in `data/www/script.js`:
   - `setupElement(...)` supports element type/id.
   - incoming `GET*` payload key matches DOM element id or custom handler.
9. If setting is locale/time/weather related, update `data/www/options.js` apply handlers too.
10. If setting should be visible in telnet or CLI flows, update `src/core/telnet.cpp`.
11. If setting affects startup behavior, check `main.cpp` and `config.init()/startupServices`.
12. Update this `code-summary.md`.

## When removing a setting field

1. Remove/disable command usage in `commandhandler.cpp`.
2. Remove from `GET*` payload in `netserver.cpp`.
3. Remove UI controls and JS references.
4. Remove from `config_t` + `keyMap`.
5. Add removed key to `Config::deleteOldKeys()` if old persisted value should be cleaned.
6. Remove locale keys from `src/locale/webui/en_US.json` (and regenerate/check).
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
- Telnet/serial: `telnet.cpp` custom parser
- MQTT: `mqtt.cpp` command payload parser
- Physical controls: `controls.cpp`

Implication:
- If behavior should be universal, update all relevant channels.

## Matrix C: Playlist actions

- Edit/import in browser -> upload to `/webboard` -> SPIFFS write
- `netserver` triggers `PLAYLISTSAVED`
- `config.indexPlaylist()/initPlaylist()` refresh index
- `player` and display refresh via request queue events

---

## Telnet Section Interactions (Explicit)

`telnet.cpp` is not just diagnostics; it can mutate runtime config and behavior:
- playback control (`play`, `stop`, `vol`, `next`, `prev`)
- time and timezone commands (`tzo`, `tzposix`)
- wifi management commands
- battery calibration commands (`calbatt`)
- reset/reboot operations

If you add new setting logic only to WebUI and want CLI parity, you must add matching telnet handling.

---

> Tracked issues and risk notes are in `.github/code-issues.md`.


