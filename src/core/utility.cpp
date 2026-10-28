#include "options.h"
#include "utility.h"

#include <ESPFileUpdater.h>
#include <WiFi.h>
#include <ctype.h>
#include <cstdlib>
#include <cstring>
#include <stdarg.h>
#include <driver/rtc_io.h>
#include "backlightcontrols.h"
#include "config.h"
#include "display.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "../displays/tools/pretext.h"

// Fills 'out' with the current local time and returns true only when the system clock is real. "Real" needs a floor:
// before SNTP the clock is seconds since boot - a valid-looking positive time_t - so one year of epoch is the smallest
// honest floor, and below it localtime() has no timezone to apply either. This is the one place that read happens:
// clockTrustworthy() and refreshClock() both ask here instead of each calling time(NULL) and localtime_r().
bool systemLocalTime(struct tm& out) {
  const time_t now = time(nullptr);
  if (now <= 31536000) return false;  // seconds since boot, not a date
  return localtime_r(&now, &out) != nullptr;
}

bool clockTrustworthy() {
  // The clock may only print a time the device can stand behind.  network.timeinfo is now a pure cache filled by
  // refreshClock(), so the only question left is whether it holds anything real:
  //   1. Nothing has set it yet - a zeroed struct is year 1900, so 1901 or less IS the "never set" state and the
  //      zeroed struct is its own marker; the no-RTC radio showed 00:00 here until the first doSync().
  //   2. With the system clock unset there is nothing to check against, and localtime() has no timezone to apply
  //      either (configTzTime() only runs at the WiFi-connect event).  Offline the RTC chip is the only source there
  //      will ever be, so it is trusted; online it is not, because SNTP and doSync() have not spoken yet.
  // No agreement test is needed any more: refreshClock() copies the system clock whenever it is set, so an RTC in
  // another zone can no longer reach the glass; the chip only fills the cache while the system clock is unset, which
  // is exactly the "nothing to agree with" case handled below.  The chip is kept honest by doSync()'s rtc.setTime().
  if (network.timeinfo.tm_year + 1900 <= 1901) return false;   // nothing has set it: year 1900, the zeroed struct
  struct tm sysTm;
  if (systemLocalTime(sysTm)) return true;                     // the system clock is authoritative once set
  return network.status == SDOFFLINE;                          // no system clock: trust the chip only offline
}

namespace {

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
  #ifndef DEEP_SLEEP_DISABLE
    // Configure RTC-domain pulls on all wake pins (digital pulls power off during deep sleep)
    {
      uint64_t mask = WAKE_GPIO_MASK;
      for (uint8_t i = 0; i < 40; i++) {
        if (mask & (1ULL << i)) {
          rtc_gpio_init((gpio_num_t)i);
          rtc_gpio_set_direction((gpio_num_t)i, RTC_GPIO_MODE_INPUT_ONLY);
          rtc_gpio_pullup_en((gpio_num_t)i);
          rtc_gpio_pulldown_dis((gpio_num_t)i);
        }
      }
      #if defined(ARDUINO_ESP32S3_DEV)
        // S3: native GPIO wake on LOW level (button PRESS, not release)
        for (uint8_t i = 0; i < 40; i++) {
          if (mask & (1ULL << i)) {
            gpio_wakeup_enable((gpio_num_t)i, GPIO_INTR_LOW_LEVEL);
          }
        }
        esp_sleep_enable_gpio_wakeup();
      #else
        // ESP32 classic: ext1 wake on HIGH transition (button RELEASE)
        // Wait for all wake pins to be released before sleeping
        for (uint8_t retry = 0; retry < 300; retry++) {  // ~3s max
          bool allHigh = true;
          for (uint8_t i = 0; i < 40; i++) {
            if (mask & (1ULL << i)) {
              if (digitalRead(i) == LOW) { allHigh = false; break; }
            }
          }
          if (allHigh) break;
          delay(10);
        }
        esp_sleep_enable_ext1_wakeup(WAKE_GPIO_MASK, ESP_EXT1_WAKEUP_ANY_HIGH);
      #endif
    }
  #endif
}

