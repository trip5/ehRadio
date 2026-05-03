## Custom Libraries

The following libraries are custom to ehRadio, mostly inherited from yoRadio v0.9.533.  They cannot be easily replaced with an external library.

### Display Drivers

#### Adafruit GC9106 https://github.com/prenticedavid/Adafruit_GC9102_kbv
  - not on Platformio (and not actually an Adafruit library)

#### Adafruit ST7796S https://github.com/prenticedavid/Adafruit_ST7796S_kbv
  - not on Platformio (and not actually an Adafruit library)
  
#### FT6336_Touchscreen
  - made by https://github.com/kasperaitis for ehRadio
  - used as source: https://github.com/aselectroworks/Arduino-FT6336U

#### ILI9225Fix https://github.com/arduinopavlodar/TFT_22_ILI9225
  - not on Platformio and also highly-modified from an unknown version

#### ILI9488 https://github.com/ZinggJM/ILI9486_SPI
  - highly-modified from version 1.0.5?

#### LiquidCrystalI2C https://github.com/johnrickman/LiquidCrystal_I2C
  - slightly-modified from version 1.1.3

#### SSD1322 https://github.com/JamesHagerman/Jamis_SSD1322
  - slightly-modified from initial commit

#### ST7920 https://github.com/BornaBiro/ST7920_GFX_Library
  - very similar or modified (or perhaps share a common source)
  - may be worth looking at as well: https://github.com/BornaBiro/ST7920_GFX_Library

### Audio Decoder Drivers

#### ES8311_Audio
  - made by https://github.com/kasperaitis for ehRadio

#### I2S_Audio
  - likely adapted https://github.com/schreibfaul1/ESP32-audioI2S
  - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
    - Maleksm says source from Wolle (schreibfaul1) 3.3.2l on 2025.07.09
    - But source mostly matches 3.1.0 - so perhaps there's a few minor fixes from 3.3.2 or...?
  - attempt to update using Maleksm's mod v0.9.533m failed (probably due to similar issues as below)

#### VS1053_Audio
  - original DNA in https://github.com/Edzelf/Esp-radio and https://github.com/Edzelf/ESP32Radio-V2
  - but then replaced with https://github.com/schreibfaul1/ESP32-vs1053_ext
  - likely adapted from https://github.com/nstepanets/ESP32-vs1053_ext
  - from Maleksm's yoRadio mod v0.9.512m: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228
    - Maleksm says source from Wolle (schreibfaul1) 3.0.13t on 2024.11.16
  - attempt to update using Maleksm's mod v0.9.533m failed (probably due to similar issues)

#### Further notes

See also this issue: https://github.com/trip5/ehRadio/issues/39

Attempts were made to full upgrade to more recent versions from https://github.com/schreibfaul1/ESP32-audioI2S and https://github.com/nstepanets/ESP32-vs1053_ext

But these lines will be required in `platformio.ini`:

```
[env]
platform = espressif32@6.10.0 ; needed to support GCC 11.2.0
platform_packages = 
  ; this allows gnu++20 (for span function in I2C_Audio)
  toolchain-xtensa-esp32 @ ~11.2.0 ; ESP32 (original)
  toolchain-xtensa-esp32s2 @ ~11.2.0 ; ESP32-S2
  toolchain-xtensa-esp32s3 @ ~11.2.0 ; ESP32-S3
  toolchain-riscv32-esp @ ~11.2.0 ; ESP32-C3/C6/H2 (RISC-V)

build_unflags = 
  -std=gnu++11
  -std=gnu++14
  -std=gnu++17
build_flags =
  -std=gnu++20
```


A modification may be needed to `Audio.h`:
```
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/i2s_std.h>
#else
#include <driver/i2s.h>
#endif
```

Also, it looks like:
```
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
```
Will all need to be replaced with:
```
#include <NetworkClient.h>
#include <NetworkClientSecure.h>
```
Because new Arduino-ESP32 v3.x uses unified network API. ehRadio still uses the old v2.x API.

That would likely be a very large re-factor.

---

## Trip5's Thoughts

It seems to me most of this codebase was actually developed for Arduino IDE by hand and if it compiled, it was left alone until functionality was broke and it had to be fixed...

