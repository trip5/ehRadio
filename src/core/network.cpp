#include "options.h"
#include <time.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ehDP.h>
#include <ESPFileUpdater.h>
#include <ESPmDNS.h>
#include <ImprovWiFiLibrary.h>
#include "config.h"
#include "display.h"
#include "locale.h"
#include "mqtt.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "rtcsupport.h"
#include "telnet.h"
#include "../pluginsManager/pluginsManager.h"

#ifndef WIFI_ATTEMPTS
  #define WIFI_ATTEMPTS  16
#endif

MyNetwork network;

TaskHandle_t syncTaskHandle;
TaskHandle_t streamRetryTaskHandle = NULL;

bool getWeather(char *wstr);
void doSync(void * pvParameters);
void retryStreamConnection(void * pvParameters);
static bool onImprovCustomConnect(const char* ssid, const char* password);

EhDP ehdp;

void ticks() {
  if (!display.ready()) return; //waiting for SD is ready
  pm.on_ticker();
  static uint32_t timeSyncTicks = 0;
  static uint16_t weatherSyncTicks = 0;
  static bool divrssi;
  timeSyncTicks++;
  weatherSyncTicks++;
  divrssi = !divrssi;
  if (network.status == CONNECTED) {
    if (config.store.ehdp) ehdp.loop();
    if (network.forceTimeSync || network.forceWeather) {
      xTaskCreatePinnedToCore(doSync, "doSync", 1024 * 4, NULL, 0, &syncTaskHandle, 0);
    }
    // check at :01s mark (fix network clock not matching system clock after Daylight Savings Time changes)
    if (network.timeinfo.tm_sec == 1) {
      time_t now = time(NULL);
      struct tm localNow;
      localtime_r(&now, &localNow);
      if ((network.timeinfo.tm_min != localNow.tm_min) || (network.timeinfo.tm_hour != localNow.tm_hour)) {
        timeSyncTicks = 0;
        network.forceTimeSync = true;
      }
    }
    // Time sync interval: config value is in hours, convert to seconds
    uint32_t timeSyncInterval = (uint32_t)config.store.timesyncinterval * 3600;
    if (timeSyncTicks >= timeSyncInterval) {
      timeSyncTicks=0;
      network.forceTimeSync = true;
    }
    // Weather sync interval: config value is in minutes, convert to seconds
    uint16_t weatherSyncInterval = (uint16_t)config.store.weathersyncinterval * 60;
    if (weatherSyncTicks >= weatherSyncInterval) {
      weatherSyncTicks=0;
      network.forceWeather = true;
    }
  }
  #ifndef DSP_LCD
    if (config.store.screensaverEnabled && display.mode()==PLAYER && !player.isRunning()) {
      config.screensaverTicks++;
      if (config.screensaverTicks > config.store.screensaverTimeout+SCREENSAVERSTARTUPDELAY) {
        if (config.store.screensaverBlank) {
          display.putRequest(NEWMODE, SCREENBLANK);
        } else {
          display.putRequest(NEWMODE, SCREENSAVER);
        }
      }
    }
    if (config.store.screensaverPlayingEnabled && display.mode()==PLAYER && player.isRunning()) {
      config.screensaverPlayingTicks++;
      if (config.screensaverPlayingTicks > config.store.screensaverPlayingTimeout*60+SCREENSAVERSTARTUPDELAY) {
        if (config.store.screensaverPlayingBlank) {
          display.putRequest(NEWMODE, SCREENBLANK);
        } else {
          display.putRequest(NEWMODE, SCREENSAVER);
        }
      }
    }
  #endif //#ifndef DSP_LCD
  #if RTCSUPPORTED
    if (config.isRTCFound()) {
      rtc.getTime(&network.timeinfo);
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #else
    if (network.timeinfo.tm_year>100 || network.status == SDREADY) {
      network.timeinfo.tm_sec++;
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #endif //#if RTCSUPPORTED
  if (player.isRunning() && config.getMode()==PM_SDCARD) netserver.requestOnChange(SDPOS, 0);
  if (divrssi) {
    if (network.status == CONNECTED) {
      netserver.setRSSI(WiFi.RSSI());
      netserver.requestOnChange(NRSSI, 0);
      display.putRequest(DSPRSSI, netserver.getRSSI());
    }
    #ifdef USE_SD
      if (display.mode()!=SDCHANGE) player.sendCommand({PR_CHECKSD, 0});
      #if SD_AUTOPLAY && SD_CARD_DETECT_PIN!=255
        if (config.getMode()!=PM_SDCARD && digitalRead(SD_CARD_DETECT_PIN)==LOW)
          config.changeMode(PM_SDCARD);
      #endif
    #endif
    player.sendCommand({PR_VUTONUS, 0});
  }
}

void retryStreamConnection(void * pvParameters) {
  const uint8_t maxAttempts = 40;  // 40 attempts * 15 seconds = 10 minutes
  uint8_t attemptCount = 0;
  while (attemptCount < maxAttempts) {
    delay(15000);  // Wait 15 seconds between attempts
    // Check if we should still be retrying
    if (network.lostPlaying && WiFi.status() == WL_CONNECTED && !player.isRunning()) {
      attemptCount++;
      Serial.printf("Stream reconnect attempt %d/%d\n", attemptCount, maxAttempts);
      player.sendCommand({PR_PLAY, config.lastStation()});
      delay(3000);  // Give it a moment to try connecting
      // Check if it worked
      if (player.isRunning()) {
        Serial.println("Stream reconnected successfully!");
        network.lostPlaying = false;
        streamRetryTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
      }
    } else {
      // Conditions changed (user pressed stop, or already playing, or WiFi lost again)
      if (!network.lostPlaying || player.isRunning()) {
        network.lostPlaying = false;
      }
      streamRetryTaskHandle = NULL;
      vTaskDelete(NULL);
      return;
    }
  }
  // Max attempts reached - give up
  Serial.println("Stream reconnection failed after 10 minutes. User intervention required.");
  network.lostPlaying = false;
  streamRetryTaskHandle = NULL;
  vTaskDelete(NULL);
}

void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  network.beginReconnect = false;
  player.lockOutput = false;
  delay(100);
  display.putRequest(NEWMODE, PLAYER);
  if (config.getMode()==PM_SDCARD) {
    network.status=CONNECTED;
    display.putRequest(NEWIP, 0);
  } else {
    display.putRequest(NEWMODE, PLAYER);
    if (network.lostPlaying) {
      player.sendCommand({PR_PLAY, config.lastStation()});
      // Launch retry task if not already running
      if (streamRetryTaskHandle == NULL) {
        xTaskCreatePinnedToCore(retryStreamConnection, "streamRetry", 1024 * 4, NULL, 1, &streamRetryTaskHandle, 0);
      }
    }
  }
  #ifdef MQTT_ENABLE
    if (config.store.mqttenable) connectToMqtt();
  #endif
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (!network.beginReconnect) {
    Serial.printf("WiFiLost: %lu ms, event=%d, SSID=%s, RSSI=%d\n", millis(), (int)event, config.ssids[config.store.lastSSID-1].ssid, WiFi.RSSI());
    if (config.getMode()==PM_SDCARD) {
      network.status=SDREADY;
      display.putRequest(NEWIP, 0);
    } else {
      network.lostPlaying = player.isRunning();
      if (network.lostPlaying) { player.lockOutput = true; player.sendCommand({PR_STOP, 0}); }
      // when we're in the middle of an update, keep the UPDATING dialog active
      if (display.mode() != UPDATING) {
        display.putRequest(NEWMODE, LOST);
      }
    }
  }
  network.beginReconnect = true;
  WiFi.reconnect();
}

bool MyNetwork::wifiBegin(bool silent) {
  uint8_t ls = (config.store.lastSSID == 0 || config.store.lastSSID > config.ssidsCount) ? 0 : config.store.lastSSID - 1;
  uint8_t startedls = ls;
  uint8_t errcnt = 0;
  WiFi.mode(WIFI_STA);
  struct MatchedNetwork {
    uint8_t configIndex;
    int scanIndex;
    int32_t rssi;
    uint8_t channel;
    uint8_t bssid[6];
  };
  MatchedNetwork matches[20];  // Max 20 matches (reasonable limit)
  int matchCount = 0;
  if (config.store.wifiscanbest && !silent) BOOTLOG("Scanning for best available network...");
  int n = WiFi.scanNetworks();

  if (config.store.wifiscanbest) {
    if (!silent) BOOTLOG("Scan complete: %d networks found", n);
    if (n > 0) {
      // Find all matching networks and build sorted list
      for (int i = 0; i < n; i++) {
        String scannedSSID = WiFi.SSID(i);
        if (scannedSSID.length() == 0) continue;
        for (uint8_t j = 0; j < config.ssidsCount; j++) {
          if (strcmp(scannedSSID.c_str(), config.ssids[j].ssid) == 0) {
            // Found a match - add to array if there's space
            if (matchCount < 20) {
              matches[matchCount].configIndex = j;
              matches[matchCount].scanIndex = i;
              matches[matchCount].rssi = WiFi.RSSI(i);
              matches[matchCount].channel = WiFi.channel(i);
              uint8_t* bssid = WiFi.BSSID(i);
              memcpy(matches[matchCount].bssid, bssid, 6);
              matchCount++;
            }
            break;
          }
        }
      }
      // Sort matches by RSSI (strongest first) using bubble sort
      for (int i = 0; i < matchCount - 1; i++) {
        for (int j = 0; j < matchCount - i - 1; j++) {
          if (matches[j].rssi < matches[j + 1].rssi) {
            MatchedNetwork temp = matches[j];
            matches[j] = matches[j + 1];
            matches[j + 1] = temp;
          }
        }
      }
      // Log all matches
      if (!silent && matchCount > 0) {
        BOOTLOG("Available networks from your saved list (sorted by strength):");
        for (int i = 0; i < matchCount; i++) {
          BOOTLOG("  %d. %s | MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm | Ch: %d", 
                  i+1, config.ssids[matches[i].configIndex].ssid,
                  matches[i].bssid[0], matches[i].bssid[1], matches[i].bssid[2],
                  matches[i].bssid[3], matches[i].bssid[4], matches[i].bssid[5],
                  matches[i].rssi, matches[i].channel);
        }
      }
    }
    // Try each matched network in RSSI order
    for (int attempt = 0; attempt < matchCount; attempt++) {
      uint8_t configIdx = matches[attempt].configIndex;
      if (!silent) {
        BOOTLOG("Attempt %d: connecting to %s | MAC: %02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d dBm)", 
                attempt + 1, config.ssids[configIdx].ssid,
                matches[attempt].bssid[0], matches[attempt].bssid[1], matches[attempt].bssid[2],
                matches[attempt].bssid[3], matches[attempt].bssid[4], matches[attempt].bssid[5],
                matches[attempt].rssi);
        Serial.print("##[BOOT]#\t");
        display.putRequest(BOOTSTRING, configIdx);
      }
      WiFi.begin(config.ssids[configIdx].ssid, config.ssids[configIdx].password, 
                 matches[attempt].channel, matches[attempt].bssid); // Connect to specific AP by BSSID
      errcnt = 0;
      while (WiFi.status() != WL_CONNECTED) {
        if (!silent) Serial.print(".");
        delay(500);
        if (REAL_LEDBUILTIN!=255 && !silent) digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
        errcnt++;
        if (errcnt > WIFI_ATTEMPTS) {
          if (!silent) Serial.println();
          break;  // Failed, try next match
        }
      }
      if (WiFi.status() == WL_CONNECTED) {
        WiFi.scanDelete();
        config.setLastSSID(configIdx + 1);
        return true;
      }
    }

    // All scanned matches failed, clean up
    WiFi.scanDelete();
    if (!silent) BOOTLOG("All scanned networks failed, falling back to sequential try");
    ls = startedls;
  }
  
  // Fallback: try all configured SSIDs sequentially (original behavior)
  while (true) {
    if (!silent) {
      BOOTLOG("Attempt to connect to %s", config.ssids[ls].ssid);
      Serial.print("##[BOOT]#\t");
      display.putRequest(BOOTSTRING, ls);
    }
    WiFi.begin(config.ssids[ls].ssid, config.ssids[ls].password);
    
    while (WiFi.status() != WL_CONNECTED) {
      if (!silent) Serial.print(".");
      delay(500);
      network.loopImprov();
      if (REAL_LEDBUILTIN!=255 && !silent) digitalWrite(REAL_LEDBUILTIN, !digitalRead(REAL_LEDBUILTIN));
      errcnt++;
      if (errcnt > WIFI_ATTEMPTS) {
        errcnt = 0;
        ls++;
        if (ls > config.ssidsCount - 1) ls = 0;
        if (!silent) Serial.println();
        break;
      }
    }
    if (WiFi.status() != WL_CONNECTED && ls == startedls) {
      return false; break;
    }
    if (WiFi.status() == WL_CONNECTED) {
      config.setLastSSID(ls + 1);
      return true; break;
    }
  }
  return false;
}

void MyNetwork::ehDPinit() {
  if (strlen(config.store.ehdpname) > 0) {
    ehdp.setName(config.store.ehdpname);
    #ifdef FIRMWARE_NAME
      ehdp.setFirmware(FIRMWARE_NAME);
    #elif defined(FIRMWARE)
      String fw = FIRMWARE;
      if (fw.endsWith(".bin")) fw.remove(fw.length() - 4);
      ehdp.setFirmware(fw.c_str());
    #endif
  } else {
    #ifdef FIRMWARE_NAME
      ehdp.setName(FIRMWARE_NAME);
    #endif
    #ifdef FIRMWARE
      String fw = FIRMWARE;
      if (fw.endsWith(".bin")) fw.remove(fw.length() - 4);
      ehdp.setFirmware(fw.c_str());
    #endif
  }
  ehdp.setProject("ehRadio");
  ehdp.setVersion(RADIOVERSION);
  ehdp.setUIPort(80);
  ehdp.setMaterialSymbol("0xe03e");
  if (strlen(config.store.mdnsname) > 0) ehdp.setMdns(config.store.mdnsname);
  if (ehdp.begin()) {
    BOOTLOG("ehDP listening");
  } else {
    BOOTLOG("ehDP failed to start");
  }
}

void searchWiFi(void * pvParameters) {
  if (!network.wifiBegin(true)) {
    delay(10000);
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, 0);
  } else {
    network.status = CONNECTED;
    netserver.begin(true);
    telnet.begin(true);
    network.setWifiParams();
    display.putRequest(NEWIP, 0);
  }
  vTaskDelete(NULL);
}

