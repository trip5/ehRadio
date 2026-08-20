#include "options.h"
#include "startup.h"

#include <ESPFileUpdater.h>
#include <WiFi.h>
#include <time.h>

#include "config.h"
#include "display.h"
#include "logging.h"
#include "network.h"
#include "netserver.h"
#include "player.h"
#include "utility.h"
#include "../locale/dsplocale.h"

Startup startup;

void Startup::checkSafeMode() {
  if (!config.store.bootStableMarker) {
    FUNCTIONLOG("SAFE MODE", "Smartstart and Autoupdate disabled for this session; Web mode saved to NVS.");
    config.store.smartstart = false;
    config.store.autoupdate = false;
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  // Mark this boot as in-progress (not yet proven stable)
  config.saveValue(&config.store.bootStableMarker, false);
  _bootStablePending = true;
}

void Startup::sdOfflineMode() {
  network.status = SDOFFLINE;
  WiFi.mode(WIFI_OFF);
  network.ctimer.attach(1, ticks);  // 1ms heartbeat for player audio callbacks (bitrate, etc.)
}

void Startup::markBootStable() {
  config.saveValue(&config.store.bootStableMarker, true);
  BOOTLOG("Boot stable after %lu ms", millis() - _bootStartMs);
}

void Startup::loop() {
  if (!_bootStablePending) return;
  if (_bootStartMs == 0) {
    _bootStartMs = millis();  // First loop() call — setup() (including smartstart) is done
    return;
  }
  if (millis() - _bootStartMs > (BOOT_STABLE_TIME * 1000UL)) {
    markBootStable();
    _bootStablePending = false;
  }
}

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

void Startup::deassertCsPins() {
  // Deassert all known SPI chip-select pins before any device init.
  // If a CS pin floats LOW, the device responds to traffic intended for
  // other devices on the same SPI bus, corrupting detection/communication.
  #if VS1053_CS != 255
    pinMode(VS1053_CS, OUTPUT); digitalWrite(VS1053_CS, HIGH);
  #endif
  #if SD_CS != 255
    pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  #endif
  #if TFT_CS != 255
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  #endif
  #if TS_CS != 255
    pinMode(TS_CS, OUTPUT); digitalWrite(TS_CS, HIGH);
  #endif
}

void Startup::checkSpiffsandVer() {
  esp_log_level_set("SPIFFS", ESP_LOG_NONE); // Suppress ESP-IDF "SPIFFS: mount failed, -10025" on first boot.
  bool spiffsReady = SPIFFS.begin(false); // Try mounting without formatting first; if that fails, format explicitly.
  if (!spiffsReady) {
    BOOTLOG("SPIFFS not formatted, formatting now (please be patient)...");
    spiffsReady = SPIFFS.begin(true);
  }
  esp_log_level_set("SPIFFS", ESP_LOG_ERROR); // allow SPIFFS logging again
  if (!spiffsReady) {
    ERRORLOG("SPIFFS Mount Failed");
    return;
  }
  BOOTLOG("SPIFFS mounted");

  // Health check: verify SPIFFS is readable AND writable (can be corrupted after crash).
  // Retry up to 3 times with remount; reboot if still broken.
  {
    bool healthy = false;
    for (int attempt = 1; attempt <= 3; attempt++) {
      // Phase 1: readability — read the last www file (player.html or .gz variant).
      // A valid HTML file starts with whitespace or '<'; a valid gzip starts with 0x1F 0x8B.
      // Use exists() first: open() on a missing file can return a truthy File on some cores.
      bool readable = false;
      if (Config::wwwFilesCount > 0) {
        const char* lastFile = Config::wwwFiles[Config::wwwFilesCount - 1];
        char lastPath[64];
        snprintf(lastPath, sizeof(lastPath), "/www/%s", lastFile);
        char gzPath[64];
        snprintf(gzPath, sizeof(gzPath), "/www/%s.gz", lastFile);

        bool probeGz = false;
        const char* probePath = nullptr;
        if (SPIFFS.exists(gzPath)) { probePath = gzPath; probeGz = true; }
        else if (SPIFFS.exists(lastPath)) { probePath = lastPath; }

        if (probePath) {
          File f = SPIFFS.open(probePath, "r");
          if (f) {
            if (probeGz) {
              int b0 = f.read(), b1 = f.read();
              readable = (b0 == 0x1F && b1 == 0x8B);
            } else {
              int c = f.read();
              readable = (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '<');
            }
            f.close();
          }
        }
      }
      if (readable) {
        healthy = true;
        BOOTLOG("SPIFFS read health check passed");
        break;
      }

      // Phase 2: write + read-back test (deeper — catches write/read failures)
      bool writeOk = false;
      File test = SPIFFS.open("/.ehradio_test", "w");
      if (test) {
        test.print('!');
        test.close();
        File rt = SPIFFS.open("/.ehradio_test", "r");
        if (rt) {
          writeOk = (rt.read() == '!');
          rt.close();
        }
        SPIFFS.remove("/.ehradio_test");
      }
      if (writeOk) {
        healthy = true;
        BOOTLOG("SPIFFS write-read health check passed");
        break;
      }
      BOOTLOG("SPIFFS health check failed (attempt %d/3), remounting...", attempt);
      SPIFFS.end();
      delay(100);
      SPIFFS.begin(false);
    }
    if (!healthy) {
      ERRORLOG("SPIFFS health check failed after 3 attempts - rebooting...");
      delay(500);  // flush serial before reboot
      ESP.restart();
    }
  }

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
    // New install — prevent false Safe Mode on first boot
    { Preferences prefs; prefs.begin("ehradio", false);
    prefs.putBool("bootstablemark", true);
    prefs.end(); }
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
    String line = file.readStringUntil('\n');
    if (line.length() > 0 && line[line.length()-1] == '\r') {
      line = line.substring(0, line.length()-1);
    }
    if (utility.parseSsid(line.c_str(), ssidValue, passValue)) {
      strlcpy(config.ssids[config.ssidsCount].ssid, ssidValue, sizeof(config.ssids[0].ssid));
      strlcpy(config.ssids[config.ssidsCount].password, passValue, sizeof(config.ssids[0].password));
      config.ssidsCount++;
    }
  }
  file.close();
}

