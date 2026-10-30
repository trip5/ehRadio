#include "options.h"
#include <LittleFS.h>
#include <esp_system.h>
#include <esp_attr.h>
#include <time.h>
#include <string.h>
#include "coredump.h"
#include "crashreport.h"
#include "config.h"
#include "logging.h"

namespace {

const char* LOG_DIR    = "/data";
const char* LOG_PREFIX = "/data/coredump.";

#if defined(FIRMWARE)
  const char* kFirmware = FIRMWARE;         // the .bin name, when the build defines it
#elif defined(FIRMWARE_NAME)
  const char* kFirmware = FIRMWARE_NAME;    // the friendly name from myoptions.h
#else
  const char* kFirmware = "unknown firmware";
#endif

// A note written by the heap integrity walk BEFORE a panic.  RTC_DATA_ATTR lives in RTC slow memory, which the panic's
// software reset (RTC_SW_CPU_RST) leaves alone - the same mechanism recoveryReboots relies on.  It is printed above the
// crashdump text, because the interesting fact is that the heap was already broken before the allocation that died.
RTC_DATA_ATTR static char    g_heapNote[160];
RTC_DATA_ATTR static uint8_t g_heapNoteSet = 0;

// A wall clock we can believe: anything after 2020-09 is a real sync, anything below is the chip's power-on default.
// Not trusted by presence of a define: this is the timestamp written into the file, so it says what it is.
bool clockUsable(time_t now) { return now > 1600000000; }

const char* resetReasonName() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:   return "power-on";
    case ESP_RST_EXT:       return "external reset";
    case ESP_RST_SW:        return "software restart";
    case ESP_RST_PANIC:     return "panic";
    case ESP_RST_INT_WDT:   return "interrupt watchdog";
    case ESP_RST_TASK_WDT:  return "task watchdog";
    case ESP_RST_WDT:       return "other watchdog";
    case ESP_RST_DEEPSLEEP: return "deep sleep wake";
    case ESP_RST_BROWNOUT:  return "brownout";
    case ESP_RST_SDIO:      return "SDIO";
    default:                return "unknown";
  }
}

// "/data/coredump.12" (or a bare "coredump.12") -> 12.  0 when the name is not one of ours.
uint16_t indexFromName(const char* n) {
  if (!n) return 0;
  const char* slash = strrchr(n, '/');
  const char* base  = slash ? slash + 1 : n;
  if (strncmp(base, "coredump.", 9) != 0) return 0;
  const char* num = base + 9;
  if (!*num) return 0;
  for (const char* p = num; *p; p++) if (*p < '0' || *p > '9') return 0;
  const long v = atol(num);
  return (v > 0 && v <= 65535) ? (uint16_t)v : 0;
}

struct Ring {
  uint16_t maxIdx = 0;
  uint16_t minIdx = 0;
  uint8_t  count  = 0;
};

Ring scanRing() {
  Ring r;
  File dir = LittleFS.open(LOG_DIR);
  if (!dir || !dir.isDirectory()) return r;
  while (true) {
    File f = dir.openNextFile();
    if (!f) break;
    const uint16_t idx = indexFromName(f.name());
    if (idx) {
      r.count++;
      if (idx > r.maxIdx) r.maxIdx = idx;
      if (!r.minIdx || idx < r.minIdx) r.minIdx = idx;
    }
    f.close();
  }
  dir.close();
  return r;
}

void pathFor(uint16_t idx, char* out, size_t len) { snprintf(out, len, "%s%u", LOG_PREFIX, (unsigned)idx); }

// Ascending list of the indices present, newest last.  Falls back to nothing when the directory is unreadable.
uint8_t listIndices(uint16_t* out, uint8_t cap) {
  uint8_t n = 0;
  File dir = LittleFS.open(LOG_DIR);
  if (!dir || !dir.isDirectory()) return 0;
  while (true) {
    File f = dir.openNextFile();
    if (!f) break;
    const uint16_t idx = indexFromName(f.name());
    f.close();
    if (!idx || n >= cap) continue;
    uint8_t at = n++;                     // insertion sort: a handful of entries, newest last
    while (at > 0 && out[at - 1] > idx) { out[at] = out[at - 1]; at--; }
    out[at] = idx;
  }
  dir.close();
  return n;
}

}  // namespace

