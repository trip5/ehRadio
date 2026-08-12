# Frankenstein Surgery Notes — VS1053 Library

## Purpose
This document records the complete process of grafting Maleksm's anti-skip FreeRTOS task infrastructure into the PR226 audio core (which handles VS1053 patches correctly). It serves as both a record and a template for future library migrations.

---

## Naming Convention

| Name | Folder | Description |
|------|--------|-------------|
| **Grafted (current)** | `src/libraries/VS1053_Audio/` | Result of this surgery — PR226 core + Maleksm task + ehRadio adaptations |
| **Backup of current** | `src/libraries/VS1053_Audio (ehRadio nsteplanets PR226 frankenstein)` | Backup of this surgery — PR226 core + Maleksm task + ehRadio adaptations |
| **ehRadio PR226 copy** | `VS1053_Audio (ehRadio nsteplanets yoRadio PR226)` | PR226 with Trip5's minimal ehRadio adaptations. **This was the graft base.** |
| **Active (old)** | `VS1053_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` | Backup of previous Maleksm-based library, preserved for reference |
| **Maleksm** | `VS1053_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` | Original Maleksm v0.9.434m — FreeRTOS task infrastructure originates here |

---

## Trust Order (when making decisions)
Active > Maleksm

When in doubt during grafting: prefer the Active library's patterns, then Maleksm's.

---

## Why This Surgery Was Needed

| Problem | Source | Solution |
|---------|--------|----------|
| VS1053 patches cause silence | Maleksm library audio core | Use PR226's init sequence + audio code |
| Audio skipping during display updates | PR226 has no FreeRTOS task | Graft Maleksm's anti-skip task |
| VU meter stuck at 99-100% | PR226 narrow 85-92 mapping | Graft Maleksm's 0-95 range + peak-hold decay |
| SM_CANCEL stuck after redirects | Both libraries | Graft Maleksm's SM_CANCEL clearing fix |

---

## Pre-Surgery Comparison Strategy

Before grafting, compare these pairs to understand what changed:

1. **`VS1053_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` vs `VS1053_Audio (ehRadio Maleksm v0.9.434m(04.04.25))`**
   → Reveals Trip5's ehRadio adaptations to Maleksm (include paths, logging, PSRAM macros, config changes)

2. **`VS1053_Audio (ehRadio nsteplanets yoRadio PR226)` vs `VS1053_Audio (yoRadio Maleksm v0.9.434m(04.04.25))`**
   → Reveals what Maleksm changed from the PR226 base (task infrastructure, VU meter, SM_CANCEL fix, playAudioData coordination)

For future migrations, do both comparisons before starting the graft.

---

## Key Findings

### FreeRTOS Task Infrastructure Is From Maleksm
The original Maleksm library contains `startAudioTask()`, `performAudioTask()`, `mutex_playAudioData`, `mutex_audioTask`, `m_f_audioTaskIsRunning`, etc. in both `.h` (lines 264-267, 460-467) and `.cpp`. Maleksm added the task to solve skipping on yoRadio. The old Active library kept it unchanged.

### Stack Size: 3300 Words
`AUDIO_STACK_SIZE = 3300` words (13.2KB on ESP32-S3). This value was "mostly fixed" after earlier overflow testing and is used in Maleksm, Active, and the ehRadio Maleksm backup.

### Mutex Type Mismatch (Documented, Not a Bug)
`mutex_playAudioData` is created with `xSemaphoreCreateMutex()` (non-recursive) but the connect functions use `xSemaphoreTakeRecursive()`. This works in practice because the mutex is never recursively taken — always take-once-give-once per function. No action needed.

### PSRAM Allocation Strategy
Per trust order: use Maleksm/Active's approach. Small buffers (`m_chbuf`, `m_ibuff`) use `malloc()`. Large audio buffer uses `ps_calloc()`. Destructor uses `x_ps_free()`.

---

## Critical Bug: `m_f_stream_ready` Never Set

### Discovery
After the initial graft, audio was completely silent with or without patches. The root cause: `m_f_stream_ready` was declared in the PR226 header but **never written to** anywhere in the code. PR226 uses local `f_stream` variables in each processing function instead of the member variable.

