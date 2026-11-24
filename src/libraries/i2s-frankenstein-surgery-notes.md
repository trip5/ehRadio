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
* **Some code is duplicated on purpose, and must be kept in step.** The FLAC tag parser exists twice - `dgLe32()`,
  `dgKeyIs()` and `FLACOfferCommentPacket()` in `I2S_Audio/flac_decoder/flac_decoder.cpp`, and `v1053Le32()`,
  `v1053KeyIs()`, `v1053VorbisComment()` and `v1053CommentTitle()` in `VS1053_Audio/audioVS1053Ex.cpp` - because that
  fork has no de-ogger to hang it on. A fix in one belongs in the other; see item 16 of the 2026.11.13 to 2026.11.16
  section.

## Purpose (historical)
Graft ehRadio's adaptations from the I2S library as it was (Maleksm v0.9.434m / I2S 3.1.0) onto Maleksm's v0.9.720m
(I2S 3.4.6w) - and, from here on, record what the grafted library does and why.

The v0.9.720m copy this refers to no longer exists: it was removed on 2026.11.20, so this is the record of the graft
rather than a procedure that still has a source tree. If the graft is ever picked up again, recover the target with
`git checkout bfe571afa7ef758d7f6340041eeca70a1a73d7af -- "src/libraries/I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))"`
- commit "2026.11.16" is the last tree that held it.

## Libraries

| Name | Folder | Version | Notes |
|------|--------|---------|-------|
| **Active** | `src/libraries/I2S_Audio/` | 3.1.0n + Maleksm v0.9.434m | The current ehRadio library - **the only copy, of our code and of anything else** |

Every other copy is gone, and that is now the intended state:

- `I2S_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` was labelled "Active backup" but stopped being accurate the moment
  the library was patched, and `I2S_Audio (yoRadio Maleksm v0.9.512m(13.07.25))` was an intermediate superseded by
  v0.9.720m. Both were removed on 2026-09-27.
- The upstream copies the graft worked from - `I2S_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` and
  `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))`, the graft target - were removed on 2026.11.20, once they had been
  mined for everything they could teach. Recover one with
  `git checkout bfe571afa7ef758d7f6340041eeca70a1a73d7af -- "src/libraries/I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))"`:
  that commit, subject "2026.11.16", is the last tree that contained them. They are not expected back.

The rule that follows: **never add a folder copy of a library.** A snapshot stops being accurate the moment either side
is patched, and restoring one silently drops every fix made since it was taken. Git history is the backup, and a diff
against a named commit is the comparison.

## Trust Order
Active. There is nothing local left to compare it against; a future graft diffs against a commit, not a folder.

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

#### Conclusion (2026.11.11: no longer true - the toolchain was upgraded)
The v0.9.434m I2S library is the **maximum compatible version** for the current toolchain. Both Stage 1 (v0.9.512m) and Stage 2 (v0.9.720m) are blocked until a toolchain upgrade. The active library was restored from its backup copy and builds successfully — that copy has since been removed (see the note under the library table), so the library that ships is `src/libraries/I2S_Audio/` and its git history is the record.

**Correction, 2026.11.11.** The upgrade above happened: `platformio.ini` now pins `espressif32@8.0.0`, which builds with
`toolchain-xtensa-esp-elf` **14.2.0** and `-std=gnu++2b`, so `<span>` exists, `requires`/`inline` variables are plain
C++20 and Error 3's GCC 8.4 crash cannot recur. **Stage 1 and Stage 2 are unblocked** - nothing in this section was
re-attempted, only the blocker is gone. **The graft would not have raised the stream bitrate ceiling either:** lwIP in
8.0.0 still ships `CONFIG_LWIP_TCP_WND_DEFAULT=5760` with window scaling off, so the FLAC input-rate limit is a
platform matter, separate from this graft.

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

## Post-Graft Patch Fixes (2026-11-03)

Started from "HLS/AAC stutters and plays slow on I2S only". Ended with the decoder keeping up for the first time.
Listed as outcomes: what fixed it, then what was tried and failed. Everything below was measured, not reasoned.

### What fixed it

1. **Sample-rate lock.** `setSampleRate()` substituted `44100` whenever it was handed `0`, so a new song's codec
   setup overwrote a known-good `48000` and nothing re-applied it - the warble. It now ignores `0`, records a
   *pending* rate, and `playAudioData()` applies it on the audio task via `reconfigI2S()`. `reconfigI2S()` stores
   `m_i2sAppliedRate` and clears the flag.
2. **Segment identity from the media sequence, not the URL.** The old accept loop guessed identity by searching each
   segment URL for the decimal form of a drifting counter, with a one-shot resync that reset it to `0` - which is how
   the field saw 887 twice, 888 twice, then 892. Now `EXT-X-MEDIA-SEQUENCE` is parsed properly and identity is
   `base + entry index`, with a persisted watermark (`m_m3u8BaseSeq`, `m_m3u8EntryIdx`, `m_lastM3u8Seq`,
   `m_m3u8SeqValid`). Also fixed `f_mediaSeq_found`, which was `static` and never cleared per window, so a window
   without the tag paired a stale base with a fresh index.
3. **A playlist retry must never be a session reset.** The nothing-new branch called `connecttohost()`, which frees
   buffers, clears the input ring and rebuilds the decoder - audible as seconds of silence after seconds of audio. It
   was also a duplicate: `Audio::loop()` already re-requests the media playlist through `httpPrint()`. The call was
   deleted and the parser now only logs.
4. **Audio task priority 6** (`AUDIO_TASK_PRIORITY`, was the library default 2). On `AUDIO_CORE` 0 that put the
   decoder *below* AsyncTCP (4), so every playlist GET, WebUI request and socket log line preempted it. 6 keeps it
   above AsyncTCP and below lwIP (18), so the network stack still wins.
5. **IIR filter chains bypassed when the coefficients are identity.** Three chains, each 5 float multiplies + 4 adds
   + 8 loads + 8 stores per channel per sample pair, all computing `y = 1 * x` with a flat equaliser. They run only
   when `setTone()` has actually changed the coefficients. This was worth several percent of the audio core.
6. **Float decode.** `FIXED_POINT` disabled in `aac_decoder/libfaad/neaacdec.h`, so FAAD uses its float paths and the
   S3's hardware FPU for the FFT and QMF. This is the change that finally closed the gap - `frames/s` reached the
   clock. Side effect: `MAIN_DEC` is no longer undefined (the fixed-point block is what undid it).
7. **`#pragma GCC optimize("Os")` on `neaacdec.cpp`.** Required, not cosmetic - see the maintenance note below.
8. **The instruments.** An `i2s:` line per 10 s carrying `frames/s`, `decode` and `write` ms, `short` writes, worst
   audio-task `gap`, silence runs, and ring occupancy; plus `lost N seq` on the occupancy line and a `join:` line per
   segment boundary. Every conclusion above came from these. Keep them while there is any doubt.
9. **ID3 preload accounting.** The preload copy used `ID3BuffSize` (the *requested* 4096) for both the memcpy length
   and the byte counter, so a short read would copy uninitialised bytes into the audio ring and end the segment early.
   Both now use `ID3WritePtr`, the bytes actually read. The `id3 preload` timing line was removed once it had answered
   its question (one completing read, 8-20 ms).
10. **Playlist poll back-off.** The pause is `M3U8_NO_SEGMENT_WAIT_MS` (500 ms) multiplied by the count of consecutive
    empty answers, up to 2000 ms, reset when a window yields a segment. A flat 500 ms cost 5-6 GETs per segment. Note
    one poll per segment is the floor - see `code-summary.md`.
11. **HLS read-ahead target raised to 200000** (`HLS_BUFFER_TARGET_BYTES`), with the other buffer/interval values made
    `#ifndef` overridable.
