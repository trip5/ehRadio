#include "options.h"
#include "utility.h"

#include <ESPFileUpdater.h>
#include <WiFi.h>
#include <ctype.h>
#include <cstdlib>
#include <cstring>

#ifndef ARDUINO_ESP32C3_DEV
  #include <driver/rtc_io.h>
#endif

#include "config.h"
#include "display.h"
#include "netserver.h"
#ifdef USE_NEXTION
  #include "../displays/nextion.h"
#endif

namespace {

struct LocaleUpdateParams {
  ESPFileUpdater* updater;
  uint8_t clientId;
  char localeCode[16];
};

bool readStationEntry(File& playlist, File& index, uint16_t idx, char* name, char* url, int& ovol) {
  index.seek((idx - 1) * 4, SeekSet);
  uint32_t pos = 0;
  index.readBytes((char*)&pos, 4);
  playlist.seek(pos, SeekSet);
  return utility.parseCSV(playlist.readStringUntil('\n').c_str(), name, url, ovol);
}

} // namespace

uint16_t Utility::sleepfor = 0;

void Utility::sleepCore() {
  if (BRIGHTNESS_PIN != 255) analogWrite(BRIGHTNESS_PIN, 0);
  display.deepsleep();
  #ifdef USE_NEXTION
    nextion.sleep();
  #endif
  #if defined(ARDUINO_ESP32C3_DEV)
    if (WAKE_PIN != 255) {
      esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN), WAKE_PIN_STATE ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
    }
  #else
    if (WAKE_PIN != 255) {
      // Digital GPIO pull resistors power off during deep sleep — configure the RTC domain pull instead
      if (WAKE_PIN_STATE == HIGH) {
        rtc_gpio_pulldown_en((gpio_num_t)WAKE_PIN);
        rtc_gpio_pullup_dis((gpio_num_t)WAKE_PIN);
      } else {
        rtc_gpio_pullup_en((gpio_num_t)WAKE_PIN);
        rtc_gpio_pulldown_dis((gpio_num_t)WAKE_PIN);
      }
      esp_sleep_enable_ext0_wakeup((gpio_num_t)WAKE_PIN, WAKE_PIN_STATE);
    }
  #endif
}

void Utility::doSleep() {
  sleepCore();
  esp_sleep_enable_timer_wakeup(sleepfor * 60 * 1000000ULL);
  esp_deep_sleep_start();
}

namespace {

bool updateLocaleFileCore(ESPFileUpdater* updater, const char* localeCode) {
  #ifdef UPDATEURL
    if (strcmp(localeCode, HARDCODED_WEBUI_LOCALE) == 0) {
      FUNCTIONLOG("Locale", "Updating locale: %s - no need to download, hardcoded locale uses default.", HARDCODED_WEBUI_LOCALE);
      return true;
    }

    char tryFile[64] = "/www/locale.new";
    char finalFile[64] = {0};
    char tryUrl[128] = {0};
    bool success = false;
    FUNCTIONLOG("Locale Update", "Downloading file for %s...", localeCode);
    for (size_t j = 0; j < 2; j++) {
      SPIFFS.remove(tryFile);
      if (j == 0) {
        snprintf(finalFile, sizeof(finalFile), "/www/%s.json.gz", localeCode);
        snprintf(tryUrl, sizeof(tryUrl), "%s%s.json.gz", FILESURL, localeCode);
      } else {
        snprintf(finalFile, sizeof(finalFile), "/www/%s.json", localeCode);
        snprintf(tryUrl, sizeof(tryUrl), "%s%s.json", FILESURL, localeCode);
      }
      ESPFileUpdater::UpdateStatus result = updater->checkAndUpdate(
          tryFile,
          tryUrl,
          "",
          ESPFILEUPDATER_VERBOSE
      );
      if (result == ESPFileUpdater::UPDATED) {
        FUNCTIONLOG("Locale Update", "Download for %s successful, saving as %s", localeCode, finalFile);
        SPIFFS.remove(finalFile);
        if (SPIFFS.rename(tryFile, finalFile)) {
          success = true;
          break;
        }
      }
    }
    if (!success) {
      FUNCTIONLOG("Locale Update", "Failed to fetch file from either .gz or uncompressed URL");
    }
    return success;
  #else
    return false;
  #endif
}

void updateLocaleFileAsyncWrapper(void* param) {
  LocaleUpdateParams* params = (LocaleUpdateParams*)param;
  bool success = updateLocaleFileCore(params->updater, params->localeCode);
  if (success) {
    char oldLocaleGz[64] = {0};
    char oldLocale[64] = {0};
    snprintf(oldLocaleGz, sizeof(oldLocaleGz), "/www/%s.json.gz", config.store.locale_webui);
    snprintf(oldLocale, sizeof(oldLocale), "/www/%s.json", config.store.locale_webui);
    SPIFFS.remove(oldLocaleGz);
    SPIFFS.remove(oldLocale);
    config.saveValue(config.store.locale_webui, params->localeCode);
    FUNCTIONLOG("Locale Update", "Successfully updated to %s", params->localeCode);
    char msg[64] = {0};
    snprintf(msg, sizeof(msg), "{\"locale_updated\":true,\"locale\":\"%s\"}", params->localeCode);
    websocket.text(params->clientId, msg);
  } else {
    FUNCTIONLOG("Locale Update", "Failed to update to %s", params->localeCode);
    websocket.text(params->clientId, "{\"locale_update_failed\":true}");
  }
  delete params->updater;
  delete params;
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL) * 4);
  #endif
  vTaskDelete(NULL);
}

} // namespace

