#ifndef sdmanager_h
#define sdmanager_h

#include "options.h"
// The FS API, for fs::FSImplPtr in the constructor.  Both transport headers include it too, but <SD_MMC.h> does not
// carry the "using namespace fs;" that <SD.h> does, so state the dependency rather than inherit it.
#include <FS.h>
#include <esp_task_wdt.h>   // esp_task_wdt_reset(), for the long card walks

// Two transports: the native SDMMC host on ESP32-S3 (SDMMC_ pins in myoptions.h, see SD_USE_MMC) or classic SPI.
// fs::SDFS and fs::SDMMCFS are sibling fs::FS subclasses with the same constructor, so the base class swaps without
// touching a caller - sdman is consumed as fs::FS& by Config::SDPLFS() and Audio::connecttoFS().
#if defined(SD_USE_MMC)
  #include <SD_MMC.h>
  #define SDMAN_FS_BASE fs::SDMMCFS
#else
  #include <SD.h>
  #define SDMAN_FS_BASE fs::SDFS
#endif

#define SD_PATH_LENGTH 256 // max length for SD filesystem path buffers

// Consecutive failed presence probes before a card is declared gone.  The probe is a raw sector read that shares the
// SD host with FATFS writes, so a card still busy after a write times out one probe - one failure must never be
// trusted.  The probe itself is rate-limited to SDMAN_CARD_PROBE_MS, so the count advances at most once a second
// however many callers ask for it.
#ifndef SDMAN_CARD_GONE_STRIKES
  #define SDMAN_CARD_GONE_STRIKES 3
#endif
#ifndef SDMAN_CARD_PROBE_MS
  #define SDMAN_CARD_PROBE_MS 1000UL
#endif
// Silence the probe for this long after a manager session closes: the card is known good and demonstrably busy for a
// moment, and the first raw read on the freshly remounted card is the one that used to drop the mode to web.
#ifndef SDMAN_CARD_CHECK_GRACE_MS
  #define SDMAN_CARD_CHECK_GRACE_MS 3000UL
#endif

// Feed the task watchdog from inside a long card operation.  Web handlers run in AsyncTCP's task, which is subscribed,
// so a card that takes seconds would abort the device rather than merely be slow.  The tick also lets other tasks run,
// and where the caller is not subscribed esp_task_wdt_reset() reports ESP_ERR_NOT_FOUND harmlessly.
static inline void sdFeedWatchdog() {
  esp_task_wdt_reset();
  vTaskDelay(1);
}

class SDManager : public SDMAN_FS_BASE {
  public:
    bool ready = false;
  public:
    SDManager(fs::FSImplPtr impl) : SDMAN_FS_BASE(impl) {}
    // Mount at `freq` (the SPI bus clock; the SDMMC path takes its clock from the driver config and ignores it) and
    // remember what it mounted at.  Discards every open handle, so it may only be called with nothing holding the card.
    bool start(uint32_t freq = SDSPISPEED);
    void stop();
    // Mount if needed, and REMOUNT if the clock asked for is not the one in use. This is the whole of the manager-session
    // speed switch: a session opens at SDSPISPEED_MANAGER because writes are what it does, and the configured speed
    // comes back when the session ends so the player's reads are unaffected.
    bool ensureSpeed(uint32_t freq);
    uint32_t mountedFreq() const { return _freq; }
    // THE CARD'S CLUSTER SIZE in bytes, 0 when it could not be read - read once per mount. REPORTED, never acted on.
    uint32_t allocationUnit() const { return _auBytes; }
    bool cardPresent();
    // Debounced presence: true while the card is still considered present.  One failed raw read is a busy card, not a
    // removal, so this returns false only after SDMAN_CARD_GONE_STRIKES consecutive failures and resets the count on
    // any success - the ONE owner of that count, so the player and the manager cannot drift apart.
    bool cardPresentStable();
    uint8_t cardGoneStrikes() const { return _cardGoneStrikes; }
    bool presenceProbeAllowed() const { return (int32_t)(millis() - _probeGraceUntil) >= 0; }
    // Reset the strikes and stop probing for a moment: called when a manager session ends, when the card has just been
    // written to and remounted.
    void grantPresenceGrace(uint32_t ms = SDMAN_CARD_CHECK_GRACE_MS);
    bool listSD(File &plSDfile, File &plSDindex, const char * dirname, uint8_t levels);
    void indexSDPlaylist();
    uint32_t countAudioFiles();
    // Is this NAME something the player can play? The ONE copy of the extension list, public because filemanager.cpp
    // asks it to decide whether a change can invalidate the playlist and index pair.
    static bool isAudioName(const char* fn);
    void trySdRemount();  // attempt SD mount + re-index (called from controls in SDOFFLINE mode)
  private:
    uint32_t _freq = SDSPISPEED;   // the clock the card is mounted at, see mountedFreq()
    uint32_t _auBytes = 0;         // allocation unit (cluster) size in bytes, 0 = unknown; see allocationUnit()
    uint32_t _sdFCount = 0;
    uint8_t  _cardGoneStrikes = 0;   // consecutive failed probes, see cardPresentStable()
    uint32_t _probeLastMs = 0;       // when the last raw probe actually ran, for the rate limit
    uint32_t _probeGraceUntil = 0;   // millis() before which nothing should probe
    bool _mount(uint32_t freq);    // the mount retry ladder; start() wraps it so the AU is read after a success
    void _readAllocationUnit();    // asks FATFS for csize once per mount; never allowed to fail a mount
    uint32_t _countAudioFilesRecursive(const char* dirname, uint8_t levels);
    bool _checkNoMedia(const char* path);
};

extern SDManager sdman;
#endif
