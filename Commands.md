# Commands

Simple reference for commands handled in src/core/commandhandler.cpp.
Order and sections match that file.

## Websockets for Player

| Command(s) | Action |
| --- | --- |
| `toggle` | Toggle play/pause. |
| `prev` | Play previous station/track. |
| `next` | Play next station/track. |
| `voldown`, `volumedown`, `volm`, `vol-` | Step volume down by configured step size. |
| `volup`, `volumeup`, `volp`, `vol+` | Step volume up by configured step size. |
| `newmode` | Set config mode selection and request CHANGEMODE update. Blocked in HTTP/MQTT/Telnet. |
| `balance` | Set balance (-16..16), apply to player, and notify clients. |
| `treble` | Set treble (-16..16). |
| `middle` | Set middle (-16..16). |
| `bass` | Set bass (-16..16). |
| `volume`, `vol` | Set absolute volume (clamped 0..VOLUME_SCALE). |
| `mute` | Toggle mute: volume 0 <-> last active level. Same behaviour as the IR mute button; physical-button mute is suppressed without a display. |
| `startstandby` | Turn display off, stop playback, and preserve smartstart value. |
| `stopstandby` | Turn display on, optionally resume smartstart playback. |
| `togglestandby` | Toggle standby on or off |
| `burl`, `playurl` | Play direct stream URL (http/https). |
| `sdpos` | Set SD playback position when in SD mode. |
| `playstation`, `play` | Play station by playlist index (clamped to valid range). |
| `shuffle` | Enable/disable SD shuffle; if enabled, advance to next entry. |
| `start` | Start playback from last station. |
| `stop` | Stop playback. |
| `sleep` | Set sleep timer using for,after values (may be disabled by build) |
| `mode` | Change playback mode: `0` = Radio/Web, `1` = SD card, out-of-range values cycle modes (`2` cycles in Telnet). |
| `submitplaylist` | Stop playback before playlist submit flow. |
| `submitplaylistdone` | Finalize playlist submit flow, reload best station target, and trigger MQTT playlist sync. |

## Hidden Websockets

| Command(s) | Action |
| --- | --- |
| `getindex` | Request index payload to clients. Blocked in HTTP/MQTT/Telnet. |
| `getactive` | Request active-state payload to clients. Blocked in HTTP/MQTT/Telnet. |
| `clearfs` | Run allowlist-based LittleFS cleanup and force play mode to web. |

## Options: Load Settings

| Command(s) | Action |
| --- | --- |
| `getcontrols` | Request controls settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getscreen` | Request screen settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getlocale` | Request locale settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getweather` | Request weather settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getsystem` | Request system settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getmqtt` | Request MQTT settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getbattery` | Request battery settings payload. Blocked in HTTP/MQTT/Telnet. |

## Options: Controls

| Command(s) | Action |
| --- | --- |
| `smartstart` | Enable/disable smartstart. |
| `fliptouch` | Toggle touchscreen axis flip and apply touch config. |
| `dbgtouch` | Enable/disable touch debug mode. |
| `encacc` | Set encoder acceleration value. |
| `oneclickswitch` | Toggle one-click playlist switching behavior. |
| `irtlp` | Set IR tolerance value. |

## Options: Screen

The layout, theme and font commands all take an **index, 0-based**: the lists are built in table order,
so the first layout is `layout=0`. A value past the end of a list is clamped to its **last** entry, the
same four ways - the two font commands clamp to the number of fonts that build actually has, not to a
fixed number. `vustyle` and `screensavervustyle` take the ids `/visuals.json` publishes, and they are
deliberately **two independent settings**: the box on the player page and a meter covering the whole
panel are different pictures, so nothing keeps them in step.

