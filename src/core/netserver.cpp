#include "options.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPFileUpdater.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <SPIFFS.h>
#include <Update.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "battery.h"
#include "commandhandler.h"
#include "config.h"
#include "controls.h"
#include "display.h"
#include "locale.h"
#include "mqtt.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "telnet.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/widgetsconfig.h" //BitrateFormat
#if USE_OTA
  #if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    #include <NetworkUdp.h>
  #else
    #include <WiFiUdp.h>
  #endif
  #include <ArduinoOTA.h>
#endif
#ifdef USE_SD
  #include "sdmanager.h"
#endif
#ifndef MIN_MALLOC
  #define MIN_MALLOC 24112
#endif
#ifndef NSQ_SEND_DELAY
  //#define NSQ_SEND_DELAY       portMAX_DELAY
  #define NSQ_SEND_DELAY       pdMS_TO_TICKS(300)
#endif
#ifndef NS_QUEUE_TICKS
  //#define NS_QUEUE_TICKS pdMS_TO_TICKS(2)
  #define NS_QUEUE_TICKS 0
#endif

#ifdef DEBUG_V
  #define DBGVB(...) { char buf[200]; snprintf(buf, sizeof(buf), __VA_ARGS__); Serial.print("[DEBUG]\t"); Serial.println(buf); }
#else
  #define DBGVB(...)
#endif

// Global list for Radio-Browser servers to persist across searches
String rb_servers[20];
// For the search task
TaskHandle_t g_searchTaskHandle = NULL;
// For the curated playlists task
TaskHandle_t g_curatedTaskHandle = NULL;
#define FS_REQUIRED_FREE_SPACE 150 // in KB - must be minimum x1.5 of the limit_per_page in search.js (100)

NetServer netserver;

AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");

bool  shouldReboot  = false;
#ifdef MQTT_ENABLE
  Ticker mqttplaylistticker;
  volatile bool mqttplaylistblock = false;  // volatile: written from Ticker callback, read from AsyncWebServer task
  void mqttplaylistSend() {
    if (config.store.mqttenable) {
      mqttplaylistblock = true;
      mqttplaylistticker.detach();
      mqttPublishPlaylist();
      mqttplaylistblock = false;
    }
  }
#endif

char* updateError() {
  static char ret[140] = {0};
  snprintf(ret, sizeof(ret), "Update failed with error (%d)<br /> %s", (int)Update.getError(), Update.errorString());
  return ret;
}

void handleDynamicLocale(AsyncWebServerRequest *request) {
  // Dynamically serve the current locale file as /locale.json
  // Maps to /www/{locale_code}.json[.gz] based on config.store.locale_webui or WEBUI_LOCALE
  char localeFile[64];
  #ifdef UPDATEURL
    // Update capability enabled - use config.store.locale_webui (user can download locales)
    const char* localeCode = config.store.locale_webui;
  #else
    // Update capability disabled - use hardcoded WEBUI_LOCALE
    const char* localeCode = WEBUI_LOCALE;
  #endif
  // Try .gz version first (production builds use gzipped files)
  snprintf(localeFile, sizeof(localeFile), "/www/%s.json.gz", localeCode);
  if (SPIFFS.exists(localeFile)) {
    AsyncWebServerResponse *response = request->beginResponse(SPIFFS, localeFile, "application/json");
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
    return;
  }
  // Try non-gzipped version
  snprintf(localeFile, sizeof(localeFile), "/www/%s.json", localeCode);
  if (SPIFFS.exists(localeFile)) {
    request->send(SPIFFS, localeFile, "application/json");
    return;
  }
  request->send(404, "text/plain", "Locale file not found");
}

void handleSearch(AsyncWebServerRequest *request) {
  // handle search request
  if (request->hasParam("search")) {
    if (g_searchTaskHandle != NULL) {
      request->send(429, "text/plain", "Search task is already running.");
      return;
    }
    String searchQuery = request->getParam("search")->value();
    char* search_str = new (std::nothrow) char[searchQuery.length() + 1];
    if (!search_str) {
      request->send(500, "text/plain", "Failed to allocate memory for search task.");
      return;
    }
    strcpy(search_str, searchQuery.c_str());
    xTaskCreate(vTaskSearchRadioBrowser, "searchRadioBrowser", 8192, (void*)search_str, 1, &g_searchTaskHandle);
    request->send(200, "application/json", "{\"status\":\"searching\"}");
  }
}

void handleReady(AsyncWebServerRequest *request) {
  #if defined(HTTP_USER) && defined(HTTP_PASS)
    if (network.status == CONNECTED) {
      if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
        return request->requestAuthentication();
      }
    }
  #endif

  const bool networkReady =
    (network.status == CONNECTED && WiFi.status() == WL_CONNECTED) ||
    (network.status == SDREADY);
  const bool ready = netserver.isBootReady() && config.wwwFilesExist && networkReady;
  AsyncWebServerResponse *response = request->beginResponse(
    200,
    "application/json",
    ready ? "{\"ready\":true}" : "{\"ready\":false}"
  );
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}

void handleSearchPost(AsyncWebServerRequest *request) {
  // handle preview or add to playlist
  bool addtoplaylist = false;
  if (request->hasParam("addtoplaylist", true)) {
    if (request->getParam("addtoplaylist", true)->value() == "true") addtoplaylist = true;
  }
  if (!request->hasParam("url", true) || !request->hasParam("name", true)) {
    request->send(400, "text/plain", "Missing url or name");
    return;
  }
  String sUrl = request->getParam("url", true)->value();
  String sName = request->getParam("name", true)->value();
  sName.trim();
  sUrl.trim();
  if (sName.length() >= sizeof(config.station.name)) sName = sName.substring(0, sizeof(config.station.name) - 1);
  if (sUrl.length() >= sizeof(config.station.url)) sUrl = sUrl.substring(0, sizeof(config.station.url) - 1);
  player.sendCommand({PR_STOP, 0}); // Stop current playback
  
  // Check for duplicate URL in playlist (for both preview and add)
  bool found = false;
  int foundIdx = 0;
  auto normalizeUrl = [](const String& url) -> String {
      String u = url;
      u.trim();
      if (u.startsWith("http://")) u = u.substring(7);
      else if (u.startsWith("https://")) u = u.substring(8);
      u.trim();
      return u;
      };
  String normNewUrl = normalizeUrl(sUrl);
  uint16_t cs = config.playlistLength();
  for (int i = 1; i <= cs; ++i) {
    config.loadStation(i);
    String existingUrl = String(config.station.url);
    String normExistingUrl = normalizeUrl(existingUrl);
    if (normExistingUrl.equalsIgnoreCase(normNewUrl)) {
      found = true;
      foundIdx = i;
      break;
    }
    // Reset watchdog every 5 iterations to prevent timeout
    if (i % 5 == 0) esp_task_wdt_reset();
  }
  
  if (!addtoplaylist) { // This is a preview
    if (found) { // URL exists in playlist, play that station
      player.sendCommand({PR_PLAY, (uint16_t)foundIdx});
      request->send(200, "text/plain", "EXISTING");
    } else { // URL not in playlist, preview in slot 0
      config.loadStation(0); // Load into temporary station slot
      launchPlaybackTask(sUrl, sName);
      netserver.requestOnChange(GETINDEX, 0);
      request->send(200, "text/plain", "PREVIEW");
    }
  } else { // This is add to playlist
    int sOvol = 0;
    if (found) { // play the slot if it already exists
      player.sendCommand({PR_PLAY, (uint16_t)foundIdx});
      request->send(200, "text/plain", "DUPLICATE");
    } else { // add it and play it
      File playlistfile = SPIFFS.open(PLAYLIST_PATH, "a");
      if (playlistfile) {
        playlistfile.printf("%s\t%s\t%d\r\n", sName.c_str(), sUrl.c_str(), sOvol);
        playlistfile.close();
        esp_task_wdt_reset(); // Reset watchdog before heavy operations
        uint16_t newIdx = cs + 1;
        config.indexPlaylist();
        esp_task_wdt_reset(); // Reset watchdog between operations
        config.initPlaylist();
        player.sendCommand({PR_PLAY, newIdx});
        netserver.requestOnChange(PLAYLISTSAVED, 0);
        request->send(200, "text/plain", "ADDED");
      } else {
        request->send(500, "text/plain", "Failed to open playlist file");
      }
    }
  }
}