12. **Log volume trimmed.** `connect to:` is no longer printed for any `.m3u8` URL, and the five per-segment `id3
    frame` lines are behind `AUDIO_ID3_LOG`. Per segment the flood went from about ten lines to three.

### Dead ends, all measured

- **Global `-O2`**: decode time unchanged; +181 KB flash. The decoder does not respond to optimisation level.
- **`AAC_NO_SBR`** (drop SBR and PS): gave ~23% idle CPU and `frames/s` exactly at the 24000 core rate, but the
  silence detector then showed whole 1024-sample AAC frames of digital silence being written, repeatedly. Reverted.
- **`SBR_LOW_POWER`**: corrupts the heap on the first HE-AAC frames (bad tail, `multi_heap_free` assert, store fault
  in `PeriodicTask`). The branch is complete but had never been compiled in this tree. Do not retry blind.
- **Dropping PS only** was not pursued: the user will not accept mono.
- **Deeper I2S DMA** (`dma_desc_num` 32 to 64): the worst audio-task gap measured 47-70 ms against a 682 ms DMA, so
  starvation was never the mechanism. Left in place as harmless headroom.
- **Fill-based gate on the playlist poll** (`HLS_PLAYLIST_POLL_FILL`): never engaged, because the ring settles below
  any workable threshold; and one poll per segment is the floor anyway.
- **Scoping the manifest pause out of `Audio::loop()`**: made it much worse - `frames/s` collapsed, the write stopped
  blocking (DMA starved) and the ring overfilled past its target. The whole-loop pause is load-bearing; do not touch it.
- **Refetching the *master* playlist** on a retry: new variant session each time, sequence numbering restarts, decoder
  rebuilt, segment replays. Always retry the media playlist (`m_lastM3U8host`).
- **`readBytes()` blocking on the ID3 preload**: refuted by instrumenting it - one completing read, every time.
- **Display load as the cause**: `DSP_TASK_DELAY` at default, 50 and 200 gave the same `frames/s`, and
  `DSP_TASK_CORE_ID` is 1, so the display task was never on the audio core.
- **Enlarging the input ring**: it is already ~640 KB in PSRAM; the working depth is set by the read-ahead gate.
- **`connecttohost()` as a playlist retry**, listed under fixes, is the same class of error as the master refetch.

### Maintenance notes

- **`neaacdec.cpp` carries `#pragma GCC optimize("Os")` and must keep it while `FIXED_POINT` is off.** The Xtensa
  GCC 8.4 in this toolchain dies with an internal compiler error (`insn does not satisfy its constraints`, postreload,
  on a float constant load) at both `-O2` and `-O1` in FAAD's float path. `-Os` is the only level that compiles.
  Revert the pragma only when reverting to fixed point.
- **Revert `FIXED_POINT` as a pair with the pragma.** One without the other is either a broken build or a decoder
  silently pinned to `-Os`.
- **`FIXED_POINT` off also re-enables `MAIN_DEC`.** If a MAIN-profile stream ever appears, that is why it is accepted.
- **`no_host_timer > millis()` in the m3u8 branch is not wrap-safe** across the millis rollover. Left alone
  deliberately: that line is holding the segment pipeline up, and it is the kind of "obvious improvement" that cost a
  build here.
- **Comments belong here, not in the library files.** The switch sites in `neaacdec.h` and `Audio.cpp` carry a short
  pointer only; the reasoning lives in this file.

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp` - `setSampleRate()`, `reconfigI2S()`, `playAudioData()`,
`playChunk()`, `parsePlaylist_M3U8()`, `m3u8redirection()`, `processWebStreamHLS()`, the m3u8 branch of `loop()`,
`startAudioTask()`; `Audio.h` - the segment watermark, rate-lock and instrument members;
`aac_decoder/libfaad/neaacdec.h` - `FIXED_POINT` off, `SBR_LOW_POWER` and `AAC_NO_SBR` left off with their reasons;
`aac_decoder/libfaad/neaacdec.cpp` - the `optimize` pragma, and a `void*` cast in an `SBR_LOW_POWER` path that had
never been compiled.

## Post-Graft Patch Fixes (2026.11.03)

Streams that must not be played. What the code now does, with the reason each rule exists: bytes that are not audio,
or a host that will not serve the URL, end in a one-shot verdict rather than in the retry ladder, because retrying a
guaranteed failure only delays the message the listener needs.

### What the library now refuses

1. **A verdict channel.** `streamRefuse_e` (`NONE`/`UNSUPPORTED`/`NOT_AUDIO`/`NO_HEAD`), one member `m_refuseReason`,
   one setter `setRefuse(reason, shape)` that logs the shape and the URL and stops the stream, and a public
   `refuseReason()` getter that `Player::loop()` polls once per tick. The setter is the only writer and the first
   verdict wins for the life of the connection.
   - The clear is in `connecttohost()` beside `setDefaults()`, **not** in `stopSong()`/`setDefaults()`:
     `setDefaults()` calls `stopSong()`, so a clear there would erase the verdict the setter had just written.
2. **The head sniff, `sniffHead()`.** Eight bytes, no advance: MPEG-4 boxes at offset 4 (`ftyp`/`styp`/`moof`/
   `mdat`/`moov`) are `UNSUPPORTED`; markup, JSON, PDF, zip, PNG, JPEG and GIF are `NOT_AUDIO`. **Positive signatures
   only** - an unrecognised head is deliberately not refused, because refusing an unusual-but-working stream is the
   worse error and the capped ladder in `Player::loop()` already ends a stream that never produces audio. An ID3 tag
   cannot match any signature, so it passes untouched.
   - HLS: in `processWebStreamHLS()`'s `firstBytes` block, after the 4096-byte preload and before
     `read_ID3_Header()`, where the whole segment head is already in `ID3Buff`.
   - Plain stream: in `processWebStream()` immediately before `m_f_stream` is set, using `InBuff.getReadPtr()` with a
     `bufferFilled() >= 8` test. `readAudioHeader()` is *not* the seam - it is never called on a live stream.
3. **The playlist gate in `parsePlaylist_M3U8()`.** `#EXT-X-MAP` (the fMP4 initialiser) and `#EXT-X-KEY` with a
   method other than `NONE` are conclusive; so is a body whose first non-empty line is not `#EXTM3U`, and a chosen
   segment URL containing `.m4s`/`.cmfv` or ending `.mp4`. All of it sits above `m_codec = CODEC_AAC`, which is where
   the default-to-AAC assumption was made.
4. **Header-time latches.** `unknown contentType` and `unknown content found` call `setRefuse(...)` before
   `goto exit`. `m_f_timeout` is clear on both paths, so `exit` does not reach the library's own reconnect and the
   verdict survives to be polled.

`CT_TXT` still adopts `m_expectedCodec` - which is set from the URL extension - and that is how an HTML body served
for a `.aac` URL used to be decoded as AAC. It is now caught by the body sniff instead of by reordering the
content-type table: fewer moving parts, and the bytes stay the authority.

### Added for the playlist validator (2026.11.03)

Two inline getters were added after the rest of this patch, and only for the validator - nothing in the library uses
them. `PlaylistValidator` drives this same audio object to probe every station in the list, so it needs to do two
things the player never does: empty a verdict between probes (a refusal left latched would outlive the sweep, and
`Player::loop()` would then raise a station error on the station it had just restored), and ask whether a codec was
decided, which is how a probe earns a PASS instead of merely staying connected. `CODEC_NONE` is scoped to this header,
so core cannot compare the codec directly - hence the named predicate rather than an exposed enum.

| file | change |
|---|---|
| `Audio.h` | `streamRefuse_e`, `m_refuseReason`, `refuseReason()`, `setRefuse()`, `sniffHead()`, plus `clearRefuse()` and `codecDecided()` for the validator |
| `Audio.cpp` | both definitions, the clear in `connecttohost()`, the HLS and plain-stream seams, the playlist gate, the two header-time latches |

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

