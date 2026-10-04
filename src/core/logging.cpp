#include "options.h"
#include "logging.h"
#include <stdarg.h>
#include <string.h>
#include "telnet.h"
#ifdef SAVE_LOGS_TO_FS
  #include <LittleFS.h>
  #include <time.h>
  #include <esp_heap_caps.h>
  #include <freertos/FreeRTOS.h>
  #include <freertos/semphr.h>
#endif

namespace {

void emitLogMessage(const char* category, bool appendNewline, const char* fmt, va_list args) {
  if (!fmt) return;

  char logBuffer[LOG_BUF_LEN] = {0};
  size_t prefixLen = 0;

  if (category && category[0] != '\0') {
    // Pad bracketed category to 16 chars for aligned columns (e.g. "[PSRAM]       ")
    char catBuf[20];
    snprintf(catBuf, sizeof(catBuf), "[%s]", category);
    int written = snprintf(logBuffer, sizeof(logBuffer), "%-16s", catBuf);
    if (written > 0) {
      prefixLen = static_cast<size_t>(written);
      if (prefixLen >= sizeof(logBuffer)) {
        prefixLen = sizeof(logBuffer) - 1;
      }
    }
  }

  #ifdef BOOTLOG_TIME
    if (appendNewline && category && (strcmp(category, "BOOT") == 0 && prefixLen < sizeof(logBuffer))) {
      int written = snprintf(logBuffer + prefixLen, sizeof(logBuffer) - prefixLen, "%05lums: ", (unsigned long)millis());
      if (written > 0) {
        prefixLen += static_cast<size_t>(written);
        if (prefixLen >= sizeof(logBuffer)) {
          prefixLen = sizeof(logBuffer) - 1;
        }
      }
    }
  #endif

  if (prefixLen < sizeof(logBuffer)) {
    vsnprintf(logBuffer + prefixLen, sizeof(logBuffer) - prefixLen, fmt, args);
  }

  if (appendNewline) {
    // Everything the firmware logs passes through here, so these two calls are the whole capture
    #ifdef SAVE_LOGS_TO_FS
      const bool critical = category && (!strcmp(category, "ERROR") || !strcmp(category, "Network") ||
                                         !strcmp(category, "Player") || !strcmp(category, "Services"));
      logRingWrite(logBuffer, true, critical);
    #endif
    logToTelnetLine(logBuffer);
    size_t outLen = strlen(logBuffer);
    if (outLen + 2 < sizeof(logBuffer)) {
      logBuffer[outLen++] = '\r';
      logBuffer[outLen++] = '\n';
    }
    Serial.write(reinterpret_cast<const uint8_t*>(logBuffer), outLen);
  } else {
    #ifdef SAVE_LOGS_TO_FS
      logRingWrite(logBuffer, false, false);   // a fragment: held until the line that closes the run arrives
    #endif
    logToTelnetRaw(logBuffer);
    Serial.write(reinterpret_cast<const uint8_t*>(logBuffer), strlen(logBuffer));
  }
}

} // namespace

void logToTelnetLine(const char* text) {
  telnet.logLine(text);
}

void logToTelnetRaw(const char* text) {
  telnet.logRaw(text);
}

void serialLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(nullptr, true, fmt, args);
  va_end(args);
}

void serialLogX(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(nullptr, false, fmt, args);
  va_end(args);
}

void functionLog(const char* category, const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage(category, true, fmt, args);
  va_end(args);
}

void bootLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("BOOT", true, fmt, args);
  va_end(args);
}

void bootLogX(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("BOOT", false, fmt, args);
  va_end(args);
}

void errorLog(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  emitLogMessage("ERROR", true, fmt, args);
  va_end(args);
}

void serialLogDot() {
  #ifdef SAVE_LOGS_TO_FS
    logRingWrite(".", false, false);
  #endif
  Serial.print(".");
}

void serialLogLf() {
  serialLog("%s", ""); // blank line (or line-ending)
}

// Boot stage timing - see logging.h. Three separate stamps on purpose: each helper measures against its own previous
// call, so the config and LittleFS markers inside a setup() stage do not disturb the setup() deltas (a single shared
// stamp would redefine every number in the log). Both the line and the stamp sit inside the #ifdef, so without
// BOOTLOG_TIME these are empty calls: no stage lines, and nothing else changes.
#ifdef BOOTLOG_TIME
  static uint32_t _bootTimeAt = 0;    // last BOOTTIMELOG   marker
  static uint32_t _fsTimeAt = 0;  // last LITTLEFSTIMELOG marker
  static uint32_t _configTimeAt = 0;  // last CONFIGTIMELOG marker
