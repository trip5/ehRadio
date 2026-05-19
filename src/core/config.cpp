#include "options.h"
#include <ctype.h>
#include <cstddef>
#include <ESPFileUpdater.h>
#include <nvs_flash.h>
#ifndef ARDUINO_ESP32C3_DEV
  #include <driver/rtc_io.h>
#endif
#include "config.h"
#include "backlightcontrols.h"
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
#include "../displays/tools/utf8_common.h"
#ifdef USE_SD
  #include "sdmanager.h"
#endif
#ifdef USE_NEXTION
  #include "../displays/nextion.h"
#endif


const char* const Config::wwwFiles[] = {"curated.js", "dragpl.js", "locale.js", "options.js", "script.js", "script2.js", "search.js",
                                        "logo.svg", "icon.png", "locales.json", "rb_srvrs.json", "timezones.json", "style.css", "theme.css",
                                        "curated.html", "irrecord.html", "options.html", "search.html", "updform.html",
                                        "player.html"}; // keep main page at end (deleted when upgraded, last to be downloaded, so user sees emptyfs_html with wait message)
const size_t Config::wwwFilesCount = sizeof(Config::wwwFiles) / sizeof(Config::wwwFiles[0]);

const char* const Config::dataFiles[] = {PLAYLIST_FILE, SSIDS_FILE, VERSION_FILE, LASTSTATION_URL_FILE};
const size_t Config::dataFilesCount = sizeof(Config::dataFiles) / sizeof(Config::dataFiles[0]);

#if defined(SPI_BUS_SECONDARY)
  SPIClass SPIB(SPI_BUS_SECONDARY);
#endif

Config config;

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
  if (store.config_set != 4262) {
    setDefaults();
  }
  store.play_mode = store.play_mode & 0b11;
  if (store.play_mode>1) store.play_mode=PM_WEB;
  _initHW();
  if (!SPIFFS.begin(true)) {
    ERRORLOG("SPIFFS Mount Failed");
    return;
  }
  BOOTLOG("SPIFFS mounted");
  startup.checkVerAndSpiffs();

  #ifdef USE_SD
    _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
  #else
    _SDplaylistFS = &SPIFFS;
  #endif
  loadLastStationUrl();
}