## Post-Graft Patch Fixes (2026.11.04)

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp` - `computeVUlevel()`, `get_VUlevel()`, `m3u8redirection()`.
The VU pair is two separate bounds: `get_VUlevel()` packs the length *cleared* from each bar, so `0` means a full bar
rather than an empty one; and the display gain is `256 / vuThreshold` because `computeVUlevel()` stores
`abs(sample >> 7)` and keeps the high-water mark as the reference.

1. **`get_VUlevel()` answered a FULL bar when it had nothing to say.** The return packs the length *cleared* from the
   loud end of each bar, 8 bits a channel, so `0` means "nothing cleared" - i.e. a fully lit bar. The old
   `if(!config.vuSourceWanted() || config.vuThreshold==0) return 0;` therefore lit both bars to **maximum** every time
   the reference was zeroed: `Config::setTitle()` does that on every title change and `_play()` on every start, so the
   bars snapped full at each track boundary and then drained back over `VU_FADE_MS`. It now answers EMPTY -
   `(dimension << 8) | dimension` - when the source is not wanted, and clamps the reference to 1 (it is also `map()`'s
   divisor) so silence maps to `dimension`, which is exactly what the VS1053 path already did.
2. **The reference releases again, but in real time.** The two `if(vuLeft > config.vuThreshold) ...` lines - a
   high-water mark that never came down while playing - are now one call to `Config::vuRefTrack(vuLeft, vuRight)`:
   raise at once, then fall by `VU_REF_DECAY_PCT` every `VU_REF_DECAY_MS` (6% per 250 ms, ~22%/s, a 2.8 s half-life),
   never below the live reading. This supersedes the earlier "removing the decay makes the reference the loudest
   reading since the last stop" decision: the decay is back, but it is timed off `millis()` rather than the call rate
   and it cannot undershoot the signal, which is what made the old per-call decay pin the bar at full. The rule lives
   in `config.h` so this path and the VS1053 one share it instead of forking it.

**Side effect, accepted:** the sample styles scale by `256 / config.vuThreshold`, so the waveform and the spectrum now
normalise to the last few seconds too and brighten through a quiet passage - the same normalisation the bars get.

3. **The codec table could not choose the profile, and one of its positions was wrong.** `m3u8redirection()` matched
   each `CODECS=` line with `indexOf()`, and `mp4a.40.2` is a literal prefix of `mp4a.40.29`. So every HE-AAC v2 line
   scored as AAC LC, ties were decided by whichever variant the manifest happened to list first, and the 2026-11-03
   "sorted by decode cost" order was **inert for exactly the case it was written for**. A match followed by a digit is
   now rejected, so the index in the table finally decides the profile. Order is now LC, v1, v2, xHE - xHE last here,
   while **v2 is last on the VS1053**, because that chip does no Parametric Stereo and renders v2 as mono. The two
   forks differ in exactly that one position, deliberately; the VS1053 notes carry the hardware proof.

## Post-Graft Patch Fixes (2026.11.11)

**Files changed**: `src/libraries/I2S_Audio/Audio.cpp`, `src/libraries/I2S_Audio/aac_decoder/libfaad/neaacdec.h`.
**No behaviour change**: the warning sweep for `platform-espressif32` 8.0.0, whose GCC 14 is stricter than the GCC 8 of
7.1.3.

1. **`_capSeq` steps with `x = x + 1` in `_capturePublish()`.** It is `volatile` - the odd/even publish guard that the
   audio task sets and every reader compares across its own copy - and `++` on a volatile is deprecated in C++20
   (`-Wvolatile` in GCC 14). The compound assignment is the same read-modify-write, so the ordering the
   `__sync_synchronize()` pair provides is untouched.
2. **`parsePlaylist_M3U8()` lost three dead locals**: `xMedSeq` (a `static` that was only ever zeroed), `f_seqResync`
   and `llasc`. The per-window comment above them stays, because it still explains why nothing in that function is
   `static` any more.
3. **`setVolume()` lost `if(vol < 0) vol = 0;`.** `vol` is `uint8_t`, so the test cannot be true (`-Wtype-limits`).
4. **`ComplexMult()` in `neaacdec.h` is `static inline`.** It is a definition in a header and the floating-point build
   does not call it, so a plain `static` warned "defined but not used" in every TU that includes the header.

## Failed Graft to Maleksm v0.9.720m (2026.11.11)

Attempted 2026.11.11 and abandoned the same day. **Another graft to 720m will not be attempted.** `src/libraries/I2S_Audio/`
is the 434m code again, and that is the point: this library is battle-tested on these boards, the graft was not, and it
cost ~190 KB of flash and a day to learn that. The one thing the newer library had that this one wants is not in the
library at all - the FLAC input rate is a lwIP constant (`CONFIG_LWIP_TCP_WND_DEFAULT`, 5760 with window scaling off).

**The rule worth keeping:** when any library is re-grafted, diff the *task creation call* - stack size, priority, core -
separately from the API surface. 720m creates its audio task at priority 2, which this project had already measured as
"let UI and log traffic preempt the decoder" - a priority that starves the decoder rather than merely delaying it - and
the copy reintroduced it silently: a constant like that produces no compile error, and it was not in the additions table
near the top of this file.

## Speed: what was considered, and why it was left alone (2026.11.13 and 2026.11.21)

**The benchmark stream is `https://radioemisoras.cl/distorsion.flac`** - 48 kHz FLAC, the highest sample rate we support,
so it asks the decoder for more than anything else we play and it shows the write-blocking plainly. It is a benchmark
and not a listening choice: it has latency of its own. Every "Distorsion" below means that URL.

So that the next round of optimisation starts from measurements rather than from a plausible theory. There are **two
rounds here and they are not comparable with each other**, because the instrument changed in between. Each figure says
which round it belongs to.

### Measurement 1 - 2026.11.13, pre-DBCUT

The instrument was `decode` minus `write` on the `i2s:` line: 23-31% on every stream that played, 28% for Distorsion at
48 kHz (`9784 - 6913` of 10025), 29-31% for Radio Paradise at 44.1 kHz. `cyc/frame` read 5,278.

Both fields turned out not to mean what they were assumed to mean - see below - so this round's percentages are a record
of what the readout said, not a measurement of decoder cost.

### Measurement 2 - 2026.11.21, with DBCUT and with `dcyc`

A third counter now brackets the decoder call itself - `MP3Decode`/`AACDecode`/`FLACDecode`/`OPUSDecode`/`VORBISDecode`,
with the post-decode bookkeeping outside it - and is reported as `dcyc`. Distorsion, 20 windows of 10 s:

| field | reads | what it is |
|---|---|---|
| `decode` | 9,807-9,838 ms of 10,027 | the `sendBytes()` call: 98% of the window |
| `write` | 6,963-7,291 ms | the blocking `i2s_write()` *inside* that call - 9,819 + 7,262 exceeds the 10 s window, so the two cannot be disjoint |
| `dcyc` | 636-751, mean **666** cycles per output frame | the decoder call and nothing else |
| `cyc/frame` | 4,890-4,906 | pinned: 4,905 x 48,003 frames/s is 235 Mcycles/s, essentially all of a 240 MHz core |
| `buf` | 178-262 KB | `InBuff.bufferFilled()`, the *input* ring: bytes read from the socket and not yet decoded. Read-ahead, not decoded audio waiting to go out |

**`dcyc` is the one stable number on this line, and the formula that makes the rest of it readable is**

    busy % of a core  =  cyc/frame  x  frames/s  /  clock

`dcyc` counts **work** per output frame and is clock-invariant: 636-751 for FLAC at 48 kHz on a 240 MHz build and
607-663 on a 160 MHz one. `cyc/frame` counts **core-busy** cycles per output frame - the counter stops while the core
idles - so the same 3,250 is 65% of a core at 240 MHz and 97.5% of one at 160. The `residual` and `lpc` fields of the
`flac cost` line sit inside `dcyc`; its `total` and `other` are `m_decodeUs` per FLAC frame with the write inside them
and are not decoder figures.

