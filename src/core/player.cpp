#include "options.h"
#include "player.h"
#include "audiohandlers.h"
#include "config.h"
#include "logging.h"
#include "telnet.h"
#include "display.h"
#include "sdmanager.h"
#include "backlightcontrols.h"
#include "netserver.h"
#include "network.h"
#include "locale.h"
#include "rgbled.h"
#include "utility.h"

#ifdef USE_ES8311
  #include "../libraries/ES8311_Audio/es8311.h"
#endif
#ifdef USE_NEXTION
  #include "../displays/nextion.h"
#endif

#define NETWORK_TASK_STACK_BYTES (NETWORK_TASK_STACK_SIZE * 1024)

Player player;
QueueHandle_t playerQueue;

#if defined(USE_AUDIO_VS1053)
  Player::Player(): Audio(VS1053_CS, VS1053_DCS, VS1053_DREQ, &VS1053_SPIBUS) {}
  void ResetChip() {
    pinMode(VS1053_RST, OUTPUT);
    digitalWrite(VS1053_RST, LOW);
    delay(30);
    digitalWrite(VS1053_RST, HIGH);
    delay(100);
  }
#else
  #ifndef USE_AUDIO_ESP32_DAC
    Player::Player() {}
  #else
    Player::Player(): Audio(true, I2S_DAC_CHANNEL_BOTH_EN)  {}
  #endif
#endif

void Player::init() {
  BOOTLOGX("player.init\t");
  playerQueue=NULL;
  //_resumeFilePos = 0;
  playerQueue = xQueueCreate(5, sizeof(playerRequestParams_t));
  setOutputPins(false);
  delay(50);
  memset(_plError, 0, PLERR_LN);
  memset(burl, 0, sizeof(burl));
  if (MUTE_PIN!=255) pinMode(MUTE_PIN, OUTPUT);
  #if defined(USE_AUDIO_I2S) || defined(USE_AUDIO_ESP32_DAC)
    #ifndef USE_AUDIO_ESP32_DAC
      setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_DIN, I2S_MCLK);
    #endif
  #else
    // SPI.begin(); // started in config.init()
    if (VS1053_RST>0) ResetChip();
    begin();
  #endif
  setAudioTaskCore(AUDIO_CORE);
  #if defined(USE_ES8311) && defined(ES8311_I2C_SDA) && defined(ES8311_I2C_SCL)
    // leaving this here but it seems not needed?  (this is the only operation that uses I2C on ES8311)
    if (es.begin(ES8311_I2C_SDA, ES8311_I2C_SCL, 400000UL)) es.setVolume(0); /* Start codec muted (or low) to avoid very loud output before saved volume is applied */
  #endif
  setBalance(config.store.balance);
  setTone(config.store.bass, config.store.middle, config.store.treble);
  setVolume(0);
  //_status = STOPPED;
  ////setOutputPins(false);
  //_volTimer=false;
  ////randomSeed(analogRead(0));
  #if PLAYER_FORCE_MONO
    forceMono(true);
  #endif
  _loadVol(config.store.volume);
  #ifdef USE_ES8311
    uint8_t i2sVol_init = volToI2S(config.store.volume); /* Also apply stored volume to codec (respecting station ovol via volToI2S) */
    es.setVolume((uint8_t)map(i2sVol_init, 0, 254, 0, 100)); /* Map I2S volume (0..254) to codec volume 0..100 */
  #endif
  #ifdef CONNECT_HTTP_HTTPS_TIMEOUT // macro must be two numbers separated by a comma, ie: 1700, 3700
    setConnectionTimeout(CONNECT_HTTP_HTTPS_TIMEOUT);
  #endif
  if (rgbled.isInitialized()) rgbled.stopped();
  SERIALLOG("done");
}

void Player::sendCommand(playerRequestParams_t request) {
  if (playerQueue==NULL) return;
  if (xQueueSend(playerQueue, &request, pdMS_TO_TICKS(PLQ_SEND_DELAY)) != pdTRUE) {
    FUNCTIONLOG("Queue", "playerQueue overflow, dropped cmd=%d", request.type);
  }
}

void Player::resetQueue() {
	if (playerQueue!=NULL) xQueueReset(playerQueue);
}

void Player::stopInfo() {
  //telnet.info();
  netserver.requestOnChange(MODE, 0);
}

void Player::setError(const char *e) {
  strlcpy(_plError, e, PLERR_LN);
  if (hasError()) {
    config.setTitle(_plError);
    ERRORLOG("%s", e);
  }
}

void Player::_stop(bool alreadyStopped) {
  log_i("%s called", __func__);
  if (config.getMode()==PM_SDCARD && !alreadyStopped) config.sdResumePos = player.getFilePos();
  _status = STOPPED;
  setOutputPins(false);
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  if (!hasError()) config.setTitle((display.mode()==LOST || display.mode()==UPDATING)?"":LANG::const_PlStopped);
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNKNOWN);
  #ifdef USE_NEXTION
    nextion.bitrate(config.station.bitrate);
  #endif
  netserver.requestOnChange(BITRATE, 0);
  display.putRequest(DBITRATE);
  display.putRequest(PSTOP);
  setDefaults();
  if (!alreadyStopped) stopSong();
  if (!lockOutput) stopInfo();
  rgbled.stopped();
  backlightControls.restart();
}

