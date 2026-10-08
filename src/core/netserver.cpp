#include "options.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPFileUpdater.h>
#include <ESPmDNS.h>
#include <esp_task_wdt.h>
#include <freertos/FreeRTOS.h>
#include <LittleFS.h>
#include <Update.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include "battery.h"
#include "commandhandler.h"
#include "config.h"
#include "controls.h"
#include "display.h"
#include "logging.h"
#include "mqtt.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "startup.h"
#include "telnet.h"
#include "utility.h"
#include "../locale/wwwlocale.h"
#include "../locale/locale_js.h"
#include "../locale/dsplocale.h"
#include "../displays/dspcore.h"
#include "../displays/dspfont.h"
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
  #include "filemanager.h"   // the /sdman page and the mode that gates the player
#endif

// Global list for Radio-Browser servers to persist across searches
char rb_servers[20][64];
// For the search task
volatile TaskHandle_t g_searchTaskHandle = NULL;
// For the curated playlists task
volatile TaskHandle_t g_curatedTaskHandle = NULL;
TaskHandle_t nsTaskHandle = NULL;
portMUX_TYPE taskSpawnMux = portMUX_INITIALIZER_UNLOCKED;

// PSRAM-backed static file cache implementation
// Determine MIME type from filename extension
static const char* mimeTypeForFile(const char* filename) {
    const char* ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    if (strcasecmp(ext, ".html") == 0) return "text/html";
    if (strcasecmp(ext, ".js") == 0)   return "application/javascript";
    if (strcasecmp(ext, ".css") == 0)  return "text/css";
    if (strcasecmp(ext, ".json") == 0) return "application/json";
    if (strcasecmp(ext, ".svg") == 0)  return "image/svg+xml";
    if (strcasecmp(ext, ".png") == 0)  return "image/png";
    return "application/octet-stream";
}

void StaticFileCache::loadOne(const char* filename, int idx) {
    if (idx < 0 || idx >= MAX_ENTRIES) return;
    CachedFile& e = entries[idx];
    memset(&e, 0, sizeof(CachedFile));
    snprintf(e.path, sizeof(e.path), "/%s", filename);
    e.contentType = mimeTypeForFile(filename);

    // open() is the existence test here: a separate exists() call would add a second metadata
    // lookup per file for no benefit, and those lookups are what dominate this loop (LittleFS
    // measured ~9-25 ms per operation on the SH1106/VS1053 build). The two exists() probes that
    // used to sit here cost more than the reads they guarded.
    // The size test doubles as the "is it really there" check: startup.cpp's health check warns
    // that open() can return a truthy File for a missing path on some cores, and a zero-length
    // result would otherwise be cached and then served as an empty file.
    char fullPath[64];

    // Try .gz variant first ??production builds use gzipped files on LittleFS
    snprintf(fullPath, sizeof(fullPath), "/www/%s.gz", filename);
    File gz = LittleFS.open(fullPath, "r");
    if (gz) {
        size_t sz = gz.size();
        char* buf = (sz > 0) ? (char*)ps_malloc(sz) : nullptr;
        if (buf) {
            gz.read((uint8_t*)buf, sz);
            e.gzData = buf;
            e.gzSize = sz;
        }
        gz.close();
    }

    // Fall back to the plain file when there is no .gz twin (or the .gz read failed)
    if (!e.gzData) {
        snprintf(fullPath, sizeof(fullPath), "/www/%s", filename);
        File plain = LittleFS.open(fullPath, "r");
        if (plain) {
            size_t sz = plain.size();
            char* buf = (sz > 0) ? (char*)ps_malloc(sz + 1) : nullptr;
            if (buf) {
                plain.read((uint8_t*)buf, sz);
                buf[sz] = '\0';
                e.data = buf;
                e.size = sz;
            }
            plain.close();
        }
    }
}

void StaticFileCache::loadAll() {
    freeAll();  // Free any previous allocations
    count = 0;
    for (size_t i = 0; i < Config::wwwFilesCount && count < MAX_ENTRIES; i++) {
        loadOne(Config::wwwFiles[i], count);
        if (entries[count].data || entries[count].gzData) count++;
    }
    SERIALLOGX("Loaded %d files into PSRAM cache\t", count);
}

size_t StaticFileCache::totalBytes() const {
    size_t total = 0;
    for (int i = 0; i < count; i++) {
        total += entries[i].size;     // plain data
        total += entries[i].gzSize;   // gzipped data (if loaded separately)
    }
    return total;
}

const CachedFile* StaticFileCache::find(const char* urlPath) const {
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].path, urlPath) == 0) {
            return &entries[i];
        }
    }
    return NULL;
}

bool StaticFileCache::invalidate(const char* urlPath) {
    for (int i = 0; i < count; i++) {
        if (strcmp(entries[i].path, urlPath) == 0) {
            if (entries[i].data)  free((void*)entries[i].data);
            if (entries[i].gzData) free((void*)entries[i].gzData);
            entries[i].data = NULL;
            entries[i].gzData = NULL;
            entries[i].size = 0;
            entries[i].gzSize = 0;
            loadOne(urlPath + 1, i);  // skip leading '/'
            FUNCTIONLOG("FileCache", "Invalidated and reloaded: %s", urlPath);
            return true;
        }
    }
    return false;
}

void StaticFileCache::freeAll() {
    for (int i = 0; i < count; i++) {
        if (entries[i].data)  free((void*)entries[i].data);
        if (entries[i].gzData) free((void*)entries[i].gzData);
        entries[i].data = NULL;
        entries[i].gzData = NULL;
        entries[i].size = 0;
        entries[i].gzSize = 0;
    }
    count = 0;
}

NetServer netserver;

AsyncWebServer webserver(80);
AsyncWebSocket websocket("/ws");

bool  shouldReboot  = false;

char* updateError() {
  static char ret[140] = {0};
  snprintf(ret, sizeof(ret), "Update failed with error (%d)<br /> %s", (int)Update.getError(), Update.errorString());
  return ret;
}