volatile uint8_t Utility::sleepWanted = 0;

// Both of these are called from an esp_timer task, never from the main loop: `ticks()` is a Ticker callback
// (`network.ctimer.attach(1, ticks)`) and so is the sleep timer that sleepForAfter() arms.  So they do the one thing that
// is safe from any task - raise a flag - and the sleep itself happens in serviceSleep() below, on the main loop.
void Utility::requestSleep()     { sleepWanted = 1; }  // sleep now, no wake timer
void Utility::requestSleepWake() { sleepWanted = 2; }  // sleep, then wake after `sleepfor` minutes

// Main loop: carry out a sleep that a timer, a button or the `sleep` command asked for.  Two phases, because the request
// and the teardown used to happen in the same instant: display.putRequest(NEWMODE, SLEEPING) is queued to the display
// task, while sleepCore() calls display.deepsleep() microseconds later - so the message was never painted, and the timer
// paths never asked for one at all.  Phase A wakes a blanked panel, requests the message and arms SLEEP_MSG_HOLD; phase B
// does the teardown once the hold has expired.  Deliberately non-blocking: the display task needs those seconds to draw,
// and the audio task keeps feeding through them.
void Utility::serviceSleep() {
  static uint32_t sleepAtMs = 0;
  static bool     holding   = false;
  const uint8_t what = sleepWanted;
  #ifndef DEEP_SLEEP_DISABLE
    if (what && !holding && SLEEP_MSG_HOLD > 0) {
      holding = true;
      sleepAtMs = millis() + (uint32_t)SLEEP_MSG_HOLD * 1000UL;
      // Silence first, so the message is noticed.
      network.cancelStreamRetry();
      player.sendCommand({PR_STOP, 0});
      config.setDspOn(true);            // a blanked panel would swallow the message we are about to ask for
      display.putRequest(NEWMODE, SLEEPING);
      FUNCTIONLOG("SLEEP", "Sleeping in %u s", (unsigned)SLEEP_MSG_HOLD);
      return;
    }
    if (holding && (int32_t)(millis() - sleepAtMs) < 0) return;   // signed: correct across the millis() wrap
  #endif
  if (!what && !holding) return;                       // nothing asked and nothing pending: the common case
  sleepWanted = 0;
  holding = false;
  #ifndef DEEP_SLEEP_DISABLE
    if (what == 2) doSleep(); else doSleepW();
  #endif
}

void Utility::doSleep() {
  #ifndef DEEP_SLEEP_DISABLE
    // 'sleepfor' is minutes: the wake timer takes microseconds, and the message says minutes.  It used to say
    // "Will wake after %i minutes" with sleepfor * 60, i.e. five minutes printed as three hundred.
    if (sleepfor > 0) FUNCTIONLOG("SLEEP", "Sleeping now.  Will wake after %i minutes.", (int)sleepfor);
    else              FUNCTIONLOG("SLEEP", "Sleeping now.");
    Serial.flush();  // esp_deep_sleep_start() does not wait for the UART to drain, and this line is the record
    delay(50);
    sleepCore();
    if (sleepfor > 0) {
      esp_sleep_enable_timer_wakeup(sleepfor * 60 * 1000000ULL);
    }
    esp_deep_sleep_start();
  #endif
}

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