bool NetServer::begin(bool quiet) {
  if (network.status==SDREADY) return true;
  if (!quiet) Serial.print("##[BOOT]#\tnetserver.begin\t");
  nsQueue = xQueueCreate(20, sizeof(nsRequestParams_t));
  while(nsQueue==NULL) {;}

  webserver.on("/", HTTP_ANY, handleIndex);
  webserver.on("/ready", HTTP_GET, handleReady);
  webserver.on("/locale.json", HTTP_GET, handleDynamicLocale);
  webserver.on("/search", HTTP_GET, handleSearch);
  webserver.on("/search", HTTP_POST, handleSearchPost);

  // Captive portal detection — redirect probes from iOS, Android, Windows to the web UI
  auto captiveRedirect = [](AsyncWebServerRequest *request) { request->redirect("/"); };
  webserver.on("/hotspot-detect.html", HTTP_GET, captiveRedirect);          // iOS / macOS
  webserver.on("/library/test/success.html", HTTP_GET, captiveRedirect);    // iOS / macOS (older)
  webserver.on("/generate_204", HTTP_GET, captiveRedirect);                 // Android
  webserver.on("/gen_204", HTTP_GET, captiveRedirect);                      // Android (older)
  webserver.on("/ncsi.txt", HTTP_GET, captiveRedirect);                     // Windows
  webserver.on("/connecttest.txt", HTTP_GET, captiveRedirect);              // Windows

  webserver.serveStatic("/", SPIFFS, "/www/").setCacheControl("max-age=3600");
  webserver.onNotFound(handleNotFound);
  webserver.onFileUpload(handleUpload);
  #ifdef CORS_DEBUG
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Origin"), F("*"));
    DefaultHeaders::Instance().addHeader(F("Access-Control-Allow-Headers"), F("content-type"));
  #endif
  webserver.begin();
  if (strlen(config.store.mdnsname)>0)
    MDNS.begin(config.store.mdnsname);

  websocket.onEvent(onWsEvent);
  webserver.addHandler(&websocket);
  /* Ensure any connected web clients receive the current battery status immediately */
  requestOnChange(GETBATTERY, 0);
  #if USE_OTA
    if (strlen(config.store.mdnsname)>0) ArduinoOTA.setHostname(config.store.mdnsname);
    #ifdef OTA_PASS
      ArduinoOTA.setPassword(OTA_PASS);
    #endif
    ArduinoOTA
      .onStart([]() {
        display.putRequest(NEWMODE, UPDATING);
        telnet.printf("Start OTA updating %s\r\n", ArduinoOTA.getCommand() == U_FLASH?"firmware":"filesystem");
      })
      .onEnd([]() {
        telnet.printf("End OTA update, Rebooting...\r\n");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        telnet.printf("Progress OTA: %u%%\r", (progress / (total / 100)));
      })
      .onError([](ota_error_t error) {
        telnet.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) {
          telnet.printf("Auth Failed\r\n");
        } else if (error == OTA_BEGIN_ERROR) {
          telnet.printf("Begin Failed\r\n");
        } else if (error == OTA_CONNECT_ERROR) {
          telnet.printf("Connect Failed\r\n");
        } else if (error == OTA_RECEIVE_ERROR) {
          telnet.printf("Receive Failed\n");
        } else if (error == OTA_END_ERROR) {
          telnet.printf("End Failed\n");
        }
      });
    ArduinoOTA.begin();
  #endif //#if USE_OTA

  if (!quiet) Serial.println("done");
  return true;
}

size_t NetServer::chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index) {
  File requiredfile;
  bool sdpl = strcmp(netserver.chunkedPathBuffer, PLAYLIST_SD_PATH) == 0;
  if (sdpl) {
    requiredfile = config.SDPLFS()->open(netserver.chunkedPathBuffer, "r");
  } else {
    requiredfile = SPIFFS.open(netserver.chunkedPathBuffer, "r");
  }
  if (!requiredfile) return 0;
  size_t filesize = requiredfile.size();
  size_t needread = filesize - index;
  if (!needread) {
    requiredfile.close();
    return 0;
  }
  #ifdef MAX_PL_READ_BYTES
    if (maxLen>MAX_PL_READ_BYTES) maxLen=MAX_PL_READ_BYTES;
  #endif
  size_t canread = (needread > maxLen) ? maxLen : needread;
  DBGVB("[%s] seek to %d in %s and read %d bytes with maxLen=%d", __func__, index, netserver.chunkedPathBuffer, canread, maxLen);
  requiredfile.seek(index, SeekSet);
  requiredfile.read(buffer, canread);
  index += canread;
  if (requiredfile) requiredfile.close();
  return canread;
}

void NetServer::chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path) {
  memset(chunkedPathBuffer, 0, sizeof(chunkedPathBuffer));
  strlcpy(chunkedPathBuffer, path, sizeof(chunkedPathBuffer)-1);
  AsyncWebServerResponse *response;
  response = request->beginChunkedResponse(contentType, chunkedHtmlPageCallback);
  request->send(response);
}

#ifndef DSP_NOT_FLIPPED
  #define DSP_CAN_FLIPPED true
#else
  #define DSP_CAN_FLIPPED false
#endif
#if !defined(HIDE_WEATHER) && (!defined(DUMMYDISPLAY) && !defined(USE_NEXTION))
  #define SHOW_WEATHER  true
#else
  #define SHOW_WEATHER  false
#endif

#ifndef NS_QUEUE_TICKS
  #define NS_QUEUE_TICKS 0
#endif

const char *getFormat(BitrateFormat _format) {
  switch (_format) {
    case BF_MP3:  return "MP3";
    case BF_AAC:  return "AAC";
    case BF_FLAC: return "FLAC";
    case BF_WAV:  return "WAV";
    case BF_VOR:  return "OGG";
    case BF_OPU:  return "OPUS";
    default:      return "";   // no codec info
  }
}

