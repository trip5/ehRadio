#include "options.h"
#include <ctype.h>
#include <cstddef>
#include <esp_log.h>
#include <ESPFileUpdater.h>
#include <nvs_flash.h>
#ifndef ARDUINO_ESP32C3_DEV
  #include <driver/rtc_io.h>
#endif
#include "config.h"
#include "backlightcontrols.h"
#include "battery.h"
#include "controls.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "rtcsupport.h"
#include "startup.h"
#include "telnet.h"
#include "utility.h"
#include "../displays/dspcore.h"
#include "../displays/themes.h"
#ifdef USE_SD
  #include "sdmanager.h"
#endif


const char* const Config::wwwFiles[] = {"curated.js", "options.js", "script.js", "script2.js", "search.js",
                                        "logo.svg", "icon.png", "style.css", "theme.css", "rb_srvrs.json", "timezones.json",
                                        "curated.html", "irrecord.html", "options.html", "search.html", "updform.html",
                                        "player.html"}; // keep main page at end (deleted when upgraded, last to be downloaded, so user sees emptyfs_html with wait message)
const size_t Config::wwwFilesCount = sizeof(Config::wwwFiles) / sizeof(Config::wwwFiles[0]);

const char* const Config::dataFiles[] = {PLAYLIST_FILE, SSIDS_FILE, VERSION_FILE};
const size_t Config::dataFilesCount = sizeof(Config::dataFiles) / sizeof(Config::dataFiles[0]);

#if defined(SPI_BUS_SECONDARY)
  SPIClass SPIB(SPI_BUS_SECONDARY);
#endif

Config config;
uint8_t _activeLocale = 0;  // default is whatever is first (likely be_BY), updated at boot from config.store.locale_display

bool wasUpdated(ESPFileUpdater::UpdateStatus status) { return status == ESPFileUpdater::UPDATED; }

namespace {

bool isHttpUrl(const char* url) {
  return url != nullptr && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

} // namespace

void u8fix(char *src) {
  if (strlen(src) == 0) return;
  char last = src[strlen(src)-1]; 
  if ((uint8_t)last >= 0xC2) src[strlen(src)-1]='\0';
}

bool Config::_wwwFilesExist() {
  char fullpath[64];
  for (size_t i = 0; i < Config::wwwFilesCount; i++) {
    sprintf(fullpath, "/www/%s", Config::wwwFiles[i]);
    String gzPath = String(fullpath) + ".gz";
    bool plainExists = SPIFFS.exists(fullpath);
    bool gzExists = SPIFFS.exists(gzPath);
    if (gzExists && plainExists) SPIFFS.remove(fullpath);
    if (!plainExists && !gzExists) return false;
  }
  return true;
}

void Config::init() {
  loadPreferences();
  if (!config.store.bootStableMarker) delay(1000);  // Allow serial monitor to connect before logging in Safe mode
  bootInfo();
  #if RTCSUPPORTED
    BOOTLOG("RTC begin(SDA=%d,SCL=%d)", RTC_SDA, RTC_SCL);
    if (rtc.init()) {
      BOOTLOG("RTC.init done");
      _rtcFound = true;
    } else {
      BOOTLOG("[ERROR] - Couldn't find RTC");
    }
  #endif
  #if defined(SPIA_SCK) && (SPIA_SCK != 255)
    SPI.begin(SPIA_SCK, SPIA_MISO, SPIA_MOSI);
  #endif
  #if defined(SPIB_SCK) && (SPIB_SCK != 255)
    SPIB.begin(SPIB_SCK, SPIB_MISO, SPIB_MOSI);
  #endif
  store.play_mode = store.play_mode & 0b11;
  if (store.play_mode>1) store.play_mode=PM_WEB;
  _initHW();
  #ifdef USE_SD
    _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
  #else
    _SDplaylistFS = &SPIFFS;
  #endif
}

void Config::loadPreferences() {
  prefs.begin("ehradio", false);
  // Check config_set_magic first
  uint16_t configSetValue = 0;
  size_t configSetRead = prefs.getBytes("cfgset", &configSetValue, sizeof(configSetValue));
  if (configSetRead != sizeof(configSetValue) || configSetValue != 1867) {
    if (!netserver.isBootReady()) delay(1000); // this should only happen on FIRST boot but this delay helps catch it in serial
    if (configSetRead != sizeof(configSetValue)) {
      FUNCTIONLOG("Prefs", "NVS Sentinel absent (NVS uninitialized or corrupt), resetting to defaults...");
    } else {
      FUNCTIONLOG("Prefs", "Invalid config_set_magic (%u), resetting config...", configSetValue);
    }
    prefs.end();
    setDefaults();
    return;
  }
  // Load all fields in keyMap
  for (size_t i = 0; keyMap[i].key != nullptr; ++i) {
    uint8_t* field = (uint8_t*)&store + keyMap[i].fieldOffset;
    size_t sz = keyMap[i].size;
    size_t read = prefs.getBytes(keyMap[i].key, field, sz);
  }
  deleteOldKeys();
  prefs.end();
}

void Config::saveLastStationUrl(const char* url, uint16_t waitMs) {
  if (url != nullptr && url[0] != '\0') {
    strlcpy(store.lastStationUrl, url, STATION_FIELD_LENGTH);
  }
  _lastStationUrlDueMs = millis() + waitMs;
}

void Config::changeMode(int newmode) {
  #ifdef USE_SD
    bool pir = player.isRunning();
    if (SD_CS==255) return;
    if (network.status==SDOFFLINE) return;  // no mode switching in offline SD mode
    if (getMode()==PM_SDCARD) {
      sdResumePos = player.getFilePos();
    }
    if (network.status==SOFT_AP || display.mode()==LOST) {
      FUNCTIONLOG("REBOOT", "Marking NVS Pref keys for intentional reboot to SD Offline mode. Rebooting.");
      saveValue(&store.bootStableMarker, true);
      saveValue(&store.SDoffline, true);
      saveValue(&store.play_mode, static_cast<uint8_t>(PM_SDCARD));
      display.putRequest(NEWMODE, CLEAR);
      delay(100);
      ESP.restart();
    }
    if (!sdman.ready && newmode!=PM_WEB) {
      #if SD_CARD_DETECT_PIN!=255
        if (digitalRead(SD_CARD_DETECT_PIN)==HIGH) {
          ERRORLOG("SD card not inserted");
          netserver.requestOnChange(GETPLAYERMODE, 0);
          sdman.stop();
          return;
        }
      #endif
      uint32_t _t_sdstart = millis();
      if (!sdman.start()) {
        ERRORLOG("SD card not found");
        netserver.requestOnChange(GETPLAYERMODE, 0);
        sdman.stop();
        return;
      }
      FUNCTIONLOG("SD", "sdman.start: %lums", millis() - _t_sdstart);
    }
    if (newmode<0||newmode>MAX_PLAY_MODE) {
      store.play_mode++;
      if (getMode() > MAX_PLAY_MODE) store.play_mode=0;
    } else {
      store.play_mode=(playMode_e)newmode;
    }
    saveValue(&store.play_mode, store.play_mode);
    player.resetQueue();  // clear stale ticks commands before mode transition
    _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
    if (getMode()==PM_SDCARD) {
      if (pir) player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, SDCHANGE);
      unsigned long _modeWaitStart = millis();
      while(display.mode()!=SDCHANGE && millis()-_modeWaitStart<2000)
        delay(10);
      delay(50);
    }
    if (getMode()==PM_WEB) {
      if (network.status==SDOFFLINE) {
        FUNCTIONLOG("REBOOT", "Marking NVS Pref key for intentional reboot. Rebooting.");
        saveValue(&store.bootStableMarker, true);
        display.putRequest(NEWMODE, CLEAR);
        delay(100);
        ESP.restart();
      }
      if (pir) {
        player.stopSync();  // synchronous stop — closes SD audio file before unmount
        #if MUTE_PIN!=255
          digitalWrite(MUTE_PIN, MUTE_VAL);  // mute output immediately (DMA buffer still drains)
        #endif
      }
      uint32_t _t_sdstop = millis();
      sdman.stop();
      FUNCTIONLOG("SD", "sdman.stop: %lums", millis() - _t_sdstop);
    }
    if (!_bootDone) return;
    uint32_t _t_plinit = millis();
    initPlaylistMode();
    FUNCTIONLOG("SD", "initPlaylistMode: %lums", millis() - _t_plinit);
    if (pir) player.sendCommand({PR_PLAY, getMode()==PM_WEB?store.lastStation:store.lastSdStation});
    netserver.resetQueue();
    //netserver.requestOnChange(GETPLAYERMODE, 0);
    netserver.requestOnChange(GETINDEX, 0);
    //netserver.requestOnChange(GETMODE, 0);
    // netserver.requestOnChange(CHANGEMODE, 0);
    display.resetQueue();
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(NEWSTATION);
  #endif //#ifdef USE_SD
}

