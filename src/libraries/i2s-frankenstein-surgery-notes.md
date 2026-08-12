# I2S Frankenstein Surgery Notes

## Purpose
Graft ehRadio's adaptations from the current I2S library (based on Maleksm v0.9.434m / I2S 3.1.0) onto Maleksm's latest v0.9.720m (I2S 3.4.6w).

## Libraries

| Name | Folder | Version | Notes |
|------|--------|---------|-------|
| **Active** | `src/libraries/I2S_Audio/` | 3.1.0n + Maleksm v0.9.434m | Current ehRadio library |
| **Active backup** | `I2S_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` | 3.1.0n | Backup of current library |
| **Maleksm v0.9.434m** | `I2S_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` | 3.1.0n | Base of Active library |
| **Maleksm v0.9.512m** | `I2S_Audio (yoRadio Maleksm v0.9.512m(13.07.25))` | 3.3.2p | Intermediate: added psram_unique_ptr, more decoder files |
| **Maleksm v0.9.720m** | `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))` | 3.4.6w | **GRAFT TARGET** — latest Maleksm |

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

**These are the only differences.** No task changes, no VU meter changes, no PSRAM changes. The I2S library was used essentially as-is from Maleksm.

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
- `AUDIO_STACK_SIZE` still 3300 words
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
The v0.9.434m I2S library is the **maximum compatible version** for the current toolchain. Both Stage 1 (v0.9.512m) and Stage 2 (v0.9.720m) are blocked until a toolchain upgrade. The active library was restored from backup and builds successfully.

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

`Active backup` = `I2S_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` — backup of current ehRadio I2S library

`Maleksm v0.9.434m` = `I2S_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` — original Maleksm I2S 3.1.0n

`Maleksm v0.9.512m` = `I2S_Audio (yoRadio Maleksm v0.9.512m(13.07.25))` — Maleksm I2S 3.3.2p

`Maleksm v0.9.720m` = `I2S_Audio (yoRadio Maleksm v0.9.720m(23.06.26))` — latest Maleksm I2S 3.4.6w, Stage 2 graft target

Graft result = `src/libraries/I2S_Audio/` — after surgery

---

## Addendum (problem discovered much later)

Fixed AudioBuffer::bytesWritten() at Audio.cpp:142: m_writePtr == m_endPtr → >= with overflow wrapping (same pattern as bytesWasRead at line 150). Prevents write pointer from overshooting buffer and corrupting adjacent PSRAM.