void Utility::normalizeToCRLF(const char* input, char* output, size_t outputSize) {
  if (!input || !output || outputSize == 0) return;

  size_t inputLen = strlen(input);
  size_t out = 0;

  bool needsConversion = false;
  for (size_t i = 0; i < inputLen; ++i) {
    if (input[i] == '\n' && (i == 0 || input[i - 1] != '\r')) {
      needsConversion = true;
      break;
    }
  }

  if (!needsConversion) {
    size_t copyLen = (inputLen < outputSize - 1) ? inputLen : outputSize - 1;
    memcpy(output, input, copyLen);
    output[copyLen] = '\0';
    return;
  }

  for (size_t i = 0; i < inputLen && out + 1 < outputSize; ++i) {
    char c = input[i];
    if (c != '\n') {
      output[out++] = c;
    } else if (i > 0 && input[i - 1] == '\r') {
      output[out++] = '\n'; // already CRLF
    } else if (out + 2 < outputSize) {
      output[out++] = '\r';
      output[out++] = '\n';
    } else {
      break; // no room for the pair
    }
  }
  output[(out < outputSize) ? out : outputSize - 1] = '\0';
}

bool Utility::isHttpUrl(const char* text) {
  return text && (strncmp(text, "http://", 7) == 0 || strncmp(text, "https://", 8) == 0);
}

bool Utility::parseCommandLine(const char* input, char* command, size_t commandSize, char* value, size_t valueSize) {
  if (!input || input[0] == '\0' || !command || !value || commandSize == 0 || valueSize == 0) return false;

  command[0] = '\0';
  value[0] = '\0';

  if (isHttpUrl(input)) {
    strlcpy(command, "burl", commandSize);
    strlcpy(value, input, valueSize);
    return true;
  }

  const char* eq = strchr(input, '=');
  if (eq) {
    size_t commandLen = static_cast<size_t>(eq - input);
    if (commandLen >= commandSize) commandLen = commandSize - 1;
    memcpy(command, input, commandLen);
    command[commandLen] = '\0';
    strlcpy(value, eq + 1, valueSize);
  } else {
    const char* open = strchr(input, '(');
    const char* close = strrchr(input, ')');
    if (open && close && close > open) {
      size_t commandLen = static_cast<size_t>(open - input);
      if (commandLen >= commandSize) commandLen = commandSize - 1;
      memcpy(command, input, commandLen);
      command[commandLen] = '\0';

      size_t valueLen = static_cast<size_t>(close - open - 1);
      if (valueLen >= valueSize) valueLen = valueSize - 1;
      memcpy(value, open + 1, valueLen);
      value[valueLen] = '\0';
    } else {
      const char* space = strchr(input, ' ');
      if (space) {
        size_t commandLen = static_cast<size_t>(space - input);
        if (commandLen >= commandSize) commandLen = commandSize - 1;
        memcpy(command, input, commandLen);
        command[commandLen] = '\0';
        strlcpy(value, space + 1, valueSize);
      } else {
        strlcpy(command, input, commandSize);
      }
    }
  }

  stripWhitespace(command);
  stripWhitespace(value);
  stripWrappingQuotes(value);

  if (strcmp(command, "play") == 0) {
    if (value[0] == '\0') {
      strlcpy(command, "start", commandSize);
    } else if (isHttpUrl(value)) {
      strlcpy(command, "burl", commandSize);
    }
  }

  return command[0] != '\0';
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
      FUNCTIONLOG("REBOOT", "Reboot triggered by Wifi.");
      config.saveValue(&config.store.bootStableMarker, true);
      delay(250);
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

  File file = LittleFS.open(TMP_PATH, "w");
  if (!file) return false;
  for (int i = 0; i < config.ssidsCount; i++) {
    if (strlen(config.ssids[i].ssid) > 0) {
      // can't use printf — Arduino's Print::printf has a 64-byte stack buffer that overflows on long URLs
      file.print(config.ssids[i].ssid);
      file.print('\t');
      file.print(config.ssids[i].password);
      file.write((const uint8_t*)"\r\n", 2);
    }
  }
  file.close();

  if (LittleFS.exists(TMP_PATH)) {
    LittleFS.remove(SSIDS_PATH);
    return LittleFS.rename(TMP_PATH, SSIDS_PATH);
  }
  return false;
}

