#include "options.h"
#if SD_CS!=255 // ============================== Everything ignored if not defined ==============================
#include <Arduino.h>
#include <SPI.h>
#include <vector>
#include <algorithm>
#include "vfs_api.h"
#if defined(SD_USE_MMC)
  #include "sdmmc_cmd.h"   // sdmmc_read_sectors() for the card-present probe below
#else
  #include <SD.h>
  #include "sd_diskio.h"
#endif
//#define USE_SD
#include "config.h"
#include "logging.h"
#include "sdmanager.h"  // pulls in <SD_MMC.h> (SDMMC transport) or <SD.h> (SPI transport)
#include "display.h"
#include "player.h"
#include "utility.h"
#include "../locale/dsplocale.h"

// FATFS itself, for the allocation-unit figure.  Included on BOTH transports on purpose: the MMC path does not pull in
// diskio_impl.h (which carries ff.h on the SPI path), and the cluster size is worth reporting either way.
#include "ff.h"

#if !defined(SD_USE_MMC)
// SPIB is declared and initialized in config.cpp (Config::init) — do not re-declare here.
// SD uses Bus B if assigned via SD_SPI 'B', otherwise Bus A.
#if defined(SD_SPI) && (SD_SPI == 'B') && defined(SPIB_SCK)
  #define SDREALSPI SPIB
#else
  #define SDREALSPI SPIA
#endif
#endif

SDManager sdman(fs::FSImplPtr(new VFSImpl()));

bool SDManager::_mount(uint32_t freq) {
  #if defined(SD_USE_MMC)
    // ---- Native SDMMC host (ESP32-S3 only; configured by the SDMMC_ block in options.h) ----
    // Pins must be set before the first begin(); setPins() is a no-op once the card is mounted.
    #if SDMMC_D1==255 || SDMMC_D2==255 || SDMMC_D3==255
      const bool sdmmc1bit = true;   // 1-bit: CLK, CMD, D0
      setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0);
    #else
      const bool sdmmc1bit = false;  // 4-bit: CLK, CMD, D0..D3
      setPins(SDMMC_CLK, SDMMC_CMD, SDMMC_D0, SDMMC_D1, SDMMC_D2, SDMMC_D3);
    #endif
    #if SDMMC_FREQ > 0
      const int sdmmcFreq = SDMMC_FREQ;           // myoptions.h override
    #else
      const int sdmmcFreq = BOARD_MAX_SDMMC_FREQ; // driver default (40 MHz high speed)
    #endif
    ready = begin("/sdcard", sdmmc1bit, false, sdmmcFreq);
    if (ready) return ready;
    vTaskDelay(10);
    ready = begin("/sdcard", sdmmc1bit, false, sdmmcFreq);
    if (ready) return ready;
    vTaskDelay(20);
    ready = begin("/sdcard", sdmmc1bit, false, sdmmcFreq);
    if (ready) return ready;
    vTaskDelay(50);
    ready = begin("/sdcard", sdmmc1bit, false, sdmmcFreq);
    if (!ready) ERRORLOG("SDMMC mount failed");
    return ready;
  #else
    #if defined(SD_SPI) && (SD_SPI == 'B') && defined(SPIB_SCK) && defined(SPIB_SCK) && (SPIB_SCK != 255)
      SPIB.end();
      SPIB.begin(SPIB_SCK, SPIB_MISO, SPIB_MOSI);
    #elif defined(SPIA_SCK) && (SPIA_SCK != 255)
      SPI.end();
      SPI.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI);
    #endif
    ready = begin(SD_CS, SDREALSPI, freq);
    if (ready) return ready;
    vTaskDelay(10);
    ready = begin(SD_CS, SDREALSPI, freq);
    if (ready) return ready;
    vTaskDelay(20);
    ready = begin(SD_CS, SDREALSPI, freq);
    if (ready) return ready;
    vTaskDelay(50);
    ready = begin(SD_CS, SDREALSPI, freq);
    return ready;
  #endif
}

// Mount, then read the card's allocation unit. Asked for here rather than inside _mount(): it is a figure to REPORT,
// and asking FATFS for it must never be able to fail a mount.
bool SDManager::start(uint32_t freq) {
  const bool ok = _mount(freq);
  if (ok) _readAllocationUnit();
  else _auBytes = 0;
  return ok;
}