namespace coredump {

uint8_t count() { return scanRing().count; }

uint8_t clear() {
  uint16_t idx[32];
  const uint8_t n = listIndices(idx, 32);
  uint8_t removed = 0;
  for (uint8_t i = 0; i < n; i++) {
    char path[32];
    pathFor(idx[i], path, sizeof(path));
    if (LittleFS.remove(path)) removed++;
  }
  if (removed) BOOTLOG("CoreDump: cleared %u log(s) left by the previous firmware", (unsigned)removed);
  return removed;
}

void noteHeapCorruption(const char* text) {
  if (!text) return;
  if (g_heapNoteSet && strcmp(g_heapNote, text) == 0) return;   // one note per distinct reading, not one per cycle
  strlcpy(g_heapNote, text, sizeof(g_heapNote));
  g_heapNoteSet = 1;
}

bool archive() {
  if (!crashDumpAvailable()) return false;   // the overwhelmingly common case: nothing was written, nothing to do

  Ring r = scanRing();
  // Keep exactly KEEP files: when the ring is full the lowest number goes before the new one is written, and if it
  // cannot be deleted the write is abandoned rather than allowed to grow the ring.
  if (r.count >= KEEP) {
    char oldPath[32];
    pathFor(r.minIdx, oldPath, sizeof(oldPath));
    if (!LittleFS.remove(oldPath)) {
      ERRORLOG("CoreDump: cannot delete %s - not writing a new log over a full ring", oldPath);
      return false;
    }
    BOOTLOG("CoreDump: ring full, removed %s", oldPath);
  }

  const uint16_t next = (r.maxIdx < 65535) ? (uint16_t)(r.maxIdx + 1) : (uint16_t)65535;
  char path[32];
  pathFor(next, path, sizeof(path));

  File f = LittleFS.open(path, "w");
  if (!f) {
    ERRORLOG("CoreDump: cannot open %s for writing - the dump stays in flash for the next boot", path);
    return false;
  }

  // Two times, because they are not the same one and saying only the second one misleads: this file is written a few
  // seconds AFTER the reboot, so stamping it with the current time makes the crash look later than it was.
  const time_t now = time(nullptr);
  const unsigned long sinceCrash = (unsigned long)(millis() / 1000UL);
  char stamp[128];
  if (clockUsable(now)) {
    const time_t crash = now - (time_t)sinceCrash;
    struct tm tmc, tma;
    localtime_r(&crash, &tmc);
    localtime_r(&now, &tma);
    char crashed[24], archived[24];
    strftime(crashed, sizeof(crashed), "%Y-%m-%d %H:%M:%S", &tmc);
    strftime(archived, sizeof(archived), "%Y-%m-%d %H:%M:%S", &tma);
    snprintf(stamp, sizeof(stamp), "crashed ~%s, archived %s (%lus later, clock verified)",
             crashed, archived, sinceCrash);
  } else {
    snprintf(stamp, sizeof(stamp), "clock unverified, archived %lus after the crash", sinceCrash);
  }

  // What the device was doing when it died, as persisted by that boot - the mode and the station survive the crash,
  // which is why they are worth recording here.  The build that crashed is named by the elf sha256 in the body below.
  char header[288];
  snprintf(header, sizeof(header), "%s | next boot ran because of: %s (%d) | at crash: %s mode, station %u \"%s\"",
           stamp, resetReasonName(), (int)esp_reset_reason(),
           (config.getMode() == PM_SDCARD) ? "SD" : "web",
           (unsigned)config.lastStation(), config.station.name);

  bool wrote = (f.printf("%s\n", header) > 0);
  // A note the integrity walk left behind before the panic belongs above the dump: it says the heap was already broken
  // when the allocation below tripped over it, which is the half of the story the dump cannot tell.
  if (wrote && g_heapNoteSet) {
    wrote = (f.printf("HEAP was already corrupt before this panic: %s\n", g_heapNote) > 0);
  }

  String body;
  if (crashDumpText(body)) {
    wrote = wrote && (f.print(body) > 0);
  } else {
    wrote = false;
  }
  if (wrote) f.print("\n\n");              // keep successive entries apart when the endpoint concatenates them
  wrote = wrote && (f.getWriteError() == 0);
  f.close();

  if (!wrote) {
    LittleFS.remove(path);                 // a short file would be worse than none: it looks like a valid entry
    ERRORLOG("CoreDump: write of %s failed - the dump stays in flash for the next boot", path);
    return false;
  }

  g_heapNoteSet = 0;                       // the note now lives in the file
  g_heapNote[0] = 0;
  if (crashDumpErase()) {
    BOOTLOG("CoreDump: archived to %s (%u kept) - %s is now the record of it",
            path, (unsigned)((r.count >= KEEP) ? KEEP : r.count + 1), path);
  } else {
    WARNINGLOG("CoreDump: archived to %s but the partition could not be erased - it will be reported again", path);
  }
  return true;
}

void service() {
  static bool    done     = false;
  static uint8_t attempts = 0;
  if (done) return;
  if (!crashDumpAvailable()) { done = true; return; }   // nothing pending: this is the normal path, one check per boot
  // A timestamp is the point of the file, and SNTP is asynchronous, so wait for a real clock - but not for ever: an
  // offline SD boot never syncs, and the dump must still be kept.  Three failed writes and the dump is left for the
  // next boot rather than retried every loop.
  if (!clockUsable(time(nullptr)) && millis() < CLOCK_WAIT_MS) return;
  if (archive()) done = true;
  else if (++attempts >= 3) done = true;
}

bool buildAllText(String& out) {
  uint16_t idx[32];
  const uint8_t n = listIndices(idx, 32);
  if (!n) return false;

  const time_t now = time(nullptr);
  char when[48];
  if (clockUsable(now)) {
    struct tm tmv;
    localtime_r(&now, &tmv);
    strftime(when, sizeof(when), "%Y-%m-%d %H:%M:%S", &tmv);
  } else {
    snprintf(when, sizeof(when), "clock unverified");
  }
  out += "ehRadio " RADIOVERSION " | ";
  out += kFirmware;
  out += " | app md5 ";
  out += ESP.getSketchMD5();
  out += " | now ";
  out += when;
  out += "\n";
  out += n;
  out += " crash log(s), oldest first\n\n";

  for (uint8_t i = 0; i < n; i++) {
    char path[32];
    pathFor(idx[i], path, sizeof(path));
    out += "==== coredump.";
    out += idx[i];
    out += " ====\n";
    File f = LittleFS.open(path, "r");
    if (!f) { out += "(could not be opened)\n\n"; continue; }
    out += f.readString();
    f.close();
    if (!out.endsWith("\n\n")) out += "\n";
  }
  return true;
}

}  // namespace coredump