bool Utility::importWifi() {
  if (!LittleFS.exists(TMP_PATH)) return false;
  LittleFS.remove(SSIDS_PATH);
  LittleFS.rename(TMP_PATH, SSIDS_PATH);
  FUNCTIONLOG("REBOOT", "Reboot trigger by Import Wifi.");
  config.saveValue(&config.store.bootStableMarker, true);
  delay(250);
  ESP.restart();
  return true;
}

void Utility::indexPlaylist() {
  File playlist = LittleFS.open(PLAYLIST_PATH, "r");
  if (!playlist) {
    FUNCTIONLOG("Playlist", "indexPlaylist: playlist.csv not found, cannot build index");
    return;
  }

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;
  File index = LittleFS.open(INDEX_PATH, "w");
  uint32_t entries = 0;
  while (playlist.available()) {
    uint32_t pos = playlist.position();
    if (parseCSV(playlist.readStringUntil('\n').c_str(), stationName, stationUrl, stationOvol)) {
      // Stop READING at the ceiling: a row past MAX_PLAYLIST_ENTRIES can never be given an id. Breaking here
      // rather than skipping keeps the index contiguous and stops the parse of a hopeless file.
      if (entries >= MAX_PLAYLIST_ENTRIES) {
        WARNINGLOG("playlist.csv has more than %u stations: the rest are not indexed and cannot be played",
                   (unsigned)MAX_PLAYLIST_ENTRIES);
        break;
      }
      index.write((uint8_t*)&pos, 4);
      entries++;
    }
  }
  index.close();
  playlist.close();
  size_t idxSize = 0;
  if (LittleFS.exists(INDEX_PATH)) {
    File idxRead = LittleFS.open(INDEX_PATH, "r");
    if (idxRead) {
      idxSize = idxRead.size();
      idxRead.close();
    }
  }
  FUNCTIONLOG("Playlist", "indexPlaylist: built index.dat, %u bytes (%u entries)", idxSize, idxSize / 4);
}

void Utility::initPlaylist() {
  if (!LittleFS.exists(INDEX_PATH)) {
    FUNCTIONLOG("Playlist", "initPlaylist: index missing, running clean and index");
    bool cleaned = cleanPlaylist();
    if (!cleaned) indexPlaylist();
  } else {
    FUNCTIONLOG("Playlist", "initPlaylist: index exists, no action needed");
  }
  // Both files, every boot: the pair is what says whether the device and the playlist agree, and an index
  // that does not belong to the CSV is a device reporting no stations while the file lists them.
  size_t csvSize = 0, idxSize = 0;
  File csv = LittleFS.open(PLAYLIST_PATH, "r");
  if (csv) { csvSize = csv.size(); csv.close(); }
  File idx = LittleFS.open(INDEX_PATH, "r");
  if (idx) { idxSize = idx.size(); idx.close(); }
  FUNCTIONLOG("Playlist", "initPlaylist: %u bytes of playlist.csv, %u index entries (%u bytes)",
              (unsigned)csvSize, (unsigned)(idxSize / 4), (unsigned)idxSize);
  // The index that was just built is the authoritative count for PM_WEB, and it is the width the big number is
  // zero-filled to: the display task may not open the index, so the store carries the figure.  Clamped the same
  // way playlistLength() clamps, or a full file would wrap to zero here.
  uint32_t entries = (uint32_t)(idxSize / 4);
  if (entries > MAX_PLAYLIST_ENTRIES) entries = MAX_PLAYLIST_ENTRIES;
  config.setCountStation((uint16_t)entries);
}