#endif

void bootTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("Boot: %-30s %6lums", name, (unsigned long)(now - _bootTimeAt));
    _bootTimeAt = now;
  #else
    (void)name;
  #endif
}

void littleFsTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("LittleFS: %-30s %6lums", name, (unsigned long)(now - _fsTimeAt));
    _fsTimeAt = now;
  #else
    (void)name;
  #endif
}

void configTimeLog(const char* name) {
  #ifdef BOOTLOG_TIME
    const uint32_t now = millis();
    BOOTLOG("Config: %-30s %6lums", name, (unsigned long)(now - _configTimeAt));
    _configTimeAt = now;
  #else
    (void)name;
  #endif
}

void littleFsTimeLogReset() {
  #ifdef BOOTLOG_TIME
    _fsTimeAt = millis();
  #endif
}

void configTimeLogReset() {
  #ifdef BOOTLOG_TIME
    _configTimeAt = millis();
  #endif
}

// ===== log ring: everything the loggers see, saved to LittleFS (SAVE_LOGS_TO_FS) =====
#ifdef SAVE_LOGS_TO_FS

namespace {

constexpr uint8_t  RING_FILES      = 10;            // /logs/log0.txt .. log9.txt
constexpr size_t   RING_PSRAM      = 16 * 1024;     // staging ring when PSRAM is present: free of internal heap
constexpr size_t   RING_INTERNAL   = 2 * 1024;      // fallback: the internal heap has to host the TLS clients
constexpr size_t   STAGE_BYTES     = 256;           // one flash write's worth, internal DRAM (see _stage)
constexpr size_t   PENDING_BYTES   = 192;           // fragment runs join the next line; longer ones are truncated
constexpr uint32_t FLUSH_MAX_MS    = 15;            // bound one flush so loop() cannot stall on a sector erase
constexpr uint32_t FLUSH_PERIOD_MS = 250;           // ordinary flush cadence from loop()
constexpr uint32_t SERVE_IDLE_MS   = 5000;          // a download that reads nothing for this long is abandoned
constexpr uint32_t IO_LOCK_WAIT_MS = 40;            // how long a /log chunk waits for the flash lock before giving up
constexpr uint32_t CLOCK_MIN_EPOCH = 1600000000UL;  // below this the clock was never set, so stamp millis

const char* const LOG_DIR  = "/logs";
const char* const IDX_PATH = "/logs/idx";

portMUX_TYPE _mux = portMUX_INITIALIZER_UNLOCKED;

uint8_t* _buf = nullptr;
size_t   _cap = 0;
uint16_t _head = 0, _tail = 0;   // byte positions, wrapping at _cap
uint32_t _dropped = 0;
uint32_t _perFile = 0;
uint8_t  _cur = 0;
bool     _ready = false;         // mounted and state read: flushing is allowed
bool     _disabled = false;      // no buffer, no room, or the FS refused us: off for this boot
bool     _saidDrop = false;
uint32_t _lastFlushMs = 0;

// Fragments waiting for the line that closes them (X logger text, progress dots).  The stamp is taken when the
// run opens, so the timestamp marks when the work began rather than when the newline arrived.
char     _pending[PENDING_BYTES];
uint16_t _pendingLen = 0;

// Flash writes run with the cache disabled and PSRAM is unreachable then, so every write is staged through this
// internal-DRAM buffer: a PSRAM pointer must never reach File::write().
uint8_t _stage[STAGE_BYTES];

// ONE lock for every LittleFS touch in the ring, and no handle cached across calls.  The old design kept the
// download's file open between chunk callbacks and let flush()/snapshot()/clear() close that handle from other
// tasks, so a seek could land on a handle littlefs no longer had open - which is exactly what lfs_file_seek
// asserts on.  Every operation now opens what it needs, uses it and closes it before returning, with this mutex
// excluding the others.  A real mutex, never a critical section: flash I/O must not run with interrupts disabled.
SemaphoreHandle_t _ioLock = nullptr;

bool ioLockTake(uint32_t waitMs) {
  return _ioLock && xSemaphoreTake(_ioLock, pdMS_TO_TICKS(waitMs)) == pdTRUE;
}
void ioLockGive() { if (_ioLock) xSemaphoreGive(_ioLock); }

// A /log download: its layout is written once per request, and these two fields are the only cross-task state the
// reader keeps.  flush() uses them to leave the oldest slot alone while a client is reading through it; a request
// that reads nothing for SERVE_IDLE_MS is treated as abandoned and stops protecting the slot.
uint32_t _serveLastMs = 0;
bool     _serveActive = false;

void pathOf(char* out, size_t n, uint8_t slot) { snprintf(out, n, "%s/log%u.txt", LOG_DIR, (unsigned)slot); }

bool downloadLive() { return _serveActive && (uint32_t)(millis() - _serveLastMs) <= SERVE_IDLE_MS; }

uint16_t ringUsed() {
  const uint16_t h = _head, t = _tail;
  return (h >= t) ? (uint16_t)(h - t) : (uint16_t)(_cap - t + h);
}

uint16_t ringFree() { return (uint16_t)(_cap - 1 - ringUsed()); }

// PSRAM first, internal RAM as the fallback (as vuScratchAlloc() does in the audio library), allocated once and
// never reallocated: PSRAM fragments like DRAM and the audio library is already filling it.
bool allocRing() {
  _cap = RING_PSRAM;
  _buf = static_cast<uint8_t*>(heap_caps_malloc(_cap, MALLOC_CAP_SPIRAM));
  if (!_buf) { _cap = RING_INTERNAL; _buf = static_cast<uint8_t*>(heap_caps_malloc(_cap, MALLOC_CAP_8BIT)); }
  if (!_buf) { _cap = 0; _disabled = true; return false; }
  _head = _tail = 0;
  return true;
}

// Make room by dropping the OLDEST whole lines: the newest are the evidence worth keeping.
void dropForRoom(uint16_t need) {
  while (ringFree() <= need && ringUsed() > 0) {
    uint16_t t = _tail;
    while (t != _head && _buf[t] != '\n') t = (uint16_t)((t + 1) % _cap);
    if (t != _head) t = (uint16_t)((t + 1) % _cap);   // step past the newline
    _tail = t;
    _dropped++;
  }
}

// Called under _mux.  Copies out; the flash call itself stays outside every lock.
size_t takeFromRing(size_t maxLen) {
  size_t n = ringUsed();
  if (n == 0) return 0;
  if (n > maxLen) n = maxLen;
  if (_tail + n > _cap) n = _cap - _tail;             // never straddle the wrap in one go
  memcpy(_stage, _buf + _tail, n);
  _tail = (uint16_t)((_tail + n) % _cap);
  return n;
}

// saveIdx(), ensureFile(), currentSize() and rotate() are for callers that already hold _ioLock - flush(),
// logRingInit() and logRingClear() - and must never take it themselves: it is not a recursive mutex.
void saveIdx() {
  File f = LittleFS.open(IDX_PATH, "w");
  if (!f) return;
  f.printf("%u\n", (unsigned)_cur);
  f.close();
}

void ensureFile() {
  char p[24];
  pathOf(p, sizeof(p), _cur);
  if (!LittleFS.exists(p)) { File f = LittleFS.open(p, "a"); if (f) f.close(); }
}

uint32_t currentSize() {
  char p[24];
  pathOf(p, sizeof(p), _cur);
  File f = LittleFS.open(p, "r");
  if (!f) return 0;
  const uint32_t sz = (uint32_t)f.size();
  f.close();
  return sz;
}

// Entering a slot deletes it: its content is the oldest in the ring, which keeps one generation of history and
// makes (cur + 1) mod N the oldest file.  Callers hold _ioLock, and call it only when no download is reading the
// ring (flush() checks), because the slot being removed is the one a live download starts from.
void rotate() {
  _cur = (uint8_t)((_cur + 1) % RING_FILES);
  char p[24];
  pathOf(p, sizeof(p), _cur);
  LittleFS.remove(p);
  saveIdx();
  ensureFile();
}

void flush(bool force) {
  if (!_ready || _disabled || !_buf) return;
  const uint32_t now = millis();
  if (!force) {
    if (ringUsed() == 0) return;
    if (now - _lastFlushMs < FLUSH_PERIOD_MS) return;
  }
  _lastFlushMs = now;

  // A download is reading through these files.  No handle is held open between its chunks any more, so an append
  // landing between two of them is safe - but the rotate below is not: it deletes the oldest slot, which is where a
  // live download is reading.  So this waits while one is alive and the RAM ring absorbs the delay; a request that
  // has read nothing for SERVE_IDLE_MS is treated as abandoned and stops protecting its slot.
  const bool live = downloadLive();
  if (!live && _serveActive) _serveActive = false;

  // One flasher at a time, and never blocking: a critical line can arrive from the player, the network or the audio
  // task, so two flushes used to interleave their appends and each advance _cur on its own.  If the lock is busy this
  // pass is skipped, and the ring simply keeps the bytes for the next one.
  if (!ioLockTake(0)) return;

  char p[24];
  pathOf(p, sizeof(p), _cur);
  File f = LittleFS.open(p, "a");
  if (!f) {
    _disabled = true;   // no point retrying every loop; the mount may be gone
    FUNCTIONLOG("Logs", "cannot open %s - file logging stopped for this boot", p);
    ioLockGive();
    return;
  }

  const uint32_t start = millis();
  bool ioError = false;
  for (;;) {
    portENTER_CRITICAL(&_mux);
    const size_t n = takeFromRing(STAGE_BYTES);
    portEXIT_CRITICAL(&_mux);
    if (n == 0) break;
    if (f.write(_stage, n) != n) { ioError = true; break; }
    if (millis() - start > FLUSH_MAX_MS) break;   // keep loop() responsive; the rest waits for the next call
  }
  f.close();
  if (ioError) {
    _disabled = true;
    FUNCTIONLOG("Logs", "write failed - file logging stopped for this boot");
    ioLockGive();
    return;
  }

  // _perFile is non-zero here (init disables logging otherwise): a zero cap would rotate on every flush.  A rotate
  // deletes the slot it enters - the oldest - so it waits for a download that is still reading through the ring;
  // the size test simply fires again on a later flush once that download has finished or been abandoned.
  if (_perFile && !live && currentSize() >= _perFile) rotate();

  ioLockGive();

  if (_dropped && !_saidDrop) {
    _saidDrop = true;
    FUNCTIONLOG("Logs", "%u lines dropped (ring full at %u bytes)", (unsigned)_dropped, (unsigned)_cap);
  }
}

// The stamp for one line: wall-clock when the clock is real, millis when it is not (a boot line).
size_t stampInto(char* out, size_t room) {
  const time_t now = time(nullptr);
  struct tm tmv;
  if (now > (time_t)CLOCK_MIN_EPOCH && localtime_r(&now, &tmv)) {
    const size_t n = strftime(out, room, "%Y-%m-%d %H:%M:%S\t", &tmv);
    if (n) return n;
  }
  return (size_t)snprintf(out, room, "%8lums\t", (unsigned long)millis());
}

// One finished line (newline included) into the ring.  A line is far smaller than the ring, so one wrap at most.
void ringPush(const char* line, size_t len) {
  portENTER_CRITICAL(&_mux);
  dropForRoom((uint16_t)len);
  if (ringFree() > len) {
    size_t room = _cap - _head;
    if (room > len) room = len;
    memcpy(_buf + _head, line, room);
    if (room < len) memcpy(_buf, line + room, len - room);
    _head = (uint16_t)((_head + len) % _cap);
  } else {
    _dropped++;   // a single line longer than the whole ring
  }
  portEXIT_CRITICAL(&_mux);
}

}  // namespace

