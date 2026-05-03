#include "options.h"
#include <cstddef>
#include <ESPFileUpdater.h>
#include <nvs_flash.h>
#ifndef ARDUINO_ESP32C3_DEV
  #include <driver/rtc_io.h>
#endif
#include "config.h"
#include "controls.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "rtcsupport.h"
#include "telnet.h"
#include "../displays/tools/utf8_common.h"
#ifdef USE_SD
  #include "sdmanager.h"
#endif
#ifdef USE_NEXTION
  #include "../displays/nextion.h"
#endif


// List of required web asset files
static const char* wwwFiles[] = {"curated.js", "dragpl.js", "locale.js", "options.js", "script.js", "script2.js", "search.js",
                                 "logo.svg", "icon.png", "locales.json", "rb_srvrs.json", "timezones.json", "style.css", "theme.css",
                                 "curated.html", "irrecord.html", "options.html", "search.html", "updform.html",
                                 "player.html"}; // keep main page at end (deleted when upgraded, last to be downloaded, so user sees emptyfs_html with wait message)
static const size_t wwwFilesCount = sizeof(wwwFiles) / sizeof(wwwFiles[0]);

// List of optional data files
static const char* dataFiles[] = {"playlist.csv", "wifi.csv"};
static const size_t dataFilesCount = sizeof(dataFiles) / sizeof(dataFiles[0]);

Config config;

bool wasUpdated(ESPFileUpdater::UpdateStatus status) { return status == ESPFileUpdater::UPDATED; }

void u8fix(char *src) {
  if (strlen(src) == 0) return;
  char last = src[strlen(src)-1]; 
  if ((uint8_t)last >= 0xC2) src[strlen(src)-1]='\0';
}

