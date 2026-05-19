#ifndef config_h
#define config_h
#pragma once
#include <Arduino.h>
#include <Ticker.h>
#include <SPI.h>
#if defined(SPI_BUS_SECONDARY)
  extern SPIClass SPIB;
#endif
#include <SPIFFS.h>
#include <Preferences.h>
#include "locale.h"
#include "logging.h"
#include "../displays/widgets/widgetsconfig.h"

#define PLAYLIST_FILE        "playlist.csv"
#define SSIDS_FILE           "wifi.csv"
#define VERSION_FILE         "ehradio.ver"
#define LASTSTATION_URL_FILE "laststation.url"
#define TMP_FILE             "tmpfile.txt"
#define INDEX_FILE           "index.dat"
#define PLAYLIST_SD_FILE     "playlistsd.csv"
#define INDEX_SD_FILE        "indexsd.dat"

#define PLAYLIST_PATH        "/data/" PLAYLIST_FILE
#define SSIDS_PATH           "/data/" SSIDS_FILE
#define VERSION_PATH         "/data/" VERSION_FILE
#define LASTSTATION_URL_PATH "/data/" LASTSTATION_URL_FILE
#define TMP_PATH             "/data/" TMP_FILE
#define INDEX_PATH           "/data/" INDEX_FILE
#define PLAYLIST_SD_PATH     "/data/" PLAYLIST_SD_FILE
#define INDEX_SD_PATH        "/data/" INDEX_SD_FILE

#define REAL_PLAYL   config.getMode()==PM_WEB?PLAYLIST_PATH:PLAYLIST_SD_PATH
#define REAL_INDEX   config.getMode()==PM_WEB?INDEX_PATH:INDEX_SD_PATH

#define WEATHERKEY_LENGTH 64
#define MDNS_LENGTH 32
#define EHDPNAME_LENGTH 32

#define ESPFILEUPDATER_USERAGENT "ehradio/" RADIOVERSION "(" GITHUBURL ")"  // used as a user-agent string for downloading with ESPFileUpdater

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
  #define ESP_ARDUINO_3 1
#endif

#define MAX_PLAY_MODE 1
enum playMode_e      : uint8_t  { PM_WEB=0, PM_SDCARD=1 };

void u8fix(char *src);

