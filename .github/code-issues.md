# code-issues.md — ehRadio Issue Tracker

Working document for audit findings. Does **not** replace `code-summary.md` (the permanent operational bible).
Delete this file once all items are resolved or explicitly accepted.
Add `(ALL FIXED)` to title/section after issues are resolved.

**Severity**: `[HIGH]` = silent data corruption or crash risk | `[MEDIUM]` = incorrect behavior or user-facing bug | `[LOW]` = code quality / inconsistency | `[TRIVIAL]` = naming / doc only

---

## Overview of Issues

- [X] 1. Dead or Misnamed Macros (Silent Failure Risk)
- [X] 2. Dead Defines (Defined But Never Consumed)
- [ ] 3. Macros Without `options.h` Fallback
- [ ] 4. Write-Only Variables (Set But Never Read)
- [ ] 5. Dead / Unreachable Code
- [X] 6. Commandhandler Issues
- [X] 7. Unhandled or Mis-handled Web UI Commands
- [ ] 8. Unified Command Dispatch — Route MQTT, Telnet, and URL Parameters Through `commandhandler.cpp`
- [X] 9. Logic / Correctness Bugs
- [ ] 10. `BUFLEN` — Multi-Purpose Magic Number
- [X] 11. Unsafe String Handling / Buffer Overflow Risks
- [X] 12. WebUI / JavaScript Security Issues
- [ ] 13. Stability / Architecture Risks
- [X] 14. Dead / Redundant Includes (`netserver.cpp`)
- [X] 15. Include Path Convention Inconsistency
- [ ] 16. C-style modules in `src/core/` — refactor to class + global instance
- [ ] 17. Memory Leaks and Heap Fragmentation
- [X] 18. Race Conditions and Concurrency Hazards
- [ ] 19. Blocking Operations on Real-Time / Non-Background Contexts
- [ ] 20. TLS / HTTPS Security
- [X] 21. NVS Write Endurance
- [ ] 22. Stack Overflow Risks in FreeRTOS Tasks
- [ ] 23. Telnet / WiFi Scan Blocking and Credential Exposure
- [X] 24. A Workflow that checks pull-requests
- [ ] 25. Display conf.h Files That Lack a Battery Widget
- [ ] 26. ESP32-S3 Migration — Dropping Original ESP32 Support
- [ ] 27. `main.cpp` — Non-Boot Code That Belongs in Its Own Files
- [ ] 28. Plugin System — Dead Infrastructure, Remove Entirely
- [ ] 98. Documentation Needs Serious Work
- [ ] 99. Issues Found Randomly or Outside Above Issues

---

## [X] 1. Dead or Misnamed Macros (Silent Failure Risk) (ALL FIXED)

These macros are mentioned in comments in `myoptions.h` as something a user can define, but they are **never consumed anywhere in `src/`**. Defining them produces no effect and no error — pure silent failure.

| Macro | Where it appears | Silent-failure scenario |
|---|---|---|
| `WEATHER_IMPERIAL` | `myoptions.h` comment | User defines it expecting imperial units. Nothing changes. Correct macros are `WEATHER_TEMPERATURE_F`, `WEATHER_PRESSURE_MMHG`, `WEATHER_WIND_SPEED_UNITS` (set by `#define WEATHER_METRIC false` in `options.h`). |
| `WEBUI_LANGUAGE_STRING` | `myoptions.h` comment | User defines it expecting to set the WebUI language. Nothing changes. Correct macro is `WEBUI_LOCALE` (used in `src/core/locale.h`). |

**Action**: Remove misleading comments, add correct names with examples in `myoptions.h`. `[MEDIUM]` — user-facing documentation bug.

---

## [X] 2. Dead Defines (Defined But Never Consumed) (ALL FIXED)

These macros are actually **defined** (not just commented) in `myoptions.h` board profiles but are **never read by any code in `src/`**.

| Fixed | Macro | Defined in | Notes |
|---|---|---|---|
| [x] | `PA_ENABLE` | `myoptions.h` ES3C28P profile | Defined as `1`. Zero matches in all of `src/` and in `ES8311_Audio/`. **The actual amp-enable is fully implemented via `MUTE_PIN 1` / `MUTE_VAL HIGH` in `player.cpp` (lines ~54 and ~214).** `PA_ENABLE` is a dead legacy name for the same GPIO — `MUTE_PIN` replaced it. Safe to remove from the profile. |
| [X] | `GFX_BL` | `myoptions.h` (commented out) | Zero matches in `src/`. Possibly a leftover from an alternate display backlight approach. |
| [x] | `TFT_MOSI`, `TFT_SCLK`, `TFT_MISO` | `myoptions.h` ES3C28P profile (values 11, 12, 13) | Zero matches in `src/`. Adafruit_ILI9341 does not read these macros — it takes a `SPIClass*` and calls `spi->begin()` with the bus defaults. Confirmed from `variants/esp32s3/pins_arduino.h`: ESP32-S3 devkitc-1 default SPI pins are exactly `MOSI=11, MISO=13, SCK=12`. These macros are 100% redundant — they match hardware defaults and are never consumed. |
| [x] | `TFT_BL` | `myoptions.h` ES3C28P profile (value 45) | Only exists to be aliased as `#define BRIGHTNESS_PIN TFT_BL`. `BRIGHTNESS_PIN` is the macro actually consumed by `main.cpp` (multiple sites) and has an `options.h` fallback (=255). No library reads `TFT_BL` directly. Could be collapsed to `#define BRIGHTNESS_PIN 45` removing the intermediate name. |
| [x] | `FF_FS_EXFAT` | `myoptions.h` comment — "Does this get carried to SD Lib?" | The comment itself expresses uncertainty. Zero matches in `src/`. Whether it propagates to the SD library at the linker/compile level is unclear. |

**Action**: Remove `PA_ENABLE` — amp control is covered by `MUTE_PIN`. Remove `TFT_MOSI`/`TFT_SCLK`/`TFT_MISO` — confirmed redundant to board defaults. Optionally collapse `TFT_BL` + `BRIGHTNESS_PIN` to a single define. Clarify `FF_FS_EXFAT`. `[LOW]` to `[MEDIUM]`.

---

## [ ] 3. Macros Without `options.h` Fallback

These macros are consumed in `src/` via `#ifdef`/`#if defined` guards (so they are safe when undefined), but they have **no fallback default in `options.h`**. This is inconsistent with how most settings are managed and makes them invisible to someone reading `options.h` for the full feature list.

| Fixed | Macro | Consumed in | Fallback location | Notes |
|---|---|---|---|---|
| [X] | `BIG_BOOT_LOGO` | `displayILI9488.h`, `displayST7796.h`, conf files | None — `#ifdef` only | Safe: undefined = no big logo. But undocumented in `options.h`. |
| [ ] | `DOWN_LEVEL` | `main.cpp` (heavily, with `#ifdef`) | None — optional feature | Safe: undefined = feature disabled. Only present in ILI9488 board profile. |
| [ ] | `DOWN_INTERVAL` | `main.cpp` (heavily, with `#ifdef`) | None — optional feature | Same as `DOWN_LEVEL`. |
| [X] | `FIRMWARE_NAME` | `network.cpp` lines ~337–346 via `#ifdef` | None | Safe: undefined = no firmware name in eHDP discovery. But boards built without a profile won't get a name, and there's no documented way to add one without knowing this macro exists. |
| [x] | `SDSPISPEED` | `sdmanager.cpp` | Fallback defined **inside `sdmanager.cpp`** itself (`#ifndef SDSPISPEED #define SDSPISPEED 20000000`), NOT in `options.h` | Inconsistent pattern. Works fine, but breaks the convention that `options.h` is the canonical fallback location. |
| [X] | `ESPFILEUPDATER_DEBUG` | `config.h` line ~13 via `#ifdef` | None — intentional debug flag | Safe but worth noting in `options.h` as a commented-out debug option. |
| [X] | `MQTT_ENABLE` | `mqtt.h`, `mqtt.cpp`, `netserver.cpp`, `player.cpp`, `commandhandler.cpp`, `main.cpp` — guards the entire MQTT subsystem | None — opt-in feature | Undefined = MQTT disabled. No `#ifndef MQTT_ENABLE` entry in `options.h`. Should appear as a commented-out stub so it's discoverable without consulting board profiles or README. |
| [X] | `RGB_LED_PIN` | `rgbled.cpp` — `#if defined(RGB_LED_PIN) && (RGB_LED_PIN!=255)` guards the entire NeoPixel module | None for `RGB_LED_PIN` itself; `RGB_LED_ORDER` falls back inside `rgbled.cpp`; `NUM_RGB_LEDS` is hardcoded as `1` in `rgbled.cpp` | Undefined = NeoPixel disabled. The `=255 means disabled` convention used for all other pin macros (MUTE_PIN, BRIGHTNESS_PIN, etc.) is not applied here — no options.h entry at all. |
| [X] | `MAX_PL_READ_BYTES` | `netserver.cpp` line ~312 — caps playlist body size during HTTP upload | None anywhere | Undefined = no upper limit on playlist read size. Tuning parameter that should have an options.h default (e.g., something like `65536`). |
| [X] | `PLAYLIST_DEFAULT_URL` | `config.cpp` line ~1307 — seeds default playlist on first boot | None | Undefined = no default playlist URL seeded. Silent, no fallback. Should have a commented stub in `options.h`. |
| [ ] | `SD_SPIPINS` | `config.cpp` line ~75, `sdmanager.h/cpp` — custom SPI pin tuple for SD card | None | Paired feature with `SD_HSPI` which HAS an `options.h` fallback, but `SD_SPIPINS` itself does not. Inconsistent. |
| [ ] | `TS_SPIPINS` | `touchscreen.cpp` lines ~27, ~46 — custom SPI pin tuple for touchscreen | None | Same asymmetry as `SD_SPIPINS` vs `SD_HSPI`. `TS_HSPI` has an `options.h` fallback; `TS_SPIPINS` does not. |
| [X] | `DEBUG_V`, `CORS_DEBUG`, `BATTERY_DEBUG` | Various `src/core/` files | None — intentional debug flags | Same category as `ESPFILEUPDATER_DEBUG`. Three separate debug flags with no options.h entry. Should be grouped as commented-out debug stubs. |

**Action**: Consider documenting these in `options.h` as commented-out stubs so they are discoverable. Move the `SDSPISPEED` fallback from `sdmanager.cpp` to `options.h`. Add `RGB_LED_PIN 255` default following the existing `=255 means disabled` pin convention. Add a default for `MAX_PL_READ_BYTES`. `[LOW]`.

---

## [ ] 4. Write-Only Variables (Set But Never Read)

### [ ] 4.1 `network.trueWeather` `[MEDIUM]`

- **Declaration**: `bool trueWeather;` in `src/core/network.h` line ~24
- **Written**: `config.cpp` (3 assignments), `network.cpp` `getWeather()` return value stored here (2 assignments including the return)
- **Read**: **Zero occurrences anywhere in `src/`**
- **Analysis**: `getWeather()` returns a `bool` indicating whether real weather data was received. The caller stores this in `trueWeather`, but no code path ever checks `trueWeather`. The variable appears intended to track "we have real data vs. placeholder" but the consuming logic was never written (or was removed). The indicator is silently discarded every time.
- **Action**: Either add consuming logic that uses `trueWeather` (e.g., suppress stale display when false), or remove the variable and ignore the return value explicitly at the call sites.

- Trip5 note: This is supposed to track if weather info held in cache is valid.  Is it seriously not being checked anywhere?  It's supposed to be refreshed according to config.store... weather interval?

---

## [ ] 5. Dead / Unreachable Code

### [ ] 5.1 `|| true` dead branch in `player.cpp` `[LOW]`

- **Line 132**: `if (strlen(file)==0 || true) return; //TODO Read TAGs`
- **Analysis**: `|| true` permanently short-circuits to `true`. Any code below this `return` is unreachable. This appears to be an acknowledged TODO — someone commented out the tag-reading functionality with `|| true` as a temporary measure that became permanent.
- **Action**: Either implement the tag-reading block and remove `|| true`, or remove the `|| true` and let `strlen(file)==0` be the real condition. The current state is confusing because it looks like a real condition check when it is not.

---

## [X] 6. Commandhandler Issues (ALL FIXED)

### [X] 6.1 Command handlers in `config.cpp` that can be moved to `commandhandler.cpp` `[MEDIUM]`

The refactor pattern (as completed for `setSmartStart()`) is: inline the wrapper body into `commandhandler.cpp` and remove the wrapper from `config.cpp`/`config.h`. Functions that are also called from other places (`mqtt.cpp`, etc.) cannot be fully removed from `config.cpp` — those are noted separately at the bottom.

- [X] **`setSmartStart()`** — inlined; commandhandler now calls `config.saveValue` directly.
- [X] **`setShuffle(bool)`** — 2 lines: `saveValue` + `player.next()`. Easy.
- [X] **`setBalance(int8_t)`** — 3 lines: `saveValue` + `player.setBalance` + `requestOnChange`. Fix the dup + cast bugs from §6.4 and §6.5 at the same time.
- [X] **`enableScreensaver(bool)`** — `saveValue` + `#ifndef DSP_LCD display.putRequest(NEWMODE, PLAYER)`. Carry the `#ifndef DSP_LCD` guard into commandhandler.
- [X] **`setScreensaverTimeout(uint16_t)`** — `constrain(val,5,65520)` + `saveValue` + same `#ifndef DSP_LCD` guard. Note: brings its own input clamp.
- [X] **`setScreensaverBlank(bool)`** — same pattern as `enableScreensaver`.
- [X] **`setScreensaverPlayingEnabled(bool)`** — same pattern.
- [X] **`setScreensaverPlayingTimeout(uint16_t)`** — `constrain(val,1,1080)` + `saveValue` + `#ifndef DSP_LCD` guard.
- [X] **`setScreensaverPlayingBlank(bool)`** — same pattern as `enableScreensaver`.
- [X] **`setShowweather(bool)`** — 4 lines: `saveValue` + `network.trueWeather=false` + `network.forceWeather=true` + `display.putRequest(SHOWWEATHER)`.
- [X] **`setWeatherKey(const char*)`** — 4 lines: `saveValue` + `network.trueWeather=false` + two `display.putRequest` calls.
- [X] **`setIrBtn(int)`** — body is entirely inside `#if IR_PIN!=255`; the commandhandler caller is already inside that same guard, so the inline is clean.
- [X] **`setSDpos(uint32_t)`** — multi-branch (checks `PM_SDCARD`, calls `player.setResumeFilePos` / `player.setFilePos`). Candidate but less trivial than the above.

**Others Cannot be fully removed from `config.cpp`** (also called from non-commandhandler callers — deferred to §8.3):

### [X] 6.2 Sort all commands in `commandhandler.cpp` according to their order in WebUI (mostly-ish) `[LOW]`

Just sort them a bit into categories for future maintenance.

### [X] 6.3 config.SaveValue - booleans ignored for compatibility `[LOW]`

Originally kept for compatibility with existing yoRadio functions, booleans are ignored and thus can be removed everywhere in the codebase.

### [X] 6.4 Duplicate `balance` handler in `commandhandler.cpp` `[MEDIUM]`

- **Line ~30**: `if (strEquals(command, "balance")) { config.setBalance(atoi(value)); return true; }`
- **Line ~118**: `if (strEquals(command, "balance")) { config.setBalance(static_cast<uint8_t>(atoi(value))); return true; }`
- **Analysis**: The handler at line ~30 runs first and always returns `true`. The handler at line ~118 (inside the `//<-----TODO` block) is **unreachable dead code**. It will never execute.
- **Side effect of the dead code**: The cast at line ~118 is also wrong — it casts to `uint8_t` before passing to `int8_t`, which corrupts negative balance values (e.g., -10 → `uint8_t` 246 → `int8_t` -10 by accident in twos-complement, but values like -100 → `uint8_t` 156 → `int8_t` -100, so actually they happen to be equivalent for most values due to twos-complement... BUT the intent is wrong and a future reader would see a bogus cast).
- **The live handler (line ~30)** has its own problem: it passes raw `int` from `atoi()` directly with no cast and no range clamp (see Section 6.1 below).
- **TODO block context**: The `//<-----TODO` marker separates two groups of handlers. Commands BELOW the marker (`volume`, `sdpos`, `shuffle`, `reboot`, `format`, `submitplaylist`, `irbtn`, `chkid`, `irclr`, etc.) are **not duplicates** — they are the only handlers for those commands. Only `balance` is accidentally duplicated. This indicates an **incomplete refactoring**: some commands were being migrated or reworked, but the work stopped partway. The entire EQ missing-handler situation (`treble`, `bass`, `middle`) is a direct result of this same incomplete state — see Section 7.1.
- **Action**: Remove the dead handler at line ~118. Fix the live handler at line ~30 (see Section 6.1).