#define DBGAP false

void MyNetwork::begin() {
  BOOTLOG("network.begin");
  
  // Initialize Improv early if not already done, so it's always available via Serial
  if (!improv) {
    improv = new ImprovWiFi(&Serial);
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S3;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_C3;
    #else
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32;
    #endif
    char deviceUrl[64];
    strlcpy(deviceUrl, "http://{LOCAL_IPV4}/", sizeof(deviceUrl));
    improv->setDeviceInfo(chip, "ehRadio", RADIOVERSION, "ehRadio", deviceUrl);
    improv->setCustomConnectWiFi(onImprovCustomConnect);
  }

  config.initNetwork();
  ctimer.detach();
  if (config.ssidsCount == 0 || DBGAP) {
    raiseSoftAP();
    return;
  }
  if (config.getMode()!=PM_SDCARD) {
    if (!wifiBegin()) {
      raiseSoftAP();
      Serial.println("##[BOOT]#\tdone");
      return;
    }
    Serial.println(".");
    status = CONNECTED;
    setWifiParams();
  } else {
    status = SDREADY;
    xTaskCreatePinnedToCore(searchWiFi, "searchWiFi", 1024 * 4, NULL, 0, NULL, 0);
  }
  
  Serial.println("##[BOOT]#\tdone");
  ehDPinit();
  if (REAL_LEDBUILTIN!=255) digitalWrite(REAL_LEDBUILTIN, LOW);
  
  #if RTCSUPPORTED
    if (config.isRTCFound()) {
      rtc.getTime(&network.timeinfo);
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #endif
  ctimer.attach(1, ticks);
  if (network_on_connect) network_on_connect();
  pm.on_connect();
}

void MyNetwork::loopImprov() {
  if (!improv) return;
  improv->handleSerial();
  // Note: periodic IMPROV heartbeat broadcast was removed — the Improv
  // protocol state is driven by the host-side tool; unsolicited broadcasts
  // caused false provisioning prompts on some platforms.
}

static Ticker improvRebootTicker;

static void triggerImprovReboot() {
  ESP.restart();
}

static bool onImprovCustomConnect(const char* ssid, const char* password) {
  // Try to connect briefly to verify if credentials work before saving
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    network.loopImprov();
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Revert to AP if we were in AP mode, or just return false to signal error to browser
    // This will notify the user in the browser that connection failed
    return false;
  }

  // CONNECTION SUCCESSFUL - Proceed with saving logic
  if (config.addSsid(ssid, password)) {
    // Update the URL immediately before returning success to browser
    IPAddress ip = WiFi.localIP();
    char deviceUrl[64];
    snprintf(deviceUrl, sizeof(deviceUrl), "http://%d.%d.%d.%d/", ip[0], ip[1], ip[2], ip[3]);
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S3;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_C3;
    #else
    ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32;
  #endif
    if (network.improv) network.improv->setDeviceInfo(chip, "ehRadio", RADIOVERSION, "ehRadio", deviceUrl);

    improvRebootTicker.once(3, triggerImprovReboot);
    return true;
  }
  return false;
}