### Impact
Both `playAudioData()` and `performAudioTask()` have `if(!m_f_stream_ready) return;` as their first/second guard. Since the flag was always `false`, both returned immediately on every call. The FreeRTOS task ran but never called `sendBytes()` — zero audio data reached the VS1053.

### Fix
Added `m_f_stream_ready = true;` at all 5 locations where PR226 sets local `f_stream = true`:
1. `processLocalFile()` — line 782: file header parsed, stream ready
2. `processWebStream()` — line 956: MP3/AAC buffer filled, stream ready
3. `processWebStreamTS()` — line 1091: TS buffer filled, stream ready
4. `processWebStreamHLS()` — line 1209: HLS buffer filled, stream ready
5. `processWebFile()` — line 1275: web file buffer filled, stream ready

Also added resets in `setDefaults()`: `m_f_stream_ready = false`, `m_f_eof = false`, `m_f_ID3v1TagFound = false` to properly reset state between streams.

### Prevention for Future Grafts
When grafting task coordination into a library that uses local stream-ready flags, search for all `f_stream = true` assignments and add corresponding member variable writes. Also search for all member variables used in task functions and verify they are written somewhere.

---

## Graft Execution Log (14 Steps + Bug Fix)

### Step 1: Overwrite Active Library With PR226 Copy
Copy all files from `VS1053_Audio (ehRadio nsteplanets yoRadio PR226)` into `src/libraries/VS1053_Audio/`. This becomes the graft base with PR226's correct audio core and init sequence.

### Step 2: Header — Add FreeRTOS Includes, Task Members, Declarations

**Added includes** (after WiFiClientSecure):
```cpp
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
```

**Added static task buffers** (between AudioBuffer class and Audio class):
```cpp
static const size_t AUDIO_STACK_SIZE = 3300;
static StaticTask_t __attribute__((unused)) xAudioTaskBuffer;
static StackType_t  __attribute__((unused)) xAudioStack[AUDIO_STACK_SIZE];
```

**Added member variables** (near SPISettings):
```cpp
SemaphoreHandle_t   mutex_playAudioData = NULL;
SemaphoreHandle_t   mutex_audioTask     = NULL;
TaskHandle_t        m_audioTaskHandle = nullptr;
bool                m_f_audioTaskIsDecoding = false;
bool                m_f_lockInBuffer = false;
bool                m_f_firstPlayCall = true;
```

**Added Maleksm-required member variables** (near other bool flags):
```cpp
bool            m_f_stream = false;
bool            m_f_eof = false;
bool            m_f_ID3v1TagFound = false;
uint32_t        m_sumBytesDecoded = 0;
uint32_t        m_bytesNotDecoded = 0;
int16_t         m_validSamples = {0};
```

**Replaced `setAudioTaskCore` no-op stub** with full task declarations:
```cpp
public:
  void            setAudioTaskCore(uint8_t coreID);
  uint32_t        getHighWatermark();
private:
  void            startAudioTask();
  void            stopAudioTask();
  static void     taskWrapper(void *param);
  void            audioTask();
  void            performAudioTask();
  uint8_t         m_audioTaskCoreId = 1;
  bool            m_f_audioTaskIsRunning = false;
```

### Step 3: Constructor — Add Mutex Creation + startAudioTask()
Added after `spi_VS1053->begin()`:
```cpp
mutex_playAudioData = xSemaphoreCreateMutex();
mutex_audioTask     = xSemaphoreCreateMutex();
```
Added before closing brace: `startAudioTask();`

### Step 4: Destructor — Add Cleanup
Added before freeing buffers:
```cpp
stopAudioTask();
vSemaphoreDelete(mutex_playAudioData);
vSemaphoreDelete(mutex_audioTask);
```

### Step 5: begin() — Add startAudioTask() at End
Added after `loadUserCode()` block. Maleksm calls it in both constructor and begin(); the function has a guard so double-calling is safe.

### Step 6: Task Infrastructure — Append 7 Functions
Copied from Maleksm and appended before the closing `#endif`:
- `setAudioTaskCore(uint8_t coreID)` — stops task, changes core, restarts
- `startAudioTask()` — creates FreeRTOS task via `xTaskCreateStaticPinnedToCore`
- `stopAudioTask()` — deletes task with mutex guard
- `taskWrapper(void *param)` — static wrapper calling `audioTask()`
- `audioTask()` — loop: calls `performAudioTask()` every 1ms
- `performAudioTask()` — guards then calls `playAudioData()` with mutex
- `getHighWatermark()` — returns task stack HWM