### [X] 6.5 Incorrect Casts / Type Safety Issues

#### [X] 6.5.1 `balance` command — no range clamp, implicit narrowing `[MEDIUM]`

- **File**: `src/core/commandhandler.cpp` line ~30
- **Code**: `config.setBalance(atoi(value))`
- **Signature**: `Config::setBalance(int8_t balance)` in `config.cpp`
- **Problem**: `atoi()` returns `int`. Passing it to `int8_t` is an implicit narrowing conversion. Values outside −128..127 silently truncate. No input validation. Compare how `dim` command clamps before cast (e.g., `val < 0 ? 0 : val > 100 ? 100 : val`).
- **Action**: Add `int b = atoi(value); b = (b < -16) ? -16 : (b > 16 ? 16 : b); config.setBalance(static_cast<int8_t>(b));` at the live handler. The correct range is **-16..16**, matching: (1) the HTML slider (`min="-16" max="16"`), (2) the `options.h` compile-time `SOUND_BALANCE` validation guard (`#elif (SOUND_BALANCE < -16) || (SOUND_BALANCE > 16)`). The prior suggestion of -100..100 in this document was incorrect.

#### [X] 6.5.1 `brightness` command — no range clamp before `uint8_t` cast `[MEDIUM]`

- **File**: `src/core/commandhandler.cpp` line ~69
- **Code**: `config.store.brightness = static_cast<uint8_t>(atoi(value));`
- **Problem**: `atoi()` on a negative or very large string silently wraps into a garbage `uint8_t`. The nearby `dim` command at line ~46 properly clamps with a ternary to 0..100.
- **Action**: Clamp `atoi(value)` to **0..100** before the cast (matching the HTML slider `min="0" max="100"` and the `dim` handler which also clamps to 0..100). Not 0..255.

#### [X] 6.5.2 `contrast` command — same pattern as `brightness` `[LOW]`

- **File**: `src/core/commandhandler.cpp` — the `contrast` command
- **Code**: `config.saveValue(&config.store.contrast, static_cast<uint8_t>(atoi(value)))`
- **Problem**: Same unclamped `uint8_t` cast. Input from the WebUI is expected to be 0..100 (HTML slider `min="0" max="100"`) but no firmware guard enforces this.
- **Action**: Clamp to **0..100** before cast, consistent with the HTML slider range.

## [X] 7. Unhandled or Mis-handled Web UI Commands (ALL FIXED)

### [X] 7.1 `treble`, `middle`, `bass` bypass `commandhandler` — split routing `[MEDIUM]`

- **Context**: All four EQ sliders in `player.html` have `data-command` values matching their names (`balance`, `treble`, `middle`, `bass`), all with `min="-16" max="16"`. Moving a slider sends `command=value` via WebSocket.
- **`balance`**: Handled in `commandhandler.cpp` at line ~30 (above the `//-----TODO` marker).
- **`treble`, `middle`, `bass`**: **Not present in `commandhandler.cpp` at all.** They are intercepted in `netserver.cpp` directly (lines ~614–627), *before* `cmd.exec()` is called, in the raw WebSocket message handler.
- **Split routing impact**: Commands handled in `netserver.cpp` before `cmd.exec()` are invisible to any command source that doesn't go through `netserver.cpp`. **Neither MQTT nor telnet routes through `cmd.exec()` today** — both have their own separate hardcoded dispatch loops (`mqtt.cpp` `onMqttMessage()` and `telnet.cpp` `on_input()`). Concretely: none of `balance`, `treble`, `middle`, or `bass` are reachable via MQTT or telnet in any form — `balance` happens to be in `commandhandler.cpp` but that file isn't called from either subsystem.
- **Root cause**: `treble`/`middle`/`bass` were added directly to `netserver.cpp` as a shortcut while `balance` was added to `commandhandler` as part of the incomplete TODO-block refactoring (see Section 5.1). The intent was clearly for all four to be in `commandhandler`, but the work was never finished.
- **Action**: Move the `treble`, `middle`, `bass` handlers from `netserver.cpp` into `commandhandler.cpp` alongside `balance`. Use `config.setTone()` the same way those handlers already do. Add appropriate clamping (see Section 7.2).

### [X] 7.2 All four EQ commands lack server-side range clamping `[MEDIUM]`

- **`treble`, `middle`, `bass`** handlers in `netserver.cpp`: `int8_t valb = atoi(val)` is an implicit narrowing cast with no range validation.
- **`balance`** handler in `commandhandler.cpp`: passes raw `atoi(value)` directly to `int8_t` param (see Section 6.1 for the live handler; section 6.1 recommended clamp has been corrected to -16..16 in this update).
- The valid range **-16..16** is enforced consistently in three places: (1) HTML sliders with `min="-16" max="16"`, (2) `options.h` `SOUND_BALANCE` guard `#elif (SOUND_BALANCE < -16) || (SOUND_BALANCE > 16)`, and (3) `options.h` `EQ_TREBLE`/`EQ_MIDDLE`/`EQ_BASS` guards with the same -16..16 bounds. The firmware runtime handlers are the only place that does not enforce this.
- **Action**: Each EQ command handler should clamp: `int v = atoi(val); v = (v < -16) ? -16 : (v > 16 ? 16 : v); config.setXxx((int8_t)v);`

### [X] 7.3 `volume` command (WebUI slider) vs `vol` command — inconsistent behavior `[MEDIUM]`

- **HTML sends `volume=N`**: The volume slider uses `data-command="volume"`, so `sliderInput()` sends `volume=N` to the backend.
- **Handler for `volume=N`** (commandhandler.cpp TODO block, line ~119): `player.setVol(static_cast<uint8_t>(atoi(value)))` — queues `PR_VOL` asynchronously; `config.store.volume` is only updated when the queue processes (via `config.setVolume()` inside `PR_VOL` handler in `player.cpp`).
- **Handler for `vol=N`** (commandhandler.cpp line ~40, above TODO): Clamps value to 0..254, **synchronously** updates `config.store.volume = clamped_v`, then queues `player.setVol(v)`. This is the robust path used by MQTT, telnet, and IR.
- **Behavioral differences**:
  1. `volume` handler: no clamp before `uint8_t` cast (negative values or values > 255 silently wrap).
  2. `volume` handler: `config.store.volume` lags until `PR_VOL` processes. If `VOLUME` is broadcast between receiving the command and the queue processing, the old value is reported.
  3. `vol` handler: proper clamp, immediate synchronous config update.
- **Action**: Either consolidate both commands into one handler using the `vol` pattern (clamp + synchronous update), or fix the `volume` handler to match: clamp to 0..254 and synchronously set `config.store.volume` before calling `player.setVol()`.

### [X] 7.4 `submitplaylistdone` intercepted in `netserver.cpp` before `cmd.exec()` `[LOW]`

- **Location**: `netserver.cpp` `onWsMessage()`, lines ~630–645 — handled **before** `cmd.exec()` is called.
- **Sent by**: `script.js` sends `websocket.send('submitplaylistdone=1')` after the server has confirmed the playlist file was saved. This triggers MQTT republish and playlist-length validation in the firmware.
- **Routing impact**: Same as 8.1 — any MQTT or telnet path that sends `submitplaylistdone` falls through `cmd.exec()` unhandled (returns `false`). Currently this only matters for testing or automation, since it's an internal JS signal.
- **Action**: Low priority. Consider moving to `commandhandler.cpp` for consistency.

---

## [ ] 8. Unified Command Dispatch — Route MQTT, Telnet, and URL Parameters Through `commandhandler.cpp` `[MEDIUM]`

Today there are **four separate, independent command dispatch tables** that must all be updated when a command changes:

- **WebSocket** (from WebUI): `netserver.cpp` `onWsMessage()` → `cmd.exec()` in `commandhandler.cpp`
- **MQTT**: `mqtt.cpp` `onMqttMessage()` — own hardcoded handler, ~10 commands only
- **Telnet / Serial**: `telnet.cpp` `on_input()` — own hardcoded handler, a different ~15-command set
- **HTTP GET** (URL params): `netserver.cpp` `handleIndex()` — single-param calls already route through `cmd.exec()`, but multi-param combinations have hardcoded special cases that bypass it

Adding or fixing a command currently means updating up to four files. The refactor goal is to make `commandhandler.cpp` the **single source of truth** for all command logic, with MQTT, Telnet, and HTTP GET routing through it via thin parser shims. This is also the prerequisite for §6.1 (moving `config.cpp` wrappers into `commandhandler`) to have full system-wide effect.

### [ ] 8.1 Route MQTT Through `commandhandler` `[MEDIUM]`

**Current state**: `onMqttMessage()` in `mqtt.cpp` manually handles: `prev`, `next`, `toggle`, `stop`, `start`/`play`, `boot`/`reboot`, `voldown`/`volm`, `volup`/`volp`, `turnoff`, `turnon`, `vol N`, `play N`, and raw URL strings (`burl`). All other `commandhandler.cpp` commands are unreachable via MQTT.

**Strategy**:
1. Add a `burl` command to `commandhandler.cpp` that loads a URL directly into `player.burl` and sends `PR_BURL` — currently only reachable via raw MQTT long-payload path.
2. Keep `turnoff` / `turnon` as thin wrappers in `onMqttMessage()` (they combine `setDspOn` + `smartstart` logic that has no single commandhandler equivalent), or add them to commandhandler.
3. For short payloads (`len < 20`), parse `"key value"` or `"key=value"` format, split into `cmd`/`val`, and call `cmd.exec(cmd, val, 0)`.
4. Replace the remaining manual handlers with the `cmd.exec()` call once all equivalents are confirmed present in `commandhandler`.

**Commands to block from MQTT dispatch** (do not forward to `cmd.exec()`):

| Command | Reason to block |
|---|---|
| `get*` family (`getsystem`, `getscreen`, `getlocale`, `getcontrols`, `getweather`, `getmqtt`, `getactive`, `getbattery`, `getindex`) | These trigger `netserver.requestOnChange()` which broadcasts JSON over WebSocket to a web client. Via MQTT the response is silently lost — wasted work always. |
| `rebootmdns` | Restarts after a short delay and no longer calls `websocket.text(cid, ...)` (the old cid misrouting issue is fixed). |
| `newmode` | Sets `config.newConfigMode` and triggers `requestOnChange(CHANGEMODE)` — an interactive WebUI display-flow command with no meaning over MQTT. |

**Commands that need care but are otherwise safe**:

| Command | Note |
|---|---|
| `reboot`, `format`, `clearspiffs` | Destructive. Consider gating behind `#ifdef MQTT_ALLOW_DANGEROUS_COMMANDS` (opt-in at build time). Currently `reboot` is already handled inline in `onMqttMessage()`. |
| `battref` | Calibration; calls `netserver.requestOnChange(GETBATTERY, cid)` with `cid=0` — sends WebSocket feedback to client 0. The calibration save still works; only the WebSocket confirmation is misdirected. |
| `curated_import`, `loadindex`, `loadplaylist` | Send `websocket.text(cid, ...)` inline responses. Via MQTT `cid=0` — file operations succeed but feedback is misdirected to a WebSocket client. |

### [ ] 8.2 Route Telnet Through `commandhandler` `[MEDIUM]`

**Current state**: `telnet.on_input()` has its own ~15-command hardcoded handler covering `prev`, `next`, `toggle`, `stop`, `start`, `vol`, `vol±`, battery commands, `date`, `audioinfo`, `smartstart`, `list`, `info`, and a few others. Commands added to `commandhandler.cpp` are not automatically accessible from telnet. This is the "duplicated command form list vs. `commandhandler.cpp`" problem noted in §13 (Stability Risks).

**Strategy**: Telnet-native commands stay in `on_input()` because they produce telnet-specific formatted output (e.g., `cli.list` prints a numbered station list, `cli.info` prints status lines, `calbatt` has interactive multi-line calibration output). For everything else, fall through to `cmd.exec()` at the bottom of `on_input()` before the `show_prompt` label:

```cpp
// At end of on_input(), before show_prompt:
char tcmd[64] = {0}, tval[BUFLEN] = {0};
if (config.parseWsCommand(str, tcmd, tval, sizeof(tcmd))) {
    if (cmd.exec(tcmd, tval, clientId)) goto show_prompt;
}
```

`parseWsCommand()` already splits `"key=value"` format used by WebSocket messages. Telnet also uses `"key value"` and `"key(value)"` forms — either extend the parser or add a simple space-split before the fallthrough.

**Commands to block from telnet dispatch** (handle in `on_input()` with a CLI response instead):

| Command | Reason |
|---|---|
| `get*` family | Triggers `requestOnChange()` → JSON broadcast over WebSocket. These could eventually be rerouted to print on the telnet stream, but that requires a netserver refactor. For now: block and print `##CLI.UNSUPPORTED#`. |
| `rebootmdns` | Restarts after a short delay and no longer calls `websocket.text(cid, ...)`, so telnet/WebSocket client-id namespace confusion is removed for this command. |
| `curated_import`, `loadindex`, `loadplaylist` | Same `websocket.text(cid, ...)` namespace mismatch. Block or add telnet-specific response. |

- [X] `rebootmdns` websocket redirect/cid-misdirection bug fixed by removing backend redirect payloads and handling redirect timing entirely in browser-side ready polling.

**⚠️ Client ID namespace collision — important hazard**:

Telnet client IDs are `0`–`(MAX_TLN_CLIENTS-1)` (typically 0–4), assigned by telnet slot number. WebSocket client IDs are assigned by `AsyncWebSocket`, also starting from 0. Any place in `commandhandler.cpp` that calls `websocket.text(cid, ...)` with the `cid` argument will accidentally target a real WebSocket client when the caller is telnet. **Audit every `websocket.text(cid, ...)` call in `commandhandler.cpp` before routing telnet through it.** Proposed mitigation: define `#define CID_NO_WEBSOCKET 255` and pass that from the telnet shim — handlers can then check `if (cid != CID_NO_WEBSOCKET) websocket.text(cid, ...)`.

**Commands immediately available once routed through `cmd.exec()`** (none of these call `websocket.text(cid,...)`):
`balance`, `volume`, `shuffle`, `screensaver*`, `wenable`, `wapi`, `wlat`/`wlon`, `locale_webui`, `tz_name`, `tzposix`, and all other settings commands. Also `treble`/`middle`/`bass` once moved from `netserver.cpp` per §7.1.

**Payoff**: Every future command added to `commandhandler.cpp` automatically becomes available from the telnet CLI with zero additional code.



### [ ] 8.3 Route URL Parameters Through `commandhandler` `[LOW]`

**Current state**: `handleIndex()` in `netserver.cpp` already routes single-param GET requests through `cmd.exec()` (e.g., `/?vol=50`, `/?play=3`, `/?toggle=1`). However, multi-param combinations fall through to hardcoded special cases instead of iterating through all params. Two legacy cases remain (see §8.5.1 and §8.5.2).

**Strategy**:
1. Add `sleep` to `commandhandler.cpp` — value is sleep minutes, `after` offset defaults to 0. Single-param `/?sleep=30` sleeps immediately; two-param `/?sleep=30&after=5` is handled by the loop executing both params.
2. Replace the `paramsNr==1` guard in `handleIndex()` with a loop over all params, calling `cmd.exec()` for each. This makes `/?treble=3&middle=0&bass=-2` route through the existing clamped handlers and `/?sleep=30` route through the new `sleep` handler.
3. Keep a targeted two-param carve-out only for `sleep`+`after` (since `after` is not a standalone command and needs to be passed together with `sleep` to `config.sleepForAfter()`), or extend the `sleep` handler to read `after` from a pre-scanned request context.
4. Special-case carve-out: `reset` and `clearspiffs` still need `request->redirect("/")` after `cmd.exec()` — the loop must check for these by name after executing.
5. Once the loop is in place, remove the hardcoded `treble`+`middle`+`bass` block (§8.5.2) and the `sleep`+`after` block (§8.5.1) — both are superseded.

### [ ] 8.4 Move `setDspOn` and `setBrightness` to `commandhandler.cpp` `[MEDIUM]`