void MyNetwork::setWifiParams() {
  WiFi.setSleep(false);
  WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  weatherBuf=NULL;
  trueWeather = false;
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    if (weatherBuf) { free(weatherBuf); weatherBuf = nullptr; }
    weatherBuf = (char *) malloc(sizeof(char) * WEATHER_STRING_L);
    memset(weatherBuf, 0, WEATHER_STRING_L);
  #endif
  if (strlen(config.store.sntp1)>0 && strlen(config.store.sntp2)>0) {
    configTzTime(config.store.tzposix, config.store.sntp1, config.store.sntp2);
  } else if (strlen(config.store.sntp1)>0) {
    configTzTime(config.store.tzposix, config.store.sntp1);
  }
}

void MyNetwork::requestTimeSync(bool withTelnetOutput, uint8_t clientId) {
  if (withTelnetOutput) {
    if (strlen(config.store.sntp1) > 0 && strlen(config.store.sntp2) > 0)
      configTzTime(config.store.tzposix, config.store.sntp1, config.store.sntp2);
    else if (strlen(config.store.sntp1) > 0)
      configTzTime(config.store.tzposix, config.store.sntp1);
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    telnet.printf(clientId, "##SYS.DATE#: %s (%s)\r\n> ", timeStringBuff, config.store.tzposix);
    telnet.printf(clientId, "##SYS.TZNAME#: %s\r\n> ", config.store.tz_name);
    telnet.printf(clientId, "##SYS.TZPOSIX#: %s\r\n> ", config.store.tzposix);
  }
}