// ASK FATFS FOR THE CLUSTER SIZE - `csize`, in SECTORS - and keep it in bytes. Two drive letters are tried because a
// second FatFs volume would take "0:" (FFat is FatFs too); the SD card answers on "0:".
void SDManager::_readAllocationUnit() {
  _auBytes = 0;
  for (const char *drv : {"0:", "1:"}) {
    DWORD nclst = 0;
    FATFS *fs = nullptr;
    if (f_getfree(drv, &nclst, &fs) != FR_OK || fs == nullptr || fs->csize == 0) continue;
    // FF_MAX_SS != FF_MIN_SS means the sector size is a runtime figure (`ssize`); when they are equal it is fixed at
    // 512 and the field does not exist, so the constant is the only answer. Neither path is a guess.
    #if FF_MAX_SS != FF_MIN_SS
      const uint32_t ss = (uint32_t)fs->ssize;
    #else
      const uint32_t ss = 512;
    #endif
    _auBytes = (uint32_t)fs->csize * (ss ? ss : 512);
    return;
  }
}

void SDManager::stop() {
  end();
  ready = false;
  _auBytes = 0;
  _cardGoneStrikes = 0;   // an unmounted card is not "gone"; a later mount starts the count fresh
  _probeLastMs = 0;
}

// Mount if needed, remount if the CLOCK is not the one asked for. Only the SPI path can be steered this way - the
// frequency is a begin() argument - and a remount discards every open handle, so callers switch speed at a mode boundary
// or between transfers, never underneath a request. SDMMC takes its clock from the driver config.
bool SDManager::ensureSpeed(uint32_t freq) {
  #if defined(SD_USE_MMC)
    if (ready) return true;
    return start(freq);
  #else
    if (ready && _freq == freq) return true;
    const uint32_t previous = _freq;
    if (ready) stop();
    if (start(freq)) {
      _freq = freq;
      FUNCTIONLOG("SD", "card clock now %lu Hz", (unsigned long)freq);
      return true;
    }
    // The card did not come back at the clock we asked for. Leaving it unmounted is worse than a slower bus: every path
    // in the manager answers `no_card` while ready is false, and the writer abandons the file it was continuing. Fall
    // back to the clock that DID work.
    if (previous != freq && start(previous)) {
      _freq = previous;
      FUNCTIONLOG("SD", "the card did not mount at %lu Hz - back at %lu Hz", (unsigned long)freq, (unsigned long)previous);
      return true;
    }
    FUNCTIONLOG("SD", "the card did not come back at %lu Hz or %lu Hz", (unsigned long)freq, (unsigned long)previous);
    return false;
  #endif
}
#if !defined(SD_USE_MMC)
  #include "diskio_impl.h"  // readRAW()/sectorSize() probe below is SPI-transport specific
#endif
bool SDManager::cardPresent() {
  if (!ready) return false;
#if defined(SD_USE_MMC)
  // Must be a real probe, matching what readRAW() does on the SPI side. cardSize() reads the cached CSD
  // out of the card descriptor, and _card stays set until end() - so it still reported a card after the
  // card was physically removed, and PR_CHECKSD therefore never fired on SDMMC builds. Reading a
  // physical sector through the host is the only way to see it go away. The read is non-destructive and
  // does not disturb FATFS's own cached window, since it bypasses the filesystem layer entirely.
  if (_card == nullptr) return false;
  uint8_t probe[512];
  return sdmmc_read_sectors(_card, probe, 0, 1) == ESP_OK;
#else
  if (sectorSize()<1) {
    return false;
  }
  uint8_t buff[sectorSize()] = { 0 };
  bool bread = readRAW(buff, 1);
  if (sectorSize()>0 && !bread) return false;
  return bread;
#endif
}

// Debounced presence, and the ONE owner of the strike count.  The rate limit lives here rather than at each caller:
// the manager's loop and the player's PR_CHECKSD both ask, and without it the same busy second would count twice.
bool SDManager::cardPresentStable() {
  const uint32_t now = millis();
  if (_probeLastMs && (now - _probeLastMs) < SDMAN_CARD_PROBE_MS)
    return _cardGoneStrikes < SDMAN_CARD_GONE_STRIKES;   // too soon to re-probe: report the standing verdict
  _probeLastMs = now;
  if (cardPresent()) {
    _cardGoneStrikes = 0;
    return true;
  }
  if (_cardGoneStrikes < SDMAN_CARD_GONE_STRIKES) _cardGoneStrikes++;
  return _cardGoneStrikes < SDMAN_CARD_GONE_STRIKES;
}