void NetServer::processQueue() {
  if (nsQueue==NULL) return;
  nsRequestParams_t request;
  if (xQueueReceive(nsQueue, &request, NS_QUEUE_TICKS)) {
    char wsbuf[WEBSOCKET_BUFFER] = {0};
    uint8_t clientId = request.clientId;
    switch (request.type) {
      case PLAYLIST:        getPlaylist(clientId); break;
      case PLAYLISTSAVED:   {
        #ifdef USE_SD
          if (config.getMode()==PM_SDCARD) {
          //  config.indexSDPlaylist();
            config.initSDPlaylist();
          }
        #endif
        if (config.getMode()==PM_WEB) {
          config.indexPlaylist(); 
          config.initPlaylist(); 
        }
        getPlaylist(clientId); break;
      }
      case GETACTIVE: {
          bool dbgact = false, nxtn=false;
          String act = F("\"group_wifi\",");
          if (network.status == CONNECTED) {
                                                                act += F("\"group_system\",");
            if (battery_is_initialized() || dbgact)             act += F("\"group_battery\",");
                                                              #ifdef MQTT_ENABLE
                                                                act += F("\"group_mqtt\",");
                                                              #endif
            if (BRIGHTNESS_PIN != 255 || DSP_CAN_FLIPPED || DSP_MODEL == DSP_NOKIA5110 || dbgact)    act += F("\"group_display\",");
          #ifdef USE_NEXTION
                                                                act += F("\"group_nextion\",");
            if (!SHOW_WEATHER || dbgact)                        act += F("\"group_weather\",");
            nxtn=true;
          #endif
                                                              #if defined(LCD_I2C) || defined(DSP_OLED)
                                                                act += F("\"group_oled\",");
                                                              #endif
                                                              #ifndef HIDE_VU
                                                                act += F("\"group_vu\",");
                                                              #endif
            if (BRIGHTNESS_PIN != 255 || nxtn || dbgact)                act += F("\"group_brightness\",");
            if (DSP_CAN_FLIPPED || dbgact)                      act += F("\"group_tft\",");
            if (TS_MODEL != TS_MODEL_UNDEFINED || dbgact)       act += F("\"group_touch\",");
            if (DSP_MODEL == DSP_NOKIA5110)                     act += F("\"group_nokia\",");
                                                                act += F("\"group_locale\",");
            if (SHOW_WEATHER || dbgact)                         act += F("\"group_weather\",");
                                                                act += F("\"group_controls\",");
            if (ENC_BTNL != 255 || ENC2_BTNL != 255 || dbgact)  act += F("\"group_encoder\",");
            if (IR_PIN != 255 || dbgact)                        act += F("\"group_ir\",");
          }
                                                                act = act.substring(0, act.length() - 1);
          snprintf(wsbuf, sizeof(wsbuf), "{\"act\":[%s]}", act.c_str());
          break;
        }
      case GETINDEX:      {
          requestOnChange(STATION, clientId);
          requestOnChange(TITLE, clientId);
          requestOnChange(VOLUME, clientId);
          requestOnChange(EQUALIZER, clientId);
          requestOnChange(BALANCE, clientId); 
          requestOnChange(BITRATE, clientId); 
          requestOnChange(MODE, clientId); 
          requestOnChange(SDINIT, clientId);
          requestOnChange(GETPLAYERMODE, clientId);
          requestOnChange(GETBATTERY, clientId); 
          if (config.getMode()==PM_SDCARD) { requestOnChange(SDPOS, clientId); requestOnChange(SDLEN, clientId); requestOnChange(SDSHUFFLE, clientId); } 
          return; 
          break;
        }
      case GETSYSTEM:     snprintf(wsbuf, sizeof(wsbuf), "{\"sst\":%d,\"aif\":%d,\"vu\":%d,\"wifiscan\":%d,\"softr\":%d,\"ehdp\":%d,\"ehdpname\":\"%s\",\"vut\":%d,\"autoupdate\":%d,\"mdns\":\"%s\"}", 
                                  config.store.smartstart,
                                  config.store.audioinfo,
                                  config.store.vumeter,
                                  config.store.wifiscanbest,
                                  config.store.softapdelay,
                                  config.store.ehdp,
                                  config.store.ehdpname,
                                  config.vuThreshold,
                                  config.store.autoupdate,
                                  config.store.mdnsname);
                                  break;
      case GETSCREEN:     snprintf(wsbuf, sizeof(wsbuf), "{\"flip\":%d,\"inv\":%d,\"nump\":%d,\"tsf\":%d,\"tsd\":%d,\"dspon\":%d,\"br\":%d,\"con\":%d,\"scre\":%d,\"scrt\":%d,\"scrb\":%d,\"scrpe\":%d,\"scrpt\":%d,\"scrpb\":%d,\"volumepage\":%d,\"clock12\":%d}",
                                  config.store.flipscreen,
                                  config.store.invertdisplay,
                                  config.store.numplaylist,
                                  config.store.fliptouch,
                                  config.store.dbgtouch,
                                  config.store.dspon,
                                  config.store.brightness,
                                  config.store.contrast,
                                  config.store.screensaverEnabled,
                                  config.store.screensaverTimeout,
                                  config.store.screensaverBlank,
                                  config.store.screensaverPlayingEnabled,
                                  config.store.screensaverPlayingTimeout,
                                  config.store.screensaverPlayingBlank,
                                  config.store.volumepage,
                                  config.store.clock12);
                                  break;
      case GETLOCALE:     snprintf(wsbuf, sizeof(wsbuf), "{\"locale_webui\":\"%s\",\"locale_disp\":\"%s\",\"tz_name\":\"%s\",\"tzposix\":\"%s\",\"sntp1\":\"%s\",\"sntp2\":\"%s\",\"timeinterval\":%d}",
                                  config.store.locale_webui,
                                  DSP_LOCALE,
                                  config.store.tz_name,
                                  config.store.tzposix,
                                  config.store.sntp1,
                                  config.store.sntp2,
                                  config.store.timesyncinterval);
                                  break;
      case GETWEATHER:    snprintf(wsbuf, sizeof(wsbuf), "{\"wen\":%d,\"wlat\":\"%s\",\"wlon\":\"%s\",\"wtempunit\":%d,\"wpressunit\":%d,\"wspeedunit\":\"%s\",\"wen_feelslike\":%d,\"wen_humidity\":%d,\"wen_pressure\":%d,\"wen_wind\":%d,\"wapi\":\"%s\",\"welev\":\"%d\",\"wlang\":\"%s\",\"wkey\":\"%s\",\"winterval\":%d}",
                                  config.store.showweather,
                                  config.store.weatherlat,
                                  config.store.weatherlon,
                                  config.store.weathertempimp,
                                  config.store.weatherpressimp,
                                  config.store.weatherwindspeed,
                                  config.store.weatherfeels,
                                  config.store.weatherhumidity,
                                  config.store.weatherpressure,
                                  config.store.weatherwind,
                                  config.store.weatherapi,
                                  config.store.weatherelevation,
                                  config.store.weatherlang,
                                  config.store.weatherkey,
                                  config.store.weathersyncinterval);
                                  break;
      case GETMQTT:       snprintf(wsbuf, sizeof(wsbuf), "{\"mqttenable\":%d,\"mqtthost\":\"%s\",\"mqttport\":\"%d\",\"mqttuser\":\"%s\",\"mqttpass\":\"%s\",\"mqtttopic\":\"%s\"}",
                                  config.store.mqttenable,
                                  config.store.mqtthost,
                                  config.store.mqttport,
                                  config.store.mqttuser,
                                  config.store.mqttpass,
                                  config.store.mqtttopic);
                                  break;
      case GETCONTROLS:   snprintf(wsbuf, sizeof(wsbuf), "{\"vols\":%d,\"enca\":%d,\"irtl\":%d,\"skipup\":%d}",
                                  config.store.volsteps,
                                  config.store.encacc,
                                  config.store.irtlp,
                                  config.store.skipPlaylistUpDown);
                                  break;
      case DSPON:         snprintf(wsbuf, sizeof(wsbuf), "{\"dspontrue\":%d}", 1); break;
      case STATION:       requestOnChange(STATIONNAME, clientId); requestOnChange(ITEM, clientId); break;
      case STATIONNAME:   snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"nameset\", \"value\": \"%s\"}]}", config.station.name); break;
      case ITEM:          snprintf(wsbuf, sizeof(wsbuf), "{\"current\": %d}", config.lastStation()); break;
      case TITLE:         snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"meta\", \"value\": \"%s\"}]}", config.station.title); telnet.printf("##CLI.META#: %s\r\n", config.station.title); break;
      case VOLUME:        snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"volume\", \"value\": %d}]}", config.store.volume); telnet.printf("##CLI.VOL#: %d\r\n", config.store.volume); break;
      case NRSSI:         snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"rssi\", \"value\": %d}]}", rssi); /*rssi = 255;*/ break;
      case SDPOS:         snprintf(wsbuf, sizeof(wsbuf), "{\"sdpos\": %d,\"sdend\": %d,\"sdtpos\": %d,\"sdtend\": %d}",
                                  player.getFilePos(),
                                  player.getFileSize(),
                                  player.getAudioCurrentTime(),
                                  player.getAudioFileDuration()); 
                                  break;
      case SDLEN:         snprintf(wsbuf, sizeof(wsbuf), "{\"sdmin\": %d,\"sdmax\": %d}", player.sd_min, player.sd_max); break;
      case SDSHUFFLE:     snprintf(wsbuf, sizeof(wsbuf), "{\"shuffle\": %d}", config.store.sdshuffle); break;
      case BITRATE:       snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"bitrate\", \"value\": %d}, {\"id\":\"fmt\", \"value\": \"%s\"}]}", config.station.bitrate, getFormat(config.configFmt)); break;
      case GETBATTERY: {
        BatteryStatus bat = battery_get_status();
        if (!bat.valid && !battery_is_initialized()) {
          /* Still send battref even if battery not detected so UI shows calibration value */
          uint32_t battref = config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : (uint32_t)BATTERY_ADC_REF_MV;
          snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"battery\", \"value\": \"\"}, {\"id\":\"battref\", \"value\": %u}]}", battref);
        } else {
          /* formatted with labels: "volt: 4049mV, percentage: 92%, status: Idle" */
          const char *statusstr = "Idle";
          if (bat.charging) statusstr = "Charging";
          else if (bat.discharging_inferred) statusstr = "Discharging";
          char valbuf[96];
          snprintf(valbuf, sizeof(valbuf), "volt: %dmV, percentage: %d%%, status: %s", bat.voltage_mv, bat.percentage, statusstr);
          uint32_t battref = config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : (uint32_t)BATTERY_ADC_REF_MV;
          snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"battery\", \"value\": \"%s\"}, {\"id\":\"battref\", \"value\": %u}]}", valbuf, battref);
        }
        break;
      }
      case MODE:          snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"playerwrap\", \"value\": \"%s\"}]}", player.status() == PLAYING ? "playing" : "stopped"); telnet.info(); break;
      case EQUALIZER:     snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"bass\", \"value\": %d}, {\"id\": \"middle\", \"value\": %d}, {\"id\": \"treble\", \"value\": %d}]}", config.store.bass, config.store.middle, config.store.treble); break;
      case BALANCE:       snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\": \"balance\", \"value\": %d}]}", config.store.balance); break;
      case SDINIT:        snprintf(wsbuf, sizeof(wsbuf), "{\"sdinit\": %d}", SDC_CS!=255); break;
      case GETPLAYERMODE: snprintf(wsbuf, sizeof(wsbuf), "{\"playermode\": \"%s\"}", config.getMode()==PM_SDCARD?"modesd":"modeweb"); break;
      case SEARCH_DONE:   snprintf(wsbuf, sizeof(wsbuf), "{\"search_done\":true}"); break;
      case SEARCH_FAILED: snprintf(wsbuf, sizeof(wsbuf), "{\"search_failed\":true}"); break;
      case CURATED_INDEX_DONE: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_index_done\":true}"); break;
      case CURATED_PLAYLIST_DONE: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_playlist_done\":true}"); break;
      case CURATED_FAILED: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_failed\":true}"); break;
      #ifdef USE_SD
        case CHANGEMODE:    config.changeMode(config.newConfigMode); return; break;
      #endif
      default:          break;
    }
    if (strlen(wsbuf) > 0) {
      if (clientId == 0) { websocket.textAll(wsbuf); } else { websocket.text(clientId, wsbuf); }
      #ifdef MQTT_ENABLE
        if (config.store.mqttenable) {
          if (clientId == 0 && (request.type == STATION || request.type == ITEM || request.type == TITLE || request.type == MODE)) mqttPublishStatus();
          if (clientId == 0 && request.type == VOLUME) mqttPublishVolume();
        }
      #endif
    }
  }
}