void handleDynamicLocale(AsyncWebServerRequest *request) {
  // Serve locale.json (gzip-compressed) from PROGMEM
  // Uses ?l=XX query param if present, otherwise config.store.locale_webui
  // English is served like every other locale.  The HTML carries English text as its own fallback, so en_US used to
  // be refused with a 404 to save the request - which only made the page log a failed load in devtools, while the
  // file stayed the master the other locales are diffed against.  One path, nothing to get out of step.
  const char* localeCode = request->hasArg("l") ? request->arg("l").c_str() : config.store.locale_webui;
  for (uint8_t i = 0; i < WWW_LOCALE_COUNT; i++) {
    if (strcmp_P(localeCode, ((const char*)pgm_read_ptr(&www_locales[i].code))) == 0) {
      uint16_t size = pgm_read_word(&www_locales[i].size);
      AsyncWebServerResponse *response = request->beginResponse(200, "application/json",
        (const uint8_t*)pgm_read_ptr(&www_locales[i].data), size);
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
  }
  request->send(404);
}

void handleWWWLocaleIndex(AsyncWebServerRequest *request) {
  request->send(200, "application/json", wwwlocale_index);
}

void handleSearch(AsyncWebServerRequest *request) {
  // handle search request
  if (request->hasParam("search")) {
    if (g_searchTaskHandle != NULL) {
      request->send(429, "text/plain", "Search task is already running.");
      return;
    }
    if (ESP.getFreeHeap() <= MIN_MALLOC) {
      request->send(503, "text/plain", "Insufficient heap for search task.");
      return;
    }
    String searchQuery = request->getParam("search")->value();
    char* search_str = new (std::nothrow) char[searchQuery.length() + 1];
    if (!search_str) {
      request->send(500, "text/plain", "Failed to allocate memory for search task.");
      return;
    }
    strcpy(search_str, searchQuery.c_str());
    taskENTER_CRITICAL(&taskSpawnMux);
    if (g_searchTaskHandle == NULL) {
      if (xTaskCreatePinnedToCore(vTaskSearchRadioBrowser, "searchRadioBrowser", 8192, (void*)search_str, LOW_TASK_PRIORITY, (TaskHandle_t*)&g_searchTaskHandle, NETWORK_CORE) != pdPASS) {
        delete[] search_str;
      }
    } else {
      delete[] search_str;
    }
    taskEXIT_CRITICAL(&taskSpawnMux);
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
    (network.status == SDOFFLINE);
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

#ifdef SAVE_LOGS_TO_FS
// /log.txt: the whole log ring as one text file, oldest file first.  The chunked loader hands the callback a running byte offset,
// which is exactly what logRingReadAt() wants, so the layout is snapshotted once at the start of the request and the offsets stay stable
// - anything logged while the client is downloading is beyond the snapshot and is simply not part of this response.
static size_t logChunk(uint8_t* buffer, size_t maxLen, size_t index) {
  return logRingReadAt(index, buffer, maxLen);
}

void handleLog(AsyncWebServerRequest *request) {
  if (logRingServeBusy()) {
    request->send(409, "text/plain", "a log download is already in progress\n");
    return;
  }
  const size_t total = logRingSnapshot();
  if (total == 0) {
    request->send(200, "text/plain", "no log stored\n");
    return;
  }
  AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain", logChunk);
  response->addHeader("Cache-Control", "no-store");
  request->send(response);
}

// /logclear empties the ring (state file too)
void handleLogClear(AsyncWebServerRequest *request) {
  if (logRingServeBusy()) {   // the wipe would pull the files out from under that download
    request->send(409, "text/plain", "a log download is in progress - wait for it to finish\n");
    return;
  }
  const size_t had = logRingClear();   // wipes and reports the bytes, without snapshotting over anyone
  char body[48];
  snprintf(body, sizeof(body), "log cleared (%u bytes)\n", (unsigned)had);
  AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", body);
  response->addHeader("Cache-Control", "no-store");   // a cached 200 would make a second clear look like a no-op
  request->send(response);
}
#endif

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
  if (sUrl.length() > MQTT_URL_SIZE) sUrl = sUrl.substring(0, MQTT_URL_SIZE);
  player.sendCommand({PR_STOP, 0}); // Stop current playback
  
  // Check for duplicate URL in playlist (for both preview and add)
  uint16_t cs = utility.playlistLength();
  uint16_t foundIdx = utility.findStationByUrl(sUrl.c_str());
  bool found = foundIdx > 0;
  
  if (!addtoplaylist) { // This is a preview
    if (found) { // URL exists in playlist, play that station
      player.sendCommand({PR_PLAY, (uint16_t)foundIdx});
      request->send(200, "text/plain", "EXISTING");
    } else { // URL not in playlist, preview in slot 0
      config.setStation(sName.c_str());
      display.putRequest(NEWSTATION);
      netserver.requestOnChange(STATIONNAME, 0);
      if (!player.queueResolvedUrl(sUrl.c_str())) {
        request->send(400, "text/plain", "Invalid url");
        return;
      }
      request->send(200, "text/plain", "PREVIEW");
    }
  } else { // This is add to playlist
    int sOvol = 0;
    if (found) { // play the slot if it already exists
      player.sendCommand({PR_PLAY, (uint16_t)foundIdx});
      request->send(200, "text/plain", "DUPLICATE");
    } else { // add it and play it
      File playlistfile = LittleFS.open(PLAYLIST_PATH, "a");
      if (playlistfile) {
        // can't use printf ??Arduino's Print::printf has a 64-byte stack buffer that overflows on long URLs
        playlistfile.print(sName);
        playlistfile.print('\t');
        playlistfile.print(sUrl);
        playlistfile.print('\t');
        playlistfile.print(sOvol);
        playlistfile.write((const uint8_t*)"\r\n", 2);
        playlistfile.close();
        esp_task_wdt_reset(); // Reset watchdog before heavy operations
        uint16_t newIdx = cs + 1;
        utility.indexPlaylist();
        esp_task_wdt_reset(); // Reset watchdog between operations
        utility.initPlaylist();
        player.sendCommand({PR_PLAY, newIdx});
        netserver.requestOnChange(PLAYLISTSAVED, 0);
        request->send(200, "text/plain", "ADDED");
      } else {
        request->send(500, "text/plain", "Failed to open playlist file");
      }
    }
  }
}

static void netserverLoopTask(void* pvParameters) {
  for(;;) {
    if (network.status != SDOFFLINE) {
      netserver.loop();
    }
    vTaskDelay(pdMS_TO_TICKS(NETSERVER_TASK_DELAY));
  }
}

void NetServer::startLoopTask() {
  xTaskCreatePinnedToCore(netserverLoopTask, "netserverLoop", (NETSERVER_TASK_STACK_SIZE * 1024), NULL, NETSERVER_TASK_PRIORITY, &nsTaskHandle, NETWORK_CORE);
}

void NetServer::restartMdns() {
  if (strlen(config.store.mdnsname) == 0) return;
  MDNS.end();
  MDNS.begin(config.store.mdnsname);
  #if USE_OTA
    ArduinoOTA.setHostname(config.store.mdnsname);
  #endif
}

bool NetServer::begin(bool quiet) {
  if (network.status==SDOFFLINE) return true;
  if (!quiet) BOOTLOGX("netserver.begin\t");
  nsQueue = xQueueCreate(64, sizeof(nsRequestParams_t));
  if (nsQueue==NULL) { ERRORLOG("NETSERVER: nsQueue alloc failed? Rebooting."); delay(10); ESP.restart(); }

  webserver.on("/", HTTP_ANY, handleIndex);
  webserver.on("/ready", HTTP_GET, handleReady);
  #ifdef SAVE_LOGS_TO_FS
    // The saved log ring, oldest file first (logging.h). Both spellings: /log is the friendly one, /log.txt is what a
    // curl or an old bookmark carries. This router matches an exact path or that path plus "/", so neither shadows the
    // other and a bare /sdman route is impossible for the manager; /logclear is a different path, not /log plus "/".
    webserver.on("/log", HTTP_GET, handleLog);
    webserver.on("/log.txt", HTTP_GET, handleLog);
    webserver.on("/logclear", HTTP_GET, handleLogClear);
  #endif
  webserver.on("/locale.json", HTTP_GET, handleDynamicLocale);
  webserver.on("/wwwlocale.json", HTTP_GET, handleWWWLocaleIndex);
  webserver.on("/dsplocale.json", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", dsplocale_index);
  });
  webserver.on("/locale.js", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", locale_js_gz, LOCALE_JS_GZ_SIZE);
    response->addHeader("Content-Encoding", "gzip");
    request->send(response);
  });
  webserver.on("/search", HTTP_GET, handleSearch);
  webserver.on("/search", HTTP_POST, handleSearchPost);
  #ifdef USE_SD
    // SD card file manager.  GET /sdman opens the mode and the API lives under the same prefix; the page itself is served by handleIndex, which hands the root over while the mode is up
    filemanager.registerRoutes(webserver);
  #endif

  // Captive portal detection ??redirect probes from iOS, Android, Windows to the web UI
  auto captiveRedirect = [](AsyncWebServerRequest *request) { request->redirect("/"); };
  webserver.on("/hotspot-detect.html", HTTP_GET, captiveRedirect);          // iOS / macOS
  webserver.on("/library/test/success.html", HTTP_GET, captiveRedirect);    // iOS / macOS (older)
  webserver.on("/generate_204", HTTP_GET, captiveRedirect);                 // Android
  webserver.on("/gen_204", HTTP_GET, captiveRedirect);                      // Android (older)
  webserver.on("/ncsi.txt", HTTP_GET, captiveRedirect);                     // Windows
  webserver.on("/connecttest.txt", HTTP_GET, captiveRedirect);              // Windows

  if (psramFound()) fileCache.loadAll();
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
  // Ensure any connected web clients receive the current battery status immediately
  requestOnChange(GETBATTERY, 0);
  #if USE_OTA
    if (strlen(config.store.mdnsname)>0) ArduinoOTA.setHostname(config.store.mdnsname);
    #ifdef OTA_PASS
      ArduinoOTA.setPassword(OTA_PASS);
    #endif
    ArduinoOTA
      .onStart([]() {
        display.putRequest(NEWMODE, UPDATING);
        FUNCTIONLOG("OTA", "Start OTA updating %s", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
      })
      .onEnd([]() {
        FUNCTIONLOG("OTA", "End OTA update, rebooting...");
      })
      .onProgress([](unsigned int progress, unsigned int total) {
        unsigned int percent = (total > 0) ? ((progress * 100U) / total) : 0;
        FUNCTIONLOG("OTA.update", "Progress OTA: %u%%", percent);
      })
      .onError([](ota_error_t error) {
        const char* reason = "Unknown";
        if (error == OTA_AUTH_ERROR) {
          reason = "Auth Failed";
        } else if (error == OTA_BEGIN_ERROR) {
          reason = "Begin Failed";
        } else if (error == OTA_CONNECT_ERROR) {
          reason = "Connect Failed";
        } else if (error == OTA_RECEIVE_ERROR) {
          reason = "Receive Failed";
        } else if (error == OTA_END_ERROR) {
          reason = "End Failed";
        }
        FUNCTIONLOG("OTA", "Error[%u]: %s", static_cast<unsigned>(error), reason);
      });
    ArduinoOTA.begin();
  #endif //#if USE_OTA

  if (!quiet) SERIALLOG("done");
  return true;
}

size_t NetServer::chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index) {
  File requiredfile;
  bool sdpl = strcmp(netserver.chunkedPathBuffer, PLAYLIST_SD_PATH) == 0;
  if (sdpl) {
    requiredfile = config.SDPLFS()->open(netserver.chunkedPathBuffer, "r");
  } else {
    requiredfile = LittleFS.open(netserver.chunkedPathBuffer, "r");
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
  #ifdef NETSERVER_DEBUG
    FUNCTIONLOG ("Netserver.debug", "[%s] seek to %d in %s and read %d bytes with maxLen=%d", __func__, index, netserver.chunkedPathBuffer, canread, maxLen);
  #endif
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
  if (xQueueReceive(nsQueue, &request, pdMS_TO_TICKS(NS_QUEUE_DELAY))) {
    char wsbuf[WEBSOCKET_BUFFER] = {0};
    uint8_t clientId = request.clientId;
    switch (request.type) {
      case PLAYLIST:        getPlaylist(clientId); break;
      case PLAYLISTSAVED:   {
        #ifdef USE_SD
          if (config.getMode()==PM_SDCARD) {
            config.initSDPlaylist();
          }
        #endif
        if (config.getMode()==PM_WEB) {
          utility.indexPlaylist(); 
          utility.initPlaylist(); 
        }
        getPlaylist(clientId); break;
      }
      case GETACTIVE: {
          #define DBGWUI false // set true to debug WebUI
          String act = F("\"group_wifi\",");
          if (network.status == CONNECTED) {
                                                                act += F("\"group_system\",");
            if (battery.isInitialized() || DBGWUI)              act += F("\"group_battery\",");
            if (DSP_MODEL != DSP_DUMMY || DBGWUI)
                                                                act += F("\"group_display\",");
            #if (I2S_BCLK!=255 || (VS1053_CS != 255 && VS_PATCH_ENABLE == true) || DBGWUI)
                                                                act += F("\"group_vu_ss\",");
              if (vuConf_ptr->textsize > 0 || DBGWUI)           act += F("\"group_vu\",");
                                                           else act += F("\"hide_group_vu\",");
              if (config.store.screensaverVU || DBGWUI)         act += F("\"group_vu_ss_style\",");
                                                           else act += F("\"hide_group_vu_ss_style\",");
            #endif
            if (bufferbarConf_ptr->height > 0 || DBGWUI)        act += F("\"group_buffer\",");
                                                           else act += F("\"hide_group_buffer\",");
            if (BRIGHTNESS_PIN != 255 || DBGWUI)                act += F("\"group_brightness\",");
            if (DSP_DIMMING_ENABLED || DBGWUI)                  act += F("\"group_dimming\",");
            #if defined(DSP_TFT)
                                                                act += F("\"group_color\",");
            #endif
            if (display.getLayoutCount() > 1 || DBGWUI)         act += F("\"group_layout\",");
            #if defined(DSP_TFT) || DBGWUI
              if (display.getThemeCount() > 1|| DBGWUI)         act += F("\"group_theme\",");
            #endif
            if ((activeLayout.fullClock && !config.store.screensaverVU) || DBGWUI)
                                                                act += F("\"group_full_time\",");
                                                           else act += F("\"hide_group_full_time\",");
            if (TS_MODEL != TS_MODEL_UNDEFINED || DBGWUI)       act += F("\"group_touch\",");
                                                                act += F("\"group_locale\",");
            if (weatherConf_ptr->buffsize > 0 || DBGWUI)        act += F("\"group_weather\",");
                                                           else act += F("\"hide_group_weather\",");
                                                                act += F("\"group_controls\",");
            if ((DSP_MODEL != DSP_DUMMY && (BTN_NEXT != 255 || BTN_PREV != 255)) || DBGWUI)
                                                                act += F("\"group_stnbuttons\",");
            if (ENC_DT != 255 || ENC2_DT != 255 || DBGWUI)      act += F("\"group_encoder\",");
            if (IR_PIN != 255 || DBGWUI)                        act += F("\"group_ir\",");
            #if defined(UPDATEURL) || DBGWUI
                                                                act += F("\"group_update\",");
            #endif
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
          requestOnChange(PLAYLISTREADY, clientId);
          requestOnChange(GETBATTERY, clientId); 
          if (config.getMode()==PM_SDCARD) { requestOnChange(SDPOS, clientId); requestOnChange(SDLEN, clientId); requestOnChange(SDSHUFFLE, clientId); } 
          return; 
          break;
        }
      case GETCONTROLS:   snprintf(wsbuf, sizeof(wsbuf), "{\"sst\":%d,\"oneclick\":%d,\"tsf\":%d,\"tsd\":%d,\"enca\":%d,\"irtl\":%d,\"maxVol\":%d}",
                                  config.store.smartstart,
                                  config.store.oneclickswitch,
                                  config.store.fliptouch,
                                  config.store.dbgtouch,
                                  config.store.encacc,
                                  config.store.irtlp,
                                  VOLUME_SCALE);
                                  break;
      case GETSCREEN:     snprintf(wsbuf, sizeof(wsbuf), "{\"flip\":%d,\"inv\":%d,\"nump\":%d,\"dspon\":%d,\"br\":%d,\"scre\":%d,\"scrb\":%d,\"scrt\":%d,\"scrpe\":%d,\"scrpb\":%d,\"scrpt\":%d,\"scrfull\":%d,\"scrtext\":%d,\"scrvu\":%d,\"scrstyle\":%d,\"scrpeak\":%d,\"bufbar\":%d,\"vu\":%d,\"vupeak\":%d,\"vustyle\":%d,\"dim\":%d,\"dimto\":%d,\"dimbr\":%d,\"volpg\":%d,\"clock12\":%d,\"invtitle\":%d,\"layoutId\":%d,\"themeId\":%d,\"systemFontId\":%d,\"clockFontId\":%d,\"clockglow\":%d}",
                                  config.store.flipscreen,
                                  config.store.invertdisplay,
                                  config.store.numplaylist,
                                  config.store.dspon,
                                  config.store.brightness,
                                  config.store.screensaverEnabled,
                                  config.store.screensaverBlank,
                                  config.store.screensaverTimeout,
                                  config.store.screensaverPlayingEnabled,
                                  config.store.screensaverPlayingBlank,
                                  config.store.screensaverPlayingTimeout,
                                  config.store.screensaverFullDateTime,
                                  config.store.screensaverText,
                                  config.store.screensaverVU,
                                  config.store.screensaverVUStyle,
                                  config.store.screensaverVUpeak,
                                  config.store.bufferbar,
                                  config.store.vumeter,
                                  config.store.vupeak,
                                  config.store.vustyle,
                                  config.store.dimmingEnabled,
                                  config.store.dimmingTimeout,
                                  config.store.dimmingBrightness,
                                  config.store.volumepage,
                                  config.store.clock12,
                                  config.store.inverttitle,
                                  config.store.layoutId,
                                  config.store.themeId,
                                  config.store.systemFontId,
                                  config.store.clockFontId,
                                  config.store.clockglow);
                                  break;
      case GETLOCALE:     snprintf(wsbuf, sizeof(wsbuf), "{\"locale_webui\":\"%s\",\"locale_disp\":\"%s\",\"tz_name\":\"%s\",\"tzposix\":\"%s\",\"sntp1\":\"%s\",\"sntp2\":\"%s\",\"timeinterval\":%d}",
                                  config.store.locale_webui,
                                  config.store.locale_display,
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
      case GETSYSTEM:     snprintf(wsbuf, sizeof(wsbuf), "{\"wifiscan\":%d,\"ehdp\":%d,\"ehdpname\":\"%s\",\"mdns\":\"%s\",\"autoupdate\":%d}",
                                  config.store.wifiscanbest,
                                  config.store.ehdp,
                                  config.store.ehdpname,
                                  config.store.mdnsname,
                                  config.store.autoupdate);
                                  break;
      case GETMQTT:       snprintf(wsbuf, sizeof(wsbuf), "{\"mqttenable\":%d,\"mqtthost\":\"%s\",\"mqttport\":\"%d\",\"mqttuser\":\"%s\",\"mqttpass\":\"%s\",\"mqtttopic\":\"%s\"}",
                                  config.store.mqttenable,
                                  config.store.mqtthost,
                                  config.store.mqttport,
                                  config.store.mqttuser,
                                  config.store.mqttpass,
                                  config.store.mqtttopic);
                                  break;
      case DSPON:         snprintf(wsbuf, sizeof(wsbuf), "{\"dspontrue\":%d}", 1); break;
      case STATION:       requestOnChange(STATIONNAME, clientId); requestOnChange(ITEM, clientId); break;
      case STATIONNAME:   snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"nameset\", \"value\": \"%s\"}]}", config.station.name); break;
      case ITEM:          snprintf(wsbuf, sizeof(wsbuf), "{\"current\": %d}", config.lastStation()); break;
      case TITLE:         snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"meta\", \"value\": \"%s\"}]}", config.station.title); break;
      case VOLUME:        snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"volume\", \"value\": %d, \"max\": %d}]}", config.store.volume, VOLUME_SCALE); break;
      case NRSSI:         snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"rssi\", \"value\": %d}, {\"id\":\"rssibars\", \"value\": %d}]}", rssi, rssiLevel(rssi)); break;
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
        uint32_t battref = config.store.battery_adc_ref_mv ? config.store.battery_adc_ref_mv : (uint32_t)BATTERY_ADC_REF_MV;
        #ifdef BATTERY_FORCE_DISPLAY
          // The same fake the on-screen widget uses, so the WebUI battery row can be laid out with no battery fitted
          int pct = BATTERY_FORCE_DISPLAY;
          if (pct < 0) pct = 0;
          if (pct > 100) pct = 100;
          uint32_t mvmin = (uint32_t)BATTERY_PRESENT_MIN_MV;
          uint32_t mvmax = (uint32_t)BATTERY_PRESENT_MAX_MV;
          if (mvmin < 2500) mvmin = 2500;
          if (mvmax > 5000) mvmax = 5000;
          if (mvmin >= mvmax) { mvmin = 3000; mvmax = 4200; }
          char valbuf[64];
          snprintf(valbuf, sizeof(valbuf), "volt: %umV, percentage: %d%%", (unsigned)(mvmin + (uint32_t)pct * (mvmax - mvmin) / 100), pct);
          snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"battery\", \"value\": \"%s\"}, {\"id\":\"battref\", \"value\": %u}]}", valbuf, battref);
        #else
          BatteryStatus bat = battery.getStatus();
          if (!bat.present && !battery.isInitialized()) {
            snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"battery\", \"value\": \"\"}, {\"id\":\"battref\", \"value\": %u}]}", battref);
          } else {
            char valbuf[64];
            snprintf(valbuf, sizeof(valbuf), "volt: %umV, percentage: %d%%", (unsigned)bat.voltage_mv, bat.percentage);
            snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"battery\", \"value\": \"%s\"}, {\"id\":\"battref\", \"value\": %u}]}", valbuf, battref);
          }
        #endif
        break;
      }
      case MODE:          snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"playerwrap\", \"value\": \"%s\"}]}", player.status() == PLAYING ? "playing" : "stopped"); break;
      case EQUALIZER:     snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\":\"bass\", \"value\": %d}, {\"id\": \"middle\", \"value\": %d}, {\"id\": \"treble\", \"value\": %d}]}", config.store.bass, config.store.middle, config.store.treble); break;
      case BALANCE:       snprintf(wsbuf, sizeof(wsbuf), "{\"payload\":[{\"id\": \"balance\", \"value\": %d}]}", config.store.balance); break;
      case SDINIT:        snprintf(wsbuf, sizeof(wsbuf), "{\"sdinit\": %d}", SD_CS!=255); break;
      case GETPLAYERMODE: snprintf(wsbuf, sizeof(wsbuf), "{\"playermode\": \"%s\"}", config.getMode()==PM_SDCARD?"modesd":"modeweb"); break;
      case SEARCH_DONE:   snprintf(wsbuf, sizeof(wsbuf), "{\"search_done\":true}"); break;
      case SEARCH_FAILED: snprintf(wsbuf, sizeof(wsbuf), "{\"search_failed\":true}"); break;
      // One message, two meanings, so a page never has to guess: "false" is the card being walked (blank the
      // list, spin, and refuse to load) and "true" is that walk being over (unlock and fetch).  The flag lives in
      // the file manager, so a client that connects mid-walk is answered from the same source.
      case PLAYLISTREADY:
        #ifdef USE_SD
          snprintf(wsbuf, sizeof(wsbuf), "{\"playlistready\":%s}", filemanager.rebuilding() ? "false" : "true");
        #else
          snprintf(wsbuf, sizeof(wsbuf), "{\"playlistready\":true}");
        #endif
        break;
      // The manager page follows the device on this instead of polling it, which is why the state is the MANAGER's
      // and not the play mode: the page is only valid while the manager is up, whatever the radio is playing.
      // Broadcast on enter and leave, and sent to a client the moment it connects, so a page opened later still
      // learns the state.  No manager in the build, no reason for a page to stay: 0.
      case SDMANACTIVE:
        #ifdef USE_SD
          snprintf(wsbuf, sizeof(wsbuf), "{\"sdmanactive\":%d}", filemanager.active() ? 1 : 0);
        #else
          snprintf(wsbuf, sizeof(wsbuf), "{\"sdmanactive\":0}");
        #endif
        break;
      case CURATED_INDEX_DONE: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_index_done\":true}"); break;
      case CURATED_PLAYLIST_DONE: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_playlist_done\":true}"); break;
      case CURATED_FAILED: snprintf(wsbuf, sizeof(wsbuf), "{\"curated_failed\":true}"); break;
      case ARTWORK:       break;
      #ifdef USE_SD
        case CHANGEMODE:    config.changeMode(config.newConfigMode); return; break;
      #endif
      default:          break;
    }
    if (strlen(wsbuf) > 0) {
      if (clientId == 0) { websocket.textAll(wsbuf); } else { websocket.text(clientId, wsbuf); }
    }
  }
}