void Config::syncSDFS() {
  #ifdef USE_SD
    _SDplaylistFS = (getMode()==PM_SDCARD) ? (FS*)&sdman : (FS*)&SPIFFS;
  #endif
}

void Config::initSDPlaylist(bool force) {
  #ifdef USE_SD
    bool doIndex = force || !sdman.exists(INDEX_SD_PATH);
    if (!doIndex) {
      File index = sdman.open(INDEX_SD_PATH, "r");  // use sdman directly — SDPLFS() may be SPIFFS after safe mode
      // Footer: [magic:4][count:4] = 8 bytes
      if (index && index.size() >= 12) {  // min: 1 entry (4) + footer (8)
        uint32_t magic, storedCount;
        index.seek(index.size() - 8);
        index.readBytes((char*)&magic, 4);
        index.readBytes((char*)&storedCount, 4);
        uint32_t currentCount = sdman.countAudioFiles();
        FUNCTIONLOG("SD", "Index found:\tcount: %d magic: %04X\tcurrent count: %d",
                    storedCount, magic, currentCount);
        if (magic != 0x1867) {
          FUNCTIONLOG("SD", "Magic mismatch (should be 1867). Re-indexing.");
          doIndex = true;
        } else if (storedCount != currentCount) {
          FUNCTIONLOG("SD", "File count mismatch. Re-indexing.");
          doIndex = true;
        }
      } else {
        FUNCTIONLOG("SD", "Index open failed or too small. Re-indexing.");
        doIndex = true;
      }
      if (index) index.close();
    }
    if (doIndex) {
      FUNCTIONLOG("SD", "Waiting for SD card indexing.");
      sdman.indexSDPlaylist();
      store.countStation = utility.playlistLength();
      lastStation(_randomStation());
      sdResumePos = 0;
    } else {
      store.countStation = utility.playlistLength();
    }
  #endif //#ifdef USE_SD
}