void Utility::stripWhitespace(char* text) {
  if (!text) return;

  size_t len = strlen(text);
  while (len > 0 && (text[len - 1] == '\r' || text[len - 1] == '\n' || isspace(static_cast<unsigned char>(text[len - 1])))) {
    text[--len] = '\0';
  }

  char* start = text;
  while (*start && isspace(static_cast<unsigned char>(*start))) {
    ++start;
  }
  if (start != text) {
    memmove(text, start, strlen(start) + 1);
  }
}

void Utility::stripWrappingQuotes(char* text) {
  if (!text) return;

  size_t len = strlen(text);
  if (len < 2) return;

  if ((text[0] == '"' && text[len - 1] == '"') || (text[0] == '\'' && text[len - 1] == '\'')) {
    memmove(text, text + 1, len - 2);
    text[len - 2] = '\0';
  }
}

char* Utility::ipToStr(IPAddress ip) {
  snprintf(ipBuf, sizeof(ipBuf), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return ipBuf;
}

void Utility::escapeQuotes(const char* input, char* output, size_t maxLen) {
  if (!output || maxLen == 0) return;

  output[0] = '\0';
  if (!input) return;

  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j < maxLen - 1; ++i) {
    if (input[i] == '"') {
      if (j >= maxLen - 2) break;
      output[j++] = '\\';
      output[j++] = '"';
    } else {
      output[j++] = input[i];
    }
  }
  output[j] = '\0';
}

bool Utility::parseCSV(const char* line, char* name, char* url, int& ovol) {
  char* tab = nullptr;
  const char* cursor = line;
  char volumeBuffer[5] = {0};

  if (!cursor || !name || !url) return false;

  tab = strstr(cursor, "\t");
  if (tab == nullptr) return false;
  strlcpy(name, cursor, static_cast<size_t>(tab - cursor) + 1);
  if (strlen(name) == 0) return false;

  cursor = tab + 1;
  tab = strstr(cursor, "\t");
  if (tab == nullptr) return false;
  strlcpy(url, cursor, static_cast<size_t>(tab - cursor) + 1);
  if (strlen(url) == 0) return false;

  cursor = tab + 1;
  if (strlen(cursor) == 0) return false;

  strlcpy(volumeBuffer, cursor, sizeof(volumeBuffer));
  ovol = atoi(volumeBuffer);
  return true;
}

