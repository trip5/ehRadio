<!--
![image](./images/logo-color.svg)
-->

<img src="images/logo-color.svg" width="50%">

# ehRadio

***This documentation is the same on the [Github Page](https://trip5.github.io/ehRadio/), which may be easier to read.***

## Introduction

ehRadio runs on an ESP32 to play Internet radio streams. 

An radio may be built using an ESP32, an audio decoder, a display, and some inputs.
I prefer to build with ESP32-S3 boards but ESP32 or ESP32-C3 boards are possible, too.

To develop, I prefer [VS Code](https://code.visualstudio.com/) but you may try other IDEs.
I compiled using [Platformio](https://platformio.org/) but it may compile in Arduino IDE as well
Some libraries may only be available from [Platformio Registry](https://registry.platformio.org/).

ehRadio is a fork of [ёRadio](https://github.com/e2002/yoradio/) / yoRadio v0.9.533.
Read the [A History of ESP Radios](#a-history-of-esp-radios).

## Features

ehRadio inherits a lot from ёRadio, so here's some similarities and differences,
especially in terms of how they are used and how they are built.

### Features For Users

<table><tr valign="top"><td align="left" width="50%">

#### ёRadio

  - Physical controls (decided by builder)
    - up to 2 rotaries
    - up to 6 buttons
    - touchscreen control (very basic)
      - Nextion uses advanced control
  - WebUI interface
    - control playback
    - edit/import playlists
    - change certain settings
  - MQTT, Telnet, HTTP
    - mostly used for playback
    - some settings can be changed
  - Home Assistant integration (through MQTT)

</td><td align="left" width="50%">

#### ehRadio

  - Includes all ёRadio physical controls
    - Nextion support is incomplete or broken
  - WebUI very similar but with added functionality
    - Radio Station search
      - uses Radio-browser API
      - auto-updates server list
    - Curated lists
      - can download/merge/preview other playlists
    - Mobile device screen optimization
  - eh Discovery Protocol
    - mobile app can be used to quickly open the WebUI
  - Proper time zone support
    - Daylight times
    - Auto-updates the list
  - Web Flasher
    - improv used to set Wifi
  - Firmware can be OTA updated
    - new binary downloaded and applied automatically
    - will automatically update data/www files
    - WebUI automatically reloads
  - Unified commandhandler
    - WebUI, MQTT, Telnet, HTTP commands same

</td></tr></table>

### Features For Builders

<table><tr valign="top"><td align="left" width="50%">

#### ёRadio

  - Primarily focused on ESP32
    - 4MB of flash is fine
    - ESP-WROVER has PSRAM, works better
    - ESP32-WROOM has no PSRAM but still functions
    - Support for ESP32-S3
    - ESP32-C3
  - Multiple audio decoders available
    - I2S PCM decoder
    - VS1053 (or VS1003)
    - ESP32's builtin DAC
  - multiple displays
    - see `options.h` for a full list
  - SPI buses use default pins
    - SPI display and VS1053 must use VSPI or HSPI
      - VSPI and HSPI are in board definition files
    - ESP32 defines VSPI/SPI3 bus or HSPI/SPI2 bus
    - ESP32-S3 defines FSPI/SPI2 (HSPI in `myoptions.h`)
      - board defines do not have a second bus
      - no way to use VS1053 and SPI display together
    - ESP32-C3 defines FSPI/SPI2
      - this chip can't have second bus anyways
    - SD and touchscreen can use any pins
      - no enforcement for bus limit

</td><td align="left" width="50%">

#### ehRadio

  - Primarily focused on ESP32-S3
    - 8MB flash recommended
      - 4MB may work but usability features will break
    - ESP-WROVER with PSRAM probably supported but untested
    - ESP32-WROOM likely doesn't work
    - Support for ESP32-C3 but untested
  - Multiple audio decoders available
    - I2S and VS1053 updated (for higher bitrate streams)
  - same display architecture
    - supported displays same as of ёRadio v0.9.533
    - ёRadio and ehRadio display configs should be compatible with each other
  - Almost all user default settings can be set in `myoptions.h`
  - SPI buses can use custom pins
    - define the bus pins in `myoptions.h`
      - needed for boards that don't have board definitions
      - fixes ESP32-S3 problem with SPI display and VS1053
    - SPI display will always use Bus A
    - other devices can be assigned to Bus A or B

</td></tr></table>

---

## Tools

[Online Flasher](https://trip5.github.io/ehRadio/firmware.html)

[myoptions Generator](https://trip5.github.io/ehRadio/myoptions/generator.html)

Many more tools are available in the codebase as well.

---

## Documentation

I realize documentation is a little sparse right now.  I'm working on it.

A lot of build options and comments and notes are actually in `options.h` and in various files.

As of `2026.05.18` I've done a lot of work to clean up the codebase and organize it.

More work remains to be done and the `Feature Freeze` will remain in place at least until the "de-fork" of the audio libraries is finished.

---

## A History of ESP Radios

### In the beginning...

Edzelf was probably the first to work on the idea of an ESP-based radio with [Esp-radio](https://github.com/Edzelf/Esp-radio/), making his first Github upload
[April 4, 2016](https://github.com/Edzelf/Esp-radio/tree/0a53a03c2301e9e5f0bfaee418942be67739dee0) which included audio decoding with a VS1053 decoder, built on a ESP8266.

Edzelf then made [ESP32-Radio](https://github.com/Edzelf/ESP32-Radio/), with his first release on [May 23, 2017](https://github.com/Edzelf/ESP32-Radio/tree/c268677dd8e46db2b7a8bfbd12a131c169e32019).
Later, [ESP32Radio-V2](https://github.com/Edzelf/ESP32Radio-V2/) was created on [October 4, 2021](https://github.com/Edzelf/ESP32Radio-V2/tree/81ea92481eb36e49c8983e9dd1e5a34fecca73a9) and includes I2C audio decoding (see below).
Edzelf still maintains ESP32Radio-V2.

karawin began work on [Ka-Radio](https://github.com/karawin/Ka-Radio) with his first Github submission [June 15, 2016](https://github.com/karawin/Ka-Radio/tree/13df16e5bd4dcf646ebfa6ffbb1eeb43173d2093) for VS1053 and ESP8266.
Unlike the other projects and libraries here, it is not done using Arduino code. It uses the ESP8266 RTOS SDK directly using C and assmbly.
karawin continued with [Ka-Radio32](https://github.com/karawin/Ka-Radio32/), with the first commits on Github [September 20, 2017](https://github.com/karawin/Ka-Radio32/tree/8cfd1f9e41fff42723dcfb8ea1a3244bfe5ef4ae).
Ka-Radio32 credits MrBuddyCasino's [ESP32_MP3_Decoder](https://github.com/MrBuddyCasino/ESP32_MP3_Decoder) for its I2S audio decoder, which was first put on Github
[January 19, 2019](https://github.com/MrBuddyCasino/ESP32_MP3_Decoder/tree/f1a92e1fbdcca3f2ffc6570aa599bc24806d8fd1).

At some point, schreibfaul1 began work on the [ESP32-vs1053_ext](https://github.com/schreibfaul1/ESP32-vs1053_ext) library, making his first version available
on Github [October 13, 2017](https://github.com/schreibfaul1/ESP32-vs1053_ext/tree/651dcce0f7d617a81153bc9a078ee8542db200f1), crediting Edzelf's ESP32-Radio as inspiration.
Further work has been done by [nstepanets](https://github.com/nstepanets/ESP32-vs1053_ext) who took over [October 25, 2025](https://github.com/nstepanets/ESP32-vs1053_ext/tree/bddf3137ec361a6b13d0a1687d3065561d507b8c).

schreibfaul1 also created the [ESP32-audioI2S](https://github.com/schreibfaul1/ESP32-audioI2S) library, making his first version available on Github [October 28, 2018](https://github.com/schreibfaul1/ESP32-audioI2S/tree/dc87801a7b1b369925080276cedd5a019715470a).
Work continues on that library today. Coming full circle, Edzelf likely adapted this library when working on ESP32Radio-V2 radio.

It is hard to say definitively, but more than likely all of these projects have inspired each other in various ways.  The DNA of all these projects are still evident in ёRadio and ehRadio.

If you have more information about the history of these projects or corrections to this story, I'd be interested to know!

### ёRadio

Russian site 4PDA's megathread "WI-FI internet radio DIY" started in [November 21, 2020](https://4pda.to/forum/index.php?showtopic=1010378), at first primarily centered around modifications to Ka-Radio32.
This thread contains a lot of useful information regarding hardware. I can't say how much of this information was used to improve Ka-Radio but easy (e2002) and Wolle (schreibfaul1) both appear to have improved their libraries using this information.

e2002 began sharing his work on the 4PDA "WI-FI internet radio DIY" thread [January 25, 2022](https://4pda.to/forum/index.php?showtopic=1010378&st=1800#entry112992611).

[ёRadio](https://github.com/e2002/yoradio/) v0.4.170 was first added to Github [Feb 4, 2022](https://github.com/e2002/yoradio/tree/6c847cdc308150e786e5340200f8e3ea18c01042), based primarily on schreibfaul1's libraries.

### ehRadio

In July 2023, I built my first radio using Edzelf's [ESP32Radio-V2](https://github.com/Edzelf/ESP32Radio-V2/).
In November 2024, I discovered [ёRadio](https://github.com/e2002/yoradio/) and on December 26, 2024, I made my [first PR](https://github.com/e2002/yoradio/pull/125).

In May I began adding full support for Radio-browser API with the hope it would improve a user's experience. I also merged many of maleksm's mods.
That PR started [June 13, 2025](https://github.com/e2002/yoradio/pull/184) was abandoned June 19, 2025 with a ridiculous amount of changes.

In retrospect, proposing thousands of lines of changes was rude and unrealistic.  After some thought, ehRadio was officially forked August 10, 2025.

As of 2026.05.08, ehRadio uses the `ESP32-audioI2S` library from [Maleksm's ёRadio mod v0.9.512m](https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228),
likely mostly from schreibfaul1's library [3.1.0 January 7, 2025](https://github.com/schreibfaul1/ESP32-audioI2S/releases/tag/3.1.0).
ehRadio also currently uses the `ESP32-vs1053_ext` library from Maleksm's ёRadio mod v0.9.512m, likely mostly from schreibfaul1's [final version](https://github.com/schreibfaul1/ESP32-vs1053_ext).
These libraries are so intertwined with the codebase that it may be impossible to migrate to newer versions, but... I will try.

For that and other major needed changes to the codebase, I maintain [code-issues.md](/.github/code-issues.md), which may be a messy file to look at, depending on how these efforts are going.

I will add a note here that although I do use AI-assisted coding, I am not a "vibe-coder" by any measure.

---

## Update History

### Updates

| Date       | Release Notes    |
| ---------- | ---------------- |
| 2026.05.18 | This readme, myoptions generator, cpu cores/stack sizes optimized (monitor added), auto dimming, plugins removed, general & specific code repair, refactor, optimization |
| 2026.05.08 | SPI buses more flexible, unified commandhandler and error logging, Home Assistant component fixed, OTA & naming methods finalized |
| 2026.04.09 | major and minor changes to structure, aggressive reconnect to wi-fi |
| 2026.03.30 | feature freeze begins, OTA page reload graceful, 3 javascript files combined to 1 (`script2.js`) |
| 2026.03.22 | ehDP added, playlist editor grabbable fixed |
| 2026.03.18 | multiple weather providers, screensaver mode fixed, https connection improved, w/Kasperaitis: multi-locales in display and WebUI, Kasperaitis: battery handling |
| 2026.02.18 | WebUI improved, Curated Lists, default playlist on first boot, Smart start fixed, SPIFFS cleanup, Kasperaitis: battery monitor and telnet formatting |
| 2026.02.06 | online flasher, improv mode |
| 2026.02.04 | scan/connect wi-fi, use PIO libraries and less local libraries, Kasperaitis: ES8311 and FT6336 (for ES3C28) |
| 2025.08.31 | Display fixes and other fixes from ёRadio up to v0.9.693 including framebuffer |
| 2025.08.20 | Online updater fixed, WebUI fixes for mobile displays, MQTT added to WebUI |
| 2025.08.12 | `builds\` folder added to share configurations, hotspot AP mode fixed, preferences in `myoptions.h`, improvements from ёRadio v0.9.574 |
| 2025.08.10 | ehRadio fork begins, folders restructured, `data\` files stay uncompressed (compressed in from Releases, which radio can download), timezones.json updated automatically |
| 2025.07.23 | more options in WebUI |
| 2025.07.19 | PR to ёRadio v0.9.533: EEProm storage changed to Preferences, fixes for screens that can't display certain characters, ESPFileUpdater added (updates timezones and Radio Browser servers), proper timezones, many macros added to `myoptions.h`, Radio station search, Playback queue now RTOS background task, improved JSON and CSV file importing, maleksm's backlightdown and decoder improvements, ESP8266 support removed |

### Old Readme

A full history of ёRadio from v0.4.177 to v0.9.533 and to ehRadio 2026.05.08 can be seen in the [old Readme](README.old.md).

### Credit

Thanks to:
  - [Kasperaitis](https://github.com/kasperaitis) - for work initiating locales (WebUI and display language, display fonts, etc.) and a bunch of work for ES3C28P (including ES8311 decoder, ILI9341 battery widget, FT6336 touchscreen)
  - [e2002](https://github.com/e2002) - for [ёRadio](https://github.com/e2002/yoradio/)