void Player::initHeaders(const char *file) {
  if (strlen(file)==0 || true) return; //TODO Read TAGs
  connecttoFS(sdman,file);
  eofHeader = false;
  while(!eofHeader) Audio::loop();
  //netserver.requestOnChange(SDPOS, 0);
  setDefaults();
}

#ifndef PL_QUEUE_TICKS
  #define PL_QUEUE_TICKS 0
#endif
#ifndef PL_QUEUE_TICKS_ST
  #define PL_QUEUE_TICKS_ST 15
#endif
void Player::loop() {
  if (playerQueue==NULL) return;
  playerRequestParams_t requestP;
  if (xQueueReceive(playerQueue, &requestP, isRunning()?PL_QUEUE_TICKS:PL_QUEUE_TICKS_ST)) {
    switch (requestP.type) {
      case PR_STOP: _stop(); break;
      case PR_PLAY: {
        if (requestP.payload>0) {
          config.setLastStation((uint16_t)requestP.payload);
        }
        _play((uint16_t)abs(requestP.payload)); 
        break;
      }
      case PR_TOGGLE: {
        toggle();
        break;
      }
      case PR_VOL: {
        config.setVolume(requestP.payload);
        config.saveValueButWait(&config.store.volume, (uint8_t)requestP.payload, 3000);
        uint8_t i2sVol = volToI2S(requestP.payload);
        Audio::setVolume(i2sVol);
        #ifdef USE_ES8311
          es.setVolume((uint8_t)map(i2sVol, 0, 254, 0, 100)); /* Map I2S volume (already adjusted for station ovol) 0..254 -> codec 0..100 */
        #endif
        break;
      }
      #ifdef USE_SD
        case PR_CHECKSD: {
          if (config.getMode()==PM_SDCARD) {
            if (!sdman.cardPresent()) {
              sdman.stop();
              config.changeMode(PM_WEB);
            }
          }
          break;
        }
      #endif
      case PR_VUTONUS: {
        if (config.vuThreshold>10) config.vuThreshold -=10;
        break;
      }
      case PR_BURL: {
        if (strlen(burl) > 0) browseUrl();
        break;
      }
      default: break;
    }
  }
  Audio::loop();
  if (!isRunning() && _status==PLAYING) {
    // Stream died unexpectedly - trigger reconnection if WiFi is still up
    if (WiFi.status() == WL_CONNECTED && !network.lostPlaying) {
      FUNCTIONLOG("Player", "Stream stopped unexpectedly. Starting reconnection attempts...");
      network.lostPlaying = true;
      // Launch retry task if not already running
      if (streamRetryTaskHandle == NULL) {
        xTaskCreatePinnedToCore(retryStreamConnection, "streamRetry", NETWORK_TASK_STACK_BYTES, NULL, NET_TASK_PRIORITY, &streamRetryTaskHandle, NETWORK_CORE);
      }
    }
    _stop(true);
  }
  if (strlen(burl) > 0) browseUrl();
}

bool Player::queueResolvedUrl(const char* url) {
  if (url == nullptr || url[0] == '\0') return false;
  if (strncmp(url, "http://", 7) != 0 && strncmp(url, "https://", 8) != 0) return false;

  uint16_t foundIdx = utility.findStationByUrl(url);
  if (foundIdx > 0) {
    sendCommand({PR_PLAY, foundIdx});
    return true;
  }

  if (strlen(url) >= sizeof(burl)) return false;
  strlcpy(burl, url, sizeof(burl));
  sendCommand({PR_BURL, 0});
  return true;
}

bool Player::resumeLastWebSource() {
  if (config.hasLastStationUrl() && queueResolvedUrl(config.getLastStationUrl())) return true;

  uint16_t stationId = config.lastStation();
  if (stationId == 0) return false;

  sendCommand({PR_PLAY, stationId});
  return true;
}

void Player::setOutputPins(bool isPlaying) {
  if (LED_PIN!=255) digitalWrite(LED_PIN, LED_INVERT?!isPlaying:isPlaying);
  bool _ml = MUTE_LOCK?!MUTE_VAL:(isPlaying?!MUTE_VAL:MUTE_VAL);
  if (MUTE_PIN!=255) digitalWrite(MUTE_PIN, _ml);
}

