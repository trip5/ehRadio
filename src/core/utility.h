#ifndef utility_h
#define utility_h
#pragma once

#include "options.h"
#include <Arduino.h>
#include <Ticker.h>

class Utility {
public:
  void stripWhitespace(char* text);
  void stripWrappingQuotes(char* text);
  char* ipToStr(IPAddress ip);
  void escapeQuotes(const char* input, char* output, size_t maxLen);
  bool parseCSV(const char* line, char* name, char* url, int& ovol);
  bool parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize);
  bool parseSsid(const char* line, char* ssid, char* pass);
  bool saveWifi(const char* post);
  bool addSsid(const char* ssid, const char* password);
  bool importWifi();
  void indexPlaylist();
  void initPlaylist();
  uint16_t playlistLength();
  bool loadStation(uint16_t stationId);
  uint16_t findStationByUrl(const char* url);
  char* stationByNum(uint16_t num);
  void doSleepW();
  void sleepForAfter(uint16_t sleepfor, uint16_t sa = 0);
  void cleanupSpiffs();
  void deleteMainwwwFile();
  void updateFile(void* param, const char* localFile, const char* onlineFile, const char* updatePeriod, const char* simpleName);
  void updateLocaleFile();
  bool updateLocaleFileAsync(const char* localeCode, uint8_t clientId);

private:
  static uint16_t sleepfor;
  static void sleepCore();
  static void doSleep();
  char ipBuf[16] = {0};
  char stationBuf[STATION_FIELD_LENGTH / 2] = {0};
  Ticker sleepTimer;
};

extern Utility utility;

#endif // utility_h