void Config::initPlaylistMode() {
  uint16_t _lastStation = 0;
  uint16_t cs = utility.playlistLength();
  #ifdef USE_SD
    if (getMode()==PM_SDCARD) {
      #if SD_CARD_DETECT_PIN!=255
        if (digitalRead(SD_CARD_DETECT_PIN)==HIGH) {
          if (network.status == SDOFFLINE) {
            FUNCTIONLOG("SD", "No SD card. Staying in offline mode.");
            strncpy(config.station.name, "ehRadio", STATION_FIELD_LENGTH);
            return;  // no SD — nothing more to init
          } else {
            store.play_mode=PM_WEB;
            ERRORLOG("SD card not inserted.");
            changeMode(PM_WEB);
            _lastStation = store.lastStation;
          }
        } else
      #endif
      if (!sdman.ready && !sdman.start()) {
        if (network.status == SDOFFLINE) {
          FUNCTIONLOG("SD", "SD mount failed. Staying in offline mode.");
          strncpy(config.station.name, "ehRadio", STATION_FIELD_LENGTH);
          return;  // no SD — nothing more to init
        } else {
          store.play_mode=PM_WEB;
          ERRORLOG("SD mount failed");
          changeMode(PM_WEB);
          _lastStation = store.lastStation;
        }
      } else {
        FUNCTIONLOG("SD", "SD card mounted.");
        initSDPlaylist();
        cs = utility.playlistLength();  // refresh after potential re-index
        FUNCTIONLOG("SD", "SD card ready");
        _lastStation = store.lastSdStation;
        if (_lastStation > cs || _lastStation == 0) {
          _lastStation = (cs > 0) ? 1 : 0;
        }
      }
    } else {
      FUNCTIONLOG("SD", "done");
      _lastStation = store.lastStation;
    }
  #else
    store.play_mode=PM_WEB;
    _lastStation = store.lastStation;
  #endif //ifdef USE_SD
  if (getMode()==PM_WEB && _wwwFilesExist()) {
    utility.initPlaylist();
    cs = utility.playlistLength();
  }
  log_i("%d" ,_lastStation);
  // Validate station number is within range
  if (cs == 0) {
    _lastStation = 0;  // No playlist, no valid station
  } else if (_lastStation > cs) {
    _lastStation = 1;  // Station out of range, reset to first
  } else if (_lastStation == 0) {
    if (getMode() == PM_WEB) {
      uint16_t found = utility.findStationByUrl(store.lastStationUrl);
      if (found > 0) {
        _lastStation = found;  // Resolved from persisted URL
      }
      // else: stays 0 — will fall through to "ehRadio" below
    } else {
      _lastStation = _randomStation();  // SD mode: pick a random track
    }
  }
  lastStation(_lastStation);
  saveValue(&store.play_mode, store.play_mode);
  _bootDone = true;
  if (_lastStation == 0 && cs > 0) {
    // Playlist exists but we couldn't determine the last station:
    // show "ehRadio" instead of misleading first-station name
    memset(config.station.url, 0, STATION_FIELD_LENGTH);
    memset(config.station.name, 0, STATION_FIELD_LENGTH);
    strncpy(config.station.name, "ehRadio", STATION_FIELD_LENGTH);
    config.station.ovol = 0;
  } else {
    utility.loadStation(_lastStation);
  }
}

void Config::_initHW() {
  loadTheme();
  #if IR_PIN!=255
    prefs.begin("ehradio", false);
    memset(&ircodes, 0, sizeof(ircodes));
    size_t read = prefs.getBytes("ircodes", &ircodes, sizeof(ircodes));
    if (read != sizeof(ircodes) || ircodes.ir_set != 4224) {
      FUNCTIONLOG("_initHW", "ircodes not initialized or corrupt, resetting...");
      prefs.remove("ircodes");
      memset(ircodes.irVals, 0, sizeof(ircodes.irVals));
    }
    prefs.end();
  #endif
  #if BRIGHTNESS_PIN!=255
    pinMode(BRIGHTNESS_PIN, OUTPUT);
    setBrightness(false);
  #endif
  #if SD_CARD_DETECT_PIN!=255
    pinMode(SD_CARD_DETECT_PIN, INPUT_PULLUP);
  #endif
}

void Config::loadTheme() {
  uint8_t count = sizeof(_themes) / sizeof(_themes[0]);
  if (config.store.themeId >= count) config.store.themeId = 0;
  memcpy_P(&theme, &_themes[config.store.themeId], sizeof(ThemeData));
}

