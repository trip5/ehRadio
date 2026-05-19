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

**Rule #1**: **Plan Mode for Large Changes**
- For changes spanning **more than 50 lines**, you **MUST STOP** and inform the user:  
  > "This change is large. Please switch to Plan mode so we can review a plan before making edits."  
- Only proceed if the user explicitly confirms or switches to Plan mode.
- In Plan mode, once the user states "start implementation" (or equivalent), **implement exactly as agreed**. Do **NOT re-confirm or reinterpret** the scope — follow the finalized plan.

**Rule #2**: **One File/Simple Changes Exception**
- The only exception to Rule #1 (satisfy **all 3 points**):
  - Changes are **less than or equal to 50 lines**.
  - The changes affect **only one file or one logical set of files** (e.g., a `.cpp` file and its matching `.h` file).
  - The user has given **explicit instructions** on what to modify.

**Rule #3**: **Restricted Configuration Files**
- Do **NOT** edit the following files unless the user provides **explicit confirmation**:
  - `myoptions.h`
  - `src/core/options.h`
  - `platformio.ini`

**Rule #4**: **Code Interaction Documentation**
- If your change affects **code interactions**, external APIs, storage keys, or external contracts:
  - Ensure `.github/code-summary.md` is updated in the **same change set**.
- **Bug fixes** do not require updates unless they modify external contracts.

**Rule #5**: **Pre-edit Research Required**
- For any non-trivial change (beyond an isolated one-line fix):
  - You **MUST READ** `.github/code-summary.md` before writing any code.
  - If the area you are editing touches multiple files or is flagged as risky in `.github/code-summary.md`:
    - Read the per-file sections in `.github/code-summary.md`.
    - Check for related open issues in `.github/code-issues.md`.
    - Do not proceed without completing this review.

**Enforcement and AI Behavior**
- Always validate proposed changes against these rules.
- For Rule #1: Explain when Plan mode is required and wait for user confirmation.
- Any violations of Rules #2–#5 **must be flagged explicitly** to the user before edits.

## Project Structure
- **Config Cascade**: `platformio.ini` (env #define) → `myoptions.h` (hardware profile, user defaults) → `mytheme.h` (UI theme) → `options.h` (fallback defaults for anything undefined) → `optionschecker.h` (forces compile failure when certain defines are incorrect).
- **Core logic**: `src/core/` (Player, Display, Network, Config, Controls).
- **Libraries path**: Software codecs: `libraries/I2S_Audio/`, `libraries/ES8311_Audio` / Hardware decoder: `libraries/VS1053_Audio/` (Hardware chip), other folders are custom drivers for other display, touchscreen, and other hardware.
- **UI**: Widgets in `src/displays/widgets/`, drivers in `src/displays/`.
- **Plugins**: The former yoRadio plugin hook system has been removed. Add new behavior in the owning core module instead of reviving plugin-style hooks.
- **Web UI**: Most files in `data/www` are served with headers in `src/core/netserver.h`. `search.html` and `curated.html` are not.

## Functionality
- **Hardware**: The firmware is built according to the hardware that is connected to it and users who will use it.  These are defined by files listed in Config Cascade.
- **Software**: `src/core/options.h` and the Config Cascade should be used to extend functionality, not limit it. `#if defined` and `#ifndef` should not be used in the code for configuration not related to hardware.
- **Granular Control in Web UI**: If not hardware-related, functionality should be changeable in the Web UI, not controlled by a `#define` in Config Cascade.
