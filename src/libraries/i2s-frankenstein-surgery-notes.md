# I2S Frankenstein Surgery Notes

## Read This With the Other Library's Notes

The two audio libraries share their defect classes, and the dates here and in
`vs1053-frankenstein-surgery-notes.md` show the same faults surfacing on both: `2026.10.11` (the m3u8 refetch pause, the
media-sequence resync, the `TIT2` mis-wire), `2026-10-21` (the screensaver's VU gate) and `2026.10.24` (the per-track
resets and the ID3v2 frame-size guard). Two practices follow:

* **A fix in one library is a question about the other.** Check the counterpart before closing the issue, and record
  whether it needed the same change or was already correct.
* **Expect the VS1053 board to find these first - and this library to be the reference for some of them.** The VS1053
  library has a hardware decoder, a DREQ-gated SPI feed and the shallower graft, so end-of-stream accounting faults show
  up there as audible stalls with readable counters, and that file carries the longer record (fixes from 2026-07 on,
  against 2026-08 here). Working the VS1053 side patiently has produced the diagnosis every time. This library has then
  either needed the same fix, or turned out to hold the correct version: the three per-track resets lost from the VS1053
  graft (`m_sumBytesDecoded`, `m_audioDataStart`, `m_audioDataSize`) were present and correct here, and comparing
  against them is what identified the loss. The reverse also happens - the tail-of-file fix recorded below, dated
  `2026.10.24`, ended with a prediction that a VS1053 unit stopping at a track end would be the first place to look, and
  that is exactly what subsequently happened.

## Purpose
Graft ehRadio's adaptations from the current I2S library (based on Maleksm v0.9.434m / I2S 3.1.0) onto Maleksm's latest v0.9.720m (I2S 3.4.6w).

## Libraries

| Name | Folder | Version | Notes |
|------|--------|---------|-------|
| **Active** | `src/libraries/I2S_Audio/` | 3.1.0n + Maleksm v0.9.434m | Current ehRadio library — **the only copy of our own code** |
| **Maleksm v0.9.434m** | `I2S_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` | 3.1.0n | Base of Active library |
| **Maleksm v0.9.720m** | `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))` | 3.4.6w | **GRAFT TARGET** — latest Maleksm |

Two folders were removed on 2026-09-27. `I2S_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` was labelled
"Active backup" but was a snapshot of `I2S_Audio/` that stopped being accurate as soon as the library was
patched, and `I2S_Audio (yoRadio Maleksm v0.9.512m(13.07.25))` was an intermediate superseded by
v0.9.720m. **Git history is the backup for our own code**; folder copies of it are a trap, because
restoring one silently drops every fix made since it was taken. The upstream folders stay: they are the
only local copies of code that is not ours, and the graft plan in this file refers to v0.9.720m by name.

## Trust Order
Active > Maleksm v0.9.720m (latest)

---

## Active vs Maleksm v0.9.434m: ehRadio Adaptations (Minimal!)

The I2S library has far fewer ehRadio changes than VS1053:

| Change | Active | Maleksm | Why |
|--------|--------|---------|-----|
| Include path | `../../core/options.h` | `../core/options.h` | ehRadio deeper dir |
| Include guard | `#if VS1053_CS==255` | `#if VS1053_CS==255` | Same! (VS1053_CS==255 means "no VS1053, use I2S") |
| Header comment | "Some adjustments by Trip5" added | — | Attribution |
| Config include | `../../core/config.h` | `../core/config.h` | Same reason |

The include-path and attribution rows above were the only differences from Maleksm **as it was when the
library was adopted**. Since then ehRadio has added four things of its own, recorded here so that a
future graft does not quietly lose them:

| Addition | Where | Why |
|----------|-------|-----|
| VU capture window | `Audio.h` / `Audio.cpp`: `_capWin`, `_capPub`, `_capPos`, `_capSeq`, `_capturePublish()`, `getWaveform()`, and a self-contained radix-2 `vuFft()` | The VU widget's waveform, Lissajous and real-spectrum styles need PCM that the level meter does not provide. The window is `VU_CAPTURE_SAMPLES` (512), appended in `computeVUlevel()` and published on the `f_vu` tick behind a sequence counter, so a torn read is dropped rather than drawn. |
| PSRAM scratch for the transform | `vuScratchAlloc()` plus `vuPsramBytes` (declared in `core/config.h`) | The transform's working set is 8 KB and taking it from the internal heap during boot was enough to tip the device over - the boot services hold three TLS sessions at the same time. It is allocated lazily, which is why the core monitor's `VU FFT` field reads 0 until a spectrum frame has been drawn. |
| Separate connect timeout | `m_connectTimeout_ms` / `m_connectTimeout_ms_ssl` (1200 ms), used at the four `connect()` sites in `connecttohost()` and the mid-stream reconnects | `connecttohost()` runs on the MAIN task, so its timeout is a UI freeze - no button sampled, no WebUI request answered - and not merely a slow reconnect. It must stay separate from `m_timeout_ms` / `m_timeout_ms_ssl`, which double as the socket READ timeouts and have to remain patient for a slow stream to trickle in. Merging them re-breaks slow streams. |

| Fast-attack / slow-fall hold | `getSpectrum()`: a static `hold[]` per band with `holdN`, `holdMs`, `holdValid`, and the dropped-frame branch | Two reasons, both measured on hardware. A single 512-sample window is a noisy estimate, so a band keeps the highest value it has seen and falls at `VU_SPECTRUM_FALL_MS` - the same shape `VU_FADE_MS` gives the level bars. And the seqlock drops a frame whenever the publisher moves during the staging copy (7% of frames at 30 Hz), which used to be reported as `false`; a painter reads `false` as "this backend has no transform" and switches to its simulated source, which is exactly what the visible flicker was. A dropped frame now repeats the hold, and only "never computed" answers `false`. |

---

## What Changed Across Maleksm Versions

### v0.9.434m → v0.9.512m (3.1.0n → 3.3.2p)
- **New file**: `psram_unique_ptr.hpp` (3442 lines) — PSRAM smart pointer framework (ps_make_unique, ps_vector, ps_string, etc.)
- **Decoder expansions**: aac_decoder/libfaad/ gets structs.h, tables.h, aac_defines.h, aac_settings.h, aac_structs.h, aac_tables.h
- **Decoder expansions**: opus_decoder/ gets celt_defines.h, celt_structs.h, celt_tables.h, range_decoder.cpp/.h, silk_defines.h, silk_structs.h, silk_tables.h
- **Decoder expansions**: mp3_decoder/ gets structs.h, tables.h
- Header grows from 801 to 959 lines

### v0.9.512m → v0.9.720m (3.3.2p → 3.4.6w)
- **New file**: `audiolib_structs.hpp` (444 lines) — shared structs: sylt_t, ID3Hdr_t, various M4A structs, FLAC structs, APIC structs
- **New decoder**: `wav_decoder/` (wav_decoder.cpp/.h) — WAV file support added
- `#include "audiolib_structs.hpp"` and `#include "esp_dsp.h"` added
- **New function**: `calculateVolumeLimits()` replaces static volume calculation
- Task infrastructure refactored: `AUDIO_LOG_*` macros replace `log_i`/`log_e`
- `startAudioTask()` called from `I2Sstart()` not constructor/begin()
- **Decoder abstraction**: `Decoder` base class with virtual methods
- **VU meter replaced**: `calculateVUlevel(c)` + `gain_ramp()` instead of `computeVUlevel()`/`get_VUlevel()`

---

## Critical Architecture Changes in v0.9.720m

### 1. Decoder Abstraction (NEW)
```cpp
class Decoder {
  public:
    virtual bool     init() = 0;
    virtual void     clear() = 0;
    virtual int32_t  decode(uint8_t* inbuf, int32_t* bytesLeft, int32_t* outbuf1) = 0;
    // ... more virtual methods
  protected:
    Decoder(Audio& audioRef) : audio(audioRef) {}
    Audio& audio;
};
```
All decoders (MP3, AAC, FLAC, etc.) now inherit from this base class. The `Audio` class uses `m_decoder` pointer.

### 2. performAudioTask() Completely Redesigned
```cpp
void Audio::performAudioTask() {
    if (m_decoder) {
        xSemaphoreTake(mutex_audioTask, ...);
        while (m_validSamples) { vTaskDelay(20); playChunk(); }
        playAudioData();
        xSemaphoreGive(mutex_audioTask);
        gain_ramp();
    } else {
        int32_t c[2] = {0};
        calculateVUlevel(c);
        gain_ramp();
        vTaskDelay(20);
    }
}
```
- `playChunk()` — new function, feeds I2S DMA from decoded samples
- `gain_ramp()` — new function, smooth volume transitions
- When no decoder active: runs VU meter idle loop