void Config::defaultSettings(const char *val, uint8_t clientId) {
  if (strcmp(val, "controls") == 0) {
    saveValue(&store.smartstart, (bool)SMART_START);
    saveValue(&store.oneclickswitch, (bool)ONE_CLICK_SWITCH);
    saveValue(&store.fliptouch, (bool)TOUCH_FLIP);
    controls.flipTS();
    saveValue(&store.dbgtouch, (bool)TOUCH_DEBUG);
    controls.setEncAcceleration(ROTARY_ACCEL);
    controls.setIRTolerance(IR_TOLERANCE);
    netserver.requestOnChange(GETCONTROLS, clientId);
    return;
  }
  if (strcmp(val, "screen") == 0) {
    saveValue(&store.flipscreen, (bool)SCREEN_FLIP);
    display.flip();
    saveValue(&store.invertdisplay, (bool)SCREEN_INVERT);
    saveValue(&store.inverttitle, INVERT_TITLE);
    saveValue(&store.themeId, (uint8_t)0);
    display.applyTheme(0);
    saveValue(&store.layoutId, (uint8_t)0);
    display.applyLayout(0);
    saveValue(&store.numplaylist, (bool)NUMBERED_PLAYLIST);
    saveValue(&store.clock12, (bool)CLOCK_TWELVE);
    saveValue(&store.bufferbar, (bool)SHOW_BUFFERBAR);
    saveValue(&store.vumeter, (bool)SHOW_VU_METER);
    saveValue(&store.volumepage, (bool)VOLUME_PAGE);
    saveValue(&store.dspon, true);
    store.brightness = (uint8_t)SCREEN_BRIGHTNESS; setBrightness(false);
    saveValue(&store.contrast, (uint8_t)SCREEN_CONTRAST);
    display.setContrast();
    saveValue(&store.screensaverEnabled, (bool)SS_NOTPLAYING);
    saveValue(&store.screensaverBlank, (bool)SS_NOTPLAYING_BLANK);
    saveValue(&store.screensaverTimeout, (uint16_t)SS_NOTPLAYING_TIME);
    saveValue(&store.screensaverPlayingEnabled, (bool)SS_PLAYING);
    saveValue(&store.screensaverPlayingBlank, (bool)SS_PLAYING_BLANK);
    saveValue(&store.screensaverPlayingTimeout, (uint16_t)SS_PLAYING_TIME);
    saveValue(&store.screensaverFullDateTime, (bool)SS_FULL_DATETIME);
    saveValue(&store.dimmingEnabled, (bool)DIMMING_ENABLED);
    saveValue(&store.dimmingTimeout, (uint16_t)DIMMING_TIMEOUT);
    saveValue(&store.dimmingBrightness, (uint8_t)DIMMING_BRIGHTNESS);
    backlightControls.restart();
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETSCREEN, clientId);
    return;
  }
  if (strcmp(val, "locale") == 0) {
    saveValue(store.locale_webui, WEBUI_LOCALE);
    saveValue(store.locale_display, DSP_LOCALE);
    _activeLocale = l10n_findLocale(config.store.locale_display);
    network.buildWeatherString();
    display.putRequest(CLOCK, true);
    saveValue(store.tz_name, TIMEZONE_NAME);
    saveValue(store.tzposix, TIMEZONE_POSIX);
    saveValue(store.sntp1, SNTP_1);
    saveValue(store.sntp2, SNTP_2);
    saveValue(&store.timesyncinterval, (uint8_t)TIME_SYNC_INTERVAL);
    network.forceTimeSync = true;
    network.requestTimeSync(true);
    network.forceTimeSync = true;
    netserver.requestOnChange(GETLOCALE, clientId);
    return;
  }
  if (strcmp(val, "weather") == 0) {
    saveValue(&store.showweather, false);
    saveValue(&store.weathersyncinterval, (uint8_t)WEATHER_SYNC_INTERVAL);
    saveValue(&store.weathertempimp, (bool)WEATHER_TEMPERATURE_F);
    saveValue(&store.weatherpressimp, (bool)WEATHER_PRESSURE_MMHG);
    saveValue(store.weatherwindspeed, WEATHER_WIND_SPEED_UNITS);
    saveValue(&store.weatherfeels, false);
    saveValue(&store.weatherhumidity, false);
    saveValue(&store.weatherpressure, false);
    saveValue(&store.weatherwind, false);
    saveValue(store.weatherlang, WEATHER_LANG_OWM);
    saveValue(store.weatherlat, WEATHER_LAT);
    saveValue(store.weatherlon, WEATHER_LON);
    saveValue(store.weatherapi, WEATHER_API);
    saveValue(&store.weatherelevation, (int16_t)0);
    //saveValue(store.weatherkey, ""); // don't reset API key
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETWEATHER, clientId);
    return;
  }
  if (strcmp(val, "system") == 0) {
    saveValue(&store.wifiscanbest, (bool)WIFI_SCAN_BEST_RSSI);
    saveValue(&store.autoupdate, false);
    saveValue(&store.ehdp, (bool)EHDP);
    saveValue(store.ehdpname, "");
    saveValue(&store.softapdelay, (uint8_t)SOFTAP_REBOOT_DELAY);
    char tmp[MDNS_LENGTH]; snprintf(tmp, MDNS_LENGTH, "ehradio-%x", getChipId()); saveValue(store.mdnsname, tmp);
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETSYSTEM, clientId);
    return;
  }
  if (strcmp(val, "mqtt") == 0) {
    saveValue(&store.mqttenable, false);
    saveValue(store.mqtthost, MQTT_HOST);
    saveValue(&store.mqttport, (uint16_t)MQTT_PORT);
    saveValue(store.mqttuser, MQTT_USER);
    saveValue(store.mqttpass, MQTT_PASS);
    saveValue(store.mqtttopic, MQTT_TOPIC);
    netserver.requestOnChange(GETMQTT, clientId);
    return;
  }
  if (strcmp(val, "battery") == 0) {
    saveValue(&store.battery_adc_ref_mv, (uint16_t)BATTERY_ADC_REF_MV);
    battery.recalcNow();
    netserver.requestOnChange(GETBATTERY, clientId);
    return;
  }
  if (strcmp(val, "1") == 0 || strcmp(val, "") == 0) {
    setDefaults();
    defaultSettings("controls", clientId);
    defaultSettings("screen", clientId);
    defaultSettings("locale", clientId);
    defaultSettings("weather", clientId);
    defaultSettings("system", clientId);
    defaultSettings("mqtt", clientId);
    defaultSettings("battery", clientId);
    return;
  }
}

void Config::setDefaults() {
  FUNCTIONLOG("setDefaults", "Resetting all settings to user defaults!");
  nvs_flash_erase();
  nvs_flash_init();
  // defaults set by struct, except one
  snprintf(store.mdnsname, MDNS_LENGTH, "ehradio-%x", getChipId());
  // Write the sentinel immediately after erase so the next boot finds a valid cfgset
  // and does not loop back into reset() again
  prefs.begin("ehradio", false);
  prefs.putBytes("cfgset", &store.config_set_magic, sizeof(store.config_set_magic));
  prefs.end();
}