void NetServer::loop() {
  if (network.status==SDOFFLINE) return;
  if (shouldReboot) {
    FUNCTIONLOG("Netserver", "Rebooting...");
    delay(100);
    ESP.restart();
  }
  websocket.cleanupClients();
  switch (importRequest) {
    case IMWIFI:  utility.importWifi(); importRequest = IMDONE; break;
    default:      break;
  }
  processQueue();
  mqtt.loop(); // single owner of the MQTT client: applies requests, runs queued commands, publishes
  #ifdef RADIO_BROWSER_SEND_CLICKS
    processRadioBrowserClick();
  #endif
  #if USE_OTA
    ArduinoOTA.handle();
  #endif
}

void NetServer::irToWs(const char* protocol, uint64_t irvalue) {
  #if IR_PIN!=255
    char buf[80] = { 0 };
    snprintf(buf, sizeof(buf), "{\"ircode\": %llu, \"protocol\": \"%s\"}", irvalue, protocol);
    websocket.textAll(buf);
  #endif
}
void NetServer::irValsToWs() {
  #if IR_PIN!=255
    if (!irRecordEnable) return;
    const uint64_t* irVals = config.irCodes(static_cast<uint8_t>(config.irindex));
    if (irVals == nullptr) return;
    char buf[80] = { 0 };
    snprintf(buf, sizeof(buf), "{\"irvals\": [%llu, %llu, %llu]}", irVals[0], irVals[1], irVals[2]);
    websocket.textAll(buf);
  #endif
}