### 3. VU Meter Completely Replaced
- `computeVUlevel()` / `get_VUlevel()` — GONE
- `calculateVUlevel(int32_t c[2])` — new function, takes sample buffer
- `gain_ramp()` — new function, handles volume ramping
- `VolumeCurveFn` — function pointer type for custom volume curves (commented out)
- **⚠️ VU meter in v0.9.720m may use 0-100 scale — verify before grafting**

### 4. PSRAM Is Mandatory
- No fallback to SRAM in v0.9.720m
- `psram_unique_ptr.hpp` provides the allocation framework
- `audiolib_structs.hpp` depends on it

### 5. Task / Core Assignment
- `m_audioTaskCoreId = 1` (default)
- `setAudioTaskCore(uint8_t coreID)` — same pattern, stops/restarts task
- `startAudioTask()` uses `xTaskCreateStaticPinnedToCore(..., m_audioTaskCoreId)`
- **No `AUDIO_CORE` macro used** — needs to be wired to options.h AUDIO_CORE
- `AUDIO_STACK_SIZE` raised to **3500** words (was 3300) — see Addendum 2 for the stream that overflowed it
- Task starts from `I2Sstart()` method, not constructor

### 6. New Logging System
```cpp
#define AUDIO_LOG_ERROR(fmt, ...) AUDIO_LOG_IMPL(1, ...)
#define AUDIO_LOG_WARN(fmt, ...)  AUDIO_LOG_IMPL(2, ...)
#define AUDIO_LOG_INFO(fmt, ...)  AUDIO_LOG_IMPL(3, ...)
#define AUDIO_LOG_DEBUG(fmt, ...) AUDIO_LOG_IMPL(4, ...)
```
Replaces old `AUDIO_INFO`/`AUDIO_ERROR` macros and `log_i`/`log_e`. May need to integrate with ehRadio's `logging.h`.

### 7. New/Changed Functions to Watch
- `calculateVolumeLimits()` — called after I2S init
- `gain_ramp()` — smooth volume transitions
- `playChunk()` — I2S DMA feeding from decoded samples
- `calculateVUlevel(int32_t c[2])` — new VU meter
- `I2Sstop()` / `I2Sstart()` / `I2Srestart()` — new I2S management
- `m_f_I2S_init` flag — guards audio task execution

---

## User Decisions
1. **PSRAM mandatory** — Accepted. No SRAM fallback needed.
2. **Logging system** — Not critical at this stage. v0.9.720m's `AUDIO_LOG_*` macros (using `AUDIO_LOG_IMPL` with file/line/function) will be dealt with later. May conflict with ehRadio's `logging.h`.
3. **v0.9.535m deleted** — Going straight to v0.9.720m. The intermediate stepping stones are not needed for this graft.

## Graft Plan — Two Stages

### Why Staged
The jump from v0.9.434m to v0.9.720m is too large for one hop. v0.9.512m is structurally similar to v0.9.434m (same task, same VU meter API) but introduces PSRAM framework. Verify Stage 1 works before tackling Stage 2's Decoder abstraction and new VU meter.

Additionally, v0.9.720 I2S is a prerequisite for the v0.9.720 VS1053 library — they share the same audio core.

---

### STAGE 1 — v0.9.512m (Low Risk)

**Target**: Maleksm v0.9.512m (I2S 3.3.2p)
**What changes**: `psram_unique_ptr.hpp`, expanded decoder files, PSRAM allocation
**What stays the same**: Task structure, VU meter API (`computeVUlevel`/`get_VUlevel`), I2S DMA feeding

#### Stage 1 Steps
1. **Copy v0.9.512m files**: All files from `I2S_Audio (yoRadio Maleksm v0.9.512m(13.07.25))` into `src/libraries/I2S_Audio/`. Includes `psram_unique_ptr.hpp` and expanded decoder subfolders.
2. **Apply ehRadio adaptations**:
   - Fix include paths: `../../core/` (not `../core/`)
   - Add `#include "../../core/config.h"` if needed
   - Wire `m_audioTaskCoreId` to `AUDIO_CORE` from options.h
   - Add "Some adjustments by Trip5 for ehRadio" to header