void NetServer::loop() {
  if (network.status==SDREADY) return;
  if (shouldReboot) {
    Serial.println("Rebooting...");
    delay(100);
    ESP.restart();
  }
  websocket.cleanupClients();
  switch (importRequest) {
    case IMWIFI:  config.importWifi(); importRequest = IMDONE; break;
    default:      break;
  }
  processQueue();
  #ifdef RADIO_BROWSER_SEND_CLICKS
    processRadioBrowserClick();
  #endif
  #if USE_OTA
    ArduinoOTA.handle();
  #endif
}

void NetServer::irToWs(const char* protocol, uint64_t irvalue) {
  #if IR_PIN!=255
    char buf[BUFLEN] = { 0 };
    snprintf(buf, sizeof(buf), "{\"ircode\": %llu, \"protocol\": \"%s\"}", irvalue, protocol);
    websocket.textAll(buf);
  #endif
}
void NetServer::irValsToWs() {
  #if IR_PIN!=255
    if (!irRecordEnable) return;
    char buf[BUFLEN] = { 0 };
    snprintf(buf, sizeof(buf), "{\"irvals\": [%llu, %llu, %llu]}", config.ircodes.irVals[config.irindex][0], config.ircodes.irVals[config.irindex][1], config.ircodes.irVals[config.irindex][2]);
    websocket.textAll(buf);
  #endif
}

void NetServer::onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    /*
     * Do NOT write to data[len] — AsyncWebServer does not guarantee an extra
     * NUL byte in the provided buffer. Copy into a local, NUL-terminated
     * stack buffer and parse that instead to avoid heap corruption.
     */
    char payload[BUFLEN*2];
    size_t payloadLen = (len < sizeof(payload) - 1) ? len : (sizeof(payload) - 1);
    memcpy(payload, data, payloadLen);
    payload[payloadLen] = '\0';

    char command[65], val[65];
    if (config.parseWsCommand(payload, command, val, 65)) {
      if (cmd.exec(command, val, clientId)) {
        return;
      }
    }
  }
}

void NetServer::getPlaylist(uint8_t clientId) {
  char buf[BUFLEN*2] = {0};  // Increased buffer for IPv6 or longer paths
  snprintf(buf, sizeof(buf), "{\"file\": \"http://%s%s\"}", WiFi.localIP().toString().c_str(), PLAYLIST_PATH);
  if (clientId == 0) { websocket.textAll(buf); } else { websocket.text(clientId, buf); }
}

int NetServer::_readPlaylistLine(File &file, char * line, size_t size) {
  int bytesRead = file.readBytesUntil('\n', line, size);
  if (bytesRead>0) {
    line[bytesRead] = 0;
    if (line[bytesRead-1]=='\r') line[bytesRead-1]=0;
  }
  return bytesRead;
}

void NetServer::requestOnChange(requestType_e request, uint8_t clientId) {
  if (nsQueue==NULL) return;
  nsRequestParams_t nsrequest;
  nsrequest.type = request;
  nsrequest.clientId = clientId;
  xQueueSend(nsQueue, &nsrequest, NSQ_SEND_DELAY);
}

void NetServer::resetQueue() {
  if (nsQueue!=NULL) xQueueReset(nsQueue);
}

void NetServer::triggerMqttPlaylistSync() {
  #ifdef MQTT_ENABLE
    if (config.store.mqttenable) mqttplaylistticker.attach(5, mqttplaylistSend);
  #endif
}