bool Utility::parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize) {
  char* equals = nullptr;

  if (!line || !cmd || !val || cSize == 0) return false;

  equals = strstr(line, "=");
  if (equals == nullptr) return false;

  memset(cmd, 0, cSize);
  strlcpy(cmd, line, static_cast<size_t>(equals - line) + 1);
  memset(val, 0, cSize);
  strlcpy(val, equals + 1, strlen(line) - strlen(cmd) + 1);
  return true;
}

bool Utility::parseSsid(const char* line, char* ssid, char* pass) {
  char* tab = nullptr;

  if (!line || !ssid || !pass) return false;

  tab = strstr(line, "\t");
  if (tab == nullptr) return false;

  uint16_t pos = static_cast<uint16_t>(tab - line);
  if (pos >= sizeof(config.ssids[0].ssid)) return false;
  if (strlen(line + pos + 1) >= sizeof(config.ssids[0].password)) return false;

  memset(ssid, 0, sizeof(config.ssids[0].ssid));
  strlcpy(ssid, line, pos + 1);
  memset(pass, 0, sizeof(config.ssids[0].password));
  strlcpy(pass, line + pos + 1, sizeof(config.ssids[0].password));
  return true;
}

bool Utility::saveWifi(const char* post) {
  char ssidValue[sizeof(config.ssids[0].ssid)] = {0};
  char passValue[sizeof(config.ssids[0].password)] = {0};
  if (parseSsid(post, ssidValue, passValue)) {
    if (addSsid(ssidValue, passValue)) {
      ESP.restart();
      return true;
    }
  }
  return false;
}

bool Utility::addSsid(const char* ssid, const char* password) {
  int slot = -1;
  for (int i = 0; i < config.ssidsCount; i++) {
    if (strcmp(config.ssids[i].ssid, ssid) == 0) {
      slot = i;
      break;
    }
  }

  if (slot == -1) {
    slot = (config.ssidsCount < 5) ? config.ssidsCount : 4;
    if (slot == config.ssidsCount && config.ssidsCount < 5) {
      config.ssidsCount++;
    }
  }

  strlcpy(config.ssids[slot].ssid, ssid, sizeof(config.ssids[0].ssid));
  strlcpy(config.ssids[slot].password, password, sizeof(config.ssids[0].password));
  config.setLastSSID(slot + 1);

  File file = SPIFFS.open(TMP_PATH, "w");
  if (!file) return false;
  for (int i = 0; i < config.ssidsCount; i++) {
    if (strlen(config.ssids[i].ssid) > 0) {
      file.printf("%s\t%s\n", config.ssids[i].ssid, config.ssids[i].password);
    }
  }
  file.close();

  if (SPIFFS.exists(TMP_PATH)) {
    SPIFFS.remove(SSIDS_PATH);
    return SPIFFS.rename(TMP_PATH, SSIDS_PATH);
  }
  return false;
}

bool Utility::importWifi() {
  if (!SPIFFS.exists(TMP_PATH)) return false;
  SPIFFS.remove(SSIDS_PATH);
  SPIFFS.rename(TMP_PATH, SSIDS_PATH);
  ESP.restart();
  return true;
}

void Utility::indexPlaylist() {
  File playlist = SPIFFS.open(PLAYLIST_PATH, "r");
  if (!playlist) return;

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;
  File index = SPIFFS.open(INDEX_PATH, "w");
  while (playlist.available()) {
    uint32_t pos = playlist.position();
    if (parseCSV(playlist.readStringUntil('\n').c_str(), stationName, stationUrl, stationOvol)) {
      index.write((uint8_t*)&pos, 4);
    }
  }
  index.close();
  playlist.close();
}

void Utility::initPlaylist() {
  if (!SPIFFS.exists(INDEX_PATH)) indexPlaylist();
}

uint16_t Utility::playlistLength() {
  uint16_t out = 0;
  if (config.SDPLFS()->exists(REAL_INDEX)) {
    File index = config.SDPLFS()->open(REAL_INDEX, "r");
    out = index.size() / 4;
    index.close();
  }
  return out;
}