void rebootTime() {
  ESP.restart();
}

void MyNetwork::raiseSoftAP() {
  WiFi.mode(WIFI_AP);
  #ifdef AP_PASSWORD
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  #else
    WiFi.softAP(AP_SSID);
  #endif
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());
  Serial.println("##[BOOT]#");
  BOOTLOG("************************************************");
  BOOTLOG("Running in AP/Improv mode");
  #ifdef AP_PASSWORD
    BOOTLOG("Connect to AP %s with password %s", AP_SSID, AP_PASSWORD);
  #else
    BOOTLOG("Connect to AP %s with no password", AP_SSID);
  #endif
  BOOTLOG("and go to http://192.168.4.1/ to configure");
  BOOTLOG("Improv WiFi provisioning available via serial");
  BOOTLOG("************************************************");
  
  status = SOFT_AP;
  if (config.store.softapdelay>0)
    rtimer.once(config.store.softapdelay*60, rebootTime);
}

void MyNetwork::requestWeatherSync() {
  display.putRequest(NEWWEATHER);
}


void doSync(void * pvParameters) {
  static uint8_t tsFailCnt = 0;
  //static uint8_t wsFailCnt = 0;
  if (network.forceTimeSync) {
    network.forceTimeSync = false;
    if (getLocalTime(&network.timeinfo)) {
      tsFailCnt = 0;
      network.forceTimeSync = false;
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
      network.requestTimeSync(true);
      #if RTCSUPPORTED
        if (config.isRTCFound()) rtc.setTime(&network.timeinfo);
      #endif
    } else {
      if (tsFailCnt<4) {
        network.forceTimeSync = true;
        tsFailCnt++;
      } else {
        network.forceTimeSync = false;
        tsFailCnt=0;
      }
    }
  }
  if (network.weatherBuf && config.store.showweather && network.forceWeather) {
    // Fetch weather without interrupting display (keep showing cached data)
    network.forceWeather = false;
    network.trueWeather=getWeather(network.weatherBuf);
  }
  vTaskDelete(NULL);
}