int freeSpace;
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (request->url()=="/upload") {
    if (!index) {
      if (filename!="tempwifi.csv") {
        if (SPIFFS.exists(PLAYLIST_PATH)) SPIFFS.remove(PLAYLIST_PATH);
        if (SPIFFS.exists(INDEX_PATH)) SPIFFS.remove(INDEX_PATH);
        if (SPIFFS.exists(PLAYLIST_SD_PATH)) SPIFFS.remove(PLAYLIST_SD_PATH);
        if (SPIFFS.exists(INDEX_SD_PATH)) SPIFFS.remove(INDEX_SD_PATH);
      }
      freeSpace = (float)SPIFFS.totalBytes()/100*68-SPIFFS.usedBytes();
      request->_tempFile = SPIFFS.open(TMP_PATH , "w");
    }
    if (len) {
      if (freeSpace>index+len) {
        request->_tempFile.write(data, len);
      }
    }
    if (final) {
      request->_tempFile.close();
    }
  } else if (request->url()=="/update") {
    if (!index) {
      int target = (request->getParam("updatetarget", true)->value() == "spiffs") ? U_SPIFFS : U_FLASH;
      Serial.printf("Update Start: %s\n", filename.c_str());
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, UPDATING);
      if (!Update.begin(UPDATE_SIZE_UNKNOWN, target)) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (!Update.hasError()) {
      if (Update.write(data, len) != len) {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
    if (final) {
      if (Update.end(true)) {
        Serial.printf("Update Success: %uB\n", index + len);
      } else {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
  } else { // "/webboard"
    DBGVB("File: %s, size:%u bytes, index: %u, final: %s\n", filename.c_str(), len, index, final?"true":"false");
    if (!index) {
      String spath = "/www/";
      if (filename=="playlist.csv" || filename=="wifi.csv") spath = "/data/";
      request->_tempFile = SPIFFS.open(spath + filename , "w");
    }
    if (len) {
      request->_tempFile.write(data, len);
    }
    if (final) {
      request->_tempFile.close();
      if (filename=="playlist.csv") {
        config.indexPlaylist();
        netserver.requestOnChange(PLAYLISTSAVED, 0);
      }
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
        if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        /* Send current battery status to the newly connected client immediately */
        netserver.requestOnChange(GETBATTERY, client->id());
        break;
    case WS_EVT_DISCONNECT: if (config.store.audioinfo) Serial.printf("[WEBSOCKET] client #%u disconnected\n", client->id()); break;
    case WS_EVT_DATA: netserver.onWsMessage(arg, data, len, client->id()); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Helper to select and randomize radio-browser servers
void selectRadioBrowserServer() {
  size_t arr_size = sizeof(rb_servers) / sizeof(rb_servers[0]);
  for (size_t i = 0; i < arr_size; ++i) rb_servers[i] = "";
  File serversFile = SPIFFS.open("/www/rb_srvrs.json", "r");
  if (!serversFile) {
    Serial.println("[Search] [Error] Failed to open /www/rb_srvrs.json.");
    goto useHostname;
  } else {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, serversFile);
    serversFile.close();
    if (error) {
      Serial.print(F("[Search] [Error] deserializeJson() failed: "));
      Serial.println(error.c_str());
      goto useHostname; // get out of the else
    }
    JsonArray servers = doc.as<JsonArray>();
    if (servers.isNull() || servers.size() == 0) {
      Serial.println("[Search] [Error] JSON is not a valid or is an empty array.");
      goto useHostname; //get out of the else
    }
    // Collect unique IPv4 server names
    size_t count = 0;
    for (JsonObject server_obj : servers) {
      const char* srvr_name = server_obj["name"];
      bool duplicate = false;
      for (size_t j = 0; j < count; ++j) {
        if (rb_servers[j] == srvr_name) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate && count < arr_size) {
        rb_servers[count++] = srvr_name;
      }
    }
    // Shuffle (Fisher-Yates)
    if (count > 1) {
      for (size_t i = count - 1; i > 0; --i) {
        size_t j = random(i + 1);
        String temp = rb_servers[i];
        rb_servers[i] = rb_servers[j];
        rb_servers[j] = temp;
      }
    }

    // Add fallback as last entry after the shuffled servers
    if (count < arr_size) rb_servers[count] = RADIO_BROWSER_SERVER;
  }
  return;
useHostname:
  // Use hostname instead of IP to ensure proper Host header and HTTPS support
  Serial.printf("[Search] Using fallback: %s.\n", RADIO_BROWSER_SERVER);
  rb_servers[0] = RADIO_BROWSER_SERVER;
}

void vTaskSearchRadioBrowser(void *pvParameters) {
  char* search_str = (char*)pvParameters;
  Serial.printf("[Search] Starting Radio Browser search. Search: %s\n", search_str);
  SPIFFS.remove("/www/searchresults.json");
  // Check SPIFFS free space
  size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    Serial.printf("[Search] [Error] Not enough free SPIFFS space: %u bytes. Aborting.\n", freeSpace);
    netserver.requestOnChange(SEARCH_FAILED, 0);
    delete[] search_str;
    g_searchTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  // Count non-empty servers from our global persistent list
  size_t arr_size = sizeof(rb_servers) / sizeof(rb_servers[0]);
  int server_count = 0;
  for (size_t i = 0; i < arr_size; ++i) {
    if (rb_servers[i].length() > 0) server_count++;
  }
  // If the list is empty, it's the first run or all servers failed previously. Let's (re)populate it.
  if (server_count == 0) {
    Serial.println("[Search] Server list is empty, repopulating from file.");
    selectRadioBrowserServer();
    // Recount after filling
    server_count = 0;
    for (size_t i = 0; i < arr_size; ++i) {
      if (rb_servers[i].length() > 0) server_count++;
    }
  }
  // If still no servers, then the API source is likely down or unreachable.
  if (server_count == 0) {
    Serial.println("[Search] [Error] No servers available after attempting to select.");
    netserver.requestOnChange(SEARCH_FAILED, 0);
    delete[] search_str;
    g_searchTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  ESPFileUpdater searchResultsFetch(SPIFFS);
  searchResultsFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  searchResultsFetch.setBuffer(SEARCHRESULTS_BUFFER);
  searchResultsFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/searchresults.json";
  bool success = false;
  bool server_retried = false;
  bool json_valid = false;
  for (size_t i = 0; i < arr_size; ++i) {
    if (rb_servers[i].length() == 0) continue;
    String server = rb_servers[i];
    // Compose the URL using the full search string
    String url = "https://" + server + "/json/stations/search?" + String(search_str);
    Serial.printf("[Search] Attempting to download from: %s\n", url.c_str());
    auto status = searchResultsFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      Serial.printf("[Search] Successfully downloaded from %s\n", server.c_str());
      // Check if the downloaded file ends with ']' (an incomplete .json will not)
      File jsonFile = SPIFFS.open(localPath, "r");
      if (jsonFile) {
        int fileSize = jsonFile.size();
        char lastChar = 0;
        if (fileSize > 0) {
          for (int pos = fileSize - 1; pos >= 0; --pos) {
            jsonFile.seek(pos, SeekSet);
            char c = jsonFile.read();
            if (!isspace((unsigned char)c)) {
              lastChar = c;
              break;
            }
          }
        }
        jsonFile.close();
        if (lastChar != ']') {
          if (server_retried == true) {
            Serial.printf("[Search] [Warning] JSON validation failed. Not retrying.\n");
            server_retried = false;
            SPIFFS.remove(localPath); // Clean up bad file
          } else {
            Serial.printf("[Search] [Warning] JSON validation failed. Retrying same server.\n");
            server_retried = true;
            --i;
            SPIFFS.remove(localPath); // Clean up bad file
          }
          continue;
        } else {
          json_valid = true;
        }
      } else {
        if (server_retried == true) {
          Serial.println("[Search] [Error] Could not open searchresults.json for validation. Not retrying.\n");
          server_retried = false;
        } else {
          Serial.println("[Search] [Error] Could not open searchresults.json for validation. Retrying same server.\n");
          server_retried = true;
          --i;
        }
        continue;
      }
      if (json_valid) {
        // Write /www/search.txt with the actual search string (single line)
        File file = SPIFFS.open("/www/search.txt", "w");
        if (file) {
          file.printf("%s\n", search_str);
          file.close();
        } else {
          Serial.println("[Search] [Error] Failed to open search.txt for writing.");
        }
        success = true;
        break;
      } else {
        Serial.printf("[Search] [Error] Invalid JSON from %s. Removing from list.\n", server.c_str());
        rb_servers[i] = "";
        server_retried = false;
      }
    } else {
      Serial.printf("[Search] [Error] Failed to download from %s. Removing from persistent list.\n", server.c_str());
      rb_servers[i] = "";
      server_retried = false;
    }
  }
  if (success) {
    netserver.requestOnChange(SEARCH_DONE, 0);
  } else {
    Serial.println("[Search] [Error] Failed to download from all available servers.");
    SPIFFS.remove(localPath); // Clean up any incomplete file
    netserver.requestOnChange(SEARCH_FAILED, 0);
  }
  delete[] search_str;
  search_str = nullptr;
  g_searchTaskHandle = NULL;
  vTaskDelete(NULL);
}

void vTaskFetchCuratedIndex(void *pvParameters) {
  Serial.println("[Curated] Starting curated index fetch");
  SPIFFS.remove("/www/curated.json");
  
  // Check SPIFFS free space
  size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    Serial.printf("[Curated] [Error] Not enough free SPIFFS space: %u bytes. Aborting.\n", freeSpace);
    netserver.requestOnChange(CURATED_FAILED, 0);
    g_curatedTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  
  ESPFileUpdater curatedFetch(SPIFFS);
  curatedFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  curatedFetch.setBuffer(SEARCHRESULTS_BUFFER);
  curatedFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/curated.json";
  
  #ifdef CURATED_LISTS_URL
    String url = String(CURATED_LISTS_URL) + String(CURATED_LISTS_INDEX);
    Serial.printf("[Curated] Attempting to download index from: %s\n", url.c_str());
    
    auto status = curatedFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      Serial.println("[Curated] Successfully downloaded curated index");
      netserver.requestOnChange(CURATED_INDEX_DONE, 0);
    } else {
      Serial.println("[Curated] [Error] Failed to download curated index");
      SPIFFS.remove(localPath);
      netserver.requestOnChange(CURATED_FAILED, 0);
    }
  #else
    Serial.println("[Curated] [Error] CURATED_LISTS_URL not defined");
    netserver.requestOnChange(CURATED_FAILED, 0);
  #endif
  
  g_curatedTaskHandle = NULL;
  vTaskDelete(NULL);
}

void vTaskFetchCuratedPlaylist(void *pvParameters) {
  char* filename = (char*)pvParameters;
  Serial.printf("[Curated] Starting playlist fetch: %s\n", filename);
  SPIFFS.remove("/www/pl_import.json");
  
  // Check SPIFFS free space
  size_t freeSpace = SPIFFS.totalBytes() - SPIFFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    Serial.printf("[Curated] [Error] Not enough free SPIFFS space: %u bytes. Aborting.\n", freeSpace);
    netserver.requestOnChange(CURATED_FAILED, 0);
    delete[] filename;
    g_curatedTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  
  ESPFileUpdater playlistFetch(SPIFFS);
  playlistFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  playlistFetch.setBuffer(SEARCHRESULTS_BUFFER);
  playlistFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/pl_import.json";
  
  #ifdef CURATED_LISTS_URL
    String url = String(CURATED_LISTS_URL) + String(filename);
    Serial.printf("[Curated] Attempting to download playlist from: %s\n", url.c_str());
    
    auto status = playlistFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      Serial.printf("[Curated] Successfully downloaded playlist: %s\n", filename);
      netserver.requestOnChange(CURATED_PLAYLIST_DONE, 0);
    } else {
      Serial.printf("[Curated] [Error] Failed to download playlist: %s\n", filename);
      SPIFFS.remove(localPath);
      netserver.requestOnChange(CURATED_FAILED, 0);
    }
  #else
    Serial.println("[Curated] [Error] CURATED_LISTS_URL not defined");
    netserver.requestOnChange(CURATED_FAILED, 0);
  #endif
  
  delete[] filename;
  g_curatedTaskHandle = NULL;
  vTaskDelete(NULL);
}

void launchPlaybackTask(const String& url, const String& name) {
  if (name.length() > 0 && name.length() < sizeof(config.station.name)) {
    strlcpy(config.station.name, name.c_str(), sizeof(config.station.name));
  } else {
    strlcpy(config.station.name, "Playing", sizeof(config.station.name));
  }
  player.sendCommand({PR_STOP, 0}); // Stop any current playback first
  display.putRequest(NEWSTATION, 0);
  Serial.println("[netserver] Creating a dedicated task for playback.");
  // Use a lambda to capture the URL and pass it to the task
  String* url_copy = new String(url);
  if (url_copy) {
    // Use a larger stack for HTTPS, as it requires more memory for SSL/TLS.
    UBaseType_t stackSize = url.startsWith("https://") ? 8192 : 4096;
    xTaskCreate(
        [](void* pvParameters) {
          String* urlToPlay = (String*)pvParameters;
          vTaskDelay(pdMS_TO_TICKS(100)); // A small delay can help the network stack release resources
          Serial.printf("[PlaybackTask] Starting playback for URL: %s. Free heap: %u\n", urlToPlay->c_str(), ESP.getFreeHeap());
          player.playUrl(urlToPlay->c_str());
          delete urlToPlay; // Free the string
          vTaskDelete(NULL);
        },
        "playbackTask",
        stackSize,
        (void*)url_copy,
        1,
        NULL
   );
  } else {
    Serial.println("[netserver] ERROR: Failed to allocate memory for playback task URL.");
  }
}

#ifdef RADIO_BROWSER_SEND_CLICKS
  // Global state for click tracking
  static unsigned long clickDelayStart = 0;
  static bool clickDelayActive = false;
  static char pendingClickUrl[256] = {0};
  // Helper: Make HTTPS request and extract a specific JSON key's value
  // Returns extracted value or empty string on failure
  String streamJsonExtract(const String& url, const char* key) {
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(10000);  // 10 second timeout for server response
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", ESPFILEUPDATER_USERAGENT);
    int httpCode = http.GET();
    if (httpCode != HTTP_CODE_OK) {
      Serial.printf("[RB Click] HTTP error %d\n", httpCode);
      http.end();
      return "";
    }
    WiFiClient* stream = http.getStreamPtr();
    String keyPattern = String("\"") + key + "\"";
    String buffer;
    String value;
    bool inValue = false;
    bool foundKey = false;
    bool isStringValue = false;
    unsigned long loopStart = millis();
    const unsigned long loopTimeout = 15000; // 15 second max loop time
    while (stream->connected() || stream->available()) {
      // Check for loop timeout
      if (millis() - loopStart > loopTimeout) {
        Serial.println("[RB Click] Stream parsing timeout");
        http.end();
        return "";
      }
      if (!stream->available()) {
        delay(1);
        continue;
      }
      char c = stream->read();
      buffer += c;
      if (buffer.length() > 512) buffer = buffer.substring(buffer.length() - 256); // Keep buffer manageable
      if (!foundKey && c == ']') {
        http.end();
        return ""; // End of array without finding the key (empty array or key not present)
      }
      // Look for our key
      if (!foundKey && buffer.indexOf(keyPattern) >= 0) {
        foundKey = true;
        buffer.clear();
        continue;
      }
      // Once key is found, extract the value
      if (foundKey && !inValue) {
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == ':') {
          continue;
        }
        inValue = true;
        if (c == '"') {
          // String value (quoted)
          isStringValue = true;
          value.clear();
        } else {
          // Unquoted value (boolean, number, etc.)
          isStringValue = false;
          value = c;
        }
      } else if (foundKey && inValue) {
        if (isStringValue) {
          // Handle quoted string
          if (c == '"') {
            // Found closing quote - we're done
            http.end();
            return value;
          } else if (c == '\\') {
            // Handle escaped characters - read next char
            if (stream->available()) {
              value += (char)stream->read();
            }
          } else {
            value += c;
          }
        } else {
          // Handle unquoted value - stop at comma, closing brace, or whitespace
          if (c == ',' || c == '}' || c == ']' || c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            http.end();
            return value;
          } else {
            value += c;
          }
        }
      }
    }
    http.end();
    return value; // Return what we have, even if incomplete
  }
#endif //#ifdef RADIO_BROWSER_SEND_CLICKS

void radioBrowserSendClick(const char* stationUrl) {
  #ifdef RADIO_BROWSER_SEND_CLICKS
    // If a new request comes in, cancel the pending one and start fresh
    if (clickDelayActive) Serial.println("[RB Click] New station - canceling pending click");
    // Store the URL and start the delay timer
    strlcpy(pendingClickUrl, stationUrl, sizeof(pendingClickUrl));
    clickDelayStart = millis();
    clickDelayActive = true;
    Serial.printf("[RB Click] Starting %dms delay for: %s\n", RADIO_BROWSER_SEND_CLICK_DELAY, stationUrl);
  #endif //#ifdef RADIO_BROWSER_SEND_CLICKS
}

#ifdef RADIO_BROWSER_SEND_CLICKS
  // Background task to register the click without blocking
  void vTaskRadioBrowserClick(void* pvParameters) {
    char* stationUrl = (char*)pvParameters;
    // Step 1: Get station UUID from URL
    String lookupUrl = String("https://") + RADIO_BROWSER_SERVER + "/json/stations/byurl?url=" + String(stationUrl);
    Serial.printf("[RB Click] Looking up UUID: %s\n", lookupUrl.c_str());
    String stationUuid = streamJsonExtract(lookupUrl, "stationuuid");
    if (stationUuid.length() == 0) {
      Serial.println("[RB Click] Station not found in Radio-Browser database (empty response or lookup failed)");
      delete[] stationUrl;
      vTaskDelete(NULL);
      return;
    }
    // Step 2: Send the click
    String clickUrl = String("https://") + RADIO_BROWSER_SERVER + "/json/url/" + stationUuid;
    String okStatus = streamJsonExtract(clickUrl, "ok");
    if (okStatus == "true") {
      Serial.println("[RB Click] Click registered successfully");
    } else {
      Serial.println("[RB Click] Click not confirmed");
    }
    delete[] stationUrl;
    vTaskDelete(NULL);
  }
#endif
  
void processRadioBrowserClick() {
  #ifdef RADIO_BROWSER_SEND_CLICKS
    if (!clickDelayActive) return;
    // Check if delay has elapsed
    if (millis() - clickDelayStart < RADIO_BROWSER_SEND_CLICK_DELAY) {
      return; // Still waiting
    }
    clickDelayActive = false;
    // Copy URL to pass to task (task will delete it)
    char* urlCopy = new char[strlen(pendingClickUrl) + 1];
    if (urlCopy == nullptr) {
      Serial.println("[RB Click] Failed to allocate memory for task");
      return;
    }
    strcpy(urlCopy, pendingClickUrl);
    // Spawn the background task (allow multiple concurrent tasks for different stations)
    xTaskCreate(
      vTaskRadioBrowserClick,
      "rbClickTask",
      8192,  // Stack size - HTTPS needs more memory
      (void*)urlCopy,
      1,     // Priority
      NULL   // No handle tracking - task cleans up itself
    );
  #endif // RADIO_BROWSER_SEND_CLICKS
}

void checkForOnlineUpdate() {
  #ifdef UPDATEURL
    const char* versionUrl = CHECKUPDATEURL;
    WiFiClientSecure client;
    client.setInsecure(); // skip server cert validation
    HTTPClient http;
    http.begin(client, versionUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", ESPFILEUPDATER_USERAGENT);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      WiFiClient* stream = http.getStreamPtr();
      String line;
      String remoteVer;
      while (stream->connected() || stream->available()) {
        if (stream->available()) {
          char c = stream->read();
          if (c == '\n') {
            if (line.startsWith(VERSIONSTRING)) {
              int q1 = line.indexOf('"');
              int q2 = line.indexOf('"', q1 + 1);
              if (q1 > 0 && q2 > q1) {
                remoteVer = line.substring(q1 + 1, q2);
                break;
              }
            }
            line.clear();
          } else {
            line += c;
          }
        }
      }
      http.end();
      if (remoteVer.length() == 0) {
        websocket.textAll("{\"onlineupdateerror\": \"Remote RADIOVERSION not found\"}");
        return;
      }
      char msgBuf[BUFLEN*2];
      if (remoteVer != String(RADIOVERSION)) {
        snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateavailable\":true,\"remoteVersion\":\"%s\"}", remoteVer.c_str());
      } else {
        snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateavailable\":false,\"remoteVersion\":\"%s\"}", remoteVer.c_str());
      }
      websocket.textAll(msgBuf);
    } else {
      websocket.textAll(String("{\"onlineupdateerror\": \"HTTP code ") + httpCode + "\"}");
      http.end();
    }
  #endif //#ifdef UPDATEURL
}

