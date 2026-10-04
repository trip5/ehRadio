#ifndef filemanager_h
#define filemanager_h

#include "options.h"

#ifdef USE_SD

#include <Arduino.h>
#include <ESPAsyncWebServer.h>

// SD card file manager: the /sdman browser UI and the runtime mode that makes it safe. SD builds only.
// Runtime-only - nothing is persisted, so unplugging instead of pressing Done costs nothing.

// Idle time before the mode closes. Overridable from myoptions.h; the countdown asks idleRemainingMs().
#ifndef SDMAN_AUTO_EXIT_MS
  #define SDMAN_AUTO_EXIT_MS 180000UL
#endif

// The used-bytes figure hInfo() measures is logged only when it CHANGES or when that FAT walk takes longer than
// this: the page calls the handler on every load, click and idle poll, so a line each time was noise.
#ifndef SDMAN_INFO_SLOW_MS
  #define SDMAN_INFO_SLOW_MS 20UL
#endif

// The countdown is only DRAWN at or below this. Above it, a clock refreshed by every chunk would sit at the top of
// its range looking stopped; below, it is a real countdown the user can act on.
#ifndef SDMAN_COUNTDOWN_FROM_MS
  #define SDMAN_COUNTDOWN_FROM_MS 120000UL
#endif

// How long an upload may go without a chunk before the mode abandons it. An open upload holds the mode open, so
// this is the only thing that can end a transfer whose browser has gone away - deliberately far below the idle
// timeout, and well clear of any plausible pause. A card that answers slowly in the WRITE is the WDT's case.
#ifndef SDMAN_UPLOAD_STALL_MS
  #define SDMAN_UPLOAD_STALL_MS 120000UL
#endif

// The same deadline once a chunk has already failed: the file is lost and nothing is being waited for, so the
// transfer is released quickly instead of holding the manager - and the whole WebUI - locked. It only has to
// outlast the page's ABORT, not a browser that may come back.
#ifndef SDMAN_UPLOAD_STALL_FAILED_MS
  #define SDMAN_UPLOAD_STALL_FAILED_MS 1000UL
#endif

// Rescues are budgeted by SIZE, because a flat count conflated an isolated refusal with a card that is gone: a 10 MB
// file was thrown away one rescue short of finishing, while a dead card refused 17 chunks in a row. The divisor is
// small enough that this is only a sanity ceiling - the BURST rule decides a card is gone. Rescues are counted, not logged.
#ifndef SDMAN_UPLOAD_RETRY_PER_BYTES
  #define SDMAN_UPLOAD_RETRY_PER_BYTES 65536ULL    // one rescue allowed per this much written
#endif
#ifndef SDMAN_UPLOAD_RETRY_FLOOR
  #define SDMAN_UPLOAD_RETRY_FLOOR 8               // plus this many, so a small file is not starved
#endif
#ifndef SDMAN_UPLOAD_RETRY_BURST
  #define SDMAN_UPLOAD_RETRY_BURST 4               // consecutive refusals with no successful chunk between them
#endif
// Give the card a moment before retrying: the refusal answered here is `0 of 1436 bytes, errno 5, after 0ms` - no
// bus time at all, a card declining the next write while it programs the previous block. Time is the remedy.
#ifndef SDMAN_UPLOAD_RETRY_DELAY_MS
  #define SDMAN_UPLOAD_RETRY_DELAY_MS 2
#endif
// A ceiling for a pause that grows with consecutive refusals: it doubles from the base for each refusal in a row
// (2, 4, 8, 16, 32, 50, 50...), so an isolated refusal costs only the base and a struggling card gets room.
#ifndef SDMAN_UPLOAD_RETRY_MAX_DELAY_MS
  #define SDMAN_UPLOAD_RETRY_MAX_DELAY_MS 50
#endif

// How long a card is given to catch up before a short size() is BELIEVED. The budget card's signature is always a
// few KB of shortfall, which is what a card still programming the block it just accepted reports, so the size is
// read, the card is given this long to settle, and read again: if it catches up nothing was lost.
#ifndef SDMAN_UPLOAD_LAG_SETTLE_MS
  #define SDMAN_UPLOAD_LAG_SETTLE_MS 50
#endif

// How many bytes may be written between two proofs that the stream is real. flush() plus size() is the only truthful
// instrument in this path - a buffered write reports what it accepted, not what landed - so checking early makes a
// failure cheap and retryable. 0 disables the check.
#ifndef SDMAN_UPLOAD_VERIFY_BYTES
  #define SDMAN_UPLOAD_VERIFY_BYTES 65536UL
#endif