bool Utility::loadStation(uint16_t stationId) {
  uint16_t count = playlistLength();
  if (count == 0) {
    memset(config.station.url, 0, STATION_FIELD_LENGTH);
    memset(config.station.name, 0, STATION_FIELD_LENGTH);
    strncpy(config.station.name, "ehRadio", STATION_FIELD_LENGTH);
    config.station.ovol = 0;
    return false;
  }

  if (stationId > count) stationId = 1;

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  if (readStationEntry(playlist, index, stationId, stationName, stationUrl, stationOvol)) {
    memset(config.station.url, 0, STATION_FIELD_LENGTH);
    memset(config.station.name, 0, STATION_FIELD_LENGTH);
    strncpy(config.station.name, stationName, STATION_FIELD_LENGTH);
    strncpy(config.station.url, stationUrl, STATION_FIELD_LENGTH);
    config.station.ovol = stationOvol;
    config.setLastStation(stationId);
  }
  playlist.close();
  index.close();
  return true;
}

uint16_t Utility::findStationByUrl(const char* url) {
  uint16_t count = playlistLength();
  if (count == 0 || url == nullptr || url[0] == '\0') return 0;

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  if (!playlist || !index) {
    if (playlist) playlist.close();
    if (index) index.close();
    return 0;
  }

  for (uint16_t i = 1; i <= count; i++) {
    if (readStationEntry(playlist, index, i, stationName, stationUrl, stationOvol) && strcmp(stationUrl, url) == 0) {
      playlist.close();
      index.close();
      return i;
    }
    if (i % 20 == 0) yield();
  }

  playlist.close();
  index.close();
  return 0;
}

char* Utility::stationByNum(uint16_t num) {
  memset(stationBuf, 0, sizeof(stationBuf));
  uint16_t count = playlistLength();
  if (num < 1 || num > count) return stationBuf;

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  if (!playlist || !index) {
    if (playlist) playlist.close();
    if (index) index.close();
    return stationBuf;
  }

  if (readStationEntry(playlist, index, num, stationName, stationUrl, stationOvol)) {
    strlcpy(stationBuf, stationName, sizeof(stationBuf));
  }
  playlist.close();
  index.close();
  return stationBuf;
}

void Utility::doSleepW() {
  sleepCore();
  esp_deep_sleep_start();
}

void Utility::sleepForAfter(uint16_t sleepfor, uint16_t sa) {
  Utility::sleepfor = sleepfor;
  if (sa > 0) {
    sleepTimer.attach(sa * 60, Utility::doSleep);
  } else {
    Utility::doSleep();
  }
}

void Utility::cleanupSpiffs() {
  FUNCTIONLOG("Cleanup", "Scanning SPIFFS for unwanted files...");
  File root = SPIFFS.open("/");
  if (!root || !root.isDirectory()) return;

  File file = root.openNextFile();
  while (file) {
    if (file.isDirectory()) {
      file = root.openNextFile();
      continue;
    }

    String path = file.path();
    bool keep = false;
    if (path.startsWith("/www/")) {
      String name = path.substring(5);
      char currentLocaleGz[64] = {0};
      char currentLocale[64] = {0};
      #ifdef UPDATEURL
        snprintf(currentLocaleGz, sizeof(currentLocaleGz), "%s.json.gz", config.store.locale_webui);
        snprintf(currentLocale, sizeof(currentLocale), "%s.json", config.store.locale_webui);
      #else
        snprintf(currentLocaleGz, sizeof(currentLocaleGz), "%s.json.gz", WEBUI_LOCALE);
        snprintf(currentLocale, sizeof(currentLocale), "%s.json", WEBUI_LOCALE);
      #endif
      if (name == currentLocaleGz || name == currentLocale) {
        keep = true;
      } else {
        for (size_t i = 0; i < Config::wwwFilesCount; i++) {
          if (name == String(Config::wwwFiles[i]) || name == String(Config::wwwFiles[i]) + ".gz") {
            keep = true;
            break;
          }
        }
      }
      if (keep && !name.endsWith(".gz")) {
        String gzPath = "/www/" + name + ".gz";
        if (SPIFFS.exists(gzPath)) {
          SPIFFS.remove(path);
          FUNCTIONLOG("Cleanup", "Removed duplicate (compressed version exists): %s", path.c_str());
          file = root.openNextFile();
          continue;
        }
      }
    } else if (path.startsWith("/data/")) {
      String name = path.substring(6);
      for (size_t i = 0; i < Config::dataFilesCount; i++) {
        if (name == String(Config::dataFiles[i])) {
          keep = true;
          break;
        }
      }
    }
    if (!keep) {
      SPIFFS.remove(path);
      BOOTLOG("Removed: %s", path.c_str());
    }
    file = root.openNextFile();
  }
  FUNCTIONLOG("Cleanup", "All unnecessary files purged");
}