Two corrections to this file's own earlier claims, both from the 2026.11.21 runs:

- **`cyc/frame` is not pinned to clock/rate.** It read 4,905 in one session and 3,250 in another with the same code
  path, because what it counts depends on how busy the core is, not on how fast the decoder runs. Three builds a whole
  `-O` level apart reading 4,905, 4,900 and 4,895 was not a ceiling - it was three builds measured while the core
  happened to be saturated.
- **`write`, and therefore `decode` minus `write`, is the unstable field.** It read 5,670-6,427 ms, then 6,963-7,291 ms,
  then 5,784-6,130 ms across three sessions with no change to that path. A share computed from it moves ten points on
  its own, so **never compare sessions on `decode` minus `write`**. Compare `dcyc`, and compare `frames/s` against the
  clock to see whether the stream was actually being kept up with.
- **Nothing beside `sendBytes()` is worth optimising.** The per-sample path outside it - VU, `loudnessAccum()`, the gain,
  the ring copy - lives in the 2% of the window `decode` does not cover. `loudnessAccum()` was moved to integer
  accumulation on 2026.11.21 for that reason and not because it was measurable: identical sum, six soft-float calls per
  sample-frame fewer, 88 bytes of flash, and `dcyc` unchanged at 597-697 afterwards, as predicted.

### Measurement 3 - 2026.11.21, the CPU clock trial, and why 240 stays

Three minutes each on an FNIRSI FNB-58 at the USB port, volume 0, Distorsion, ring and cut settled first:

| | 240 MHz | 160 MHz |
|---|---|---|
| `dcyc` | 597-697 | 607-663 |
| `cyc/frame` | 3,243-3,260 | 3,245-3,262 |
| `write` | 5,674-6,130 ms | 5,850-6,107 ms |
| energy over 3 min | 0.076 Wh (1.52 W avg) | 0.076 Wh (1.52 W avg) |

**Unverified, and it cannot be verified from these numbers.** Every reading above is identical whether or not the clock
changed - `dcyc` and `cyc/frame` count work, not time, so a half-speed core doing the same work produces the same
figures. The one line that would have settled it, `Clock: CPU 160 MHz, HAL 160 MHz` from the boot probe, was not kept in
the capture. So the honest statement is: **either the trial ran at 160 and the core went from ~65% busy to ~97.5% busy,
or both runs were the same 240 MHz build and the result is only that the counters reproduce to within 0.5% across
sessions.** The second reading is not worthless, but it is not the experiment.

What does stand, independent of the clock: the energy was 0.076 Wh over three minutes either way, so there is no
saving large enough to see at this resolution, and 240 stays because it is the default every figure in this file assumes
and there is no evidence to trade headroom for. The `CPU_FREQ_MHZ` option written for the trial was removed again.

**The rule this costs us:** a benchmark defines its own evidence, so the boot log goes in the capture with it. Anything
that says which clock, which build and which stream produced the numbers is part of the measurement, not decoration -
without it a run cannot be told apart from a repeat of the previous one.

### What `dcyc` found on the AAC path, which is the real result of the exercise

Same instrument, `zc9483` - AAC/LATM over HLS - against FLAC at the same 48 kHz:

| | FLAC | AAC over HLS |
|---|---|---|
| `dcyc` | 666 | **2,920-3,008** |
| `write` | ~5,900 ms | **20-108 ms** |
| `frames/s` against a 48,000 clock | 47,931-48,046 | **43,904-45,075** |

The AAC/LATM decoder alone costs **4.4x the FLAC decoder per output frame** - ~131 Mcycles/s, 55% of a 240 MHz core -
while `write` sits near zero and frames go missing, which says the task is working rather than waiting on the I2S. The
`hls: nothing new, buffer ...` lines say part of the shortfall is the segment cadence rather than the CPU. **This is the
heaviest stream we play and where optimisation would pay, not the FLAC bit reader**; AUDIO_DEBUG on top of it is what
tips it over, which is why the skips disappear with logging off.

| idea | verdict |
|---|---|
| `-O3`, project-wide or as a decoder `#pragma` | **Measured, no - and the field that would settle it did not exist yet.** 5,278 `cyc/frame` at `-O2` on 2026.11.13, 5,267-5,289 across three configurations, the project-wide one costing 128,724 bytes of flash; on 2026.11.21 `-O3` plus `-fipa-pta` read 4,895 against 4,905 for `-O2`, for 127,568 B more. All of those are `cyc/frame`, which is pinned, so the verdict rests on the flash cost rather than on speed. Re-open only with `dcyc`, and the bar is beating 666 cycles per output frame for 128 KB. |
| FLAC bit-reader rewrite: inline the three readers, 32-bit cache instead of 64, leading-zero table for the unary quotient | **Not the bottleneck, and now bounded.** `dcyc` says the whole decoder call is 666 cycles per output frame, 13.3% of the core, and only part of that is the bit reader. Needs a stream that is actually decode-bound, and none measured is. |
| Prediction-loop hoist | **Cheapest real win, deferred - and it is worth ~4% of the window.** Concretely: locals for the pointer, bit cursor and end in `decodeResiduals()` with the refill inlined so the unary loop stops calling `readUint()` per bit; and a `switch(order)` in `restoreLinearPrediction()` with straight-line cases for 0-4, 8 and 12 so the coefficient loads hoist. 13.2 ms of a 29 ms decoder frame, of which it might take a third. |
| Measure the ~16 ms/frame that no instrument covers (header parse, subframe walk, decorrelation, output scaling) | **Partly located by `dcyc` on 2026.11.21, and still unsized.** The decoder call is 666 cycles per output frame and 13.3% of a 240 MHz core; what is left over inside `sendBytes()` moves with the `write` field, which is the unreliable one, so it cannot be sized from these fields. It needs its own bracket - that is where the next one belongs. |
| Higher-rate FLAC | **Out of scope by decision.** Decode cost scales with sample rate x bit depth, not bitrate: 48/16 is 666 cycles per output frame, 13.3% of the core, so 96/24 lands near 53% of it and 192/24 near all of it, before the write-blocking is added - which is what the 6.1 Mbit stream proved. 1441 kbit/s at 44.1/16 is CD-level and decodes in a fraction of the window, which is the bar we chose to stop at. |
| Bigger reader window, or higher TCP receive window | **Tried, no effect.** `DEOGG_READ_MAX` 4 KB -> 16 KB changed nothing; the de-ogger now reads the full ring room. The 1.2-1.5 Mbit feeds are limited by their own upstream. |
| Audio task priority | **Already done.** Priority 6, above AsyncTCP (4): that change took the deficit from 7% to 2.7% and is still the reason the decoder keeps up. |
| DMA depth | **Already done.** 64 descriptors of 256 frames (682 ms at 24 kHz) after a 341 ms network burst emptied 32. |
| Lower the CPU clock to 160 MHz | **Trial inconclusive (2026.11.21); 240 stays.** 0.076 Wh over three minutes at both settings, and `dcyc` and `cyc/frame` identical - but those fields count work, not time, so they read the same whether or not the clock changed, and the boot line that would have settled it was not captured. No measurable saving, no evidence to trade headroom for, and the framework default is what every figure here assumes. See measurement 3. |
| Smaller ring for MP3/AAC | **Pointless.** ~6 MB of PSRAM stays free, and only FLAC ever fills a ring. |
| Prefetch cap | Not a speed matter - it bounds latency. See item 10. |
| VU and spectrum work | On the display task, never on the audio task. |

**WLED's `[Speed_Flags]` list, checked against the above (2026.11.21).** Nine flags there. Two are already here -
`build_unflags = -Os` and `-O2` in `platformio.ini` - and the `-O3` row above is the filter for most of the rest,
because it says the decoder is not codegen-bound.

