#include "options.h"
#if SD_CS!=255 // ============================== Everything ignored if not defined ==============================
#include <Arduino.h>
#include <SPI.h>
#include <SD.h>
#include "vfs_api.h"
#include "sd_diskio.h"
//#define USE_SD
#include "config.h"
#include "logging.h"
#include "sdmanager.h"
#include "display.h"
#include "player.h"

// SPIB is declared and initialized in config.cpp (Config::init) — do not re-declare here.
// SD uses Bus B if assigned via SD_SPI 'B', otherwise Bus A.
#if defined(SD_SPI) && (SD_SPI == 'B') && defined(SPIB_SCK)
  #define SDREALSPI SPIB
#else
  #define SDREALSPI SPIA
#endif

SDManager sdman(FSImplPtr(new VFSImpl()));

bool SDManager::start() {
  ready = begin(SD_CS, SDREALSPI, SDSPISPEED);
  if (ready) return ready;
  vTaskDelay(10);
  ready = begin(SD_CS, SDREALSPI, SDSPISPEED);
  if (ready) return ready;
  vTaskDelay(20);
  ready = begin(SD_CS, SDREALSPI, SDSPISPEED);
  if (ready) return ready;
  vTaskDelay(50);
  ready = begin(SD_CS, SDREALSPI, SDSPISPEED);
  return ready;
}

void SDManager::stop() {
  end();
  ready = false;
}
#include "diskio_impl.h"
bool SDManager::cardPresent() {

  if (!ready) return false;
  if (sectorSize()<1) {
    return false;
  }
  uint8_t buff[sectorSize()] = { 0 };
  bool bread = readRAW(buff, 1);
  if (sectorSize()>0 && !bread) return false;
  return bread;
}

bool SDManager::_checkNoMedia(const char* path) {
  char nomedia[SD_PATH_LENGTH]= {0};
  strlcat(nomedia, path, SD_PATH_LENGTH);
  strlcat(nomedia, "/.nomedia", SD_PATH_LENGTH);
  bool nm = exists(nomedia);
  return nm;
}

bool SDManager::_endsWith (const char* base, const char* str) {
  int slen = strlen(str) - 1;
  const char *p = base + strlen(base) - 1;
  while(p > base && isspace(*p)) p--;
  p -= slen;
  if (p < base) return false;
  return (strncmp(p, str, slen) == 0);
}

void SDManager::listSD(File &plSDfile, File &plSDindex, const char* dirname, uint8_t levels) {
  File root = sdman.open(dirname);
  if (!root) {
    ERRORLOG("Failed to open directory");
    return;
  }
  if (!root.isDirectory()) {
    ERRORLOG("Not a directory");
    return;
  }

  uint32_t pos = 0;
  char* filePath;
  while (true) {
    vTaskDelay(2);
    player.loop();
    bool isDir;
    String fileName = root.getNextFileName(&isDir);
    if (fileName.isEmpty()) break;
    filePath = (char*)malloc(fileName.length() + 1);
    if (filePath == NULL) {
      ERRORLOG("Memory allocation failed");
      break;
    }
    strcpy(filePath, fileName.c_str());
    const char* fnSlash = strrchr(filePath, '/');
    const char* fn = fnSlash ? fnSlash + 1 : filePath;
    if (isDir) {
      if (levels && !_checkNoMedia(filePath)) {
        listSD(plSDfile, plSDindex, filePath, levels - 1);
      }
    } else {
      if (_endsWith(strlwr((char*)fn), ".mp3") || _endsWith(fn, ".m4a") || _endsWith(fn, ".aac") ||
          _endsWith(fn, ".wav") || _endsWith(fn, ".flac")) {
        pos = plSDfile.position();
        plSDfile.printf("%s\t%s\t0\n", fn, filePath);
        plSDindex.write((uint8_t*)&pos, 4);
        SERIALLOGDOT();
        if (display.mode()==SDCHANGE) display.putRequest(SDFILEINDEX, _sdFCount+1);
        _sdFCount++;
        if (_sdFCount % 64 == 0) SERIALLOG("");
      }
    }
    free(filePath);
  }
  root.close();
}

void SDManager::indexSDPlaylist() {
  _sdFCount = 0;
  if (exists(PLAYLIST_SD_PATH)) remove(PLAYLIST_SD_PATH);
  if (exists(INDEX_SD_PATH)) remove(INDEX_SD_PATH);
  File playlist = open(PLAYLIST_SD_PATH, "w", true);
  if (!playlist) {
    return;
  }
  File index = open(INDEX_SD_PATH, "w", true);
  listSD(playlist, index, "/", SD_MAX_LEVELS);
  index.flush();
  index.close();
  playlist.flush();
  playlist.close();
  SERIALLOG("");
  delay(50);
}
#endif