void NetServer::onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId) {
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    /*
     * Do NOT write to data[len] ??AsyncWebServer does not guarantee an extra
     * NUL byte in the provided buffer. Copy into a local, NUL-terminated
     * stack buffer and parse that instead to avoid heap corruption.
     */
    char payload[WEBSOCKET_BUFFER];
    size_t payloadLen = (len < sizeof(payload) - 1) ? len : (sizeof(payload) - 1);
    memcpy(payload, data, payloadLen);
    payload[payloadLen] = '\0';

    char command[65], val[65];
    if (utility.parseWsCommand(payload, command, val, 65)) {
      if (cmd.exec(command, val, clientId, CommandSource::WebSocket)) {
        return;
      }
    }
  }
}

void NetServer::getPlaylist(uint8_t clientId) {
  char buf[WEBSOCKET_BUFFER] = {0};  // buffer for playlist URL JSON (IPv6-safe)
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
  if (xQueueSend(nsQueue, &nsrequest, pdMS_TO_TICKS(NSQ_SEND_DELAY)) != pdTRUE) {
    FUNCTIONLOG("Queue", "nsQueue overflow, dropped req=%d", request);
  }
}

void NetServer::resetQueue() {
  if (nsQueue!=NULL) xQueueReset(nsQueue);
}