void startOnlineUpdate() {
  #ifdef UPDATEURL
    String updateUrl = String(UPDATEURL) + String(FIRMWARE);
    Serial.printf("[Online Update] Online Update download URL: %s\n", updateUrl.c_str());
    WiFiClientSecure client;
    client.setInsecure(); // skip server cert validation
    HTTPClient http;
    http.begin(client, updateUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", ESPFILEUPDATER_USERAGENT);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      Serial.printf("[Online Update] Content-Length: %d\n", contentLength);
      if (contentLength > 0) {
        bool canBegin = Update.begin(contentLength);
        if (canBegin) {
          player.sendCommand({PR_STOP, 0});
          display.putRequest(NEWMODE, UPDATING);
          WiFiClient* stream = http.getStreamPtr();
          size_t written = 0;
          const size_t bufSize = 512;
          uint8_t buf[bufSize];
          unsigned long lastProgressTime = millis();
          while (written < contentLength) {
            int len = stream->read(buf, bufSize);
            if (len <= 0) {
              if (!stream->connected()) break;
              vTaskDelay(pdMS_TO_TICKS(10));
              continue;
            }
            size_t w = Update.write(buf, len);
            written += w;
            int percent = (written * 100) / contentLength;
            unsigned long now = millis();
            if (percent == 100 || now - lastProgressTime >= 1000) {
              lastProgressTime = now;
              char progMsg[64];
              snprintf(progMsg, sizeof(progMsg), "{\"onlineupdateprogress\":%d}", percent);
              websocket.textAll(progMsg);
              display.updateProgress(LANG::updFirmware, (float)written / (float)contentLength);
            }
          }
          if (Update.end(true)) { // end(true) will finish and commit the update
            Serial.println("[Online Update] Update successful, rebooting...");
            config.deleteMainwwwFile();
            websocket.textAll("{\"onlineupdatestatus\": \"Update successful, rebooting...\"}");
            delay(1000);
            ESP.restart();
          } else {
            websocket.textAll(String("{\"onlineupdateerror\": \"Update failed on end(): ") + String(Update.errorString()) + "\"}");
          }
        } else {
         websocket.textAll("{\"onlineupdateerror\": \"Cannot begin update (reboot then try again)\"}");
        }
      } else {
        websocket.textAll("{\"onlineupdateerror\": \"Invalid firmware size\"}");
      }
    } else {
      websocket.textAll("{\"onlineupdateerror\": \"Failed to download firmware\"}");
    }
    http.end();
  #endif //#ifdef UPDATEURL
}