// Helper: Download URL to temporary file using EspFileUpdater (handles chunked encoding)
bool downloadToTempFile(const char* url) {
  // Delete old temp file if exists
  if (SPIFFS.exists(TMP_PATH)) {
    SPIFFS.remove(TMP_PATH);
  }
  
  ESPFileUpdater* downloader = new ESPFileUpdater(SPIFFS);
  downloader->setUserAgent(ESPFILEUPDATER_USERAGENT);
  downloader->setMaxSize(2048);  // Weather JSON responses are small
  
  ESPFileUpdater::UpdateStatus result = downloader->checkAndUpdate(
    TMP_PATH,
    url,
    "",
    ESPFILEUPDATER_VERBOSE
  );
  
  delete downloader;
  return (result == ESPFileUpdater::UPDATED);
}

// WMO Weather Code to Description (for Open-Meteo)
const char* getWMODescription(int code) {
  switch(code) {
    case 0:  return LANG::w_clear_sky;
    case 1: case 2: case 3: return LANG::w_overcast;
    case 45: case 48: return LANG::w_foggy;
    case 51: case 53: case 55: return LANG::w_drizzle;
    case 56: case 57: return LANG::w_freezing_drizzle;
    case 61: case 63: case 65: return LANG::w_rain;
    case 66: case 67: return LANG::w_freezing_rain;
    case 71: case 73: case 75: return LANG::w_snow;
    case 77: return LANG::w_snow_grains;
    case 80: case 81: case 82: return LANG::w_rain_showers;
    case 85: case 86: return LANG::w_snow_showers;
    case 95: return LANG::w_thunderstorm;
    case 96: case 99: return LANG::w_thunderstorm_hail;
    default: return "Unknown";
  }
}

// Weather data cache (stores raw metric data from last API fetch)
namespace WeatherCache {
  bool valid = false;
  bool is_openmeteo = false;  // Track API type for icon mapping
  unsigned long fetch_time = 0;  // Timestamp when data was fetched
  float temp_c = 0;
  float feels_like_c = 0;
  int humidity = 0;
  float pressure_hpa = 0;
  float wind_speed_ms = 0;  // Always stored in m/s (meters per second) for both APIs
  int wind_deg = 0;
  char description[64] = "";
  char icon[8] = "";  // For OpenWeather
  int wmo_code = 0;    // For OpenMeteo
}

// Build weather display string from cached data (no API refetch)
bool MyNetwork::buildWeatherString() {
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    if (!weatherBuf) return false;
    
    // Check if cached data is stale (older than 2x the sync interval)
    if (WeatherCache::valid) {
      unsigned long cache_age = (millis() - WeatherCache::fetch_time) / 1000;  // Age in seconds
      unsigned long max_age = (unsigned long)config.store.weathersyncinterval * 60 * 2;  // 2x sync interval
      if (cache_age > max_age) {
        Serial.printf("Weather: Cache expired (age: %lu sec, max: %lu sec)\n", cache_age, max_age);
        WeatherCache::valid = false;
      }
    }
    
    // If no cached data or cache expired, show loading message
    if (!WeatherCache::valid) {
      snprintf(weatherBuf, WEATHER_STRING_L, "%s", LANG::weather_loading);
      display.putRequest(NEWWEATHER);
      return false;
    }
    
    Serial.println("Weather: Rebuilding display string from cached data");
    
    // Convert temperature based on user preference
    float temp_display = config.store.weathertempimp ? (WeatherCache::temp_c * 9.0 / 5.0 + 32.0) : WeatherCache::temp_c;
    float feels_display = config.store.weathertempimp ? (WeatherCache::feels_like_c * 9.0 / 5.0 + 32.0) : WeatherCache::feels_like_c;
    const char *tempUnit = config.store.weathertempimp ? "\011F" : "\011C";
    
    // Convert pressure based on user preference
    float press_display = config.store.weatherpressimp ? (WeatherCache::pressure_hpa * 0.750062) : WeatherCache::pressure_hpa;
    const char *pressUnit = config.store.weatherpressimp ? "mmHg" : "hPa";
    
    // Convert wind speed from cached m/s to user's preferred display unit
    float wind_display;
    const char *windUnit;
    
    if (strcmp(config.store.weatherwindspeed, "kmh") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 3.6;
      windUnit = "km/h";
    } else if (strcmp(config.store.weatherwindspeed, "mph") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 2.23694;
      windUnit = "mph";
    } else if (strcmp(config.store.weatherwindspeed, "kn") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 1.94384;
      windUnit = "kn";
    } else {  // ms
      wind_display = WeatherCache::wind_speed_ms;
      windUnit = "m/s";
    }
    
    int wind_dir_idx = (int)(WeatherCache::wind_deg / 22.5) % 16;
    
    // Build weather string dynamically based on enabled fields
    char *p = weatherBuf;
    size_t remaining = WEATHER_STRING_L;
    int written;
    written = snprintf(p, remaining, "%s, %.1f%s", WeatherCache::description, temp_display, tempUnit);
    if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    
    if (config.store.weatherfeels && remaining > 1) {
      written = snprintf(p, remaining, " \007 %s %.1f%s", LANG::weather_feelslike, feels_display, tempUnit);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherpressure && remaining > 1) {
      written = snprintf(p, remaining, " \007 %s %.0f %s", LANG::weather_pressure, press_display, pressUnit);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherhumidity && remaining > 1) {
      written = snprintf(p, remaining, " \007 %s %d%%", LANG::weather_humidity, WeatherCache::humidity);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherwind && remaining > 1) {
      written = snprintf(p, remaining, " \007 %s %.1f %s [%s]", LANG::weather_wind, wind_display, windUnit, LANG::wind[wind_dir_idx]);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= (size_t)written; }
    }
    
    Serial.printf("Weather: %s\n", weatherBuf);
    display.putRequest(NEWWEATHER);
    return true;
  #endif
  return false;
}

