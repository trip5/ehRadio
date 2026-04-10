<img src="images/logo-color.svg" width="50%">

# ehRadio

***This documentation is the same on the [Github Page](https://trip5.github.io/ehRadio/), which may be easier to read.***

## Under Heavy Construction

This is a fork of yoRadio from version 0.9.533.  Many changes are coming... many things remain untested.

The objective of this fork is to create a more pleasant experience to use for all users, including those we builders might want to give to family members.

No support for ESP8266.  Support for low-end versions of ESP32s may get dropped (PSRAM is already a requirement).  I build with ESP32-S3s and various displays.
I can't test every piece of hardware this firmware is capable of suporting.

I may drop support for certain components like Nextion (it's already likely broken).

If you have issues it may help to also check yoRadio documentation.  The hardware implementations should still be 99% compatible.

Documentation will be improved at some point... Until then, check this page and [yoRadio](https://github.com/e2002/yoradio) documentation.


## ehRadio Version history

### 2026.04.09 - Feature Freeze ON
  - Fixes worth a release even if still in repairs:
    - aggressive re-connect on streams that disconnect (for any reason)
    - SD card scan on boot fixed
  - commandhandler re-arranged
  - options.h expanded with checks (optionchecker.h removed)
  - multiple other minor and major code changes

### 2026.03.30 - Feature Freeze ON
  - the codebase is in serious need of organization and repair
    - see `.github/code-issues.md` for more info
    - please hold off on any PRs until this is finished (it could take some time)
    - the `ehRadio` branch will not be updated until finished
    - the `dev` branch will be updated periodically
  - minor fix to how ehDP is handled in options (updates immediately)
  - redirect browser after OTA update and reboots are now handled gracefully with redirects
  - internal documentation (`options.h`) is improving... this `README.md` is still a mess.
  - 3 javascript files combined to 1 (`script2.js`) to improve OTA file updating speed

### 2026.03.22
  - [ehDP Library](https://github.com/trip5/ehDP) added
    - Device can be found using [eh Device Scanner](https://github.com/trip5/eh-Device-Scanner)
  - Grabbable and hovered items fixed in playlist editor for mobile devices
  - Some major changes to the workflows that handle Releases and firmware generation

### 2026.03.18

  - Thanks to [kasperaitis](https://github.com/kasperaitis) for [PR 50](https://github.com/trip5/ehRadio/pull/50)
    - Ignore placeholder stream titles
    - improved battery handling
  - Also: [PR 51](https://github.com/trip5/ehRadio/pull/51)
    - good work on multi-locale options for display!
    - glyph tools (more characters are always good)
    - began multi-locale support of WebUI
  - Configure the display language with something like `#define DSP_LANGUAGE_de_DE` in `myoptions.h`
    - see the available options by checking `displayL10n_*.h` files in `locale` folder 
  - WebUI locale now dynamically configurable
    - if online update capable, can switch between any that are available in the release
    - if not, then can switch between hardcoded HTML (en_US) and another (if English is not the default)
    - translations may not be good... report issues or open a PR!
    - Configure the default webUI language with `#define WEBUI_LANGUAGE_STRING "de_DE"` in `myoptions.h`
    - if not specified, will use the same as the display language
    - sets a default WebUI locale different than the display - check locale/webui folder .json files (user-configurable)
    - can still switch between 2 languages even if not using an online firmware (if it uses a locale .json)
  - Multiple Weather providers now available (APIs: Openweather 2.5 & 3.0, Open-Meteo v1)
    - old code relied on Openweather API 2.5 which will be discontinued
    - completely re-factored with possibility to add more
    - configurable in WebUI
    - preferences for units can be defined in `myoptions.h` with
      - `#define WEATHER_METRIC false` (for °F, mmHg, mph)
      - `#define WEATHER_METRIC true` (for °C, hPa, km/h) (this is default)
      - individual unit preferences can also be defined
        - `#define WEATHER_TEMPERATURE_F true` (for °F)
        - `#define WEATHER_PRESSURE_MMHG true` (for mmHg)
        - `#define WEATHER_WIND_SPEED_UNITS "mph"` (for mph)
          - `"kmh"` (for km/h)
          - `"ms"` (for m/s)
          - `"kn"` (for knots)
  - screensaver mode exits faster on button press
  - connection timeout can be altered from library default of 250ms for HTTP and 1700ms for HTTPS connections
    - in `myoptions.h` there should be two numbers like: `#define CONNECT_HTTP_HTTPS_TIMEOUT 1700, 3700`
    - probably only useful on older ESP32 boards
  - Can add `#define DISABLE_UPDATER` to `myoptions.h` to disable firmware updating capabilities
  - Time sync interval can be configured in WebUI
    - default in `myoptions.h` set with `#define TIME_SYNC_INTERVAL 1` (time in hours: 1 to 24)
  - Weather API sync interval can be configured in WebUI
    - default in `myoptions.h` set with `#define WEATHER_SYNC_INTERVAL 30` (time in minutes: 10 to 60)

### 2026.02.18
  - WebUI greatly improved for mobile and tablet devices
    - automatic checking for new version availability
    - .css files improved
  - Broken playlist editor fixed (sorry!) with major improvements
    - can now import 2 ways: replace or merge
    - importing of json and csv files are done in-browser
    - undo button can undo any changes
    - re-order by dragging stations on mobile browsers
  - Broken search fixed to work with `https` radio-browser servers (using names instead of IPs)
    - fallback server added to defines:
      - `#define RADIO_BROWSER_SERVER "all.api.radio-browser.info"` added
    - as part of this, ESPFileUpdater was updated to handle chunked transfers
    - sort options added (sort by clicks is the default instead of by name)
  - Curated Lists
    - uses Releases at [Trip5's webstations](https://github.com/trip5/webstations)
    - lists can be viewed, individual stations previewed and added (just like search)
    - lists can be directly imported to the playlist editor with replace or merge
    - defines in `myoptions.h` can be used to edit sources:
      - name that appears `#define CURATED_LISTS "Trip5's webstations"`
      - the link used with the name `#define CURATED_LISTS_LINK "https://github.com/trip5/webstations"`
      - the base url where playlists can be downloaded `#define CURATED_LISTS_URL "https://github.com/trip5/webstations/releases/latest/download/"`
      - the json file that is an index of the playlists `#define CURATED_LISTS_INDEX "index.json"`
      - or disabled with `#define CURATED_LISTS false`
        - actually any define here will work as long as all others are not defined.
  - A default playlist can be downloaded on first boot if there is no playlist detected
    - must be added in `myoptions.h` - there is no default
    - `#define PLAYLIST_DEFAULT_URL "https://github.com/trip5/webstations/releases/latest/download/trip5-radio-playlist.csv"`
  - Smart start now always resumes last-played station (even if not playing when powered-off)
  - Send clicks to Radio Browser API
    - Delay before sending the click `#define RADIO_BROWSER_SEND_CLICK_DELAY 5000`
    - opt out with `#define RADIO_BROWSER_NO_SEND_CLICKS` in `myoptions.h`
      - semi-functional [Search by url doesn't find resolved_url](https://gitlab.com/radiobrowser/radiobrowser-api-rust/-/issues?sort=created_date&state=opened&search=url&first_page_size=20&show=eyJpaWQiOiIyNDkiLCJmdWxsX3BhdGgiOiJyYWRpb2Jyb3dzZXIvcmFkaW9icm93c2VyLWFwaS1ydXN0IiwiaWQiOjE4NDA4NzM5MH0%3D)
  - Settings: Tools changed to Danger Zone - with some added warnings
  - SPIFFS clean-up added (after update, unwanted files are purged)
    - added because online flasher does not erase SPIFFS
  - Minor improvements to code
    - vars set to default value in `.h` file instead of in `.cpp`
    - fixed `.h` framework headers to use `< >` instead of `" "`
    - pretty code - most `#if` & `#ifdef` blocks now indented
    - most functions now included no-op instead of being blocked by `#ifdef`
  - Optimized declarations in src files
  - More cleanup to various files and folders
  - `printFix` separated from `utf8Rus`
    - choose one text pre-processor (others may be added later)
    - use these if your display prints garbage instead of text
    - functions added for Nextion (other displays already had both options available)
    - put one of these in your `myoptions.h`
      - `#define PRINT_FIX` tries to reduce Unicode characters to ASCII (mostly successfully)
      - `#define UTF8_RUS` will convert Unicode Russian characters to ASCII
  - Added check for new versions (and will show if available in WebUI)
  - Visual feedback on display for firmware udpating and getting files
  - Platformio.ini now includes pre- and post-steps to compress www files for SPIFFS
  - more defaults added to `options.h` which can be changed in `myoptions.h`
    - `#define CHECKUPDATEURL_TIME "1 day"`
    - `#define TIMEZONES_JSON_CHECKTIME "4 weeks"`
    - `#define RB_SERVERS_CHECKTIME "1 day"`
    - `#define TIME_SYNC_INTERVAL 3600` (if RTC then `86400`)
    - `#define WEATHER_SYNC_INTERVAL 1800` (maybe this should be a configurable setting?)
  - Thanks to [kasperaitis](https://github.com/kasperaitis) for [PR 42](https://github.com/trip5/ehRadio/pull/42)
    - Auto Update on Boot option added
      - will automatically download firmware updates
      - a second reboot will begin the file downloader
    - Battery monitoring added
      - Requires these in `myoptions.h`
        - `#define BATTERY_PIN`
        - `#define BATTERY_CHARGE_PIN`
        - `#define BATTERY_DIVIDER_RATIO`
        - `#define BATTERY_ADC_REF_MV`
        - `#define BATTERY_UPDATE_INTERVAL`
        - `#define BATTERY_SAMPLES`
      - Check `options.h` for detailed notes
    - Telnet formatting looks great!

### 2026.02.06
  - Online Flasher introduced
  - Workflows make forking and building your own firmware easier
  - Improv mode added to firmware so if Wi-fi doesn't connect, use a WebUI to send Wi-fi information

### 2026.02.04
  - Thanks to [kasperaitis](https://github.com/kasperaitis) for [PR 37](https://github.com/trip5/ehRadio/pull/37)
    - adds support for ES8311 + FM8002E I2C decoder and FT6336 touchscreen on the [ES3C28](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display) ([Aliexpress](https://www.aliexpress.com/item/1005010338765126.html))
    - localization fix: weather now uses `LANG::weatherFmt` and `LANG::wind` instead of hardcoded strings
    - RGB LED (WS2812) can now do visual feedback for player state:
      - 🟢 green = playing
      - 🔴 red = stopped
      - 🔵 blue flash = track change
  - added option to scan for and connect to the strongest available RSSI from saved networks
    - will attempt to connect by BSSID (the AP's MAC address) in order of best signal to worst signal
    - useful for environments with multiple APs (ie. mesh networks)
    - adds a non-insignificant time to initial boot
    - add `#define WIFI_SCAN_BEST_RSSI true` in `myoptions.h` to set the default to true (otherwise false)
    - can be changed in the WebUI
  - added System Overrides that may be used in `myoptions.h`
    - `#define LOOP_TASK_STACK_SIZE 16` sets the stack size for the FreeRTOS task that runs the main loop
      - 16KB is OK for ESP32-S3 but maybe 8KB for ESP32?
      - 8KB is safe when using a VS1053 decoder
    - `#define CONFIG_ASYNC_TCP_QUEUE_SIZE 64` - maybe 32 for ESP32?
  - Removed outdated local libraries to use PlatformIO libraries
    - most were fairly easy to incorporate but some minor changes were made to code when needed
    - AsyncTCP library https://github.com/ESP32Async/AsyncTCP
      - former source: https://github.com/me-no-dev/AsyncTCP/
    - ESPAsyncWebServer https://github.com/ESP32Async/ESPAsyncWebServer
      - former source: https://github.com/me-no-dev/ESPAsyncWebServer
    - OneButton https://github.com/mathertel/OneButton
      - same dev, minor version bump
    - IRRemoteESP8266 https://github.com/crankyoldgit/IRremoteESP8266
      - untested but dev same, version bump minor, and source code 99.99% similar
    - AsyncMqttClient https://github.com/marvinroger/async-mqtt-client
      - this one has some 'deprecated' warnings but still functional
    - ESPFileUpdater https://github.com/trip5/ESPFileUpdater
      - mine, created for ehRadio, just involved releasing it to PlatformIO
    - Adafruit GC9A01A https://github.com/adafruit/Adafruit_GC9A01A
      - previous internal one was identical to 1.1.0
    - yoEncoder https://github.com/igorantolic/ai-esp32-rotary-encoder
      - Ai Esp32 Rotary Encoder appears to be the original (and works well)
    - GT911 Touchscreen https://github.com/tamctec/gt911-arduino
      - same dev, minor version bump
  - These could not be replaced so were moved to `libraries` folder
    - Adafruit GC9106 https://github.com/prenticedavid/Adafruit_GC9102_kbv
      - not on Platformio (and not actually an Adafruit library)
    - Adafruit ST7796S https://github.com/prenticedavid/Adafruit_ST7796S_kbv
      - not on Platformio (and not actually an Adafruit library)
    - FT6336_Touchscreen
      - made by https://github.com/kasperaitis for ehRadio
    - ILI9225Fix https://github.com/arduinopavlodar/TFT_22_ILI9225
      - not on Platformio and also highly-modified from an unknown version
    - ILI9488 https://github.com/ZinggJM/ILI9486_SPI
      - highly-modified from version 1.0.5?
    - LiquidCrystalI2C https://github.com/johnrickman/LiquidCrystal_I2C
      - slightly-modified from version 1.1.3
    - SSD1322 https://github.com/JamesHagerman/Jamis_SSD1322
      - slightly-modified from initial commit
    - ST7920 https://github.com/BornaBiro/ST7920_GFX_Library
      - very similar or modified (or perhaps share a common source)
      - may be worth looking at as well: https://github.com/BornaBiro/ST7920_GFX_Library
  - Audio decoder drivers also moved to `libraries` folder (with some renaming)
    - ES8311_Audio
      - made by https://github.com/kasperaitis for ehRadio
    - I2S_Audio https://github.com/schreibfaul1/ESP32-audioI2S
      - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
      - Maleksm says source from Wolle (schreibfaul1) 3.3.2l on 2025.07.09
    - VS1053_Audio https://github.com/schreibfaul1/ESP32-vs1053_ext
      - possibly from https://github.com/nstepanets/ESP32-vs1053_ext
      - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
      - Maleksm says source from Wolle (schreibfaul1) 3.0.13t on 2024.11.16
  - these notes and more added to `libraries` folder for future upgrading

### 2025.08.31
  - Display fixes and other fixes from yoRadio up to v0.9.693 which should include:
    - fixed incorrect behavior of the `HIDE_VU` setting
    - fixed `CORRUPT HEAP` error when playing "invalid links"
    - optimized code of utfToAscii
    - fixed artifacts in scrolling text
    - fixed SD card connection bug in configurations with `SD_SPIPINS` defined
    - time synchronization setting (maybe?)
    - fixed bug with redundant epoch time being added to the tag in SD mode (maybe?)
    - fixed clock display bug when exiting screensaver in playback mode
    - increased number of SD card initialization attempts when switching mode
    - fixed bug with incorrect text clipping on scrolling widgets
    - display performance optimization
    - to improve rendering smoothness, a framebuffer has been added for TFT SPI displays ST7735, ST7789, ILI9341, GC9106, ST7796, GC9A01A, ILI9488, ILI9486
      - the framebuffer is applied only to moving elements (scrolling text, VU meter, clock)
      - the framebuffer works on modules with additional PSRAM
      - on such modules, the framebuffer is enabled automatically, no extra steps required
      - to disable the framebuffer, add #define USE_FBUFFER false in myoptions.h
      - on modules without PSRAM, the framebuffer is disabled by default. It can be forced on by adding `#define SFBUFFER` in `myoptions.h`
      - but in that case, free memory (as well as HTTPS streams) will be severely limited
    - fixed compilation error for Nextion displays
    - code cleanup, optimization, and refactoring (actually with 2 different codebases, it may have been made more spaghettified)
    - fixed compilation error for certain displays when `#define DSP_INVERT_TITLE false` is set
    - fixed compilation error for `DSP_DUMMY`

### 2025.08.20
  - Major fixes to online updater (if running an older version, need to manually flash)
  - WebUI fixed for mobile displays
  - MQTT added to WebUI options / defaults set by adding defines to `myoptions.h`
    - `#define MQTT_ENABLE` to enable (will not be available otherwise)
    - defaults can be set with (Still editable in WebUI):
      - `#define MQTT_HOST "192.168.1.2"`
      - `#define MQTT_PORT 1883`
      - `#define MQTT_USER "mqttuser"`
      - `#define MQTT_PASS ""`
      - `#define MQTT_TOPIC "ehradio/myradio/"`
  - Home Assistant integration fixed
  - Display On toggle removed from WebUI

### 2025.08.12

  - new folder `builds/trip5` that contain my `platformio.ini`, `myoptions.h`, `mytheme.h`
    - these files are used by the workflow that generates the automatic builds
    - using VSCode extension, can be automatically synced from root to builds/username (see notes in build folder)
    - for forks, just make a sub-folder in `builds` using their github name
      - must contain `platformio.ini` at a minimum or the workflow will abort
    - automatic builds into Releases are triggered by tags
      - `git tag 2025.08.12`
      - `git push origin 2025.08.12`
    - this way, we can share configuration and platformio.ini working codes
  - `.gitignore` file now included by default that ignores certain files
    - you can still use `platformio.ini`, `myoptions.h`, `mytheme.h` in your root locally
      - but put them in `builds` if you want the automatic builds to work
  - fixes to hotspot AP mode
    - default AP is `ehRadio` with no password
    - can use `#define AP_SSID ssidname` and `#define AP_PASSWORD password` in `myoptions.h`
    - a bit faster reponse time
  - 12-hour mode fixed on color screens (space is now the same width as numbers)
  - some more options can have their defaults set by adding defines to `myoptions.h`
    - `#define SOUND_VOLUME 12` sets the default of the sound volume (0 to 254, otherwise 12)
    - `#define SOUND_BALANCE true` sets the default of the sound balance (-16 to 16, otherwise 0)
    - `#define EQ_TREBLE 0` sets the default of the EQ treble (-16 to 16, otherwise 0)
    - `#define EQ_MIDDLE 0` sets the default of the EQ middle  (-16 to 16, otherwise 0)
    - `#define EQ_BASS 0` sets the default of EQ bass (-16 to 16, otherwise 0)
    - `#define SD_SHUFFLE true` sets the default of SD shuffle (otherwise false)
    - `#define SMART_START true` sets the default of smart start (otherwise false)
    - `#define SHOW_AUDIO_INFO true` sets the default of audio info (otherwise false)
    - `#define SHOW_VU_METER true` sets the default of the VU meter (otherwise false)
    - `#define SOFTAP_REBOOT_DELAY 0` sets the default of the soft AP reboot delay (0 to 20 minutes, otherwise 0)
    - `#define SCREEN_FLIP true` sets the default of flip screen (otherwise false)
    - `#define SCREEN_INVERT true` sets the default of invert display (otherwise false)
    - `#define NUMBERED_PLAYLIST true` sets the default of numbered playlist (otherwise false)
    - `#define CLOCK_TWELVE true` sets the default of the 12-hour clock (otherwise false)
    - `#define VOLUME_PAGE true` sets the default of the volume page (otherwise false)
    - `#define SCREEN_BRIGHTNESS true` sets the default of the screen brightness (otherwise 100)
    - `#define SCREEN_CONTRAST true` sets the default of screen brightness (otherwise 55)
      - not yet editable in the UI but it should be soon.
    - screensaver options (while not playing):
      - `#define SS_NOTPLAYING true` to set the default state (otherwise false)
      - `#define SS_NOTPLAYING_BLANK true` sets the default of blank (otherwise false)
      - `#define SS_NOTPLAYING_TIME 20` sets the default of the timeout (5 to 65520 seconds, otherwise 20)
    - screensaver options (while playing):
      - `#define SS_PLAYING true` to set the default state (otherwise false)
      - `#define SS_PLAYING_BLANK true` sets the default of blank (otherwise false)
      - `#define SS_PLAYING_TIME 5` sets the default of the timeout (1 to 1080 minutes, otherwise 5)
    - `#define VOLUME_STEPS 1` sets the default of volume steps (1 to 10, otherwise 1)
    - `#define TOUCH_FLIP true` sets the default of touchscreen debug (otherwise false)
    - `#define TOUCH_DEBUG true` sets the default of touchscreen debug (otherwise false)
    - `#define ROTARY_ACCEL 200` sets the default of the rotary encoder acceleration (1 to 700, otherwise 200)
    - `#define ONE_CLICK_SWITCH true` sets the default of one-click station switching (otherwise false)
    - `#define IR_TOLERANCE 35` sets the default of IR tolerance (10 to 80, otherwise 35)
    - during this process I also deleted many store keys that were leftover from much earlier versions of yoRadio
  - WebUI colors tweaked (more changes later)
  - Displays ST7796 (480x320) and ILI9488 (480x320) can have a bigger boot logo
    - add `#define BIG_BOOT_LOGO` to `myoptions.h`
      - this takes up a not-small amount of flash memory so not recommended for plain ESP32
  - improvements from yoRadio v0.9.574 (latest as of this date)
    - ST7789_76 2.25" display
    - MQTT & HA improvements
    - AsyncTCP & WebSockets
    - vortigont's PRs (thanks!)
      - display task - optimize taks delay and message handling
      - page class migrated from LinkedList to std::list
    - other bug fixes and optimizations
    - some fixes ignored due to ongoing restructuring of yoRadio code
    - probably the last time to update core functionality from yoRadio
      - display code remains the same so those can be updated easily

### 2025.08.10

- Forked from yoRadio 0.9.533
  - was geting tired of PRs not being accepted and as you can see below (many were made previous to 2025.07.20)
- source folder structure moved up (makes more sense especially with VSCode and Platformio)
- Re-branding & Styling
  - logos added to boot screens and WebUI
  - color changes to display and WebUI (not 100% satisfied but good enough for now)
- replacement font with icons added into builds using `platformio.ini`
  - `#define YO_FIX` removed (maybe never needed?)
- data files will no longer be compressed in the repository
  - but they will be available in `Releases` as .gz files for automatic updating of SPIFFS
    - which means if you really wish to compress them and upload them to SPIFFs, you can
      - except `rb_srvrs.json` which is obtained automatically anyways
- a Github workflow is used to automatically update `timezones.json.gz` and `rb_srvrs.json`
  - added to the repo automatically whenever a commit is made

### 0.9.533 Trip5/2025.07.23

- minor additions to UI / display options
  - one-click option ignores long-press right/left
  - 12-hour clock
  - hide volume page (instead of using a `#define`)

### 0.9.533 Trip5/2025.07.20

- BREAKING CHANGES
  - requires larger app partitions
    - if using 4MB flash sizeESP32, use the `ESP32-4MB.csv` as your partition file (see the text file for more)
  - many settings will be reset to defaults
  - notes were added to `config.cpp` how to handle breaking and non-breaking store updates in the future
  - future re-flashes should not lose settings after this update
  - capability to update firmware and download SPIFFS files from online has been added
- library dependency: `bblanchon/ArduinoJson@^6.21.3`
  - https://github.com/bblanchon/ArduinoJson
- EEProm storage removed in favor of Preferences
  - config.store variables may still be used as before
  - no need to handle store version changes except by adding old ones to the "remove" list in `config.cpp`
    - char handling improved throughout (size may be changed later with 1 edit to `config.h`)
      - affects timezone, mdnsname, weather coordinate variables
- shorter searching for Wi-fi message
- LED_INVERT fixed (`#define LED_INVERT`)
- fixes for screens that can't display certain characters (add to `myoptions.h`)
  - `#define YO_FIX` // changes ёRadio to yoRadio for screens that can't print ё
  - `#define PRINT_FIX` // fix Chinese certain screens so they don't display gibberish
    - hijacks `utf8RusGFX.h` (which is meant for Russian model displays?)
      - does not interrupt this function unless `PRINT_FIX` defined
    - will transliterate as many European characters as possible by dropping accents
    - will transliterate Cyrillic... probably badly
    - will transliterate various punctuation and currency symbols
    - any characters that can't be handled will be replaced by a space
- ESPFileUpdater can update and download files (used in multiple places)
  - a new library created for this project
  - this may be used to download / update any file from online to SPIFFS
  - add `#define ESPFILEUPDATER_DEBUG` to `myoptions.h` to get verbose output
- Online updating for pre-built BIN files and other assets
  - Uses ESPFileUpdater when SPIFFS is empty or incomplete if the running program is a pre-built firmware
  - the online URL path used to download SPIFFS file assets (for the running version):
    - `#define FILESURL "https://github.com/trip5/yoradio/releases/download/2025.07.19/"`
  - the online URL path used to OTA update firmware:
    - `#define UPDATEURL "https://github.com/trip5/yoradio/releases/latest/download/"`
  - the online file that the current version can compare it's version against:
    - `#define CHECKUPDATEURL "https://raw.githubusercontent.com/trip5/yoradio/refs/heads/trip5/yoRadio/src/core/options.h"`
  - the above file must contain a line that is defined by this setting:
    - `#define VERSIONSTRING "#define RADIOVERSION"` (followed by a version string)
  - which `.bin` file to be used for online OTA updates can be specified as:
    - `#define FIRMWARE "firmware_sh1106_pcm_remote.bin"`
  - all of these can be automatically defined by `platformio.ini` amd `myoptions.h`
    - an example of this automatic building is at `https://github.com/trip5/yoradio/tree/trip5/`
    - check out `platformio-trip5-builds.yml` to see how firmwares are built and files made available online
      - First commit your changes to Github
      - Tag your local most recent commit and push it to Github:
        - `git tag 2025.07.20`
        - `git push origin 2025.07.20`
      - Re-doing a tagged release:
        - `git tag -d 2025.07.20`
        - `git tag -a 2025.07.20 -m "2025.07.20"`
        - `git push origin 2025.07.20 --force`
    - this means you can just download a .bin file and flash from a command line (see the Release page for detailed instructions)
- implements proper timezones
  - uses ESPFileUpdater to download an up-to-date json to be used as a selector in the WebUI
  - fetches from https://raw.githubusercontent.com/trip5/timezones.json/refs/heads/master/timezones.gz.json
    - this can be a gzipped or regular json
    - uses github workflow to automatically update whenever tzdb has changed
    - also created for this project
    - another json.gz can be used by a define in `myoptions.h` (default in quotes)
      - `#define TIMEZONES_JSON_URL "https://raw.githubusercontent.com/trip5/timezones.json/master/timezones.json.gz"`
  - will handle Daylight Savings Times
    - does not affect timed functions that use ticks (screensaver, weather)
    - may have side-effects on other timed functions that don't use ticks
  - timezone offset completely removed
  - timezones may be changed through WebUI or telnet
  - telnet has some extra functions added
    - `TZO ±X` now just changes to a GMT-type (non-standard is OK)
    - `TZO ±X:XX` now just changes to a custom timezone (based on GMT-type)
    - `TZPOSIX` command can create a custom timezone (be careful)
    - `PLAY url` can play a station not on the playlist
  - Nextion displays only show timezone now, cannot change timezone
    - there may be a good way to implement with + & - with GMT timezones (see `telnet.cpp` for some idea)
    - I have no Nextion display to test (sorry!)
- regional defaults now be defined in `myoptions.h` (defaults in quotes):
  - `#define TIMEZONE_NAME "Europe/Moscow"`
  - `#define TIMEZONE_POSIX "MSK-3"`
  - `#define SNTP_1 "pool.ntp.org"`
  - `#define SNTP_2 "0.ru.pool.ntp.org"`
  - `#define WEATHER_LAT "55.7512"`
  - `#define WEATHER_LON "37.6184"`
  - all can still be edited using WebUI
- Radio station search via Radio Browser API
  - performs search queries to a https://www.radio-browser.info/ server
  - uses ESPFileUpdater to download an up-to-date json of API servers
    - another json can be used by a define in options.h
      - `#define RADIO_BROWSER_SERVERS_URL "https://all.api.radio-browser.info/json/servers"` in `myoptions.h`
  - handles down API servers gracefully (and they do go down fairly often)
  - uses ESPFileUpdater to download JSON search results directly from the API to the ESP's file system
    - previous searches are saved and not lost on reboot (100 results, page number)
    - saved searches will be deleted on reboot if they are older than 24 hours
  - search results shows station name, country code, codec, bitrate
  - stations can be previewed (through the radio) with a play button that does not add it to the playlist
    - if matches URL in the playlist will play the station from the playlist
  - stations can be added with a plus button
    - will not add a station which has the same URL as one already in the playlist (http & https considered same)
- Playback queue (either from playlist, search, or telnet) put into an RTOS task that runs in the background
  - stability improved but slight delay added
- Improved JSON and CSV file importing
  - CSV import made more resilient
    - can import any type of list with fields separated by tabs or spaces
    - missing name or ovol fields will continue to be imported
    - for example, all of these are valid:
      - space-separated values:
        - `nap.casthost.net:8793/stream Intra Nature Radio`
        - `Ambient Sleeping Pill http://radio.stereoscenic.com/asp-s`
        - `Super Relax FM https://streams.radio.menu/listen/super-relax-fm/radio.mp3 0`
      - tab-separated values:
        - `Traxx FM - Ambient	http://traxx011.ice.infomaniak.ch/traxx011-low.mp3	0`
        - `Positively Meditation	0	https://streaming.positivity.radio/pr/posimeditation/icecast.audio`
        - `0	Cryosleep	http://streams.echoesofbluemars.org/8000/cryosleep`
        - `https://ice4.somafm.com/darkzone-128-mp3	Soma FM - Dark Zone 0`
      - just the URL
        - `https://ice4.somafm.com/dronezone-128-mp3`
    - the old CSV parser is still used for the yoRadio playlist internally
  - JSON import made more resilient
    - can handle line-by-line files that are not enclosed in [ ]
      - ie. each line looks like
        - `{"name":"Swinging radio 60s","host":"http://s2.xrad.io","file":"/8058/stream","port":"0","ovol":"0"}`
    - can handle proper json files where all entries are enclosed in [ ]
      - the field "url_resolved" will be preferred
      - fallback to "url"
      - finally, fallback to "host" and "file" and "port" - which will be combined into a url
  - in both parsers, only the url is mandatory
    - if "name" is absent, it will be assumed to be the url even without http(s)://
    - if "ovol" is absent, is will be assumed to be 0
- Added some of Maleksm's additions and changes from v0.9.434m (04.04.25) (with some changes)
  - includes improved decoding for VS1053 and PCM decoder and various codecs
  - ESP8266 support completely removed
  - includes BacklightDown plugin (tweaked to be non-blocking)
    - dims the display after awhile
    - `#define BRIGHTNESS_PIN` must be in your `myoptions.h`
    - `#define DOWN_LEVEL 2` (brightness level 0 to 255, default 2 )
    - `#define DOWN_INTERVAL 60` (seconds to dim, default 60 = 60 seconds)
  - use `#define HIDE_VOLPAGE` to hide the separate page showing volume (uses the progress bar instead)




---

## Original yoRadio Readme below (for now)

---

# ёRadio
<img src="images/yologo.png" width="190" height="142">

##### Web-radio based on [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) or/and [ESP32-vs1053_ext](https://github.com/schreibfaul1/ESP32-vs1053_ext) library
---
- [Hardware](#hardware)
- [Connection tables](#connection-tables)
- [Software dependencies](#dependencies)
- [Hardware setup](#hardware-setup)
- [Quick start](#quick-start)
- [Detailed start](https://github.com/e2002/yoradio/wiki/How-to-flash)
- [Update](#update)
- [Update over web-interface](#update-over-web-interface)
- [Controls](Controls.md)
- [MQTT](#mqtt)
- [Home Assistant](#home-assistant)
- [More features](#more-features)
- [Plugins](#plugins)
- [Version history](#version-history)
- [Описание на 4PDA](https://4pda.to/forum/index.php?s=&showtopic=1010378&view=findpost&p=112992611)
---
#### NEW!
##### yoRadio Printed Circuit Boards repository:
[<img src="images/yopcb.jpg" width="830" height="auto" />](https://github.com/e2002/yopcb)

https://github.com/e2002/yopcb

---
<img src="images/img0_2.jpg" width="830" height="467">

##### More images in [Images.md](Images.md)

---
## Hardware
#### Required:
**ESP32 board**: https://aliexpress.com/item/32847027609.html \
**I2S DAC**, roughly like this one: https://aliexpress.com/item/1005001993192815.html \
https://aliexpress.com/item/1005002011542576.html \
or **VS1053b module** : https://aliexpress.com/item/32893187079.html \
https://aliexpress.com/item/32838958284.html \
https://aliexpress.com/item/32965676064.html

#### Optional:
##### Displays
- **ST7735** 1.8' or 1.44' https://aliexpress.com/item/1005002822797745.html
- or **SSD1306** 0.96' 128x64 I2C https://aliexpress.com/item/1005001621806398.html
- or **SSD1306** 0,91' 128x32 I2C https://aliexpress.com/item/32798439084.html
- or **Nokia5110** 84x48 SPI https://aliexpress.com/item/1005001621837569.html
- or **ST7789** 2.4' 320x240 SPI https://aliexpress.com/item/32960241206.html
- or **ST7789** 1.3' 240x240 SPI https://aliexpress.com/item/32996979276.html
- or **SH1106** 1.3' 128x64 I2C https://aliexpress.com/item/32683094040.html
- or **LCD1602** 16x2 I2C https://aliexpress.com/item/32305776560.html
- or **LCD1602** 16x2 without I2C https://aliexpress.com/item/32305776560.html
- or **SSD1327** 1.5' 128x128 I2C https://aliexpress.com/item/1005001414175498.html
- or **ILI9341** 3.2' 320x240 SPI https://aliexpress.com/item/33048191074.html
- or **ILI9341** 2.8' 320x240 SPI https://aliexpress.com/item/1005004502250619.html
- or **SSD1305 (SSD1309)** 2.4' 128x64 SPI/I2C https://aliexpress.com/item/32950307344.html
- or **SH1107** 0.96' 128x64 I2C https://aliexpress.com/item/4000551696674.html
- or **GC9106** 0.96' 160x80 SPI (looks like ST7735S, but it's not him) https://aliexpress.com/item/32947890530.html
- or **LCD2004** 20x4 I2C https://aliexpress.com/item/32783128355.html
- or **LCD2004** 20x4 without I2C https://aliexpress.com/item/32783128355.html
- or **ILI9225** 2.0' 220x176 SPI https://aliexpress.com/item/32952021835.html
- or **Nextion displays** - [more info](https://github.com/e2002/yoradio/tree/main/nextion)
- or **ST7796** 3.5' 480x320 SPI https://aliexpress.com/item/1005004632953455.html?sku_id=12000029911293172
- or **GC9A01A** 1.28' 240x240 https://aliexpress.com/item/1005004069703494.html?sku_id=12000029869654615
- or **ILI9488** 3.5' 480x320 SPI https://aliexpress.com/item/1005001999296476.html?sku_id=12000018365356570
- or **ILI9486** (Testing mode) 3.5' 480x320 SPI https://aliexpress.com/item/1005001999296476.html?sku_id=12000018365356568
- or **SSD1322** 2.8' 256x64 SPI https://aliexpress.com/item/1005003480981568.html
- or **ST7920** 2.6' 128x64 SPI https://aliexpress.com/item/32699482638.html
- or **ST7789** 2.25' 284x76 SPI https://aliexpress.ru/item/1005009016973081.html

(see [Wiki](https://github.com/e2002/yoradio/wiki/Available-display-models) for more details)

##### Controls
- Three tact buttons https://www.aliexpress.com/item/32907144687.html
- Encoder https://www.aliexpress.com/item/32873198060.html
- Joystick https://aliexpress.com/item/4000681560472.html \
https://aliexpress.com/item/4000699838567.html
- IR Control https://www.aliexpress.com/item/32562721229.html \
https://www.aliexpress.com/item/33009687492.html
- Touchscreen https://aliexpress.com/item/33048191074.html

##### RTC
- DS1307 or DS3231 https://aliexpress.com/item/4001130860369.html

---
## Connection tables
##### Use [this tool](https://e2002.github.io/docs/myoptions-generator.html) to build your own connection table and myoptions.h file.
<img src="images/myoptions-generator.png" width="830" height="527"><br />

https://e2002.github.io/docs/myoptions-generator.html

---
## Dependencies
#### Libraries:
**Library Manager**: Adafruit_GFX, Adafruit_ST7735\*, Adafruit_SSD1306\*, Adafruit_PCD8544\*, Adafruit_SH110X\*, Adafruit_SSD1327\*, Adafruit_ILI9341\*, Adafruit_SSD1305\*, TFT_22_ILI9225\* (\* depending on display model), OneButton, IRremoteESP8266, XPT2046_Touchscreen, RTCLib \
**Github**: ~~[ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer), [AsyncTCP](https://github.com/me-no-dev/AsyncTCP), [async-mqtt-client](https://github.com/marvinroger/async-mqtt-client) (if you need MQTT support)~~ <<< **starting with version 0.8.920, these libraries have been moved into the project, and there is no need to install them additionally.**

#### Tool:
[ESP32 Filesystem Uploader](https://randomnerdtutorials.com/install-esp32-filesystem-uploader-arduino-ide/)

**See [wiki](https://github.com/e2002/yoradio/wiki/How-to-flash#preparing) for details**

---
## Hardware setup
Don't edit the options.h! \
Hardware is adjustment in the **[myoptions.h](examples/myoptions.h)** file.

**Important!**
You must choose between I2S DAC and VS1053 by disabling the second module in the settings:
````c++
// If I2S DAC used:
#define I2S_DOUT      27
#define VS1053_CS     255
// If VS1053 used:
#define I2S_DOUT      255
#define VS1053_CS     27
````
Define display model:
````c++
#define DSP_MODEL  DSP_ST7735 /*  default - DSP_DUMMY  */
````
The ST7735 display submodel:
````c++
#define DTYPE INITR_BLACKTAB // 1.8' https://aliexpress.ru/item/1005002822797745.html
//#define DTYPE INITR_144GREENTAB // 1.44' https://aliexpress.ru/item/1005002822797745.html
````
Rotation of the display:
````c++
#define TFT_ROTATE 3 // 270 degrees
````
##### Note: If INITR_BLACKTAB dsp have a noisy line on one side of the screen, then in Adafruit_ST7735.cpp:
````c++
  // Black tab, change MADCTL color filter
  if ((options == INITR_BLACKTAB) || (options == INITR_MINI160x80)) {
    uint8_t data = 0xC0;
    sendCommand(ST77XX_MADCTL, &data, 1);
    _colstart = 2; // ← add this line
    _rowstart = 1; // ← add this line
  }
````

---
## Quick start
<img src="images/board4.jpg" width="830" height="473"><br />

- <span style="color: red; font-weight: bold; font-size: 22px;text-decoration: underline;">Arduino IDE version 2.x.x is not supported. Use Arduino IDE 1.8.19</span>
- <span style="color: red; font-weight: bold; font-size: 22px;text-decoration: underline;">ESP32 core version 2.0.0 or higher is [required](https://github.com/espressif/arduino-esp32)!</span>

1. Generate a myoptions.h file for your hardware configuration using [this tool](https://e2002.github.io/docs/myoptions-generator.html).
2. Put myoptions.h file next to ehRadio.ino.
3. Replace file Arduino/libraries/Adafruit_GFX_Library/glcdfont.c with file [yoRadio/fonts/glcdfont.c](yoRadio/fonts/glcdfont.c)
4. Restart Arduino IDE.
5. In ArduinoIDE - upload sketch data via Tools→ESP32 Sketch Data Upload ([it's here](images/board2.jpg))
6. Upload the sketch to the board
7. Connect to yoRadioAP access point with password 12345987, go to http://192.168.4.1/ configure and wifi connections.  \
_\*this step can be skipped if you add WiFiSSID WiFiPassword pairs to the [yoRadio/data/data/wifi.csv](yoRadio/data/data/wifi.csv) file (tab-separated values, one line per access point) before uploading the sketch data in step 1_
8. After successful connection go to http://\<yoipaddress\>/ , add stations to playlist (or import WebStations.txt from KaRadio)
9. Well done!

**See [wiki](https://github.com/e2002/yoradio/wiki/How-to-flash#build--flash) for details**

---
## Update
1. Backup your settings: \
download _http://\<yoradioip\>/data/playlist.csv_ and _http://\<yoradioip\>/data/wifi.csv_ and place them in the yoRadio/data/data/ folder
2. In ArduinoIDE - upload sketch data via Tools→ESP32 Sketch Data Upload
3. Upload the sketch to the board
4. Go to page _http://\<yoradioip\>/_ in the browser and press Ctrl+F5 to update the scripts.
5. Well done!

## Update over web-interface
1. Backup your settings: \
download _http://\<yoradioip\>/data/playlist.csv_ and _http://\<yoradioip\>/data/wifi.csv_ and place them in the yoRadio/data/data/ folder
2. Get firmware binary: Sketch → Export compiled binary
3. Get SPIFFS binary: disconnect ESP32 from your computer, click on **ESP32 Data Sketch Upload**. \
 You will get an error and file path

 <img src="images/getspiffs.jpg" width="830" height="208">

4. Go to page _http://\<yoradioip\>/update_ and upload ehRadio.ino.esp32.bin and yoRadio.spiffs.bin in turn, checking the appropriate upload options.
5. Well done!

---
## MQTT
1. Copy file examples/mqttoptions.h to yoRadio/ directory
2. In the mqttoptions.h file, change the options to the ones you need
3. Well done!

---
## Home Assistant
<img src="images/ha.jpg" width="500" height="270"><br />

0. Requires [MQTT integration](https://www.home-assistant.io/integrations/mqtt/)
1. Copy directory HA/custom_components/yoradio to .homeassistant/custom_components/
2. Add yoRadio entity into .homeassistant/configuration.yaml ([see example](HA/example_configuration.yaml))
3. Restart Home Assistant
4. Add Lovelace Media Player card to UI (or [mini-media-player](https://github.com/kalkih/mini-media-player) card)
5. Well done!

---
## More features
- Can add up to 65535 stations to a playlist. Supports and imports [KaRadio](https://github.com/karawin/Ka-Radio32) playlists (WebStations.txt)
- Telnet with KaRadio format output \
  **see [list of available commands](https://github.com/e2002/yoradio/wiki/List-of-available-commands-(UART-telnet-GET-POST))**

- MQTT support \
 **Topics**: \
 **MQTT_ROOT_TOPIC/command**     - Commands \
 **MQTT_ROOT_TOPIC/status**      - Player status \
 **MQTT_ROOT_TOPIC/playlist**    - Playlist URL \
 **MQTT_ROOT_TOPIC/volume**      - Current volume \
 **Commands**: \
 **prev**          - prev station \
 **next**          - next station \
 **toggle**        - start/stop playing \
 **stop**          - stop playing \
 **start, play**   - start playing \
 **boot, reboot**  - reboot \
 **volm**          - step vol down \
 **volp**          - step vol up \
 **vol x**         - set volume \
 **play x**        - play station x

- Home Assistant support

---
## Plugins
The `Plugin` class serves as a base class for creating plugins that hook into various system events.  
To use it, inherit from `Plugin` and override the necessary virtual methods.  
Place your new class in the `src/plugins/<MyPlugin>` directory
More details can be found in the comments within the `yoRadio/src/pluginsManager/pluginsManager.h` file and at [here](https://github.com/e2002/yoradio/blob/main/yoRadio/src/pluginsManager/README.md).  
Additional examples are provided in the `examples/plugins` folder.
Work is in progress...

---
## Version history

### 0.9.533
- fixed compilation error for esp32 core version lower than 3.0.0
- fixed error setting display brightness to 1
- fixed error setting IR tolerance value (upload a new file `options.html.gz` via WEB Board Uploader and press Ctrl+F5 on the settings page)

### 0.9.530
- optimization of webserver/socket code in netserver.cpp, part#1
- added support for ArduinoOTA (OTA update from Arduino IDE) (disabled by default)\
  to enable you need to add to myoptions.h: `#define USE_OTA true`\
  set password: in myoptions.h `#define OTA_PASS "myotapassword12345"`
- in web interface added basic HTTP authentication capability (disabled by default)\
  to enable you need to add to myoptions.h:\
  `#define HTTP_USER "user"`\
  `#define HTTP_PASS "password"`
- added "emergency firmware uploader" form (for unforeseen cases) http://ipaddress/emergency
- added config (sys.config) telnet command that displays the same information usually shown over serial at boot.
- bug fixes 🪲

### 0.9.515
- fixed a bug with resetting all parameters when resetting only one section of parameters

### 0.9.512
- fixed bug with saving ntp server #1 value

### 0.9.511
In this version, the contents of the data/www directory have changed, so that the first time you flash it, you will be greeted by WEB Board Uploader. Just upload all the files from data/www (11 pcs) to it\
or -> **!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- fixed a bug with saving smartstart mode
- fixed a bug with no restart when initially uploading files to spiffs
- fixed a bug with hanging on unavailable hosts
- fixed a bug with attempting to connect with an empty playlist
- fixed a bug with passing strings with quotes in mqtt
- fixing some other bugs
- web interface rewritten from scratch (well, almost), bugs added 👍
- added listening to links in the browser in playlistEditor
- buttons reboot (reboot) format (spiffs format) and reset (reset settings to default) have been added to the settings
- the beginnings of theming (theme.css) (just a list of global colors that can be changed, and then uploaded to theme.css via WB uploader)

### 0.9.434
- fixed the issue with exiting Screensaver Blank Screen mode via button presses and IR commands.
- reduced the minimum frequency for tone control on I2S modules to 80Hz.
- increased the display update task delay to 10 TICKS.
  to revert to the previous setting, add `#define DSP_TASK_DELAY 2` to `myoptions.h`.
- when ENCODER2 is connected, the UP and DOWN buttons now work as PREV and NEXT (single click).
- implemented backlight off in Screensaver Blank Screen mode.

### 0.9.428
- fixed freezing after SD scanning during playback
- AsyncWebSocket queue increased to 128
- fixed VU meter  overlapping the clock on displays
- fixed Guru Meditation error when loading in SD mode with SD card removed

### 0.9.420
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- added screensaver mode during playback, configurable via the web interface, pull request[#129](https://github.com/e2002/yoradio/pull/129)
- added blank screen mode to screensaver, configurable via the web interface, pull request[#129](https://github.com/e2002/yoradio/pull/129)
  Thanks to @trip5 for the amazing code!
- speeding up indexing of SD cards (advice - don't put all files in one folder)
- i don't remember (honestly) why the AsyncTCP server worked on the same core with the player, now it works on the same core with the display
  `#define CONFIG_ASYNC_TCP_RUNNING_CORE 0`
- bug fixes

### v0.9.412
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- added mDNS support, configurable via the web interface, pull[#125](https://github.com/e2002/yoradio/pull/125)
- added a setting that allows you to switch stations with the UP and DOWN buttons immediately, bypassing the playlist, configurable via the web interface, pull[#125](https://github.com/e2002/yoradio/pull/125)

### v0.9.399
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- added a screensaver mode, configurable via the web interface.
- changes to the tone control algorithm for the VS1053.

### v0.9.390
- updated the VU meter algorithms - shamelessly borrowed from @schreibfaul1, ([thanks a lot!](https://github.com/schreibfaul1/ESP32-audioI2S/blob/1296374fc513a6d6bfaa3b1ca08f6ba938b18d99/src/Audio.cpp#L5030))
- fixed the magic error "HSPI" redefined.

### v0.9.380
- fixed compilation error for ESP32 cores >= 3.1.0
- fixed freezing error with incorrectly configured RTC module
- [www|uart|telnet] new command `mode` - change SD/WEB mode. (0 - WEB, 1 - SD, 2 - Toggle)
  example: http://\<ipaddress\>/?mode=2

#### v0.9.375
- fixed the issue with saving settings for TIMEZONE.

### v0.9.373
- fixed the issue with displaying the settings page on fresh ESP modules after saving the weather key
 (a [reset](https://github.com/e2002/yoradio/wiki/List-of-available-commands-(UART-telnet-GET-POST)) may be required)

#### v0.9.370
- fixed the issue with saving settings on fresh ESP modules.

#### v0.9.369
- fixed the issue with the non-functional HSPI bus

#### v0.9.368
- SD Card - optimization and bug fixes
- Config - improvements and bug fixes
- Added stream format display in the web interface **!!! A full update is required, including SPIFFS Data !!!**  
  *(Alternatively, upload the new `style.css.gz` and `script.js.gz` files via the web interface.)*
- The content of `ehRadio.ino` has been moved to `src/main.cpp`
- [www|uart|telnet] new command: `reset` - resets settings to default values. [More details](https://github.com/e2002/yoradio/wiki/List-of-available-commands-(UART-telnet-GET-POST))
- Fixed compilation error: `'ets_printf' was not declared in this scope`

#### v0.9.351
- fixed freezing when loading without plugins in some configurations "running dots"

#### v0.9.350
- **Added parameters for configuring `LED_BUILTIN` on ESP32S3 modules:**
  - `USE_BUILTIN_LED`: Determines whether to use the built-in `LED_BUILTIN` (default is `true`).
  - `LED_BUILTIN_S3`: Specifies a custom pin for the built-in `LED_BUILTIN`. Used in combination with `USE_BUILTIN_LED = false` (default is `255`).

  **Note:** For ESP32S3 boards, no changes are required by default; the onboard LED will work as expected.  
  These settings were added to allow disabling the built-in LED or reassigning it to a custom pin.

- **New class for plugin management**, enabling multiple plugins to be assigned to each function.  
  More details can be found in the comments within the `yoRadio/src/pluginsManager/pluginsManager.h` file and at [here](https://github.com/e2002/yoradio/blob/main/yoRadio/src/pluginsManager/README.md).  
  Additional examples are provided in the `examples/plugins` folder.
  **Backward compatibility:** The old method of adding plugins will remain functional for some time in future versions but will eventually be deprecated and removed.

#### v0.9.342b
- fixed compilation error for OLED displays

#### v0.9.340b
- fixed compilation error audioVS1053Ex.cpp:181:5: error: 'sdog' was not declared in this scope

#### v0.9.337b (homeassistant component)
- fixed the error of subscribing to mqtt topic on some systems

#### v0.9.337b
- added support for Arduino ESP32 v3.0.0 and later
- disabled SD indexing on startup; now the card is indexed only if the `data/index.dat` file is missing from the card
- IRremoteESP8266 library integrated into the project (`yoRadio/src/IRremoteESP8266`)

#### v0.9.313b
- added support for ESP32-S3 boards (ESP32 S3 Dev Module) (esp32 cores version 3.x.x is not supported yet)
- fixes in displaying sliders in the web interface

#### v0.9.300 (homeassistant component)
- HA component >> bug fixes in the component for newer versions of Home Assistant

#### v0.9.300
- added the ability to play SDCARD without an Internet connection. More in [Wiki](https://github.com/e2002/yoradio/wiki/A-little-about-SD-CARD-and-RTC)

#### v0.9.280
- fixed an issue where it was impossible to reconnect when the WiFi connection was lost

#### v0.9.273
- fixed an "Guru Meditation Error" when playing streams with the ESP32 v2.0.10 and higher core installed

#### v0.9.260
- fixed date display bug for ILI9488/ILI9486 displays

#### v0.9.259
- fixed a hang bug when switching to SD mode after removing the SD
- fixed a hangup error when the connection to the stream was lost in WEB mode

#### v0.9.250
- added support for DS1307 or DS3231 RTC module (you need to install the RTCLib library in the library manager)
- setup
```
#define RTC_MODULE	DS3231	/* or DS1307	*/
#define RTC_SDA       <pin>
#define RTC_SCL       <pin>
```

#### v0.9.242
- fixed a hang bug when scrolling through an SD playlist with an encoder in configurations with VS1053B
- fixed a hang bug when quickly switching SD / WEB modes from the WEB interface in configurations with VS1053B
- fixes in the logic of work

#### v0.9.236
- fix compilation error 'class NetServer' has no member named 'resetQueue'

#### v0.9.235
- SD card playlist moved from SPIFFS to SD card
- new parameter #define SD_MAX_LEVELS - Search depth for files on SD card
- fixed bugs with SD card in multi-threaded mode

#### v0.9.220
- fixed SD prelist indexing error when switching Web>>SD
- fixed a bug of switching to the next track when accidentally playing SD
- fixed import of large playlists (tried). PS: import playlist size is limited by SPIFFS size (SPIFFS.totalBytes()/100*65-SPIFFS.usedBytes() = approximately 60kb )
- new url parameter - http://YPRADIOIP/?clearspiffs - for clearing tails from SD playlist
- optimization of the issuance of the WEB-interface
- brought back the functionality of the track slider
- fixing bugs in the application logic

#### v0.9.201
- fixed a bug when importing a playlist

#### v0.9.200
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- implementation of WEB/SD mode switching without reboot
- replacement of SD cards without turning off the power
- switching WEB / SD from the web interface. full update required, including SPIFFS Data
- fixing the Home Assistant integration behavior logic
- SD_HSPI parameter now works. Pins HSPI - 13(MOSI) 12(MISO) 14(CLK)
- new parameter SD_SPIPINS. ```#define SD_SPIPINS sck, miso, mosi```
- sck, miso, mosi - any available pins. Used for "TTGO Tm Music Album" boards ```#define SD_SPIPINS 14, 2, 15```
- fixed a bug with garbage appearing on display ILI9225
- the slider for moving along the SD track is temporarily not working.
- bug fixes

#### v0.9.180
- OneButton library moved to the project

#### v0.9.177
- fixed bitrate display error when playing SD on VS1053B modules

#### v0.9.174
- added forced shutdown of smartstart when WebSocket freezes on problem stations
- added bitrate icon when playing files from SD card
- fix html markup errors

#### v0.9.161
- fixed errors 403 Account already in use, 401 Authorization required
- fixed bitrate icon overflow bug
- fix html markup errors

#### v0.9.156
- fixed bug of random change of playback location when playing files from SD card

#### v0.9.155
- added bitrate badget for displays ST7789, ST7796, ILI9488, ILI9486, ILI9341, ILI9225 and ST7735(BLACKTAB)
  (disable: #define BITRATE_FULL false)
- fixed a bug with garbage appearing on display ILI9225

#### v0.9.143
- fixed NOKIA5110 display invert/off bug

#### v0.9.142
- fixed a bug with smartstart playback when the power is turned off

#### v0.9.141
- fixed error reconnecting to WiFi when connection is lost
- ADDED a compilation error when choosing a board other than "ESP32 Dev Module" or "ESP32 Wrover Module"

#### v0.9.130
- fixed crash in configurations with NOKIA5110 displays
- fixed bug with displaying buffer indicator when switching audioinfo
- fixed bug with displaying the buffer indicator when the connection is lost
- fixed bug of MUTE_PIN failure when connection is lost
- other minor fixes

#### v0.9.122
- fixed a bug in the operation of SSD1305 displays
- fixed bug in operation of LCD1602/2004 displays
- fixed errors in Serial Monitor output

#### v0.9.110
- optimization and bug fixes (display, player, netserver, telnet. mqtt)

#### v0.9.084
- monospace fonts for clock on TFT displays. Fonts can be restored to their original form by adding the ```#define CLOCKFONT_MONO false``` parameter to the myoptions.h file
- new parameter ```#define COLOR_CLOCK_BG R,G,B``` - color of inactive clock segments

#### v0.9.058
- added support for ST7920 128x64 2.6' OLED display https://aliexpress.com/item/32699482638.html

#### v0.9.045
- added support for SSD1322 256x64 2.8' OLED display https://aliexpress.com/item/1005003480981568.html

#### v0.9.022
- optimization of the display of the list of stations
- now the playlist size can be changed with one parameter in the yoRadio/src/displays/conf/display<span>_XXXX_</span>conf.h file --> _const ScrollConfig playlistConf_ param #3
- fixed fonts for ILI9225 display
- fixes in Nextion displays
- bug fixes (including BUFFER FILLED IN 403 MS)

#### v0.9.001
- fixed compilation error netserver.cpp:63:28 for some configurations

#### v0.9.000
- added WEB Board Uploader. ESP32 Filesystem Uploader is no longer needed, the initial setup can be done in the browser. (see [wiki](https://github.com/e2002/yoradio/wiki/WEB-Board-Uploader) for more info)
- fixed error getting weather for some locations

#### v0.8.990
- fixed error displaying access point credentials when DSP_INVERT_TITLE is false
- fixed compilation error for OLED displays when DSP_INVERT_TITLE is false

#### v0.8.988
- **DSP_INVERT_TITLE** now works for all displays when assigned
   ```#define DSP_INVERT_TITLE false```
   the display title takes on a "classic" look (light letters on a dark background)
- advanced weather display - wind direction and strength, feels like
- sea level pressure changed to surface pressure
- added degree icon [\*ps]
- displaying the WiFi signal level in graphical form [\*ps]

[\*ps] - **glcdfont.c** from the **Adafruit_GFX_Library** library has been changed to add new icons, so for the correct display of all this, you need to replace the specified file in the Adafruit_GFX library with the file from the yoRadio/fonts/ folder

#### v0.8.962
- fixed reboot error after sending media from Home Assistant
- fixed bug when playing media from Home Assistant for VS1053
- fix grammar errors

#### v0.8.950
- added support for remote media playback from Home Assistant (Local Media, Radio Browser, TTS)

#### v0.8.933 (homeassistant component)
- HA component >> fixed bugs of getting and generating a playlist

#### v0.8.933
- added support for ILI9488 display
- added support for ILI9486 display in testing mode

#### v0.8.920
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!** \
**Please backup playlist.csv and wifi.csv before updating.**
- fixed bug with displaying horizontal scroll in playlist
- fixed compilation error with IR_PIN=255
- libraries async-mqtt-client, AsyncTCP, ESPAsyncWebServer moved to the project
- new parameter #define XTASK_MEM_SIZE - buffer size for AsyncTCP task (4096 by default)

#### v0.8.901
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!** \
**Please backup playlist.csv and wifi.csv before updating.**
- added SD Card support (more info in [connection table](#connection-tables) and [examples/myoptions.h](https://github.com/e2002/yoradio/blob/main/examples/myoptions.h))
- added MODE button to switch SD/WEB modes (more info in [Controls.md](https://github.com/e2002/yoradio/blob/main/Controls.md))
- asterisk on the remote control now switches SD/WEB modes
- double click BTN_CENTER and ENC_BTNB now toggles SD/WEB modes
- bug fixes

#### v0.8.173
- bootlog added
- fixed work of start/stop button in configurations with DSP_DUMMY

#### v0.8.138
- fixed unclosed comment in examples/myoptions.h

#### v0.8.137
- fixed compilation error without encoder

#### v0.8.135
- added numeric IR remote buttons in configurations with DSP_DUMMY
- fixed navigation bug in playlist with more than 255 stations
- fixed work of encoders in configurations with DSP_DUMMY
- fixed missing volume value bug when switching to volume control dialog
- LED_BUILTIN is now 255 by default (off)

#### v0.8.112
- fixed compilation error with BOOMBOX_STYLE parameter
- fixes in default configuration for GC9A01A display

#### v0.8.100
- added support for GC9A01A display https://aliexpress.com/item/1005004069703494.html?sku_id=12000029869654615

#### v0.8.089
- increased length of SSID string to 30 characters (requires full update + ESP32 Data Upload)
- fixed artifacts when adjusting the volume on OLED displays
- fixed bug with missing current station in playlist on OLED displays
- new parameter DSP_INVERT_TITLE - invert colors in station name for OLED displays (more details in examples/myoptions.h)

#### v0.8.03b
- added support for ST7796 display
- added support for capacitive touch GT911
- HSPI bus support added - DSP_HSPI, VS_HSPI, TS_HSPI options More details in examples/myoptions.h
- changed the method of connecting the touchscreen in myoptions.h Now instead of specifying TS_CS, you must specify TS_MODEL (by default TS_MODEL_UNDEFINED) More details in examples/myoptions.h
- new parameters TS_SDA, TS_SCL, TS_INT, TS_RST for GT911 touchscreen
- new parameters LIGHT_SENSOR and AUTOBACKLIGHT - to automatically adjust the brightness of the display. More details in examples/myoptions.h
- new parameter LED_INVERT (true/false) - to invert the behavior of the built-in LED
- fixed bug with extra sign } in humidity value

#### v0.8.02b
- fixed artifacts when displaying the volume level
- changes in mytheme.h . Added colors COLOR_PL_CURRENT, COLOR_PL_CURRENT_BG, COLOR_PL_CURRENT_FILL. Details in [examples/mytheme.h](examples/mytheme.h)

#### v0.8.01b
- fix INITR_MINI160x80 compiling error
- fix ENC_INTERNALPULLUP description in examples/myoptions.h

#### v0.8.00b
- rewritten the display engine
- added the ability to position widgets on the display using configuration files. More info in yoRadio/src/displays/conf/ and here https://github.com/e2002/yoradio/wiki/Widgets
- the VU_PARAMS3 parameter is deprecated. VUmeter configuration is done through yoRadio/src/displays/conf/ configs
- added bitrate display on displays
- added the ability to display the weather on all displays except LCD1602
- examples of plug-ins related to displaying information on the display are outdated and no longer work. The examples have been removed from the examples/plugins folder.
- the structure of the project files has been changed so that I don’t know what.
- localization of information displayed on the display (rus, en). Option DSP_LANGUAGE (EN by default. see examples/myoptions.h for details)
- changes in mytheme.h . Added colors COLOR_STATION_BG, COLOR_STATION_FILL, COLOR_BITRATE
- optimization, refactoring
- bugs fixes
- bugs adding
- probably something else that I forgot .__.

#### v0.7.540
- fixed compilation error when using NEXTION display with DUMMY display

#### v0.7.534
- added control via uart (see [list of commands](https://github.com/e2002/yoradio/wiki/List-of-available-commands-(UART-telnet-GET-POST))). The uart and telnet commands are the same.
- added additional commands
- added control via GET/POST (see [list of commands](https://github.com/e2002/yoradio/wiki/List-of-available-commands-(UART-telnet-GET-POST)))
- fixed clock operation when configured with DSP_DUMMY
- fixed RSSI display in web interface when configured with DSP_DUMMY
- added brightness control/on/off nextion displays from the web interface
- new parameter WAKE_PIN (to wake up esp after sleep command earlier than given time (see examples/myoptions.h and list of commands)
- minor memory optimization

#### v0.7.490
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!** \
**Please backup playlist.csv and wifi.csv before updating.**
- fixed playlist break down when saving it
- fixed bug with cropped song titles on single line displays (GC9106, ST7735mini, N5110 etc.)
- netserver - optimization and refactoring
- web interface optimization
- the AUDIOBUFFER_MULTIPLIER parameter is deprecated. New parameter AUDIOBUFFER_MULTIPLIER2. If everything works fine, then it is better not to touch it.
- new setting VS_PATCH_ENABLE (see PS)
- fixing other bugs

_**PS:** A bug was found with the lack of sound on some (not all) green VS1053 boards.
If there is no sound, you need to assign in myoptions_
```
#define VS_PATCH_ENABLE false
```
_On red boards and normally working green boards, nothing else needs to be done._

#### v0.7.414
- fixed non latin long titles of songs error

#### v0.7.402
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!** \
**Please backup playlist.csv and wifi.csv before updating.**
- added the ability to themize color displays. Details in [examples/mytheme.h](examples/mytheme.h)
- in this connection, examples of plugins displayhandlers.ino and rssibitrate.ino have been updated
- parameter VU_PARAMS2 is deprecated. New parameter - VU_PARAMS3. Details in [yoRadio/display_vu.h](yoRadio/display_vu.h)
- added deepsleep capability for LCD_I2C and OLED displays
- in this connection, a full update with Sketch data upload is required
- in this connection, example of plugin deepsleep.ino (examples/plugins/deepsleep.ino) have been updated
- some bug fixes

#### v0.7.355
- updating libraries ESP32-audioI2S and ESP32-vs1053_ext to the latest version
- optimization of the web interface during playback
- fixed one js bug. a [full update](#update-over-web-interface) with Sketch data upload is desirable
- plugin example for esp deep sleep when playback is stopped (examples/plugins/deepsleep.ino)

#### v0.7.330
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!** \
**Please backup playlist.csv and wifi.csv before updating.**
- added the ability to configure parameters through the [web interface](images/settings.png)
- new parameter BRIGHTNESS_PIN - pin for adjusting the brightness of the display. Details in [examples/myoptions.h](examples/myoptions.h#L105)
- the weather plugin is integrated into the code, the settings are made through the web interface

_**PS:** Due to the change in the storage location of settings in the ESP memory, settings such as:_ \
**smartstart, audioinfo, time zone, IR remote, last volume level, last played station, equalizer** \
_will have to be configured again through the web interface. Please understand and forgive._

#### v0.7.017
- fix initialization of some vs1053b green boards
- fix VU initialization on vs1053b boards

#### v0.7.010
- fixed choppy of sound when volume adjustment
- fixed initialisation of Nextion displays

#### v0.7.000
- added support for Nextion displays ([more info](nextion/README.md))
- fixed work of VU Meter
- fixed time lag when adjusting the volume / selecting a station
- optimization of work with the DSP_DUMMY option
- some bug fixes

#### v0.6.530
- adding VU meter for displays ST7735 160x80, GC9106 160x80, ILI9225 220x176, ST7789 240x240
- TFT_22_ILI9225 library is integrated into the project

#### v0.6.494
- adding VU meter for displays ST7735 160x128, ST7735 128x128, ILI9341 320x240, ST7789 320x240 \
  option ENABLE_VU_METER (see [myoptions.h](examples/myoptions.h) for example) \
  **!!! Important !!!** \
  if you enable this feature on the esp32 wroom, due to lack of memory, you must modify the file Arduino/libraries/AsyncTCP/src/AsyncTCP.cpp \
  **replace the line 221** \
  _xTaskCreateUniversal(_async_service_task, "async_tcp", 8192 * 2, NULL, 3, &_async_service_task_handle, CONFIG_ASYNC_TCP_RUNNING_CORE);_ \
  **with** \
  _xTaskCreateUniversal(_async_service_task, "async_tcp", 8192 / 2, NULL, 3, &_async_service_task_handle, CONFIG_ASYNC_TCP_RUNNING_CORE);_

#### v0.6.450
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- adding an IR remote control has been moved to the web-interface (more info in [Controls.md](Controls.md#ir-receiver))
- fixed broken internal DAC on esp32 core 2.0.3 and highest

#### v0.6.400
- fixed compilation errors with esp32 core 2.0.4

#### v0.6.380
**!!! a [full update](#update-over-web-interface) with Sketch data upload is required. After updating please press CTRL+F5 in browser !!!**
- fixed a bug when saving a playlist with special characters in the name and url
- fixed a bug when saving wifi settings with special characters in the name and password
- fixed css bugs

#### v0.6.357
- remove ZERO WIDTH NO-BREAK SPACE (BOM, ZWNBSP) from stream title

#### v0.6.355
- added support for ST7789 1.3' 240x240 SPI displays \
  _!!! Important !!! This display requires further development when used in conjunction with the VS1053 module._ \
  See this link for details https://www.instructables.com/Adding-CS-Pin-to-13-LCD/

#### v0.6.348
- fixed display bugs in the rssibitrate plugin
- fixed some compilation warnings

#### v0.6.345
- fix compilation error in rssibitrate plugin with ILI9225 display

#### v0.6.344
- fixed SPI-display bugs when used with VS1053B module
- added example plugin for analog volume control ([examples/plugins/analogvolume.ino](examples/plugins/analogvolume.ino))
- added example plugin for backlight control depending on playback ([examples/plugins/backlightcontrols.ino](examples/plugins/backlightcontrols.ino))
- added example plugin for replace a RSSI to bitrate information alternately ([examples/plugins/rssibitrate.ino](examples/plugins/rssibitrate.ino))

#### v0.6.320
- fixed ILI9225 display bug when used with VS1053B module
- fixed ILI9225 plugin support

#### v0.6.313
- added support for ILI9225 220x176 SPI displays
- added support for I2S internal DAC, option I2S_INTERNAL (see [myoptions.h](examples/myoptions.h#L111) for example) \
 _(this option worked only with esp32 core version==2.0.0)_
- new option SOFT_AP_REBOOT_DELAY (see [myoptions.h](examples/myoptions.h) for example)
- fixed MQTT connection when WiFi reconnected
- fixed date display for ILI9341 displays
- fixed garbage on volume control with displays ILI9341

#### v0.6.290
- fixed interface blocking error when synchronizing time
- time sync optimization
- new option **SNTP_SERVER**, to set your custom server for synchronization (see [myoptions.h](examples/myoptions.h) for example)

#### v0.6.278
- added support for LCD2004 displays
- added support for SSD1305/SSD1309 I2C displays
- fixed rotation of SH1106 display

#### v0.6.263
- fixed encoder internal pullup

#### v0.6.262
- change encoder library to [ai-esp32-rotary-encoder](https://github.com/igorantolic/ai-esp32-rotary-encoder) (injected to project)
- added new option VOL_ACCELERATION - volume adjustment acceleration by encoder (see [myoptions.h](examples/myoptions.h) for example)
- fixed connection error with http-stations on esp32-core v2.0.3
- fixed css errors (a [full update](#update-over-web-interface) is required)

#### v0.6.250
- added update via web-interface \
 **Attention! Full firmware with chip re-partitioning is required!** see [board setup example](#quick-start)
- fixed choppy when switching stations via Home Assistant

#### v0.6.220
- new option PLAYER_FORCE_MONO (with i2S DAC only)
- change default scroll speed in DSP_NOKIA5110
- improved reconnect to WiFi on connection loss

#### v0.6.210
- fixed choppy playback on DSP_ST7735 displays used with VS1053
- new option PL_WITH_NUMBERS (show the number of station in the playlist)
- fixed compiling error with DSP_DUMMY option
- correction of displays GC9106 and SSD1305

#### v0.6.202
- fixed errors in the operation of the second encoder
- rewrote [plugin example](examples/plugins/displayhandlers.ino)
- fixed compilation errors on macOS #2

#### v0.6.200
- please backup your playlist and wifi settings before updating (export)
- accelerated displays up to ~30fps (everything except LCD)
- corrections/additions in the WEB interface (a [full update](#update) is required)
- rewrote [plugin example](examples/plugins/displayhandlers.ino)
- fixed compilation errors on macOS
- changed the logic of the second encoder (switching to the volume control mode by double click)
- optimization, bug fixes
- probably some other things that I forgot about %)

#### v0.6.120
- added support for GC9106 160x80 SPI displays
- fixed compiling error with DSP_DUMMY option
- fixed compiling error with DSP_1602I2C / DSP_1602 option

#### v0.6.110
- the logic of division by cores has been changed
- fixed choppy playback (again)
- improvements in the stability of the web interface
- increased smoothness of the encoder
- bug fixes
- bug fixes

#### v0.6.012
- fixed choppy playback

#### v0.6.010
- added displays [SSD1327](https://aliexpress.com/item/1005001414175498.html), [ILI9341](https://aliexpress.com/item/33048191074.html), [SSD1305/SSD1309](https://aliexpress.com/item/32950307344.html), [SH1107](https://aliexpress.com/item/4000551696674.html), [1602](https://aliexpress.com/item/32685016568.html)
- added [touchscreen](Controls.md#touchscreen) support
- tasks are divided into cores, now the sound is not interrupted when selecting stations / volume
- increased speed of some displays
- optimization of algorithms, bugs fixes

#### v0.5.070
- added something similar to plugins

#### v0.5.035
- added two buttons BTN_UP, BTN_DOWN
- added the pins for the second encoder ENC2_BTNL, ENC2_BTNB, ENC2_BTNR
- fixed display of playlist with SSD1306 configuration
- improvements in the displays work
- bugs fixes, some improvements

#### v0.5.020
- added support for SSD1306 128x32 I2C displays

#### v0.5.010
- added support for ST7789 320x240 SPI displays
- added support for SH1106 I2C displays
- added support for 1602 16x2 I2C displays
- a little modified control logic
- added buttons long press feature
- small changes in options.h, check the correctness of your myoptions.h file
- bugs fixes

#### v0.4.323
- fixed bug [Equalizer not come visible after go to playlist](https://github.com/e2002/yoradio/issues/1) \
 (a [full update](#update) is required)
- fixed playlist filling error in home assistant component

#### v0.4.322
- fixed garbage in MQTT payload

#### v0.4.320
- MQTT support

<img src="images/mqtt.jpg" width="680" height="110">

#### v0.4.315
- added support for digital buttons for the IR control \
(num keys - enter number of station, ok - play, hash - cancel)
- added buttons for exporting settings from the web interface
- added MUTE_PIN to be able to control the audio output
- fixed js/html bugs (a [full update](#update) is required)

#### v0.4.298
- fixed playlist scrollbar in Chrome (a [full update](#update) is required)

#### v0.4.297
- fix _"Could not decode a text frame as UTF-8"_ websocket error _//Thanks for [Verholazila](https://4pda.to/forum/index.php?s=&showtopic=1010378&view=findpost&p=113551446)_
- fix display of non-latin characters in the web interface
- fix css in Chrome (a [full update](#update) is required)

#### v0.4.293
- IR repeat fix

#### v0.4.292
- added support for IR control
- new options in options.h (ENC_INTERNALPULLUP, ENC_HALFQUARD, BTN_INTERNALPULLUP, VOL_STEP) _//Thanks for [Buska1968](https://4pda.to/forum/index.php?s=&showtopic=1010378&view=findpost&p=113385448)_
- compilation error for module SSD1306 with arduino-esp32 version newest than 2.0.0
- fix compiler warnings in options.h
- fix some compiler warnings

#### v0.4.260
- added control of balance and equalizer for VS1053
- **TFT_ROTATE** and st7735 **DTYPE** moved to myoptions.h

#### v0.4.251
- fixed compilation error bug when using VS1053 together with ST7735

#### v0.4.249
- fix VS1003/1053 reseting
- fix css in Firefox
- fix font in NOKIA5110 display

#### v0.4.248
- added support for VS1053 module _in testing mode_

#### v0.4.210
- added timezone config by telnet
- fix telnet output
- some separation apples and oranges

#### v0.4.199
- excluded required installation of all libraries for displays

#### v0.4.197
- added support for Nokia 5110 SPI displays
- some bugs fixes

#### v0.4.183
- ovol reading bug

#### v0.4.182
- display connection algorithm changed
- added support for myoptions.h file for custom settings

#### v0.4.180
- vol steps 0..256 (in ESP32-audioI2S)

#### v0.4.177
- added support for SSD1306 I2C displays
- fixed broken buttons.