struct theme_t {
  uint16_t background;
  uint16_t meta;
  uint16_t metabg;
  uint16_t metafill;
  uint16_t title1;
  uint16_t title2;
  uint16_t digit;
  uint16_t div;
  uint16_t weather;
  uint16_t vumax;
  uint16_t vumin;
  uint16_t clock;
  uint16_t clockbg;
  uint16_t seconds;
  uint16_t dow;
  uint16_t date;
  uint16_t heap;
  uint16_t buffer;
  uint16_t ip;
  uint16_t vol;
  uint16_t rssi;
  uint16_t battery;
  uint16_t bitrate;
  uint16_t volbarout;
  uint16_t volbarin;
  uint16_t plcurrent;
  uint16_t plcurrentbg;
  uint16_t plcurrentfill;
  uint16_t playlist[5];
};
struct config_t // specify defaults here (and macros in options.h) (defaults are NOT saved to Prefs)
{
  uint16_t  config_set = 4262;
  uint16_t  lastStation = 0;
  uint16_t  countStation = 0;
  uint8_t   lastSSID = 0;
  uint16_t  lastSdStation = 0;
  uint8_t   play_mode = 0;
  uint8_t   volume = SOUND_VOLUME;
  int8_t    balance = SOUND_BALANCE;
  int8_t    treble = EQ_TREBLE;
  int8_t    middle = EQ_MIDDLE;
  int8_t    bass = EQ_BASS;
  bool      sdshuffle = SD_SHUFFLE;
  bool      smartstart = SMART_START;
  bool      autoupdate = false;
  bool      audioinfo = SHOW_AUDIO_INFO;
  bool      vumeter = SHOW_VU_METER;
  bool      wifiscanbest = WIFI_SCAN_BEST_RSSI;
  bool      ehdp = EHDP;
  char      ehdpname[EHDPNAME_LENGTH] = "";
  uint8_t   softapdelay = SOFTAP_REBOOT_DELAY;
  char      mdnsname[MDNS_LENGTH] = "";
  bool      flipscreen = SCREEN_FLIP;
  bool      invertdisplay = SCREEN_INVERT;
  bool      dspon = true;
  bool      numplaylist = NUMBERED_PLAYLIST;
  bool      clock12 = CLOCK_TWELVE;
  bool      volumepage = VOLUME_PAGE;
  uint8_t   brightness = SCREEN_BRIGHTNESS;
  uint8_t   contrast = SCREEN_CONTRAST;
  bool      screensaverEnabled = SS_NOTPLAYING;
  bool      screensaverBlank = SS_NOTPLAYING_BLANK;
  uint16_t  screensaverTimeout = SS_NOTPLAYING_TIME;
  bool      screensaverPlayingEnabled = SS_PLAYING;
  bool      screensaverPlayingBlank = SS_PLAYING_BLANK;
  uint16_t  screensaverPlayingTimeout = SS_PLAYING_TIME;
  bool      dimmingEnabled = DIMMING_ENABLED;
  uint16_t  dimmingTimeout = DIMMING_TIMEOUT;
  uint8_t   dimmingBrightness = DIMMING_BRIGHTNESS;
  uint8_t   volsteps = VOLUME_STEPS;
  bool      fliptouch = TOUCH_FLIP;
  bool      dbgtouch = TOUCH_DEBUG;
  uint16_t  encacc = ROTARY_ACCEL;
  uint16_t  battery_adc_ref_mv = BATTERY_ADC_REF_MV;
  bool      skipPlaylistUpDown = ONE_CLICK_SWITCH;
  uint8_t   irtlp = IR_TOLERANCE;
  char      locale_webui[10] = WEBUI_LOCALE;
  char      tz_name[70] = TIMEZONE_NAME;
  char      tzposix[70] = TIMEZONE_POSIX;
  char      sntp1[35] = SNTP_1;
  char      sntp2[35] = SNTP_2;
  uint8_t   timesyncinterval = TIME_SYNC_INTERVAL;
  bool      showweather = false;
  uint8_t   weathersyncinterval = WEATHER_SYNC_INTERVAL;
  char      weatherapi[6] = WEATHER_API;
  char      weatherlang[10] = WEATHER_LANG;
  char      weatherlat[10] = WEATHER_LAT;
  char      weatherlon[10] = WEATHER_LON;
  char      weatherkey[WEATHERKEY_LENGTH] = "";
  int16_t   weatherelevation = 0;
  bool      weathertempimp = WEATHER_TEMPERATURE_F;
  bool      weatherpressimp = WEATHER_PRESSURE_MMHG;
  char      weatherwindspeed[6] = WEATHER_WIND_SPEED_UNITS;
  bool      weatherfeels = false;
  bool      weatherhumidity = false;
  bool      weatherpressure = false;
  bool      weatherwind = false;
  bool      mqttenable = false;
  char      mqtthost[60] = MQTT_HOST;
  uint16_t  mqttport = MQTT_PORT;
  char      mqttuser[30] = MQTT_USER;
  char      mqttpass[40] = MQTT_PASS;
  char      mqtttopic[60] = MQTT_TOPIC;

  // if adding a variable, you can do it anywhere, just be sure to add it to configKeyMap() in config.cpp
  // if removing a variable and key, add to deleteOldKeys()
  // note that defaults are mostly built from macros, except a few which are disabled by default
};

#define CONFIG_KEY_ENTRY(field, keyname) { offsetof(config_t, field), keyname, sizeof(((config_t*)0)->field) }

struct configKeyMap {
    size_t fieldOffset;
    const char* key;
    size_t size;
};

#if IR_PIN!=255
  struct ircodes_t
  {
    unsigned int ir_set = 0; // will be 4224 if written/restored correctly
    uint64_t irVals[20][3];
  };
#endif