void logRingWrite(const char* text, bool complete, bool critical) {
  if (!text) return;
  if (_disabled) return;   // also the recursion guard: the Logs lines below reach back here
  if (!_buf && !allocRing()) return;

  // A complete line goes out on its own; a fragment joins the run it belongs to and rides out with the line that
  // closes it, so the file reads the way serial did.
  char out[LOG_BUF_LEN + PENDING_BYTES + 40];
  size_t n = 0;
  bool haveLine = false;
  const size_t add = strlen(text);

  // Clamp first: another task finishing a line while a ticker writes a fragment must not walk past _pending.
  if (_pendingLen > PENDING_BYTES - 1) _pendingLen = (uint16_t)(PENDING_BYTES - 1);

  if (_pendingLen) {
    const size_t room = (size_t)(PENDING_BYTES - 1 - _pendingLen);
    const size_t take = (add > room) ? room : add;
    memcpy(_pending + _pendingLen, text, take);
    _pendingLen = (uint16_t)(_pendingLen + take);
    if (complete) {   // the run closes: it already carries the stamp from when it opened
      memcpy(out, _pending, _pendingLen);
      n = _pendingLen;
      _pendingLen = 0;
      haveLine = true;
    }
  } else if (complete) {
    n = stampInto(out, sizeof(out));
    const size_t room = sizeof(out) - 1 - n;
    const size_t take = (add > room) ? room : add;
    memcpy(out + n, text, take);
    n += take;
    haveLine = true;
  } else {
    n = stampInto(_pending, sizeof(_pending));   // opening a run: the stamp is when the work started
    const size_t room = (size_t)(PENDING_BYTES - 1 - n);
    const size_t take = (add > room) ? room : add;
    memcpy(_pending + n, text, take);
    _pendingLen = (uint16_t)(n + take);
  }

  if (!haveLine) return;   // still a fragment: nothing reaches the ring yet
  out[n++] = '\n';
  ringPush(out, n);
  // The last lines before a reboot are the ones that matter, so a critical line skips the cadence.
  if (critical) flush(true);
}

