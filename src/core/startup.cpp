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
#include "../displays/themeregistry.h"
#ifdef USE_SD
  #include "filemanager.h" // the SD manager parks the startup services the way SD playback does
#endif

Startup startup;

static bool cardInUse(); // the card parks the services - defined with them, below

void Startup::checkSafeMode() {
  if (!config.store.bootStableMarker) {
    _safeMode = true;
    FUNCTIONLOG("SAFE MODE", "Smartstart and Autoupdate suppressed for this session; Web mode saved to NVS.");
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  // Mark this boot as in-progress (not yet proven stable)
  config.saveValue(&config.store.bootStableMarker, false);
  _bootStablePending = true;
}

// The boot-mode glyph for the boot screen line. Deliberately a plain read of the state, with no caching: the caller
// samples it once, before checkSafeMode() clears bootStableMarker, so the glyph reports how the previous boot ended.
const char* Startup::icon() const {
  if (network.offlineMode || config.store.SDoffline) return "\030\031";  // SD_A + SD_B
  if (!config.store.bootStableMarker)                return "\034";      // PAUSE (safe mode)
  if (config.store.smartstart)                       return "\035";      // PLAY (smart start)
  return "\026";                                                         // VOL_75 (default)
}

void Startup::sdOfflineMode() {
  network.status = SDOFFLINE;
  WiFi.mode(WIFI_OFF);
  network.ctimer.attach(1, ticks);  // 1ms heartbeat for player audio callbacks (bitrate, etc.)
}

void Startup::markBootStable(const char* reason) {
  config.saveValue(&config.store.bootStableMarker, true);
  BOOTLOG("Boot stable after %lu ms - %s", millis() - _bootStartMs, reason);
}

void Startup::deferBootStable(const char* reason) {
  if (!_bootStablePending) return;  // already proven, or this boot never had to prove itself
  _bootStartMs = millis();
  _servicesDoneMs = millis();
  FUNCTIONLOG("Boot.stable", "countdown restarted: %s", reason ? reason : "network instability");
}

void Startup::loop() {
  if (!netserver.isListening() && network.status == CONNECTED &&
      (_services != SVC_WILL_RUN || cardInUse())) {
    BOOTLOG("Startup Async Services are finished - starting the WebUI...");
    netserver.begin();
    netserver.startLoopTask();
    display.putRequest(NEWIP, 0);   // the footer IP is the indicator that the WebUI is up
    BOOTLOG("WebUI Ready! Go to http://%s/ to configure", WiFi.localIP().toString().c_str());
    BOOTLOG("-------------------------------------------------------");
  }
  if (!_bootStablePending) return;
  if (_bootStartMs == 0) {
    _bootStartMs = millis();  // First loop() call — setup() (including smartstart) is done
    return;
  }
  // A boot is proven stable only once the startup services have run they are the riskiest thing in the boot:
  // three TLS downloads (version check, timezones database, radio-browser list) against the internal heap
  if (_services == SVC_NONE) {
    if (millis() - _bootStartMs > (BOOT_STABLE_TIME * 1000UL)) {
      char reason[64];
      snprintf(reason, sizeof(reason), "%u s from power-on, no startup services this boot", (unsigned)BOOT_STABLE_TIME);
      markBootStable(reason);
      _bootStablePending = false;
    }
    return;
  }
  // The services can stay suspended for the whole boot (SD mode parks them), and a boot that only parked them is as
  // safe as one that never had them: the downloads are the risk and they never ran. Without this, SD mode could never
  // survive a restart - the next boot came up in safe mode, which forces web mode.
  if (_services == SVC_WILL_RUN && cardInUse() &&
      (millis() - _bootStartMs) > ((STARTUP_ASYNC_SERVICES_DELAY + BOOT_STABLE_TIME) * 1000UL)) {
    // BOOT_STABLE_TIME, as in the other reasons here: the configured wait, not the elapsed time.
    char reason[64];
    snprintf(reason, sizeof(reason), "%u s after the startup services were suspended", (unsigned)BOOT_STABLE_TIME);
    markBootStable(reason);
    _bootStablePending = false;
    return;
  }
  if (_services == SVC_DONE && (millis() - _servicesDoneMs) > (BOOT_STABLE_TIME * 1000UL)) {
    char reason[64];
    snprintf(reason, sizeof(reason), "%u s after the startup services finished", (unsigned)BOOT_STABLE_TIME);
    markBootStable(reason);
    _bootStablePending = false;
  }
}

namespace {


} // namespace

void Startup::deassertCsPins() {
  // Deassert all known SPI chip-select pins before any device init.
  // If a CS pin floats LOW, the device responds to traffic intended for
  // other devices on the same SPI bus, corrupting detection/communication.
  #if VS1053_CS != 255
    pinMode(VS1053_CS, OUTPUT); digitalWrite(VS1053_CS, HIGH);
  #endif
  #if SD_CS != 255 && SD_CS != 254 // 254 = SDMMC sentinel, not a real GPIO
    pinMode(SD_CS, OUTPUT); digitalWrite(SD_CS, HIGH);
  #endif
  #if TFT_CS != 255
    pinMode(TFT_CS, OUTPUT); digitalWrite(TFT_CS, HIGH);
  #endif
  #if TS_CS != 255
    pinMode(TS_CS, OUTPUT); digitalWrite(TS_CS, HIGH);
  #endif
}

void Startup::checkLittleFSandVer() {
  LITTLEFSTIMELOGRESET();
  // The esp_littlefs log tag is lowercase "littlefs", not the Arduino class name.
  esp_log_level_set("littlefs", ESP_LOG_NONE); // Suppress ESP-IDF "littlefs: mount failed, -10025" on first boot.
  // The partition label is passed explicitly: LittleFS.begin() still defaults to "spiffs".
  bool fsReady = LittleFS.begin(false, FS_MOUNT_POINT, FS_MAX_OPEN_FILES, FS_PARTITION_LABEL); // Try mounting without formatting first; if that fails, format explicitly.
  if (!fsReady) {
    BOOTLOG("LittleFS not formatted, formatting now (please be patient)...");
    // Say so on the panel before the format starts: display.init() has run and the boot screen is up, so the display
    // task handles the request while this one blocks on flash erases. putRequest() only queues, hence the pause.
    display.putRequest(FORMATTING, 0);
    delay(50);
    fsReady = LittleFS.begin(true, FS_MOUNT_POINT, FS_MAX_OPEN_FILES, FS_PARTITION_LABEL);
  }
  esp_log_level_set("littlefs", ESP_LOG_ERROR); // allow littlefs logging again
  if (!fsReady) {
    ERRORLOG("LittleFS Mount Failed");
    return;
  }
  BOOTLOG("LittleFS mounted");

  LittleFS.mkdir("/www");
  LittleFS.mkdir("/data");

  LITTLEFSTIMELOG("LittleFS mount & Health setup");

  // Health check: verify LittleFS is readable AND writable (it can be corrupted by an unclean
  // shutdown). Retry up to 3 times with remount; reboot if still broken.
  const bool prevBootClean = config.store.bootStableMarker;
  if (prevBootClean) {
    BOOTLOG("LittleFS health check skipped (previous boot was clean)");
  } else {
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
        if (LittleFS.exists(gzPath)) { probePath = gzPath; probeGz = true; }
        else if (LittleFS.exists(lastPath)) { probePath = lastPath; }

        if (probePath) {
          File f = LittleFS.open(probePath, "r");
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
        BOOTLOG("LittleFS read health check passed");
        break;
      }

      // Phase 2: write + read-back test (deeper — catches write/read failures)
      bool writeOk = false;
      File test = LittleFS.open("/.ehradio_test", "w");
      if (test) {
        test.print('!');
        test.close();
        File rt = LittleFS.open("/.ehradio_test", "r");
        if (rt) {
          writeOk = (rt.read() == '!');
          rt.close();
        }
        LittleFS.remove("/.ehradio_test");
      }
      if (writeOk) {
        healthy = true;
        BOOTLOG("LittleFS write-read health check passed");
        break;
      }
      BOOTLOG("LittleFS health check failed (attempt %d/3), remounting...", attempt);
      LittleFS.end();
      delay(100);
      LittleFS.begin(false, FS_MOUNT_POINT, FS_MAX_OPEN_FILES, FS_PARTITION_LABEL);
    }
    if (!healthy) {
      ERRORLOG("LittleFS health check failed after 3 attempts - rebooting...");
      delay(500);  // flush serial before reboot
      ESP.restart();
    }
  }
  LITTLEFSTIMELOG("Health check");

  String storedVersion = "";
  if (LittleFS.exists(VERSION_PATH)) {
    File verFile = LittleFS.open(VERSION_PATH, "r");
    if (verFile) {
      storedVersion = verFile.readStringUntil('\n');
      storedVersion.trim();
      verFile.close();
    }
  }
  LITTLEFSTIMELOG("Version file read");

  if (storedVersion == String(RADIOVERSION)) {
    config.wwwFilesExist = utility.verifyLittleFS();
  } else if (!LittleFS.exists(VERSION_PATH)) {
    BOOTLOG("New install detected.");
    config.wwwFilesExist = utility.verifyLittleFS();
    // New install — prevent false Safe Mode on first boot
    { Preferences prefs; prefs.begin("ehradio", false);
    prefs.putBool("bootstablemark", true);
    prefs.end(); }
  } else {
    BOOTLOG("Version mismatch detected (stored: %s, current: %s)", storedVersion.c_str(), RADIOVERSION);
    config.wwwFilesExist = false;
  }
  LITTLEFSTIMELOG("wwwFilesExist branch (verifyLittleFS)");

  if (!config.wwwFilesExist || !LittleFS.exists(VERSION_PATH)) {
    utility.pruneLittleFS();
    File verFile = LittleFS.open(VERSION_PATH, "w");
    if (verFile) {
      verFile.println(RADIOVERSION);
      verFile.close();
      BOOTLOG("Version file updated to %s", RADIOVERSION);
    }
  }

  // The custom theme slots are optional files, so they are read after the pruning above - which is
  // what keeps them, and only because their names are in Config::dataFiles[].  config.loadTheme()
  // already ran, back in config.init() before the filesystem was mounted, so it is re-run here:
  // only now can a persisted custom id be told apart from a slot whose file is gone.
  themeLoadSlots();
  const uint8_t themeIdWanted = config.store.themeId;
  config.loadTheme();
  // When the slot WAS gone, the resolved id has to be persisted rather than merely used.  Left in NVS, a
  // stale id comes back to life the moment anything refills that slot - and reflashing LittleFS takes
  // every slot file with it - so the device would change theme on a later boot for no reason the user
  // could see.  This is the only place that can do it correctly: loadTheme()'s earlier call cannot judge a
  // custom id at all, so persisting from there would forget a perfectly good custom theme on every boot.
  if (config.store.themeId != themeIdWanted) config.saveValue(&config.store.themeId, config.store.themeId);

  if (!config.wwwFilesExist) {
    utility.deleteMainwwwFile();
    #ifndef UPDATEURL
      BOOTLOG("LittleFS is missing files!");
    #else
      BOOTLOG("LittleFS is missing files.  Will attempt to get files from online...");
    #endif
  }
  LITTLEFSTIMELOG("Cleanup and write branches");

  // The file log ring starts here: after the mount, after the write-read health check and after the pruning,
  // because anything earlier would race the format-on-failure path.  Nothing is lost by the late start -
  // logRingWrite() buffered boot lines into the RAM ring and the boot banner written here marks the boundary.
  logRingInit();
  LITTLEFSTIMELOG("logRingInit");
}