bool Utility::cleanPlaylist() {
  // Phase 1: Scan for blank lines, bare LF, or invalid CSV format
  File playlist = LittleFS.open(PLAYLIST_PATH, "r");
  if (!playlist) {
    FUNCTIONLOG("Playlist", "cleanPlaylist: playlist.csv not found, nothing to clean");
    return false;
  }

  bool needsClean = false;
  while (playlist.available()) {
    String line = playlist.readStringUntil('\n');
    // Check for bare LF (no preceding CR) — should be CRLF
    if (line.length() == 0 || line[line.length()-1] != '\r') {
      needsClean = true;
      break;
    }
    // Check for blank/whitespace-only lines (just "\r")
    const char* p = line.c_str();
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') {
      needsClean = true;
      break;
    }
    // Check for invalid CSV format (must be tab-delimited: NAME\tURL\tOVOL)
    {
      char _name[STATION_FIELD_LENGTH] = {0};
      char _url[STATION_FIELD_LENGTH] = {0};
      int _ovol = 0;
      if (!parseCSV(line.c_str(), _name, _url, _ovol)) {
        needsClean = true;
        break;
      }
    }
  }
  playlist.close();

  if (!needsClean) {
    FUNCTIONLOG("Playlist", "cleanPlaylist: verified, no cleaning needed");
    return false;  // File is already clean
  }

  // Phase 2: Rewrite clean version with CRLF line endings
  FUNCTIONLOG("Playlist", "cleanPlaylist: issues found, rewriting...");
  playlist = LittleFS.open(PLAYLIST_PATH, "r");
  File tmpFile = LittleFS.open(TMP_PATH, "w");
  if (!playlist || !tmpFile) {
    FUNCTIONLOG("Playlist", "cleanPlaylist: failed to open file for rewrite (playlist=%d tmp=%d)", playlist ? 1 : 0, tmpFile ? 1 : 0);
    if (playlist) playlist.close();
    if (tmpFile) tmpFile.close();
    return false;
  }

  char stationName[STATION_FIELD_LENGTH] = {0};
  char stationUrl[STATION_FIELD_LENGTH] = {0};
  int stationOvol = 0;

  while (playlist.available()) {
    String line = playlist.readStringUntil('\n');
    // Strip trailing \r (normalize to bare content)
    if (line.length() > 0 && line[line.length()-1] == '\r') {
      line = line.substring(0, line.length()-1);
    }
    // Skip blank/whitespace-only lines
    const char* p = line.c_str();
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '\0') continue;
    // Write only valid CSV lines with CRLF ending
    if (parseCSV(line.c_str(), stationName, stationUrl, stationOvol)) {
      tmpFile.print(line);
      tmpFile.write((const uint8_t*)"\r\n", 2);
    }
  }

  playlist.close();
  tmpFile.close();

  // Replace original with cleaned version
  LittleFS.remove(PLAYLIST_PATH);
  if (!LittleFS.rename(TMP_PATH, PLAYLIST_PATH)) {
    FUNCTIONLOG("Playlist", "cleanPlaylist: failed to rename cleaned playlist");
    LittleFS.remove(TMP_PATH);
    return false;
  }

  // Rebuild index from clean file
  LittleFS.remove(INDEX_PATH);
  indexPlaylist();

  FUNCTIONLOG("Playlist", "cleanPlaylist: rewrite complete, index rebuilt");
  return true;
}