void Config::saveIR() {
  #if IR_PIN!=255
    ircodes.ir_set = 4224;
    prefs.begin("ehradio", false);
    size_t written = prefs.putBytes("ircodes", &ircodes, sizeof(ircodes));
    prefs.end();
  #endif
}

void Config::processDeferredSaves() {
  for (uint8_t i = 0; i < DEFERRED_SAVE_SLOTS; ++i) {
    if (_deferredSaves[i].entry != nullptr && (int32_t)(millis() - _deferredSaves[i].dueMs) >= 0) {
      const configKeyMap* e = _deferredSaves[i].entry;
      saveRawValue(e, _deferredSaves[i].data, e->size);
      _deferredSaves[i].entry = nullptr;
    }
  }

  if (_lastStationUrlDueMs != 0 && (int32_t)(millis() - _lastStationUrlDueMs) >= 0) {
    _lastStationUrlDueMs = 0;
    saveValue(store.lastStationUrl, store.lastStationUrl);
  }
}

uint8_t Config::setVolume(uint8_t val) {
  store.volume = val;
  display.putRequest(DRAWVOL);
  netserver.requestOnChange(VOLUME, 0);
  return store.volume;
}

void Config::setTone(int8_t bass, int8_t middle, int8_t treble) {
  saveValueButWait(&store.bass, bass, 5000);
  saveValueButWait(&store.middle, middle, 5000);
  saveValueButWait(&store.treble, treble, 5000);
  player.setTone(store.bass, store.middle, store.treble);
  netserver.requestOnChange(EQUALIZER, 0);
}

uint8_t Config::setLastStation(uint16_t val) {
  lastStation(val);
  return store.lastStation;
}

uint8_t Config::setCountStation(uint16_t val) {
  saveValue(&store.countStation, val);
  return store.countStation;
}

uint8_t Config::setLastSSID(uint8_t val) {
  saveValue(&store.lastSSID, val);
  return store.lastSSID;
}

void Config::setTitle(const char* title) {
  vuThreshold = 0;
  // Keep native UTF-8 title (single source of truth for WebUI/CLI)
  memset(config.station.title, 0, STATION_FIELD_LENGTH);
  strlcpy(config.station.title, title, STATION_FIELD_LENGTH);
  u8fix(config.station.title);
  utility.stripWhitespace(config.station.title);
  netserver.requestOnChange(TITLE, 0);
  display.putRequest(NEWTITLE);
}

void Config::setStation(const char* station) {
  memset(config.station.name, 0, STATION_FIELD_LENGTH);
  strlcpy(config.station.name, station, STATION_FIELD_LENGTH);
  u8fix(config.station.name);
  utility.stripWhitespace(config.station.name);
}  

void Config::setBrightness(bool dosave) {
  #if BRIGHTNESS_PIN!=255
    if (!store.dspon && dosave) {
      display.wakeup();
    }
    analogWrite(BRIGHTNESS_PIN, map(store.brightness, 0, 100, 0, 255));
    if (!store.dspon) store.dspon = true;
    if (dosave) {
      saveValueButWait(&store.brightness, store.brightness, 4000);
      saveValue(&store.dspon, store.dspon);
    }
  #endif
}

void Config::setDspOn(bool dspon, bool saveval) {
  if (saveval) {
    store.dspon = dspon;
    saveValue(&store.dspon, store.dspon);
  }
  if (!dspon) {
    #if BRIGHTNESS_PIN!=255
      analogWrite(BRIGHTNESS_PIN, 0);
    #endif
    display.deepsleep();
  } else {
    display.wakeup();
    #if BRIGHTNESS_PIN!=255
      analogWrite(BRIGHTNESS_PIN, map(store.brightness, 0, 100, 0, 255));
    #endif
  }
}