void handleNotFound(AsyncWebServerRequest * request) {
  #if defined(HTTP_USER) && defined(HTTP_PASS)
    if (network.status == CONNECTED)
      if (request->url() == "/logout") {
        request->send(401);
        return;
      }
      if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
        return request->requestAuthentication();
      }
  #endif
  if (request->url()=="/emergency") { request->send(200, "text/html", emergency_form); return; }
  if (request->method() == HTTP_POST && request->url()=="/webboard" && !config.wwwFilesExist) { request->redirect("/"); ESP.restart(); return; }
  if (request->method() == HTTP_GET && request->url() == "/search") { handleSearch(request); return; }
  if (request->method() == HTTP_POST && request->url() == "/search") { handleSearchPost(request); return; }

  #ifdef UPDATEURL
    if (request->method() == HTTP_GET && request->url() == "/onlineupdatecheck") {
      xTaskCreate([](void*) { checkForOnlineUpdate(); vTaskDelete(NULL); }, "checkForOnlineUpdateTask", 8096, nullptr, 1, nullptr);
      request->send(200, "text/plain", "Update check started"); return;
    }
    if (request->method() == HTTP_GET && request->url() == "/onlineupdatestart") {
      xTaskCreate([](void*) { startOnlineUpdate(); vTaskDelete(NULL); }, "startOnlineUpdateTask", 16384, nullptr, 3, nullptr);
      request->send(200, "text/plain", "Update started"); return;
    }
  #endif

  if (request->method() == HTTP_GET) {
    DBGVB("[%s] client ip=%s request of %s", __func__, request->client()->remoteIP().toString().c_str(), request->url().c_str());
    if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || 
        strcmp(request->url().c_str(), SSIDS_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_PATH) == 0 || 
        strcmp(request->url().c_str(), TMP_PATH) == 0 || 
        strcmp(request->url().c_str(), PLAYLIST_SD_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_SD_PATH) == 0) {
          #ifdef MQTT_ENABLE
            if (config.store.mqttenable) {
              if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0) while (mqttplaylistblock) vTaskDelay(5);
            }
          #endif
      if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 && config.getMode()==PM_SDCARD) {
        netserver.chunkedHtmlPage("application/octet-stream", request, PLAYLIST_SD_PATH);
      } else {
        netserver.chunkedHtmlPage("application/octet-stream", request, request->url().c_str());
      }
      return;
    }// if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 || 
  }// if (request->method() == HTTP_GET)
  
  if (request->method() == HTTP_POST) {
    if (request->url()=="/webboard") { request->redirect("/"); return; } // <--post files from /data/www
    if (request->url()=="/upload") { // <--upload playlist.csv or wifi.csv
      if (request->hasParam("wifile", true, true)) {
        netserver.importRequest = IMWIFI;
        request->send(200);
      } else {
        request->send(404);
      }
      return;
    }
    if (request->url()=="/update") { // <--upload firmware
      shouldReboot = !Update.hasError();
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", shouldReboot ? "OK" : updateError());
      response->addHeader("Connection", "close");
      request->send(response);
      return;
    }
  }// if (request->method() == HTTP_POST)
  
  if (request->url() == "/favicon.ico") {
    request->send(200, "image/x-icon", "data:,");
    return;
  }
  if (request->url() == "/variables.js") {
    char varjsbuf[BUFLEN*2];
    char escapedRadioVersion[BUFLEN];
    config.escapeQuotes(RADIOVERSION, escapedRadioVersion, sizeof(escapedRadioVersion));
    char escapedGithubUrl[BUFLEN];
    config.escapeQuotes(GITHUBURL, escapedGithubUrl, sizeof(escapedGithubUrl));
    snprintf(varjsbuf, sizeof(varjsbuf),
      "var radioVersion='%s';\n"
      "var htmlLocale='%s';\n"
      "var uiLocale='%s';\n"
      "var formAction='%s';\n"
      "var playMode='%s';\n"
      "var onlineUpdCapable=%s;\n"
      "var newVerAvailable=%s;\n"
      "var updateUrl='%s';\n",
      escapedRadioVersion,
      HARDCODED_WEBUI_LOCALE,
      config.store.locale_webui,
      (network.status == CONNECTED && config.wwwFilesExist) ? "webboard" : "",
      (network.status == CONNECTED) ? "player" : "ap",
      #ifdef UPDATEURL
        "true",
      #else
        "false",
      #endif
      (netserver.newVersionAvailable) ? "true" : "false",
      escapedGithubUrl
   );
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", varjsbuf);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
    return;
  }
  if (request->url() == "/curated_variables.js") {
    char varjsbuf[BUFLEN];
    #ifdef CURATED_LISTS
      char escapedName[BUFLEN];
      config.escapeQuotes(CURATED_LISTS, escapedName, sizeof(escapedName));
      char escapedLink[BUFLEN];
      config.escapeQuotes(CURATED_LISTS_LINK, escapedLink, sizeof(escapedLink));
      snprintf(varjsbuf, sizeof(varjsbuf),
        "var curatedLists=true;\n"
        "var curatedName=\"%s\";\n"
        "var curatedLink=\"%s\";\n",
        escapedName,
        escapedLink
      );
    #else
      snprintf(varjsbuf, sizeof(varjsbuf),
        "var curatedLists=false;\n"
        "var curatedName=\"\";\n"
        "var curatedLink=\"\";\n"
      );
    #endif
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", varjsbuf);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
    return;
  }
  if (strcmp(request->url().c_str(), "/settings.html") == 0 || strcmp(request->url().c_str(), "/update.html") == 0 || strcmp(request->url().c_str(), "/ir.html") == 0) {
    request->send(200, "text/html", index_html);
    return;
  }
  if (request->method() == HTTP_GET && request->url() == "/webboard") {
    request->send(200, "text/html", emptyfs_html);
    return;
  }
  Serial.print("Not Found: ");
  Serial.println(request->url());
  request->send(404, "text/plain", "Not found");
}