struct station_t
{
  char name[STATION_FIELD_LENGTH];
  char url[STATION_FIELD_LENGTH];
  char title[STATION_FIELD_LENGTH];
  uint16_t bitrate;
  int  ovol;
};

struct neworkItem
{
  char ssid[30];
  char password[40];
};

class Config {
  public:
    static const char* const wwwFiles[];
    static const size_t wwwFilesCount;
    static const char* const dataFiles[];
    static const size_t dataFilesCount;

    config_t store;
    station_t station;
    theme_t   theme;

    #if IR_PIN!=255
      int irindex = -1;
      uint8_t irchck = 0;
      ircodes_t ircodes;
    #endif

    BitrateFormat configFmt = BF_UNKNOWN;
    neworkItem ssids[5];
    uint8_t ssidsCount = 0;
    uint32_t sdResumePos = 0;
    bool     wwwFilesExist = false;
    uint16_t vuThreshold = 0;
    uint16_t screensaverTicks = 0;
    uint16_t screensaverPlayingTicks = 0;
    bool     isScreensaver = false;
    bool     displayIsInverted = false;
    bool     displayWasInverted = false;
    int      newConfigMode = 0;

    void init();
    void loadPreferences();
    void loadLastStationUrl();
    void changeMode(int newmode=-1);
    void initSDPlaylist();
    void initPlaylistMode();
    void loadTheme();
    void saveIR();
    void defaultSettings(const char *val, uint8_t clientId);
    void processDeferredSaves();
    uint8_t setVolume(uint8_t val);
    void setTone(int8_t bass, int8_t middle, int8_t treble);
    uint8_t setLastStation(uint16_t val);
    uint8_t setCountStation(uint16_t val);
    uint8_t setLastSSID(uint8_t val);
    void setTitle(const char* title);
    void setStation(const char* station);
    void setBrightness(bool dosave=false);
    void setDspOn(bool dspon, bool saveval = true);
    void bootInfo();
    void deleteOldKeys();
    void setLastStationUrl(const char* url, uint16_t waitMs = 5000);
    void flushLastStationUrl();
    void setBitrateFormat(BitrateFormat fmt) { configFmt = fmt; }
    const char* getLastStationUrl() const { return _lastStationUrl; }
    bool hasLastStationUrl() const { return _lastStationUrl[0] != '\0'; }
    uint16_t lastStation() {
      return getMode()==PM_WEB?store.lastStation:store.lastSdStation;
    }
    void lastStation(uint16_t newstation) {
      if (getMode()==PM_WEB) saveValue(&store.lastStation, newstation);
      else saveValue(&store.lastSdStation, newstation);
    }
    uint8_t getMode() { return store.play_mode; }
    FS* SDPLFS() { return _SDplaylistFS; }
    bool isRTCFound() { return _rtcFound; };
    Preferences prefs; // For Preferences, we use a look-up table to maintain compatibility...
    static const configKeyMap keyMap[];