| flag | verdict |
|---|---|
| `-fipa-pta` | **Measured, no (2026.11.21).** No `-O` level enables it, so the `-O3` row did not cover it. The compiler side is the decisive half: flash moved 16 B in one pair of builds and 4 B in another, RAM identical, 68 s against ~61 s - the output barely changed, which is what a 0.1% effect looks like. The board comparison predates `dcyc`, so it rests on `decode` minus `write` (35.4% against 36.3%), which the `write` field's own swing makes meaningless. If it is ever re-opened, compare `dcyc` against 666. **Out of `platformio.ini`.** |
| `-funsafe-math-optimizations` | Reachable as part of `-Ofast`, so also untested here - but every decoder in this tree is fixed-point, so it cannot move the 13.3% of the core that `dcyc` measures. The float work is the resampler and biquads in the audio task and the VU FFT on the display task, and no instrument has read either share. Not worth numeric drift in filters before that share is known. |
| `-fsingle-precision-constant` | **Superseded.** It only affects unsuffixed literals, and the per-sample double it might have helped was an explicit cast it does not touch. `loudnessAccum()` was made integer by hand on 2026.11.21 instead: exact same sum, and it costs nothing at the call site. |
| `-fno-jump-tables`, `-fno-tree-switch-conversion` | A workaround for a crash WLED hit. Both are de-optimisations wherever a jump table would exist: more flash, and a compare chain instead. `-O2` has shipped here without them, so try them only if an exception ever lands inside a `switch`. |
| `-freorder-blocks` | Enabled at `-O2` already - no-op. |
| `-Wwrite-strings` | A C diagnostic. In C++ string literals are `const char[]` already, so no-op, and it would only add warnings. |
| `-fstrict-volatile-bitfields` | Changes how volatile bitfields are accessed, and there are none under `src/`: every `volatile` is a bool, a counter or a `TaskHandle_t`. |
| `-free` | Not a GCC option - as written it fails the build, so it is a typo in their file, probably `-Wl,--gc-sections` mangled in transit. |

**Measurement 1, two streams on the old instrument, and what they disagree about.** Distorsion at 48 kHz spends 28% of the window
(`9784 - 6913` of 10025) and Radio Paradise at 44.1 kHz spends 29-31%, although Distorsion carries 9% more samples per
second: its residual is 0.55 ms per frame against RP's 8.5-11 ms, because it is a far less complex encode. So sample
rate sets the floor and source complexity sets the cost on top of it - which is also why the 192/24 stream failed, where
both applied at once. `lpc` moves with it (0.11 ms against 2.3-3.0 ms), so the two halves of a FLAC frame track the
encoder rather than the bitrate the display shows.

## Post-Graft Patch Fixes (2026.11.13 to 2026.11.16)

One session, closed on the 16th. Everything here was built and measured on **espressif32 8.0.0** and needs the patched
Arduino/IDF files that `builds/patch/` applies - the tree does not build without them.

**Files changed**: `src/libraries/I2S_Audio/flac_decoder/flac_decoder.cpp`, `src/libraries/I2S_Audio/Audio.cpp`,
`src/libraries/I2S_Audio/Audio.h`, `src/libraries/VS1053_Audio/audioVS1053Ex.cpp` (+ `.h`), `src/core/netserver.cpp`
(+ `.h`), `src/core/display.cpp` (+ `.h`), `src/displays/widgets/widget_vu.cpp` (+ `.h`), `src/core/player.cpp`,
`src/main.cpp`, `src/core/options.h`.

1. **The Ogg container is removed in the write path, so pages that split a frame play.** `FLACDeOgg()` forwards every
   byte **as it is captured** - page header and lacing included, in stream order - so its output can never exceed its
   input and the guard is `dstRoom < len`. That invariant is the point: the caller reads the full `room`, so no reserve
   is subtracted from `AudioBuffer::writeSpace()`, which hands out only the **contiguous** space to the ring's end. A
   reserve there is a latch, not a margin - once the write pointer came within it, no read could start and the space
   could never grow back. `s_flacCarry` remains for the non-stripped path (a local `.ogg`, or a run where the strip
   never armed); a zero-segment page is handled where the lacing table is parsed.

2. **24-bit FLAC decodes.** The refusal is `> 24`, the subframe decode keeps `int32_t` samples, and the shift to 16-bit
   happens once at write-out. `FLACGetBitsPerSample()` deliberately reports the **output** depth, so
   `setBitsPerSample()` and `reconfigI2S()` stay on their existing 16-bit path.

3. **A stalled stream recovers by itself, in both forks.** `streamDetection()` is given the bytes that **reached the
   ring**, not the bytes the socket offered, so a filter that swallows the stream can no longer look alive; after five
   seconds it stops the decoder, clears `m_dataMode` and raises `audio_eof_stream("stalled")`. `handleEofStream()` routes
   that to `Player::noteStreamStalled()` - guarded on `network.lostPlaying` so a climbing retry is not duplicated - and
   the ladder in `src/core/network.cpp` owns reconnection. The VS1053 fork takes the same route instead of calling
   `connecttohost()` from inside playback, and its liveness is now the same signal: a file-scope `s_ringTotal` counts
   every `InBuff.bytesWritten()`, because its `bytesAvail` argument is the offer measured *before* the read.

4. **Metadata reaches the display, the WebUI and MQTT, at connect and at every track change.** After the strip is armed
   the de-ogger walks its runs and treats a run as metadata only if it *looks* like metadata
   (`dgLooksLikeMetadata()`: the mapping header `0x7F 'F' 'L' 'A' 'C'`, or a block header with type 0-6 and a non-zero
   length). Everything else is AUDIO - a false AUDIO costs one resync, a false metadata silently deletes audio.
   `FLACOfferCommentPacket()` parses the Vorbis comment, the capture is 16,384 bytes and tolerant of truncation, and the
   pre-flip branch captures and forwards the packets that carry the title at connect. The parsed title lives in the
   **de-ogger's own** `s_dgTitle`, read by `FLACDeOggTitle()`: `FLACDecoder_FreeBuffers()` frees `s_flacStreamTitle`
   during `setDecoderItems`, so anything the de-ogger extracts has to survive outside the decoder's lifecycle.
   The WebUI alone stayed empty because `netserver.cpp` built its frame with a raw `%s`: `jsonEscape()` and a 430-byte
   buffer (512 minus the 41 bytes of frame around the value) fix that. The title is reported **once**: the poll no
   longer logs it itself, because `handleShowStreamTitle()` already does under `METADATA_LOG`.

5. **The PSRAM order is enforced by `setup()` instead of being a race.** In order: framebuffer windows and both VU
   canvases in `display.init()`, the FFT scratch in `player.init()` (`Audio::vuScratchReserve()`), the WebUI cache
   straight after LittleFS mounts (`netserver.primeFileCache()`, one-shot, so `begin()` still works if setup did not),
   then `player.reservePsram()` for the de-ogger scratch and the ring. `options.h` carries the table and every `PSRAM@`
   boot line reports it. The screensaver canvas is taken at boot whether `screensaverVU` is on or not, and
   `VuWidget::init()` re-makes a canvas only when its **dimensions** change, because `initScreensaver()` runs on every
   entry. The 1 MB floor in `AudioBuffer::init()` became named reserves for what cannot be allocated early - the SD File
   Manager's 69,632-byte assembly buffer and the 64 KB decoder working set, one codec at a time - plus slack for
   framebuffer churn, so a 2 MB board keeps a full ring with a gzipped WebUI and only FLAC pays when it steps down.

6. **`slow stream` is gone.** The ring sitting below one max block is the steady state, not a symptom: the gate is
   24,576 bytes and the decoder drains a whole block at a time, so the ring crosses it several times a second on a
   healthy stream. The only honest starvation fact is five seconds with nothing arriving, reported once by
   `Stream lost`, and a merely tight stream is described by `gap`, `buf` and frames/s on the `i2s:` line.