int freeSpace;
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  if (request->url()=="/upload") {
    if (!index) {
      freeSpace = (float)LittleFS.totalBytes()/100*68-LittleFS.usedBytes();
      request->_tempFile = LittleFS.open(TMP_PATH , "w");
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
      // getParam returns nullptr when the parameter is absent, so it is tested before being
      // dereferenced. An absent or unrecognised target means firmware, which is what the
      // firmware-only emergency form relies on.
      const AsyncWebParameter* targetParam = request->getParam("updatetarget", true);
      int target = (targetParam && targetParam->value() == "littlefs") ? U_SPIFFS : U_FLASH;
      FUNCTIONLOG("Netserver", "Update Start: %s", filename.c_str());
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
        FUNCTIONLOG("Netserver", "Update Success: %uB", index + len);
      } else {
        Update.printError(Serial);
        request->send(200, "text/html", updateError());
      }
    }
  } else { // "/webboard"
    #ifdef NETSERVER_DEBUG
      FUNCTIONLOG ("Netserver.debug", "File: %s, size:%u bytes, index: %u, final: %s\n", filename.c_str(), len, index, final?"true":"false");
    #endif
    if (!index) {
      String spath = "/www/";
      if (filename=="playlist.csv" || filename=="wifi.csv") spath = "/data/";
      request->_tempFile = LittleFS.open(spath + filename , "w");
    }
    if (len) {
      request->_tempFile.write(data, len);
    }
    if (final) {
      request->_tempFile.close();
      if (filename=="playlist.csv") {
        // Remove index and SD index to force cleanPlaylist to run
        if (LittleFS.exists(INDEX_PATH)) LittleFS.remove(INDEX_PATH);
        if (LittleFS.exists(INDEX_SD_PATH)) LittleFS.remove(INDEX_SD_PATH);
        utility.cleanPlaylist();   // Rewrites with CRLF, removes blank lines; calls indexPlaylist() internally
        netserver.requestOnChange(PLAYLISTSAVED, 0);
      } else {
        // Invalidate PSRAM cache for uploaded www files
        char cachePath[32];
        snprintf(cachePath, sizeof(cachePath), "/%s", filename.c_str());
        netserver.invalidateCache(cachePath);
      }
    }
  }
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
        FUNCTIONLOG("Websocket", "client #%u connected from %s", client->id(), client->remoteIP().toString().c_str());
        // Send current battery status to the newly connected client immediately
        netserver.requestOnChange(GETBATTERY, client->id());
        // And the file manager's state: a page that opened while the manager was already up is told so instead of
        // waiting for a broadcast it missed.
        netserver.requestOnChange(SDMANACTIVE, client->id());
        break;
    case WS_EVT_DISCONNECT: FUNCTIONLOG("Websocket", "client #%u disconnected", client->id()); break;
    case WS_EVT_DATA: netserver.onWsMessage(arg, data, len, client->id()); break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

// Helper to select and randomize radio-browser servers
void selectRadioBrowserServer() {
  size_t arr_size = sizeof(rb_servers) / sizeof(rb_servers[0]);
  for (size_t i = 0; i < arr_size; ++i) rb_servers[i][0] = '\0';
  File serversFile = LittleFS.open("/www/rb_srvrs.json", "r");
  if (!serversFile) {
    FUNCTIONLOG("Search", "[Error] Failed to open /www/rb_srvrs.json.");
    goto useHostname;
  } else {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, serversFile);
    serversFile.close();
    if (error) {
      FUNCTIONLOG("Search", "[Error] deserializeJson() failed: %s", error.c_str());
      goto useHostname; // get out of the else
    }
    JsonArray servers = doc.as<JsonArray>();
    if (servers.isNull() || servers.size() == 0) {
      FUNCTIONLOG("Search", "[Error] JSON is not a valid or is an empty array.");
      goto useHostname; //get out of the else
    }
    // Collect unique IPv4 server names
    size_t count = 0;
    for (JsonObject server_obj : servers) {
      const char* srvr_name = server_obj["name"];
      bool duplicate = false;
      for (size_t j = 0; j < count; ++j) {
        if (strcmp(rb_servers[j], srvr_name) == 0) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate && count < arr_size) {
        strlcpy(rb_servers[count++], srvr_name, sizeof(rb_servers[0]));
      }
    }
    // Shuffle (Fisher-Yates)
    if (count > 1) {
      for (size_t i = count - 1; i > 0; --i) {
        size_t j = random(i + 1);
        char temp[64];
        strlcpy(temp, rb_servers[i], sizeof(temp));
        strlcpy(rb_servers[i], rb_servers[j], sizeof(rb_servers[0]));
        strlcpy(rb_servers[j], temp, sizeof(rb_servers[0]));
      }
    }

    // Add fallback as last entry after the shuffled servers
    if (count < arr_size) strlcpy(rb_servers[count], RADIO_BROWSER_SERVER, sizeof(rb_servers[0]));
  }
  return;
useHostname:
  // Use hostname instead of IP to ensure proper Host header and HTTPS support
  FUNCTIONLOG("Search", "Using fallback: %s.", RADIO_BROWSER_SERVER);
  strlcpy(rb_servers[0], RADIO_BROWSER_SERVER, sizeof(rb_servers[0]));
}

void vTaskSearchRadioBrowser(void *pvParameters) {
  char* search_str = (char*)pvParameters;
  FUNCTIONLOG("Search", "Starting Radio Browser search. Search: %s", search_str);
  LittleFS.remove("/www/searchresults.json");
  // Check LittleFS free space
  size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    FUNCTIONLOG("Search", "[Error] Not enough free LittleFS space: %u bytes. Aborting.", freeSpace);
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
    if (rb_servers[i][0] != '\0') server_count++;
  }
  // If the list is empty, it's the first run or all servers failed previously. Let's (re)populate it.
  if (server_count == 0) {
    FUNCTIONLOG("Search", "Server list is empty, repopulating from file.");
    selectRadioBrowserServer();
    // Recount after filling
    server_count = 0;
    for (size_t i = 0; i < arr_size; ++i) {
      if (rb_servers[i][0] != '\0') server_count++;
    }
  }
  // If still no servers, then the API source is likely down or unreachable.
  if (server_count == 0) {
    FUNCTIONLOG("Search", "[Error] No servers available after attempting to select.");
    netserver.requestOnChange(SEARCH_FAILED, 0);
    delete[] search_str;
    g_searchTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  ESPFileUpdater searchResultsFetch(LittleFS);
  searchResultsFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  searchResultsFetch.setBuffer(SEARCHRESULTS_BUFFER * 1024);
  searchResultsFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/searchresults.json";
  bool success = false;
  bool server_retried = false;
  bool json_valid = false;
  for (size_t i = 0; i < arr_size; ++i) {
    if (rb_servers[i][0] == '\0') continue;
    const char* server = rb_servers[i];
    // Compose the URL using the full search string
    String url = String("https://") + server + "/json/stations/search?" + search_str;
    FUNCTIONLOG("Search", "Attempting to download from: %s", url.c_str());
    auto status = searchResultsFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      FUNCTIONLOG("Search", "Successfully downloaded from %s", server);
      // Check if the downloaded file ends with ']' (an incomplete .json will not)
      File jsonFile = LittleFS.open(localPath, "r");
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
            FUNCTIONLOG("Search", "[Warning] JSON validation failed. Not retrying.");
            server_retried = false;
            LittleFS.remove(localPath); // Clean up bad file
          } else {
            FUNCTIONLOG("Search", "[Warning] JSON validation failed. Retrying same server.");
            server_retried = true;
            --i;
            LittleFS.remove(localPath); // Clean up bad file
          }
          continue;
        } else {
          json_valid = true;
        }
      } else {
        if (server_retried == true) {
          FUNCTIONLOG("Search", "[Error] Could not open searchresults.json for validation. Not retrying.");
          server_retried = false;
        } else {
          FUNCTIONLOG("Search", "[Error] Could not open searchresults.json for validation. Retrying same server.");
          server_retried = true;
          --i;
        }
        continue;
      }
      if (json_valid) {
        // Write /www/search.txt with the actual search string (single line)
        File file = LittleFS.open("/www/search.txt", "w");
        if (file) {
          file.printf("%s\n", search_str);
          file.close();
        } else {
          FUNCTIONLOG("Search", "[Error] Failed to open search.txt for writing.");
        }
        success = true;
        break;
      } else {
        FUNCTIONLOG("Search", "[Error] Invalid JSON from %s. Removing from list.", server);
        rb_servers[i][0] = '\0';
        server_retried = false;
      }
    } else {
      FUNCTIONLOG("Search", "[Error] Failed to download from %s. Removing from persistent list.", server);
      rb_servers[i][0] = '\0';
      server_retried = false;
    }
  }
  if (success) {
    netserver.requestOnChange(SEARCH_DONE, 0);
  } else {
    FUNCTIONLOG("Search", "[Error] Failed to download from all available servers.");
    LittleFS.remove(localPath); // Clean up any incomplete file
    netserver.requestOnChange(SEARCH_FAILED, 0);
  }
  delete[] search_str;
  search_str = nullptr;
  g_searchTaskHandle = NULL;
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
  #endif
  vTaskDelete(NULL);
}