// THE CARD CLOCK FOR A MANAGER SESSION, one clock for the whole session - no stepping, no switching. Measured: the
// same batch landed 10 of 10 files with zero refusals at 10 MHz where it failed ~50 times at 20 MHz, and just as
// fast - the CARD is the limit. A high clock belongs to the PLAYER's reads, so enter() opens here and leave() puts
// SDSPISPEED back. An automatic step-down was built and removed (do not rebuild it): a budget card froze at the
// same offset from 10 MHz down to 156 kHz. Changing speed mid-session would remount, which discards every handle.
#ifndef SDSPISPEED_MANAGER
  #define SDSPISPEED_MANAGER 10000000
#endif

// THE CLUSTER ASSEMBLY: the upload assembles to the card's OWN allocation unit (SDManager::allocationUnit()), with
// the buffer in filemanager.cpp - taken from PSRAM when present and the internal heap otherwise, kept for the
// session. A sector-aligned experiment came first and was refuted: the card's unit was sixteen sectors, so aligning
// to a sector never changed how a cluster was programmed. The alignment that matters is to the CLUSTER.

class FileManager {
  public:
    // True between entering the mode and leaving it: Done, the idle timeout, or the card leaving the slot.
    bool active() const { return _active; }

    void enter();   // GET /sdman - mounts if needed, blocks the player, starts the idle clock
    // Unblocks the player and restores the display; under SmartStart it gives the audio back too. resumeAudio=false
    // for the one exit with nothing to resume (the card left the slot); `why` is named in the close's single line.
    void leave(bool resumeAudio = true, const char *why = "on request");
    void loop();    // idle timeout and card-present check - call from the main loop
    void registerRoutes(AsyncWebServer &server);

    uint32_t idleRemainingMs() const;   // drives the on-screen countdown

    // True while a file is open for writing. leave() removes a half-written file by design, so hDone() asks before
    // closing the mode: a close mid-upload would destroy one the user is still watching.
    bool uploadOpen() const;

    // Milliseconds since the last chunk of the upload in flight, 0 when none is. The stall deadline in loop() and
    // the countdown both read it, so the on-screen number keeps moving.
    uint32_t uploadIdleMs() const;

    // True while a mutating request is running - the delete batch, whose whole selection is handled inside one
    // request (uploads report through uploadOpen()). hDone() refuses while it is set; leave() clears it as a backstop.
    bool busy() const { return _busy; }
    void markBusy(bool on) { _busy = on; }

    // True while the deferred rebuild is walking the card. net-server's PLAYLISTREADY case carries this, so the
    // WebUI is told both "a rebuild is running: lock and spin" and "the list is ready: fetch".
    bool rebuilding() const { return _rebuilding; }

    // Refreshes the idle clock. Public because the handlers are free functions and must stamp activity.
    void touch();

    // Records that a mutation invalidated the card's derived files: the SD index is deleted on the spot and this
    // bit remembers a re-index is owed. Public because the handlers are free functions; loop() pays it after closing.
    void markCardChanged();

    // The root and everything under /data: playlists, SD index, credentials. No mutation may touch it.
    static bool isProtected(const String &path);

    // True if this path is the playing file, or a directory holding it. Deleting an open file succeeds until
    // the FAT handle closes, which is how a song could be deleted out from under the decoder.
    bool isPlaying(const String &path) const;

  private:
    bool _active = false;
    bool _wasPlaying = false;        // the player was running when the mode was entered - see leave()
    // A mutation invalidated the card's derived files. Honoured by loop() after the mode closes - never from the
    // handler or while open - so a hundred operations pay for one walk. leave() reads it too, to suppress the resume.
    bool _cardChanged = false;
    bool _busy = false;              // a mutating request is in flight, see busy()
    // An unfinished build leaves NO index at all, so "the index is missing" is the repair signal. A failed build is
    // owed another pass, but not immediately: the retry waits, then gives up until the next mode entry.
    uint32_t _reindexNotBeforeMs = 0;
    uint8_t  _reindexTries = 0;      // attempts since the last build that wrote an index
    bool     _rebuilding = false;    // a deferred rebuild is walking the card right now, see rebuilding()
    static constexpr uint32_t SD_REINDEX_RETRY_MS = 4000;
    static constexpr uint8_t  SD_REINDEX_MAX_RETRIES = 2;
    uint32_t _lastActivity = 0;
    uint32_t _lastCountdownMs = 0;   // the on-screen countdown is redrawn at most once a second
    // The presence probe and its strike count live in SDManager (cardPresentStable()), so the player and the manager
    // cannot drift apart.
};

extern FileManager filemanager;

#endif  // USE_SD
#endif  // filemanager_h