void Utility::deleteMainwwwFile() {
  if (Config::wwwFilesCount == 0) return;

  const char* lastFile = Config::wwwFiles[Config::wwwFilesCount - 1];
  char mainFile[64] = {0};
  for (const char* suffix : {"", ".gz"}) {
    snprintf(mainFile, sizeof(mainFile), "/www/%s%s", lastFile, suffix);
    if (SPIFFS.exists(mainFile)) {
      SPIFFS.remove(mainFile);
      FUNCTIONLOG("Utility", "Deleted main www file: %s", mainFile);
    }
  }
}

void Utility::updateFile(void* param, const char* localFile, const char* onlineFile, const char* updatePeriod, const char* simpleName) {
  FUNCTIONLOG("ESPFileUpdater", "%s - started update", simpleName);
  ESPFileUpdater* updateFile = (ESPFileUpdater*)param;
  ESPFileUpdater::UpdateStatus result = updateFile->checkAndUpdate(
      localFile,
      onlineFile,
      updatePeriod,
      ESPFILEUPDATER_VERBOSE
  );
  if (result == ESPFileUpdater::UPDATED) {
    FUNCTIONLOG("ESPFileUpdater", "%s - update completed", simpleName);
  } else if (result == ESPFileUpdater::NOT_MODIFIED || result == ESPFileUpdater::MAX_AGE_NOT_REACHED) {
    FUNCTIONLOG("ESPFileUpdater", "%s - no update needed", simpleName);
  } else {
    FUNCTIONLOG("ESPFileUpdater", "%s - update failed", simpleName);
  }
}

void Utility::updateLocaleFile() {
  #ifdef UPDATEURL
    ESPFileUpdater* updater = new ESPFileUpdater(SPIFFS);
    updater->setMaxSize(1024);
    updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    bool success = updateLocaleFileCore(updater, config.store.locale_webui);
    if (success) {
      FUNCTIONLOG("Locale Update", "Successfully updated to %s", config.store.locale_webui);
    } else {
      FUNCTIONLOG("Locale Update", "Failed to update to %s", config.store.locale_webui);
    }
    delete updater;
  #endif
}

bool Utility::updateLocaleFileAsync(const char* localeCode, uint8_t clientId) {
  if (WiFi.status() != WL_CONNECTED) return false;
  #ifdef UPDATEURL
    LocaleUpdateParams* params = new LocaleUpdateParams();
    params->updater = new ESPFileUpdater(SPIFFS);
    params->updater->setMaxSize(1024);
    params->updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    params->clientId = clientId;
    strlcpy(params->localeCode, localeCode, sizeof(params->localeCode));
    if (xTaskCreatePinnedToCore(updateLocaleFileAsyncWrapper, "updateLocaleFileAsyncWrapper", 8192, params, LOW_TASK_PRIORITY, NULL, NETWORK_CORE) != pdPASS) {
      delete params->updater;
      delete params;
      return false;
    }
    return true;
  #else
    config.saveValue(config.store.locale_webui, localeCode);
    FUNCTIONLOG("Locale Switch", "Changed to %s", localeCode);
    char msg[64] = {0};
    snprintf(msg, sizeof(msg), "{\"locale_updated\":true,\"locale\":\"%s\"}", localeCode);
    websocket.text(clientId, msg);
    return true;
  #endif
}

Utility utility;