void vTaskFetchCuratedIndex(void *pvParameters) {
  FUNCTIONLOG("Curated", "Starting curated index fetch");
  LittleFS.remove("/www/curated.json");
  
  // Check LittleFS free space
  size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    FUNCTIONLOG("Curated", "[Error] Not enough free LittleFS space: %u bytes. Aborting.", freeSpace);
    netserver.requestOnChange(CURATED_FAILED, 0);
    g_curatedTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  
  ESPFileUpdater curatedFetch(LittleFS);
  curatedFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  curatedFetch.setBuffer(SEARCHRESULTS_BUFFER * 1024);
  curatedFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/curated.json";
  
  #ifdef CURATED_LISTS_URL
    String url = String(CURATED_LISTS_URL) + String(CURATED_LISTS_INDEX);
    FUNCTIONLOG("Curated", "Attempting to download index from: %s", url.c_str());
    
    auto status = curatedFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      FUNCTIONLOG("Curated", "Successfully downloaded curated index");
      netserver.requestOnChange(CURATED_INDEX_DONE, 0);
    } else {
      FUNCTIONLOG("Curated", "[Error] Failed to download curated index");
      LittleFS.remove(localPath);
      netserver.requestOnChange(CURATED_FAILED, 0);
    }
  #else
    FUNCTIONLOG("Curated", "[Error] CURATED_LISTS_URL not defined");
    netserver.requestOnChange(CURATED_FAILED, 0);
  #endif
  
  g_curatedTaskHandle = NULL;
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
  #endif
  vTaskDelete(NULL);
}

void vTaskFetchCuratedPlaylist(void *pvParameters) {
  char* filename = (char*)pvParameters;
  FUNCTIONLOG("Curated", "Starting playlist fetch: %s", filename);
  LittleFS.remove("/www/pl_import.json");
  
  // Check LittleFS free space
  size_t freeSpace = LittleFS.totalBytes() - LittleFS.usedBytes();
  if (freeSpace < (FS_REQUIRED_FREE_SPACE * 1024)) {
    FUNCTIONLOG("Curated", "[Error] Not enough free LittleFS space: %u bytes. Aborting.", freeSpace);
    netserver.requestOnChange(CURATED_FAILED, 0);
    delete[] filename;
    g_curatedTaskHandle = NULL;
    vTaskDelete(NULL);
    return;
  }
  
  ESPFileUpdater playlistFetch(LittleFS);
  playlistFetch.setUserAgent(ESPFILEUPDATER_USERAGENT);
  playlistFetch.setBuffer(SEARCHRESULTS_BUFFER * 1024);
  playlistFetch.setYieldInterval(SEARCHRESULTS_YIELDINTERVAL);
  const char* localPath = "/www/pl_import.json";
  
  #ifdef CURATED_LISTS_URL
    String url = String(CURATED_LISTS_URL) + String(filename);
    FUNCTIONLOG("Curated", "Attempting to download playlist from: %s", url.c_str());
    
    auto status = playlistFetch.checkAndUpdate(localPath, url, ESPFILEUPDATER_VERBOSE);
    if (status == ESPFileUpdater::UPDATED) {
      FUNCTIONLOG("Curated", "Successfully downloaded playlist: %s", filename);
      netserver.requestOnChange(CURATED_PLAYLIST_DONE, 0);
    } else {
      FUNCTIONLOG("Curated", "[Error] Failed to download playlist: %s", filename);
      LittleFS.remove(localPath);
      netserver.requestOnChange(CURATED_FAILED, 0);
    }
  #else
    FUNCTIONLOG("Curated", "[Error] CURATED_LISTS_URL not defined");
    netserver.requestOnChange(CURATED_FAILED, 0);
  #endif
  
  delete[] filename;
  g_curatedTaskHandle = NULL;
  #ifdef CORE_MONITOR
    FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
  #endif
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
  FUNCTIONLOG("Netserver", "Creating a dedicated task for playback.");
  if (ESP.getFreeHeap() <= MIN_MALLOC) {
    FUNCTIONLOG("Heap", "low heap (%u), refusing playback task spawn", ESP.getFreeHeap());
    return;
  }
  // Use a lambda to capture the URL and pass it to the task
  String* url_copy = new String(url);
  if (url_copy) {
    // Use a larger stack for HTTPS, as it requires more memory for SSL/TLS.
    UBaseType_t stackSize = url.startsWith("https://") ? 8192 : 4096;
    if (xTaskCreatePinnedToCore(
        [](void* pvParameters) {
          String* urlToPlay = (String*)pvParameters;
          vTaskDelay(pdMS_TO_TICKS(100)); // A small delay can help the network stack release resources
          FUNCTIONLOG("PlaybackTask", "Starting playback for URL: %s. Free heap: %u", urlToPlay->c_str(), ESP.getFreeHeap());
          player.playUrl(urlToPlay->c_str());
          delete urlToPlay; // Free the string
          #ifdef CORE_MONITOR
            FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
          #endif
          vTaskDelete(NULL);
        },
        "playbackTask",
        stackSize,
        (void*)url_copy,
        PLAYBACK_TASK_PRIORITY,
        NULL,
        NETWORK_CORE
    ) != pdPASS) {
      delete url_copy;
      FUNCTIONLOG("Netserver", "[Error] xTaskCreate failed for playbackTask.");
    }
  } else {
    FUNCTIONLOG("Netserver", "[Error] Failed to allocate memory for playback task URL.");
  }
}

#ifdef RADIO_BROWSER_SEND_CLICKS
  // Global state for click tracking
  static unsigned long clickDelayStart = 0;
  static bool clickDelayActive = false;
  static char pendingClickUrl[256] = {0};
  // Longest a click may be held for the startup services, counted from the end of the click delay.  The cap is there
  // in case they never finish, so a stuck state cannot wedge the feature - the same rule Mqtt::servicesWaitLimitMs has.
  static constexpr uint32_t rbClickServicesWaitMs = 60000;
  static bool rbClickHoldLogged = false;
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
      FUNCTIONLOG("RB Click", "HTTP error %d", httpCode);
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
        FUNCTIONLOG("RB Click", "Stream parsing timeout");
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
    if (clickDelayActive) FUNCTIONLOG("RB Click", "New station - canceling pending click");
    rbClickHoldLogged = false;  // the held message belongs to one queued click
    // Store the URL and start the delay timer
    strlcpy(pendingClickUrl, stationUrl, sizeof(pendingClickUrl));
    clickDelayStart = millis();
    clickDelayActive = true;
    FUNCTIONLOG("RB Click", "Starting %ds delay for: %s", RADIO_BROWSER_SEND_CLICK_DELAY, stationUrl);
  #endif //#ifdef RADIO_BROWSER_SEND_CLICKS
}