uint16_t Utility::playlistLength() {
  uint16_t out = 0;
  if (config.SDPLFS()->exists(REAL_INDEX)) {
    File index = config.SDPLFS()->open(REAL_INDEX, "r");
    size_t sz = index.size();
    // SD index has an 8-byte footer: [magic:4][count:4]
    size_t rows = (config.getMode() == PM_SDCARD) ? ((sz >= 8) ? ((sz - 8) / 4) : 0) : (sz / 4);
    index.close();
    // This narrowing is where a full card used to become an empty one: 65536 rows truncate to 0, and every consumer then sees an
    // empty playlist.  Clamping here is the one chokepoint no future writer can defeat. See MAX_PLAYLIST_ENTRIES in config.h.
    if (rows > MAX_PLAYLIST_ENTRIES) {
      static bool warned = false;
      if (!warned) {
        warned = true;
        WARNINGLOG("playlist index holds %u entries; only the first %u have ids and can be played",
                   (unsigned)rows, (unsigned)MAX_PLAYLIST_ENTRIES);
      }
      rows = MAX_PLAYLIST_ENTRIES;
    }
    out = (uint16_t)rows;
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
  const bool ok = readStationEntry(playlist, index, stationId, stationName, stationUrl, stationOvol);
  if (ok) {
    memset(config.station.url, 0, STATION_FIELD_LENGTH);
    memset(config.station.name, 0, STATION_FIELD_LENGTH);
    strncpy(config.station.name, stationName, STATION_FIELD_LENGTH);
    strncpy(config.station.url, stationUrl, STATION_FIELD_LENGTH);
    config.station.ovol = stationOvol;
    config.setLastStation(stationId);
  }
  playlist.close();
  index.close();
  return ok;
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

uint16_t Utility::fillPlaylistRange(int from, uint8_t count, char names[][STATION_FIELD_LENGTH / 2]) {
  for (uint8_t c = 0; c < count; c++) names[c][0] = '\0';
  uint16_t total = playlistLength();
  if (total == 0 || count == 0) return total;

  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  File index = config.SDPLFS()->open(REAL_INDEX, "r");
  if (!playlist || !index) {
    if (playlist) playlist.close();
    if (index) index.close();
    return total;
  }

  char nameBuf[STATION_FIELD_LENGTH] = {0};
  char urlBuf[STATION_FIELD_LENGTH] = {0};
  int ovolBuf = 0;
  for (uint8_t c = 0; c < count; c++) {
    int stationId = from + c;
    if (stationId < 1 || stationId > (int)total) continue;
    memset(nameBuf, 0, sizeof(nameBuf));
    if (readStationEntry(playlist, index, (uint16_t)stationId, nameBuf, urlBuf, ovolBuf)) {
      strlcpy(names[c], nameBuf, STATION_FIELD_LENGTH / 2);
    }
  }

  playlist.close();
  index.close();
  return total;
}

char* Utility::stationByNum(uint16_t num) {
  memset(stationBuf, 0, sizeof(stationBuf));
  if (num < 1) return stationBuf;

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
  #ifndef DEEP_SLEEP_DISABLE
    FUNCTIONLOG("SLEEP", "Sleeping now.");
    Serial.flush();  // esp_deep_sleep_start() does not wait for the UART to drain
    delay(50);
    sleepCore();
    esp_deep_sleep_start();
  #endif
}

void Utility::sleepForAfter(uint16_t sleepfor, uint16_t sa) {
  Utility::sleepfor = sleepfor;
  if (sa > 0) {
    sleepTimer.attach(sa * 60, Utility::requestSleepWake);  // the callback only raises the flag; serviceSleep() sleeps
  } else {
    Utility::doSleep();  // already on the main loop, so sleep here and now
  }
}

void Utility::stopStandby() {
  config.setDspOn(true);
  backlightControls.restart();
  if (config.store.smartstart) {
    if (config.getMode() == PM_WEB) player.resumeLastWebSource();
    else player.sendCommand({PR_PLAY, config.lastStation()});
  }
}

void Utility::startStandby() {
  network.cancelStreamRetry();
  config.setDspOn(false);
  backlightControls.restart();
  player.sendCommand({PR_STOP, 0});
}

void Utility::toggleStandby() {
  if (config.store.dspon) startStandby();
  else stopStandby();
}

bool Utility::verifyLittleFS() {
  const size_t n = Config::wwwFilesCount;
  if (n == 0 || n > 64) return false;  // nothing to verify, or wider than the bitmask below

  #ifdef BOOTLOG_TIME
    const uint32_t t0 = millis();  // measured for the summary line below, and only printed with BOOTLOG_TIME
  #endif
  uint64_t plainSeen = 0, gzSeen = 0;
  // Listing "/" is not enough: LittleFS is a real filesystem, so the root listing yields only
  // the www and data directory entries. This check only cares about the www files, so /www is
  // opened directly. The basename is compared rather than the whole path, so this stays correct
  // whether or not the core returns a full path from File::path().
  File root = LittleFS.open("/www");
  if (root && root.isDirectory()) {
    File f = root.openNextFile();
    while (f) {
      if (!f.isDirectory()) {
        const String path = f.path();
        String name = path.substring(path.lastIndexOf('/') + 1);
        const bool gz = name.endsWith(".gz");
        if (gz) name.remove(name.length() - 3);
        for (size_t i = 0; i < n; i++) {
          if (name == Config::wwwFiles[i]) {
            if (gz) gzSeen |= (1ULL << i); else plainSeen |= (1ULL << i);
            break;
          }
        }
      }
      f = root.openNextFile();
    }
    root.close();
  }

  // Both forms of one file is a leftover from an update that switched to the compressed copy
  const uint64_t bothSeen = plainSeen & gzSeen;
  for (size_t i = 0; i < n; i++) {
    if (bothSeen & (1ULL << i)) {
      char fullPath[64];
      snprintf(fullPath, sizeof(fullPath), "/www/%s", Config::wwwFiles[i]);
      LittleFS.remove(fullPath);
      FUNCTIONLOG("Cleanup", "Removed duplicate (compressed version exists): %s", fullPath);
    }
  }

  const uint64_t have = plainSeen | gzSeen;
  const uint64_t need = (n >= 64) ? ~0ULL : ((1ULL << n) - 1);
  uint16_t present = 0, asPlain = 0, asGz = 0;
  for (size_t i = 0; i < n; i++) {
    const uint64_t bit = 1ULL << i;
    if (have & bit) present++;
    if (plainSeen & bit) asPlain++;
    if (gzSeen & bit) asGz++;
  }
  const bool ok = (have == need);
  // The listing pass is the interesting part of this line, but its cost is a diagnostic like the boot-stage markers:
  // printed with BOOTLOG_TIME, omitted without it.
  char tookStr[16] = "";
  #ifdef BOOTLOG_TIME
    snprintf(tookStr, sizeof(tookStr), ", %lums", (unsigned long)(millis() - t0));
  #endif
  FUNCTIONLOG("Cleanup", "verifyLittleFS: %u/%u present (%u plain, %u gz)%s%s",
              (unsigned)present, (unsigned)n, (unsigned)asPlain, (unsigned)asGz,
              tookStr, ok ? "" : ", MISSING FILES");
  if (!ok) {
    for (size_t i = 0; i < n; i++) {
      if (!(have & (1ULL << i))) { FUNCTIONLOG("Cleanup", "  missing: %s", Config::wwwFiles[i]); break; }
    }
  }
  return ok;
}

void Utility::pruneLittleFS() {
  FUNCTIONLOG("Cleanup", "Scanning LittleFS for unwanted files...");

  // As with verifyLittleFS, the root listing only yields directory entries under LittleFS, so
  // the two directories are walked explicitly. Directory entries themselves are never removed.
  // Keep this list closed to these two: the config and playlist files live at the root, and a sweep widened to it
  // would delete them.
  static const char* const dirs[] = {"/www", "/data"};
  for (const char* dir : dirs) {
    File root = LittleFS.open(dir);
    if (!root || !root.isDirectory()) continue;

    const bool isWww = (strcmp(dir, "/www") == 0);
    File file = root.openNextFile();
    while (file) {
      if (file.isDirectory()) {
        file = root.openNextFile();
        continue;
      }

      const String path = file.path();
      const String name = path.substring(path.lastIndexOf('/') + 1);
      bool keep = false;

      if (isWww) {
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
          const String gzPath = String(dir) + "/" + name + ".gz";
          if (LittleFS.exists(gzPath)) {
            LittleFS.remove(path);
            FUNCTIONLOG("Cleanup", "Removed duplicate (compressed version exists): %s", path.c_str());
            file = root.openNextFile();
            continue;
          }
        }
      } else {
        for (size_t i = 0; i < Config::dataFilesCount; i++) {
          if (name == String(Config::dataFiles[i])) {
            keep = true;
            break;
          }
        }
      }

      if (!keep) {
        LittleFS.remove(path);
        FUNCTIONLOG("Cleanup", "Removed: %s", path.c_str());
      }
      file = root.openNextFile();
    }
    root.close();
  }

  FUNCTIONLOG("Cleanup", "All unnecessary files purged");
}

void Utility::deleteMainwwwFile() {
  if (Config::wwwFilesCount == 0) return;

  const char* lastFile = Config::wwwFiles[Config::wwwFilesCount - 1];
  char mainFile[64] = {0};
  for (const char* suffix : {"", ".gz"}) {
    snprintf(mainFile, sizeof(mainFile), "/www/%s%s", lastFile, suffix);
    if (LittleFS.exists(mainFile)) {
      LittleFS.remove(mainFile);
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
    // Invalidate PSRAM cache entry for the updated file (if it's a cached www file)
    const char* basename = strrchr(localFile, '/');
    if (basename) {
      basename++; // skip '/'
      char cachePath[32];
      snprintf(cachePath, sizeof(cachePath), "/%s", basename);
      // Strip .gz suffix from cache path
      size_t clen = strlen(cachePath);
      if (clen > 3 && strcmp(cachePath + clen - 3, ".gz") == 0) {
        cachePath[clen - 3] = '\0';
      }
      netserver.invalidateCache(cachePath);
    }
  } else if (result == ESPFileUpdater::NOT_MODIFIED || result == ESPFileUpdater::MAX_AGE_NOT_REACHED) {
    FUNCTIONLOG("ESPFileUpdater", "%s - no update needed", simpleName);
  } else {
    FUNCTIONLOG("ESPFileUpdater", "%s - update failed", simpleName);
  }
}

// Software CRC32 (portable fallback; ESP32 has hardware crc32_le in ROM)
static uint32_t crc32_update(uint32_t crc, const uint8_t* data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int j = 0; j < 8; j++) {
      crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
    }
  }
  return crc;
}