void logRingInit() {
  if (!_buf && !allocRing()) return;
  if (!_ioLock) {
    _ioLock = xSemaphoreCreateMutex();
    if (!_ioLock) { _disabled = true; return; }   // without the lock nothing here may touch the files at all
  }
  if (!ioLockTake(pdMS_TO_TICKS(500))) { _disabled = true; return; }
  if (!LittleFS.exists(LOG_DIR)) LittleFS.mkdir(LOG_DIR);

  // Where the rotating window is.  Without /logs/idx (first boot, manual delete) the highest-numbered existing
  // file is the best available guess at the newest one.
  _cur = 0;
  File idx = LittleFS.open(IDX_PATH, "r");
  if (idx) {
    const int v = idx.parseInt();
    if (v >= 0 && v < RING_FILES) _cur = (uint8_t)v;
    idx.close();
  } else {
    for (uint8_t i = 0; i < RING_FILES; i++) {
      char p[24];
      pathOf(p, sizeof(p), i);
      if (LittleFS.exists(p)) _cur = i;
    }
  }
  saveIdx();
  ensureFile();

  // The reserve is exactly what netserver needs free to keep serving search and curated downloads - below it
  // those are refused - so the ring leaves that much untouched.
  const size_t reserve = (size_t)FS_REQUIRED_FREE_SPACE * 1024;
  const size_t total   = LittleFS.totalBytes();
  _perFile = (total > reserve) ? (uint32_t)((total - reserve) / RING_FILES) : 0;
  if (_perFile && _perFile < 4096) _perFile = 4096;   // never rotate on a handful of lines

  _ready = true;
  ioLockGive();

  if (_perFile == 0) {   // not one file's worth of room: off for this boot rather than rotating in a spiral
    _disabled = true;
    FUNCTIONLOG("Logs", "no room to log: %u bytes total vs a %u KB reserve", (unsigned)total, (unsigned)(reserve / 1024));
    return;
  }
  FUNCTIONLOG("Logs", "saving to %s: %u files, %u bytes each, reserve %u KB, ring %u bytes in %s",
              LOG_DIR, (unsigned)RING_FILES, (unsigned)_perFile, (unsigned)(reserve / 1024),
              (unsigned)_cap, psramFound() ? "PSRAM" : "internal RAM");
}