#ifdef RADIO_BROWSER_SEND_CLICKS
  // Background task to register the click without blocking
  void vTaskRadioBrowserClick(void* pvParameters) {
    char* stationUrl = (char*)pvParameters;
    // Step 1: Get station UUID from URL
    String lookupUrl = String("https://") + RADIO_BROWSER_SERVER + "/json/stations/byurl?url=" + String(stationUrl);
    FUNCTIONLOG("RB Click", "Looking up UUID: %s", lookupUrl.c_str());
    String stationUuid = streamJsonExtract(lookupUrl, "stationuuid");
    if (stationUuid.length() == 0) {
      FUNCTIONLOG("RB Click", "Station not found in Radio-Browser database (empty response or lookup failed)");
      delete[] stationUrl;
      vTaskDelete(NULL);
      return;
    }
    // Step 2: Send the click
    String clickUrl = String("https://") + RADIO_BROWSER_SERVER + "/json/url/" + stationUuid;
    String okStatus = streamJsonExtract(clickUrl, "ok");
    if (okStatus == "true") {
      FUNCTIONLOG("RB Click", "Click registered successfully");
    } else {
      FUNCTIONLOG("RB Click", "Click not confirmed");
    }
    delete[] stationUrl;
    #ifdef CORE_MONITOR
      FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
    #endif
    vTaskDelete(NULL);
  }
#endif
  
void processRadioBrowserClick() {
  #ifdef RADIO_BROWSER_SEND_CLICKS
    if (!clickDelayActive) return;
    // Check if delay has elapsed
    if (millis() - clickDelayStart < (RADIO_BROWSER_SEND_CLICK_DELAY*1000)) {
      return; // Still waiting
    }
    // Abandon the click while the card is off limits - HTTPS would drain DRAM and starve SD SPI reads + MP3 decoding,
    // and the SD File Manager is rewriting the card underneath itself.  Both are the user's own doing, so it is dropped.
    bool cardOffLimits = (config.getMode() == PM_SDCARD);
    #ifdef USE_SD
      cardOffLimits = cardOffLimits || filemanager.active();
    #endif
    if (cardOffLimits) {
      clickDelayActive = false;
      FUNCTIONLOG("RB Click", "Abandoning click");
      return;
    }
    // Hold the click if Heap check fails and let the delay timer run, so the click survives the wait
    if (startup.servicesPending() &&
        (millis() - clickDelayStart) < (RADIO_BROWSER_SEND_CLICK_DELAY*1000 + rbClickServicesWaitMs)) {
      if (!rbClickHoldLogged) {
        rbClickHoldLogged = true;
        FUNCTIONLOG("RB Click", "Holding click while the startup services run");
      }
      return; // still held
    }
    clickDelayActive = false;
    if (ESP.getFreeHeap() <= MIN_MALLOC) {
      FUNCTIONLOG("Heap", "low heap (%u), refusing rb click task spawn", ESP.getFreeHeap());
      return;
    }
    // Copy URL to pass to task (task will delete it)
    char* urlCopy = new char[strlen(pendingClickUrl) + 1];
    if (urlCopy == nullptr) {
      FUNCTIONLOG("RB Click", "Failed to allocate memory for task");
      return;
    }
    strcpy(urlCopy, pendingClickUrl);
    // Spawn the background task (allow multiple concurrent tasks for different stations)
    if (xTaskCreatePinnedToCore(
      vTaskRadioBrowserClick,
      "rbClickTask",
      8192,  // Stack size - HTTPS needs more memory
      (void*)urlCopy,
      LOW_TASK_PRIORITY,     // Priority
      NULL,  // No handle tracking - task cleans up itself
      NETWORK_CORE
    ) != pdPASS) {
      delete[] urlCopy;
      FUNCTIONLOG("RB Click", "[Error] xTaskCreate failed for rbClickTask.");
    }
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
              remoteVer = line.substring(strlen(VERSIONSTRING));
              remoteVer.trim();
              break;
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
      char msgBuf[WEBSOCKET_BUFFER];
      if (remoteVer != String(RADIOVERSION)) {
        snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateavailable\":true,\"remoteVersion\":\"%s\"}", remoteVer.c_str());
      } else {
        snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateavailable\":false,\"remoteVersion\":\"%s\"}", remoteVer.c_str());
      }
      websocket.textAll(msgBuf);
    } else {
      char msgBuf[96];
      snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateerror\": \"HTTP code %d\"}", httpCode);
      websocket.textAll(msgBuf);
      http.end();
    }
  #endif //#ifdef UPDATEURL
}