// Compute CRC32 of an open file's content (from current position, for 'len' bytes)
uint32_t fileCRC32(File& f, size_t len) {
  uint32_t crc = 0;
  uint8_t buf[64];
  while (len > 0) {
    size_t chunk = (len < sizeof(buf)) ? len : sizeof(buf);
    size_t read = f.readBytes((char*)buf, chunk);
    if (read == 0) break;
    crc = crc32_update(crc, buf, read);
    len -= read;
  }
  return crc;
}

// Special function to trim L10N_MSG_OFFLINE_15CHAR (msg_offline_15char in the display locale json files)
// to the 15 characters that fit in the IP widget space
const char* utf8_trim15(const char* src) {
  static constexpr uint16_t maxChars = 15;
  static char buf[64];
  uint16_t len = utf8_strlen(src);
  if (len <= maxChars) return src;
  const char* cut = utf8_offset(src, maxChars);
  size_t bytes = cut - src;
  if (bytes >= sizeof(buf)) bytes = sizeof(buf) - 1;
  memcpy(buf, src, bytes);
  buf[bytes] = '\0';
  return buf;
}

uint8_t rssiLevel(int rssi) {
  const int steps[] = { RSSI_STEPS };
  if (rssi >= steps[0]) return 4;
  if (rssi >= steps[1]) return 3;
  if (rssi >= steps[2]) return 2;
  if (rssi >= steps[3]) return 1;
  return 0;
}

Utility utility;