void logRingFlush() { flush(false); }

uint32_t logRingPending() {
  if (!_buf) return 0;
  portENTER_CRITICAL(&_mux);
  const uint32_t n = ringUsed();
  portEXIT_CRITICAL(&_mux);
  return n;
}

uint32_t logRingDropped() { return _dropped; }

// The layout is taken once per request, so bytes appended while a client downloads cannot shift an offset.  Read
// order is the ring's own: (cur + 1), (cur + 2) ... cur, modulo N, missing files skipped.
namespace {
struct SnapSlot { uint8_t slot; size_t size; };
SnapSlot _snap[RING_FILES];
uint8_t  _snapCount = 0;
size_t   _snapTotal = 0;
}  // namespace

size_t logRingSnapshot() {
  _snapCount = 0;
  _snapTotal = 0;
  if (!_ready || !ioLockTake(IO_LOCK_WAIT_MS)) return 0;
  for (uint8_t i = 1; i <= RING_FILES; i++) {   // i == RING_FILES lands on _cur, the newest
    const uint8_t s = (uint8_t)((_cur + i) % RING_FILES);
    char p[24];
    pathOf(p, sizeof(p), s);
    File f = LittleFS.open(p, "r");
    if (!f) continue;
    const size_t sz = f.size();
    f.close();
    if (sz == 0) continue;
    _snap[_snapCount].slot = s;
    _snap[_snapCount].size = sz;
    _snapTotal += sz;
    _snapCount++;
  }
  ioLockGive();
  _serveActive = (_snapCount > 0);   // this request owns the layout until the loader walks past it
  _serveLastMs = millis();
  return _snapTotal;
}