3. **Verify PSRAM allocation**: Ensure `psram_unique_ptr.hpp` allocates correctly
4. **Build and test**: All I2S environments. Verify audio playback, VU meter, volume control.
5. **If Stage 1 works**: PSRAM framework confirmed. Proceed to Stage 2.
6. **If Stage 1 fails**: Problem is in PSRAM or decoder file changes. Fix before proceeding.

---

### Stage 1 Result: BLOCKED — Toolchain Incompatibility (2026-06-28)

Stage 1 was attempted with the v0.9.512m library copied into the active folder. The build failed with three categories of errors:

#### Error 1: C++20 Standard Required
`psram_unique_ptr.hpp` uses C++20 features: `requires` keyword (concepts) and `inline` variables. The project compiles with C++17 at best. Adding `-std=gnu++2a -fconcepts` to `platformio.ini` resolved the language-level errors.

#### Error 2: `<span>` Header Not Available (Fatal)
The v0.9.720m `psram_unique_ptr.hpp` (used as replacement for missing methods) includes `#include <span>` which is a C++20 standard library header. GCC 8.4 (bundled with `framework-arduinoespressif32 @ 3.20017`) does NOT ship `<span>` — it was introduced in GCC 10. This is a **toolchain-level** blocker.

#### Error 3: GCC 8.4 Internal Compiler Error
With `-std=gnu++2a -fconcepts`, GCC 8.4 crashed with `internal compiler error: in type_unification_real` on C++20 range-for syntax in `utility.cpp`. The compiler is not stable with C++20 constructs.

#### Root Cause Summary

| Requirement | v0.9.512m+ Needs | Project Has | Status |
|-------------|-----------------|-------------|--------|
| C++ standard | C++20 (`requires`, `inline` vars) | C++17 at best | ❌ |
| `<span>` header | GCC 10+ | GCC 8.4 | ❌ |
| `-std=gnu++20` flag | GCC 11+ | GCC 8.4 (only `-std=gnu++2a`) | ❌ |

Maleksm v0.9.512m+ was developed for ESP-IDF v5.4+ which bundles GCC 13+. The project's Arduino 3.0 framework bundles GCC 8.4. To use Maleksm's newer I2S libraries, the project would need to upgrade to Arduino 3.1+ or direct ESP-IDF v5.3+.

#### Conclusion
The v0.9.434m I2S library is the **maximum compatible version** for the current toolchain. Both Stage 1 (v0.9.512m) and Stage 2 (v0.9.720m) are blocked until a toolchain upgrade. The active library was restored from its backup copy and builds successfully — that copy has since been removed (see the note under the library table), so the library that ships is `src/libraries/I2S_Audio/` and its git history is the record.

---

### STAGE 2 — v0.9.720m (High Risk)

**Target**: Maleksm v0.9.720m (I2S 3.4.6w)
**What changes**: `Decoder` abstraction, `audiolib_structs.hpp`, new VU meter, `wav_decoder/`, new logging, `gain_ramp()`, `playChunk()`
**PREREQUISITE**: Stage 1 must be working

#### Stage 2 Steps
1. **Copy v0.9.720m files**: All files from `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))` into `src/libraries/I2S_Audio/`. This adds `audiolib_structs.hpp`, `wav_decoder/`, and overwrites Audio.cpp/Audio.h with Decoder abstraction.
2. **Re-apply ehRadio adaptations** (same as Stage 1 Step 2)
3. **Wire `AUDIO_CORE`**: Ensure `m_audioTaskCoreId` uses `AUDIO_CORE`
4. **Adapt `audiohandlers.cpp`**:
   - New `calculateVUlevel(int32_t c[2])` signature — takes sample buffer, returns via reference
   - New `gain_ramp()` may affect volume transitions
   - `Decoder` base class — verify our code doesn't call removed methods
   - Any new callback events from Maleksm
5. **VU meter verification**:
   - Check scale of `calculateVUlevel()` (may be 0-100 range)
   - Update display mapping if needed
   - Verify `gain_ramp()` doesn't interfere with our volume control
6. **Logging** (deal with later if needed):
   - v0.9.720m uses `AUDIO_LOG_*` macros with `AUDIO_LOG_IMPL`
   - May conflict with ehRadio's `logging.h` — resolve after audio works