| Command(s) | Action |
| --- | --- |
| `flipscreen` | Toggle display flip and redraw player view. |
| `invertdisplay` | Toggle display inversion. |
| `inverttitle` | Toggle the inverted title bar. Saved and the current theme is re-applied, because the inverted meta band is a theme change rather than a repaint. |
| `layout` | Select the active layout by index, 0-based (clamped to the layout count). Saved, applied, and the WebUI's live groups are pushed. |
| `theme` | Select the active theme by index, 0-based (clamped to the theme count). Saved and applied. |
| `sysfont` | Select the system (display) font by index, 0-based, clamped to the number of fonts this build has. Saved, applied, and the WebUI's lists are pushed. |
| `clockfont` | Select the clock font style by index, 0-based, clamped to the number of styles this build has. Saved, applied, and the WebUI's lists are pushed. |
| `numplaylist` | Toggle numbered playlist display and redraw player view. |
| `clock12` | Toggle 12-hour clock display and refresh clock. |
| `clockglow` | Enable/disable the clock's glow. Saved, then the whole display is re-initialised. |
| `volumepage` | Toggle dedicated volume page behavior and refresh player view. |
| `bufferbar` | Enable/disable the buffer bar on the display. |
| `vumeter` | Enable/disable VU meter and refresh display state. |
| `vupeaks` | Enable/disable the VU meter peak markers (or XY axis lines). |
| `vustyle` | Select the VU meter visualisation style (numeric, 0-based, clamped to the known range). |
| `brightness`, `dim` | Set brightness (0..100), ensure screen-on state, clamp dimmed brightness if needed, and apply brightness. |
| `dimmingenabled` | Enable/disable idle dimming behavior. |
| `dimmingbrightness` | Set dimmed brightness (0..100, clamped to the current brightness setting). |
| `dimmingtimeout` | Set idle dimming timeout (5..65520). |
| `screenon`, `dspon` | Turn display on/off and reset dimming state. |
| `screensaverenabled` | Enable/disable idle screensaver behavior. |
| `screensaverblank` | Enable/disable idle screensaver blanking behavior. |
| `screensavertimeout` | Set idle screensaver timeout in seconds (5..65520). |
| `screensaverplayingenabled` | Enable/disable playing screensaver behavior. |
| `screensaverplayingblank` | Enable/disable playing screensaver blanking behavior. |
| `screensaverplayingtimeout` | Set playing screensaver timeout in minutes (1..1080). |
| `screensaverfull` | Enable/disable full time on the screensaver. |
| `screensavertext` | Show/hide the screensaver's info line: station name, 2 title lines, and weather (if enabled). |
| `screensavervu` | Draw the VU full screen as the screensaver instead of the moving clock. |
| `screensavervustyle` | Select the style that full-screen meter draws (numeric, 0-based, clamped to the known range, same as `vustyle`). |
| `screensavervupeak` | Show/hide the peak markers and the axis or reference lines on that full-screen meter (independent of `vupeaks`). |

## Options: Locale

| Command(s) | Action |
| --- | --- |
| `locale_webui` | Update WebUI locale. Blocked in HTTP/MQTT/Telnet. |
| `locale_disp` | Update display locale. |
| `tz_name` | Set timezone display name. |
| `tzposix` | Set POSIX timezone string and force time sync. |
| `sntp1` | Set primary SNTP server and force time sync. |
| `sntp2` | Set secondary SNTP server. |
| `timeinterval` | Set time sync interval. |

## Options: Weather

| Command(s) | Action |
| --- | --- |
| `wenable` | Enable/disable weather display and trigger weather refresh path. |
| `wen_feelslike` | Toggle feels-like field in weather output string. |
| `wen_humidity` | Toggle humidity field in weather output string. |
| `wen_pressure` | Toggle pressure field in weather output string. |
| `wen_wind` | Toggle wind field in weather output string. |
| `wtempunit` | Toggle temperature unit mode. |
| `wpressunit` | Toggle pressure unit mode. |
| `wspeedunit` | Set wind speed unit string. |
| `wapi` | Set weather provider API selector and force weather refresh. |
| `wlang` | Set weather language and force weather refresh. |
| `wkey` | Set weather key and refresh display mode. |
| `winterval` | Set weather sync interval. |
| `wlat` | Set weather latitude, force weather refresh. |
| `wlon` | Set weather longitude, force weather refresh. |

## Options: System

| Command(s) | Action |
| --- | --- |
| `wifiscan` | Enable/disable best-RSSI WiFi scan behavior. |
| `autoupdate` | Enable/disable auto update checks. |
| `ehdp` | Enable/disable eHDP service. |
| `ehdpname` | Set eHDP name and reinitialize eHDP. |
| `mdnsname` | Set mDNS host name. |

## Options: MQTT

| Command(s) | Action |
| --- | --- |
| `mqttenable` | Enable/disable MQTT and initialize MQTT client. |
| `mqtthost` | Set MQTT host. |
| `mqttport` | Set MQTT port. |
| `mqttuser` | Set MQTT username. |
| `mqttpass` | Set MQTT password. |
| `mqtttopic` | Set MQTT topic prefix. |

## Options: Battery

| Command(s) | Action |
| --- | --- |
| `battref` | Calibrate battery reference voltage and refresh battery payload. |
| `battrecalc` | Force battery recalculation and refresh battery payload. |

## Options: Danger Zone

| Command(s) | Action |
| --- | --- |
| `reboot`, `boot` | Reboot device immediately. |
| `format` | Stop playback, format LittleFS, reboot device. |
| `reset` | Apply default settings reset flow by requested section/value. |

## IR Recorder

Only available when built with IR_PIN != 255.

| Command(s) | Action |
| --- | --- |
| `irbtn` | Select the IR button to record by name (for example `power`, `mute`, `n1`); `-1` stops recording and saves. Blocked in HTTP/MQTT/Telnet. |
| `chkid` | Set IR check slot id (0-2). Blocked in HTTP/MQTT/Telnet. |
| `irclr` | Clear the selected IR slot (0-2) of the active button. Blocked in HTTP/MQTT/Telnet. |

## Curated Playlists

| Command(s) | Action |
| --- | --- |
| `loadindex` | Start background curated index fetch task. Blocked in HTTP/MQTT/Telnet. |
| `loadplaylist` | Start background curated playlist fetch task. Blocked in HTTP/MQTT/Telnet. |
| `curated_import` | Prepare curated import file for review and notify frontend. Blocked in HTTP/MQTT/Telnet. |