void startOnlineUpdate() {
  #ifdef UPDATEURL
    String updateUrl = String(UPDATEURL) + String(FIRMWARE);
    FUNCTIONLOG("Online Update", "Online Update download URL: %s", updateUrl.c_str());
    WiFiClientSecure client;
    client.setInsecure(); // skip server cert validation
    HTTPClient http;
    http.begin(client, updateUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.addHeader("User-Agent", ESPFILEUPDATER_USERAGENT);
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      int contentLength = http.getSize();
      FUNCTIONLOG("Online Update", "Content-Length: %d", contentLength);
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
              display.updateProgress(l10n(L10N_MSG_UPD_FIRMWARE), (float)written / (float)contentLength);
            }
          }
          if (Update.end(true)) { // end(true) will finish and commit the update
            FUNCTIONLOG("Online Update", "Update successful! Rebooting.");
            utility.deleteMainwwwFile();
            websocket.textAll("{\"onlineupdatestatus\": \"Update successful, rebooting...\"}");
            delay(1000);
            ESP.restart();
          } else {
            char msgBuf[96];
            snprintf(msgBuf, sizeof(msgBuf), "{\"onlineupdateerror\": \"Update failed on end(): %s\"}", Update.errorString());
            FUNCTIONLOG("Online Update", "Update failed on end(): %s\"}", msgBuf);
            websocket.textAll(msgBuf);
          }
        } else {
          FUNCTIONLOG("Online Update", "Cannot begin update. Reboot then try again.");
          websocket.textAll("{\"onlineupdateerror\": \"Cannot begin update (reboot then try again)\"}");
        }
      } else {
        FUNCTIONLOG("Online Update", "Update failed. Invalid firmware size.");
        websocket.textAll("{\"onlineupdateerror\": \"Invalid firmware size\"}");
      }
    } else {
      FUNCTIONLOG("Online Update", "Failed to download firmware.");
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
  // PSRAM cache check: serve static WebUI files from PSRAM (no LittleFS reads)
  if (request->method() == HTTP_GET) {
    String url = request->url();
    const CachedFile* cf = netserver.getFileCache().find(url.c_str());
    if (cf) {
      if (cf->gzData) {
        AsyncWebServerResponse *response = request->beginResponse(200, cf->contentType, (const uint8_t*)cf->gzData, cf->gzSize);
        response->addHeader("Content-Encoding", "gzip");
        response->addHeader("Cache-Control", "max-age=60");
        request->send(response);
      } else {
        AsyncWebServerResponse *response = request->beginResponse(200, cf->contentType, (const uint8_t*)cf->data, cf->size);
        response->addHeader("Cache-Control", "max-age=60");
        request->send(response);
      }
      return;
    }
  }

  if (request->url()=="/emergency") { request->send(200, "text/html", emergency_form); return; }
  if (request->method() == HTTP_POST && request->url()=="/webboard" && !config.wwwFilesExist) { request->redirect("/"); ESP.restart(); return; }
  if (request->method() == HTTP_GET && request->url() == "/search") { handleSearch(request); return; }
  if (request->method() == HTTP_POST && request->url() == "/search") { handleSearchPost(request); return; }

  #ifdef UPDATEURL
    if (request->method() == HTTP_GET && request->url() == "/onlineupdatecheck") {
      xTaskCreatePinnedToCore([](void*) {
        checkForOnlineUpdate();
        #ifdef CORE_MONITOR
          FUNCTIONLOG("Core.HWM", "[%s] stack HWM: %u bytes", pcTaskGetName(NULL), uxTaskGetStackHighWaterMark(NULL)*4);
        #endif
        vTaskDelete(NULL);
      }, "checkForOnlineUpdateTask", 8192, nullptr, LOW_TASK_PRIORITY, nullptr, NETWORK_CORE);
      request->send(200, "text/plain", "Update check started"); return;
    }
    if (request->method() == HTTP_GET && request->url() == "/onlineupdatestart") {
      xTaskCreatePinnedToCore([](void*) { startOnlineUpdate(); vTaskDelete(NULL); }, "startOnlineUpdateTask", 16384, nullptr, NET_TASK_PRIORITY, nullptr, NETWORK_CORE);
      request->send(200, "text/plain", "Update started"); return;
    }
  #endif

  if (request->method() == HTTP_GET) {
    if (strcmp(request->url().c_str(), PLAYLIST_PATH) == 0 ||
        strcmp(request->url().c_str(), SSIDS_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_PATH) == 0 || 
        strcmp(request->url().c_str(), TMP_PATH) == 0 || 
        strcmp(request->url().c_str(), PLAYLIST_SD_PATH) == 0 || 
        strcmp(request->url().c_str(), INDEX_SD_PATH) == 0) {
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
    char varjsbuf[WEBSOCKET_BUFFER];
    char escapedRadioVersion[32];
    utility.escapeQuotes(RADIOVERSION, escapedRadioVersion, sizeof(escapedRadioVersion));
    char escapedGithubUrl[128];
    utility.escapeQuotes(GITHUBURL, escapedGithubUrl, sizeof(escapedGithubUrl));
    snprintf(varjsbuf, sizeof(varjsbuf),
      "var radioVersion='%s';\n"
      "var formAction='%s';\n"
      "var playMode='%s';\n"
      "var onlineUpdCapable=%s;\n"
      "var newVerAvailable=%s;\n"
      "var updateUrl='%s';\n"
      "var currentLocale='%s';\n"
      "var casetransform=%s;\n",
      escapedRadioVersion,
      (network.status == CONNECTED && config.wwwFilesExist) ? "webboard" : "",
      (network.status == CONNECTED) ? "player" : "ap",
      #ifdef UPDATEURL
        "true",
      #else
        "false",
      #endif
      (netserver.newVersionAvailable) ? "true" : "false",
      escapedGithubUrl,
      config.store.locale_webui,
      #ifdef WWW_CASETRANSFORM
        "true"
      #else
        "false"
      #endif
   );
    AsyncWebServerResponse *response = request->beginResponse(200, "application/javascript", varjsbuf);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
    return;
  }
  if (request->url() == "/visuals.json") {
    // Which visualisers this build can actually draw, as an id -> label map for the WebUI select.
    // The ids are vuStyle_e, the same numbers that travel in vustyle=<n> and come back in GETSCREEN,
    // so the select is generated from the device rather than hard-coded in the page.  A style that needs
    // PCM is omitted by a backend that has none - a VS1053 never sees samples - so needsPcm is the only
    // capability test here.  The labels are the same on every backend: the spectrum a VS1053 synthesises
    // is deliberately not called out any more.
    static const struct { uint8_t id; const char *label; bool needsPcm; } vuStyleNames[] = {
      { VU_STYLE_BARS,             "Bars",             false },
      { VU_STYLE_DIGITAL_LED,      "Digital LED",      false },
      { VU_STYLE_HISTORY,          "History",          false },
      { VU_STYLE_SPECTRUM_REFLECT, "Spectrum Reflect", false },
      { VU_STYLE_SPECTRUM_MIRROR,  "Spectrum Mirror",  false },
      { VU_STYLE_WAVE,             "Waveform",         true  },
      { VU_STYLE_LISSAJOUS,        "Lissajous",        true  },
    };
    char visualsbuf[320];
    int n = snprintf(visualsbuf, sizeof(visualsbuf), "{");
    for (uint8_t i = 0; i < sizeof(vuStyleNames) / sizeof(vuStyleNames[0]); i++) {
      #if !defined(USE_AUDIO_I2S)
        if (vuStyleNames[i].needsPcm) continue;
      #endif
      n += snprintf(visualsbuf + n, sizeof(visualsbuf) - n, "%s\"%u\":\"%s\"", (n > 1) ? "," : "", vuStyleNames[i].id, vuStyleNames[i].label);
    }
    snprintf(visualsbuf + n, sizeof(visualsbuf) - n, "}");
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", visualsbuf);
    response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    response->addHeader("Pragma", "no-cache");
    response->addHeader("Expires", "0");
    request->send(response);
    return;
  }
  if (request->url() == "/curated_variables.js") {
    char varjsbuf[128];
    #ifdef CURATED_LISTS
      char escapedName[128];
      utility.escapeQuotes(CURATED_LISTS, escapedName, sizeof(escapedName));
      char escapedLink[128];
      utility.escapeQuotes(CURATED_LISTS_LINK, escapedLink, sizeof(escapedLink));
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
  if (request->url() == "/themes.json") {
    String json = display.getThemeListJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    return;
  }
  if (request->url() == "/layouts.json") {
    String json = display.getLayoutListJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    return;
  }
  if (request->url() == "/fonts.json") {
    String json = display.getSystemFontListJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache");
    request->send(response);
    return;
  }
  if (request->url() == "/clockfonts.json") {
    String json = display.getClockFontListJson();
    AsyncWebServerResponse *response = request->beginResponse(200, "application/json", json);
    response->addHeader("Cache-Control", "no-cache");
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
  // Fallback: try LittleFS for files not in PSRAM cache ??check .gz variant first
  if (request->method() == HTTP_GET) {
    char fsPath[64];
    snprintf(fsPath, sizeof(fsPath), "/www%s", request->url().c_str());
    char gzPath[64];
    snprintf(gzPath, sizeof(gzPath), "%s.gz", fsPath);
    if (LittleFS.exists(gzPath)) {
      AsyncWebServerResponse *response = request->beginResponse(LittleFS, gzPath, mimeTypeForFile(request->url().c_str()));
      response->addHeader("Content-Encoding", "gzip");
      request->send(response);
      return;
    }
    if (LittleFS.exists(fsPath)) {
      request->send(LittleFS, fsPath);
      return;
    }
  }
  FUNCTIONLOG("Netserver", "Not Found (404 error): %s", request->url().c_str());
  request->send(404, "text/plain", "Not found");
}

void handleIndex(AsyncWebServerRequest * request) {
  if (!config.wwwFilesExist) {
    if (request->url()=="/" && request->method() == HTTP_GET) {
      if (request->hasArg("l")) {
        cmd.exec("locale_webui", request->arg("l").c_str(), 0, CommandSource::HttpUrl);
        request->send(200, "text/html", emptyfs_html);
        return;
      }
      request->send(200, "text/html", emptyfs_html); return;
    }
    if (request->url()=="/" && request->method() == HTTP_POST) {
      if (request->arg("ssid")!="" && request->arg("pass")!="") {
        char buf[80];
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "%s\t%s", request->arg("ssid").c_str(), request->arg("pass").c_str());
        request->redirect("/");
        utility.saveWifi(buf);
        return;
      }
      request->redirect("/"); 
      ESP.restart();
      return;
    }
    FUNCTIONLOG("Netserver", "Not Found (404 error): %s", request->url().c_str());
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
  #ifdef USE_SD
    // While the SD File Manager is open the root belongs to it: a second tab, a bookmark, a captive-portal
    // redirect or a Home Assistant link must not land on a player UI whose buttons are all refused.  A
    // redirect, not the page body: the manager then keeps ONE address for the whole session, so its own
    // reloads never depend on the device's state at that instant, and this response is a constant.
    if (filemanager.active() && strcmp(request->url().c_str(), "/") == 0) {
      request->redirect("/sdmanager.html");
      return;
    }
  #endif
  if (strcmp(request->url().c_str(), "/") == 0 && request->params() == 0) {
    if (network.status == CONNECTED) {
      // The root is the one response whose body depends on the mode (player, or the redirect above), and the
      // String overload of send() adds no cache headers at all - unlike the file responses, which the library
      // marks no-cache itself.  Marked explicitly, like every other dynamic response here.
      AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html);
      response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
      request->send(response);
    } else {
      request->redirect("/settings.html");
    }
    return;
  }
  if (network.status == CONNECTED) {
    int paramsNr = request->params();

    bool handledAny = false;
    bool shouldRedirect = false;
    bool shouldRestart = false;

    String sleepValue;
    if (request->hasArg("sleep")) {
      sleepValue = request->getParam("sleep")->value();
      if (request->hasArg("after")) {
        sleepValue += ",";
        sleepValue += request->getParam("after")->value();
      }
    }

    for (int i = 0; i < paramsNr; ++i) {
      const AsyncWebParameter* p = request->getParam(i);
      String command = p->name();
      if (command == "after") continue;

      String value = (command == "sleep" && sleepValue.length() > 0) ? sleepValue : p->value();
      if (cmd.exec(command.c_str(), value.c_str(), 0, CommandSource::HttpUrl)) {
        handledAny = true;
        if (command == "reset" || command == "clearfs") shouldRedirect = true;
        if (command == "clearfs") shouldRestart = true;
      }
    }

    if (handledAny) {
      if (shouldRedirect) {
        request->redirect("/");
        if (shouldRestart) { FUNCTIONLOG("REBOOT", "Reboot."); delay(100); ESP.restart(); }
        return;
      }
      request->send(200, "text/plain", "");
      return;
    }

    request->send(404, "text/plain", "Not found");
    
  } else {
    request->send(404, "text/plain", "Not found");
  }
}