void SDManager::grantPresenceGrace(uint32_t ms) {
  _probeGraceUntil = millis() + ms;
  _probeLastMs = 0;
  _cardGoneStrikes = 0;
}

bool SDManager::_checkNoMedia(const char* path) {
  char nomedia[SD_PATH_LENGTH]= {0};
  strlcat(nomedia, path, SD_PATH_LENGTH);
  strlcat(nomedia, "/.nomedia", SD_PATH_LENGTH);
  bool nm = exists(nomedia);
  return nm;
}

// THE ONE LIST OF WHAT ehRadio PLAYS. The index's staleness check counts exactly these files (the footer count in
// indexSDPlaylist()) and filemanager.cpp asks the same question to decide whether a change can invalidate that index, so
// a second copy could drift from the counter's and hide a newly added track until the next re-index.
bool SDManager::isAudioName(const char* fn) {
  if (fn == nullptr) return false;
  const char* dot = strrchr(fn, '.');
  if (dot == nullptr) return false;
  char ext[8];                     // the longest extension here is ".flac": anything longer is not one of ours
  const size_t n = strlen(dot);
  if (n >= sizeof(ext)) return false;
  for (size_t i = 0; i <= n; i++) ext[i] = (char)tolower((unsigned char)dot[i]);
  return !strcmp(ext, ".mp3") || !strcmp(ext, ".m4a") || !strcmp(ext, ".aac") || !strcmp(ext, ".wav") ||
         !strcmp(ext, ".flac") || !strcmp(ext, ".ogg") || !strcmp(ext, ".opus");
}

// What the last walk SAW, so a failure can tell an empty listing from a skipped folder, and both from a card that refused
// the writes - "walked yes" only ever meant that no step reported an error.
static uint32_t _walkEntries = 0;   // entries counted across every listing the walk read
static uint32_t _walkDirs = 0;      // directories it opened

// The size of a file ON THE CARD, read by opening it fresh. The same instrument the upload path uses: a handle the walk
// has been writing through can report anything once the card beneath stops taking writes, and a fresh open is the only
// second opinion there is.
static size_t sdFileSize(const char* path) {
  File f = sdman.open(path);
  const size_t n = f ? (size_t)f.size() : 0;
  if (f) f.close();
  return n;
}

bool SDManager::listSD(File &plSDfile, File &plSDindex, const char* dirname, uint8_t levels) {
  File root = sdman.open(dirname);
  if (!root) {
    ERRORLOG("Failed to open directory %s", dirname);
    return false;
  }
  if (!root.isDirectory()) {
    ERRORLOG("Not a directory: %s", dirname);
    return false;
  }

  _walkDirs++;
  // Collect all entries for sorting (dirs first, then alphanumeric by basename)
  struct DirEntry { String path; bool isDir; };
  std::vector<DirEntry> entries;
  while (true) {
    vTaskDelay(2);
    player.loop();
    bool isDir;
    String fileName = root.getNextFileName(&isDir);
    if (fileName.isEmpty()) break;
    entries.push_back({fileName, isDir});
  }
  root.close();
  _walkEntries += entries.size();

  // Sort: directories before files, both case-insensitive alphanumeric by basename
  std::sort(entries.begin(), entries.end(), [](const DirEntry& a, const DirEntry& b) {
    if (a.isDir != b.isDir) return a.isDir;  // true (dir) > false (file)
    const char* an = strrchr(a.path.c_str(), '/');
    const char* bn = strrchr(b.path.c_str(), '/');
    an = an ? an + 1 : a.path.c_str();
    bn = bn ? bn + 1 : b.path.c_str();
    return strcasecmp(an, bn) < 0;
  });

  // Process sorted entries
  uint32_t pos = 0;
  bool ok = true;
  for (const auto& entry : entries) {
    sdFeedWatchdog();   // the index walk yields here already; this also feeds the watchdog if the caller is subscribed
    player.loop();
    char* filePath = (char*)malloc(entry.path.length() + 1);
    if (filePath == NULL) {
      ERRORLOG("Memory allocation failed");
      ok = false;
      break;
    }
    strcpy(filePath, entry.path.c_str());
    const char* fnSlash = strrchr(filePath, '/');
    const char* fn = fnSlash ? fnSlash + 1 : filePath;
    if (entry.isDir) {
      if (levels && !_checkNoMedia(filePath)) {
        if (!listSD(plSDfile, plSDindex, filePath, levels - 1)) ok = false;
      }
    } else {
      if (isAudioName(fn)) {
        pos = plSDfile.position();
        const size_t rowBody = (size_t)plSDfile.print(fn) + (size_t)plSDfile.print('\t') + (size_t)plSDfile.print(filePath);
        const size_t rowTail = plSDfile.write((const uint8_t*)"\t0\r\n", 4);
        const size_t idxWrote = plSDindex.write((uint8_t*)&pos, 4);
        if (rowTail != 4 || idxWrote != 4 || rowBody == 0) {
          // a write the card refused: the pair is short from here on, so say so rather than counting the file anyway
          ERRORLOG("SD write failed at %s", filePath);
          ok = false;
        } else {
          SERIALLOGDOT();
          if (display.mode()==SDCHANGE) display.putRequest(SDFILEINDEX, _sdFCount+1);
          _sdFCount++;
          if (_sdFCount % 64 == 0) SERIALLOGLF();
        }
      }
    }
    free(filePath);
    if (!ok) break;   // a failing card only produces more of the same
  }
  return ok;
}

