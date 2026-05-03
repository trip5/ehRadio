# ehRadio AI Coding Guide

## Note
- **This file**: If you wish to NOT use this file, feel free to add it to your own private exclusions (`.git/info/exclude`).  Please do not try to sync your own version of this file back in a PR to `ehRadio:dev`.
- **code-summary.md**: The ehRadio `Bible` may be hard for a human to read but provides a good (if lengthy) summary of the codebase to be more easily digested.

## About
- **ehRadio**: A fork of [yoRadio](https://github.com/e2002/yoradio) with added Web UI functionality, improved Web UI, usability, and customization.
- **Arduino**: Trip5 hosts firmware files, flashing tools, and Releases on the [Github Repo](https://github.com/trip5/ehRadio) and [Github Page](https://trip5.github.io/ehRadio/)
- **Online Flasher**: Releases are also available through the [ESP Web Tool](https://trip5.github.io/ehRadio/firmware.html)
- **Online Updating**: Files on a running device are updated using ESPFileUpdater from the Releases on the Github Repo.

## ⚠️ Critical Rules
- **Rule #1**: Before making changes spanning more than 25 lines, explain what changes will be made and ask the user to confirm before proceeding. If the user is already in Plan mode and has explicitly said "start implementation" (or equivalent), confirmation is already granted — do NOT re-ask or re-evaluate; proceed immediately.
- **Rule #2**: Before making changes spanning more than 50 lines, stop and tell the user: "This change is large. Please switch to Plan mode so we can review a plan before making edits." Only proceed if the user confirms or switches to Plan mode. Once the user has reviewed the plan and said "start implementation" (or equivalent), treat that as full approval for everything discussed in the plan — do NOT re-interpret, re-confirm, or second-guess scope; implement exactly what was agreed.
- **Rule #3**: Do not edit `myoptions.h`, `src/core/options.h`, or `platformio.ini` without explicit user confirmation first.
- **Rule #4**: If a change affects code interactions (how files/modules interact), storage keys, WebUI contracts, locale/build/dependency behavior, or other external contracts,`.github/code-summary.md` MUST be updated in the same change set; bug fixes that restore expected behavior are exempt unless they also change code interactions or external behavior/contracts.
- **Rule #5**: Before any non-trivial edit (anything beyond a single isolated line fix), you MUST read `.github/code-summary.md` first. If the change touches more than one file or an area flagged as risky in that document, read the relevant per-file sections before writing a single line of code. Check `.github/code-issues.md` for any open issues in the area you are editing. Do not proceed without doing this.

## Project Structure
- **Config Cascade**: `platformio.ini` (env #define) → `myoptions.h` (hardware profile, user defaults) → `mytheme.h` (UI theme) → `options.h` (fallback defaults for anything undefined) → `optionschecker.h` (forces compile failure when certain defines are incorrect).
- **Core logic**: `src/core/` (Player, Display, Network, Config, Controls).
- **Libraries path**: Software codecs: `libraries/I2S_Audio/`, `libraries/ES8311_Audio` / Hardware decoder: `libraries/VS1053_Audio/` (Hardware chip), other folders are custom drivers for other display, touchscreen, and other hardware.
- **UI**: Widgets in `src/displays/widgets/`, drivers in `src/displays/`.
- **Plugins**: Class-based hooks in `src/plugins/`, registered in `main.cpp`.
- **Web UI**: Most files in `data/www` are served with headers in `src/core/netserver.h`. `search.html` and `curated.html` are not.

## Functionality
- **Hardware**: The firmware is built according to the hardware that is connected to it and users who will use it.  These are defined by files listed in Config Cascade.
- **Software**: `src/core/options.h` and the Config Cascade should be used to extend functionality, not limit it. `#if defined` and `#ifndef` should not be used in the code for configuration not related to hardware.
- **Granular Control in Web UI**: If not hardware-related, functionality should be changeable in the Web UI, not controlled by a `#define` in Config Cascade.