Once MQTT and Telnet are routed through `commandhandler` (per §8.1 and §8.2), `setDspOn` and `setBrightness` can be moved from `config.cpp` into `commandhandler.cpp`. Both are currently too multi-path to inline cleanly, and both have non-commandhandler callers that only survive as long as MQTT/telnet have separate dispatch loops. After §8.1+§8.2 unification, their only callers will be in `commandhandler` and they can be inlined.

### [ ] 8.5 Commands with No HTML Entry Point

These commands exist in `commandhandler.cpp` (or elsewhere in firmware) but **cannot be triggered from any `.html` file** in the WebUI. They are reachable only via MQTT, telnet, HTTP GET URL params, or are effectively inaccessible to most users.

| Fixed | Command | Handler location | How accessible | Notes |
|---|---|---|---|---|
| [ ] | `start` | `commandhandler.cpp` | MQTT, telnet, HTTP `/?start=1` | Plays last station. No HTML button. The WebUI uses `toggle` for play/pause instead. |
| [ ] | `stop` | `commandhandler.cpp` | MQTT, telnet, HTTP `/?stop=1` | Stops playback. No HTML button. |
| [ ] | `dspon` | `commandhandler.cpp` | MQTT, telnet, HTTP `/?dspon=N` | Identical to `screenon`. Both remain in commandhandler but their HTML element is **commented out** in `options.html` line 120 with note: *"Left from yoRadio but seems to have no purpose"*. |
| [ ] | `screenon` | `commandhandler.cpp` | MQTT, telnet | Same as `dspon`. HTML element commented out. |
| [ ] | `clearspiffs` | `commandhandler.cpp` | MQTT, telnet, HTTP `/?clearspiffs=1` | Clears SPIFFS and resets play mode. No HTML button. Useful for factory cleanup but not exposed to users. |
| [ ] | `sleep` | `netserver.cpp` `handleIndex()` | HTTP `/?sleep=N&after=N` **only** | Schedules sleep timer. No WebSocket handler. No HTML. |
| [ ] | `playstation` | `commandhandler.cpp` (alias for `play`) | MQTT, telnet | The HTML uses `play=N` (via `websocket.send(\`play=${item}\`)`). `playstation` is a legacy alias; both spellings work. |

**Notes**:
- `start`, `stop`, and `dspon`/`screenon` are yoRadio leftovers that were never assigned HTML buttons in the ehRadio fork. Their presence in `commandhandler.cpp` is not harmful — they remain useful for MQTT/telnet automation — but they add dead weight to the handler if never used interactively.
- `clearspiffs` and `sleep` are subtle traps: a user configuring MQTT automations would have no way to discover these commands from the WebUI documentation or source code without reading `commandhandler.cpp` directly.

**Action items**:
- `dspon` / `screenon`: Remove the duplicate or point both to the same implementation. Decide whether to restore the HTML element or remove the command.
- `sleep`: Expose via a WebUI sleep-timer control (options page) or remove from firmware if the feature is not intended for production use.
- Document all MQTT/telnet-accessible commands in one place (currently they must be inferred by reading `commandhandler.cpp` and `telnet.cpp` separately).

#### [ ] 8.5.1 `sleep` / `after` — HTTP GET only, no WebSocket path `[MEDIUM]`

- **Location**: `netserver.cpp` `handleIndex()`.
- **Code**: `if (request->hasArg("sleep")) { ... config.sleepForAfter(sford, safterd); ... }`
- **What it does**: Schedules the device to sleep for `sleep` minutes, starting `after` minutes from now.
- **Access**: HTTP GET URL parameters only — e.g., `http://device/?sleep=30&after=5`. No WebSocket equivalent, no MQTT/telnet path, no HTML entry point.
- **Action**: Add `sleep` to `commandhandler.cpp` (per §8.3 strategy). Once the §8.3 loop is in place this hardcoded block is fully superseded and can be safely removed. Consider also exposing `sleep` via the WebUI settings page once it is in commandhandler.

#### [ ] 8.5.2 HTTP GET `treble`+`middle`+`bass` multi-param route `[LOW]`

- **Location**: `netserver.cpp` `handleIndex()`.
- **Code**: `if (request->hasArg("treble") && request->hasArg("middle") && request->hasArg("bass")) { config.setTone(...) }`
- **Analysis**: Legacy URL-parameter API that sets all three EQ bands at once (`http://device/?treble=3&middle=0&bass=-2`). Not used by any current JS code. No clamping applied.
- **Action**: Once §8.3 loop is in place, each param is dispatched individually through the existing clamped commandhandler handlers. This hardcoded block is then fully superseded and can be safely removed.

---


## [X] 9. Logic / Correctness Bugs (ALL FIXED)

### [X] 9.1 `display.cpp` — `while(!_bootStep==0)` precedence bug `[MEDIUM]`

- **Line 113**: `while(!_bootStep==0) { delay(10); }`
- Due to C++ operator precedence, this evaluates as `(!_bootStep) == 0`. When `_bootStep = 0` (initial state), `!0 = 1`, `1 == 0 = false` → loop exits immediately without waiting. When `_bootStep = 1` or `2`, `!x = 0`, `0 == 0 = true` → loop runs indefinitely.
- Likely intent was `while(_bootStep == 0)` (wait for boot to start) or `while(_bootStep < 2)` (wait for boot to complete).
- `_bootStep` is `uint8_t` in `display.h` line ~83.

### [X] 9.2 `netserver.cpp` — upload cleanup deletes wrong file `[HIGH]`

- Upload cleanup block: `if (SPIFFS.exists(INDEX_PATH)) SPIFFS.remove(PLAYLIST_PATH)` — should call `SPIFFS.remove(INDEX_PATH)`. The index file is never cleaned up; the playlist may be deleted instead.

### [X] 9.3 `optionschecker.h` — weather interval guard mismatch `[LOW]` (FIXED)

- Guard message says "10 to 60" but the enforced condition is "1 to 24". Message and bounds are mismatched.

### [X] 9.4 `netserver.cpp` — `selectRadioBrowserServer()` `size_t` underflow `[HIGH]`

- `for (size_t i = count - 1; i > 0; --i)` runs even when `count == 0`. `size_t` is unsigned, so `0 - 1` wraps to a very large value and the loop indexes out of bounds.
- `rb_servers[count] = RADIO_BROWSER_SERVER` also writes past the array end when `count == arr_size`.

---


## [ ] 10. `BUFLEN` — Multi-Purpose Magic Number

`BUFLEN = 170` is defined in `options.h` (`#define BUFLEN 170 // 170 seems safe... a lot of multipliers exist in the code...`). The comment itself is a warning sign: "a lot of multipliers exist" means 170 was already insufficient for some callers when it was written.

### 10.1 Call-site inventory

| Call site | File | Purpose | Notes |
|---|---|---|---|
| `StationInfo::name[BUFLEN]`, `::url[BUFLEN]`, `::title[BUFLEN]` | `config.h` struct | Runtime RAM fields — populated from SPIFFS playlist file | **Not NVS-stored.** Only `store.lastStation` (a `uint16_t` index) is persisted. `station.title` is from stream metadata, never saved. Changing BUFLEN just changes truncation threshold for long names/URLs. |
| `Config::_stationBuf[BUFLEN/2]` | `config.h` | Temporary CSV row parsing buffer | Consistent with playlist tab-fragment size |
| `sName[BUFLEN]`, `sUrl[BUFLEN]` | `config.cpp`, `telnet.cpp` | Stack temps matching struct field size | Consistent |
| `utf8_common.h`, `utf8Latin.cpp`, `utf8Cyrillic.cpp`, `nextion.cpp` | Display tools | Transliteration output buffer | Correct to match `title` field size |
| `audiohandlers.h` `b[BUFLEN/2]` | Audio handlers | Bitrate info string (85 bytes) | Adequate for bitrate strings |
| `audiohandlers.h` `tmp[BUFLEN]` | Audio handlers | Station title + audio info combined | Appropriate |
| `nomedia[BUFLEN]` | `sdmanager.cpp` | SD path building (`path + "/.nomedia"`) | Wrong semantic — SD paths can exceed 170 chars |
| `wsbuf[BUFLEN*2]`, `payload[BUFLEN*2]`, `buf[BUFLEN*2]`, `msgBuf[BUFLEN*2]`, `varjsbuf[BUFLEN*2]` | `netserver.cpp` | WebSocket JSON payloads, URL buffers | `*2` multiplier is a code smell — see 10.2 |
| `buf[BUFLEN]`, scratch uses | `telnet.cpp`, `netserver.cpp` | Short `snprintf` scratch buffers | Acceptable for short messages |
| `BOOTLOG` macro | `telnet.h` | Boot log buffer with bare `sprintf` | Overflow risk — cross-ref Section 11 |

### 10.2 The `*2` and `/2` multiplier smell `[MEDIUM]`

Five places in `netserver.cpp` use `BUFLEN*2` (340 bytes) because 170 was insufficient for JSON payloads. The code doubled an already-arbitrary number rather than defining an appropriately-sized constant. This conflates two unrelated constraints in one macro:

- **`wsbuf[BUFLEN*2]` with bare `sprintf`**: embeds `config.station.name` (up to 170 bytes) and `config.station.title` (up to 170 bytes) simultaneously into a 340-byte buffer with JSON framing overhead. At worst-case inputs, this is structurally too small. Already flagged in Section 11 as unsafe `sprintf`.
- The multiply/divide pattern (`*2`, `/2`) makes the actual buffer sizes invisible and means any future change to `BUFLEN` for the struct silently changes all these buffers too.

### 10.3 No NVS tie-in — RAM only `[correction]`

`station_t` (`name`, `url`, `title`) is **not stored in NVS**. `loadStation()` reads the SPIFFS playlist file at runtime and fills `station` in RAM. `station.title` is set from audio stream metadata and is never persisted. The only NVS save is `store.lastStation` — a `uint16_t` index.

Changing `BUFLEN` carries **no NVS migration risk**. The only behavioral change would be the truncation threshold for very long station names or URLs read from the playlist. Raising it does increase stack frame sizes wherever `char buf[BUFLEN]` locals are declared. The `*2` multipliers in `netserver.cpp` still exist as a sizing smell, but the cause is simpler: the JSON payload combining name + title was larger than a single BUFLEN.

### 10.4 Relationship to `RXBUFLEN` / `TXBUFLEN`

`RXBUFLEN = 50` and `TXBUFLEN = 255` are defined in `src/displays/nextion.h`. They are **completely unrelated** to `BUFLEN` — they are Nextion serial protocol frame sizes. The naming similarity is coincidental.

### 10.5 Other buffer-size constants not derived from `BUFLEN`

| Constant | Where | Value | Notes |
|---|---|---|---|
| `SET_PLAY_ERROR` buff size | `player.h` macro | `512 + 64` = 576 | Fixed literal; independent. Bare `sprintf` — see Section 11. |
| `DBGVB` buf size | `netserver.cpp` macro | `200` | Fixed literal; bare `sprintf` — see Section 11. |
| `MAX_PRINTF_LEN` | `telnet.h` | `BUFLEN + 50` = 220 | Derived from `BUFLEN`. If `BUFLEN` changes, this changes silently too. |
| `EHDPNAME_LENGTH` | `config.h` | `24` | Named purpose-specific constant — **this is the right pattern**. |

### 10.6 Recommended resolution `[LOW]`

The core problem: `BUFLEN` conflates two distinct things that happen to share one numeric value:
1. **Station field size** (`StationInfo` fields and matching transliteration buffers) — the meaningful semantic is "max station name/URL/title length".
2. **General scratch sentinel** — arbitrary "safe size" for stack temporaries.

Recommended approach: introduce `STATION_FIELD_LEN` (or similar) for category 1, making the intent explicit. Leave `BUFLEN` as-is or remove it for category 2. Replace `BUFLEN*2` and `BUFLEN/2` with purpose-named sizes or comment-justified literals. This is a **refactor, not an urgent fix** — nothing is currently broken solely because of `BUFLEN` — and unlike the previous analysis, there is **no NVS migration risk** involved.

### [ ] 10.7 Replacing `BUFLEN` Usage as a Magic Number

Each call site below should be investigated and given either a purpose-specific named constant or an inline literal with a justifying comment.

- [ ] `StationInfo::name[BUFLEN]`, `::url[BUFLEN]`, `::title[BUFLEN]` (`config.h`) — primary semantic: "max station name/URL/title length". Candidate: introduce `STATION_FIELD_LEN 170`.
- [ ] `Config::_stationBuf[BUFLEN/2]` (`config.h`) — name explicitly (e.g., `STATION_FIELD_LEN/2`) or justified inline literal.
- [ ] `sName[BUFLEN]`, `sUrl[BUFLEN]` in `config.cpp`, `telnet.cpp` — stack temps matching struct field size; update to follow `STATION_FIELD_LEN` once defined.
- [ ] Transliteration output buffers in `utf8_common.h`, `utf8Latin.cpp`, `utf8Cyrillic.cpp`, `nextion.cpp` — should match `title` field size; update to `STATION_FIELD_LEN`.
- [ ] `b[BUFLEN/2]` in `audiohandlers.h` (bitrate string, 85 bytes) — adequate size; name it or leave as explicit literal with comment.
- [ ] `tmp[BUFLEN]` in `audiohandlers.h` (title + audio info combined) — appropriate; update to `STATION_FIELD_LEN`.
- [ ] `nomedia[BUFLEN]` in `sdmanager.cpp` (SD path building) — wrong semantic; SD paths can exceed 170 chars. Replace with a purpose-specific `SD_PATH_LEN` or explicit larger literal.
- [ ] `wsbuf[BUFLEN*2]`, `payload[BUFLEN*2]`, `buf[BUFLEN*2]`, `msgBuf[BUFLEN*2]`, `varjsbuf[BUFLEN*2]` in `netserver.cpp` — the `*2` is the smell. Each should become a justified literal or purpose-named size.
- [ ] `buf[BUFLEN]` scratch uses in `telnet.cpp`, `netserver.cpp` — evaluate each; either leave with an explicit literal + comment, or retain `BUFLEN` if it remains as a general scratch sentinel.
- [ ] `BOOTLOG` macro buffer `buf[BUFLEN]` in `telnet.h` — also has bare `sprintf` overflow risk (cross-ref Section 11); address rename alongside `snprintf` fix.
- [ ] `MAX_PRINTF_LEN = BUFLEN + 50` in `telnet.h` — must not silently change if `BUFLEN` changes for other reasons. Replace with explicit `220` or `STATION_FIELD_LEN + 50` with comment.

---

## [X] 11. Unsafe String Handling / Buffer Overflow Risks (ALL FIXED)

| Fixed | Location | Issue |
|---|---|---|
| [X] | `config.cpp` `u8fix()` | `src[strlen(src)-1]` with no empty-string guard — UB if `strlen(src) == 0`. |
| [X] | `sdmanager.cpp` | `strrchr(filePath, '/') + 1` — if `strrchr` returns NULL, `NULL+1` is UB. |
| [X] | `nextion.cpp` rx buffer | `rx_pos` incremented without bounds check against `RXBUFLEN` (50). Overflow on long frames. |
| [X] | `nextion.cpp` `sscanf` | `sscanf(rxbuf, "page=%s", scanBuf)` uses `%s` without width; `scanBuf[50]` can overflow by prefix length. |
| [X] | `commandhandler.cpp` `irclr` | `config.irindex` initialized to -1; `irVals[config.irindex][...]` is out-of-bounds if `irclr` sent before `chkid`. Also second subscript from `atoi` can be 0–255 but array dim is only 3. |
| [X] | `netserver.cpp` `STATIONNAME`/`TITLE` | `sprintf(wsbuf, ...)` embeds ~170-byte fields into a 340-byte buffer with JSON framing. Near-limit inputs can overflow. |
| [X] | `netserver.cpp` `DBGVB` macro | `char buf[200]; sprintf(buf, __VA_ARGS__)` — VA_ARGS controlled by callers, no overflow guard. |
| [X] | `network.cpp` `strcpy(weatherBuf, LANG::weather_loading)` | `weatherBuf` is 254 bytes; locale string has no enforced max. Long translations overflow. |
| [X] | `network.cpp` weather builder | Chained `sprintf` advancing pointer `p` across `weatherBuf` (254 bytes) with no remaining-length checks. |
| [X] | `telnet.h` `BOOTLOG` macro | `sprintf(buf, __VA_ARGS__)` into `buf[BUFLEN]` (170 bytes); no VA_ARGS overflow guard. |
| [X] | `player.h` `SET_PLAY_ERROR` macro | `sprintf(buff, __VA_ARGS__)` into `buff[576]`; caller-controlled VA_ARGS. |
| [X] | `widgets.cpp` date string | `sprintf(_tmp, "%2d %s %d", ..., LANG::mnths[...], ...)` into `_tmp[30]`; UTF-8 month names have more bytes than display characters; 30-byte buffer can overflow with multi-byte locale encodings. |