7. **Build and test**: All I2S environments. Audio, VU meter, volume, core assignment.

---

## Open Questions / Concerns (Stage 2)

1. **VU meter scale**: v0.9.720m's `calculateVUlevel()` may use different scale than Active's `computeVUlevel()`. Need to verify range.

2. **`AUDIO_CORE` integration**: `m_audioTaskCoreId` defaults to 1. Must be wired to `AUDIO_CORE` from options.h in the constructor or `I2Sstart()`.

3. **Logging conflict**: v0.9.720m has its own `AUDIO_LOG_*` system. May conflict with ehRadio's `logging.h` macros. Low priority — resolve after audio works.

4. **Decoder abstraction**: The `Decoder` base class is new. Our audio handler (`audiohandlers.cpp`) may call decoder methods directly — if the API changed, adaptations needed.

5. **`esp_dsp.h` dependency**: v0.9.720m includes `esp_dsp.h`. Verify this ESP-IDF component is available.

6. **`gain_ramp()` thread safety**: Called from performAudioTask() and possibly from setVolume(). Check for mutex protection.

7. **v0.9.720 VS1053 dependency**: The v0.9.720 VS1053 library shares the I2S audio core. Getting I2S to v0.9.720 is a prerequisite for future VS1053 graft.

---

## Notes for Future Use

`Active` = `src/libraries/I2S_Audio/` — current ehRadio I2S library (Maleksm v0.9.434m base)

`Maleksm v0.9.434m` = `I2S_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` — original Maleksm I2S 3.1.0n

`Maleksm v0.9.720m` = `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))` — latest Maleksm I2S 3.4.6w, Stage 2 graft target

Graft result = `src/libraries/I2S_Audio/` — after surgery

Backups: none by folder, for the reason under the library table. Our own code is backed up by git; the
upstream folders are kept because they are the only local copies of code that is not ours.

---

## Post-Graft Patch Fixes (2026-08-10)

Fixed AudioBuffer::bytesWritten() at Audio.cpp:142: m_writePtr == m_endPtr → >= with overflow wrapping (same pattern as bytesWasRead at line 150). Prevents write pointer from overshooting buffer and corrupting adjacent PSRAM.

---

## Post-Graft Patch Fixes (2026-09-28)

`AUDIO_STACK_SIZE` went from **3300 to 3500 words** (13.2 KB → 14 KB) because one station killed the firmware
in the middle of decoding it: `streams.echoesofbluemars.org:8000/cryosleep`, which the decoder reports as
`MPEG-2.5, Layer I`.

```
Guru Meditation Error: Core  0 panic'ed (Unhandled debug exception).
Debug exception reason: Stack canary watchpoint triggered (PeriodicTask)
Backtrace: 0x40381e58:0x3fca49a0 0x40380621:0x3fca49e0 0x4037ec48:0x3fca4a10 0x4037ec3e:0xa5a5a5a5 |<-CORRUPTED
```

`PeriodicTask` is this library's own audio decode task — the static one created around `Audio.cpp:6802` with
`AUDIO_STACK_SIZE` words and priority 2, pinned to `AUDIO_CORE` — so the canary trip is that task's stack
ending, not a fault in something it called. 4096 words was tried and the stream was stable; 3500 is what was
kept, because upstream Maleksm 0.9.720m uses it too (`I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))/Audio.cpp:39`)
and the stream is stable at that value. One more reason to keep Maleksm's libraries on disk: he had already
raised this number for the same class of failure.

What this was **not**, since that ground was walked first:

- **Not the task watchdog.** There is no `task_wdt` line in the panic, and `CONFIG_ASYNC_TCP_USE_WDT` makes no
  difference to it: the canary is a hardware watchpoint at the end of a stack, not the TWDT.
- **Not the AsyncTCP core or priority.** The panic happened at `CONFIG_ASYNC_TCP_PRIORITY` 2, 3 *and* 4, and on
  an I2S build `async_tcp` is pinned to `NETWORK_CORE` = 1 while the audio task sits on `AUDIO_CORE` = 0, so the
  two never shared a core. A fault that is invariant under a setting is not caused by that setting — leave the
  priority at 3 and the core pinned, because `-1` would let `async_tcp` land on the audio core.