// The folder the derived pair lives in is SD_DATA_DIR, defined in config.h - the ONE place the name is written, and
// the parent of PLAYLIST_SD_PATH and INDEX_SD_PATH, so it cannot drift from them.

void SDManager::indexSDPlaylist() {
  _sdFCount = 0;
  _walkEntries = 0;
  _walkDirs = 0;
  // Make sure the folder exists ON THE CARD before anything is opened in it: FATFS cannot create a file inside a missing
  // directory and nothing else creates this one (startup.cpp's mkdir is for the FLASH filesystem). Without it the index
  // was never built and SD mode reported no playable station for ever, while uploads kept working.
  if (!exists(SD_DATA_DIR)) {
    if (!mkdir(SD_DATA_DIR)) {
      ERRORLOG("SD: cannot create %s on the card - the playlist and index have nowhere to live", SD_DATA_DIR);
      SERIALLOGLF();
      return;
    }
    // Nothing migrates: an old /data on the card is clutter the user may delete.
    FUNCTIONLOG("SD", "created %s on the card", SD_DATA_DIR);
  }
  if (exists(PLAYLIST_SD_TMP_PATH)) remove(PLAYLIST_SD_TMP_PATH);
  if (exists(INDEX_SD_TMP_PATH)) remove(INDEX_SD_TMP_PATH);
  errno = 0;
  File playlist = open(PLAYLIST_SD_TMP_PATH, "w", true);
  // Captured HERE and never asked again: the handles below are closed before anything is reported, and a CLOSED handle
  // answers false - which made the failure line blame the wrong file whatever had really happened.
  const bool plOpened = playlist ? true : false;
  const int  plErr    = plOpened ? 0 : errno;
  if (!playlist) {
    // Never silent: a failed open here is the difference between a working card and a device that claims to have
    // nothing to play, and the old code returned without a word.
    ERRORLOG("SD: cannot create %s - indexing skipped", PLAYLIST_SD_TMP_PATH);
    SERIALLOGLF();
    return;
  }
  errno = 0;
  File index = open(INDEX_SD_TMP_PATH, "w", true);
  const bool idxOpened = index ? true : false;
  const int  idxErr    = idxOpened ? 0 : errno;
  const bool walked = listSD(playlist, index, "/", SD_MAX_LEVELS);

  index.flush();                                 // size() is only accurate after a flush
  const size_t idxRows = idxOpened ? index.size() : 0;
  const bool complete = walked && plOpened && idxOpened && idxRows == (size_t)_sdFCount * 4;

  if (!complete) {
    if (idxOpened) index.close();
    if (plOpened) playlist.flush();
    const size_t plBytes = plOpened ? playlist.size() : 0;
    if (plOpened) playlist.close();
    // A SECOND, INDEPENDENT OPINION, taken before the pair is removed: the figures above come from handles the walk wrote
    // through, which can report anything once the card stops accepting writes. Re-opened by name, the two files report
    // what actually reached the card.
    const size_t plOnCard  = sdFileSize(PLAYLIST_SD_TMP_PATH);
    const size_t idxOnCard = sdFileSize(INDEX_SD_TMP_PATH);
    remove(PLAYLIST_SD_TMP_PATH);
    remove(INDEX_SD_TMP_PATH);
    SERIALLOGLF();
    // WHICH condition failed, named, with everything that decides it: the open state, the errno behind a failure to open,
    // and what the walk SAW. `_sdFCount` counts a file only when all four of its writes reported their full length, so a
    // large count beside a small index means the writes were accepted and lost: a failing card, not a failed walk.
    const char* why = !idxOpened ? "the index temp could not be created"
                                 : (!walked ? "the walk stopped at a refused write"
                                            : "the row count does not match the files counted");
    FUNCTIONLOG("SD", "indexing did not finish (%s): playlist open %s (errno %d, %u bytes, %u on the card), index open %s (errno %d, rows %u, %u on the card), walked %s, saw %u entries in %u dir(s), counted %u files (rows should be %u) - the partial pair was discarded, any previous pair is untouched",
                why,
                plOpened ? "yes" : "no", plErr, (unsigned)plBytes, (unsigned)plOnCard,
                idxOpened ? "yes" : "no", idxErr, (unsigned)idxRows, (unsigned)idxOnCard,
                walked ? "yes" : "no", (unsigned)_walkEntries, (unsigned)_walkDirs,
                (unsigned)_sdFCount, (unsigned)(_sdFCount * 4));
    delay(50);
    return;
  }

  // Append footer: [magic:4][count:4] = 8 bytes
  // - magic = 0x1867 validates this is our format
  // - count = number of audio files found (staleness check)
  uint32_t magic = 0x1867;
  uint32_t fcount = _sdFCount;
  index.seek(index.size());
  index.write((uint8_t*)&magic, 4);
  index.write((uint8_t*)&fcount, 4);
  index.close();

  playlist.flush();
  playlist.close();

  if (exists(PLAYLIST_SD_PATH)) remove(PLAYLIST_SD_PATH);
  if (exists(INDEX_SD_PATH)) remove(INDEX_SD_PATH);
  bool swapped = rename(PLAYLIST_SD_TMP_PATH, PLAYLIST_SD_PATH);
  swapped = rename(INDEX_SD_TMP_PATH, INDEX_SD_PATH) && swapped;
  if (!swapped) {
    ERRORLOG("could not move the new SD playlist and index into place");
  }
  SERIALLOGLF();
  delay(50);
}