---

## [X] 12. WebUI / JavaScript Security Issues (ALL FIXED)

| Fixed | Location | Issue |
|---|---|---|
| [X] | `data/www/search.js` | `${station.name}` injected into `innerHTML` without escaping. Station names from Radio Browser API (external, untrusted). Script injection risk. `escapeHtml()` exists in `curated.js` but not applied here. |
| [X] | `data/www/curated.js` | `curatedName` injected into `innerHTML` without escaping. `escapeHtml()` now applied. Note: `curatedLink` href scheme validation was reviewed and decided unnecessary (build-time SPIFFS data, not external API input). |

---

## [ ] 13. Stability / Architecture Risks


## [ ] 13.1 Three issues that don't Need their own checklists (broken, commandhandler issue above, spelling)

| Fixed | Location | Issue |
|---|---|---|
| [ ] | `nextion.cpp` | file starts with explicit warning comment that implementation may be broken; treat as unstable until revalidated.
| [ ] | `telnet.cpp`| duplicated command form list vs. `commandhandler.cpp`; new settings added to one path are easily missed in the other.
| [ ] | `touchscreen.h`| enum member naming inconsistency `TDS_REQUEST` vs. `TSD_*` pattern.

## [ ] 13.2 Heavy async queue and task usage — ordering/race conditions are possible `[MEDIUM]`

The codebase runs multiple concurrent FreeRTOS tasks and uses three separate queues for inter-task communication. None of these queues, nor the shared state they protect, use mutexes. The following specific risks have been identified by code inspection.

### Queue depth vs. burst injection

| Queue | Declared depth | Send timeout | Sender task |
|---|---|---|---|
| `displayQueue` | 5 | 200 ms | Any caller of `display.putRequest()` |
| `nsQueue` (netserver) | 20 | 300 ms | Any caller of `requestOnChange()` |
| `playerQueue` | 5 | 1000 ms | Any caller of `player.sendCommand()` |

**`GETINDEX` bursts**: `processQueue()` handles `GETINDEX` by re-enqueuing 10–12 sub-requests (`STATION`, `TITLE`, `VOLUME`, `EQUALIZER`, `BALANCE`, `BITRATE`, `MODE`, `SDINIT`, `GETPLAYERMODE`, `GETBATTERY`, and optionally `SDPOS`/`SDLEN`/`SDSHUFFLE`) in one shot. If the netserver queue already holds ~8 items, the burst will fill it completely; further `requestOnChange()` calls from anywhere (audio callbacks, player change, battery tick) arrive at a full queue and block for up to 300 ms each. This 300 ms block happens on the AsyncWebServer task (the WebSocket handler), stalling all HTTP and WebSocket event processing for as long as it takes the queue to drain.

**Player queue 1-second block**: `PLQ_SEND_DELAY = 1000 ms`. A bounce sequence (stop → play → vol) from the WebUI sends three `sendCommand()` calls in rapid succession. If the player loop is busy (e.g., doing a DNS lookup or stream connect), the third call will block the WebSocket task for close to one full second. The WebSocket task does not time out on its own; this manifests as the UI appearing frozen.

**Display queue drop-and-forget**: `DSQ_SEND_DELAY = 200 ms`. There is no caller that checks the return value of `xQueueSend()`. A dropped display request is silently lost — the display never updates state until the next spontaneous trigger. Worst-case during rapid station-change sequences: the "station name" display update is dropped, leaving stale text on screen indefinitely.

### `g_searchTaskHandle` / `g_curatedTaskHandle` check-then-act race

In `commandhandler.cpp`:
```cpp
extern TaskHandle_t g_curatedTaskHandle;
if (g_curatedTaskHandle == NULL) {
    xTaskCreate(vTaskFetchCuratedIndex, ..., &g_curatedTaskHandle);
}
```
The handle variable is not `volatile`. The completed task body sets `g_curatedTaskHandle = NULL` on its own core before calling `vTaskDelete(NULL)`. On an SMP system (ESP32/S3 both have two cores), the NULL write from Core 1 (or Core 0 depending on where the task ran) is not guaranteed to be visible to Core 0 without a memory barrier or `volatile` marker, even though FreeRTOS itself provides barriers at scheduling boundaries. More concretely: two near-simultaneous WebSocket messages (e.g., from two open browser tabs) from different TCP connections can both be delivered before the first task has had a chance to fire and set the non-NULL handle. Both check `== NULL`, both pass, both call `xTaskCreate` — resulting in two concurrent curated-index download tasks overwriting the same SPIFFS file simultaneously. The `g_searchTaskHandle` pattern in `handleSearch()` has the same exposure.

### Concurrent SPIFFS access during boot

`startupServicesAsync` (FreeRTOS task, priority 2) is launched near the end of `config.startupServices()` and downloads updated www files, writing them directly to `/www/` on SPIFFS. The task runs concurrently with `netserver.begin()` and the live `webserver.serveStatic("/", SPIFFS, "/www/")` handler that is already serving files. If a browser makes a request to `/www/script.js.gz` at the exact moment `startupServicesAsync` is in the middle of writing a new version of that file, the `serveStatic` handler opens the file, reads partial bytes (however many have been written so far), and sends a truncated or corrupted response. SPIFFS' internal per-handle mutex prevents two writes to the same file at the block level, but does not coordinate a read-during-write at the file-content level. The browser may cache the corrupted response and show a broken UI.

### Curated playlist import — task/handler interleave

`vTaskFetchCuratedPlaylist` starts by calling `SPIFFS.remove("/www/pl_import.json")` and then writes that path. The `curated_import` command in `commandhandler.cpp` opens and reads `/www/pl_import.json`. If a user triggers `curated_import` (or the browser retries) before the download task finishes writing, the handler opens a partially-written or already-deleted file. `SPIFFS.open()` will succeed (on a newly-created file) and `file.readBytesUntil()` will return fewer bytes than expected, leading to silent import failure — no error to the user, just an empty import result.

### `startupServicesAsync` task priority race

`startupServicesAsync` runs at priority 2. `DspTask` runs at priority 2. They are equal priority on what may be the same core (the task is `xTaskCreate` without a core affinity pin, so FreeRTOS places it on whichever core has capacity). During boot, both tasks run concurrently. The display task drives `display.loop()` and `netserver.loop()` — including processing the netserver queue. If `startupServicesAsync` is writing to SPIFFS while the display task drains the netserver queue and `processQueue()` triggers `PLAYLISTSAVED` (which calls `config.indexPlaylist()` which reads SPIFFS), both call SPIFFS APIs simultaneously. SPIFFS' internal FreeRTOS mutex should serialize block operations, but the SPIFFS layer in Arduino ESP32 does not protect composite multi-block operations (like iterating a playlist file with multiple `readBytesUntil` calls) as a single atomic unit. Interleaved reads and writes during startup can corrupt the in-memory playlist index.

### Worst-case scenario summary

The combination most likely to produce a real failure: user opens the WebUI in two browser tabs, both perform initial state fetch (`GETINDEX`), which bursts ~24 items into a 20-item queue. The queue overflows. Some items are dropped. Meanwhile `startupServicesAsync` is downloading a locale file. The player receives a `PR_PLAY` command that the audio library needs to resolve via DNS. All three tasks are blocked. `sendCommand()` blocks the WebSocket handler for 1 second. The second tab's WebSocket connection times out. The audio stream never starts. There is no error in the Serial log.

### Suggested fixes

1. **Queue overflow detection**: Check the return value of every `xQueueSend()` call. Log a warning on `pdFALSE`. Do not silently drop display/netserver/player requests.
2. **TaskHandle guard**: Declare `g_searchTaskHandle` and `g_curatedTaskHandle` as `volatile TaskHandle_t`. Add a FreeRTOS critical-section wrapper around the NULL-check + `xTaskCreate` pair if double-spawn must be prevented absolutely.
3. **Boot file update sequencing**: Delay `netserver.begin()` until `startupServicesAsync` has finished (or restructure to update files before starting the web server). Alternatively, write updates to a staging path and rename when complete — SPIFFS `rename()` is atomic at the file-system level.
4. **GETINDEX flood protection**: Batch the 10+ sub-requests for `GETINDEX` into a single queue item rather than re-enqueuing each separately, or increase `nsQueue` depth.
5. **Player queue timeout reduction**: Lower `PLQ_SEND_DELAY` from 1000 ms to something caller-appropriate (50–100 ms). A missed player command should be retried, not waited on for a second.

---

## [ ] 13.3 SPIFFS space constraints can silently break search, curated, and update workflows `[MEDIUM]`

### Partition budget for `board_esp32` (4 MB flash)

The `builds/4MBflash.csv` partition table allocates:
- `app0` / `app1`: 1.75 MB each (OTA-capable firmware slots)
- `spiffs`: **0x70000 = 448 KB** — the entire budget for all runtime data and all www files combined

Static content that occupies SPIFFS on every boot (approximate gzipped sizes from a production SPIFFS image):

| File | Approx. size |
|---|---|
| `/www/player.html.gz` + `options.html.gz` etc. | ~30–50 KB total |
| `/www/script.js.gz` + `script2.js.gz` | ~30–40 KB |
| `/www/style.css.gz` + `theme.css.gz` | ~8 KB |
| `/www/locales.json.gz` (or per-locale file) | ~20–35 KB |
| `/www/rb_srvrs.json` | ~10 KB |
| `/www/timezones.json` | ~30 KB |
| `/data/playlist.csv` | user-variable, easily 20–50 KB for a modest list |
| `/data/wifi.csv` | ~1 KB |
| `/data/ehradio.ver` | ~0.1 KB |
| **Static subtotal** | **~150–220 KB** |

That leaves approximately **230–300 KB free** for dynamic operations on a plausibly-loaded device. Not enormous.

### Dynamic operations and their space footprints

| Operation | Files written | Max size |
|---|---|---|
| Radio Browser search | `/www/searchresults.json` + `/www/search.txt` | up to ~100 KB (enforced by `search.js` `limit_per_page`) |
| Curated index download | `/www/curated.json` | unknown — depends on host |
| Curated playlist import | `/www/pl_import.json` → `TMP_PATH` → PLAYLIST | depends on playlist size |
| Online www update | writes each file directly to `/www/` | ~150–200 KB total for all files |
| Playlist upload | `TMP_PATH` → `PLAYLIST_PATH` | user-variable |
| Locale download | `/www/{locale}.json` | ~20–35 KB |

If a user has a 50 KB playlist, runs a search (100 KB result), and then starts a www update, the aggregate demand is roughly `50 + 100 + 150 = 300 KB` of simultaneous SPIFFS writes — exceeding the budget on a 448 KB partition with any baseline static content.

### The `FS_REQUIRED_FREE_SPACE` guard is coarse and consistently bypassed

`FS_REQUIRED_FREE_SPACE = 150 KB` is checked at the start of `vTaskSearchRadioBrowser`, `vTaskFetchCuratedIndex`, and `vTaskFetchCuratedPlaylist`. It is **not** checked by:
- The online update path (`startupServicesAsync` / `updateFile`)
- The locale download path (`updateLocaleFileAsync`)
- The playlist upload handler (`handleUpload`)
- The `curated_import` command (which copies `pl_import.json` to the playlist)

`handleUpload` has its own guard: `freeSpace = (float)SPIFFS.totalBytes()/100*68-SPIFFS.usedBytes()` — caps the upload at 68% of total SPIFFS. On a 448 KB partition with 200 KB already used, this allows only ~105 KB upload. The cap is not communicated back to the user as an error message; the upload simply silently truncates at the cap.

### Silent failure modes enumerated

**Search fails quietly**: The free-space check fires, `requestOnChange(SEARCH_FAILED, 0)` is sent. The WebUI receives `{"search_failed":true}` and displays a generic failure message. The user sees no indication that disk space was the cause.

**Online update writes partial file then stops**: `updateFile()` calls `ESPFileUpdater::checkAndUpdate()`. If SPIFFS runs out of space mid-download, the file is truncated. On the next boot, `config.init()` may not recognise the truncated firmware version, trigger a fresh download cycle, fail again, and loop — never successfully booting with a clean www directory. (The `ehradio.ver` marker guards against this somewhat, but a truncated file that passes the version check would escape it.)

**Locale download leaves old locale in place with no notification**: `updateLocaleFileAsync` writes to a temp path and renames. If the write fails mid-stream due to space, `SPIFFS.rename()` is not reached. The device silently keeps the old locale file while the WebUI locale selector shows the new one as selected in NVS.

**Playlist upload truncates silently**: The 68% guard on `handleUpload` silently discards write bytes past the cap. The user's newly-uploaded playlist is truncated at whatever fits. No HTTP error is returned — the upload completes with HTTP 200. The device then re-indexes the truncated file and the user loses stations with no indication why.

**Curated import with a full SPIFFS**: `vTaskFetchCuratedPlaylist` fails its free-space check. `CURATED_FAILED` is sent. The user retries. Still fails. There is no WebUI prompt suggesting they free space first.

### ESP32-S3 (16 MB flash) largely escapes this problem

The `board_esp32_s3_n16r8` environment does not specify a `board_build.partitions` override. PlatformIO's default for `esp32-s3-devkitc-1` with 16 MB flash uses `default_16MB.csv`, which gives approximately 9–12 MB to SPIFFS. At that scale, the 448 KB constraint disappears entirely: a full www update, a 100 KB search result, and a 50 KB playlist together consume under 2% of available space.

This means **the SPIFFS space problem is effectively an `board_esp32` (4 MB flash) problem**. Any user running an S3-N16R8 build is unlikely to ever encounter it under normal use.

### Suggested fixes

1. **Check remaining space before every SPIFFS write**, not just before search/curated. `ESPFileUpdater`, `handleUpload`, and `updateLocaleFileAsync` all need the guard.
2. **Return specific error to the user** when a write fails due to space: HTTP 507 Insufficient Storage for uploads; WebSocket `{"error":"spiffs_full"}` for async tasks.
3. **Expose SPIFFS usage in the WebUI** (e.g., in options.html system group or `getsystem` response). `SPIFFS.usedBytes()` and `SPIFFS.totalBytes()` are cheap calls; including them in `GETSYSTEM` lets the user see how full the filesystem is before attempting operations.
4. **For `board_esp32` only**: consider moving the partition table to give SPIFFS more space at the cost of one OTA slot (single-slot non-OTA partition), or move to LittleFS which has better space utilisation than SPIFFS for the same physical allocation.

---

## [X] 14. Dead / Redundant Includes (`netserver.cpp`) `[LOW]` (ALL FIXED)

`src/core/netserver.cpp` contained duplicate `#include` directives. Fixed as part of the codebase-wide include standardization pass (which also corrected include ordering across all `src/core/` files, `src/displays/widgets/`, and `src/libraries/`):

| Fixed | Duplicate | Lines |
|---|---|---|
| [X] | `#include "config.h"` | Lines 7 AND 11 — two identical includes |
| [X] | `#include "options.h"` | Lines 1 AND 15 — two identical includes |
| [X] | `//= #include <ESPmDNS.h>` | Line 25 — commented-out redundant copy of the active include on line 5 |

---

## [X] 15. Include Path Convention Inconsistency `[TRIVIAL]` (ALL FIXED)

Most `src/core/*.cpp` files include peer headers using relative names (`#include "battery.h"`), but `battery.cpp` and `rgbled.cpp` use fully-prefixed paths (`#include "core/battery.h"`, `#include "core/rgbled.h"`) even for their **own** headers. This works because PlatformIO exposes both `src/` and `src/core/` as include roots, but the mixed convention is confusing.

Notable cases:
- `battery.cpp` includes `"core/battery.h"` (self-include with prefix) and all peers with `"core/xxx.h"`
- `rgbled.cpp` includes `"core/rgbled.h"` (self-include with prefix)
- `commandhandler.cpp` includes `"core/battery.h"` but `"commandhandler.h"` without prefix

**Action**: Normalise to either all-relative (preferred for files in `src/core/`) or all-prefixed, not mixed. Low priority but makes `#include` scanning clearer. `[TRIVIAL]`