This file was my own hand-made hand-googled investigation of the source of the libraries. Actually there used to be dozens more for displays but they were literally copies of libraries available in Platformio online and in some cases, remained unchanged (display drivers particularly don't really get updated often), in others updated and much better than when they had been left in the libraries folder (like encoder and onebutton) 2-5 years ago...

But the audio decoding libraries are troublesome.  They were so modified from their original source that they are nearly impossible to update, even by hand, except by the very skilled maleksm, who managed to merge bits from updated libraries into the yoradio libraries to expand their capabilities.

The reason for this is probably because when the audio libraries were first added to yoradio, they were almost immediately modified to interact directly with other parts of the code, instead of modifying radio routines to interact with it differently.  In fact, that may have actually been the original-original dev who likely wasn't making the libraries to be standalone but rather was using them in his own code... that was Edzelf and likely e2002 who made yoradio just copied the source files from esp32radio-v2 to make his own... probably this went on for years.  edzelf updated his aaudio code... e2002 studied it and merged it into yoradio... repeat.

Finally, schreibfaul wanted to make a radio too but this time thought he would make a library-first repository, basing his audio libraries on edzelf's code.... and building his radio code on the library... updating one didn't always mean the other got updated but at least the libraries were separate so... more flexible for people to work with.

Does this make sense so far?  It seems like a common scenario, possible because 5-8 years ago, online libraries were less common than they are now... people shared ZIP files on Github but that was about it.

Anyways, that's my forensic analysis...

---

## Below is Copilot's Analysis How To Update the Audio Libraries

### Executive Verdict

- Your general theory is mostly correct.
- The current ehRadio audio folders are not plain upstream libraries anymore. They already act as a hand-built compatibility layer so the app can treat very different decoder backends as one local `Audio` API.
- Because of that, the real problem is not just "find newer library versions". The real problem is "move the ehRadio compatibility layer out of the vendored libraries, then update the backends underneath it".
- A direct drop-in jump to current upstream `ESP32-audioI2S` master is not realistic.
- The most practical near-term targets are upstream `ESP32-audioI2S` 3.x for the I2S path and `nstepanets/ESP32-vs1053_ext` for the VS1053 path.

### What I Checked

- ehRadio local files:
  - `src/core/player.h`
  - `src/core/player.cpp`
  - `src/core/audiohandlers.h`
  - `src/libraries/I2S_Audio/Audio.h`
  - `src/libraries/VS1053_Audio/audioVS1053Ex.h`
  - `src/libraries/ES8311_Audio/es8311.h`
  - `src/libraries/ES8311_Audio/es8311.cpp`
- Upstream I2S library snapshots:
  - `schreibfaul1/ESP32-audioI2S` tag `3.1.0`
  - `schreibfaul1/ESP32-audioI2S` tag `3.3.2`
  - current `schreibfaul1/ESP32-audioI2S` master
- Upstream VS1053 libraries:
  - current `schreibfaul1/ESP32-vs1053_ext` master
  - current `nstepanets/ESP32-vs1053_ext` master
- Upstream ES8311 helper:
  - `schreibfaul1/ESP32-audioI2S/examples/ES8311`

### The Main Forensic Finding

The real local asset is not the vendored decoder source itself. The real local asset is the compatibility surface that ehRadio built around it.

Right now:

- `src/core/player.h` includes either `I2S_Audio/Audio.h` or `VS1053_Audio/audioVS1053Ex.h`.
- `Player` inherits directly from that chosen class.
- `src/core/audiohandlers.h` expects a normalized set of generic `audio_*` callbacks.
- The app assumes both backends expose one shared contract: play, stop, connect, seek, metadata callbacks, EOF callbacks, bitrate callbacks, and some shared state like `eofHeader`.

That means ehRadio is not consuming upstream libraries directly anymore. It is consuming its own locally-shaped API.

### What That Means For Your Theory

I would summarize your theory like this:

- historical origin: yes, mostly confirmed
- present architecture: more complicated than a straight VS1053-first or I2S-first story

More specifically:

- Historically, the codebase still reads like it grew up around the hardware decoder path first. The `Player` constructor and `Player::init()` still branch around VS1053-specific SPI startup when I2S is absent.
- But at the code-contract level, ehRadio already moved toward a generic `Audio` API that hides whether the backend is software I2S or hardware VS1053.
- Upstream today is definitely not symmetric anymore. `ESP32-audioI2S` is the fast-moving backbone project. VS1053 lives in a separate lineage with a different class name and callback style.

So the short answer is: your theory is mostly right, but ehRadio already built a custom bridge between those two worlds, and that bridge is now the hard part.

### I2S_Audio Findings

#### 1. ehRadio local I2S is closer to upstream 3.1.x / 3.3.x than to current master

The local `src/libraries/I2S_Audio/Audio.h` banner says:

- `Version 3.1.0n`
- updated Feb 27 2025 and Apr 04 2025 by Maleksm

That matches your suspicion that this is not current upstream, but it also shows something important: the local code is not ancient. It is a 3.1-era codebase with later hand-merges.

Upstream evidence:

- tag `3.1.0` still uses the old weak `audio_*` callback style and a public `Audio` class very similar to what ehRadio expects
- tag `3.3.2` is still recognizably the same family
- current master is a much larger redesign

#### 2. Current upstream `ESP32-audioI2S` master is not a drop-in upgrade

Current master changed in ways that matter to ehRadio:

- callback model moved toward `Audio::audio_info_callback` with `msg_t` events instead of only weak `audio_*` functions
- transport now uses `NetworkClient` / `NetworkClientSecure` directly
- internals use newer C++ features and ownership patterns like `std::function`, `std::unique_ptr`, `std::deque`, `std::span`, and PSRAM helper wrappers
- README now explicitly says current master expects a multi-core chip and PSRAM
- the decoder architecture is more library-owned and less compatible with ehRadio's old assumptions

That is not a normal "merge upstream" update. That is a backend rewrite from ehRadio's point of view.

#### 3. Two blockers you listed are real, but one is already partly solved

Your note above mentioned:

- `driver/i2s_std.h` vs `driver/i2s.h`
- `WiFiClient` vs `NetworkClient`

What I found:

- local ehRadio `I2S_Audio/Audio.h` already has the IDF 5 `i2s_std` include gate
- upstream `ESP32-audioI2S` tag `3.1.0` already had partial Arduino 3 compatibility logic for `NetworkClient`
- current master goes much further and is clearly Arduino-ESP32-v3-oriented

So the real blocker is not just missing include swaps. The real blocker is that the library's public model changed while ehRadio stayed tied to the older contract.

### VS1053_Audio Findings

#### 1. ehRadio local VS1053 is more customized than the I2S side

This is the strongest evidence for your "the app and library were fused together" theory.

Upstream VS1053 expects:

- class name `VS1053`
- callbacks named `vs1053_*`
- a more hardware-decoder-specific API surface

ehRadio local VS1053 does not look like that anymore. It was reshaped so it behaves much more like the I2S `Audio` class.

Local changes include:

- class renamed / exposed as `Audio`
- generic `audio_*` callbacks instead of upstream `vs1053_*`
- method surface aligned with the I2S path so `Player` can compile against either backend with the same code

That is not a passive vendor copy. That is a local shim baked into the vendored library.

#### 2. If VS1053 is updated, use the nstepanets fork as the upstream base

Between the two current upstream lines:

- `schreibfaul1/ESP32-vs1053_ext` is the original public branch
- `nstepanets/ESP32-vs1053_ext` is the more relevant modern fork for ehRadio-style maintenance

Why I say that:

- it is closer to the feature set ehRadio already expects
- it carries the more current community maintenance energy for the VS1053 side
- your own research already pointed in that direction, and the local code shape supports that conclusion

So if VS1053 is rebased, `nstepanets` is the right starting point, not a jump back to the older original repo.

### ES8311 Findings

Strictly speaking, the thing to update is not an "I2C audio library". There are really two separate pieces on the software-decoder side:

- `I2S_Audio` = stream transport + decoder + I2S output
- `ES8311_Audio` = small codec-control helper over I2C

That distinction matters.

The good news: local `src/libraries/ES8311_Audio` matches the upstream `ESP32-audioI2S/examples/ES8311` helper very closely. It does not look like a one-off Copilot invention from random internet code. It looks like a lightly adapted copy of the upstream example helper.

That makes ES8311 the least scary part of the whole migration.

My conclusion on ES8311:

- it can be refreshed independently
- it can also safely stay vendored during the first migration stages
- it is not the main blocker

### The Current ehRadio Audio Contract

This is the contract that any future shim must preserve before the backends can be swapped safely.

The app currently expects at least these things:

- constructors usable by `Player` for either VS1053 or I2S setup
- `begin()` for the VS1053 path
- `setPinout()` for the I2S path
- `setBalance()`
- `setTone()`
- `setVolume()`
- `forceMono()`
- `setConnectionTimeout()`
- `getFilePos()`
- `stopSong()`
- `setDefaults()`
- `connecttoFS()`
- `connecttohost()`
- `loop()`
- `isRunning()`
- public `eofHeader`

And the app also expects normalized callbacks/events such as:

- `audio_info`
- `audio_bitrate`
- `audio_showstation`
- `audio_showstreamtitle`
- `audio_error`
- `audio_id3artist`
- `audio_id3album`
- `audio_id3title`
- `audio_eof_mp3`
- `audio_eof_stream`

That contract is the thing to preserve. Not the exact local vendor file layout.

### Recommended Migration Strategy

#### Phase 1: Freeze the contract and move the shim into ehRadio-owned code

This is the key step.

Do not keep making upstream libraries pretend to be ehRadio core classes.

Instead:

- stop treating `Player` as `class Player : public Audio`
- move toward composition instead of inheritance
- create ehRadio-owned wrappers such as:
  - one wrapper for the I2S backend
  - one wrapper for the VS1053 backend
- put callback translation in those wrappers

The wrappers should translate:

- upstream I2S event/callback model into the existing ehRadio `audio_*` behavior
- upstream VS1053 `vs1053_*` behavior into the same normalized ehRadio behavior

Once that exists, ehRadio owns the contract again instead of depending on whichever upstream header it vendored last year.

#### Where the shim should live

This was clarified further after the initial investigation.

- The shim should be ehRadio-owned code, not a rewrite baked back into the vendored libraries.
- The shim should not be merged into `audiohandlers.h`.
- `audiohandlers.h` is better thought of as the current app-side event sink: metadata, EOF, bitrate, and title updates get turned into `config`, `display`, `netserver`, and `player` behavior there.

The cleaner target architecture is:

- `Player` talks to a small backend interface.
- One adapter implements that interface for the I2S backend.
- One adapter implements that interface for the VS1053 backend.
- Those adapters normalize backend-specific callbacks/events into one app-facing event model.
- The existing logic in `audiohandlers.h` consumes those normalized events during the transition period.

So the relationship should be: the shim feeds the `audiohandlers.h` behavior, not that the shim gets integrated into `audiohandlers.h` itself.

Long term, the `audiohandlers.h` logic would probably be better moved into a real `.cpp/.h` pair or another explicit event-consumer module, but that can happen after the adapter boundary is working.

#### How to use the shim for testing

The first use of the shim should be with the current vendored libraries.

That is not because the current forks are the final answer. It is because wrapping the current backends first gives a stable baseline:

1. prove the new interface is sufficient
2. prove `Player` can stop inheriting directly from library classes
3. confirm that metadata, EOF, resume, bitrate, and volume behavior remain unchanged
4. only then swap the adapter implementation underneath to upstream `3.3.2`
5. after that is stable, evaluate the jump from `3.3.2` to current-era `ESP32-audioI2S`

#### Phase 2: Update the I2S side first, but not straight to current master

For the I2S backend, I would not jump directly from the local 3.1-era hybrid to current master.

Safer path:

1. diff local ehRadio I2S against upstream `3.1.0`
2. diff that against upstream `3.3.2`
3. merge only the changes that help while keeping the wrapper stable
4. only after that, evaluate whether current master is worth the next jump

Why:

- `3.3.2` is still in the same family as the local code
- current master is effectively a different generation
- smaller jumps make it easier to tell whether a regression comes from ehRadio glue code or from the decoder itself

#### Phase 3: Rebase VS1053 separately

For VS1053, the likely end state is:

- upstream `nstepanets/ESP32-vs1053_ext` remains a real `VS1053` backend
- ehRadio wrapper translates it into the app contract

Do not keep solving this by renaming upstream internals until they look like `Audio`. That is exactly how the de-fork problem got worse over time.

### What I Would Not Do

I would not do all of this in one move:

- update PlatformIO / toolchains
- switch to Arduino-ESP32 v3-only assumptions
- replace both audio backends
- rewrite callback handling
- rewrite `Player`

That is too many unknowns in one step.

### Practical Order Of Work

If the goal is "make this actually achievable", I would do it in this order:

1. document the current app-level audio contract
2. convert `Player` from inheritance to composition
3. build wrappers around the current vendored libraries first
4. swap the I2S wrapper underneath to upstream `3.3.2`-era code
5. swap the VS1053 wrapper underneath to `nstepanets/ESP32-vs1053_ext`
6. refresh `ES8311_Audio` if needed
7. only then decide whether current `ESP32-audioI2S` master is worth the next jump

### Likely Breakpoints During Migration

The places most likely to break are:

- metadata/title/station callbacks
- bitrate reporting
- SD resume / seek behavior
- EOF handling and auto-next logic
- volume mapping between:
  - I2S software volume
  - VS1053 hardware volume
  - ES8311 codec volume
- HLS / M3U8 behavior
- builds without generous PSRAM assumptions

### Bottom Line

If you want the shortest honest answer:

- yes, your theory mostly holds up
- no, this is not a direct library-replacement task
- yes, it is still solvable

But the solution is not "replace the folders with newer upstream folders".

The solution is:

1. preserve the current ehRadio audio contract in app-owned shims
2. rebase the backends underneath those shims in smaller steps

If you want the lowest-risk first target, aim for:

- `ESP32-audioI2S` 3.3.x lineage first
- `nstepanets/ESP32-vs1053_ext` for the VS1053 lineage

That is the path that best matches what ehRadio actually is today.