void handleIndex(AsyncWebServerRequest * request) {
  if (!config.wwwFilesExist) {
    if (request->url()=="/" && request->method() == HTTP_GET) { request->send(200, "text/html", emptyfs_html); return; }
    if (request->url()=="/" && request->method() == HTTP_POST) {
      if (request->arg("ssid")!="" && request->arg("pass")!="") {
        char buf[BUFLEN];
        memset(buf, 0, BUFLEN);
        snprintf(buf, BUFLEN, "%s\t%s", request->arg("ssid").c_str(), request->arg("pass").c_str());
        request->redirect("/");
        config.saveWifi(buf);
        return;
      }
      request->redirect("/"); 
      ESP.restart();
      return;
    }
    Serial.print("Not Found: ");
    Serial.println(request->url());
    request->send(404, "text/plain", "Not found");
    return;
  } // end if (!config.wwwFilesExist)
  #if defined(HTTP_USER) && defined(HTTP_PASS)
    if (network.status == CONNECTED) {
      if (!request->authenticate(HTTP_USER, HTTP_PASS)) {
        return request->requestAuthentication();
      }
    }
  #endif
  if (strcmp(request->url().c_str(), "/") == 0 && request->params() == 0) {
    if (network.status == CONNECTED) request->send(200, "text/html", index_html); else request->redirect("/settings.html");
    return;
  }
  if (network.status == CONNECTED) {
    int paramsNr = request->params();
    if (paramsNr==1) {
      const AsyncWebParameter* p = request->getParam(0);
      if (cmd.exec(p->name().c_str(),p->value().c_str())) {
        if (p->name()=="reset" || p->name()=="clearspiffs") request->redirect("/");
        if (p->name()=="clearspiffs") { delay(100); ESP.restart(); }
        request->send(200, "text/plain", "");
        return;
      }
    }
    if (request->hasArg("treble") && request->hasArg("middle") && request->hasArg("bass")) {
      config.setTone(request->getParam("bass")->value().toInt(), request->getParam("middle")->value().toInt(), request->getParam("treble")->value().toInt());
      request->send(200, "text/plain", "");
      return;
    }
    if (request->hasArg("sleep")) {
      int sford = request->getParam("sleep")->value().toInt();
      int safterd = request->hasArg("after")?request->getParam("after")->value().toInt():0;
      if (sford > 0 && safterd >= 0) { request->send(200, "text/plain", ""); config.sleepForAfter(sford, safterd); return; }
    }
    request->send(404, "text/plain", "Not found");
    
  } else {
    request->send(404, "text/plain", "Not found");
  }
}