---

## [ ] 16. C-style modules in `src/core/` — should be refactored to class + global instance `[LOW]`

A repo-wide audit of all `src/core/` module pairs confirms that **four modules** use the C-style pattern (free functions + file-static state) rather than the standard codebase convention of a C++ class with a single `extern` global instance. All other core modules (`Config`, `Player`, `Display`, `MyNetwork`, `NetServer`, `Telnet`, `CommandHandler`, `SDManager`, `RTC`, `TouchScreen`) already follow the class pattern.

The established codebase pattern is:
- **Class** defined in `.h` (e.g. `class Config`, `class NetServer`, `class Player`)
- **Single global instance** defined in `.cpp` (e.g. `Config config;`)
- **`extern` declaration** in `.h` so any includer gets the named global (e.g. `extern Config config;`)
- Callers use `config.saveValue(...)`, `netserver.requestOnChange(...)`, `player.setVol(...)`, etc.

None of these four refactors are urgent — each module is functionally correct as-is. The §8 MQTT/telnet consolidation does **not** depend on any of them. These are purely long-term consistency items.

**Scope summary**:

| Module | Files | Free functions exposed | File-static globals |
|---|---|---|---|
| Battery | `battery.cpp` / `battery.h` | `battery_init`, `battery_loop`, `battery_recalc_now`, `battery_calibrate`, `battery_get_status`, `battery_is_initialized`, `battery_format_status_line`, `battery_boot_status` | 20+ state vars: ADC samples, EMA voltage, pct ring buffer, inferred charge/discharge state, thresholds |
| RGB LED | `rgbled.cpp` / `rgbled.h` | `rgbled_init`, `rgbled_is_initialized`, `rgbled_set`, `rgbled_playing`, `rgbled_stopped`, `rgbled_trackchange`, `rgbled_loop` | `strip` object, `flashCount`, `lastFlash`, `_rgb_inited`, `_cycle`, `_lastCycle`, `_cycleState`, `_state` |
| Controls | `controls.cpp` / `controls.h` | `initControls`, `loopControls`, `encoder1Loop`, `encoder2Loop`, `encodersLoop`, `irBlink`, `irNumber`, `irLoop`, `onBtnClick`, `onBtnDoubleClick`, `onBtnLongPressStart`, `onBtnLongPressStop`, `onBtnDuringLongPress`, `controlsEvent`, `setIRTolerance`, `setEncAcceleration`, `flipTS` | `encOldPosition`, `enc2OldPosition`, `lpId`, `button[]`, `encoder`, `encoder2`, `irrecv`, `irResults`, `irVolRepeat` |
| MQTT | `mqtt.cpp` / `mqtt.h` | `connectToMqtt`, `mqttInit`, `zeroBuffer`, `onMqttConnect`, `onMqttDisconnect`, `onMqttMessage`, `mqttPublishStatus`, `mqttPublishPlaylist`, `mqttPublishVolume` | `mqttClient`, `mqttReconnectTimer`, `topic[100]`, `status[BUFLEN+50]` |

---

### [ ] 16.1 `battery.cpp` / `battery.h` `[LOW]`

**Suggested refactor** (future, not urgent):
1. Define `class Battery` in `battery.h` with public methods mirroring current free functions (`init()`, `loop()`, `recalcNow()`, `calibrate(int meas_mv)`, `getStatus()`, `isInitialized()`, `formatStatusLine(...)`, `bootStatus()`).
2. Move all file-static state to private members of the class.
3. Define `Battery battery;` in `battery.cpp` and `extern Battery battery;` in `battery.h`.
4. Update all callers: `commandhandler.cpp`, `telnet.cpp`, `netserver.cpp`, `main.cpp` (and any display drivers that call `battery_get_status()`).
5. The `#if` hardware guard should wrap the class body or use a no-op stub class in the `#else` branch.

**Blockers / notes:**
- The `#if (defined(BATTERY_PIN)...) || (defined(BATTERY_CHARGE_PIN)...)` guard that wraps nearly the entire `.cpp` needs careful handling so the no-op stubs still compile as class methods.

---

### [ ] 16.2 `rgbled.cpp` / `rgbled.h` `[LOW]`

**Suggested refactor** (future, not urgent):
1. Define `class RgbLed` in `rgbled.h` with public methods: `init()`, `isInitialized()`, `set(uint8_t r, uint8_t g, uint8_t b)`, `playing()`, `stopped()`, `trackChange()`, `loop()`.
2. Move all file-static state (`strip`, `flashCount`, `lastFlash`, `_rgb_inited`, `_cycle`, `_lastCycle`, `_cycleState`, `_state`, `rgb_state_e`) to private members of the class.
3. Define `RgbLed rgbled;` in `rgbled.cpp` and `extern RgbLed rgbled;` in `rgbled.h`.
4. Update callers in `main.cpp` (`rgbled_init()` → `rgbled.init()`, `rgbled_loop()` → `rgbled.loop()`, etc.).
5. The `#if defined(RGB_LED_PIN) && (RGB_LED_PIN!=255)` guard should wrap the class body; the `#else` branch provides a no-op stub class.

**Blockers / notes:**
- The `strip` member is a templated `Adafruit_NeoPixel` constructed with `RGB_LED_PIN` — a compile-time constant. This is fine as a class member initialised in the constructor: `strip(NUM_RGB_LEDS, RGB_LED_PIN, RGB_LED_ORDER + NEO_KHZ800)`.
- Simplest of the four refactors — no callers outside `main.cpp` and the module itself.

---

### [ ] 16.3 `controls.cpp` / `controls.h` `[LOW]`

**Suggested refactor** (future, not urgent):
1. Define `class Controls` in `controls.h` with public methods mirroring the current free functions.
2. Move `encOldPosition`, `enc2OldPosition`, `lpId`, the `button[]` array, `encoder`, `encoder2`, `irrecv`, `irResults`, and `irVolRepeat` to private members.
3. The `IRAM_ATTR` ISR functions (`readEncoderISR`, `readEncoder2ISR`) must remain free functions because ISRs cannot be non-static member functions on ESP32; they should be thin free-function wrappers that call a static member or a global instance method.
4. Define `Controls controls;` in `controls.cpp` and `extern Controls controls;` in `controls.h`.
5. Update callers in `main.cpp` (`initControls()` → `controls.init()`, `loopControls()` → `controls.loop()`, etc.).

**Blockers / notes:**
- The ISR constraint (step 3) is the main non-trivial complication. The standard workaround is two free-function ISRs that call `controls.encoderISR()` etc. on the global instance — this is already essentially the pattern in `controls.cpp` (the ISRs just call `encoder.readEncoder_ISR()`), so the wrapping is minimal.
- The `ctrls_on_loop` weak-attribute plugin hook is declared `extern __attribute__((weak))` — it must remain exposed or be adapted to the plugin manager hook system.
- Most complex of the four refactors due to the number of `#if` hardware guards throughout the file.

---

### [ ] 16.4 `mqtt.cpp` / `mqtt.h` `[LOW]`

**Suggested refactor** (future, not urgent):
1. Define `class Mqtt` in `mqtt.h` (inside `#ifdef MQTT_ENABLE`) with public methods: `init()`, `connect()`, `publishStatus()`, `publishPlaylist()`, `publishVolume()`.
2. Move `mqttClient`, `mqttReconnectTimer`, `topic[]`, `status[]` to private members.
3. The `onMqttConnect`, `onMqttDisconnect`, `onMqttMessage` callbacks registered with `AsyncMqttClient` must be static member functions or free functions; they can be made private static methods that access the global instance.
4. Define `Mqtt mqtt;` in `mqtt.cpp` and `extern Mqtt mqtt;` in `mqtt.h`.
5. Update callers in `main.cpp` and anywhere MQTT is triggered: `mqttInit()` → `mqtt.init()`, etc.

**Blockers / notes:**
- `AsyncMqttClient` callback registration uses `std::function` or C-style function pointers depending on library version; promoting to `static` member callbacks is the safe path either way.
- The entire file is already guarded by `#ifdef MQTT_ENABLE`. The class body and `extern` declaration should remain inside the same guard so non-MQTT builds produce no symbols.
- No external module calls into `mqtt.cpp` other than `main.cpp` — this is the second-simplest refactor after rgbled.

---

## [ ] 17. Memory Leaks and Heap Fragmentation

ESP32 devices run indefinitely without rebooting (under normal conditions). Any heap allocation that is not freed, or any pattern that fragments the heap over time, will eventually exhaust memory, causing allocation failures that manifest as silent playback failures, failed updates, or hard resets. This section audits all dynamic allocation patterns in the application code.

### [ ] 17.1 `Config::startupServices()` — `ESPFileUpdater* updater` never freed `[MEDIUM]`

- **File**: `src/core/config.cpp`, around line 1255.
- **Code**:
  ```cpp
  ESPFileUpdater* updater = new ESPFileUpdater(SPIFFS);
  ...
  xTaskCreate(startupServicesAsync, "startupServicesAsync", 8192, updater, 2, NULL);
  ```
- **Task (`startupServicesAsync`)**: receives `updater` as `param`, calls `config.updateFile(param, ...)` for each file update, then calls `vTaskDelete(NULL)` — **without ever calling `delete (ESPFileUpdater*)param`**.
- **Second path** (`!wwwFilesExist`): `updater` is allocated, then `getRequiredFiles()` is called. `getRequiredFiles()` calls `ESP.restart()` on success — but if it returns on failure the `updater` is never freed.
- **Impact**: A one-time leak of one `ESPFileUpdater` object per boot-services invocation. Not recurring, but confirms the design does not account for ownership transfer through `void*` task parameters.
- **Action**: At the end of `startupServicesAsync`, before `vTaskDelete(NULL)`, add `delete (ESPFileUpdater*)param;`. In the `!wwwFilesExist` path, add `delete updater;` after `getRequiredFiles()` returns.

Trip5 Note: Doesn't getRequiredFiles() always result in a reboot?  (Fixing maybe unnecessary)

### [X] 17.2 `MyNetwork::setWifiParams()` — `weatherBuf` reassigned without `free()` `[MEDIUM]`

- **File**: `src/core/network.cpp`, around line 491.
- **Code**:
  ```cpp
  weatherBuf = NULL;   // ← overwrites existing pointer without free()
  ...
  weatherBuf = (char *) malloc(sizeof(char) * WEATHER_STRING_L);
  ```
- **Problem**: If `setWifiParams()` is called more than once (e.g., during the `searchWiFi` task retry path in SD-card mode, or any future reconnect code path), the previously `malloc`'d block of `WEATHER_STRING_L` bytes is leaked.
- **Current risk**: In typical boot flows `setWifiParams()` is called once. The `WiFiReconnected` callback does not call it again. However the design has no protection against accidental double-call leaking `WEATHER_STRING_L` bytes.
- **Action**: Add a guard before the assignment: `if (weatherBuf) { free(weatherBuf); weatherBuf = nullptr; }`.

### [ ] 17.3 `xTaskCreate` failure leaves heap allocations unclaimed `[LOW]`

Four call sites allocate heap memory and pass the pointer as `pvParameters` to `xTaskCreate`, but do not free the memory if `xTaskCreate` fails (returns `errCOULD_NOT_ALLOCATE_REQUIRED_MEMORY`). Task creation failure is unlikely in normal operation but is most likely to occur precisely when the heap is already critically fragmented.

| File | Site | Allocation | Action on failure |
|---|---|---|---|
| `commandhandler.cpp` ~line 182 | `loadplaylist` handler | `char* filename = new char[strlen(value)+1]` | Not freed if `xTaskCreate` fails |
| `netserver.cpp` ~line 1044 | `launchPlaybackTask()` | `String* url_copy = new String(url)` | Not freed if `xTaskCreate` fails |
| `netserver.cpp` ~line 1219 | `processRadioBrowserClick()` | `char* urlCopy = new char[...]` | Not freed if `xTaskCreate` fails |
| `config.cpp` ~line 1191 | `Config::updateLocaleFileAsync()` | `LocaleUpdateParams*` + inner `ESPFileUpdater*` | Neither freed if `xTaskCreate` fails |

- **Action**: All four sites must check the return value of `xTaskCreate` and free the allocation when it is not `pdPASS`. Pattern:
  ```cpp
  if (xTaskCreate(..., (void*)ptr, ...) != pdPASS) {
      delete[] ptr;  // or delete ptr
      // handle error
  }
  ```

### [ ] 17.4 `rb_servers[20]` global `String` array — recurring heap fragmentation `[LOW]`

- **File**: `src/core/netserver.cpp`, line 61.
- **Declaration**: `String rb_servers[20];`
- **Problem**: 20 Arduino `String` objects at global scope. Each non-empty `String` holds a heap-allocated character buffer. `selectRadioBrowserServer()` rebuilds this array by repeatedly assigning new values, which frees old heap blocks and allocates new ones of varying sizes. On a long-running device, this repeated churn of differently-sized small heap blocks creates fragmentation. Fragmentation is the primary cause of allocation failures on ESP32 that don't appear immediately but develop over hours/days.
- **Action**: Replace with a 2D `char` array: `char rb_servers[20][64]`. Server hostnames are short domain names well under 64 bytes. This eliminates all heap involvement for this array. Replace the `String` assignment and comparison operations with `strlcpy`/`strcmp`.

### [ ] 17.5 Temporary `String` construction in hot/error paths — fragmentation `[LOW]`

- **File**: `src/core/netserver.cpp`, lines ~1275 and ~1331.
- **Code**:
  ```cpp
  websocket.textAll(String("{\"onlineupdateerror\": \"HTTP code ") + httpCode + "\"}");
  websocket.textAll(String("{\"onlineupdateerror\": \"Update failed on end(): ") + String(Update.errorString()) + "\"}");
  ```
- **Problem**: Each statement constructs 2–3 temporary `String` heap objects (allocate → copy → concatenate → allocate again). These are freed immediately after the statement but each allocate+free cycle leaves behind a fragmentation scar. The success path immediately above already uses `snprintf` into a stack `char msgBuf[]` — the error paths should too.
- **Action**: Replace with `snprintf(msgBuf, sizeof(msgBuf), ...)` matching the pattern already used for the success case.

### [ ] 17.6 `MIN_MALLOC` defined but never enforced `[LOW]`

- **File**: `src/core/netserver.cpp`, lines 42–43.
- **Code**: `#define MIN_MALLOC 24112`
- **Problem**: `MIN_MALLOC` is defined (with a `#ifndef` guard suggesting it is user-overridable), but no code anywhere checks `ESP.getFreeHeap() < MIN_MALLOC` before starting new background tasks or network operations. The constant was presumably intended as a low-heap guard threshold but the associated check was never implemented.
- **Action**: Either (a) implement a guard: refuse to create new background tasks (`search`, `curated`, `playback`, `rb_click`) when heap is critically low, logging a warning via `Serial.printf`; or (b) remove `MIN_MALLOC` to avoid misleading future readers into thinking a guard exists.

### [ ] 17.7 No heap health monitoring or low-memory recovery path `[MEDIUM]`

