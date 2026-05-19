#include "options.h"
#include "startup.h"

#include <ESPFileUpdater.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "locale.h"
#include "logging.h"
#include "netserver.h"
#include "player.h"
#include "utility.h"

Startup startup;

namespace {

bool requiredWebFilesExist() {
  char fullPath[64];
  for (size_t i = 0; i < Config::wwwFilesCount; i++) {
    snprintf(fullPath, sizeof(fullPath), "/www/%s", Config::wwwFiles[i]);
    String gzPath = String(fullPath) + ".gz";
    bool plainExists = SPIFFS.exists(fullPath);
    bool gzExists = SPIFFS.exists(gzPath);
    if (gzExists && plainExists) SPIFFS.remove(fullPath);
    if (!plainExists && !gzExists) return false;
  }
  return true;
}

} // namespace

void Startup::checkVerAndSpiffs() {
  String storedVersion = "";
  if (SPIFFS.exists(VERSION_PATH)) {
    File verFile = SPIFFS.open(VERSION_PATH, "r");
    if (verFile) {
      storedVersion = verFile.readStringUntil('\n');
      storedVersion.trim();
      verFile.close();
    }
  }

  if (storedVersion == String(RADIOVERSION)) {
    config.wwwFilesExist = requiredWebFilesExist();
  } else if (!SPIFFS.exists(VERSION_PATH)) {
    BOOTLOG("New install detected.");
    config.wwwFilesExist = requiredWebFilesExist();
  } else {
    BOOTLOG("Version mismatch detected (stored: %s, current: %s)", storedVersion.c_str(), RADIOVERSION);
    config.wwwFilesExist = false;
  }

  if (!config.wwwFilesExist || !SPIFFS.exists(VERSION_PATH)) {
    utility.cleanupSpiffs();
    File verFile = SPIFFS.open(VERSION_PATH, "w");
    if (verFile) {
      verFile.println(RADIOVERSION);
      verFile.close();
      BOOTLOG("Version file updated to %s", RADIOVERSION);
    }
  }

  if (!config.wwwFilesExist) {
    utility.deleteMainwwwFile();
    #ifndef UPDATEURL
      BOOTLOG("SPIFFS is missing files!");
    #else
      BOOTLOG("SPIFFS is missing files.  Will attempt to get files from online...");
    #endif
  }
}

void Startup::initNetwork() {
  File file = SPIFFS.open(SSIDS_PATH, "r");
  if (!file || file.isDirectory()) {
    return;
  }

  config.ssidsCount = 0;
  char ssidValue[sizeof(config.ssids[0].ssid)] = {0};
  char passValue[sizeof(config.ssids[0].password)] = {0};
  while (file.available() && config.ssidsCount < 5) {
    if (utility.parseSsid(file.readStringUntil('\n').c_str(), ssidValue, passValue)) {
      strlcpy(config.ssids[config.ssidsCount].ssid, ssidValue, sizeof(config.ssids[0].ssid));
      strlcpy(config.ssids[config.ssidsCount].password, passValue, sizeof(config.ssids[0].password));
      config.ssidsCount++;
    }
  }
  file.close();
}

void Startup::cleanStaleSearchResults() {
  const char* metaPath = "/www/searchresults.json.meta";
  if (SPIFFS.exists(metaPath)) {
    File metaFile = SPIFFS.open(metaPath, "r");
    metaFile.readStringUntil('\n');
    String timeStr = metaFile.readStringUntil('\n');
    metaFile.close();
    if (timeStr.length() > 0) {
      time_t fileTime = atol(timeStr.c_str());
      time_t now = time(nullptr);
      if (now < 100000000 || (now - fileTime) > 86400) {
        SERIALLOG("Cleaning stale search results.");
        SPIFFS.remove(metaPath);
        SPIFFS.remove("/www/searchresults.json");
        SPIFFS.remove("/www/search.txt");
      }
    }
  }
}

void Startup::fixPlaylistFileEnding() {
  const char* playlistPath = PLAYLIST_PATH;
  if (!SPIFFS.exists(playlistPath)) return;

  File playlistFile = SPIFFS.open(playlistPath, "r+");
  if (!playlistFile) return;

  size_t size = playlistFile.size();
  if (size < 2) {
    playlistFile.close();
    return;
  }

  playlistFile.seek(size - 2, SeekSet);
  char last2[3] = {0};
  playlistFile.read((uint8_t*)last2, 2);
  if (!(last2[0] == '\r' && last2[1] == '\n')) {
    playlistFile.seek(size, SeekSet);
    playlistFile.write((const uint8_t*)"\r\n", 2);
  }
  playlistFile.close();
}