void Config::loadPreferences() {
  prefs.begin("ehradio", false);
  // Check config_set first
  uint16_t configSetValue = 0;
  size_t configSetRead = prefs.getBytes("cfgset", &configSetValue, sizeof(configSetValue));
  if (configSetRead != sizeof(configSetValue) || configSetValue != 4262) {
    if (configSetRead != sizeof(configSetValue)) {
      FUNCTIONLOG("Prefs", "NVS Sentinel absent (NVS uninitialized or corrupt), resetting to defaults...");
    } else {
      FUNCTIONLOG("Prefs", "Invalid config_set (%u), resetting config...", configSetValue);
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

void Config::loadLastStationUrl() {
  memset(_lastStationUrl, 0, sizeof(_lastStationUrl));
  _lastStationUrlDirty = false;
  _lastStationUrlDueMs = 0;

  if (!SPIFFS.exists(LASTSTATION_URL_PATH)) return;

  File file = SPIFFS.open(LASTSTATION_URL_PATH, "r");
  if (!file) return;

  String url = file.readStringUntil('\n');
  file.close();
  url.trim();
  if (!isHttpUrl(url.c_str())) {
    FUNCTIONLOG("Config.lasturl", "Ignoring invalid laststation.url contents");
    return;
  }

  strlcpy(_lastStationUrl, url.c_str(), sizeof(_lastStationUrl));
}

void Config::setLastStationUrl(const char* url, uint16_t waitMs) {
  char normalizedUrl[MQTT_URL_SIZE + 1] = {0};
  if (url != nullptr) strlcpy(normalizedUrl, url, sizeof(normalizedUrl));
  utility.stripWhitespace(normalizedUrl);

  if (normalizedUrl[0] != '\0' && !isHttpUrl(normalizedUrl)) return;

  bool changed = strcmp(_lastStationUrl, normalizedUrl) != 0;
  if (changed) {
    strlcpy(_lastStationUrl, normalizedUrl, sizeof(_lastStationUrl));
  }

  if (changed || _lastStationUrlDirty) {
    _lastStationUrlDirty = true;
    _lastStationUrlDueMs = millis() + waitMs;
  }
}

bool Config::_writeLastStationUrlFile() {
  if (_lastStationUrl[0] == '\0') {
    if (!SPIFFS.exists(LASTSTATION_URL_PATH)) return true;
    return SPIFFS.remove(LASTSTATION_URL_PATH);
  }

  File file = SPIFFS.open(LASTSTATION_URL_PATH, "w");
  if (!file) return false;

  bool wroteAll = file.print(_lastStationUrl) == strlen(_lastStationUrl);
  file.close();
  return wroteAll;
}

void Config::flushLastStationUrl() {
  if (!_lastStationUrlDirty) return;

  if (_writeLastStationUrlFile()) {
    _lastStationUrlDirty = false;
    return;
  }

  _lastStationUrlDueMs = millis() + 1000;
}

void Config::changeMode(int newmode) {
  #ifdef USE_SD
    bool pir = player.isRunning();
    if (SD_CS==255) return;
    if (getMode()==PM_SDCARD) {
      sdResumePos = player.getFilePos();
    }
    if (network.status==SOFT_AP || display.mode()==LOST) {
      saveValue(&store.play_mode, static_cast<uint8_t>(PM_SDCARD));
      delay(50);
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
      if (!sdman.start()) {
        ERRORLOG("SD card not found");
        netserver.requestOnChange(GETPLAYERMODE, 0);
        sdman.stop();
        return;
      }
    }
    if (newmode<0||newmode>MAX_PLAY_MODE) {
      store.play_mode++;
      if (getMode() > MAX_PLAY_MODE) store.play_mode=0;
    } else {
      store.play_mode=(playMode_e)newmode;
    }
    saveValue(&store.play_mode, store.play_mode);
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
      if (network.status==SDREADY) ESP.restart();
      sdman.stop();
    }
    if (!_bootDone) return;
    initPlaylistMode();
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

void Config::initSDPlaylist() {
  #ifdef USE_SD
    //store.countStation = 0;
    bool doIndex = !sdman.exists(INDEX_SD_PATH);
    if (doIndex) sdman.indexSDPlaylist();
    if (SDPLFS()->exists(INDEX_SD_PATH)) {
      File index = SDPLFS()->open(INDEX_SD_PATH, "r");
      //store.countStation = index.size() / 4;
      if (doIndex) {
        lastStation(_randomStation());
        sdResumePos = 0;
      }
      index.close();
      //saveValue(&store.countStation, store.countStation);
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
          store.play_mode=PM_WEB;
          ERRORLOG("SD card not inserted");
          changeMode(PM_WEB);
          _lastStation = store.lastStation;
        } else
      #endif
      if (!sdman.start()) {
        store.play_mode=PM_WEB;
        ERRORLOG("SD mount failed");
        changeMode(PM_WEB);
        _lastStation = store.lastStation;
      } else {
        if (_bootDone) FUNCTIONLOG("SD", "SD card mounted"); else BOOTLOG("SD card mounted");
          if (_bootDone) FUNCTIONLOG("SD", "Waiting for SD card indexing..."); else BOOTLOGX("Waiting for SD card indexing...\t");
          initSDPlaylist();
          if (_bootDone) FUNCTIONLOG("SD", "done"); else SERIALLOG("done");
          _lastStation = store.lastSdStation;
          
          if (_lastStation>cs && cs>0) {
            _lastStation=1;
          }
          if (_lastStation==0) {
            _lastStation = _randomStation();
          }
      }
    } else {
      if (_bootDone) FUNCTIONLOG("SD", "done"); else BOOTLOG("SD card done");
      _lastStation = store.lastStation;
    }
  #else
    store.play_mode=PM_WEB;
    _lastStation = store.lastStation;
  #endif //ifdef USE_SD
  if (getMode()==PM_WEB && _wwwFilesExist()) utility.initPlaylist();
  log_i("%d" ,_lastStation);
  // Validate station number is within range
  if (cs == 0) {
    _lastStation = 0;  // No playlist, no valid station
  } else if (_lastStation > cs) {
    _lastStation = 1;  // Station out of range, reset to first
  } else if (_lastStation == 0) {
    _lastStation = getMode()==PM_WEB?1:_randomStation();  // No station selected, pick first
  }
  lastStation(_lastStation);
  saveValue(&store.play_mode, store.play_mode);
  _bootDone = true;
  utility.loadStation(_lastStation);
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

uint16_t Config::color565(uint8_t r, uint8_t g, uint8_t b)
{
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void Config::loadTheme() {
  theme.background    = color565(COLOR_BACKGROUND);
  theme.meta          = color565(COLOR_STATION_NAME);
  theme.metabg        = color565(COLOR_STATION_BG);
  theme.metafill      = color565(COLOR_STATION_FILL);
  theme.title1        = color565(COLOR_SNG_TITLE_1);
  theme.title2        = color565(COLOR_SNG_TITLE_2);
  theme.digit         = color565(COLOR_DIGITS);
  theme.div           = color565(COLOR_DIVIDER);
  theme.weather       = color565(COLOR_WEATHER);
  theme.vumax         = color565(COLOR_VU_MAX);
  theme.vumin         = color565(COLOR_VU_MIN);
  theme.clock         = color565(COLOR_CLOCK);
  theme.clockbg       = color565(COLOR_CLOCK_BG);
  theme.seconds       = color565(COLOR_SECONDS);
  theme.dow           = color565(COLOR_DAY_OF_W);
  theme.date          = color565(COLOR_DATE);
  theme.heap          = color565(COLOR_HEAP);
  theme.buffer        = color565(COLOR_BUFFER);
  theme.ip            = color565(COLOR_IP);
  theme.vol           = color565(COLOR_VOLUME_VALUE);
  theme.rssi          = color565(COLOR_RSSI);
  theme.battery       = color565(COLOR_BATTERY);
  theme.bitrate       = color565(COLOR_BITRATE);
  theme.volbarout     = color565(COLOR_VOLBAR_OUT);
  theme.volbarin      = color565(COLOR_VOLBAR_IN);
  theme.plcurrent     = color565(COLOR_PL_CURRENT);
  theme.plcurrentbg   = color565(COLOR_PL_CURRENT_BG);
  theme.plcurrentfill = color565(COLOR_PL_CURRENT_FILL);
  theme.playlist[0]   = color565(COLOR_PLAYLIST_0);
  theme.playlist[1]   = color565(COLOR_PLAYLIST_1);
  theme.playlist[2]   = color565(COLOR_PLAYLIST_2);
  theme.playlist[3]   = color565(COLOR_PLAYLIST_3);
  theme.playlist[4]   = color565(COLOR_PLAYLIST_4);
  #include "../displays/tools/tftinverttitle.h"
}

void Config::defaultSettings(const char *val, uint8_t clientId) {
  if (strcmp(val, "system") == 0) {
    saveValue(&store.smartstart, (bool)SMART_START);
    saveValue(&store.audioinfo, (bool)SHOW_AUDIO_INFO);
    saveValue(&store.vumeter, (bool)SHOW_VU_METER);
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
  if (strcmp(val, "screen") == 0) {
    saveValue(&store.flipscreen, (bool)SCREEN_FLIP);
    saveValue(&store.volumepage, (bool)VOLUME_PAGE);
    saveValue(&store.clock12, (bool)CLOCK_TWELVE);
    display.flip();
    saveValue(&store.invertdisplay, (bool)SCREEN_INVERT);
    display.invert();
    saveValue(&store.dspon, true);
    store.brightness = (uint8_t)SCREEN_BRIGHTNESS; setBrightness(false);
    saveValue(&store.contrast, (uint8_t)SCREEN_CONTRAST);
    display.setContrast();
    saveValue(&store.numplaylist, (bool)NUMBERED_PLAYLIST);
    saveValue(&store.screensaverEnabled, (bool)SS_NOTPLAYING);
    saveValue(&store.screensaverTimeout, (uint16_t)SS_NOTPLAYING_TIME);
    saveValue(&store.screensaverBlank, (bool)SS_NOTPLAYING_BLANK);
    saveValue(&store.screensaverPlayingEnabled, (bool)SS_PLAYING);
    saveValue(&store.screensaverPlayingTimeout, (uint16_t)SS_PLAYING_TIME);
    saveValue(&store.screensaverPlayingBlank, (bool)SS_PLAYING_BLANK);
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
    saveValue(store.weatherlang, WEATHER_LANG);
    saveValue(store.weatherlat, WEATHER_LAT);
    saveValue(store.weatherlon, WEATHER_LON);
    saveValue(store.weatherapi, WEATHER_API);
    saveValue(&store.weatherelevation, (int16_t)0);
    //saveValue(store.weatherkey, ""); // don't reset API key
    display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER);
    netserver.requestOnChange(GETWEATHER, clientId);
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
  if (strcmp(val, "controls") == 0) {
    saveValue(&store.volsteps, (uint8_t)VOLUME_STEPS);
    saveValue(&store.fliptouch, (bool)TOUCH_FLIP);
    controls.flipTS();
    saveValue(&store.dbgtouch, (bool)TOUCH_DEBUG);
    saveValue(&store.skipPlaylistUpDown, (bool)ONE_CLICK_SWITCH);
    controls.setEncAcceleration(ROTARY_ACCEL);
    controls.setIRTolerance(IR_TOLERANCE);
    netserver.requestOnChange(GETCONTROLS, clientId);
    return;
  }
  if (strcmp(val, "1") == 0 || strcmp(val, "") == 0) {
    setDefaults();
    defaultSettings("system", clientId);
    defaultSettings("screen", clientId);
    defaultSettings("controls", clientId);
    defaultSettings("locale", clientId);
    defaultSettings("weather", clientId);
    defaultSettings("mqtt", clientId);
    return;
  }
}

void Config::setDefaults() {
  SERIALLOG("setDefaults called");
  nvs_flash_erase();
  nvs_flash_init();
  // defaults set by struct, except one
  snprintf(store.mdnsname, MDNS_LENGTH, "ehradio-%x", getChipId());
  // Write the sentinel immediately after erase so the next boot finds a valid cfgset
  // and does not loop back into reset() again
  prefs.begin("ehradio", false);
  prefs.putBytes("cfgset", &store.config_set, sizeof(store.config_set));
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

  if (_lastStationUrlDirty && (int32_t)(millis() - _lastStationUrlDueMs) >= 0) {
    flushLastStationUrl();
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
  #ifdef USE_NEXTION
    nextion.wake();
    char cmd[15];
    snprintf(cmd, 15, "dims=%d", store.brightness);
    nextion.putcmd(cmd);
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
  #ifdef USE_NEXTION
    if (!dspon) nextion.sleep();
    else nextion.wake();
  #endif
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
  BOOTLOG("arduino:\t%d", ARDUINO);
  BOOTLOG("compiler:\t%s", __VERSION__);
  BOOTLOG("esp32core:\t%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
  uint32_t chipId = 0;
  for(int i=0; i<17; i=i+8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  BOOTLOG("chip:\t\tmodel: %s | rev: %d | id: %d | cores: %d | psram: %d", ESP.getChipModel(), ESP.getChipRevision(), chipId, ESP.getChipCores(), ESP.getPsramSize());
  BOOTLOG("display:\t%d", DSP_MODEL);
  if (VS1053_CS==255) {
    BOOTLOG("audio:\t\t%s (%d, %d, %d)", "I2S", I2S_DOUT, I2S_BCLK, I2S_LRC);
  } else {
    BOOTLOG("audio:\t\t%s (%d, %d, %d, %d)", "VS1053", VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_RST);
  }
  BOOTLOG("display locale :\t%s", DSP_LOCALE);
  BOOTLOG("webui locale :\t%s", store.locale_webui);
  BOOTLOG("audioinfo:\t%s", store.audioinfo?"true":"false");
  BOOTLOG("smartstart:\t%s", store.smartstart ? "true" : "false");
  BOOTLOG("vumeter:\t%s", store.vumeter?"true":"false");
  BOOTLOG("softapdelay:\t%d", store.softapdelay);
  BOOTLOG("flipscreen:\t%s", store.flipscreen?"true":"false");
  BOOTLOG("volumepage:\t%s", store.volumepage?"true":"false");
  BOOTLOG("clock12:\t%s", store.clock12?"true":"false");
  BOOTLOG("invertdisplay:\t%s", store.invertdisplay?"true":"false");
  BOOTLOG("showweather:\t%s", store.showweather?"true":"false");
  BOOTLOG("wifiscanbest:\t%s", store.wifiscanbest?"true":"false");
  BOOTLOG("mqttenable:\t%s", store.mqttenable?"true":"false");
  BOOTLOG("buttons:\tleft=%d, center=%d, right=%d, up=%d, down=%d, mode=%d, pullup=%s", 
          BTN_PREV, BTN_PLAY, BTN_NEXT, BTN_UP, BTN_DOWN, BTN_MODE);
  BOOTLOG("encoders:\tl1=%d, b1=%d, r1=%d, pullup=%s, l2=%d, b2=%d, r2=%d, pullup=%s", 
          ENC_BTNL, ENC_BTNB, ENC_BTNR, ENC_PULLUP?"true":"false", ENC2_BTNL, ENC2_BTNB, ENC2_BTNR, ENC2_PULLUP?"true":"false");
  BOOTLOG("ir:\t\t%d", IR_PIN);
  if (SD_CS!=255) BOOTLOG("SD:\t%d", SD_CS);
  #ifdef FIRMWARE
    BOOTLOG("firmware:\t%s", FIRMWARE);
  #endif
  #ifdef UPDATEURL
    BOOTLOG("updateurl:\t%s", UPDATEURL);
  #endif
  BOOTLOG("------------------------------------------------");
}

// Preferences Look-up Table (store_variable, "key_max_15_char")
// Macro expands to 3 fields (offset_of_config_t_store_variable, "key_max_15_char", size_of_store_variable)
const configKeyMap Config::keyMap[] = {
  CONFIG_KEY_ENTRY(config_set, "cfgset"),
  CONFIG_KEY_ENTRY(lastStation, "laststa"),
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
  CONFIG_KEY_ENTRY(smartstart, "smartstartx"),
  CONFIG_KEY_ENTRY(autoupdate, "autoupdate"),
  CONFIG_KEY_ENTRY(audioinfo, "audioinfo"),
  CONFIG_KEY_ENTRY(vumeter, "vumeter"),
  CONFIG_KEY_ENTRY(wifiscanbest, "wifiscan"),
  CONFIG_KEY_ENTRY(ehdp, "ehdp"),
  CONFIG_KEY_ENTRY(ehdpname, "ehdpname"),
  CONFIG_KEY_ENTRY(softapdelay, "softapdelay"),
  CONFIG_KEY_ENTRY(mdnsname, "mdnsname"),
  CONFIG_KEY_ENTRY(flipscreen, "flipscr"),
  CONFIG_KEY_ENTRY(invertdisplay, "invdisp"),
  CONFIG_KEY_ENTRY(dspon, "dspon"),
  CONFIG_KEY_ENTRY(numplaylist, "numplaylist"),
  CONFIG_KEY_ENTRY(clock12, "clock12"),
  CONFIG_KEY_ENTRY(volumepage, "volpage"),
  CONFIG_KEY_ENTRY(brightness, "bright"),
  CONFIG_KEY_ENTRY(contrast, "contrast"),
  CONFIG_KEY_ENTRY(battery_adc_ref_mv, "battref"),
  CONFIG_KEY_ENTRY(screensaverEnabled, "scrnsvren"),
  CONFIG_KEY_ENTRY(screensaverTimeout, "scrnsvrto"),
  CONFIG_KEY_ENTRY(screensaverBlank, "scrnsvrbl"),
  CONFIG_KEY_ENTRY(screensaverPlayingEnabled, "scrnsvrplen"),
  CONFIG_KEY_ENTRY(screensaverPlayingTimeout, "scrnsvrplto"),
  CONFIG_KEY_ENTRY(screensaverPlayingBlank, "scrnsvrplbl"),
  CONFIG_KEY_ENTRY(dimmingEnabled, "dimmingen"),
  CONFIG_KEY_ENTRY(dimmingTimeout, "dimmingto"),
  CONFIG_KEY_ENTRY(dimmingBrightness, "dimmingbr"),
  CONFIG_KEY_ENTRY(volsteps, "vsteps"),
  CONFIG_KEY_ENTRY(fliptouch, "fliptouch"),
  CONFIG_KEY_ENTRY(dbgtouch, "dbgtouch"),
  CONFIG_KEY_ENTRY(encacc, "encacc"),
  CONFIG_KEY_ENTRY(skipPlaylistUpDown, "skipplupdn"),
  CONFIG_KEY_ENTRY(irtlp, "irtlp"),
  CONFIG_KEY_ENTRY(locale_webui, "localewebui"),
  CONFIG_KEY_ENTRY(tz_name, "tzname"),
  CONFIG_KEY_ENTRY(tzposix, "tzposix"),
  CONFIG_KEY_ENTRY(sntp1, "sntp1"),
  CONFIG_KEY_ENTRY(sntp2, "sntp2"),
  CONFIG_KEY_ENTRY(timesyncinterval, "timesync"),
  CONFIG_KEY_ENTRY(showweather, "showwthr"),
  CONFIG_KEY_ENTRY(weatherapi, "weatherapi"),
  CONFIG_KEY_ENTRY(weathersyncinterval, "weathersync"),
  CONFIG_KEY_ENTRY(weatherlat, "weatherlat"),
  CONFIG_KEY_ENTRY(weatherlon, "weatherlon"),
  CONFIG_KEY_ENTRY(weatherlang, "weatherlang"),
  CONFIG_KEY_ENTRY(weatherkey, "weatherkey"),
  CONFIG_KEY_ENTRY(weatherelevation, "weatherelev"),
  CONFIG_KEY_ENTRY(weathertempimp, "weathertempi"),
  CONFIG_KEY_ENTRY(weatherpressimp, "weatherpressi"),
  CONFIG_KEY_ENTRY(weatherwindspeed, "weatherwindsp"),
  CONFIG_KEY_ENTRY(weatherfeels, "weatherfeels"),
  CONFIG_KEY_ENTRY(weatherhumidity, "weatherhumid"),
  CONFIG_KEY_ENTRY(weatherpressure, "weatherpress"),
  CONFIG_KEY_ENTRY(weatherwind, "weatherwind"),
  CONFIG_KEY_ENTRY(mqttenable, "mqttenable"),
  CONFIG_KEY_ENTRY(mqtthost, "mqtthost"),
  CONFIG_KEY_ENTRY(mqttport, "mqttport"),
  CONFIG_KEY_ENTRY(mqttuser, "mqttuser"),
  CONFIG_KEY_ENTRY(mqttpass, "mqttpass"),
  CONFIG_KEY_ENTRY(mqtttopic, "mqtttopic"),
  {0, nullptr, 0} // Yup, 3 fields - don't delete the last line!
};

void Config::deleteOldKeys() {
  // List any old/legacy keys to remove here (they will be deleted from prefs if found)
  prefs.remove("smartstart"); // previous smartstart was numeric 0, 1, 2
  // prefs.remove("removedkey"); // note
}
