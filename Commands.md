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
| `volume`, `vol` | Set absolute volume (clamped 0..254). |
| `turnoff` | Turn display off, stop playback, and preserve smartstart value. |
| `turnon` | Turn display on, optionally resume smartstart playback. |
| `burl`, `playurl` | Play direct stream URL (http/https). |
| `sdpos` | Set SD playback position when in SD mode. |
| `playstation`, `play` | Play station by playlist index (clamped to valid range). |
| `shuffle` | Enable/disable SD shuffle; if enabled, advance to next entry. |
| `start` | Start playback from last station. |
| `stop` | Stop playback. |
| `sleep` | Set sleep timer using for,after values. |
| `mode` | Change playback mode: `0` = Radio/Web, `1` = SD Card, out-of-range values cycle modes (`2` cycles in Telnet). |
| `reset` | Reset runtime config state when cid is 0. |
| `submitplaylist` | Stop playback before playlist submit flow. |
| `submitplaylistdone` | Finalize playlist submit flow, reload best station target, and trigger MQTT playlist sync. |

## Hidden Websockets

| Command(s) | Action |
| --- | --- |
| `getindex` | Request index payload to clients. Blocked in HTTP/MQTT/Telnet. |
| `getactive` | Request active-state payload to clients. Blocked in HTTP/MQTT/Telnet. |
| `clearspiffs` | Run allowlist-based SPIFFS cleanup and force play mode to web. |

## Options: Load Settings

| Command(s) | Action |
| --- | --- |
| `getsystem` | Request system settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getscreen` | Request screen settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getlocale` | Request locale settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getcontrols` | Request controls settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getweather` | Request weather settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getmqtt` | Request MQTT settings payload. Blocked in HTTP/MQTT/Telnet. |
| `getbattery` | Request battery settings payload. Blocked in HTTP/MQTT/Telnet. |

## Options: System

| Command(s) | Action |
| --- | --- |
| `smartstart` | Enable/disable smartstart. |
| `audioinfo` | Enable/disable audio info UI and refresh display state. |
| `vumeter` | Enable/disable VU meter and refresh display state. |
| `wifiscan` | Enable/disable best-RSSI WiFi scan behavior. |
| `autoupdate` | Enable/disable auto update checks. |
| `ehdp` | Enable/disable eHDP service. |
| `ehdpname` | Set eHDP name and reinitialize eHDP. |
| `softap` | Set soft AP delay setting. |
| `mdnsname` | Set mDNS host name. |
| `rebootmdns` | Reboot device after short delay. |

## Options: Battery

| Command(s) | Action |
| --- | --- |
| `battref` | Calibrate battery reference voltage and refresh battery payload. |
| `battrecalc` | Force battery recalculation and refresh battery payload. |

## Options: Screen

| Command(s) | Action |
| --- | --- |
| `flipscreen` | Toggle display flip and redraw player view. |
| `invertdisplay` | Toggle display inversion. |
| `numplaylist` | Toggle numbered playlist display and redraw player view. |
| `clock12` | Toggle 12-hour clock display and refresh clock. |
| `volumepage` | Toggle dedicated volume page behavior and refresh player view. |
| `brightness`, `dim` | Set brightness (0..100), ensure screen-on state, clamp dimmed brightness if needed, and apply brightness. |
| `screenon`, `dspon` | Turn display on/off and reset dimming state. |
| `contrast` | Set contrast (0..100) and apply to display. |
| `screensaverenabled` | Enable/disable idle screensaver behavior. |
| `screensavertimeout` | Set idle screensaver timeout (5..65520). |
| `screensaverblank` | Enable/disable idle screensaver blanking behavior. |
| `screensaverplayingenabled` | Enable/disable playing screensaver behavior. |
| `screensaverplayingtimeout` | Set playing screensaver timeout (1..1080). |
| `screensaverplayingblank` | Enable/disable playing screensaver blanking behavior. |
| `dimmingenabled` | Enable/disable idle dimming behavior. |
| `dimmingtimeout` | Set idle dimming timeout (5..65520). |
| `dimmingbrightness` | Set dimmed brightness (0..100, clamped to the current brightness setting). |

## Options: Controls

| Command(s) | Action |
| --- | --- |
| `volsteps` | Set volume step size. |
| `fliptouch` | Toggle touchscreen axis flip and apply touch config. |
| `dbgtouch` | Enable/disable touch debug mode. |
| `encacc` | Set encoder acceleration value. |
| `oneclickswitching` | Toggle one-click playlist switching behavior. |
| `irtlp` | Set IR tolerance value. |

## Options: Locale

| Command(s) | Action |
| --- | --- |
| `locale_webui` | Update WebUI locale file asynchronously. Blocked in HTTP/MQTT/Telnet. |
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
| `wlat` | Set weather latitude, reset elevation, force weather refresh. |
| `wlon` | Set weather longitude, reset elevation, force weather refresh. |

## Options: MQTT

Only available when built with MQTT_ENABLE.

| Command(s) | Action |
| --- | --- |
| `mqttenable` | Enable/disable MQTT and initialize MQTT client. |
| `mqtthost` | Set MQTT host. |
| `mqttport` | Set MQTT port. |
| `mqttuser` | Set MQTT username. |
| `mqttpass` | Set MQTT password. |
| `mqtttopic` | Set MQTT topic prefix. |

## Options: Danger Zone

| Command(s) | Action |
| --- | --- |
| `reboot`, `boot` | Reboot device immediately. |
| `format` | Stop playback, format SPIFFS, reboot device. |
| `reset` | Apply default settings reset flow by requested section/value. |

## IR Recorder

Only available when built with IR_PIN != 255.

| Command(s) | Action |
| --- | --- |
| `irbtn` | Set IR recording index and update IR recording mode/state. Blocked in HTTP/MQTT/Telnet. |
| `chkid` | Set IR check slot id. Blocked in HTTP/MQTT/Telnet. |
| `irclr` | Clear selected IR slot value at active index. Blocked in HTTP/MQTT/Telnet. |

## Curated Playlists

| Command(s) | Action |
| --- | --- |
| `loadindex` | Start background curated index fetch task. Blocked in HTTP/MQTT/Telnet. |
| `loadplaylist` | Start background curated playlist fetch task. Blocked in HTTP/MQTT/Telnet. |
| `curated_import` | Prepare curated import file for review and notify frontend. Blocked in HTTP/MQTT/Telnet. |

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
- Telnet mode examples: `mode 0` (Radio/Web), `mode 1` (SD Card), `mode 2` (cycle mode).
- Telnet maps `play` with no value to `start`, and `play` with URL to `burl`.
- Boolean-like values use numeric parsing (0 false, non-zero true in most handlers).
- Numeric values are parsed with atoi-style integer conversion.
- HTTP, MQTT, and Telnet share source policy blocking for certain commands because they are only used by websockets with WebUI