bool Config::_wwwFilesExist() {
  char fullpath[64];
  for (size_t i = 0; i < wwwFilesCount; i++) {
    sprintf(fullpath, "/www/%s", wwwFiles[i]);
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
  #if defined(SD_SPIPINS) || SD_HSPI
    #if !defined(SD_SPIPINS)
      SDSPI.begin();
    #else
      SDSPI.begin(SD_SPIPINS); // SCK, MISO, MOSI
    #endif
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
  
  // Check version file and determine if files need updating
  const char* versionPath = "/data/ehradio.ver";
  String storedVersion = "";
  if (SPIFFS.exists(versionPath)) {
    File verFile = SPIFFS.open(versionPath, "r");
    if (verFile) {
      storedVersion = verFile.readStringUntil('\n');
      storedVersion.trim();
      verFile.close();
    }
  }
  // if version is correct or file doesn't exist (which may indiciate SPIFFS files installed via flash), check files
  if (storedVersion == String(RADIOVERSION)) {
    wwwFilesExist = _wwwFilesExist();
  } else if (!SPIFFS.exists(versionPath)) {
    BOOTLOG("New install detected.");
    wwwFilesExist = _wwwFilesExist();
  } else {
    BOOTLOG("Version mismatch detected (stored: %s, current: %s)", storedVersion.c_str(), RADIOVERSION);
    wwwFilesExist = false;
  }
  // if version is incorrect or version file doesn't exist, need to clean SPIFFS and make the version file
  if (!wwwFilesExist || !SPIFFS.exists(versionPath)) {
    purgeUnwantedFiles();
    File verFile = SPIFFS.open(versionPath, "w");
    if (verFile) {
        verFile.println(RADIOVERSION);
        verFile.close();
        BOOTLOG("Version file updated to %s", RADIOVERSION);
    }
  }
  if (!wwwFilesExist) {
    deleteMainwwwFile(); // Forces update process or (kind of) makes SPIFFS unusable
    #ifndef UPDATEURL
      BOOTLOG("SPIFFS is missing files!");
    #else
      BOOTLOG("SPIFFS is missing files.  Will attempt to get files from online...");
    #endif
  }
  // Note: Locale file mismatch will be handled async in startupServices() after WiFi connects

  #ifdef USE_SD
    _SDplaylistFS = getMode()==PM_SDCARD?&sdman:(true?&SPIFFS:_SDplaylistFS);
  #else
    _SDplaylistFS = &SPIFFS;
  #endif
}

void Config::loadPreferences() {
  prefs.begin("ehradio", false);
  // Check config_set first
  uint16_t configSetValue = 0;
  size_t configSetRead = prefs.getBytes("cfgset", &configSetValue, sizeof(configSetValue));
  if (configSetRead != sizeof(configSetValue)) {
    // Preferences is empty, save config_set and version
    FUNCTIONLOG("Prefs", "Empty NVS detected, initializing config_set...");
    saveValue(&store.config_set, store.config_set);
  } else if (configSetValue != 4262) {
    // config_set present but not valid, reset config
    FUNCTIONLOG("Prefs", "Invalid config_set (%u), resetting config...", configSetValue);
    prefs.end();
    reset();
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

void Config::changeMode(int newmode) {
  #ifdef USE_SD
    bool pir = player.isRunning();
    if (SDC_CS==255) return;
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
      while(display.mode()!=SDCHANGE)
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

bool Config::spiffsCleanup() {
  bool ret = (SPIFFS.exists(PLAYLIST_SD_PATH)) || (SPIFFS.exists(INDEX_SD_PATH)) || (SPIFFS.exists(INDEX_PATH));
  if (SPIFFS.exists(PLAYLIST_SD_PATH)) SPIFFS.remove(PLAYLIST_SD_PATH);
  if (SPIFFS.exists(INDEX_SD_PATH)) SPIFFS.remove(INDEX_SD_PATH);
  if (SPIFFS.exists(INDEX_PATH)) SPIFFS.remove(INDEX_PATH);
  return ret;
}

char * Config::ipToStr(IPAddress ip) {
  snprintf(ipBuf, 16, "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  return ipBuf;
}

void Config::initPlaylistMode() {
  uint16_t _lastStation = 0;
  uint16_t cs = playlistLength();
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
  if (getMode()==PM_WEB && _wwwFilesExist()) initPlaylist();
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
  loadStation(_lastStation);
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

void Config::reset() {
  FUNCTIONLOG("Prefs", "Reset requested, resetting config...");
  //prefs.begin("ehradio", false);
  //prefs.clear();
  //prefs.end();
  setDefaults();
  delay(500);
  ESP.restart();
}

void Config::defaultSettings(const char *val, uint8_t clientId) {
  if (strcmp(val, "system") == 0) {
    saveValue(&store.smartstart, (bool)SMART_START);
    saveValue(&store.audioinfo, (bool)SHOW_AUDIO_INFO);
    saveValue(&store.vumeter, (bool)SHOW_VU_METER);
    saveValue(&store.wifiscanbest, (bool)WIFI_SCAN_BEST_RSSI);
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
    network.trueWeather=false;
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
  if (strcmp(val, "1") == 0) {
    config.reset();
    return;
  }
}



void Config::setDefaults() {
  SERIALLOG("setDefaults called");
  nvs_flash_erase();
  nvs_flash_init();
  // defaults set by struct, except one
  snprintf(store.mdnsname, MDNS_LENGTH, "ehradio-%x", getChipId());
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

// Helper: remove trailing CR/LF and trim leading/trailing whitespace from a C string
static void strip_whitespace(char *s) {
  if (!s) return;
  // Trim trailing CR/LF and any trailing whitespace
  size_t len = strlen(s);
  while (len > 0 && (s[len-1] == '\r' || s[len-1] == '\n' || isspace((unsigned char)s[len-1]))) {
    s[--len] = '\0';
  }
  // Trim leading whitespace
  char *start = s;
  while (*start && isspace((unsigned char)*start)) start++;
  if (start != s) memmove(s, start, strlen(start) + 1);
}

void Config::setTitle(const char* title) {
  vuThreshold = 0;
  // Keep native UTF-8 title (single source of truth for WebUI/CLI)
  memset(config.station.title, 0, BUFLEN);
  strlcpy(config.station.title, title, BUFLEN);
  u8fix(config.station.title);
  strip_whitespace(config.station.title);
  netserver.requestOnChange(TITLE, 0);
  netserver.loop();
  display.putRequest(NEWTITLE);
}

void Config::setStation(const char* station) {
  memset(config.station.name, 0, BUFLEN);
  strlcpy(config.station.name, station, BUFLEN);
  u8fix(config.station.name);
  strip_whitespace(config.station.name);
}  

void Config::indexPlaylist() {
  File playlist = SPIFFS.open(PLAYLIST_PATH, "r");
  if (!playlist) {
    return;
  }
  char sName[BUFLEN], sUrl[BUFLEN];
  int sOvol;
  File index = SPIFFS.open(INDEX_PATH, "w");
  while (playlist.available()) {
    uint32_t pos = playlist.position();
    if (parseCSV(playlist.readStringUntil('\n').c_str(), sName, sUrl, sOvol)) {
      index.write((uint8_t *) &pos, 4);
    }
  }
  index.close();
  playlist.close();
}

void Config::initPlaylist() {
  //store.countStation = 0;
  if (!SPIFFS.exists(INDEX_PATH)) indexPlaylist();

  /*if (SPIFFS.exists(INDEX_PATH)) {
    File index = SPIFFS.open(INDEX_PATH, "r");
    store.countStation = index.size() / 4;
    index.close();
    saveValue(&store.countStation, store.countStation);
  }*/
}
uint16_t Config::playlistLength() {
  uint16_t out = 0;
  if (SDPLFS()->exists(REAL_INDEX)) {
    File index = SDPLFS()->open(REAL_INDEX, "r");
    out = index.size() / 4;
    index.close();
  }
  return out;
}
bool Config::loadStation(uint16_t ls) {
  uint16_t cs = playlistLength();
  if (cs == 0) {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, "ehRadio", BUFLEN);
    station.ovol = 0;
    return false;
  }
  if (ls > cs) ls = 1;
  char sName[BUFLEN], sUrl[BUFLEN];
  int sOvol;
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  if (_readStationEntry(playlist, index, ls, sName, sUrl, sOvol)) {
    memset(station.url, 0, BUFLEN);
    memset(station.name, 0, BUFLEN);
    strncpy(station.name, sName, BUFLEN);
    strncpy(station.url, sUrl, BUFLEN);
    station.ovol = sOvol;
    setLastStation(ls);
  }
  playlist.close();
  index.close();
  return true;
}

bool Config::_readStationEntry(File& playlist, File& index, uint16_t idx, char* name, char* url, int& ovol) {
  index.seek((idx - 1) * 4, SeekSet);
  uint32_t pos;
  index.readBytes((char*)&pos, 4);
  playlist.seek(pos, SeekSet);
  return parseCSV(playlist.readStringUntil('\n').c_str(), name, url, ovol);
}

uint16_t Config::findStationByUrl(const char* url) {
  uint16_t cs = playlistLength();
  if (cs == 0 || url == nullptr || url[0] == '\0') return 0;
  char sName[BUFLEN], sUrl[BUFLEN];
  int sOvol;
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  if (!playlist || !index) { if (playlist) playlist.close(); if (index) index.close(); return 0; }
  for (uint16_t i = 1; i <= cs; i++) {
    if (_readStationEntry(playlist, index, i, sName, sUrl, sOvol) && strcmp(sUrl, url) == 0) {
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

char * Config::stationByNum(uint16_t num) {
  File playlist = SDPLFS()->open(REAL_PLAYL, "r");
  File index = SDPLFS()->open(REAL_INDEX, "r");
  index.seek((num - 1) * 4, SeekSet);
  uint32_t pos;
  memset(_stationBuf, 0, BUFLEN/2);
  index.readBytes((char *) &pos, 4);
  index.close();
  playlist.seek(pos, SeekSet);
  strncpy(_stationBuf, playlist.readStringUntil('\t').c_str(), BUFLEN/2);
  playlist.close();
  return _stationBuf;
}

void Config::escapeQuotes(const char* input, char* output, size_t maxLen) {
  size_t j = 0;
  for (size_t i = 0; input[i] != '\0' && j < maxLen - 1; ++i) {
    if (input[i] == '"') {
      if (j >= maxLen - 2) break; // not enough room for \ and " — truncate cleanly
      output[j++] = '\\';
      output[j++] = '"';
    } else {
      output[j++] = input[i];
    }
  }
  output[j] = '\0';
}

bool Config::parseCSV(const char* line, char* name, char* url, int &ovol) {
  char *tmpe;
  const char* cursor = line;
  char buf[5];
  tmpe = strstr(cursor, "\t");
  if (tmpe == NULL) return false;
  strlcpy(name, cursor, tmpe - cursor + 1);
  if (strlen(name) == 0) return false;
  cursor = tmpe + 1;
  tmpe = strstr(cursor, "\t");
  if (tmpe == NULL) return false;
  strlcpy(url, cursor, tmpe - cursor + 1);
  if (strlen(url) == 0) return false;
  cursor = tmpe + 1;
  if (strlen(cursor) == 0) return false;
  strlcpy(buf, cursor, 4);
  ovol = atoi(buf);
  return true;
}

bool Config::parseWsCommand(const char* line, char* cmd, char* val, uint8_t cSize) {
  char *tmpe;
  tmpe = strstr(line, "=");
  if (tmpe == NULL) return false;
  memset(cmd, 0, cSize);
  strlcpy(cmd, line, tmpe - line + 1);
  //if (strlen(tmpe + 1) == 0) return false;
  memset(val, 0, cSize);
  strlcpy(val, tmpe + 1, strlen(line) - strlen(cmd) + 1);
  return true;
}

bool Config::parseSsid(const char* line, char* ssid, char* pass) {
  char *tmpe;
  tmpe = strstr(line, "\t");
  if (tmpe == NULL) return false;
  uint16_t pos = tmpe - line;
  if (pos >= sizeof(ssids[0].ssid)) return false;
  if (strlen(line + pos + 1) >= sizeof(ssids[0].password)) return false;
  memset(ssid, 0, sizeof(ssids[0].ssid));
  strlcpy(ssid, line, pos + 1);
  memset(pass, 0, sizeof(ssids[0].password));
  strlcpy(pass, line + pos + 1, sizeof(ssids[0].password));
  return true;
}

bool Config::saveWifi(const char* post) {
  char ssidval[sizeof(ssids[0].ssid)], passval[sizeof(ssids[0].password)];
  if (parseSsid(post, ssidval, passval)) {
    if (addSsid(ssidval, passval)) {
      ESP.restart();
      return true;
    }
  }
  return false;
}

bool Config::addSsid(const char* ssid, const char* password) {
  int slot = -1;
  for (int i = 0; i < ssidsCount; i++) {
    if (strcmp(ssids[i].ssid, ssid) == 0) {
      slot = i;
      break;
    }
  }
  
  if (slot == -1) {
    slot = (ssidsCount < 5) ? ssidsCount : 4;
    if (slot == ssidsCount && ssidsCount < 5) {
      ssidsCount++;
    }
  }
  
  strlcpy(ssids[slot].ssid, ssid, sizeof(ssids[0].ssid));
  strlcpy(ssids[slot].password, password, sizeof(ssids[0].password));
  setLastSSID(slot + 1);

  File file = SPIFFS.open(TMP_PATH, "w");
  if (!file) return false;
  for (int i = 0; i < ssidsCount; i++) {
    if (strlen(ssids[i].ssid) > 0) {
      file.printf("%s\t%s\n", ssids[i].ssid, ssids[i].password);
    }
  }
  file.close();
  
  if (SPIFFS.exists(TMP_PATH)) {
    SPIFFS.remove(SSIDS_PATH);
    return SPIFFS.rename(TMP_PATH, SSIDS_PATH);
  }
  return false;
}

bool Config::importWifi() {
  if (!SPIFFS.exists(TMP_PATH)) return false;
  SPIFFS.remove(SSIDS_PATH);
  SPIFFS.rename(TMP_PATH, SSIDS_PATH);
  ESP.restart();
  return true;
}

bool Config::initNetwork() {
  File file = SPIFFS.open(SSIDS_PATH, "r");
  if (!file || file.isDirectory()) {
    return false;
  }
  ssidsCount = 0;
  char ssidval[sizeof(ssids[0].ssid)], passval[sizeof(ssids[0].password)];
  while (file.available() && ssidsCount < 5) {
    if (parseSsid(file.readStringUntil('\n').c_str(), ssidval, passval)) {
      strlcpy(ssids[ssidsCount].ssid, ssidval, sizeof(ssids[0].ssid));
      strlcpy(ssids[ssidsCount].password, passval, sizeof(ssids[0].password));
      ssidsCount++;
    }
  }
  file.close();
  return true;
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

static void sleepCore() {
  if (BRIGHTNESS_PIN!=255) analogWrite(BRIGHTNESS_PIN, 0);
  display.deepsleep();
  #ifdef USE_NEXTION
    nextion.sleep();
  #endif
  #if defined(ARDUINO_ESP32C3_DEV)
    if (WAKE_PIN!=255) esp_deep_sleep_enable_gpio_wakeup((1ULL << WAKE_PIN), WAKE_PIN_STATE ? ESP_GPIO_WAKEUP_GPIO_HIGH : ESP_GPIO_WAKEUP_GPIO_LOW);
  #else
    if (WAKE_PIN!=255) {
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

void Config::doSleep() {
  sleepCore();
  esp_sleep_enable_timer_wakeup(config.sleepfor * 60 * 1000000ULL);
  esp_deep_sleep_start();
}

void Config::doSleepW() {
  sleepCore();
  esp_deep_sleep_start();
}

void Config::sleepForAfter(uint16_t sf, uint16_t sa) {
  sleepfor = sf;
  if (sa > 0) _sleepTimer.attach(sa * 60, doSleep);
  else doSleep();
}

void Config::purgeUnwantedFiles() {
  BOOTLOG("Scanning SPIFFS for unwanted files...");
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
      // Keep the current locale file (which depends...)
      char currentLocaleGz[64], currentLocale[64];
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
        for (size_t i = 0; i < wwwFilesCount; i++) {
          if (name == String(wwwFiles[i]) || name == String(wwwFiles[i]) + ".gz") {
            keep = true;
            break;
          }
        }
      }
      // If we're keeping this file but it's uncompressed, check if .gz exists
      if (keep && !name.endsWith(".gz")) {
        String gzPath = "/www/" + name + ".gz";
        if (SPIFFS.exists(gzPath)) {
          SPIFFS.remove(path);
          BOOTLOG("Removed duplicate (compressed version exists): %s", path.c_str());
          file = root.openNextFile();
          continue;
        }
      }
    } else if (path.startsWith("/data/")) {
      String name = path.substring(6);
      for (size_t i = 0; i < dataFilesCount; i++) {
        if (name == String(dataFiles[i])) {
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
  BOOTLOG("Purge complete.");
}

void Config::deleteMainwwwFile() {
  if (wwwFilesCount > 0) {
    const char* lastFile = wwwFiles[wwwFilesCount - 1];
    char mainfile[64];
    // Try both uncompressed and compressed versions
    for (const char* suffix : {"", ".gz"}) {
      snprintf(mainfile, sizeof(mainfile), "/www/%s%s", lastFile, suffix);
      if (SPIFFS.exists(mainfile)) {
        SPIFFS.remove(mainfile);
        FUNCTIONLOG("Config", "Deleted main www file: %s", mainfile);
      }
    }
  }
}

void cleanStaleSearchResults() {
  const char* metaPath = "/www/searchresults.json.meta";
  if (SPIFFS.exists(metaPath)) {
    File metaFile = SPIFFS.open(metaPath, "r");
    metaFile.readStringUntil('\n'); // 1st line query
    String timeStr = metaFile.readStringUntil('\n'); //2nd line is time
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

void fixPlaylistFileEnding() {
  const char* playlistPath = PLAYLIST_PATH;
  if (!SPIFFS.exists(playlistPath)) return;
  File playlistfile = SPIFFS.open(playlistPath, "r+");
  if (!playlistfile) return;
  size_t sz = playlistfile.size();
  if (sz < 2) { playlistfile.close(); return; }
  playlistfile.seek(sz - 2, SeekSet);
  char last2[3] = {0};
  playlistfile.read((uint8_t*)last2, 2);
  if (!(last2[0] == '\r' && last2[1] == '\n')) {
    playlistfile.seek(sz, SeekSet);
    playlistfile.write((const uint8_t*)"\r\n", 2);
  }
  playlistfile.close();
}

void getRequiredFiles() {
  #ifdef UPDATEURL
    player.sendCommand({PR_STOP, 0});
    ESPFileUpdater* getRequiredFile = new ESPFileUpdater(SPIFFS);
    getRequiredFile->setMaxSize(1024);
    getRequiredFile->setUserAgent(ESPFILEUPDATER_USERAGENT);
    char localFileGz[64];
    char localFile[64];
    char tryFile[64];
    char tryUrl[128];
    display.putRequest(NEWMODE, UPDATING);
    for (size_t i = 0; i < wwwFilesCount; i++) {
      display.updateProgress(LANG::updFiles, (float)(i + 1) / (float)wwwFilesCount);
      const char* fname = wwwFiles[i];
      snprintf(localFileGz, sizeof(localFileGz), "/www/%s.gz", fname);
      snprintf(localFile, sizeof(localFile), "/www/%s", fname);
      if (SPIFFS.exists(localFileGz)) SPIFFS.remove(localFileGz);
      if (SPIFFS.exists(localFile)) SPIFFS.remove(localFile);
      bool success = false;
      for (size_t j = 0; j < 2; j++) {
        if (j == 0) { // Try compressed first
          snprintf(tryFile, sizeof(tryFile), "%s", localFileGz);
          snprintf(tryUrl, sizeof(tryUrl), "%s%s.gz", FILESURL, fname);
        } else { // Fallback to uncompressed
          snprintf(tryFile, sizeof(tryFile), "%s", localFile);
          snprintf(tryUrl, sizeof(tryUrl), "%s%s", FILESURL, fname);
        }
        FUNCTIONLOG("ESPFileUpdater", "%s - updating required file...", tryFile);
        ESPFileUpdater::UpdateStatus result = getRequiredFile->checkAndUpdate(
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
        // give up and show update‑failed message briefly before returning to lost dialog
        display.updateProgress(LANG::updFailed, 0.0f);
        delay(3000);
        display.putRequest(NEWMODE, LOST);
        return;
      }
    }
    delete getRequiredFile;
    config.purgeUnwantedFiles();
    delay(200);
    ESP.restart();
  #endif //#ifdef UPDATEURL
}

void checkNewVersionFile() {
  #ifdef UPDATEURL
    const char* newVersionPath = "/data/new_ver.txt";
    netserver.newVersion = String(RADIOVERSION);
    if (SPIFFS.exists(newVersionPath)) {
      File newVerFile = SPIFFS.open(newVersionPath, "r");
      if (newVerFile) {
        String line = newVerFile.readStringUntil('\n');
        line.trim();
        if (line.indexOf(VERSIONSTRING) >= 0) {
          int firstQuote = line.indexOf('"');
          int lastQuote = line.lastIndexOf('"');
          if (firstQuote >= 0 && lastQuote > firstQuote) {
            String extractedVersion = line.substring(firstQuote + 1, lastQuote);
            if (extractedVersion.length() > 0) {
              netserver.newVersion = extractedVersion;
            }
          }
        }
        newVerFile.close();
      }
    }
    if (netserver.newVersion != String(RADIOVERSION)) {
      netserver.newVersionAvailable = true;
    } else {
      netserver.newVersionAvailable =  false;
    }
  #endif //#ifdef UPDATEURL
}

void Config::updateFile(void* param, const char* localFile, const char* onlineFile, const char* updatePeriod, const char* simpleName) {
  FUNCTIONLOG("ESPFileUpdater", "%s - started update", simpleName);
  ESPFileUpdater* updatefile = (ESPFileUpdater*)param;
  ESPFileUpdater::UpdateStatus result = updatefile->checkAndUpdate(
      localFile,
      onlineFile,
      updatePeriod,
      ESPFILEUPDATER_VERBOSE
 );
  if (result == ESPFileUpdater::UPDATED) {
    FUNCTIONLOG("ESPFileUpdater", "%s - update completed", simpleName);
  } else if (result == ESPFileUpdater::NOT_MODIFIED||result == ESPFileUpdater::MAX_AGE_NOT_REACHED) {
    FUNCTIONLOG("ESPFileUpdater", "%s - no update needed", simpleName);
  } else {
    FUNCTIONLOG("ESPFileUpdater", "%s - update failed", simpleName);
  }
}

// Struct to pass parameters to async locale update task
struct LocaleUpdateParams {
  ESPFileUpdater* updater;
  uint8_t clientId;
  char localeCode[16];
};

bool updateLocaleFileCore(ESPFileUpdater* updater, const char* localeCode) {
  // called by updateLocaleFileAsync or updateLocaleFile
  // Returns true on success, false on failure
  #ifdef UPDATEURL
    // Special case: hardcoded locale uses default, no file needed
    if (strcmp(localeCode, HARDCODED_WEBUI_LOCALE) == 0) {
      FUNCTIONLOG("Locale", "Updating locale: %s - no need to download, hardcoded locale uses default.", HARDCODED_WEBUI_LOCALE);
      return true;
    }
    char tryFile[64] = "/www/locale.new";
    char finalFile[64];
    char tryUrl[128];
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
  #endif //#ifdef UPDATEURL
}

void updateLocaleFileAsyncWrapper(void* param) {
  LocaleUpdateParams* params = (LocaleUpdateParams*)param;
  // Attempt to download and install the locale file
  bool success = updateLocaleFileCore(params->updater, params->localeCode);
  if (success) {
    // Remove old locale files before updating config
    char oldLocaleGz[64], oldLocale[64];
    snprintf(oldLocaleGz, sizeof(oldLocaleGz), "/www/%s.json.gz", config.store.locale_webui);
    snprintf(oldLocale, sizeof(oldLocale), "/www/%s.json", config.store.locale_webui);
    SPIFFS.remove(oldLocaleGz);
    SPIFFS.remove(oldLocale);
    // Download successful - commit the locale code to config
    config.saveValue(config.store.locale_webui, params->localeCode);
    FUNCTIONLOG("Locale Update", "Successfully updated to %s", params->localeCode);
    // Send success message to frontend
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"locale_updated\":true,\"locale\":\"%s\"}", params->localeCode);
    websocket.text(params->clientId, msg);
  } else {
    // Download failed - don't modify config, send error message
    FUNCTIONLOG("Locale Update", "Failed to update to %s", params->localeCode);
    websocket.text(params->clientId, "{\"locale_update_failed\":true}");
  }
  delete params->updater;
  delete params;
  vTaskDelete(NULL);
}

void Config::updateLocaleFile() {
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

bool Config::updateLocaleFileAsync(const char* localeCode, uint8_t clientId) {
  if (WiFi.status() != WL_CONNECTED) return false;
  #ifdef UPDATEURL
    // If switching WebUI Locales, need to download a file
    LocaleUpdateParams* params = new LocaleUpdateParams();
    params->updater = new ESPFileUpdater(SPIFFS);
    params->updater->setMaxSize(1024);
    params->updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    params->clientId = clientId;
    strlcpy(params->localeCode, localeCode, sizeof(params->localeCode));
    xTaskCreate(updateLocaleFileAsyncWrapper, "updateLocaleFileAsyncWrapper", 8192, params, 2, NULL);
    return true; // Task created successfully (NOT download result)
  #else
    // If not, then just need to switch
    config.saveValue(config.store.locale_webui, localeCode);
    FUNCTIONLOG("Locale Switch", "Changed to %s", localeCode);
    char msg[64];
    snprintf(msg, sizeof(msg), "{\"locale_updated\":true,\"locale\":\"%s\"}", localeCode);
    websocket.text(clientId, msg);
    return true;
  #endif
}

bool checkLocaleFile() {
  // Special case: hardcoded locale uses default, no file needed
  if (strcmp(config.store.locale_webui, HARDCODED_WEBUI_LOCALE) == 0) {
    FUNCTIONLOG("Locale Check", "%s uses hardcoded default, no file needed", HARDCODED_WEBUI_LOCALE);
    return true;
  }
  char localeFileGz[64], localeFile[64];
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

void startupServicesAsync(void* param) {
  fixPlaylistFileEnding(); // playlist.csv MUST have a line-feed at end (can happen easily by uploading a file)
  #ifdef UPDATEURL
    if (!checkLocaleFile()) {
      FUNCTIONLOG("Locale Check", "Locale file verification failed, updating to %s...", config.store.locale_webui);
      config.updateLocaleFile();
    }
    config.updateFile(param, "/data/new_ver.txt", CHECKUPDATEURL, CHECKUPDATEURL_TIME, "New version number");
    checkNewVersionFile();
    if (config.store.autoupdate && netserver.newVersionAvailable) {
      FUNCTIONLOG("AutoUpdate", "New version detected - starting online update");
      startOnlineUpdate();
    }
  #endif
  #ifdef PLAYLIST_DEFAULT_URL
    if (!SPIFFS.exists("/data/playlist.csv")) {
      config.updateFile(param, "/data/playlist.csv", PLAYLIST_DEFAULT_URL, "", "Default playlist");
      if (SPIFFS.exists("/data/playlist.csv")) netserver.requestOnChange(PLAYLISTSAVED, 0);
    }
  #endif
  config.updateFile(param, "/www/timezones.json.gz", TIMEZONES_JSON_URL, TIMEZONES_JSON_CHECKTIME, "Timezones database file");
  config.updateFile(param, "/www/rb_srvrs.json", RADIO_BROWSER_SERVERS_URL, RB_SERVERS_CHECKTIME, "Radio Browser servers list");
  cleanStaleSearchResults();
  vTaskDelete(NULL);
}

void Config::startupServices() {
  if (WiFi.status() != WL_CONNECTED) return;
  #ifdef UPDATEURL
    ESPFileUpdater* updater = nullptr;
    updater = new ESPFileUpdater(SPIFFS);
    updater->setMaxSize(1024);
    updater->setUserAgent(ESPFILEUPDATER_USERAGENT);
    if (!wwwFilesExist) {
      getRequiredFiles();
    } else {
      xTaskCreate(startupServicesAsync, "startupServicesAsync", 8192, updater, 2, NULL);
    }
  #endif
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
    BOOTLOG("audio:\t\t%s (%d, %d, %d, %d, %s)", "VS1053", VS1053_CS, VS1053_DCS, VS1053_DREQ, VS1053_RST, VS_HSPI?"true":"false");
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
          BTN_LEFT, BTN_CENTER, BTN_RIGHT, BTN_UP, BTN_DOWN, BTN_MODE, BTN_INTERNALPULLUP?"true":"false");
  BOOTLOG("encoders:\tl1=%d, b1=%d, r1=%d, pullup=%s, l2=%d, b2=%d, r2=%d, pullup=%s", 
          ENC_BTNL, ENC_BTNB, ENC_BTNR, ENC_INTERNALPULLUP?"true":"false", ENC2_BTNL, ENC2_BTNB, ENC2_BTNR, ENC2_INTERNALPULLUP?"true":"false");
  BOOTLOG("ir:\t\t%d", IR_PIN);
  if (SDC_CS!=255) BOOTLOG("SD:\t%d", SDC_CS);
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