7. **Diagnostics that stay.** `deogg:` reports the filter state with `read B/s` against `emitted B/s` (audio being
   discarded is invisible to every counter that advances on the read) plus `meta`, `cm` (comment captures offered),
   `rj` (reassemblies) and `c` (a capture pending across a page). The `i2s:` line carries `smp decoded/written` and
   `nod`, and `smp` reads identical in every window - nothing is lost between decoder and DMA, so any shortfall is
   producer-side. `FLACProfileReset()` keeps `flac cost` from printing stale numbers into an MP3 window. The `deogg:`
   line is compiled only under `AUDIO_DEBUG`, and it can only print while the de-ogger is in the path - a line at
   connect and one per track change, never for the life of a stream.

8. **The decoder refuses reserved values** - `chanAsgn > 10`, `sampleSizeCode` 3 or 7 - so a garbage header cannot
   reconfigure I2S to `96000 Hz, 1 ch, 8 bits` and produce the `-2/-4/-5/-11` burst that made a healthy stream sound
   broken.

9. **Not worth re-opening without a measurement: decoder speed.** `-O3` moved 5,278 cycles per frame to 5,267-5,289
   across project-wide and `#pragma` variants and cost 128,724 bytes of flash, so the build stays `-O2`. With the
   `decode`/`write` split now on the `i2s:` line, distorsion spends 23% of the window decoding and Radio Paradise 27-31%:
   the `flac cost` total includes the blocking `i2s_write()`, so it must not be quoted as a decode cost. Measured per
   FLAC frame on `mellow-flacm` (1441 kbit/s, block 4096, 44.1 kHz, 16-bit): 91,456 us total, of which `i2s_write()` is
   ~61,000; residual 10,516 and lpc 2,677 are the whole of the Rice/LPC work - 13.2 ms of a ~29 ms decoder frame. The
   other ~16 ms is header parse, the subframe walk, stereo decorrelation and output scaling, which **no instrument
   covers yet, and it is the larger target**: measure it before any hot loop is touched.

10. **A prefetch cap for live streams: considered, rejected.** The ring roams 595 KB to 1.33 MB (869 KB-1.03 MB through
    the six-hour soak) because these servers send in bursts faster than real time. A cap would be a stop-reading
    watermark, not a smaller ring, and its only effect would be to *shrink* the buffer: `nod 0`, `sil 0 runs` and `smp`
    equal for six hours say the buffer is doing its job, and the servers that hand out only 150-250 KB stay safe
    because of it. `BUFFERBAR_VISUAL_FULL_KB` stays at 250 for the same reason.

11. **One crash to watch, not ours.** `assert failed: udp_new_ip_type ... (Required to lock TCPIP core functionality!)`
    on `loopTask` at a `connecttohost` while the file updater, MQTT and the radio-browser click were all in flight -
    IDF's lwIP thread-safety check, a UDP socket created while another task held the TCPIP core. The tree has **zero**
    UDP call sites (no `WiFiUDP`, no `udp_new`, no `configTime`/SNTP), so the caller is IDF or a library. `builds/patch/`
    does change lwIP's window, receive mailbox and `MAX_ACTIVE_TCP`, so if it returns, revert those two first.

12. **The PSRAM budget reports its own steps, and the step is named.** The `[Heap]`/`[PSRAM]` walk is every minute (the
    LittleFS query stays at ten), the PSRAM line names framebuffer+canvas, cache and `Ring+other`, and a move past
    16 KB logs `PSRAM step:` or `Heap step:`. The 247 KB a 2.5-hour soak showed as unattributed is the **FLAC decoder
    set**, taken at the first FLAC stream start by `FLACDecoder_AllocateBuffers()`: 192 KB of it is `s_samplesBuffer`
    (2 x `MAX_BLOCKSIZE` 24,576 x int32), the rest the Ogg carry and the header/metadata structs.
    `FLACDecoder_FreeBuffers()` frees all of it and two station changes returned `Ring+other` to the same 1894 KB, so it
    is a floor, not a leak. The 93 KB on top of that on an `https` station is its live TLS session, which mbedTLS takes
    from PSRAM by design and releases with the connection. `PSRAM_RESERVE_DECODER` was 64 KB and is now 256 KB: the old
    figure would have failed FLAC outright on a 2 MB board, where the ring takes everything above the floor.

13. **A Vorbis stream could crash the device, and no longer can.** `dgLooksLikeMetadata()` accepted "block header type
    0-6 with a non-zero length" as FLAC metadata, which an Ogg Vorbis header (`0x01`/`0x03`/`0x05` + `vorbis`) also
    satisfies - so the de-ogger captured Vorbis comment packets and parsed them as FLAC, visible as `strip 0` with `cm 5`
    in its own line. `FLACOfferCommentPacket()` then read the vendor length out of Vorbis text, went negative as
    `int32_t`, slipped past the `> end` test and became a pointer offset: `LoadProhibited` on `loopTask`, the same PC
    and backtrace on every preview of that station. Vorbis and Opus headers are now refused by name, the vendor length
    is checked against the block it has to fit inside, and the title is copied by length instead of `strncat` because a
    comment field is length-prefixed and need not be NUL-terminated.

14. **A station refused by both identities now ends with a message.** Two faults sat behind "the WebUI says it is
    playing and pause shows [stopped]": the alternate-user-agent retry reconnected with the *same* identity (the UA flag
    is reset by a successful header parse, so using it as the attempt counter made "retrying once" print four times), and
    when the budget ran out the library logged one line and stopped - no `audio_showstreamtitle`, no verdict, so
    `noteFatalStream()` never ran. The retry is now gated on a function static in each response-header case, untouched by
    `setDefaults()` and by `lastToDo:`, so it happens at most once per chain; and at the cap both paths
    `setRefuse(STREAM_REFUSE_UNSUPPORTED, "refused with both user agents")`, which the core already turns into a
    permanent stop. Measured: ABC's HLS playlist answers 403 to both identities, and the device now shows
    `Error connecting to <url>` in about four seconds instead of sitting in PLAYING for thirty. The player's own
    `STREAM_START_DEADLINE_MS` (30 s) is the backstop, proven healthy by the `arm gate:` trace - it was never the
    problem, only too slow to be the answer.

15. **An HLS segment request must never travel on the playlist's connection.** `httpPrint()` reconnected only when
    `!_client->connected()`, so as soon as the playlist was open every segment request was written down *that* socket -
    `GET /<segment>.aac` sent to the playlist's host, which of course does not have it: the CDN answered 404, no connect
    was logged, and `m_lastHost` (the playlist) is what the error message printed, which made a perfectly good station
    look like a dead URL.  Real chains do this: scastream's media playlist lists segments on `lax-sa46…`/`syd-sa46…` at
    `https:443` while the playlist itself was plain `http:80`, and curl fetched every one of those URLs with our own
    agent and no cookie.  `httpPrint()` now derives the scheme and port from the URL, and reconnects whenever the
    target's host/port/scheme differs from the socket in use, stopping the old one first.  The same guard was in the
    VS1053 fork and is fixed there too.  Also on that path: the user agent was the literal `VLC/3.0.21 …`, so the core
    did not control identity for HLS segments at all; it now sends `STREAM_USERAGENT` or `STREAM_USERAGENT_ALT`.

16. **The FLAC tag parser now has a twin in the VS1053 fork, and the twin is deliberate.** The chip reports audio and no
    text, and an Ogg FLAC stream carries no ICY metadata, so a FLAC station on the VS1053 board showed no title at all
    while the same station had one here. That fork's `feedOggFlac()` already walks pages, so the VORBIS_COMMENT parse was
    written there as `v1053CommentTitle()` / `v1053VorbisComment()` instead of hoisting `FLACOfferCommentPacket()` into a
    shared header: this file's de-ogger is the most delicate code in the tree and it was left untouched. The consequence
    is a standing obligation recorded in both notes - **`dgLe32` / `dgKeyIs` / `FLACOfferCommentPacket()` here and the
    `v1053*` copies there are the same parser**, so a bound check changed in one belongs in the other. The vendor-length
    test from item 13 is the one that matters: a length read from foreign data must never become an offset.