void Startup::initNetwork() {
  File file = LittleFS.open(SSIDS_PATH, "r");
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
    if (!LittleFS.exists("/data/playlist.csv")) {
      BOOTLOG("Fetching default playlist");
      ESPFileUpdater updater(LittleFS);
      updater.setMaxSize(1024);
      updater.setUserAgent(ESPFILEUPDATER_USERAGENT);
      ESPFileUpdater::UpdateStatus result = updater.checkAndUpdate("/data/playlist.csv", PLAYLIST_DEFAULT_URL, "", ESPFILEUPDATER_VERBOSE);
      // The CSV alone is not a playlist: index.dat carries one offset per line and is the only place the
      // station count comes from.  This is the one writer of the four that did not index what it wrote, and
      // an unindexed CSV is a device that reports no stations until the next boot builds one - see
      // plans/playlist-index-and-weather-hide.md.
      if (LittleFS.exists("/data/playlist.csv")) {
        BOOTLOG("Indexing fetched playlist");
        utility.indexPlaylist();
      }
    }
  #endif
}

void Startup::cleanStaleSearchResults() {
  const char* metaPath = "/www/searchresults.json.meta";
  if (LittleFS.exists(metaPath)) {
    File metaFile = LittleFS.open(metaPath, "r");
    metaFile.readStringUntil('\n');
    String timeStr = metaFile.readStringUntil('\n');
    metaFile.close();
    if (timeStr.length() > 0) {
      time_t fileTime = atol(timeStr.c_str());
      time_t now = time(nullptr);
      if (now < 100000000 || (now - fileTime) > 86400) {
        SERIALLOG("Cleaning stale search results.");
        LittleFS.remove(metaPath);
        LittleFS.remove("/www/searchresults.json");
        LittleFS.remove("/www/search.txt");
      }
    }
  }
}