void Config::bootInfo() {
  BOOTLOG("************************************************");
  BOOTLOG("*               ehRadio %s             *", RADIOVERSION);
  BOOTLOG("************************************************");
  BOOTLOG("------------------------------------------------");
  BOOTLOG("Arduino API:\t%d.%d.%d", ARDUINO / 10000, (ARDUINO / 100) % 100, ARDUINO % 100);
  BOOTLOG("Arduino Core:\t%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  BOOTLOG("GCC Toolchain:\t%s", __VERSION__);
  BOOTLOG("%s:\trev: %d, Cores: %d, PSRAM: %dMB, ID: %d", ESP.getChipModel(), ESP.getChipRevision(), ESP.getChipCores(), (ESP.getPsramSize() + 524288) / 1048576, ({uint64_t _m=ESP.getEfuseMac(); (((_m>>40)&0xFF)<<16)|(((_m>>32)&0xFF)<<8)|((_m>>24)&0xFF);}));
  BOOTLOG("Core Processes:\tMain: 1, Audio: %d, Network: %d, Display: %d", AUDIO_CORE, NETWORK_CORE, DSP_TASK_CORE_ID);
  BOOTLOG("Stack Sizes:\tLoop: %dKB, Display: %dKB, Netserver: %dKB, Network: %dKB", LOOP_TASK_STACK_SIZE, DSP_TASK_STACK_SIZE, NETSERVER_TASK_STACK_SIZE, NETWORK_TASK_STACK_SIZE);
  BOOTLOG("Task Priority:\tDisplay: %d, Netserver: %d, Playback: %d, Network: %d, Low: %d", DSP_TASK_PRIORITY, NETSERVER_TASK_PRIORITY, PLAYBACK_TASK_PRIORITY, NET_TASK_PRIORITY, LOW_TASK_PRIORITY);
  #ifdef SPIA_SCK
    if (SPIA_SCK!=255) BOOTLOG("SPIA:\t\tSCK: %d, MISO: %d, MOSI: %d", SPIA_SCK, SPIA_MISO, SPIA_MOSI);
  #endif
  #ifdef SPIB_SCK
    if (SPIB_SCK!=255) BOOTLOG("SPIB:\t\tSCK: %d, MISO: %d, MOSI: %d", SPIB_SCK, SPIB_MISO, SPIB_MOSI);
  #endif
  BOOTLOG("Display %d:\t%s (width: %d, height: %d)", DSP_MODEL, DISPLAY_MODEL_NAME, DSP_WIDTH, DSP_HEIGHT);
  #if I2C_SDA!=255
    BOOTLOG("\t\tI2C SDA: %d, SCL: %d, RST: %d", I2C_SDA, I2C_SCL, I2C_RST);
  #endif
  #if LCD_RS!=255
    BOOTLOG("\t\tLCD RS: %d, E: %d, D4: %d, D5: %d, D6: %d, D7: %d,", LCD_RS, LCD_E, LCD_D4, LCD_D5, LCD_D6, LCD_D7);
  #endif
  #if TFT_DC!=255
    BOOTLOG("\t\tTFT SPIA, CS: %d, RST: %d, DC: %d", TFT_CS, TFT_RST, TFT_DC);
  #endif
  #if DSP_MODEL!=DSP_DUMMY
    BOOTLOG("\t\tInvert Quirk: %s, Brightness Pin: %d, Dimming Enabled: %s", DSP_INVERT_QUIRK?"true":"false", BRIGHTNESS_PIN, DIMMING_ENABLED?"true":"false");
  #endif
  #ifdef AUTOBACKLIGHT
    if (LIGHT_SENSOR!=255) BOOTLOG("Autobacklight Enabled: Light Sensor Pin: %d Max: %d Min: %d", LIGHT_SENSOR, AUTOBACKLIGHT_MAX, AUTOBACKLIGHT_MIN);
  #endif
  #if VS1053_CS!=255
    BOOTLOG("Audio (VS1053):\tSPI%c, CS: %d, DCS: %d, DREQ: %d, RST: %d, Patch: %s", VS1053_SPI_BUS, VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_RST, VS_PATCH_ENABLE?"enabled":"disabled");
  #endif
  #if I2S_DOUT!=255
    BOOTLOG("Audio (I2S):\tDOUT: %d, BCLK: %d, LRC: %d, DIN: %d, MCLK: %d", I2S_DOUT, I2S_BCLK, I2S_LRC, I2S_DIN, I2S_MCLK);
  #endif
  #ifdef USE_ES8311
    BOOTLOGX("Audio (ES8311)\tMAX_I2S: %d");
    #ifdef ES8311_I2C_SCL
      SERIALLOG(" I2C SCL: %d SDA: %d", ES8311_I2C_SCL, ES8311_I2C_SDA);
    #else
      SERIALLOG("");
    #endif
  #endif
  BOOTLOGX("\t\tVolume Scale: %d, Force Mono: %s", VOLUME_SCALE, PLAYER_FORCE_MONO?"true":"false");
  #if MUTE_PIN!=255
    SERIALLOG(", Mute Pin: %d, Mute Val: %d, Mute Lock: %s", MUTE_PIN, MUTE_VAL, MUTE_LOCK?"true":"false");
  #else
    SERIALLOG("");
  #endif
  #if BTN_DOWN!=255
    BOOTLOG("Button Down:\tPin: %d, Pullup: %s%s", BTN_DOWN, BTN_DOWN_PULLUP?"true":"false", _WBTN_DOWN ? ", Wake" : "");
  #endif
  #if BTN_PLAY!=255
    BOOTLOG("Button Play:\tPin: %d, Pullup: %s%s", BTN_PLAY, BTN_PLAY_PULLUP?"true":"false", _WBTN_PLAY ? ", Wake" : "");
  #endif
  #if BTN_UP!=255
    BOOTLOG("Button Up:\tPin: %d, Pullup: %s%s", BTN_UP, BTN_UP_PULLUP?"true":"false", _WBTN_UP ? ", Wake" : "");
  #endif
  #if BTN_PREV!=255
    BOOTLOG("Button Prev:\tPin: %d, Pullup: %s%s", BTN_PREV, BTN_PREV_PULLUP?"true":"false", _WBTN_PREV ? ", Wake" : "");
  #endif
  #if BTN_NEXT!=255
    BOOTLOG("Button Next:\tPin: %d, Pullup: %s%s", BTN_NEXT, BTN_NEXT_PULLUP?"true":"false", _WBTN_NEXT ? ", Wake" : "");
  #endif
  #if BTN_MODE!=255
    BOOTLOG("Button Mode:\tPin: %d, Pullup: %s%s", BTN_MODE, BTN_MODE_PULLUP?"true":"false", _WBTN_MODE ? ", Wake" : "");
  #endif
  #if ENC_DT!=255
    BOOTLOG("Encoder 1:\tDT: %d%s, CLK: %d%s, Pullup: %s, SW: %d%s (Pullup: %s), STEPS: %d",
      ENC_DT, _WENC_DT ? " (wake)" : "",
      ENC_CLK, _WENC_CLK ? " (wake)" : "",
      ENC_PULLUP?"true":"false",
      ENC_SW, _WENC_SW ? " (wake)" : "",
      ENC_SW_PULLUP?"true":"false",
      ENC_STEPS);
  #endif
  #if ENC2_DT!=255
    BOOTLOG("Encoder 2:\tDT: %d%s, CLK: %d%s, Pullup: %s, SW: %d%s (Pullup: %s), STEPS: %d",
      ENC2_DT, _WENC2_DT ? " (wake)" : "",
      ENC2_CLK, _WENC2_CLK ? " (wake)" : "",
      ENC2_PULLUP?"true":"false",
      ENC2_SW, _WENC2_SW ? " (wake)" : "",
      ENC2_SW_PULLUP?"true":"false",
      ENC2_STEPS);
  #endif
  #if TS_INT!=255
    BOOTLOG("Touch INT:\tPin: %d%s", TS_INT, _WTS_INT ? ", Wake" : "");
  #endif
  #ifndef DEEP_SLEEP_DISABLE
    BOOTLOG("Wake GPIOs:\t%llu (mask)", WAKE_GPIO_MASK);
  #else
    BOOTLOG("Wake GPIOs:\tnone (deep sleep disabled)");
  #endif
  #if IR_PIN!=255
    BOOTLOG("IR:\t\tPin: %d", IR_PIN);
  #endif
  #if SD_CS!=255
    #if SD_CARD_DETECT_PIN!=255
      BOOTLOG("SD:\t\tSPI%c Pin: %d Detect Pin: %d Autoplay: %s", SD_SPI, SD_CS, SD_CARD_DETECT_PIN, SD_AUTOPLAY?"true":"false");
    #else
      BOOTLOG("SD:\t\tSPI%c Pin: %d", SD_SPI, SD_CS);
    #endif
  #endif
  #if (TS_MODEL!=TS_MODEL_UNDEFINED)
    #if (TS_CS!=255)
      BOOTLOG("Touchscreen:\t Model: %d, SPI%c CS: %d", TS_MODEL, TS_SPI, TS_CS);
    #else
      BOOTLOG("Touchscreen:\t Model: %d, SDA: %d, SCL: : %d, INT: %d, RST: %d", TS_MODEL, TS_SDA, TS_SCL, TS_INT, TS_RST);
    #endif
  #endif
  #ifdef FIRMWARE
    BOOTLOG("Firmware:\t%s", FIRMWARE);
  #endif
  #ifdef UPDATEURL
    BOOTLOG("Update URL:\t%s", UPDATEURL);
    BOOTLOG("Auto Update:\t%s", store.autoupdate?"true":"false");
  #endif
  #ifdef MQTT_ENABLE
    BOOTLOG("MQTT Enabled:\t%s", store.mqttenable?"true":"false");
  #endif
  BOOTLOGX("AP SSID:\t%s", AP_SSID);
  #ifdef AP_PASSWORD
    SERIALLOG(", Password: %s", AP_PASSWORD);
  #else
    SERIALLOG(" (no password)");
  #endif
  BOOTLOG("Soft AP Delay:\t%d", store.softapdelay);
  #ifdef ALL_DEBUG_LOGS
    BOOTLOG("ALL_DEBUG_LOGS:\tenabled");
  #else
    #if defined(ESPFILEUPDATER_DEBUG)
      BOOTLOG("ESPFILEUPDATER_DEBUG enabled");
    #endif
    #if defined(NETSERVER_DEBUG)
      BOOTLOG("NETSERVER_DEBUG\tenabled");
    #endif
    #if defined(CORE_MONITOR)
      BOOTLOG("CORE_MONITOR\tenabled");
    #endif
  #endif
  #if defined(CORS_DEBUG)
    BOOTLOG("CORS_DEBUG\tenabled");
  #endif
  #if defined(BATTERY_DEBUG)
    BOOTLOG("BATTERY_DEBUG\tenabled");
  #endif
  BOOTLOG("Display Locale:\t%s", store.locale_display);
  BOOTLOG("WebUI Locale:\t%s", store.locale_webui);
  BOOTLOG("Smartstart:\t%s", store.smartstart?"true":"false");
  BOOTLOG("Wifi Scan Best:\t%s", store.wifiscanbest?"true":"false");
  BOOTLOG("------------------------------------------------");
}

// Preferences Look-up Table (store_variable, "key_max_15_char")
// Macro expands to 3 fields (offset_of_config_t_store_variable, "key_max_15_char", size_of_store_variable)
const configKeyMap Config::keyMap[] = {
  // Internal / player state
  CONFIG_KEY_ENTRY(config_set_magic, "cfgset"),
  CONFIG_KEY_ENTRY(lastStationUrl, "lasturl"),
  CONFIG_KEY_ENTRY(countStation, "countsta"),
  CONFIG_KEY_ENTRY(lastSSID, "lastssid"),
  CONFIG_KEY_ENTRY(lastSdStation, "lastsdsta"),
  CONFIG_KEY_ENTRY(play_mode, "playmode"),
  CONFIG_KEY_ENTRY(volume, "vol"),
  CONFIG_KEY_ENTRY(balance, "bal"),
  CONFIG_KEY_ENTRY(treble, "treb"),
  CONFIG_KEY_ENTRY(middle, "mid"),
  CONFIG_KEY_ENTRY(bass, "bass"),
  CONFIG_KEY_ENTRY(sdshuffle, "sdshuffle"),
  // Controls
  CONFIG_KEY_ENTRY(smartstart, "smartstart"),
  CONFIG_KEY_ENTRY(oneclickswitch, "oneclicksw"),
  CONFIG_KEY_ENTRY(fliptouch, "fliptouch"),
  CONFIG_KEY_ENTRY(dbgtouch, "dbgtouch"),
  CONFIG_KEY_ENTRY(encacc, "encaccel"),
  CONFIG_KEY_ENTRY(irtlp, "irtlp"),
  // Screen
  CONFIG_KEY_ENTRY(flipscreen, "flipscr"),
  CONFIG_KEY_ENTRY(invertdisplay, "invdisp"),
  CONFIG_KEY_ENTRY(inverttitle, "inverttitle"),
  CONFIG_KEY_ENTRY(layoutId, "layoutid"),
  CONFIG_KEY_ENTRY(themeId, "themeid"),
  CONFIG_KEY_ENTRY(dspon, "dspon"),
  CONFIG_KEY_ENTRY(numplaylist, "numplaylist"),
  CONFIG_KEY_ENTRY(clock12, "clock12"),
  CONFIG_KEY_ENTRY(bufferbar, "audioinfo"),
  CONFIG_KEY_ENTRY(vumeter, "vumeter"),
  CONFIG_KEY_ENTRY(volumepage, "volpage"),
  CONFIG_KEY_ENTRY(brightness, "bright"),
  CONFIG_KEY_ENTRY(contrast, "contrast"),
  CONFIG_KEY_ENTRY(screensaverEnabled, "scrnsvren"),
  CONFIG_KEY_ENTRY(screensaverBlank, "scrnsvrbl"),
  CONFIG_KEY_ENTRY(screensaverTimeout, "scrnsvrto"),
  CONFIG_KEY_ENTRY(screensaverPlayingEnabled, "scrnsvrplen"),
  CONFIG_KEY_ENTRY(screensaverPlayingBlank, "scrnsvrplbl"),
  CONFIG_KEY_ENTRY(screensaverPlayingTimeout, "scrnsvrplto"),
  CONFIG_KEY_ENTRY(screensaverFullDateTime, "scrnsvrfull"),
  CONFIG_KEY_ENTRY(dimmingEnabled, "dimmingen"),
  CONFIG_KEY_ENTRY(dimmingTimeout, "dimmingto"),
  CONFIG_KEY_ENTRY(dimmingBrightness, "dimmingbr"),
  // Locale
  CONFIG_KEY_ENTRY(locale_webui, "localewebui"),
  CONFIG_KEY_ENTRY(locale_display, "localedsp"),
  CONFIG_KEY_ENTRY(tz_name, "tzname"),
  CONFIG_KEY_ENTRY(tzposix, "tzposix"),
  CONFIG_KEY_ENTRY(sntp1, "sntp1"),
  CONFIG_KEY_ENTRY(sntp2, "sntp2"),
  CONFIG_KEY_ENTRY(timesyncinterval, "timesync"),
  // Weather
  CONFIG_KEY_ENTRY(showweather, "showweather"),
  CONFIG_KEY_ENTRY(weathersyncinterval, "weathersync"),
  CONFIG_KEY_ENTRY(weatherapi, "weatherapi"),
  CONFIG_KEY_ENTRY(weatherlang, "weatherlang"),
  CONFIG_KEY_ENTRY(weatherlat, "weatherlat"),
  CONFIG_KEY_ENTRY(weatherlon, "weatherlon"),
  CONFIG_KEY_ENTRY(weatherkey, "weatherkey"),
  CONFIG_KEY_ENTRY(weatherelevation, "weatherelev"),
  CONFIG_KEY_ENTRY(weathertempimp, "weathertempi"),
  CONFIG_KEY_ENTRY(weatherpressimp, "weatherpressi"),
  CONFIG_KEY_ENTRY(weatherwindspeed, "weatherwindsp"),
  CONFIG_KEY_ENTRY(weatherfeels, "weatherfeels"),
  CONFIG_KEY_ENTRY(weatherhumidity, "weatherhumid"),
  CONFIG_KEY_ENTRY(weatherpressure, "weatherpress"),
  CONFIG_KEY_ENTRY(weatherwind, "weatherwind"),
  // System
  CONFIG_KEY_ENTRY(wifiscanbest, "wifiscan"),
  CONFIG_KEY_ENTRY(autoupdate, "autoupdate"),
  CONFIG_KEY_ENTRY(ehdp, "ehdp"),
  CONFIG_KEY_ENTRY(ehdpname, "ehdpname"),
  CONFIG_KEY_ENTRY(softapdelay, "softapdelay"),
  CONFIG_KEY_ENTRY(mdnsname, "mdnsname"),
  // MQTT
  CONFIG_KEY_ENTRY(mqttenable, "mqttenable"),
  CONFIG_KEY_ENTRY(mqtthost, "mqtthost"),
  CONFIG_KEY_ENTRY(mqttport, "mqttport"),
  CONFIG_KEY_ENTRY(mqttuser, "mqttuser"),
  CONFIG_KEY_ENTRY(mqttpass, "mqttpass"),
  CONFIG_KEY_ENTRY(mqtttopic, "mqtttopic"),
  // Battery
  CONFIG_KEY_ENTRY(battery_adc_ref_mv, "battref"),
  CONFIG_KEY_ENTRY(bootStableMarker, "bootstablemark"),
  CONFIG_KEY_ENTRY(SDoffline, "sdoffline"),
  {0, nullptr, 0} // Yup, 3 fields - don't delete the last line!
};

void Config::deleteOldKeys() {
  // List any old/legacy keys to remove here (they will be deleted from prefs if found)
  prefs.remove("lastbootgood"); // previous name made logs confusing (now bootstablemark)
  prefs.remove("smartstartx"); // why the x...?
  prefs.remove("skipplupdn"); // replaced by oneclickswitch
  prefs.remove("showwthr"); // replaced by showweather
  // none yet
}

