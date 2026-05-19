#include "options.h"
#include <Arduino.h>
#include "backlightcontrols.h"
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
#include "utility.h"

CommandHandler cmd;

const char* CommandHandler::sourceName(CommandSource source) {
  switch (source) {
    case CommandSource::WebSocket: return "websocket";
    case CommandSource::HttpUrl:   return "http";
    case CommandSource::Mqtt:      return "mqtt";
    case CommandSource::Telnet:    return "telnet";
    default:                       return "unknown";
  }
}

bool CommandHandler::isBlockedForSource(const char *command, CommandSource source) const {
  if (source != CommandSource::HttpUrl && source != CommandSource::Mqtt && source != CommandSource::Telnet) {
    return false;
  }

  return cmdIs(command,
    "getindex", "getactive",
    "getsystem", "getscreen", "getlocale", "getcontrols", "getweather", "getmqtt", "getbattery",
    "curated_import", "loadindex", "loadplaylist",
    "newmode", "locale_webui",
    "irbtn", "chkid", "irclr"
  );
}

bool CommandHandler::exec(const char *command, const char *value, uint8_t cid, CommandSource source) {
  if (isBlockedForSource(command, source)) {
    return false;
  }

  /* Websockets for Player */
  if (cmdIs(command, "toggle"))      { player.toggle(); return true; }
  if (cmdIs(command, "prev"))        { player.prev(); return true; }
  if (cmdIs(command, "next"))        { player.next(); return true; }
  if (cmdIs(command, "voldown", "volumedown", "volm", "vol-")) { player.stepVol(false); return true; }
  if (cmdIs(command, "volup",   "volumeup",   "volp", "vol+")) { player.stepVol(true);  return true; }
  if (cmdIs(command, "newmode"))     { config.newConfigMode = atoi(value); netserver.requestOnChange(CHANGEMODE, cid); return true; }
  if (cmdIs(command, "balance"))     { int b = atoi(value); b = (b < -16) ? -16 : (b > 16 ? 16 : b); config.saveValueButWait(&config.store.balance, static_cast<int8_t>(b), 5000); player.setBalance(static_cast<int8_t>(b)); netserver.requestOnChange(BALANCE, 0); return true; }
  if (cmdIs(command, "treble"))      { int v = atoi(value); v = (v < -16) ? -16 : (v > 16 ? 16 : v); config.setTone(config.store.bass, config.store.middle, (int8_t)v); return true; }
  if (cmdIs(command, "middle"))      { int v = atoi(value); v = (v < -16) ? -16 : (v > 16 ? 16 : v); config.setTone(config.store.bass, (int8_t)v, config.store.treble); return true; }
  if (cmdIs(command, "bass"))        { int v = atoi(value); v = (v < -16) ? -16 : (v > 16 ? 16 : v); config.setTone((int8_t)v, config.store.middle, config.store.treble); return true; }
  if (cmdIs(command, "volume", "vol")) { int v = atoi(value); config.store.volume = v < 0 ? 0 : (v > 254 ? 254 : v); player.setVol(v); return true; }
  if (cmdIs(command, "turnoff"))     { bool sst = config.store.smartstart; config.setDspOn(false); backlightControls.restart(); player.sendCommand({PR_STOP, 0}); delay(100); config.saveValue(&config.store.smartstart, sst); return true; }
  if (cmdIs(command, "turnon"))      { config.setDspOn(true); backlightControls.restart(); if (config.store.smartstart) { if (config.getMode() == PM_WEB) player.resumeLastWebSource(); else player.sendCommand({PR_PLAY, config.lastStation()}); } return true; }
  if (cmdIs(command, "burl", "playurl")) {
    return player.queueResolvedUrl(value);
  }
  if (cmdIs(command, "sdpos")) {
    if (config.getMode()==PM_SDCARD) {
      uint32_t sdval = static_cast<uint32_t>(atoi(value)); config.sdResumePos = 0;
      if (!player.isRunning()) { player.setResumeFilePos(sdval-player.sd_min); player.sendCommand({PR_PLAY, config.store.lastSdStation}); }
      else { player.setFilePos(sdval-player.sd_min); }
    }
    return true;
  }
  if (cmdIs(command, "playstation", "play")) { uint16_t id = atoi(value); uint16_t cs = utility.playlistLength(); id = (id < 1) ? 1 : (id > cs ? cs : id); player.sendCommand({PR_PLAY, id}); return true; }
  if (cmdIs(command, "shuffle"))         { config.saveValue(&config.store.sdshuffle, static_cast<bool>(atoi(value))); if (config.store.sdshuffle) player.next(); return true; }
  if (cmdIs(command, "start"))           { if (config.getMode() == PM_WEB) return player.resumeLastWebSource(); player.sendCommand({PR_PLAY, config.lastStation()}); return true; }
  if (cmdIs(command, "stop"))            { player.sendCommand({PR_STOP, 0}); return true; }
  if (cmdIs(command, "sleep")) {
    int sfor = 0;
    int safter = 0;
    if (sscanf(value, "%d,%d", &sfor, &safter) != 2 && sscanf(value, "%d %d", &sfor, &safter) != 2) {
      if (sscanf(value, "%d", &sfor) != 1) return false;
      safter = 0;
    }
    if (sfor <= 0 || safter < 0) return false;
    utility.sleepForAfter(static_cast<uint16_t>(sfor), static_cast<uint16_t>(safter));
    return true;
  }
  if (cmdIs(command, "mode"))            { config.changeMode(atoi(value)); return true; }
  if (cmdIs(command, "submitplaylist"))  { player.sendCommand({PR_STOP, 0}); return true; }
  if (cmdIs(command, "submitplaylistdone")) {
    char currentUrl[STATION_FIELD_LENGTH];
    strncpy(currentUrl, config.station.url, STATION_FIELD_LENGTH);
    uint16_t newLen = utility.playlistLength();
    uint16_t foundIdx = utility.findStationByUrl(currentUrl);
    if (foundIdx > 0) {
      utility.loadStation(foundIdx);
    } else if (newLen > 0) {
      player.sendCommand({PR_STOP, 0});
      utility.loadStation(1);
    } else {
      player.sendCommand({PR_STOP, 0});
      config.setLastStation(0);
    }
    netserver.triggerMqttPlaylistSync();
    return true;
  }

  /* Hidden Websockets */
  if (cmdIs(command, "getindex"))    { netserver.requestOnChange(GETINDEX, cid); return true; }
  if (cmdIs(command, "getactive"))   { netserver.requestOnChange(GETACTIVE, cid); return true; }
  if (cmdIs(command, "clearspiffs")) { utility.cleanupSpiffs(); config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB)); return true; }

  /* Options: Load Settings */
  if (cmdIs(command, "getsystem"))   { netserver.requestOnChange(GETSYSTEM, cid); return true; }
  if (cmdIs(command, "getscreen"))   { netserver.requestOnChange(GETSCREEN, cid); return true; }
  if (cmdIs(command, "getlocale"))   { netserver.requestOnChange(GETLOCALE, cid); return true; }
  if (cmdIs(command, "getcontrols")) { netserver.requestOnChange(GETCONTROLS, cid); return true; }
  if (cmdIs(command, "getweather"))  { netserver.requestOnChange(GETWEATHER, cid); return true; }
  if (cmdIs(command, "getmqtt"))     { netserver.requestOnChange(GETMQTT, cid); return true; }
  if (cmdIs(command, "getbattery"))  { netserver.requestOnChange(GETBATTERY, cid); return true; }

  /* Options: System */
  if (cmdIs(command, "smartstart"))  { config.saveValue(&config.store.smartstart, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "audioinfo"))   { config.saveValue(&config.store.audioinfo, static_cast<bool>(atoi(value))); display.putRequest(AUDIOINFO); return true; }
  if (cmdIs(command, "vumeter"))     { config.saveValue(&config.store.vumeter, static_cast<bool>(atoi(value))); display.putRequest(SHOWVUMETER); return true; }
  if (cmdIs(command, "wifiscan"))    { config.saveValue(&config.store.wifiscanbest, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "autoupdate"))  { config.saveValue(&config.store.autoupdate, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "ehdp"))        { config.saveValue(&config.store.ehdp, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "ehdpname"))    { config.saveValue(config.store.ehdpname, value); network.ehDPinit(); return true; }
  if (cmdIs(command, "softap"))      { config.saveValue(&config.store.softapdelay, static_cast<uint8_t>(atoi(value))); return true; }
  if (cmdIs(command, "mdnsname"))    { config.saveValue(config.store.mdnsname, value); netserver.restartMdns(); return true; }

  /* Options: Battery */
  if (cmdIs(command, "battref"))     { if (battery.calibrate(atoi(value))) netserver.requestOnChange(GETBATTERY, cid); return true; }
  if (cmdIs(command, "battrecalc"))  { battery.recalcNow(); netserver.requestOnChange(GETBATTERY, cid); return true; }

  /* Options: Screen */
  if (cmdIs(command, "flipscreen"))    { config.saveValue(&config.store.flipscreen, static_cast<bool>(atoi(value))); display.flip(); display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER); return true; }
  if (cmdIs(command, "invertdisplay")) { config.saveValue(&config.store.invertdisplay, static_cast<bool>(atoi(value))); return true; } //display.invert();
  if (cmdIs(command, "numplaylist"))   { config.saveValue(&config.store.numplaylist, static_cast<bool>(atoi(value))); display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER); return true; }
  if (cmdIs(command, "clock12"))       { config.saveValue(&config.store.clock12, static_cast<bool>(atoi(value))); display.putRequest(CLOCK); return true; }
  if (cmdIs(command, "volumepage"))    { config.saveValue(&config.store.volumepage, static_cast<bool>(atoi(value))); display.putRequest(NEWMODE, PLAYER); return true; }
  if (cmdIs(command, "brightness", "dim")) {
    if (!config.store.dspon) netserver.requestOnChange(DSPON, 0);
    int bri=atoi(value);
    config.store.brightness = (uint8_t)(bri < 0 ? 0 : (bri > 100 ? 100 : bri));
    if (config.store.dimmingBrightness > config.store.brightness) {
      config.store.dimmingBrightness = config.store.brightness;
      config.saveValueButWait(&config.store.dimmingBrightness, config.store.dimmingBrightness, 4000);
    }
    config.setBrightness(true);
    backlightControls.restart();
    return true;
  }
  if (cmdIs(command, "screenon", "dspon"))    { config.setDspOn(static_cast<bool>(atoi(value))); backlightControls.restart(); return true; }
  if (cmdIs(command, "contrast"))      { int con=atoi(value); config.saveValueButWait(&config.store.contrast, (uint8_t)(con < 0 ? 0 : (con > 100 ? 100 : con)), 4000); display.setContrast(); return true; }
  /* De-deplicated helper for screensaver / No-op for LCDs */
  auto screensaverHelper = []() {
    #ifndef DSP_LCD
      display.putRequest(NEWMODE, PLAYER);
    #endif
  };
  if (cmdIs(command, "screensaverenabled"))        { config.saveValue(&config.store.screensaverEnabled, static_cast<bool>(atoi(value))); screensaverHelper(); return true; }
  if (cmdIs(command, "screensavertimeout"))        { config.saveValue(&config.store.screensaverTimeout, static_cast<uint16_t>(constrain(atoi(value), 5, 65520))); screensaverHelper(); return true; }
  if (cmdIs(command, "screensaverblank"))          { config.saveValue(&config.store.screensaverBlank, static_cast<bool>(atoi(value))); screensaverHelper(); return true; }
  if (cmdIs(command, "screensaverplayingenabled")) { config.saveValue(&config.store.screensaverPlayingEnabled, static_cast<bool>(atoi(value))); screensaverHelper(); return true; }
  if (cmdIs(command, "screensaverplayingtimeout")) { config.saveValue(&config.store.screensaverPlayingTimeout, static_cast<uint16_t>(constrain(atoi(value), 1, 1080))); screensaverHelper(); return true; }
  if (cmdIs(command, "screensaverplayingblank"))   { config.saveValue(&config.store.screensaverPlayingBlank, static_cast<bool>(atoi(value))); screensaverHelper(); return true; }
  if (cmdIs(command, "dimmingenabled"))            { config.saveValue(&config.store.dimmingEnabled, static_cast<bool>(atoi(value))); backlightControls.restart(); return true; }
  if (cmdIs(command, "dimmingbrightness"))         {
    int dimbr=atoi(value);
    const uint8_t clampedBrightness = static_cast<uint8_t>(dimbr < 0 ? 0 : (dimbr > 100 ? 100 : dimbr));
    const uint8_t effectiveBrightness = (clampedBrightness > config.store.brightness) ? config.store.brightness : clampedBrightness;
    config.store.dimmingBrightness = effectiveBrightness;
    config.saveValueButWait(&config.store.dimmingBrightness, config.store.dimmingBrightness, 4000);
    backlightControls.restart();
    return true;
  }
  if (cmdIs(command, "dimmingtimeout"))            { config.saveValue(&config.store.dimmingTimeout, static_cast<uint16_t>(constrain(atoi(value), 5, 65520))); backlightControls.restart(); return true; }

  /* Options: Controls */
  if (cmdIs(command, "volsteps"))          { config.saveValue(&config.store.volsteps, static_cast<uint8_t>(atoi(value))); return true; }
  if (cmdIs(command, "fliptouch"))         { config.saveValue(&config.store.fliptouch, static_cast<bool>(atoi(value))); controls.flipTS(); return true; }
  if (cmdIs(command, "dbgtouch"))          { config.saveValue(&config.store.dbgtouch, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "encacc"))            { controls.setEncAcceleration(static_cast<uint16_t>(atoi(value))); return true; }
  if (cmdIs(command, "oneclickswitching")) { config.saveValue(&config.store.skipPlaylistUpDown, static_cast<bool>(atoi(value))); return true; }
  if (cmdIs(command, "irtlp"))             { controls.setIRTolerance(static_cast<uint8_t>(atoi(value))); return true; }

  /* Options: Locale */
  if (cmdIs(command, "locale_webui")) { utility.updateLocaleFileAsync(value, cid); return true; }
  if (cmdIs(command, "tz_name"))      { config.saveValue(config.store.tz_name, value); return true; }
  if (cmdIs(command, "tzposix"))      { config.saveValue(config.store.tzposix, value); network.forceTimeSync = true; network.requestTimeSync(true); return true; }
  if (cmdIs(command, "sntp1"))        { config.saveValue(config.store.sntp1, value); network.forceTimeSync = true; network.requestTimeSync(true); return true; }
  if (cmdIs(command, "sntp2"))        { config.saveValue(config.store.sntp2, value); return true; }
  if (cmdIs(command, "timeinterval")) { config.saveValue(&config.store.timesyncinterval, static_cast<uint8_t>(atoi(value))); return true; }

  /* Options: Weather */
  if (cmdIs(command, "wenable"))       { config.saveValue(&config.store.showweather, static_cast<bool>(atoi(value))); network.forceWeather=true; display.putRequest(SHOWWEATHER); return true; }
  if (cmdIs(command, "wen_feelslike")) { config.saveValue(&config.store.weatherfeels, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wen_humidity"))  { config.saveValue(&config.store.weatherhumidity, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wen_pressure"))  { config.saveValue(&config.store.weatherpressure, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wen_wind"))      { config.saveValue(&config.store.weatherwind, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wtempunit"))     { config.saveValue(&config.store.weathertempimp, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wpressunit"))    { config.saveValue(&config.store.weatherpressimp, (atoi(value) != 0)); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wspeedunit"))    { config.saveValue(config.store.weatherwindspeed, value); network.buildWeatherString(); return true; }
  if (cmdIs(command, "wapi"))          { config.saveValue(config.store.weatherapi, value); network.forceWeather = true; return true; }
  if (cmdIs(command, "wlang"))         { config.saveValue(config.store.weatherlang, value); network.forceWeather = true; return true; }
  if (cmdIs(command, "wkey"))          { config.saveValue(config.store.weatherkey, value); display.putRequest(NEWMODE, CLEAR); display.putRequest(NEWMODE, PLAYER); return true; }
  if (cmdIs(command, "winterval"))     { config.saveValue(&config.store.weathersyncinterval, static_cast<uint8_t>(atoi(value))); return true; }
  if (cmdIs(command, "wlat"))          { config.saveValue(config.store.weatherlat, value); config.store.weatherelevation = 0; config.saveValue(&config.store.weatherelevation, static_cast<int16_t>(0)); network.forceWeather = true; return true; }
  if (cmdIs(command, "wlon"))          { config.saveValue(config.store.weatherlon, value); config.store.weatherelevation = 0; config.saveValue(&config.store.weatherelevation, static_cast<int16_t>(0)); network.forceWeather = true; return true; }

  /* Options: MQTT */
  #ifdef MQTT_ENABLE
    if (cmdIs(command, "mqttenable")) { config.saveValue(&config.store.mqttenable, static_cast<bool>(atoi(value))); mqtt.init(); return true; }
    if (cmdIs(command, "mqtthost"))   { config.saveValue(config.store.mqtthost, value); return true; }
    if (cmdIs(command, "mqttport"))   { config.saveValue(&config.store.mqttport, static_cast<uint16_t>(atoi(value))); return true; }
    if (cmdIs(command, "mqttuser"))   { config.saveValue(config.store.mqttuser, value); return true; }
    if (cmdIs(command, "mqttpass"))   { config.saveValue(config.store.mqttpass, value); return true; }
    if (cmdIs(command, "mqtttopic"))  { config.saveValue(config.store.mqtttopic, value); return true; }
  #endif

  /* Options: Danger Zone */
  if (cmdIs(command, "reboot", "boot"))  { ESP.restart(); return true; }
  if (cmdIs(command, "format"))  { player.sendCommand({PR_STOP, 0}); SPIFFS.format(); ESP.restart(); return true; }
  if (cmdIs(command, "reset"))   { config.defaultSettings(value, cid); return true; } // also used by Section resets

  /* IR Recorder */
  #if IR_PIN!=255
    if (cmdIs(command, "irbtn"))  { config.irindex = atoi(value); netserver.irRecordEnable = (config.irindex >= 0); config.irchck = 0; netserver.irValsToWs(); if (config.irindex < 0) config.saveIR(); return true; }
    if (cmdIs(command, "chkid"))  { config.irchck = static_cast<uint8_t>(atoi(value)); return true; }
    if (cmdIs(command, "irclr"))  { if (config.irindex < 0 || config.irindex >= 20) return true; int irslot = atoi(value); if (irslot < 0 || irslot > 2) return true; config.ircodes.irVals[config.irindex][irslot] = 0; return true; }
  #endif

  /* Curated Playlists */
  if (cmdIs(command, "loadindex")) {
    extern volatile TaskHandle_t g_curatedTaskHandle;
    extern portMUX_TYPE taskSpawnMux;
    if (ESP.getFreeHeap() > MIN_MALLOC) {
      taskENTER_CRITICAL(&taskSpawnMux);
      if (g_curatedTaskHandle == NULL) {
        xTaskCreatePinnedToCore(vTaskFetchCuratedIndex, "curatedIndex", 8192, NULL, LOW_TASK_PRIORITY, (TaskHandle_t*)&g_curatedTaskHandle, NETWORK_CORE);
      }
      taskEXIT_CRITICAL(&taskSpawnMux);
    }
    return true;
  }
  if (cmdIs(command, "loadplaylist")) {
    extern volatile TaskHandle_t g_curatedTaskHandle;
    extern portMUX_TYPE taskSpawnMux;
    if (ESP.getFreeHeap() > MIN_MALLOC) {
      taskENTER_CRITICAL(&taskSpawnMux);
      if (g_curatedTaskHandle == NULL) {
        char* filename = new char[strlen(value) + 1];
        if (filename) {
          strcpy(filename, value);
          if (xTaskCreatePinnedToCore(vTaskFetchCuratedPlaylist, "curatedPlaylist", 8192, filename, LOW_TASK_PRIORITY, (TaskHandle_t*)&g_curatedTaskHandle, NETWORK_CORE) != pdPASS) {
            delete[] filename;
          }
        }
      }
      taskEXIT_CRITICAL(&taskSpawnMux);
    }
    return true;
  }
  if (cmdIs(command, "curated_import")) {
    // Import the downloaded playlist file (pl_import.json)
    // Value is "replace" or "merge"
    // This prepares the file for review but doesn't save permanently yet
    bool isReplace = (strcmp(value, "replace") == 0);
    // Copy pl_import.json to tmp_pl for editing
    if (SPIFFS.exists("/www/pl_import.json")) {
      SPIFFS.remove(TMP_PATH);
      File src = SPIFFS.open("/www/pl_import.json", "r");
      File dst = SPIFFS.open(TMP_PATH, "w");
      if (src && dst) {
        uint8_t buffer[512];
        while (src.available()) {
          size_t len = src.read(buffer, sizeof(buffer));
          dst.write(buffer, len);
        }
        src.close();
        dst.close();
        FUNCTIONLOG("Curated", "Prepared playlist for review (mode: %s)", value);
        // Send signal to frontend to open editor with this file
        char msgbuf[64];
        snprintf(msgbuf, sizeof(msgbuf), "{\"curated_ready\":true,\"mode\":\"%s\"}", value);
        websocket.text(cid, msgbuf);
      } else {
        FUNCTIONLOG("Curated", "Failed to prepare playlist");
        websocket.text(cid, "{\"curated_failed\":true}");
      }
    } else {
      FUNCTIONLOG("Curated", "pl_import.json not found");
      websocket.text(cid, "{\"curated_failed\":true}");
    }
    return true;
  }

/* end of commandHandler */
  return false;
}