void Startup::getRequiredFiles() {
  #ifdef UPDATEURL
    player.sendCommand({PR_STOP, 0});
    ESPFileUpdater* updater = new ESPFileUpdater(SPIFFS);
    updater->setMaxSize(1024);
    updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    char localFileGz[64];
    char localFile[64];
    char tryFile[64];
    char tryUrl[128];
    display.putRequest(NEWMODE, UPDATING);
    for (size_t i = 0; i < Config::wwwFilesCount; i++) {
      display.updateProgress(LANG::updFiles, (float)(i + 1) / (float)Config::wwwFilesCount);
      const char* fileName = Config::wwwFiles[i];
      snprintf(localFileGz, sizeof(localFileGz), "/www/%s.gz", fileName);
      snprintf(localFile, sizeof(localFile), "/www/%s", fileName);
      if (SPIFFS.exists(localFileGz)) SPIFFS.remove(localFileGz);
      if (SPIFFS.exists(localFile)) SPIFFS.remove(localFile);
      bool success = false;
      for (size_t j = 0; j < 2; j++) {
        if (j == 0) {
          snprintf(tryFile, sizeof(tryFile), "%s", localFileGz);
          snprintf(tryUrl, sizeof(tryUrl), "%s%s.gz", FILESURL, fileName);
        } else {
          snprintf(tryFile, sizeof(tryFile), "%s", localFile);
          snprintf(tryUrl, sizeof(tryUrl), "%s%s", FILESURL, fileName);
        }
        FUNCTIONLOG("ESPFileUpdater", "%s - updating required file...", tryFile);
        ESPFileUpdater::UpdateStatus result = updater->checkAndUpdate(
            tryFile,
            tryUrl,
            "",
            ESPFILEUPDATER_VERBOSE
        );
        if (result == ESPFileUpdater::UPDATED) {
          FUNCTIONLOG("ESPFileUpdater", "%s - download complete", tryFile);
          success = true;
          break;
        } else {
          if (j == 0) FUNCTIONLOG("ESPFileUpdater", "%s - download failed - will try for uncompressed file...", tryFile);
          if (j == 1) FUNCTIONLOG("ESPFileUpdater", "%s - download failed because no online file available. Are you running a custom version?", tryFile);
        }
      }
      if (!success) {
        display.updateProgress(LANG::updFailed, 0.0f);
        delay(3000);
        display.putRequest(NEWMODE, LOST);
        delete updater;
        return;
      }
    }
    delete updater;
    utility.cleanupSpiffs();
    delay(200);
    ESP.restart();
  #endif
}

void Startup::checkNewVersionFile() {
  #ifdef UPDATEURL
    const char* newVERSION_PATH = "/data/new_ver.txt";
    netserver.newVersion = String(RADIOVERSION);
    if (SPIFFS.exists(newVERSION_PATH)) {
      File newVerFile = SPIFFS.open(newVERSION_PATH, "r");
      if (newVerFile) {
        String line = newVerFile.readStringUntil('\n');
        line.trim();
        int versionPos = line.indexOf(VERSIONSTRING);
        if (versionPos >= 0) {
          String extractedVersion = line.substring(versionPos + strlen(VERSIONSTRING));
          extractedVersion.trim();
          if (extractedVersion.length() > 0) {
            netserver.newVersion = extractedVersion;
          }
        }
        newVerFile.close();
      }
    }
    netserver.newVersionAvailable = netserver.newVersion != String(RADIOVERSION);
  #endif
}

bool Startup::checkLocaleFile() {
  if (strcmp(config.store.locale_webui, HARDCODED_WEBUI_LOCALE) == 0) {
    FUNCTIONLOG("Locale Check", "%s uses hardcoded default, no file needed", HARDCODED_WEBUI_LOCALE);
    return true;
  }

  char localeFileGz[64];
  char localeFile[64];
  snprintf(localeFileGz, sizeof(localeFileGz), "/www/%s.json.gz", config.store.locale_webui);
  snprintf(localeFile, sizeof(localeFile), "/www/%s.json", config.store.locale_webui);
  if (SPIFFS.exists(localeFileGz)) {
    FUNCTIONLOG("Locale Check", "Found %s.json.gz", config.store.locale_webui);
    return true;
  }
  if (SPIFFS.exists(localeFile)) {
    FUNCTIONLOG("Locale Check", "Found %s.json", config.store.locale_webui);
    return true;
  }
  FUNCTIONLOG("Locale Check", "Locale file not found for %s", config.store.locale_webui);
  return false;
}

void Startup::startupServicesAsync(void* param) {
  startup.fixPlaylistFileEnding();
  #ifdef UPDATEURL
    if (!startup.checkLocaleFile()) {
      FUNCTIONLOG("Locale Check", "Locale file verification failed, updating to %s...", config.store.locale_webui);
      utility.updateLocaleFile();
    }
    utility.updateFile(param, "/data/new_ver.txt", CHECKUPDATEURL, CHECKUPDATEURL_TIME, "New version number");
    startup.checkNewVersionFile();
    if (config.store.autoupdate && netserver.newVersionAvailable) {
      FUNCTIONLOG("AutoUpdate", "New version detected - starting online update");
      startOnlineUpdate();
    }
  #endif
  #ifdef PLAYLIST_DEFAULT_URL
    if (!SPIFFS.exists("/data/playlist.csv")) {
      utility.updateFile(param, "/data/playlist.csv", PLAYLIST_DEFAULT_URL, "", "Default playlist");
      if (SPIFFS.exists("/data/playlist.csv")) netserver.requestOnChange(PLAYLISTSAVED, 0);
    }
  #endif
  utility.updateFile(param, "/www/timezones.json.gz", TIMEZONES_JSON_URL, TIMEZONES_JSON_CHECKTIME, "Timezones database file");
  utility.updateFile(param, "/www/rb_srvrs.json", RADIO_BROWSER_SERVERS_URL, RB_SERVERS_CHECKTIME, "Radio Browser servers list");
  startup.cleanStaleSearchResults();
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL) * 4);
  #endif
  delete (ESPFileUpdater*)param;
  vTaskDelete(NULL);
}

void Startup::startupServices() {
  if (WiFi.status() != WL_CONNECTED) return;
  #ifdef UPDATEURL
    if (!config.wwwFilesExist) {
      getRequiredFiles();
      return;
    }

    ESPFileUpdater* updater = new ESPFileUpdater(SPIFFS);
    updater->setMaxSize(1024);
    updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    xTaskCreatePinnedToCore(Startup::startupServicesAsync, "startupServicesAsync", 8192, updater, LOW_TASK_PRIORITY, NULL, NETWORK_CORE);
  #endif
}