    // Helper to get key map entry for a field pointer
    const configKeyMap* getKeyMapEntryForField(const void* field) const {
        size_t offset = (const uint8_t*)field - (const uint8_t*)&store;
        for (size_t i = 0; keyMap[i].key != nullptr; ++i) {
            if (keyMap[i].fieldOffset == offset) return &keyMap[i];
        }
        return nullptr;
    }
    template <typename T>
    void loadValue(T *field) {
      const configKeyMap* entry = getKeyMapEntryForField(field);
      if (entry) prefs.getBytes(entry->key, field, entry->size);
    }
    bool saveRawValue(const configKeyMap* entry, const void* value, size_t size) {
      prefs.begin("ehradio", false);
      size_t existingLen = prefs.getBytesLength(entry->key);
      bool keyExists = (existingLen == size);
      bool needSave;
      if (keyExists) {
        uint8_t oldValue[size];
        prefs.getBytes(entry->key, oldValue, size);
        needSave = memcmp(oldValue, value, size) != 0;
      } else {
        // Key not in NVS: always write. Comparing against the in-memory field is unreliable
        // because saveValueButWait updates *field before queuing, so currentField == value always.
        needSave = true;
      }
      if (needSave) {
        prefs.putBytes(entry->key, value, size);
        if (strcmp(entry->key, "mqttpass") == 0 || strcmp(entry->key, "weatherkey") == 0) {
          FUNCTIONLOG("Config.key", "%s: *", entry->key);
        } else if (size > 4) {
          FUNCTIONLOG("Config.key", "%s: %s", entry->key, static_cast<const char*>(value));
        } else {
          uint32_t rawValue = 0;
          memcpy(&rawValue, value, size);
          FUNCTIONLOG("Config.key", "%s: %lu", entry->key, static_cast<unsigned long>(rawValue));
        }
      }
      prefs.end();
      return needSave;
    }
    template <typename T>
    void saveValue(T *field, const T &value) {
      const configKeyMap* entry = getKeyMapEntryForField(field);
      if (entry && saveRawValue(entry, &value, entry->size)) *field = value;
    }
    void saveValue(char *field, const char *value) {
      const configKeyMap* entry = getKeyMapEntryForField(field);
      if (entry) {
        size_t sz = entry->size;
        char normalizedValue[sz];
        memset(normalizedValue, 0, sz);
        if (value != nullptr) strlcpy(normalizedValue, value, sz);
        if (saveRawValue(entry, normalizedValue, sz)) strlcpy(field, normalizedValue, sz);
      }
    }
    /* Debounced save: updates the in-memory field immediately, defers the NVS write until
       waitMs milliseconds after the last call for this field. Call processDeferredSaves()
       from the main loop. Falls back to immediate saveValue if all slots are occupied or
       the field has no key-map entry. Not for string/char-array fields (static_assert guards). */
    template <typename T>
    void saveValueButWait(T *field, const T &value, uint16_t waitMs = 2000) {
      static_assert(sizeof(T) <= 4, "saveValueButWait: use saveValue for string/char-array fields");
      const configKeyMap* entry = getKeyMapEntryForField(field);
      if (!entry) { saveValue(field, value); return; }
      *field = value;
      for (uint8_t i = 0; i < DEFERRED_SAVE_SLOTS; ++i) {
        if (_deferredSaves[i].entry == entry) {
          memcpy(_deferredSaves[i].data, &value, sizeof(T));
          _deferredSaves[i].dueMs = millis() + waitMs;
          return;
        }
      }
      for (uint8_t i = 0; i < DEFERRED_SAVE_SLOTS; ++i) {
        if (_deferredSaves[i].entry == nullptr) {
          memcpy(_deferredSaves[i].data, &value, sizeof(T));
          _deferredSaves[i].dueMs = millis() + waitMs;
          _deferredSaves[i].entry = entry; /* written last — acts as a release signal to processDeferredSaves */
          return;
        }
      }
      saveValue(field, value); /* all slots occupied — fall back to immediate write */
    }
    uint32_t getChipId() {
      uint32_t chipId = 0;
      for(int i=0; i<17; i=i+8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
      }
      return chipId;
    }

  private:
    bool _bootDone = false;
    bool _rtcFound = false;
    FS* _SDplaylistFS = nullptr;
    char _lastStationUrl[MQTT_URL_SIZE + 1] = {0};
    bool _lastStationUrlDirty = false;
    uint32_t _lastStationUrlDueMs = 0;

    static constexpr uint8_t DEFERRED_SAVE_SLOTS = 8;
    struct DeferredSave {
      const configKeyMap* volatile entry = nullptr; /* volatile: written by WebSocket task, read by main loop */
      uint8_t data[4] = {};
      uint32_t dueMs = 0;
    };
    DeferredSave _deferredSaves[DEFERRED_SAVE_SLOTS];

    bool _wwwFilesExist();
    void _initHW();
    bool _writeLastStationUrlFile();
    uint16_t color565(uint8_t r, uint8_t g, uint8_t b);
    void setDefaults();

    uint16_t _randomStation() {
      randomSeed(esp_random() ^ millis());
      uint16_t station = random(1, store.countStation);
      return station;
    }
};

extern Config config;

#endif