void Player::_play(uint16_t stationId) {
  log_i("%s called, stationId=%d", __func__, stationId);
  setError("");
  setDefaults();
  remoteStationName = false;
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  config.setDspOn(1);
  config.vuThreshold = 0;
  //display.putRequest(PSTOP);
  config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
  config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
  if (config.getMode()!=PM_SDCARD) {
    display.putRequest(PSTOP);
  }
  setOutputPins(false);
  //config.setTitle(config.getMode()==PM_WEB?const_PlConnect:"");
  if (!utility.loadStation(stationId)) return;
  config.setTitle(config.getMode()==PM_WEB?LANG::const_PlConnect:"[next track]");
  config.station.bitrate=0;
  config.setBitrateFormat(BF_UNKNOWN);
  
  _loadVol(config.store.volume);
  display.putRequest(DBITRATE);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
  bool isConnected = false;
  if (config.getMode()==PM_SDCARD && SD_CS!=255) {
    isConnected=connecttoFS(sdman,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min);
  } else {
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  if (config.getMode()==PM_WEB) {
    isConnected=connecttohost(config.station.url);
    // Note: Removed blind retry - if connection fails (timeout/404/refused), retrying 1.5s later
    // won't help and just adds 20+ seconds of delay. Stream reconnection is now handled by the
    // retryStreamConnection task which monitors for unexpected disconnects during playback.
  }
  if (isConnected) {
  //if (config.store.play_mode==PM_WEB?connecttohost(config.station.url):connecttoFS(SD,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min)) {
    _status = PLAYING;
    if (config.getMode()==PM_SDCARD) {
      config.sdResumePos = 0;
      config.saveValue(&config.store.lastSdStation, stationId);
    } else {
      config.setLastStationUrl(config.station.url);
    }
    //config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(NEWMODE, PLAYER);
    display.putRequest(PSTART);
    network.lostPlaying = false;  // Clear flag - we're playing again!
    #ifdef RADIO_BROWSER_SEND_CLICKS
      if (config.getMode()==PM_WEB) radioBrowserSendClick(config.station.url);
    #endif
    rgbled.playing();
    backlightControls.restart();
  } else {
    ERRORLOG("Error connecting to %s", config.station.url);
    SET_PLAY_ERROR("Error connecting to %s", config.station.url);
    _stop(true);
  };
}

void Player::browseUrl() {
  if (burl[0] == '\0') return;
  playUrl(burl);
  memset(burl, 0, sizeof(burl));
}

void Player::playUrl(const char* url) {
  setError("");
  remoteStationName = true;
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  config.setDspOn(1);
  resumeAfterUrl = _status==PLAYING;
  display.putRequest(PSTOP);
  setOutputPins(false);
  config.setTitle(LANG::const_PlConnect);
  if (connecttohost(url)) {
    _status = PLAYING;
    config.setLastStationUrl(url);
    config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    #ifdef RADIO_BROWSER_SEND_CLICKS
      radioBrowserSendClick(url);
    #endif
    rgbled.playing();
    backlightControls.restart();
  } else {
    ERRORLOG("Error connecting to %s", url);
    SET_PLAY_ERROR("Error connecting to %s", url);
    _stop(true);
  }
}

void Player::prev() {
  uint16_t lastStation = config.lastStation();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    if (lastStation == 1) config.lastStation(utility.playlistLength()); else config.lastStation(lastStation-1);
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::next() {
  uint16_t lastStation = config.lastStation();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    if (lastStation == utility.playlistLength()) config.lastStation(1); else config.lastStation(lastStation+1);
  } else {
    config.lastStation(random(1, utility.playlistLength()));
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::toggle() {
  if (_status == PLAYING) {
    sendCommand({PR_STOP, 0});
  } else {
    if (config.getMode() == PM_WEB) {
      resumeLastWebSource();
    } else {
      sendCommand({PR_PLAY, config.lastStation()});
    }
  }
}

void Player::stepVol(bool up) {
  if (up) {
    if (config.store.volume <= 254 - config.store.volsteps) {
      setVol(config.store.volume + config.store.volsteps);
    } else {
      setVol(254);
    }
  } else {
    if (config.store.volume >= config.store.volsteps) {
      setVol(config.store.volume - config.store.volsteps);
    } else {
      setVol(0);
    }
  }
}

uint8_t Player::volToI2S(uint8_t volume) {
  #ifdef USE_ES8311
    // Apply gamma curve for ES3C28P to make low volumes more audible
    int maxIn = 254 - config.station.ovol * 3;
    if (maxIn < 1) maxIn = 1; // avoid division by zero; treat invalid ovol as no reduction
    if (volume > (uint8_t)maxIn) volume = (uint8_t)maxIn;
    float vnorm = (float)volume / (float)maxIn; // 0..1
    if (vnorm < 0.0f) vnorm = 0.0f;
    if (vnorm > 1.0f) vnorm = 1.0f;
    /* Apply gamma curve (sqrt) to make low volumes more audible and top end less aggressive */
    const float gamma = 0.5f;
    float vout = powf(vnorm, gamma);
    int vol = (int)(vout * 254.0f + 0.5f);
    if (vol > 254) vol = 254;
    if (vol < 0) vol = 0;
    return (uint8_t)vol;
  #else
    int vol = map(volume, 0, 254 - config.station.ovol * 3 , 0, 254);
    if (vol > 254) vol = 254;
    if (vol < 0) vol = 0;
    return vol;
  #endif
}

void Player::_loadVol(uint8_t volume) {
  setVolume(volToI2S(volume));
}

void Player::setVol(uint8_t volume) {
  player.sendCommand({PR_VOL, volume});
}