## Post-Graft Patch Fixes (2026.11.17)

**Files changed**: `Audio.cpp`, `Audio.h`. Built on **espressif32 8.0.0** with the patched files that `builds/patch/`
applies.

1. **`setCutDb(float)` is the output-stage gain the normalization work needed.** `m_cutLin` is `powf(10, -dB/20)` and is
   multiplied into `m_limit_left/right` at the end of `computeLimit()`: after the cubic, so the knob's curve and its feel
   are untouched. `m_limit_*` are private, so the setter has to call `computeLimit()` itself - which is also why the four
   new methods live in the `public:` block near `setAudioTaskCore()` rather than beside `computeLimit()`, where they
   would have been private to the library.

2. **`loudnessAccum()` measures the stream before the fader.** It is called from `playChunk()` right beside
   `computeVUlevel()` - deliberately *not* from inside it, because that function returns early when the VU bar is not
   wanted - and it runs before `Gain()`, so its value is independent of the knob and of the cut. Blocks are
   `m_sampleRate / 20` (~50 ms), the statistic is a mean of **power** rather than the VU's peak-ish high-water mark, and
   anything under -50 dBFS is dropped so pauses cannot drag the mean down. The exponential mean uses a 1/200 step, i.e. a
   10 s time constant.

3. **Two multiplies and two adds per sample pair.** `m_loudSum` accumulates `l*l + r*r` in a double; the divide and the
   log happen once per block, never per sample, so the added cost sits well under the three IIR chains already running.

## Post-Graft Patch Fixes (2026.11.18)

**Files changed**: `Audio.cpp`, `Audio.h`. Built on **espressif32 8.0.0** with the patched files that `builds/patch/`
applies.

1. **`loudnessRaw()` is the only addition, and it is a diagnostic accessor.** It returns `-1` on this path, because the
   codec measures the samples it decodes instead of reporting a register. The VS1053 build returns its raw
   `SCI_AICTRL3` reading under the same name, so `player.cpp` can log one line every 10 s while a station is learning on
   *either* build, and the two logs can be paired to calibrate that meter against this path - which is how its
   `VS1053_METER_FULL_DB` was set to 100. Nothing about the measurement, the gate or the time constant changed.

## Post-Graft Patch Fixes (2026.11.23)

**Files changed**: `src/libraries/VS1053_Audio/audioVS1053Ex.cpp` (+ `.h`). A VS1053 change recorded here because the
numbers that explain it are the ones above; the fork's own notes carry the short version.

1. **The VS1053 bitrate is measured now, not read out of a register.** `getAudioCurrentTime()` read `SCI_HDAT0` and
   multiplied by 8 - the AAC conversion, which that file itself calls undocumented - and the result had to land inside
   8,000-640,000 to survive. On an Ogg FLAC stream the register holds something else: two consecutive connections to the
   same ~2 Mbit source read **262,944** and **444,104** bit/s, and `m_brEstimated` then locked the first of them in for
   the rest of the stream. The display was never at fault - `formatBitrate3()` already renders `kbps >= 1000` as one
   decimal of a Mbit, so 2,000 kbit reads `2.0`, and 9.9 is the only ceiling because that is all three characters hold.

   The replacement is the ring balance: `(Δ bytes that reached the ring − Δ ring occupancy) × 8 ÷ Δt` is what the chip
   consumed, and a real-time stream consumes at its own bitrate. Server bursts and ring depth cancel in the subtraction,
   so it needs no per-codec table and cannot be fooled by a full ring. The two numbers it subtracts already existed:
   `s_ringTotal`, counted for the liveness clock, and `InBuff.bufferFilled()`.

2. **A 2 s window, smoothed over five, published only when the kbit would change.** The core's field is kbit and the
   badge redraws on every `setBitrate()`, so sub-kbit wobble would cost display work and show nothing. **No range gate
   and no latch:** whatever the measurement says is the number, and it is free to change - which is the whole point of
   replacing a latch with a measurement.

3. **MP3 keeps its exact value.** `getAudioCurrentTime()` reads the frame bitrate out of `HDAT1` through the l3id3 /
   l3id012 tables, better than any measurement for MP3 including VBR, so the measured path skips MP3 and the rest of
   that function is now `return 0`.

4. **`slow stream, dropouts are possible` is gone from the VS1053 fork**, for exactly the reason item 6 of the I2S
   post-graft list gives about the same message here: a ring below one max block is the steady state, because the feeder
   drains whole blocks. Measured on a healthy 1.4 Mbit stream it fired three times over a ring that then held 334 KB.
   The five-second no-bytes-reached-the-ring clock is the honest test and stays.

5. **`track10s` reports a feed rate in KB/s.** `m_feedBytes` is cumulative across connections, so the rate comes from a
   baseline captured when the connection starts. It is the throughput figure that fork lacked, and it separates "the chip
   is ahead of us" from "the network is behind us" in a capture.

6. **The same number on I2S, from the one place that can see it.** `computeAudioTime()` returns at once for a live
   stream - its guard wants a local file or a `ST_WEBFILE`, and a stream is `ST_WEBSTREAM` - so the FLAC badge there
   showed `setBitrate(FLACGetBitRate())`, the decoder's one-frame nominal, for ever: **2.7 Mbit on a 24-bit stream that
   carries 1.7**, because that formula divides by a "compression ratio" below 1, which FLAC cannot produce. The
   measurement now lives in `playChunk()`'s existing 10 s report window - `Δm_sumBytesDecoded × 8000 ÷ span` - and both
   forks share one publishing rule: **at most once every 10 s, and only when the number the badge renders changes**
   (tenths of a Mbit at 1000 kbit and above, whole kbit below). A 1.5 Mbit stream wobbles by several kbit a second, and
   publishing every wobble was printing a line a second. MP3 keeps its exact `HDAT1` frame bitrate, which is better than
   any measurement and only changes when a VBR encoder does.

7. **What a 24-bit stream really does on I2S: it starves, it does not decode slowly.** Lapfox
   (`radio.lapfoxradio.com/radio/8000/stream-flac.flac`, 48 kHz, 2 ch, **24 bit**, block 4096, ~1.7 Mbit, chunked over
   TLS) reports `38,477 frames/s` against a 48,000 clock - **20% of frames never produced**, which is the static heard
   on that board - while `dcyc` reads **1,144**, the same as Distorsion's 1,089-1,172 on the same build, `decode` is
   only 30% of the window and `write` is 36 ms. So the decoder is not the constraint: the ring sits at 10-28 KB (one to
   three frames), the de-ogger shows `avail 0` and its reads swing 45-208 KB/s. **The free heap at connect was 46 KB
   here against 156 KB on the VS1053 board for the same station**, which is where to look next: a TLS session needs
   room, and a socket that keeps answering "nothing available" is what a 20% frame deficit looks like from the audio
   task. The frame sizes rule out the other easy theory - measured in a capture, median 19,796 bytes, max 21,635, all
   under the 24,576-byte reader window.

8. **The 24-bit static was a 32-bit accumulator in the LPC reconstruction.** `restoreLinearPrediction()` summed
   `sample x coefficient` in an `int32_t`. For a 16-bit source that lands just under 2^31 - 2^15 samples x 2^15
   coefficients, with the quantiser shift keeping it in range - which is why it looked safe for years. For a 24-bit
   source it is **2^23 x 2^15 = 2^38**, so every LPC subframe wrapped and came out as noise. The measured signature is
   worth remembering because nothing in the log looks wrong: `smp` equal to `frames`, `nod 0`, `dcyc` the same as a
   16-bit stream, `sil 0 runs`, and a post-downshift peak just under full scale (`8269473 -> 32303`, exactly /256). A
   value that is *wrong* is invisible to every counter here, and the frame CRC-16 is read and discarded, so there was no
   integrity check to catch it either. The accumulator is `int64_t` when `bitsPerSample > 16`; 16-bit streams keep the
   int32 loop unchanged. `shift` also became `int32_t` - LPC's quantiser shift is signed by definition, and a negative
   value was being truncated to `uint8_t` and turned into a shift of 255.