size_t logRingReadAt(size_t offset, uint8_t* out, size_t maxLen) {
  if (!out || !maxLen) return 0;
  size_t skip = offset;
  for (uint8_t i = 0; i < _snapCount; i++) {
    if (skip >= _snap[i].size) { skip -= _snap[i].size; continue; }
    size_t avail = _snap[i].size - skip;
    if (avail > maxLen) avail = maxLen;
    // Open, seek, read and close inside this one call: the handle never outlives the callback, so no other task can
    // close it and no rotate can remove the file under it.  The lock keeps this read out of the middle of a flush's
    // append or a rotate's remove, and the wait is bounded so a stalled flash op cannot wedge the network task.
    size_t got = 0;
    if (ioLockTake(IO_LOCK_WAIT_MS)) {
      char p[24];
      pathOf(p, sizeof(p), _snap[i].slot);
      File f = LittleFS.open(p, "r");
      if (f) {
        if (f.seek(skip, SeekSet)) got = f.read(out, avail);
        f.close();
      }
      ioLockGive();
    }
    _serveLastMs = millis();     // tells flush() this download is still alive, so it keeps waiting
    _serveActive = (got > 0);    // a failed open or read here is the end of this response
    return got;
  }
  _serveActive = false;          // the loader walked past the snapshot: this download is done
  return 0;
}

// Returns the bytes wiped, so the caller does not have to snapshot first - that snapshot was what used to rewrite
// the layout a live download was reading.
size_t logRingClear() {
  size_t had = 0;
  _serveActive = false;
  if (_ready && ioLockTake(pdMS_TO_TICKS(200))) {
    char p[24];
    for (uint8_t i = 0; i < RING_FILES; i++) {
      pathOf(p, sizeof(p), i);
      File f = LittleFS.open(p, "r");
      if (f) { had += f.size(); f.close(); }
      LittleFS.remove(p);
    }
    LittleFS.remove(IDX_PATH);
    ioLockGive();
  }
  portENTER_CRITICAL(&_mux);
  _head = _tail = 0;
  _dropped = 0;
  portEXIT_CRITICAL(&_mux);
  _saidDrop = false;
  _snapCount = 0;
  _snapTotal = 0;
  _cur = 0;
  if (_ready && ioLockTake(pdMS_TO_TICKS(200))) {
    saveIdx();
    ensureFile();
    ioLockGive();
  }
  return had;
}

// True while a /log download is reading the current snapshot.  A second request is refused rather than sharing the
// layout the first one is using (and rather than stealing its offsets) - the same "one at a time" rule the SD card
// manager applies to a delete.
bool logRingServeBusy() { return downloadLive(); }

// The danger zone is about to format the filesystem: get the last lines out, then stop touching the files for the
// rest of this boot.  Every entry point checks _disabled first, so a format cannot race the ring afterwards.
void logRingShutdown() {
  flush(true);
  _serveActive = false;
  _disabled = true;
}

#else

void     logRingWrite(const char*, bool, bool) {}
void     logRingInit() {}
void     logRingFlush() {}
uint32_t logRingPending() { return 0; }
uint32_t logRingDropped() { return 0; }
size_t   logRingSnapshot() { return 0; }
size_t   logRingReadAt(size_t, uint8_t*, size_t) { return 0; }
bool     logRingServeBusy() { return false; }
size_t   logRingClear() { return 0; }
void     logRingShutdown() {}

#endif  // SAVE_LOGS_TO_FS
