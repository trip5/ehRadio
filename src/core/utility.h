#ifndef utility_h
#define utility_h
#pragma once

#include "options.h"
#include <Arduino.h>
#include <FS.h>
#include <Ticker.h>

bool clockTrustworthy();

/* ===== Deep Sleep Wake GPIO Mask =====
   All defined input pins on RTC-capable GPIOs are automatically used as wake sources.
   ANY interaction (button press release, rotary turn) wakes the device.
   If no pins are RTC-capable, deep sleep is disabled (no way to wake). */

// Per-board RTC GPIO validator — true if pin 'p' is RTC-capable for the target chip
#if defined(ARDUINO_ESP32_DEV)
  #define _IS_RTC(p) ((p)==0||(p)==2||(p)==4||((p)>=12&&(p)<=15)||((p)>=25&&(p)<=27)||((p)>=32&&(p)<=39))
#elif defined(ARDUINO_ESP32S3_DEV)
  #define _IS_RTC(p) ((p)<=21)
#elif defined(ARDUINO_ESP32C3_DEV)
  #define _IS_RTC(p) ((p)!=255)  // any input-capable GPIO works with esp_deep_sleep_enable_gpio_wakeup
#endif

// Build individual wake bits: only include pins on RTC GPIOs; skip pins left at 255
#if _IS_RTC(ENC_DT)
  #define _WENC_DT     (1ULL << ENC_DT)
#else
  #define _WENC_DT     0ULL
#endif
#if _IS_RTC(ENC_CLK)
  #define _WENC_CLK    (1ULL << ENC_CLK)
#else
  #define _WENC_CLK    0ULL
#endif
#if _IS_RTC(ENC_SW)
  #define _WENC_SW     (1ULL << ENC_SW)
#else
  #define _WENC_SW     0ULL
#endif
#if _IS_RTC(ENC2_DT)
  #define _WENC2_DT    (1ULL << ENC2_DT)
#else
  #define _WENC2_DT    0ULL
#endif
#if _IS_RTC(ENC2_CLK)
  #define _WENC2_CLK   (1ULL << ENC2_CLK)
#else
  #define _WENC2_CLK   0ULL
#endif
#if _IS_RTC(ENC2_SW)
  #define _WENC2_SW    (1ULL << ENC2_SW)
#else
  #define _WENC2_SW    0ULL
#endif
#if _IS_RTC(BTN_PREV)
  #define _WBTN_PREV   (1ULL << BTN_PREV)
#else
  #define _WBTN_PREV   0ULL
#endif
#if _IS_RTC(BTN_PLAY)
  #define _WBTN_PLAY   (1ULL << BTN_PLAY)
#else
  #define _WBTN_PLAY   0ULL
#endif
#if _IS_RTC(BTN_NEXT)
  #define _WBTN_NEXT   (1ULL << BTN_NEXT)
#else
  #define _WBTN_NEXT   0ULL
#endif
#if _IS_RTC(BTN_UP)
  #define _WBTN_UP     (1ULL << BTN_UP)
#else
  #define _WBTN_UP     0ULL
#endif
#if _IS_RTC(BTN_DOWN)
  #define _WBTN_DOWN   (1ULL << BTN_DOWN)
#else
  #define _WBTN_DOWN   0ULL
#endif
#if _IS_RTC(BTN_MODE)
  #define _WBTN_MODE   (1ULL << BTN_MODE)
#else
  #define _WBTN_MODE   0ULL
#endif
#if _IS_RTC(TS_INT)
  #define _WTS_INT     (1ULL << TS_INT)
#else
  #define _WTS_INT     0ULL
#endif

// Final GPIO mask — OR of all valid wake pins
#define WAKE_GPIO_MASK (_WENC_DT | _WENC_CLK | _WENC_SW | _WENC2_DT | _WENC2_CLK | _WENC2_SW | \
                        _WBTN_PREV | _WBTN_PLAY | _WBTN_NEXT | _WBTN_UP | _WBTN_DOWN | _WBTN_MODE | \
                        _WTS_INT)

// Block deep sleep when no physical wake source exists
#if WAKE_GPIO_MASK == 0
  #define DEEP_SLEEP_DISABLE
#endif

class Utility {
public:
  void stripWhitespace(char* text);
  void stripWrappingQuotes(char* text);
  char* ipToStr(IPAddress ip);
  void escapeQuotes(const char* input, char* output, size_t maxLen);
  void normalizeToCRLF(const char* input, char* output, size_t outputSize);
  bool isHttpUrl(const char* text);
  bool parseCommandLine(const char* input, char* command, size_t commandSize, char* value, size_t valueSize);
  bool parseCSV(const char* line, char* name, char* url, int& ovol);
  bool parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize);
  bool parseSsid(const char* line, char* ssid, char* pass);
  bool saveWifi(const char* post);
  bool addSsid(const char* ssid, const char* password);
  bool importWifi();
  void indexPlaylist();
  void initPlaylist();
  bool cleanPlaylist();
  uint16_t playlistLength();
  bool loadStation(uint16_t stationId);
  uint16_t findStationByUrl(const char* url);
  char* stationByNum(uint16_t num);
  uint16_t fillPlaylistRange(int from, uint8_t count, char names[][STATION_FIELD_LENGTH / 2]);
  void doSleepW();
  void sleepForAfter(uint16_t sleepfor, uint16_t sa = 0);
  void stopStandby();
  void startStandby();
  void toggleStandby();
  bool verifyLittleFS();
  void pruneLittleFS();
  void deleteMainwwwFile();
  void updateFile(void* param, const char* localFile, const char* onlineFile, const char* updatePeriod, const char* simpleName);

private:
  static uint16_t sleepfor;
  static void sleepCore();
  static void doSleep();
  char ipBuf[16] = {0};
  char stationBuf[STATION_FIELD_LENGTH / 2] = {0};
  Ticker sleepTimer;
};

extern Utility utility;

// CRC32 helper for index validation (see initSDPlaylist / indexSDPlaylist)
uint32_t fileCRC32(File& f, size_t len);

// UTF-8 safe trim: returns src truncated to at most 15 code points (static buffer)
const char* utf8_trim15(const char* src);

uint8_t rssiLevel(int rssi);

#endif // utility_h