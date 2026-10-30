#ifndef coredump_h
#define coredump_h
#pragma once

#include <Arduino.h>

/* The black box: the previous boot's crash, written into /data so it survives the reboot and can be read later from
   /coredump without a serial connection and without the build that produced it.  Nine files are kept; the numbers keep
   growing and the lowest is deleted, so the ring never grows and the newest crash is always the highest number.

   service() is called from loop() rather than setup(): the file wants a real timestamp, and SNTP answers after setup()
   has already moved on.  It archives as soon as the clock is usable, or with an honest "clock unverified" stamp once
   CLOCK_WAIT_MS has passed (an offline SD boot never syncs at all).  The dump is erased only after the write succeeds,
   so a boot that cannot write simply leaves it for the next one. */
namespace coredump {

constexpr uint8_t  KEEP         = 9;       // files retained; the tenth write deletes the lowest-numbered
constexpr uint32_t CLOCK_WAIT_MS = 45000;  // how long to wait for a real clock before archiving without one

void service();                  // once per boot, from loop(): archives the pending dump when the time is right
void noteHeapCorruption(const char* text);  // stash a line in RTC memory; archive() prints it above the dump
uint8_t clear();                 // delete every kept file; a version change starts the ring empty
bool archive();                  // write the pending dump to /data/coredump.N, then erase it. False = nothing written
bool buildAllText(String& out);  // every kept file as one stream, for the /coredump endpoint
uint8_t count();                 // files currently kept

}  // namespace coredump

#endif