uint32_t SDManager::countAudioFiles() {
  _sdFCount = 0;
  _countAudioFilesRecursive("/", SD_MAX_LEVELS);
  return _sdFCount;
}

uint32_t SDManager::_countAudioFilesRecursive(const char* dirname, uint8_t levels) {
  File root = sdman.open(dirname);
  if (!root || !root.isDirectory()) {
    if (root) root.close();
    return 0;
  }

  while (true) {
    sdFeedWatchdog();
    bool isDir;
    String fileName = root.getNextFileName(&isDir);
    if (fileName.isEmpty()) break;

    char* filePath = (char*)malloc(fileName.length() + 1);
    if (!filePath) break;
    strcpy(filePath, fileName.c_str());
    const char* fnSlash = strrchr(filePath, '/');
    const char* fn = fnSlash ? fnSlash + 1 : filePath;

    if (isDir) {
      if (levels && !_checkNoMedia(filePath)) {
        _countAudioFilesRecursive(filePath, levels - 1);
      }
    } else {
      if (isAudioName(fn)) {
        _sdFCount++;
      }
    }
    free(filePath);
  }
  root.close();
  return 0;
}

void SDManager::trySdRemount() {
  if (ready) return;  // already mounted
  FUNCTIONLOG("SD", "Remount attempt...");
  display.putRequest(NEWMODE, SDCHANGE);
  if (start()) {
    config.initSDPlaylist();
    player.setReady();                    // the one implementation of the mode-entry text: see player.h
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWSTATION);
  } else {
    display.putRequest(NEWMODE, PLAYER);  // restore from SDCHANGE
    player.setReady();                    // no card mounted: the same helper, which knows that variant
  }
}
#endif