void Startup::getDefaultPlaylist() {
  #ifdef PLAYLIST_DEFAULT_URL
    if (!SPIFFS.exists("/data/playlist.csv")) {
      BOOTLOG("Fetching default playlist");
      ESPFileUpdater updater(SPIFFS);
      updater.setMaxSize(1024);
      updater.setUserAgent(ESPFILEUPDATER_USERAGENT);
      ESPFileUpdater::UpdateStatus result = updater.checkAndUpdate("/data/playlist.csv", PLAYLIST_DEFAULT_URL, "", ESPFILEUPDATER_VERBOSE);
    }
  #endif
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
      display.updateProgress(l10n(L10N_MSG_UPD_FILES), (float)(i + 1) / (float)Config::wwwFilesCount);
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
        display.updateProgress(l10n(L10N_MSG_UPD_FAILED), 0.0f);
        delay(3000);
        if (config.getMode() == PM_SDCARD) config.store.play_mode = PM_WEB; // Force away from SD mode so playback can't escape LOST
        player.sendCommand({PR_STOP, 0});
        display.putRequest(NEWMODE, LOST);
        delete updater;
        return;
      }
    }
    delete updater;
    utility.cleanupSpiffs();
    FUNCTIONLOG("REBOOT", "Required Files done. Reboot.");
    config.saveValue(&config.store.bootStableMarker, true);
    delay(250);
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


void Startup::startupServicesAsync(void* param) {
  // Wait until device leaves SD card playback mode before starting
  // background downloads. SD mode uses DRAM for SPI reads + MP3 decoding;
  // ESPFileUpdater's SSL downloads would starve both and drain the audio buffer.
  // The goto allows restarting the entire wait sequence if the user switches
  // back to SD mode during the countdown delay.
wait_for_online:
  if (config.getMode() == PM_SDCARD) FUNCTIONLOG("Services", "Startup Async Services will not begin while in SD Mode", STARTUP_ASYNC_SERVICES_DELAY);
  while (config.getMode() == PM_SDCARD) {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  // Delay to let audio stream buffer fill before background HTTP tasks compete for WiFi.
  // Check mode each second — if user switched back to SD, restart from the top.
  FUNCTIONLOG("Services", "Startup Async Services will begin in %d seconds", STARTUP_ASYNC_SERVICES_DELAY);
  for (int i = 0; i < STARTUP_ASYNC_SERVICES_DELAY; i++) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (config.getMode() == PM_SDCARD) goto wait_for_online;
  }

  FUNCTIONLOG("Services", "Startup Async Services starting", STARTUP_ASYNC_SERVICES_DELAY);
  #ifdef UPDATEURL
    utility.updateFile(param, "/data/new_ver.txt", CHECKUPDATEURL, CHECKUPDATEURL_TIME, "New version check");
    startup.checkNewVersionFile();
    if (config.store.autoupdate && netserver.newVersionAvailable) {
      FUNCTIONLOG("AutoUpdate", "New version detected - starting online update");
      startOnlineUpdate();
    }
  #endif
  utility.updateFile(param, "/www/timezones.json.gz", TIMEZONES_JSON_URL, TIMEZONES_JSON_CHECKTIME, "Timezones database file");
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL) * 4);
  #endif
  utility.updateFile(param, "/www/rb_srvrs.json", RADIO_BROWSER_SERVERS_URL, RB_SERVERS_CHECKTIME, "Radio Browser servers list");
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
