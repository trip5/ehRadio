#include "options.h"
#include "heapq.h"
#include "logging.h"

namespace {

constexpr uint8_t FILL_BYTE = 0xA5;
constexpr uint8_t LIVE_SLOTS = 24;

struct LiveEntry { void* ptr; size_t size; const char* id; };
struct HeldEntry { void* ptr; size_t size; const char* id; uint8_t pass; bool used; };

LiveEntry s_live[LIVE_SLOTS] = {};
HeldEntry s_held[heapq::SLOTS] = {};

SemaphoreHandle_t s_lock = nullptr;
bool s_capLogged  = false;
bool s_liveLogged = false;

void ensureLock() {
  if (!s_lock) s_lock = xSemaphoreCreateMutex();
}

void takeLock()   { if (s_lock) xSemaphoreTake(s_lock, portMAX_DELAY); }
void giveLock()   { if (s_lock) xSemaphoreGive(s_lock); }

} // namespace

//---------------------------------------------------------------------------------------------------------------------

void* heapq::take(const char* id, size_t size) {
  ensureLock();
  void* p = malloc(size);
  if (!p) {
    ERRORLOG("heapq: %s could not allocate %u bytes", id, (unsigned)size);
    return nullptr;
  }
  bool full = true;
  takeLock();
  for (uint8_t i = 0; i < LIVE_SLOTS; i++) {
    if (s_live[i].ptr) continue;
    s_live[i].ptr  = p;
    s_live[i].size = size;
    s_live[i].id   = id;
    full = false;
    break;
  }
  giveLock();
  if (full && !s_liveLogged) {
    s_liveLogged = true;
    WARNINGLOG("heapq: live table full, further blocks are freed without a size and never quarantined");
  }
  return p;
}

//---------------------------------------------------------------------------------------------------------------------

void heapq::give(const char* id, void* ptr) {
  if (!ptr) return;
  ensureLock();

  size_t size = 0;
  const char* owner = id;
  takeLock();
  for (uint8_t i = 0; i < LIVE_SLOTS; i++) {
    if (s_live[i].ptr != ptr) continue;
    size = s_live[i].size;
    if (s_live[i].id) owner = s_live[i].id;
    s_live[i] = { nullptr, 0, nullptr };
    break;
  }

  // Nothing held means nothing to compare against later, and nothing to check now.
  int8_t slot = -1;
  if (size >= 8 && size <= CAP_MAX) {
    for (uint8_t i = 0; i < SLOTS; i++) {
      if (s_held[i].used) continue;
      slot = (int8_t)i;
      break;
    }
  }
  bool announceCap = false;
  if (slot >= 0) {
    memset(ptr, FILL_BYTE, size);
    s_held[slot] = { ptr, size, owner, 0, true };
  }
  else if (size > CAP_MAX && !s_capLogged) {
    s_capLogged = true;
    announceCap = true;
  }
  giveLock();

  if (slot >= 0) return;
  if (announceCap) {
    FUNCTIONLOG("HeapQ", "%s is %u bytes, above the %u cap - freed normally from here on, not quarantined",
                owner, (unsigned)size, (unsigned)CAP_MAX);
  }
  free(ptr);
}

//---------------------------------------------------------------------------------------------------------------------

void heapq::verify() {
  ensureLock();

  const char* badId   = nullptr;
  void*       badPtr  = nullptr;
  size_t      badSize = 0;
  size_t      badOff  = 0;
  uint8_t     badVal  = 0;

  takeLock();
  for (uint8_t i = 0; i < SLOTS; i++) {
    if (!s_held[i].used) continue;
    uint8_t* p = (uint8_t*)s_held[i].ptr;

    size_t off = s_held[i].size;
    for (size_t k = 0; k < s_held[i].size; k++) {
      if (p[k] != FILL_BYTE) { off = k; break; }
    }

    if (off != s_held[i].size) {
      // Report the first one; the rest are released so a burst cannot fill the ring forever.
      if (!badId) {
        badId   = s_held[i].id;
        badPtr  = s_held[i].ptr;
        badSize = s_held[i].size;
        badOff  = off;
        badVal  = p[off];
      }
      free(s_held[i].ptr);
      s_held[i] = { nullptr, 0, nullptr, 0, false };
      continue;
    }

    if (++s_held[i].pass >= RETAIN) {
      free(s_held[i].ptr);
      s_held[i] = { nullptr, 0, nullptr, 0, false };
    }
  }
  giveLock();

  if (badId) {
    ERRORLOG("WRITE-AFTER-FREE in %s: %u-byte block at %p, byte +%u changed to 0x%02X (was 0x%02X)",
             badId, (unsigned)badSize, badPtr, (unsigned)badOff, badVal, FILL_BYTE);
  }
}

//---------------------------------------------------------------------------------------------------------------------

// Both of these serve the [HeapMap] line only, so they are compiled only when it is.
#ifdef HEAPMAP_LOG

size_t heapq::held() {
  size_t n = 0;
  takeLock();
  for (uint8_t i = 0; i < SLOTS; i++) if (s_held[i].used) n++;
  giveLock();
  return n;
}

//---------------------------------------------------------------------------------------------------------------------

const char* heapq::poolName(const void* p) {
  uintptr_t a = (uintptr_t)p;
  if (a >= 0x3D800000UL && a < 0x3E000000UL) return "psram";
  if (a >= 0x3C000000UL && a < 0x3D800000UL) return "psram";
  if (a >= 0x3FC80000UL && a < 0x3FD00000UL) return "dram";
  return "?";
}

#endif  // HEAPMAP_LOG