void Startup::getRequiredFiles() {
  #ifdef UPDATEURL
    player.sendCommand({PR_STOP, 0});
    ESPFileUpdater* updater = new ESPFileUpdater(LittleFS);
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
      if (LittleFS.exists(localFileGz)) LittleFS.remove(localFileGz);
      if (LittleFS.exists(localFile)) LittleFS.remove(localFile);
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
    utility.pruneLittleFS();
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
    if (LittleFS.exists(newVERSION_PATH)) {
      File newVerFile = LittleFS.open(newVERSION_PATH, "r");
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


// True while the card has to be left alone: either the player is reading it (SD playback) or the manager is rewriting it
// SD playback needs the park because it uses DRAM for SPI reads plus MP3 decoding, and the updater's SSL downloads would starve both and drain the audio buffer;
// SD File Manager needs the same park because the user is editing the card, so nothing may open files or start playback underneath it
static bool cardInUse() {
  #ifdef USE_SD
    if (filemanager.active()) return true;
  #endif
  return config.getMode() == PM_SDCARD;
}

void Startup::startupServicesAsync(void* param) {
  // Wait until the card is free - SD playback or an open manager both count. The goto restarts the whole
  // wait if the card becomes busy again during the countdown.
wait_for_online:
  if (cardInUse()) FUNCTIONLOG("Services", "Startup Async Services will not begin while in SD Mode or the SD Manager", STARTUP_ASYNC_SERVICES_DELAY);
  while (cardInUse()) {
    vTaskDelay(pdMS_TO_TICKS(2000));
  }

  // Let the audio buffer fill before HTTP tasks compete for WiFi; re-checked each second.
  FUNCTIONLOG("Services", "Startup Async Services will begin in %d seconds", STARTUP_ASYNC_SERVICES_DELAY);
  for (int i = 0; i < STARTUP_ASYNC_SERVICES_DELAY; i++) {
    vTaskDelay(pdMS_TO_TICKS(1000));
    if (cardInUse()) goto wait_for_online;
  }

  FUNCTIONLOG("Services", "Startup Async Services starting", STARTUP_ASYNC_SERVICES_DELAY);
  // From here to the end of the task the display chokes its redraw rate, because these three downloads
  // hold a TLS session each on the network core and the audio stream is usually up by the second one.
  startup._servicesBusy = true;
  #ifdef UPDATEURL
    utility.updateFile(param, "/data/new_ver.txt", CHECKUPDATEURL, CHECKUPDATEURL_TIME, "New version check");
    startup.checkNewVersionFile();
    if (!startup.safeMode() && config.store.autoupdate && netserver.newVersionAvailable) {
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
  // Last act of the services, and it is what lets Startup::loop() prove the boot.  Order matters: the
  // timestamp first, then the state, so the countdown can never read DONE with a stale timestamp.
  startup._servicesDoneMs = millis();
  startup._servicesBusy = false;
  startup._services = Startup::SVC_DONE;
  delete (ESPFileUpdater*)param;
  vTaskDelete(NULL);
}

void Startup::startupServices() {
  // Every exit from here means something definite about whether the services will run, and Startup::loop()
  // relies on that: SVC_NONE is the default, so "not connected" needs no assignment, but the running case
  // must be recorded before the task can finish, or a fast download could be missed entirely.
  #ifndef UPDATEURL
    return;
  #else
    if (WiFi.status() != WL_CONNECTED) return;
    if (!config.wwwFilesExist) {
      getRequiredFiles();   // reboots once the files are back
      return;
    }

    ESPFileUpdater* updater = new ESPFileUpdater(LittleFS);
    updater->setMaxSize(1024);
    updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    _services = SVC_WILL_RUN;
    xTaskCreatePinnedToCore(Startup::startupServicesAsync, "startupServicesAsync", 8192, updater, LOW_TASK_PRIORITY, NULL, NETWORK_CORE);
  #endif
}