// Get weather from Open-Meteo API (free, no API key)
bool getWeather_OpenMeteo(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    Serial.println("Weather: Calling Open-Meteo v1 API for current weather...");
    
    // Build URL - always request metric (Celsius, m/s, hPa) for consistent processing
    // Wind speed: always request in m/s so we can cache and convert to any display unit
    char url[512];
    sprintf(url, "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&models=best_match&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,surface_pressure,wind_direction_10m,wind_speed_10m&forecast_days=1&wind_speed_unit=ms",
            config.store.weatherlat, config.store.weatherlon);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      Serial.println("Weather: Failed to download Open-Meteo data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = SPIFFS.open(TMP_PATH, "r");
    if (!file) {
      Serial.println("Weather: Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    SPIFFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.printf("Weather: Open-Meteo JSON parse error: %s\n", error.c_str());
      return false;
    }
    
    // Cache elevation if available and not already cached
    if (doc["elevation"].is<float>() && config.store.weatherelevation == 0) {
      float elevation = doc["elevation"];
      config.store.weatherelevation = (int16_t)elevation;
      config.saveValue(&config.store.weatherelevation, config.store.weatherelevation);
      Serial.printf("Weather: Elevation retrieved from Open-Meteo: %d meters\n", config.store.weatherelevation);
    }
    
    JsonObject current = doc["current"];
    if (current.isNull()) {
      Serial.println("Weather: No current data in Open-Meteo response");
      return false;
    }
    
    // Get raw data from API (always in Celsius from Open-Meteo)
    float temp_c = current["temperature_2m"];
    float feels_like_c = current["apparent_temperature"];
    int humidity = current["relative_humidity_2m"];
    int wmo_code = current["weather_code"];
    float pressure_hpa = current["surface_pressure"];  // hPa
    float wind_speed_ms = current["wind_speed_10m"];  // Now always in m/s
    int wind_deg = current["wind_direction_10m"];
    
    const char* description = getWMODescription(wmo_code);
    
    // Cache raw weather data for later string rebuilding
    WeatherCache::valid = true;
    WeatherCache::is_openmeteo = false;  // Now uses same conversion logic as OpenWeather
    WeatherCache::fetch_time = millis();  // Record fetch timestamp
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in consistent m/s
    WeatherCache::wind_deg = wind_deg;
    WeatherCache::wmo_code = wmo_code;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    
    #ifdef USE_NEXTION
      // For Nextion, need to compute display values
      float temp_display = config.store.weathertempimp ? (temp_c * 9.0 / 5.0 + 32.0) : temp_c;
      float press_display = config.store.weatherpressimp ? (pressure_hpa * 0.750062) : pressure_hpa;
      const char *pressUnit = config.store.weatherpressimp ? "mmHg" : "hPa";
      
      nextion.putcmdf("press_txt.txt=\"%.0f%s\"", press_display, pressUnit);
      nextion.putcmdf("hum_txt.txt=\"%d%%\"", humidity);
      nextion.putcmdf("temp_txt.txt=\"%.1f\"", temp_display);
      // WMO codes don't map 1:1 to OpenWeather icons, use generic mapping
      int iconoffset = (wmo_code == 0) ? 0 : (wmo_code <= 3) ? 1 : (wmo_code < 50) ? 2 : 
                       (wmo_code < 60) ? 4 : (wmo_code < 70) ? 5 : (wmo_code < 80) ? 7 : 4;
      nextion.putcmd("cond_img.pic", 50 + iconoffset);
      nextion.weatherVisible(1);
    #endif
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

// Helper: Get icon offset from OpenWeather icon code
int getWeatherIconOffset(const char* icon) {
  if (strstr(icon,"01")!=NULL)      return 0;
  else if (strstr(icon,"02")!=NULL) return 1;
  else if (strstr(icon,"03")!=NULL) return 2;
  else if (strstr(icon,"04")!=NULL) return 3;
  else if (strstr(icon,"09")!=NULL) return 4;
  else if (strstr(icon,"10")!=NULL) return 5;
  else if (strstr(icon,"11")!=NULL) return 6;
  else if (strstr(icon,"13")!=NULL) return 7;
  else if (strstr(icon,"50")!=NULL) return 8;
  else                             return 9;
}

// Get weather from OpenWeather API 2.5 (legacy)
bool getWeather_OpenWeather25(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    Serial.println("Weather: Calling OpenWeather API 2.5 for current weather...");
    
    // Check for API key
    if (strlen(config.store.weatherkey) == 0) {
      Serial.println("Weather: OpenWeather requires API key");
      return false;
    }
    
    // Build URL - always request metric for consistent processing
    char url[512];
    sprintf(url, "http://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&units=metric&lang=%s&appid=%s",
            config.store.weatherlat, config.store.weatherlon,
            config.store.weatherlang, config.store.weatherkey);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      Serial.println("Weather: Failed to download OpenWeather 2.5 data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = SPIFFS.open(TMP_PATH, "r");
    if (!file) {
      Serial.println("Weather: Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    SPIFFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.printf("Weather: OpenWeather 2.5 JSON parse error: %s\n", error.c_str());
      return false;
    }
    
    // Extract data (metric: Celsius, m/s, hPa)
    const char* description = doc["weather"][0]["description"];
    const char* icon = doc["weather"][0]["icon"];
    float temp_c = doc["main"]["temp"];
    float feels_like_c = doc["main"]["feels_like"];
    
    // Use grnd_level if available, otherwise sea_level pressure
    float pressure_hpa;
    if (doc["main"]["grnd_level"].is<float>()) {
      pressure_hpa = doc["main"]["grnd_level"];
    } else if (doc["main"]["pressure"].is<float>()) {
      pressure_hpa = doc["main"]["pressure"];
    } else {
      Serial.println("Weather: No pressure data in OpenWeather 2.5 response");
      return false;
    }
    
    int humidity = doc["main"]["humidity"];
    float wind_speed_ms = doc["wind"]["speed"];  // m/s from metric API
    int wind_deg = doc["wind"]["deg"];
    
    // Cache raw weather data for later string rebuilding
    WeatherCache::valid = true;
    WeatherCache::is_openmeteo = false;
    WeatherCache::fetch_time = millis();  // Record fetch timestamp
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in m/s for OpenWeather
    WeatherCache::wind_deg = wind_deg;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    strncpy(WeatherCache::icon, icon, sizeof(WeatherCache::icon) - 1);
    WeatherCache::icon[sizeof(WeatherCache::icon) - 1] = '\0';
    
    #ifdef USE_NEXTION
      // For Nextion, need to compute display values
      float temp_display = config.store.weathertempimp ? (temp_c * 9.0 / 5.0 + 32.0) : temp_c;
      float press_display = config.store.weatherpressimp ? (pressure_hpa * 0.750062) : pressure_hpa;
      const char *pressUnit = config.store.weatherpressimp ? "mmHg" : "hPa";
      
      nextion.putcmdf("press_txt.txt=\"%.0f%s\"", press_display, pressUnit);
      nextion.putcmdf("hum_txt.txt=\"%d%%\"", humidity);
      nextion.putcmdf("temp_txt.txt=\"%.1f\"", temp_display);
      int iconoffset = getWeatherIconOffset(icon);
      nextion.putcmd("cond_img.pic", 50 + iconoffset);
      nextion.weatherVisible(1);
    #endif
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

// Helper: Fetch elevation from open-elevation.com API (fallback for OW 3.0)
// Helper: Fetch and cache elevation from APIs (Open-Elevation with Open-Meteo fallback)
void fetchAndCacheElevation() {
  float lat = atof(config.store.weatherlat);
  float lon = atof(config.store.weatherlon);
  float elevation = 0.0;
  bool success = false;
  
  // Try Open-Elevation API first
  Serial.println("Weather: Getting elevation from Open-Elevation...");
  char url[256];
  sprintf(url, "http://api.open-elevation.com/api/v1/lookup?locations=%.4f,%.4f", lat, lon);
  
  if (downloadToTempFile(url)) {
    File file = SPIFFS.open(TMP_PATH, "r");
    if (file) {
      String response = file.readString();
      file.close();
      
      JsonDocument doc;
      if (deserializeJson(doc, response) == DeserializationError::Ok) {
        if (doc["results"][0]["elevation"].is<float>()) {
          elevation = doc["results"][0]["elevation"];
          success = true;
        }
      }
    }
  }
  
  // Fall back to Open-Meteo if Open-Elevation failed
  if (!success) {
    Serial.println("Weather: Getting elevation from Open-Meteo...");
    sprintf(url, "https://api.open-meteo.com/v1/elevation?latitude=%.4f&longitude=%.4f", lat, lon);
    
    if (downloadToTempFile(url)) {
      File file = SPIFFS.open(TMP_PATH, "r");
      if (file) {
        String response = file.readString();
        file.close();
        
        JsonDocument doc;
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
          if (doc["elevation"].is<float>()) {
            elevation = doc["elevation"];
            success = true;
          }
        }
      }
    }
  }
  
  // Clean up temp file
  SPIFFS.remove(TMP_PATH);
  
  // Cache elevation if successfully retrieved
  if (success && elevation > 0.0) {
    config.store.weatherelevation = (int16_t)elevation;
    config.saveValue(&config.store.weatherelevation, config.store.weatherelevation);
    Serial.printf("Weather: Caching elevation: %d meters\n", config.store.weatherelevation);
  } else {
    Serial.println("Weather: Failed to retrieve elevation from all sources");
  }
}

// Helper: Calculate ground-level pressure from sea-level pressure using elevation
float calculateGroundPressure(float seaLevelPressure, float elevationMeters) {
  // Barometric formula: P_ground = P_sea * (1 - elevation / 44330)^5.255
  if (elevationMeters == 0.0) return seaLevelPressure;
  return seaLevelPressure * pow((1.0 - elevationMeters / 44330.0), 5.255);
}

// Get weather from OpenWeather API 3.0 (current)
bool getWeather_OpenWeather30(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    Serial.println("Weather: Calling OpenWeather API 3.0 for current weather...");
    
    // Check for API key
    if (strlen(config.store.weatherkey) == 0) {
      Serial.println("Weather: OpenWeather requires API key");
      return false;
    }
    
    // Build URL - always request metric for consistent processing
    char url[512];
    sprintf(url, "http://api.openweathermap.org/data/3.0/onecall?exclude=minutely,hourly,daily&lat=%s&lon=%s&units=metric&lang=%s&appid=%s",
            config.store.weatherlat, config.store.weatherlon,
            config.store.weatherlang, config.store.weatherkey);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      Serial.println("Weather: Failed to download OpenWeather 3.0 data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = SPIFFS.open(TMP_PATH, "r");
    if (!file) {
      Serial.println("Weather: Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    SPIFFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      Serial.printf("Weather: OpenWeather 3.0 JSON parse error: %s\n", error.c_str());
      return false;
    }
    
    JsonObject current = doc["current"];
    if (current.isNull()) {
      Serial.println("Weather: No current data in OpenWeather 3.0 response");
      return false;
    }
    
    // Extract data (metric: Celsius, m/s, hPa)
    const char* description = current["weather"][0]["description"];
    const char* icon = current["weather"][0]["icon"];
    float temp_c = current["temp"];
    float feels_like_c = current["feels_like"];
    float pressure_sea_hpa = current["pressure"];  // Sea-level pressure
    int humidity = current["humidity"];
    float wind_speed_ms = current["wind_speed"];  // m/s from metric API
    int wind_deg = current["wind_deg"];
    int wind_dir_idx = (int)(wind_deg / 22.5) % 16;
    
    // Get or fetch elevation for barometric adjustment
    float elevation = 0.0;
    if (config.store.weatherelevation != 0) {
      elevation = (float)config.store.weatherelevation;
      Serial.printf("Weather: Using cached elevation: %d meters\n", config.store.weatherelevation);
    } else {
      // Fetch and cache elevation
      fetchAndCacheElevation();
      elevation = (float)config.store.weatherelevation;
    }
    
    // Calculate ground-level pressure from sea-level pressure
    float pressure_hpa = calculateGroundPressure(pressure_sea_hpa, elevation);
    Serial.printf("Weather: Adjusted pressure from %.0f hPa (sea) to %.0f hPa (ground) using %.0f m elevation\n",
                  pressure_sea_hpa, pressure_hpa, elevation);
    
    // Cache raw weather data for later string rebuilding
    WeatherCache::valid = true;
    WeatherCache::is_openmeteo = false;
    WeatherCache::fetch_time = millis();  // Record fetch timestamp
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;  // Ground-level adjusted
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in m/s for OpenWeather
    WeatherCache::wind_deg = wind_deg;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    strncpy(WeatherCache::icon, icon, sizeof(WeatherCache::icon) - 1);
    WeatherCache::icon[sizeof(WeatherCache::icon) - 1] = '\0';
    
    #ifdef USE_NEXTION
      // For Nextion, need to compute display values
      float temp_display = config.store.weathertempimp ? (temp_c * 9.0 / 5.0 + 32.0) : temp_c;
      float press_display = config.store.weatherpressimp ? (pressure_hpa * 0.750062) : pressure_hpa;
      const char *pressUnit = config.store.weatherpressimp ? "mmHg" : "hPa";
      
      nextion.putcmdf("press_txt.txt=\"%.0f%s\"", press_display, pressUnit);
      nextion.putcmdf("hum_txt.txt=\"%d%%\"", humidity);
      nextion.putcmdf("temp_txt.txt=\"%.1f\"", temp_display);
      int iconoffset = getWeatherIconOffset(icon);
      nextion.putcmd("cond_img.pic", 50 + iconoffset);
      nextion.weatherVisible(1);
    #endif
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

bool getWeather(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY || defined(USE_NEXTION)) && !defined(HIDE_WEATHER)
    // Provider dispatcher - route to appropriate weather API
    if (strcmp(config.store.weatherapi, "OW30") == 0) {
      return getWeather_OpenWeather30(wstr);
    } else if (strcmp(config.store.weatherapi, "OW25") == 0) {
      return getWeather_OpenWeather25(wstr);
    } else {  // Default: "OM1" or any other value
      return getWeather_OpenMeteo(wstr);
    }
  #endif
  return false;
}