9. **An unsupported FLAC depth now ends the stream instead of spinning.** `bitsPerSample > 24` returns
   `ERR_FLAC_BITS_PER_SAMPLE_TOO_BIG` on **every** frame, and the `stopSong()` for it was commented out - so the error
   path only cleared `m_f_playing`, sought the next syncword, failed again, and looped at frame rate with the UI showing
   PLAYING and nothing said. It now calls `setRefuse(STREAM_REFUSE_UNSUPPORTED, …)` and stops, the same route item 14
   used for a station refused by both identities. 24 bits stays the ceiling and that is a decision, not an oversight: a
   32-bit sample plus its one-bit side channel is **33 bits**, so `s_samplesBuffer` (int32_t), `readUint()` (uint32_t)
   and the mid/side `<< 1` would all have to become 64-bit, and the decoder's PSRAM would double from ~196 KB to ~393 KB
   and no longer fit a 2 MB board. No station in the list sends 32-bit FLAC, and the wall for the 24-bit one we do have
   is the input path, not the decoder - so refusing clearly is worth more than half-supporting it.

10. **The display throttles itself while the audio ring is starving.** `loopDspTask()`'s `DSP_TASK_DELAY` is multiplied
    by four while the ring holds less than **250 ms** of audio and released above **750 ms**. The evidence is the
    24-bit station: with a live 480x320 TFT its ring sat at 10-28 KB and 20% of frames were never produced; with the
    screensaver blanking the screen the same stream filled to **1.29-1.42 MB**, ran at 48,000 frames/s with `sil 0`, and
    **did not fall back when the screen came on again** - so the cost is the fill phase, not drawing per se.

    The threshold is TIME, not bytes - `bufferFilled() x 1000 / streamBytesPerSecond()` - because the same byte count is
    a moment at 1.7 Mbit and tens of seconds at 128 kbit, and HLS stations legitimately run a shallow ring, so a byte
    threshold would pin the limiter on for a stream that is healthy. Both forks grew a `streamBytesPerSecond()` under
    the same name (measured consumption, 0 until known) so `display.cpp` needs no `#if`; until a rate exists - the first
    seconds of a station, exactly when this matters - a 32 KB floor stands in for it. On the VS1053 path the ring
    normally holds hundreds of KB against 128-320 kbit, so it never engages. Transitions log `Audio.risk engaged` /
    `released` under AUDIO_DEBUG, so a soak can confirm it rather than assume it.

    The older entry in this file saying "display load as the cause" was measured on a stream that was keeping up
    (48,000 frames/s, nothing to lose), so it neither contradicts this nor tested it.

**And one misreading worth recording, because the log invites it.** `feed ok: wrote 640 of 1600, DREQ low` is not
starvation. The feeder loop breaks when DREQ goes low, so for a partial write that field is the *reason* it stopped: the
chip's FIFO is full, i.e. we are ahead of it. Starvation reads `feed stalled: wrote 0 of …` and requires more than a
second since the last accepted byte. `DREQ high` with a full 1600-byte write is the chip taking everything and still
wanting more - equally healthy, and both appear in an ordinary capture.

## Comment Sweep (2026.11.24)

**Files changed**: `Audio.cpp`, `Audio.h`, `flac_decoder.cpp`, `flac_decoder.h`, `audioVS1053Ex.cpp`, `audioVS1053Ex.h`,
`display.cpp`. Comments only - no statement was added, removed or reordered.

The project rule is a comment of five to thirty words as `//`, never a paragraph. These two libraries break it in three
ways: upstream `/* */` blocks, banner attribution, and paragraphs added by hand during surgery. `builds/comment_audit.py`
was written to scope the sweep from numbers rather than memory, and it stays in the repository as the check.

1. **What was converted.** 106 standalone `/* */` blocks became `//` lines across the seven files. A block is converted
   only when it starts a line and nothing follows it on its closing line - `x = a /* note */ + b;` cannot become `//`
   without swallowing the statement, and 76 such inline blocks were reported and left alone. The file banner is never
   touched: it is attribution, and one of the two things the header convention is for.

2. **Vendored and upstream code is excluded by name**, because rewriting its comments makes the next re-graft undiffable:
   `libfaad/*`, `opus_decoder/silk.cpp` + `celt.cpp`, `vorbis_decoder/lookup.h`, `vs1053b-patches-flac.h` (10 files, never
   touched), and the four decoder implementations under `mp3_decoder/`, `vorbis_decoder/`, `opus_decoder/`,
   `aac_decoder/`. Those are still *audited* so their size is visible - 409 block comments in `mp3_decoder.cpp` alone - and
   the tool prints them in a separate table that it never converts.

3. **Two converter defects found by running it, both silent.** The first: no `//` branch. A `/*` *mentioned inside* a
   `//` comment - `if(/* m_streamType != ST_WEBFILE && */ …)` is the reverse case, but a comment that documents a
   block-comment syntax is the real one - then pairs with a distant `*/` and the region between is read as one block, so
   real blocks went unconverted in `Audio.cpp` (20 of them, found only after the first pass reported 106 and left the file
   still full of `/*`). The second: `standalone` tested only what preceded the block on its line, so `    /* note */ x = 1;`
   would have become `    // note x = 1;` and commented out the assignment. Both are fixed, and the fix is recorded in
   `libraries-note.md` where the convention lives.

4. **Three guards, because a comment sweep is a refactor.** `--fix --write` compares each file's code - with comments,
   blank lines and indentation removed - against the file it is about to write, and refuses that file if a statement
   moved. It reports any `//` line that would swallow a macro continuation's backslash, which would shorten a macro
   silently rather than fail to build; there were none, and the three `VS1053VOL` curves are exactly the shape that would
   hide it. And the block-comment count before and after is printed, so "did it actually convert?" needs no diff.

5. **The numbers.** Our seven files: 177 block comments and 57 comments longer than five lines before; 50 and 22 after
   (a `//` run and a block are counted alike). 106 blocks converted, 76 inline blocks reported. The 22 that remain are
   rule boxes (`// ---- … ----`), ASCII diagrams - the `AudioBuffer` wrap-around drawing in both headers, which is the
   only explanation of `m_resBuffSize` there is - the `SCI_MODE` bit-field table, and Wolle's API text. Paragraphs that
   were genuinely prose were rewritten to two or three lines: the de-ogger's four paragraphs, the cycle-counter note, the
   HLS and XML examples (13 and 11 lines), `computeLimit()`'s commented-out variants, `setPlaybackSpeed()`'s dead
   reclock, and `display.cpp`'s limiter, MOVE, clock, numbering and font-switch notes.

6. **The proof that nothing moved.** Both forks were built before the sweep and after: `firmware.bin` is
   **3,160,640** bytes for `ili9488_pcm_1button_full` and **2,566,592** for `sh1106_vs1053_3buttons`, both byte-for-byte
   the same size as the pre-sweep images, with RAM 110,328 and 90,524 unchanged. A swallowed statement changes image
   size; a converted comment does not.

7. **The two files that had no ehRadio header got one**, `flac_decoder.cpp` and `flac_decoder.h` - the most-edited files
   of the day and the only two in the libraries without it. `flac_decoder.h`'s restriction list also stopped claiming
   "bits per sample must be 8 or 16", which the 24-bit fix has made untrue, and now says 24 is downshifted to 16. The
   convention and the check that enforces it are in `libraries-note.md`.
