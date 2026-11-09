#ifndef heapq_h
#define heapq_h

#include <Arduino.h>

/*
 * A named quarantine for internal-heap blocks, built to find a use-after-free.
 *
 * The panic this exists for is a store into a freed TLSF block: the allocator finds a block that is still on a free
 * list, reads its link, and the link is garbage.  A canary cannot see that, because the block is no longer ours.  So
 * `heapq::give()` does not return the block to the allocator at all - it fills it, keeps it, and checks it later.  A
 * stale write then lands in memory we still own, the allocator survives, and the log names the owner.
 *
 * Only blocks up to CAP_MAX are held, and only for RETAIN verify passes, so the memory cost is bounded and tiny.
 */
namespace heapq {

constexpr size_t  CAP_MAX = 4096;   // larger blocks are freed normally; the first skip is logged
constexpr uint8_t SLOTS   = 12;     // quarantined blocks held at once
constexpr uint8_t RETAIN  = 2;      // verify passes survived before a block is really freed

// Allocate and register the block so give() can find its size later.  `id` must outlive the block (use a literal).
void* take(const char* id, size_t size);

// Free, or quarantine if it can be held.  Unknown pointers fall through to free().
void give(const char* id, void* ptr);

// Called from the core-monitor tick: check every held block, report the first write found, release what has aged out.
void verify();

// Both of these exist only to build the [HeapMap] line, so they compile only when it does.
#ifdef HEAPMAP_LOG
// Blocks currently held, for the [HeapMap] line.
size_t held();

// "psram", "dram" or "?" for an address, so a log line says which pool a buffer came from.
const char* poolName(const void* p);
#endif

} // namespace heapq

#endif