Also fixed the closing `#endif` comment from `// if I2S_DOUT==255` to `// if defined(USE_AUDIO_VS1053)`.

### Step 7: playAudioData() — Replace Body With Maleksm's
Replaced PR226's simple single-threaded body with Maleksm's task-coordinated version. Adapted variable names:
- `m_dataMode` → `getDatamode()` (PR226 uses getter)
- `m_f_stream` → `m_f_stream_ready` (PR226's equivalent member)

### Step 8: Comment Out All Inline playAudioData() Calls
PR226 calls `playAudioData()` from 10+ places in loop() and processing functions. All must be commented out since the FreeRTOS task is now the sole audio feeder. Each call replaced with comment:
```cpp
// [Maleksm graft] playAudioData(); // task handles this via performAudioTask()
```

### Step 9: Connect Functions — Mutex Guards (Deferred)
Not yet implemented. `connecttohost()` and `connecttoFS()` need `mutex_playAudioData` guards to prevent race between task's `playAudioData()` and loop's connection management. This should be added for production stability.

### Step 10: VU Meter — Replace With Maleksm Improved (with post-graft corrections)

Replaced PR226's VU meter with Maleksm's version with peak-hold decay, then corrected two bugs found during testing:

**Initial graft**: Maleksm's full 0-95 dB range, peak-hold with decay constants.
- `setVUmeter()`: uses `ERRORLOG` instead of `Serial.println`
- `computeVUlevel()`: activated (was commented out in PR226), maps 0-95 to 0-255, adds peak-hold with decay
- `get_VUlevel()`: uses `config.vuThreshold==0` guard

**Bug Fix 1 — `config.vuThreshold==0` chicken-and-egg deadlock**: `get_VUlevel()` checked `config.vuThreshold==0` and returned 0 if true. But `vuThreshold` starts at 0 and is only set by `computeVUlevel()` — which runs AFTER the guard. So `computeVUlevel()` never ran, `vuThreshold` stayed 0 forever, VU meter permanently dead. PR226 had this check **commented out** — Maleksm's version had it active but was never tested (patches never worked in Maleksm). Fix: removed `|| config.vuThreshold==0` from the guard.

**Bug Fix 2 — Wrong mapping range**: Maleksm used `map(reg, 0, 95, 0, 255)` — the full 0-95 dB theoretical range. PR226 used `map(reg, 85, 92, 0, 255)` — the typical MP3 85-92 dB range. Real-world MP3 streams are heavily compressed and rarely go below 85 dB. Maleksm's wider range compressed everything into 85→228 and 92→247 (89-97% of display), making the VU meter always show 80-95%. PR226's narrow range spreads 85-92 across the full display. Fix: changed mapping back to PR226's 85, 92 range. Note: this is NOT the I2S library's approach — I2S computes VU from raw PCM sample amplitude (0-255), not from a dB register.

### Step 11: stopSong() — Add SM_CANCEL Clearing
Added `write_register(SCI_MODE, _BV(SM_SDINEW) | _BV(SM_LINE1))` in two places:
- After successful SM_CANCEL clear (line ~492): prevents stuck bit
- After failed drain loop (line ~499): force-clears even on failure

### Step 12: setDefaults() — Add Lock Coordination
Added before `stopSong()`:
```cpp
m_f_lockInBuffer = false;
m_f_stream_ready = false;
m_f_eof = false;
m_f_ID3v1TagFound = false;
```
These resets ensure the task sees a clean state for each new connection.

### Step 13: options.h — Enable VS_PATCH_ENABLE
Changed default from `false` to `true` in `src/core/options.h`. User should set in myoptions.h for permanent builds.

### Step 14: Build and Test
`sh1106_vs1053_3buttons` compiled successfully:
- RAM: 21.9% (71852/327680 bytes)
- Flash: 25.9% (1699029/6553600 bytes)

### Bug Fix: `m_f_stream_ready` Not Set (Added After Initial Test)
See "Critical Bug" section above. Added at 5 stream-ready locations + resets in setDefaults().

---

## Architecture After Graft

```
┌─────────────────────────────────────────────────────────┐
│  Arduino loop() on Core 1 (Player::loop)                │
│  ┌───────────────────────────────────────────────────┐  │
│  │ Audio::loop()                                     │  │
│  │ • Stream management (parse headers, playlists)    │  │
│  │ • Buffer filling (read from network into InBuff)  │  │
│  │ • Sets m_f_stream_ready when data is available    │  │
│  │ • Sets m_f_lockInBuffer=true when manipulating    │  │
│  │ • NEVER calls playAudioData()                     │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
                        │
                        │ InBuff (shared, mutex-protected)
                        │
┌─────────────────────────────────────────────────────────┐
│  FreeRTOS Task on Core 0 (or configurable)              │
│  ┌───────────────────────────────────────────────────┐  │
│  │ performAudioTask() every 1ms                      │  │
│  │ • Checks m_f_running, m_f_stream_ready, codec     │  │
│  │ • Checks m_f_lockInBuffer (waits if set)          │  │
│  │ • Calls playAudioData() with mutex_audioTask      │  │
│  │ • playAudioData() → sendBytes() → SPI to VS1053   │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## Post-Graft Patch Fixes (2026-07-01)

Two reliability issues found during hardware testing with `VS_PATCH_ENABLE=true`:

### Problem
- Intermittent hung boot: `"patch applied"` printed but `"VS chip: VS1053 done"` never appeared — `read_register(SCI_STATUS)` hung after patch load
- Intermittent no-audio: boot completed but VS1053 DSP wouldn't decode (confirmed by `AICTRL3 = 0x0000`)

### Root Causes
1. **SPI speed too high for patch loading**: Patch was applied at 6.7 MHz, but VS1053B SCI max is CLKI/4 ≈ 3 MHz at 12.288 MHz CLKI. This marginal overclocking could corrupt patch data on some chips.
2. **Chip ID query ran AFTER patch**: `read_register(SCI_STATUS)` at `player.cpp:60` ran after `loadUserCode()` — if the patch corrupted chip state, the register read could hang or return garbage.

### Volume Curve Fix (2026-07-06)

**Problem**: The PR226 library's `VS1053VOL` macro used a log10 curve: `log10(v+1) × 50.5457 + 128`. With `VOLUME_SCALE=42`, the maximum VS1053VOL output only reached 210 out of 254 — the SCI_VOL register never got close to 0x00 (maximum volume). Max volume was significantly quieter than the old library which used 0..255 input range.

**Fix**: Replaced the log10 curve with user-selectable VOLUME_SCALE-aware cubic polynomials. Three options controlled by compile-time defines in `myoptions.h`:

| Define | Formula | Starts at (t=0) | Use case |
|--------|---------|-----------------|----------|
| `VS1053_VOL_LOG` | `log10(scaled+1) × 50.5457 + 128` | N/A (register units) | Gentlest low-end, slowest rise |
| `VS1053_VOL_CURVE` | `−112t³ + 172t² − 60` | −60 dB | Amplified VS1053 (matches I2S) |
| _(default, no define)_ | `−65t³ + 100t² − 35` | −35 dB | Unamplified VS1053 |

All curves use `t = v / VOLUME_SCALE` to normalize to 0..1, ensuring the full SCI_VOL register range (0x00 loudest) is reachable regardless of VOLUME_SCALE value.

**Tuning the default curve**: Change `c` (the last coefficient, −35). Then `b = −c / 0.35`, `a = −(b + c)`. Lower |c| = louder at low volumes. Found by testing that c=−70 was inaudible below ~12/42, c=−35 makes quarter volume clearly audible.

**File changed**: `src/libraries/VS1053_Audio/audioVS1053Ex.h` — `VS1053VOL` macro (lines 12-64).

---

### Fixes Applied
| # | File | Change |
|---|------|--------|
| 1 | `audioVS1053Ex.cpp` `begin()` | Moved chip ID query before `loadUserCode()` — reads `SCI_STATUS` bits 4-7 while chip is in known-good reset state |
| 2 | `audioVS1053Ex.cpp` `begin()` | Patch loaded at 6.7 MHz (same as data, same as PR226 upstream). 200 kHz was tried but caused patch failure — VS1053B's patch-loading protocol has a timeout; bytes arriving too slowly cause the state machine to fail. |
| 3 | `player.cpp` | Removed duplicate chip ID query (now handled inside `begin()`) |

### Template for Future Grafts
When porting to a new library version, any `if(VS_PATCH_ENABLE) { loadUserCode(); }` block should:
1. Query chip type via `read_register(SCI_STATUS)` **before** the block
2. Load the patch at the same SPI speed as normal data (6.7 MHz) — VS1053B patch-loading protocol has a timeout; speeds below ~1 MHz may fail
3. Print success/failure log lines inside the block

### Hardware Notes
- **Brown-out**: Fast power-cycling can leave VS1053 in undefined state. Large bulk capacitors on modules hold charge >10 seconds. If needed, add hardware reset supervisor or RC delay on RST pin.
- **Factory rejects**: Boards that decode AAC (proving VS1053B silicon) but fail patching are likely factory seconds with defective WRAM or DSP coprocessor. These chips pass basic testing (ROM codecs, SPI, register map) but fail when VS1053B-specific features are exercised. Common from gray market sellers.

---

## Post-Graft Patch Fixes (2026-08-12)

### SD→VS1053 Pops (Double-Feed Bug)

**Symptom**: Occasional pops during SD card → VS1053 playback, absent on online streams.

**Root cause**: The graft made `processWebStream()` fill-only (task feeds the VS1053), but `processLocalFile()` (the SD path) was left with its PR226 inline `sendBytes()` calls. Both Core 1 (`processLocalFile()`) and Core 0 (`playAudioData()` task) advanced the `InBuff` read pointer and sent data to the VS1053 without synchronization — a double-feed race producing duplicated/skipped MP3 frames.

Online streams didn't pop because `processWebStream()` was correctly fill-only.

**Fix**: Removed both inline `sendBytes()` calls from `processLocalFile()` (the `else` branch and the EOF tail-flush). `processLocalFile()` is now fill-only; the FreeRTOS task's `playAudioData()` is the sole sender.

**Files changed**: `src/libraries/VS1053_Audio/audioVS1053Ex.cpp` — `processLocalFile()`.

**Note**: The EOF tail-flush relied on the removed inline `sendBytes()`. The task's `lastFrame` logic in `playAudioData()` is intended to send the final partial block, but `processLocalFile()` now calls `stopSong()` immediately on EOF without waiting for the task to drain the tail. Watch for truncated end-of-file audio during testing.

---

## Open Items

1. **Mutex guards on connect functions** (Step 9 deferred): `connecttohost()` and `connecttoFS()` should be wrapped with `mutex_playAudioData` to prevent race conditions. Same pattern Maleksm used.

2. **`m_validSamples` in performAudioTask()**: The `while(m_validSamples)` loop is an I2S leftover. For VS1053, `m_validSamples` is initialized to 0 and never incremented, so this loop never executes. Safe to leave as-is or remove.

3. **Core assignment**: `m_audioTaskCoreId` defaults to 1. Maleksm recommends opposite core from Arduino loop. For ESP32-S3 where loop runs on Core 1, task should be on Core 0. Adjust via `setAudioTaskCore(0)`.

4. **Future comparison recommendations**: When migrating to a new upstream library version, always compare:
   - New upstream vs `VS1053_Audio (ehRadio nsteplanets yoRadio PR226)` — what changed in the audio core?
   - New upstream vs `VS1053_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` — what Maleksm changes need re-grafting?
   - Then re-apply the graft steps documented here, adapting for any API changes.

---

## Notes For Future Use

`Maleksm's original` = `VS1053_Audio (yoRadio Maleksm v0.9.434m(04.04.25))` — from Maleksm's yoRadio mod: https://4pda.to/forum/index.php?showtopic=1010378&st=11240#entry125839228

`Active` (old ehRadio) = `VS1053_Audio (ehRadio Maleksm v0.9.434m(04.04.25))` — Maleksm adapted for ehRadio

`ehRadio PR226 copy` = `VS1053_Audio (ehRadio nsteplanets yoRadio PR226)` — PR226 adapted for ehRadio

Graft result = `src/libraries/VS1053_Audio/` — current production library

Someday these files may be needed again...