**Where the value lives**: `AUDIO_STACK_SIZE` is a plain `static const size_t` in `Audio.h`, beside the static
`xAudioStack[]` the task is handed. It is not read from `options.h` and is not scaled by `STACK_MULTIPLIER`, so
it cannot be tuned from `myoptions.h` — the header itself has to be edited.

---

## Post-Graft Patch Fixes (2026.10.11)

### The m3u8 manifest was re-fetched with a pause that could never re-arm

`Audio::loop()`'s m3u8 branch re-requested the manifest as soon as `parsePlaylist_M3U8()` returned NULL — the
normal answer while the live playlist has no new segment yet. The pause that exists was armed only when
`no_host_cnt == 2` **exactly**; the counter keeps incrementing past 2, so it never armed again and the same
manifest was fetched as fast as the socket allowed.

Measured on a real HLS/AAC station: ten or more `connect to: "…/playlist.m3u8"` lines a second, each a blocking
connect run on the main loop (`Main loop stalled 819ms, worst stage player+saves`), with the station playing
nothing and the device barely responsive until paused. The alternating manifest/segment fetches in a healthy log
are this same mechanism working — HLS polls rather than holds a stream — so the fault is only the answer when
the manifest has nothing new.

**Fix**: the pause is re-armed on every pass that finds no new segment beyond the first
(`if(!host && no_host_cnt > 1)`), through a named `M3U8_NO_SEGMENT_WAIT_MS` (2000 ms) so the intent is not a
bare literal. A successful parse still resets the counter and arms nothing, so a healthy stream polls exactly as
before. This is the first of the library-visible changes; the other two sides of the same investigation (the
VS1053 library's `TIT2` wiring and the ehRadio title-line assembly) are recorded in
`vs1053-frankenstein-surgery-notes.md` and `.github/code-summary.md`.

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp` only — the m3u8 branch of `Audio::loop()`.

### The media-sequence resync was unreachable, so only a pause/play could bring the stream back

The first field result of the pause above: the wall became one manifest fetch a second, no segment was fetched,
the stream was silent, and **pause/play cured it**. That last detail is the diagnosis.

`parsePlaylist_M3U8()` clears the parsed playlist at the end of its loop
(`vector_clear_and_shrink(m_playlistContent)`), and the resynchronisation block that follows opens with
`if(m_playlistContent.size() == 0) return NULL;`. That guard is therefore **always** true, so `xMedSeq =
mediaSeq` — the repair — and the `packets lost` report behind it could never run. `xMedSeq` is a function static,
reset only through `m_f_firstM3U8call`, which `Audio::setDefaults()` sets: a fresh connect. Once the live window
had slid past the counter, the accept loop's two probes (`xMedSeq`, then `xMedSeq + 1`) both missed, nothing was
queued, the parse returned NULL, and every later parse repeated that — silence plus a manifest fetch per poll
period, until the user pressed pause and play. The one-segment gap has a designed heal (the `xMedSeq + 1` probe,
which logs `mediaseq %llu skipped`); a two-segment gap had no path at all.

**Fix**: in the accept loop, when neither probe matches — which is exact evidence that the counter is out of step,
so no counter or timeout is needed — rebuild the state a fresh connect builds and say so once per parse:

```cpp
f_seqResync = true;
log_w("m3u8 mediaseq %llu not in the window - resyncing", xMedSeq);
f_mediaSeq_found = false;
xMedSeq = 0;
m_hashQueue.clear();
```

The rest of the window is then taken by the hash branch in the same pass, and the next parse re-derives the
sequence from the window under exactly the conditions a connect does. A few seconds of audio may repeat: that is
what the pause/play the user was doing also does, and it is the behaviour chosen over "join at the newest
segment".

Also changed: the pause is now armed on **every** empty answer. Arming it only on the second (`no_host_cnt > 1`)
meant two fetches per period, which is the "one a second" that was measured.

**Not changed**: the hash-scheme path (a station with no derivable media sequence) needs no resynchronisation —
there is no counter to lose, and its 20-entry queue only withholds a segment until the window advances, which is
self-healing. Nor was the helper called again: it indexes `m_linesWithSeqNr[2]` while guarding only `size() < 2`,
so calling it more often would add exposure to a pre-existing out-of-range read on a two-segment window.

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp` — `parsePlaylist_M3U8()` and the m3u8 branch of
`Audio::loop()`.

---

## Post-Graft Patch Fixes (2026-10-21)

### The screensaver's VU read zero when the player VU was off

**Symptom**: with `config.store.vumeter` false, the screensaver's own meter looked wrong - the bar family sat
full, the waveform and Lissajous showed only the graticule, and the spectrum styles showed a fixed shape.

**Root cause**: the audio source, not the widget, was gated on the player page's preference.
`computeVUlevel()` returned before filling `vuLeft`/`vuRight` or publishing the capture window,
`get_VUlevel()` returned 0, and `getWaveform()` / `getSpectrum()` returned false. The screensaver's
`screensaverVU` option only controls whether the widget is created; it never opened the source.

**Fix**: `Config::vuSourceWanted()` (in `core/config.h`) is now the one gate, and the library's four
checks call it:

```cpp
const bool ss = isScreensaver;                       // volatile: read once
return (store.vumeter && !ss) || (ss && store.screensaverVU);
```

While asleep the player page's setting is ignored - only the screensaver meter's own option decides -
and while awake the player page's setting is the answer. The source therefore stops when nothing is
showing a meter, which keeps the CPU saving the old guard was for.

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp` - `computeVUlevel()`, `get_VUlevel()`,
`getWaveform()`, `getSpectrum()`; `src/core/config.h` - `Config::vuSourceWanted()`.

---

## Post-Graft Patch Fixes (2026.10.26)

The same story as the VS1053 notes file - SD playback stopped silently, six root causes, four of them a per-track
member this library never resets - but this library differs in three ways that shaped every fix, and they are worth
knowing before reading a diff:

* **The decoder runs on the audio task and owns nothing else.** `playAudioData()` must never touch the `File` handle, so
  anything that has to ask the file a question (its size, its position) lives in `processLocalFile()`, which owns it.
* **The VU meter is real work here** - a 32-band FFT over decoded samples - so it costs CPU on the decode task rather
  than an SPI register read on a chip.
* **The end of a track has to be declared by the reader**, because there is no chip to ask "are you still decoding?".

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | SD playback stops dead at a track end: no error, `isRunning()` true, VU at zero, `play pos` frozen with a small tail still buffered | `playAudioData()` sets `lastFrame` when less than a max block remains, then still asks `sendBytes()` for a **full** block; the decoder answers 0, nothing is consumed, and neither end rule can fire | `processLocalFile()` declares EOF once the reader is at EOF **and** `bufferFilled() < getMaxBlockSize()`. All three clauses matter: `bytesAddedToBuffer == 0` alone is also true when the buffer is full, and `position() >= size()` alone fires a whole buffer early because the reader runs ahead of the decoder |
| 2 | Every ID3v2 track skipped in 0 ms, one after another; tagless files fine | the frame walk reads tag padding as a frame header, gets a garbage size (3.1 GB measured) and state 5 skips to the end of the FILE | state-4 guard, before any skip: `remainingHeaderBytes < 6 \|\| framesize > remainingHeaderBytes - 6` -> state 99 |
| 3 | No symptom yet - found by reading this library for the VS1053 root causes, which is the practice at the top of this file | `m_sumBytesDecoded` was seeded with a FILE position (`m_resumeFilePos`, `m_audioDataStart`) while it is compared against an audio-relative size | `m_haveNewFilePos` keeps the file position; the counter becomes `m_resumeFilePos - m_audioDataStart`, and `0` on a loop |
| 4 | `sum` ran ahead of the audio by exactly the guard's `rem` on every guarded tag, so `getAudioCurrentTime()` and the resume position were a few hundred bytes out | `sendBytes()` returns the offset of the first frame it finds - **the tail of the tag**, not audio - and that offset was counted as decoded | consume those bytes without counting them, gated by `m_f_firstSync`: a resync after a decode error is still counted as damaged audio, and a resume clears the flag because it lands mid-audio |
| 5 | `MPEG-2.5, Layer I` printed for every working MP3 | the getters return enum/table indices while the label was written against the raw 2-bit header field, and the two numberings disagree | map version 0/1/2 to 1/2/2.5 and layer 3/2/1 to III/II/I, and print `Layer ?` when `MP3GetLastFrameInfo()` zero-filled the fields |
| 6 | Silent web streams after an SD track (the VS1053 latch) | `f_isFile` was latched by `m_f_firstPlayCall`; `setDefaults()` re-arms it here, so this library was correct - one lost reset line away from the same bug | derive it every pass in both libraries, so there is no reset to lose |
| 7 | A decoder that stops consuming was silent for ever: file open, running, taking nothing | nothing printed for that state at all | `loop()` progress watchdog, `AUDIO_PROGRESS_TIMEOUT_MS` = 5 s, gated on "a file is open and its header parsed" and nothing else |
| 8 | `zero read` printed on every track, looking like a fault | the buffer-full case is benign and fires constantly | gate it on `availableBytes > 0`, so it prints only when the read had room and returned nothing |

### The instrument that lied, and the one that did not

The decode errors needed a way to see whether the ring was losing bytes. Three instruments, in order:

1. **The error line** (`sum`, `buf`, `rd`, `wr`, `reader`) - it runs on the audio task while the main task writes the
   ring, so its numbers are samples from different instants. It produced `gap +630,774` - exactly one buffer capacity -
   on ten tracks, and that turned out to be a **torn sample**, not a lost buffer.
2. **A 10 s trace from `loop()`** (`track 10s: sum … ring … buf … gap …`) - one task, therefore coherent. Two hours
   settled it: `gap 0` on every untagged track at every sample (the ring is honest), and a small constant equal to the
   ID3v2 guard's `rem` on every tagged one (the counter, not the start, was wrong).
3. The fix that came out of it (row 4), and **the mistake worth remembering**: the first attempt moved the skip into
   `m_audioDataStart` instead. `Audio-Length` changed by 576 bytes and rejected it - while `gap` did not budge, because
   moving 576 from `sum` to `start` changes both terms of the same subtraction. **A composed metric can hide a change
   that two raw ones would show**, and an invariant checked for days is worth more than a clever new number.

Also worth keeping: `MP3FindSyncWord()` now checks the **layer** field, not just bitrate index 15 and sample rate index
3, because an `FF F1` false sync passed the 11-bit test and was then rejected by `UnpackFrameHeader()`. And when
`findSync()` returns -1 the search now returns -1 instead of doing `pos += -1; nBytes -= -1`, which walked *past* the
block.

**Files changed**: `Audio.cpp` - `loop()` (progress watchdog), `processLocalFile()` (zero-read gate, resume and loop
seeds), `playAudioData()` (first-sync rule, ID3v1 range, per-pass `f_isFile`), `read_ID3_Header()` state 4,
`setDecoderItems()` (label); `Audio.h` - `m_stallSum`, `m_stallMs`, `m_f_firstSync`; `mp3_decoder/mp3_decoder.cpp` -
`MP3FindSyncWord()` (layer check, resync guard) and the `MP3GetLayer()` / `MP3GetVersion()` comments.

## Future issues to watch for

- **Two `mp3 err -6` per hour** in the most recent 2-hour soak, both on tracks whose ID3v2 walk mis-sized a frame, so the
  sync search starts in the tag padding. Playback is unaffected and every track ends normally, but the recovery can lock
  onto a plausible wrong header - a 128 kbps file was reported as 112000 for a while - and that is audible. The cure is
  two-frame confirmation: accept a candidate only if another sync sits exactly `frameSize` later.
- **`MP3FindSyncWord()` and `UnpackFrameHeader()` must stay in step.** The layer check was added because `FF F1` passed
  the 11-bit test and was then rejected by the parser; every field the parser requires should be checked by the search.
- **`audiofile.position()` is not "bytes delivered".** The reader runs a whole buffer ahead of the decoder, so new code
  must not derive progress from the file position - use the ring counters or `m_sumBytesDecoded`.
- **The error line is a cross-task sample.** It reads ring state from the audio task while the main task writes it, so
  never reason from a composed figure there. The `track 10s` trace exists because it is taken on one task.
- **The `stopSong()` probe is quiet for real ends** (they set `m_f_eof` first) but fires on a manual track change, which
  is legitimate - read its numbers as "was the counter short at that moment", never as a fault by itself.
- **When migrating upstream**, diff `setDefaults()` against the ancestor's first - the reset that goes missing there is
  the one that hurts - then compare against Maleksm v0.9.720m for the decoder abstraction and the VU meter.