## SD Manager page (not a command set, but might be useful)

A WebUI page for browsing and editing the SD card, served only while **SD Manager mode** is open, and reachable in
AP mode as well as on a station connection. There is **no bare `/sdman` route**: the route matcher treats a plain
URI as an exact path *or* as a prefix followed by `/`, so a handler registered there would also swallow `/sdman/list`
and every sibling sub-route. On an SD-offline build the web server never starts, so none of this exists.

| Entry | Action |
| --- | --- |
| The SD badge in the player header | Available while the player is in **SD mode**, where that badge is shown in place of the playlist glyph. It is handled like the search icon: a plain navigation to `/sdmanager.html`, which opens the mode. |
| `http://<radio-ip>/sdmanager.html` | The manager page itself, and the address the device shows on its own screen. **The page keeps this address for the whole session**: while the mode is open a request for `/` is redirected here, so a bookmark, a second tab, a captive-portal redirect or a Home Assistant link still lands on the manager rather than on a player whose buttons are all refused. |
| `/sdman/enter` | Open the mode, or refresh its idle clock if it is already open. Also accepted with no network. Idempotent, so a second load of the page costs nothing. |
| `/sdman/done` | Close the mode and hand `/` back to the player - or to the AP settings page when the device has no network. With **SmartStart** on, the audio that was playing on entry resumes, from the byte offset it had reached. The idle timeout resumes in the same way; a card pulled out of the slot does not, since there is nothing left to play from. |

| API route | Action |
| --- | --- |
| `GET /sdman/info` | Mounted state, used/total bytes, and the seconds left before the mode closes itself. |
| `GET /sdman/list?path=/dir` | Streamed JSON listing of one folder, in card order. |
| `GET /sdman/download?path=/dir/file` | Download that file. |
| `POST /sdman/mkdir?path=/newfolder` | Create a folder. |
| `POST /sdman/rename?from=/dir/old&to=new` | Rename within the current folder. |
| `POST /sdman/move?from=/a/b&to=/c` | Move to another folder (`rename` underneath). |
| `POST /sdman/delete` | Delete the selection; the paths arrive newline-separated in the body. The answer carries `deleted` and `failed` counts. |
| `POST /sdman/upload?path=/dir&name=track.mp3` | Upload one file, multipart, streamed straight to the card. Add `skip=1` to leave an existing name untouched: the device refuses it on the first chunk and answers `{"ok":true,"skipped":true}`. **Skip Existing is normally decided by the page before anything is sent** - it tests the folder it already listed - so the device's check only covers a name created since that listing. Without the pre-check the whole file went up the wire and was then discarded. |

Every answer is JSON with `Cache-Control: no-cache, no-store, must-revalidate`. Failures carry an `error` code the
page turns into a message - `protected`, `exists`, `no_space`, `not_found`, `no_card`, `bad_name`, `no_dir`,
`not_dir`, `not_active`, or `failed` as the fallback. `/` and everything under `/data` are read-only, and mutations
are `POST` for that reason.

## Telnet Local Commands

Handled in src/core/telnet.cpp before commandhandler dispatch.

| Command(s) | Action |
| --- | --- |
| `help` | Show a short list of basic telnet commands. |
| `quit`, `bye` | Close the active Telnet client connection immediately (no confirmation text). |

## Syntax Notes

- WebSocket command format: command=value.
- HTTP command format: GET URL query params. Use a browser URL or curl GET for command calls (not POST for this path).
- HTTP single-command examples: `http://<radio-ip>/?toggle=1` and `curl "http://<radio-ip>/?volume=80"`
- HTTP multi-param behavior: params are processed in URL order, each as a separate commandhandler dispatch.
- HTTP multi-param example: `http://<radio-ip>/?volume=70&treble=4&bass=-2`
- HTTP sleep behavior: `sleep` and `after` are merged into one sleep value before dispatch: `sleep=for,after`.
- HTTP sleep examples: `http://<radio-ip>/?sleep=30` and `http://<radio-ip>/?sleep=30&after=5`
- MQTT and Telnet command formats: `key=value`, `key value`, `key(value)`.
- MQTT examples (topic <mqtttopic>command): payload `toggle`, payload `volume=80`, payload `play 12`, payload `http://example.com/stream`
- MQTT supports raw URL payloads and maps them to `burl`.
- Telnet connect example: `telnet <radio-ip> 23`
- Telnet command examples: `toggle`, `volume 80`, `play 12`, `sleep(30,5)`
- Telnet mode examples: `mode 0` (Radio/Web), `mode 1` (SD card), `mode 2` (cycle mode).
- Telnet maps `play` with no value to `start`, and `play` with URL to `burl`.
- Boolean-like values use numeric parsing (0 false, non-zero true in most handlers).
- Numeric values are parsed with atoi-style integer conversion.
- The layout, theme and font commands take an index, 0-based; `vustyle` takes its own style id.
- HTTP, MQTT, and Telnet share source policy blocking for certain commands because they are only used by websockets with WebUI