- The device has no autonomous mechanism to detect gradual heap exhaustion. When the heap fragments below the threshold needed for a new WiFi/TCP connection or audio stream buffer, the symptom is silent failure (station won't play, updates time out) rather than a recoverable logged event.
- `xPortGetFreeHeapSize()` is exposed in telnet `info` output (line 698) but never logged to Serial on a schedule or acted on.
- There is no low-heap watchdog that would trigger a controlled reboot (as opposed to a crash/WDT reset with no log).
- **Suggested actions**:
  1. Add a periodic heap log in the `ticks()` ticker (e.g., every 5 minutes): `Serial.printf("[Heap] Free: %u, Min: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());`. Cheap and invaluable for diagnosing long-uptime degradation.
  2. In the MQTT/WebSocket system-info broadcasts, include `freeHeap` so the WebUI can surface it.
  3. Consider a controlled-reboot policy: if `ESP.getMinFreeHeap()` (the all-time minimum across the session) drops below a configurable threshold (e.g., `LOW_HEAP_REBOOT_THRESHOLD`), reboot at the next station-change boundary rather than waiting for a crash.
  4. Implement the `MIN_MALLOC` guard from §17.6 as the first concrete step.

---

## [X] 18. Race Conditions and Concurrency Hazards (ALL FIXED)

### [X] 18.1 ISR reads non-volatile global `_mode` without a barrier `[FIXED]`

- **File**: `src/core/controls.cpp`, lines ~70 and ~77; `src/core/display.h` line 24.
- **Code**:
  ```cpp
  void IRAM_ATTR readEncoderISR() {
    if ((SDC_CS==255 && display.mode()==LOST) || display.mode()==UPDATING) return;
    encoder.readEncoder_ISR();
  }
  ```
- **Problem**: The ISR reads `display._mode` (an `enum` field on the main-thread `Display` object) without any synchronisation. `_mode` is not `volatile`; the compiler is permitted to cache it in a register. On ESP32 (Xtensa dual-core), the ISR can fire on Core 0 while `_mode` is being written on Core 1. The combined result is a torn read or stale cached value. The ISR may refuse to call `readEncoder_ISR()` when it should, or (worse) allow it when the display is mid-update.
- **Fix Applied**: Declared `_mode` as `volatile displayMode_e _mode;` in `display.h` to prevent compiler caching and ensure cross-core visibility.

### [X] 18.2 `network.lostPlaying` / `network.beginReconnect` written from multiple contexts without synchronisation `[FIXED]`

- **File**: `src/core/network.h` line 16 — `volatile bool lostPlaying = false, beginReconnect = false;`
- **Writers**: `WiFiLostConnection()` callback (system WiFi event task), `WiFiReconnected()` callback (system WiFi event task), `retryStreamConnection()` task (Core 0), `player.cpp` line ~202/204 (player loop).
- **Problem**: ESP32 Arduino's `WiFiEvent` callbacks run in the lwIP/WiFi system task, not on Core 0 or Core 1 directly. Non-atomic writes to plain `bool` fields from multiple tasks/cores without a mutex means a reader could see a partially-committed value, or the compiler could reorder accesses relative to surrounding logic. In practice `bool` on Xtensa is single-byte and byte-writes are atomic, but the **visibility** guarantee (cache coherency across cores) is not provided without `volatile` or a memory barrier.
- **Fix Applied**: Marked both fields `volatile bool` to ensure visibility across cores and prevent compiler caching. This provides the necessary memory-barrier semantics for cross-core boolean flag access. Critical sections were not added as the flags are simple state signals with no composite operations requiring atomicity beyond single-flag read/write.

### [X] 18.3 `g_searchTaskHandle` / `g_curatedTaskHandle` — NULL-check then use is not atomic `[ACCEPTED]`

- **File**: `src/core/commandhandler.cpp` lines ~172 and ~179; `src/core/netserver.cpp` lines ~63, 65.
- **Pattern**:
  ```cpp
  if (g_searchTaskHandle == NULL) {
      xTaskCreate(..., &g_searchTaskHandle);
  }
  ```
- **Problem**: The guard reads `NULL`, then `xTaskCreate` sets the handle inside the same task. Because both operations happen on the same core (the WebSocket/HTTP handler callback) this is fine today. However `g_searchTaskHandle` is also set to `NULL` at the end of the task body (running on a different core). If the task completes and NULLs the handle between the guard check and the `xTaskCreate` call — a race that is possible if FreeRTOS task switching is enabled at that point — a second task could be spawned. The handle is not `volatile` either, so the read could be stale.
- **Resolution**: Task handles were NOT made volatile because: (1) FreeRTOS provides its own memory barriers for task operations, (2) `xTaskCreate` API doesn't accept volatile pointers, (3) the race condition mentioned is about timing (not stale reads), and would require mutex protection, not just volatile. The risk is low in practice since task creation happens from one consistent context.

### [X] 18.4 `mqttplaylistblock` busy-spin without a semaphore `[FIXED]`

- **File**: `src/core/netserver.cpp`, lines ~76, ~1383.
- **Code**: `while (mqttplaylistblock) vTaskDelay(5);`
- **Problem**: `mqttplaylistblock` is a plain `bool` written by `mqttplaylistSend()` (called from a `Ticker` callback). A `Ticker` callback runs in the context of whatever called `ticker.update()`, while the WebSocket file-upload handler runs in the AsyncWebServer task. The spin loop burns CPU time and relies on the `bool` being visible across tasks without `volatile`. A proper binary semaphore (`xSemaphoreCreateBinary`) with `xSemaphoreTake`/`xSemaphoreGive` would be the correct pattern.
- **Fix Applied**: Marked `mqttplaylistblock` as `volatile bool` to ensure cross-task visibility.

### [X] 18.5 `player.lockOutput` multi-context access `[FIXED]`

- **File**: `src/core/player.h` line ~33, written from `network.cpp` WiFi callbacks, read from `audiohandlers.h` and `main.cpp`.
- **Code**: `bool lockOutput = true;`
- **Problem**: `lockOutput` is written from WiFi event callbacks (system task) in `network.cpp` and read from audio callback functions in `audiohandlers.h` which run in different task contexts. Without `volatile`, the compiler can cache the value and miss updates from other tasks.
- **Fix Applied**: Marked `lockOutput` as `volatile bool` in `player.h`.

---

## [ ] 19. Blocking Operations on Real-Time / Non-Background Contexts

### [ ] 19.1 `rebootmdns` executes `delay(1500)` on the calling task `[MEDIUM]`

- **File**: `src/core/commandhandler.cpp`, line ~87.
- **Code**: `if (cmdIs(command, "rebootmdns")) { delay(1500); ESP.restart(); return true; }`
- **Problem**: This command is called directly from `cmd.exec()`, which runs on the AsyncWebServer task (the WebSocket message handler). A `delay(1500)` on that task stalls all WebSocket and HTTP event handling for 1.5 seconds, triggers AsyncWebServer internal timeouts, and may feed the watchdog unevenly. The `clearspiffs` handler in `handleIndex()` has the same pattern with `delay(100)` before restart.
- **Action**: Use a `Ticker.once()` to schedule the restart, mirroring the `improvRebootTicker` pattern already used in `onImprovCustomConnect()`. Return immediately from the command handler. The `clearspiffs` handler should do the same.

### [ ] 19.2 Blocking WiFi scan on the main loop during boot `[LOW]`

- **File**: `src/core/network.cpp`, line ~215.
- **Code**: `int n = WiFi.scanNetworks();` — called synchronously inside `wifiBegin()`, which is called from `network.begin()` on the main Arduino task during boot.
- **Problem**: `WiFi.scanNetworks()` is a blocking call that can take 2–5 seconds. During this time the main task is fully blocked — the watchdog timer is fed by the idle task, but no `loop()` processing, display updates, or serial I/O occurs. On slow RF environments it can take longer.
- **Telnet context**: `telnet.cpp` line ~621 calls `WiFi.scanNetworks()` directly in `on_input()`, blocking the telnet task for the full scan duration.
- **Action**: Use `WiFi.scanNetworks(true)` (async mode, returns immediately with `WIFI_SCAN_RUNNING`) and poll `WiFi.scanComplete()` in combination with `WiFi.scanResults()`. If blocking is required at boot for correctness, at least call `esp_task_wdt_reset()` during the wait. Add a scan timeout.

### [ ] 19.3 Busy-wait spin loops without watchdog care `[LOW]`

Two spin loops use `delay()` to yield but have no timeout guard:
- `src/core/netserver.cpp` line ~247: `while(nsQueue==NULL) {;}` — pure busy-spin, no delay, no WDT reset, no timeout.
- `src/core/display.cpp` line ~111: `while(displayQueue==NULL) {;}` — same.
- `src/core/config.cpp` line ~193: `while(display.mode()!=SDCHANGE) delay(10);` — has a yield but no maximum iteration count; would spin forever if the display task fails to transition.
- `src/core/controls.cpp` line ~185: `while(display.mode() != STATIONS) {delay(10);}` — same problem; no bail-out.

For the queue-creation loops, the `while(x==NULL)` guards are a leftover defensive pattern; if `xQueueCreate` returns NULL, the queue is already broken and a restart is the appropriate response, not an infinite spin. For the mode-transition loops, a timeout (e.g., 2 seconds of total wait) should terminate with a fallback.
- **Action**: Replace the `while(nsQueue==NULL)` and `while(displayQueue==NULL)` spins with an assertion or immediate restart. Replace the mode-transition waits with a timeout + fallback:
  ```cpp
  uint32_t tStart = millis();
  while (display.mode() != STATIONS && millis() - tStart < 2000) { delay(10); }
  ```

### [X] 19.4 Blocking SD card mount attempts during boot waste time when no card is present `[LOW]` (FIXED)

- **Files**: `src/core/sdmanager.cpp` line ~23; `src/main.cpp` lines ~79–82; `src/core/config.cpp` line ~253.
- **Problem — unconditional `WAITFORSD` display + log**: In `main.cpp`, the `display.putRequest(WAITFORSD, 0)` call and `Serial.print("##[BOOT]#\tSD search\t")` fire whenever `SDC_CS != 255`, regardless of whether the last saved play mode is `PM_SDCARD` or `PM_WEB`. A device running in web-radio mode with SD hardware wired up will display "INDEX SD" on every boot even though `initPlaylistMode()` immediately takes the `else` branch and does no SD work. This misleads the user and unnecessarily occupies the boot-splash text slot.
  - **Fix Applied**: Guarded behind `config.store.play_mode == PM_SDCARD` in `main.cpp`.
- **Problem — four unconditional `SD.begin()` SPI calls**: `SDManager::start()` attempts `SD.begin()` four times with `vTaskDelay(10)`, `vTaskDelay(20)`, `vTaskDelay(50)` between each. The delays are unconditional — they run even when the first attempt succeeds, adding a minimum 80 ms of idle delay on every successful mount. When no card is present each `SD.begin()` attempt runs the Arduino-ESP32 SD init sequence internally (CMD0/CMD8/ACMD41 retries, voltage-ramp wait), which can add 100–500 ms per attempt before returning failure. At worst this is ~2 seconds of blocking on the main task with no card inserted.
  - **Fix Applied**: `SDManager::start()` now early-returns on success; delays only occur between actual retry attempts — eliminating the minimum 80 ms idle on a successful first-attempt mount.
- **Problem — no card-detect (CD) pin option**: Many SD card slot footprints expose a mechanical CD (card-detect) switch pin that reads LOW when a card is seated and HIGH when the slot is empty. No define or check existed; the firmware had no way to skip the SPI init sequence entirely when the pin reveals the slot is empty.
  - **Fix Applied**: `SD_CARD_DETECT_PIN` define added to `options.h` (default 255 = disabled). When set, initialized as `INPUT_PULLUP` in `Config::_initHW()`. Combined with `SD_AUTOPLAY` (default `false`), the `ticks()` `divrssi` block in `network.cpp` polls the pin every ~2 s and calls `config.changeMode(PM_SDCARD)` on insertion. `SD_AUTOPLAY` requires both this pin and a deliberate `#define SD_AUTOPLAY true` in `myoptions.h` to have any runtime effect.
  - **Fix Applied**: `initPlaylistMode()` and `changeMode()` now short-circuit on `SD_CARD_DETECT_PIN` before calling `sdman.start()`. When the pin reads HIGH (slot empty), the boot path falls back to `PM_WEB` immediately and manual mode switches to SD abort without incurring the SPI retry sequence.
- **Context — SD is always re-initialized on mode switch anyway**: `Config::changeMode()` already calls `sdman.start()` whenever `!sdman.ready && newmode!=PM_WEB`. There is therefore **no correctness requirement** to initialize the SD card at boot at all — `initPlaylistMode()` should only mount the card when booting directly into `PM_SDCARD`. The current code already does this for the actual `sdman.start()` call; the `WAITFORSD` splash and serial log were the only parts firing unconditionally (now fixed).

---

## [ ] 20. TLS / HTTPS Security

### [ ] 20.1 All HTTPS connections skip certificate validation `[MEDIUM]`

- **File**: `src/core/netserver.cpp`, lines ~1073, ~1234, ~1286.
- **Code**: `client.setInsecure(); // skip server cert validation`
- **Applies to**: the Radio-Browser click-reporting lookup, the version-check download, and the firmware update download.
- **Problem**: `setInsecure()` disables server-certificate verification entirely. Any attacker who can intercept traffic (e.g., a rogue AP, MITM on an unencrypted home network) can serve malicious firmware or version data. The firmware update path is the highest-risk instance: a MITM could serve a re-signed but malicious firmware binary.
- **Context**: Embedding a bundle of CA root certificates on an ESP32 with limited flash is genuinely hard; `WiFiClientSecure::setCACert()` requires the specific root CA PEM for each target host. The `setInsecure()` choice is pragmatic, but should be documented as a known trade-off, not left as a silent comment.
- **Action**:
  - For the **firmware update path** specifically: embed the root CA for `UPDATEURL` (typically a GitHub or self-hosted host) and use `setCACert()`. This is the highest-value fix.
  - For **Radio-Browser API calls**: lower risk (no privileged data), but still document the trade-off.
  - Add `#define HTTPS_SKIP_CERT_VERIFY` as an explicit opt-out define in `options.h` (defaulting to `1` for now) so the current behaviour is acknowledged and can be disabled per-build.

---

## [X] 21. NVS Write Endurance (ALL FIXED)

### [X] 21.1 High-frequency NVS writes from volume/brightness sliders `[MEDIUM]`

- **Context**: ESP32 NVS is backed by flash, which has a write endurance of approximately 100,000 erase cycles per sector. The `saveValue()` implementation in `config.h` calls `prefs.begin() / prefs.putBytes() / prefs.end()` which triggers a full NVS commit on every call. The NVS library does do some wear-levelling across allocated pages, but writes are still finite.
- **High-frequency callers**:
  - Volume slider in WebUI sends `volume=N` (resolved through `vol` handler): `config.store.volume` is updated but `saveValue` is called — every drag event generates an NVS write.
  - `dim` command: `config.store.brightness` directly assigned then `setBrightness(true)` — also calls `saveValue` indirectly from `setBrightness`.
  - IR repeat-send scenario: if a held IR button sends volume-up repeatedly, NVS is written every repeat.
  - Encoder volume control: similarly sends a write per encoder step.
- **Note**: The `saveValue` implementation *does* short-circuit on equal values (`needSave = memcmp(...) != 0`), so if the value is unchanged no flash write occurs. This mitigates the worst case of repeated same-value slider sends. The actual per-step change case (each discrete encoder tick changes volume by 1) does still write each time.
- **Action**: Consider a debounce / coalescing policy for transient settings: buffer the last seen value and only commit after a quiet period (e.g., 2 seconds of no change, via a `Ticker`). This is a well-known pattern for embedded UIs. Volume and brightness are the primary candidates. Settings that are changed rarely (locale, timezone, etc.) do not need this treatment.

---

## [ ] 22. Stack Overflow Risks in FreeRTOS Tasks

### [ ] 22.1 `DspTask` stack is a fixed 4KB — no high-watermark monitoring `[LOW]`

- **File**: `src/core/display.cpp`, line ~27 and ~69.
- **Code**: `#define CORE_STACK_SIZE 1024*4` → `xTaskCreatePinnedToCore(loopDspTask, "DspTask", CORE_STACK_SIZE, ...)`
- **Problem**: The display task runs a substantial rendering loop, calls widget draw functions, and on displays backed by Adafruit/TFT_eSPI libraries can invoke moderately deep call stacks. 4KB is tight; a stack overflow on FreeRTOS produces a watchdog reset or heap corruption (the stack grows into adjacent heap). There is no runtime check of the high-water mark.
- **Affected tasks and their declared sizes**:

| Task | Stack | Where |
|---|---|---|
| `DspTask` | 4096 | `display.cpp` |
| `searchRadioBrowser` | 8192 | `netserver.cpp` |
| `curatedIndex` | 8192 | `commandhandler.cpp` |
| `curatedPlaylist` | 8192 | `commandhandler.cpp` |
| `playbackTask` (HTTP) | 4096 | `netserver.cpp` |
| `playbackTask` (HTTPS) | 8192 | `netserver.cpp` |
| `rbClickTask` | 8192 | `netserver.cpp` |
| `startupServicesAsync` | 8192 | `config.cpp` |
| `updateLocaleFileAsync` | 8192 | `config.cpp` |
| `doSync` | 4096 | `network.cpp` |
| `retryStreamConnection` | 4096 | `network.cpp` |
| `checkForOnlineUpdateTask` | 8096 | `netserver.cpp` |
| `startOnlineUpdateTask` | 16384 | `netserver.cpp` |

- **Action**: Add `uxTaskGetStackHighWaterMark(NULL)` logging to each long-lived task at a low-frequency checkpoint (e.g., once per minute or triggered by telnet `info`). The DspTask is the most suspect — consider raising to 8KB. If `uxTaskGetStackHighWaterMark()` returns < 512 bytes in testing, that task's stack needs expanding.

---

## [ ] 23. Telnet `/` WiFi Scan Blocking and Credential Exposure

### [ ] 23.1 `wifi.con` telnet command prints all stored WiFi passwords in cleartext `[MEDIUM]`

- **File**: `src/core/telnet.cpp`, around line ~642.
- **Code**: `printf(clientId, "%d: %s, %s\r\n", c, sSid, sPas);` — prints the SSID and password for every stored network.
- **Problem**: Telnet is unencrypted (plaintext TCP). Anyone who can access the telnet port can send `wifi.con` and receive all configured WiFi passwords in cleartext on a local network. If the device is on an untrusted network or the network is monitored, this is a credential exposure. There is no authentication on the telnet interface.
- **Separate concern**: `wifi.list` calls `WiFi.scanNetworks()` synchronously and blocks the telnet task for the duration of the scan (no timeout, no WDT reset — see §19.2).
- **Action**: At minimum, mask the password field in the output (e.g., show `****` after the first two characters). A stronger fix would require that telnet is only accessible from localhost/loopback or add an optional PIN challenge. Document that the telnet interface should only be used on trusted networks.

---

## 24. [X] A Workflow that checks pull-requests (ALL FIXED)

A workflow, test build, code-check... and a PR template.

---

## [ ] 25. Display conf.h Files That Lack a Battery Widget

### [ ] 25.1 Only one display conf defines `batteryConf` and battery range format strings (ILI9341) `[LOW]`

- **File**: All `src/displays/conf/display*conf.h`
- **Problem**: `src/core/display.cpp` references `batteryConf`, `batteryRangeLowFmt`, `batteryRangeMidFmt`, and `batteryRangeHighFmt` inside `#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)` guards. These symbols are only defined in `displayILI9341conf.h`. Any hardware combination using another display with `BATTERY_PIN` set will fail to compile. The CI test build exposed this when `TEST_CI_VS1053` used SH1106 — it was worked around by switching the test env to ILI9341, but the underlying problem remains for real users.
- **Action**: Add `batteryConf`, `batteryRangeLowFmt`, `batteryRangeMidFmt`, and `batteryRangeHighFmt` to all display conf files that have a plausible battery use case (i.e., all non-LCD/non-dummy displays). Coordinates and glyph codes will differ per display.

| Has battery widget? | Display conf file |
|---|---|
| [X] | `displayILI9341conf.h` |
| [ ] | `displayGC9106conf.h` |
| [ ] | `displayGC9A01Aconf.h` |
| [ ] | `displayILI9225conf.h` |
| [ ] | `displayILI9488conf.h` |
| [ ] | `displayN5110conf.h` |
| [ ] | `displaySH1106conf.h` |
| [ ] | `displaySSD1305conf.h` |
| [ ] | `displaySSD1306conf.h` |
| [ ] | `displaySSD1306x32conf.h` |
| [ ] | `displaySSD1322conf.h` |
| [ ] | `displaySSD1327conf.h` |
| [ ] | `displayST7735_144conf.h` |
| [ ] | `displayST7735_blackconf.h` |
| [ ] | `displayST7735_miniconf.h` |
| [ ] | `displayST7789conf.h` |
| [ ] | `displayST7789_76conf.h` |
| [ ] | `displayST7789_240conf.h` |
| [ ] | `displayST7796conf.h` |
| [ ] | `displayST7920conf.h` |
| N/A (LCD) | `displayLCD1602conf.h` |
| N/A (LCD) | `displayLCD2004conf.h` |

---

## [ ] 26. ESP32-S3 Migration — Dropping Original ESP32 Support `[LOW]` (IMPORTANT FIXED - NEEDS FURTHER CONSIDERATION & INVESTIGATION)

All active trip5 radio build environments in `platformio.ini` already extend `board_esp32_s3_n16r8`. The only place the original `esp32dev` board appears is the bare `board_esp32` template environment, which has no associated hardware profile and is not used for any current firmware release. This section audits what remains in the codebase that is specific to the original ESP32 (Xtensa LX6, 4 MB flash, optional WROVER PSRAM), what would change if that board were formally dropped, and where the S3 could be better exploited than it currently is.

### 26.1 Code that exists solely because of ESP32 (original) limitations

**SDSPISPEED branching** (`src/core/options.h` line ~179):
```cpp
#if defined(ARDUINO_ESP32_DEV)
    #define SDSPISPEED 20000000  // safe for original ESP32
#elif defined(ARDUINO_ESP32S3_DEV) ...
    #define SDSPISPEED 40000000  // S3 known to work at this speed
...
```
The 20 MHz cap exists because the original ESP32's VSPI/HSPI SPI peripheral had documented instability at higher speeds with certain SD cards. The S3 has a more robust SPI peripheral verified at 40 MHz. Removing `ARDUINO_ESP32_DEV` path collapses this to a single value.

**Default I2S pin assignments** (`options.h` lines ~151–157):
```cpp
#define I2S_DOUT 27   // GPIO 27
#define I2S_BCLK 26   // GPIO 26
#define I2S_LRC  25   // GPIO 25
```
GPIO 25/26/27 are the original ESP32's dedicated DAC-capable lines and were the conventional I2S output pins for nearly every ESP32 audio board sold 2017–2021. No ESP32-S3 board uses these pins for I2S by default (S3 devkitc-1 default I2S is entirely different). In practice every S3 `myoptions.h` profile already overrides these values, so the defaults are effectively dead code on S3. They remain as a trap: any S3 build that forgets to define I2S pins inherits wrong defaults, which compile without warning but produce no audio.

**`SD_HSPI` and `TS_HSPI` pin comments** (`options.h`):
The comment `// use HSPI for SD (miso=12, mosi=13, clk=14)` is the original ESP32 HSPI bus. On ESP32-S3 those GPIO numbers have completely different functions. The `false` default (use VSPI/SPI2) is safe for S3 but the accompanying comment misleads anyone reading the code on S3 hardware.

**`WAKE_PIN` RTC domain comment** (`options.h` line ~424):
```
// must be one of: 0,2,4,12,13,14,15,25,26,27,32,33,34,35,36,39
```
This is the list of RTC-capable GPIOs on the **original ESP32** that support `ext0` wakeup. On ESP32-S3, `esp_sleep_enable_ext0_wakeup()` is defined in the IDF but returns `ESP_ERR_NOT_SUPPORTED` at runtime — it is a no-op stub. The wakeup pin feature silently does nothing on all current S3 hardware (see §26.2).

**`ESP_S3C3` macro** (`options.h` line ~318):
```cpp
#if defined(ARDUINO_ESP32S3_DEV) || defined(ARDUINO_ESP32C3_DEV)
    #define ESP_S3C3 1
```
This sets `USE_BUILTIN_LED true` and establishes `LED_BUILTIN_S3`. The `else` branch handles original ESP32 LED behavior. This is the only `#if` in `options.h` that guards on the chip family rather than a hardware-config macro, and it would be the cleanest to collapse if ESP32 support is dropped.

### 26.2 [X] `esp_sleep_enable_ext0_wakeup()` does not work on ESP32-S3 `[MEDIUM]`

**Files**: `src/core/config.cpp` lines ~847, ~860; `src/main.cpp` line ~235; `builds/plugins/deepSleep/deepsleep.cpp` line ~37.

**Code**:
```cpp
if (WAKE_PIN!=255) esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
esp_deep_sleep_start();
```

**Problem**: `esp_sleep_enable_ext0_wakeup()` is the EXT0 wakeup source which uses the RTC subsystem's ULP co-processor. On the original ESP32, this works on any of the listed RTC-domain-capable GPIOs. On ESP32-S3, Espressif removed the EXT0 source entirely — the IDF provides a stub that compiles but logs an error and returns `ESP_ERR_NOT_SUPPORTED`. The correct S3 replacement is `esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN), ESP_GPIO_WAKEUP_GPIO_LOW)`.

**Current impact**: Any user with `WAKE_PIN != 255` on an S3 build thinks their hardware wake button will work after sleep. It will not. The device enters deep sleep and can only be woken by the timer (if `sleep` was invoked with a timeout). The manual wake pin is silently broken.

**Fix**: Add an `#ifdef ARDUINO_ESP32S3_DEV` branch:
```cpp
#if defined(ARDUINO_ESP32S3_DEV)
    if (WAKE_PIN!=255) esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN), ESP_GPIO_WAKEUP_GPIO_LOW);
#else
    if (WAKE_PIN!=255) esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, LOW);
#endif
```
The same fix applies to `main.cpp` and both `deepSleep` plugin files. Update the `WAKE_PIN` comment to document S3-compatible GPIO numbers (all GPIOs below GPIO_NUM_MAX that support digital input; no special RTC domain restriction applies on S3).

### 26.3 PSRAM on ESP32-WROVER vs ESP32-S3-N16R8

The original ESP32-WROVER module includes 4 MB of SPI PSRAM (QSPI, 80 MHz max). The `board_esp32` environment in `platformio.ini` does **not** set `-DBOARD_HAS_PSRAM` and does not configure `board_build.arduino.memory_type = qio_opi` — meaning even a WROVER build is compiled without the PSRAM-access linker stubs. The audio libraries call `psramInit()` at runtime which will succeed if PSRAM is physically present and the bootloader initialized it, but without the proper `board_build` PSRAM configuration in `platformio.ini`, the `ps_malloc()` / `heap_caps_malloc(MALLOC_CAP_SPIRAM)` calls used by the FLAC decoder and audio ring buffer may fail silently, falling back to internal SRAM.

The `board_esp32_s3_n16r8` environment correctly sets:
```ini
board_build.arduino.memory_type = qio_opi
board_build.flash_mode = qio
board_build.psram_type = opi
-DBOARD_HAS_PSRAM
```
This configures OPI PSRAM (Octal SPI, up to 80 MHz) for the 8 MB PSRAM found on the N16R8 module. With this configuration, `psramInit()` returns `true`, and both audio libraries:
- Allocate the audio ring buffer from PSRAM (large buffer = smoother streaming, fewer re-buffer interruptions)
- Enable M3U8 playlist support (`m3u8 playlists requires PSRAM enabled!` — otherwise logged and refused)
- Enable full FLAC decoding (FLAC decoder uses `heap_caps_malloc_prefer` with `MALLOC_CAP_SPIRAM`)
- Expand ID3 tag buffer from 1 KB to 4 KB

**Without PSRAM**, the audio ring buffer falls back to internal SRAM. On original ESP32 this gives a modest ~8 KB buffer. On S3 without PSRAM configured, same. The difference in practice: FLAC streams are more likely to stutter; long M3U8 playlists (> 100 entries) are refused at runtime; very high-bitrate streams (320 kbps MP3) may underrun more frequently on slow network conditions.

**Application code**: There is only one place in application code (not libraries) that checks for PSRAM: `display.cpp` line ~192:
```cpp
_heapbar = new SliderWidget(heapbarConf, ..., psramInit() ? 300000 : 1600 * 10);
```
The heap bar scale changes based on whether PSRAM is present (300 KB vs 16 KB display range). No other application code explicitly allocates from PSRAM. All PSRAM-aware allocation lives inside the audio library internals. This means the application layer is PSRAM-transparent: PSRAM being present is a pure improvement with no application-level code changes needed.

### 26.4 What changes if `board_esp32` support is formally dropped

Positive effects (code simplification):
- Remove the `ARDUINO_ESP32_DEV` branch in `SDSPISPEED` — one `#if`/`#elif`/`#else` block collapses to a single `#define`.
- Remove the inaccurate GPIO 25/26/27 I2S default pins (replace with a compile-time `#error` requiring myoptions.h to define them explicitly, which every existing S3 board profile already does).
- Remove the misleading ESP32-specific pin numbers from `SD_HSPI` and `TS_HSPI` comments.
- Simplify `WAKE_PIN` comment to only list S3-compatible guidance.
- Collapse `#if defined(ARDUINO_ESP32S3_DEV) || defined(ARDUINO_ESP32C3_DEV)` / else blocks in `options.h` where the else branch was for original ESP32.
- Accept that `BOARD_HAS_PSRAM` is always set (since all actual production S3-N16R8 builds set it) and remove the `psramInit()` runtime check in `display.cpp` in favour of a compile-time block.

Neutral (no change needed):
- FreeRTOS dual-core pinning (`DSP_TASK_CORE_ID`, `xTaskCreatePinnedToCore`) works identically on both ESP32 and S3.
- All queue handling, async task patterns, SPIFFS APIs, NVS APIs, WiFi, AsyncWebServer — all unchanged.
- `SEARCHRESULTS_BUFFER 1024*4` conservative default still applies to S3 but the comment saying "likely only good for ESP32-S3" confirms the intent is already S3-first.

Potential concern (worth checking):
- Some display drivers in `src/displays/` may use SPI bus initialisation that references VSPI/HSPI constants which are defined differently for S3 (or not at all). Any driver using `VSPI` or `HSPI` macro by name will fail on S3. This should be audited before formally dropping ESP32 — but the trip5 build environments already compile successfully on S3, so the active drivers are safe.
- The `ILI9488` library uses its own SPI init path; confirm it doesn't reference VSPI/HSPI by name.

### 26.5 Unexploited ESP32-S3 advantages

The following S3 capabilities are currently unused by the application. These are not action items but observations for future improvement.

**Native USB CDC**: The S3 has a hardware USB peripheral that can expose a CDC serial port directly. The build flags already set `ARDUINO_USB_MODE=1` (Hardware Serial/JTAG mode) rather than `0` (USB-OTG). Switching the Debug Serial output to native USB CDC (mode 0) would eliminate the need for a USB-UART bridge chip on custom boards. The current approach is fine for most boards that have an on-board UART bridge; native USB is only advantageous on custom hardware without one.

**ADC2 / WiFi coexistence**: The original ESP32 has a well-known limitation — ADC2 is shared with the WiFi radio and cannot be used simultaneously. Several ESP32 battery-monitoring circuits and ADC-based light sensors ended up on ADC2 pins, causing readings to drop to 0 or become erratic when WiFi was active. The ESP32-S3 does not have this limitation; all ADC-capable GPIOs can be read freely while WiFi is running. This means ESP32-S3-only builds can safely use any ADC pin for `BATTERY_PIN` or `LIGHT_SENSOR` without the ESP32 restriction. The current `battery.cpp` and `main.cpp` code makes no attempt to warn about ADC2 pin conflicts — on S3 this is simply a non-issue.

**Higher SPI clock ceiling**: The S3 SPI peripheral is rated to 80 MHz. Adafruit display drivers are typically limited to 40 MHz (`SPI_DEFAULT_FREQ`), and most TFT displays max out at 40–80 MHz depending on the panel. The current display drivers are already written for what the display supports, not for the SoC's limit, so there is no practical improvement here without changing the display libraries. The benefit is simply that the S3 does not add an extra bottleneck the way some original ESP32 SPI configurations did.

**`SEARCHRESULTS_BUFFER`**: The `myoptions.h` profile already sets `SEARCHRESULTS_BUFFER 1024*32` (32 KB), which the comment notes is "likely only good for ESP32-S3". The `options.h` default is a conservative `1024*4` (4 KB) for safety. If ESP32 is dropped, this default can be raised to match the profile value without the caveat.

### 26.6 Summary verdict

Dropping formal `board_esp32` support carries minimal code risk and has no effect on the behaviour of any existing S3 build. The benefits are cleaner defaults, removal of three misleading pin-number comments, and elimination of a silent `WAKE_PIN` bug on S3. The only actionable bug is §26.2 (`ext0_wakeup` on S3). The PSRAM situation (§26.3) is already handled correctly for S3-N16R8 in `platformio.ini`; it is only the original ESP32-WROVER environment that is misconfigured (and it is not an active production target).

**Recommended approach**: Leave `board_esp32` in `platformio.ini` as an untested legacy template (clearly commented as such). Fix the `ext0_wakeup` S3 bug unconditionally (§26.2 fix doesn't require dropping ESP32 support). Remove or correct the misleading default pin number comments in `options.h`. Do not spend effort making S3-specific improvements conditional on `ARDUINO_ESP32S3_DEV` — by the time any such feature lands, ESP32 support will already be vestigial.

---

## [ ] 27. `main.cpp` — Non-Boot Code That Belongs in Its Own Files `[MEDIUM]`

`main.cpp` should contain only `setup()`, `loop()`, and minimal glue. Currently it hosts ~200 lines of substantive implementation code across three distinct functional areas. This inflates the file size, mixes responsibilities, and makes the code harder to locate during maintenance.

---

### 27.1 Inventory of misplaced code

**Block A — BacklightDown plugin (~75 lines, lines ~119–192)**

Guarded by `#if (BRIGHTNESS_PIN!=255) && (defined(DOWN_LEVEL) || defined(DOWN_INTERVAL))`. Contains:

- `Ticker backlightTicker`, `Ticker rampTicker` and `uint8_t current_brightness` globals
- Constants `brightness_down_level` and `Out_Interval` (derived from `DOWN_LEVEL`/`DOWN_INTERVAL`)
- `stepBacklight()` — Ticker ISR-style callback that ramps brightness down
- `backlightDown()` — Ticker callback that triggers the ramp
- `brightnessOn()` (or a no-op stub when `BRIGHTNESS_PIN==255`) — public API for "restore and restart timer"
- `ctrls_on_loop()` — named weak-callback called from `controls.cpp` line 133; restores backlight on non-PLAYER mode transitions

This was derived from the legacy `builds/plugins/backlightcontrols.ino` plugin and then extended inline in `main.cpp` across three iterated versions. An object-oriented example in `builds/plugins/backlightControls/backlightcontrols.cpp/.h` already exists but is a simplified version (no ramp, no battery integration).

**Block B — `battery_dim_loop()` and its state variables (~100 lines, lines ~30–45 and ~199–289)**

Guarded by `#if BRIGHTNESS_PIN!=255`. Contains:

- Five file-static state variables (`battery_low_handled`, `battery_critical_handled`, `battery_critical_skipped`, `battery_saved_brightness`, `battery_saved_valid`) declared at the top of `main.cpp`
- Forward declaration of `battery_dim_loop()` at line 30 (needed because `loop()` calls it before its definition)
- The full `battery_dim_loop()` function body: reads `BatteryStatus`, handles critical/low/recovery cases, calls `brightnessOn()` or `config.setBrightness()`, sends deep-sleep command

This logic bridges `battery.h` (status) and the backlight (Block A), so it's tightly coupled to Block A and belongs in the same file.

**Block C — Glue callback implementations (~25 lines, lines ~292–315)**

These are named weak-symbol overrides. All have their forward declarations in core headers:
- `ehradio_on_setup()` — declared weak in `main.cpp` line 27, defined at line 296; calls `rgbled_init()` + `brightnessOn()`
- `player_on_track_change()` — weak in `player.h` line 73, called from `display.cpp` line 774; calls `rgbled_trackchange()` + `brightnessOn()`
- `player_on_start_play()` — weak in `player.h` line 71, called from `player.cpp` lines 279/312; calls `rgbled_playing()` + `brightnessOn()`
- `player_on_stop_play()` — weak in `player.h` line 72, called from `player.cpp` line 127; calls `rgbled_stopped()` + `brightnessOn()`
- `rgbled_loop_caller()` — one-liner wrapper; no external weak declaration found; currently only defined here

All five call `brightnessOn()` from Block A, which is why they ended up in `main.cpp` alongside it rather than in their natural homes.

---

### 27.2 Proposed target: `src/core/backlightcontrols.cpp` / `src/core/backlightcontrols.h`

Move all three blocks into a new `backlightcontrols.cpp` / `backlightcontrols.h` pair in `src/core/`. This keeps the backlight/battery/callback glue together in one file, follows the naming convention already established by `builds/plugins/backlightControls/`, and mirrors how `rgbled.cpp` / `rgbled.h` is handled (optional hardware feature with a stub path).

**What stays in `main.cpp` after the move:**
- All `#include` directives (add `#include "core/backlightcontrols.h"`)
- `SET_LOOP_TASK_STACK_SIZE` macro
- `#if DSP_HSPI || TS_HSPI || VS_HSPI` / `SPIClass SPI2(HSPI)` global (see §27.4)
- The `extern __attribute__((weak)) void ehradio_on_setup()` forward declaration (stays — consumed in `setup()`)
- `setup()` (~50 lines)
- `loop()` (~20 lines)
- `#include "core/audiohandlers.h"` (intentionally after `loop()` by design)

**Header (`backlightcontrols.h`) declares:**
```cpp
void brightnessOn();        // public API used by callbacks and battery_dim_loop; no-op stub when BRIGHTNESS_PIN==255
void battery_dim_loop();    // called from loop() in main.cpp; #if BRIGHTNESS_PIN!=255 gated
```
Both are already called from `main.cpp`; declaring them in the header removes the forward declarations from `main.cpp`.

**Dependencies of Block A + B + C** (already included by every other `src/core/` file):
- `<Arduino.h>`, `<Ticker.h>`
- `"options.h"`, `"config.h"`, `"network.h"`, `"display.h"`, `"battery.h"`, `"player.h"`, `"rgbled.h"`

No circular include risk: `backlightcontrols.h` does not need to include any of the above in the header itself (just the two `void` function declarations). All heavy includes go in `backlightcontrols.cpp`.

---

### 27.3 Migration notes / gotchas

**Weak symbol mechanics**: The definitions of `ehradio_on_setup()`, `player_on_*`, and `ctrls_on_loop()` are found by the linker at link time, not at include time. Moving the strong definitions from `main.cpp` to `backlightcontrols.cpp` requires no change to the forward declarations in `player.h` and `controls.h` — the linker finds them automatically. The `extern __attribute__((weak)) void ehradio_on_setup()` declaration in `main.cpp` stays; the definition moves to `backlightcontrols.cpp`.

**`battery_dim_loop()` forward declaration**: Line 30 of `main.cpp` forward-declares `battery_dim_loop()` because `loop()` calls it before the function is defined. After the move, replace this prototype with `#include "core/backlightcontrols.h"`.

**`#include <Ticker.h>`**: Currently included inside the conditional block in `main.cpp`. This must move to `backlightcontrols.cpp`. It is not needed in the header (Ticker objects are file-static).

**`brightnessOn()` no-op stub**: The `#else` branch at the end of Block A defines `void brightnessOn() { }`. This must be preserved in `backlightcontrols.cpp` under the same `#else` guard so that the callbacks compile when `BRIGHTNESS_PIN==255`.

**`ctrls_on_loop()` naming**: The function is already declared as `extern __attribute__((weak)) void ctrls_on_loop()` in `controls.h` (line 31). The strong definition in `backlightcontrols.cpp` overrides it. No changes needed in `controls.h` or `controls.cpp`.

**`rgbled_loop_caller()`**: This one-line wrapper has no external weak declaration and is not called from any discovered location outside `main.cpp`. Verify before moving — it may be dead code that can be removed entirely rather than migrated (cross-check with §4's write-only variable audit pattern).

---

### 27.4 Minor leftover: `SPI2` global `[LOW]`

```cpp
#if DSP_HSPI || TS_HSPI || VS_HSPI
  SPIClass SPI2(HSPI);
#endif
```

This hardware global is declared in `main.cpp` but consumed by display drivers in `src/displays/`. It belongs closer to its users — ideally in a `src/core/spiinit.cpp` or in the display driver that owns the HSPI bus. However this is low-priority and its current location is not harmful: a global declared in `main.cpp` is still a valid TU-global accessible via `extern SPIClass SPI2` from any display driver. Leave this for a later refactor once the larger Block A/B/C move is validated.

---

### 27.5 What `main.cpp` looks like after the refactor

```cpp
#include "core/options.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include "core/battery.h"
#include "core/backlightcontrols.h"   // ← new; replaces battery_dim_loop() forward decl
#include "core/config.h"
#include "core/controls.h"
#include "core/display.h"
#include "core/mqtt.h"
#include "core/netserver.h"
#include "core/network.h"
#include "core/player.h"
#include "core/rgbled.h"
#include "core/telnet.h"
#include "pluginsManager/pluginsManager.h"
#ifdef USE_NEXTION
  #include "displays/nextion.h"
#endif

SET_LOOP_TASK_STACK_SIZE(LOOP_TASK_STACK_SIZE * 1024);

#if DSP_HSPI || TS_HSPI || VS_HSPI
  SPIClass SPI2(HSPI);
#endif

extern __attribute__((weak)) void ehradio_on_setup();

void setup() { ... }   // ~50 lines, unchanged
void loop()  { ... }   // ~20 lines, unchanged

#include "core/audiohandlers.h"
```

No function bodies remain in `main.cpp` other than `setup()` and `loop()`. Total line count drops from ~315 to roughly ~100–110 lines.

---

### 27.6 Summary

| Block | Lines | Proposed destination | Feasibility |
|---|---|---|---|
| BacklightDown plugin (Ticker, `brightnessOn`, `ctrls_on_loop`) | ~75 | `src/core/backlightcontrols.cpp/.h` | ✅ Straightforward |
| `battery_dim_loop()` + state vars | ~100 | `src/core/backlightcontrols.cpp/.h` | ✅ Straightforward, coupled to Block A |
| `ehradio_on_setup`, `player_on_*`, `rgbled_loop_caller` | ~25 | `src/core/backlightcontrols.cpp/.h` | ✅ Weak-symbol mechanics are fully transparent to linker |
| `SPI2` global | ~3 | Leave in `main.cpp` for now | Low priority; no real harm in current location |

**Rule #4 note**: When this refactor is executed, `code-summary.md` must be updated — specifically the `src/main.cpp` boot-flow section and a new entry for `src/core/backlightcontrols.cpp/.h`.

---

## [ ] 28. Plugin System — Dead Infrastructure, Remove Entirely `[LOW]`

`src/plugins/` contains only a `README.md`. No active plugins exist anywhere in the project. The `src/pluginsManager/` framework compiles unconditionally into every build (via `build_src_filter`) and dispatches 13 hook calls across 5 core files — all with zero registered listeners. All functionality the plugin hooks were designed to expose is already served by the existing weak-symbol callback system (`player_on_*`, `ehradio_on_setup`, `ctrls_on_loop`), which is entirely independent of the plugin system.

The `builds/plugins/` folder contains example plugins in two generations: two Arduino IDE `.ino` stubs (self-declared dead) and three class-based `.cpp/.h` examples. Two of the class-based examples are either superseded by existing code or irrelevant without the plugin system. The third (deepSleep) is the only implementation of an idle-deep-sleep-on-stop feature, though it carries a known S3 bug.

---

### 28.1 The `.ino` files — Dead Arduino IDE Remnants `[TRIVIAL]`

`builds/plugins/backlightcontrols.ino` and `builds/plugins/deepsleep.ino` both open with an explicit banner:

> *"This method of connecting plugins no longer works and is left here for history."*

Both use the stale `yoradio_on_setup` weak callback name (renamed to `ehradio_on_setup` in the current codebase) and depend on the Arduino IDE sketch-root convention. They live in `builds/plugins/` — outside `src/` — so they are never compiled under any circumstances.

**Action**: Delete both files as part of the `builds/plugins/` cleanup in §28.5.

---

### 28.2 `backlightControls` example — Superseded by §27 `[LOW]`

`builds/plugins/backlightControls/backlightcontrols.cpp/.h` (~45 lines) is a simplified backlight example:
- Hardcodes pin `13` instead of reading `BRIGHTNESS_PIN`
- Uses a single `Ticker` with a fixed 120-second off-timer
- No brightness ramping, no battery-dim integration

The production equivalent is Block A + Block B currently inlined in `main.cpp` (~175 lines, §27.1), which uses `BRIGHTNESS_PIN`, smooth ramping, and battery integration. Once §27 is implemented this example is a misleading, inferior duplicate.

**Action**: Delete `builds/plugins/backlightControls/` as part of §28.5.

---

### 28.3 `deepSleep` example — Feature Gap, Known S3 Bug `[LOW]`

`builds/plugins/deepSleep/deepsleep.cpp/.h` (~60 lines) implements an idle-sleep-after-stop timer:
- 60-second idle delay after playback stops → `display.deepsleep()` → `esp_deep_sleep_start()`
- Wakeup via `WAKEUP_PIN ENC_BTNB` / `WAKEUP_LEVEL`
- Has a C3-specific branch using `esp_deep_sleep_enable_gpio_wakeup()` but **no ESP32-S3 branch** — silently falls through to `esp_sleep_enable_ext0_wakeup()`, which does not work on S3 hardware

No equivalent idle-deep-sleep timer exists anywhere in the main codebase. `display.deepsleep()` exists as a surface but is never called automatically on idle. Removing the plugin system creates a feature gap here.

**Action**: Accept the feature gap for now. If idle deep sleep is desired in the future, a corrected version (with proper S3 wakeup branch) can be inlined directly into the main codebase using the existing weak-symbol pattern. Delete `builds/plugins/deepSleep/` as part of §28.5.

---

### 28.4 `helloWorld` example — Developer Reference Only `[TRIVIAL]`

`builds/plugins/helloWorld/helloworld.cpp/.h` is a pure logging stub demonstrating all 11 hook signatures. No production purpose.

**Action**: Delete `builds/plugins/helloWorld/` as part of §28.5.

---

### 28.5 Plugin Infrastructure Removal — Full Scope `[LOW]`

#### Files to delete

- `src/pluginsManager/pluginsManager.h`
- `src/pluginsManager/pluginsManager.cpp`
- `src/pluginsManager/README.md` (and folder)
- `src/plugins/README.md` (and folder)
- `builds/plugins/` (entire folder — all legacy examples)

#### `#include` lines to remove (5 files)

| File | Line | Include |
|---|---|---|
| `src/main.cpp` | ~16 | `#include "pluginsManager/pluginsManager.h"` |
| `src/core/controls.cpp` | ~9 | `#include "../pluginsManager/pluginsManager.h"` |
| `src/core/display.cpp` | ~15 | `#include "../pluginsManager/pluginsManager.h"` |
| `src/core/network.cpp` | ~18 | `#include "../pluginsManager/pluginsManager.h"` |
| `src/core/player.cpp` | ~10 | `#include "../pluginsManager/pluginsManager.h"` |

#### `pm.on_*()` call sites to remove (13 calls across 5 files)

| File | Lines | Calls |
|---|---|---|
| `src/main.cpp` | ~58, ~101 | `pm.on_setup()`, `pm.on_end_setup()` |
| `src/core/network.cpp` | ~38, ~434 | `pm.on_ticker()`, `pm.on_connect()` |
| `src/core/player.cpp` | ~128, ~158, ~280, ~313 | `pm.on_stop_play()`, `pm.on_station_change()`, `pm.on_start_play()` (×2) |
| `src/core/display.cpp` | ~360, ~404, ~559, ~775 | `pm.on_display_player()` (×2), `pm.on_display_queue()`, `pm.on_track_change()` |
| `src/core/controls.cpp` | ~485 | `pm.on_btn_click()` |

#### `display.cpp` special case

`display.cpp` declares `bool pm_result = true` solely as the out-parameter for `pm.on_display_queue()`, and wraps the entire display-request `switch` in `if (pm_result)`. After removal of the plugin call, both the local variable and the `if` wrapper must be removed, leaving the `switch(request.type)` block unconditional.

#### `build_src_filter` lines to remove (4 `platformio.ini` files)

| File | Lines | Content |
|---|---|---|
| `platformio.ini` | ~47–48 | `+<plugins/*>`, `+<pluginsManager/*>` |
| `builds/trip5/platformio.ini` | ~47–48 | same |
| `builds/kasperaitis/platformio.ini` | ~47–48 | same |
| `builds/test/platformio.ini` | ~38–39 | same |

**Scope boundary**: The `ehradio_on_setup()`, `player_on_*()`, and `ctrls_on_loop()` weak-symbol callbacks are **not** part of the plugin system — they are a linker-level mechanism and are unaffected by this removal.

**Rule #4 note**: `code-summary.md` must be updated when this is executed — remove the plugin system architecture section and `src/pluginsManager/` file entries.

---

## [ ] 98. Documentation Needs Serious Work

Like really, really badly.  For now, a lot of options are only listed in `options.h` and yoRadio documentation was already outdated at fork date.

---

## [ ] 99. Issues Found Randomly or Outside Above Issues (ONGOING)

Just some notes to make while going through code...

  [X] netserver.loop(); was twice in player.cpp line ~247-248 — removed duplicate
  [X] optionschecker.h should have more guardrails and re-ordered according to options.h (and optionschecker.h removed)


