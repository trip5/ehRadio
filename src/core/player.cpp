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
#include "rgbled.h"
#include "utility.h"
#include "playlistvalidator.h"
#include "../locale/dsplocale.h"
#ifdef USE_SD
  #include "filemanager.h"
#endif

#ifdef USE_ES8311
  #include "../libraries/ES8311_Audio/es8311.h"
#endif

static constexpr uint32_t PLQ_SEND_DELAY = 100; // ms; xQueueSend timeout in player.sendCommand()

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
  Player::Player() {}
#endif

void Player::init() {
  BOOTLOGX("player.init\t");
  playerQueue=NULL;
  //_resumeFilePos = 0;
  playerQueue = xQueueCreate(10, sizeof(playerRequestParams_t));
  setOutputPins(false);
  delay(50);
  memset(_plError, 0, PLERR_LN);
  memset(burl, 0, sizeof(burl));
  if (MUTE_PIN!=255) pinMode(MUTE_PIN, OUTPUT);
  #if defined(USE_AUDIO_I2S)
    setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT, I2S_DIN, I2S_MCLK);
  #else
    // SPI.begin(); // started in config.init()
    if (VS1053_RST>0) ResetChip();
    begin(); // chip query + patch load now handled inside begin()
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
    es.setVolume((uint8_t)map(i2sVol_init, 0, VOLUME_SCALE, 0, 100)); /* Map I2S volume (0..VOLUME_SCALE) to codec volume 0..100 */
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

void Player::stopSync() {
  _stop();  // synchronous stop — closes the audio file so SD can be unmounted safely
}

// Close the audio stream without _stop()'s side effects (a "[stopped]" title and client notifications).
// The playlist validator drives the audio directly and must not churn the title between stations.
void Player::stopStream() {
  stopSong();
}

void Player::stopInfo() {
  //telnet.info();
  netserver.requestOnChange(MODE, 0);
}

// Lives here because this file already owns the display locale: duplicating dsplocale.h into another
// translation unit embeds a second copy of every locale table, which costs tens of KB of flash.
void Player::setReady() {
  setError("");
  #ifdef USE_SD
    if (network.status == SDOFFLINE && !sdman.ready) { config.setTitle(l10n(L10N_MSG_NO_SD_CARD)); return; }
  #endif
  if (config.station.url[0] != '\0') config.setTitle(l10n(L10N_MSG_READY));
}

bool Player::isConnecting() {
  return strcmp_P(config.station.title, l10n(L10N_MSG_CONNECT)) == 0;
}

const char* Player::validatingLabel() {
  return l10n(L10N_LBL_VALIDATING);
}

void Player::setError(const char *e) {
  strlcpy(_plError, e, PLERR_LN);
  if (hasError()) {
    config.setTitle(_plError);
    ERRORLOG("%s", e);
  }
}

// The host answered 404/410: the URL is gone, so the ladder would only replay a guaranteed failure.
// Cancel any running ladder and let the arm gate stop instead of re-arming.
void Player::noteFatalStream() {
  _fatalStream = true;
  network.lostPlaying = false;  // a dead URL is not a resume IOU; also stops ticks() re-spawning the ladder
  network.cancelStreamRetry();
  char errbuf[STATION_FIELD_LENGTH];
  snprintf_P(errbuf, sizeof(errbuf), l10n(L10N_MSG_CONNECT_ERROR), config.station.url);
  setError(errbuf);
}

void Player::_stop(bool alreadyStopped) {
  log_i("%s called", __func__);
  if (config.getMode()==PM_SDCARD && !alreadyStopped) config.sdResumePos = player.getFilePos();
  _status = STOPPED;
  _playingStationId = 0;
  setOutputPins(false);
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  // An empty url means the station table was deliberately emptied - an empty card, or a playlist with nothing in
  // it - and config.setTitle("") was just written for exactly that reason.  Announcing "[stopped]" over it is what
  // kept a title line alive on a card with nothing to play.
  const bool noStation = (config.station.url[0] == '\0');
  if (!hasError() && !noStation) config.setTitle((display.mode()==LOST || display.mode()==UPDATING)?"":l10n(L10N_MSG_STOPPED));
  config.station.bitrate = 0;
  config.setBitrateFormat(BF_UNKNOWN);
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
  if (strlen(file)==0 || true) return; //TODO Read TAGs (SD Mode) (may never be implemented because I2S already handles metadata correctly, the issue only exists on VS1053 non-MP3)
  #ifdef USE_SD
    connecttoFS(sdman,file);
    eofHeader = false;
    while(!eofHeader) Audio::loop();
    //netserver.requestOnChange(SDPOS, 0);
  #endif
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
      case PR_STOP: _muted = false; _stop(); break;
      case PR_PLAY: {
        if (requestP.payload>0) {
          config.setLastStation((uint16_t)requestP.payload);
        }
        // Manual next/prev/playlist pick/station selection clears mute; the EOF auto-advance keeps it.
        if (!autoAdvance) _muted = false;
        // Re-derived on every PR_PLAY, which is the only call site of _play(): an EOF advance must not wake the panel,
        // restart its countdown or force a mode change, because none of that was the user's doing.
        _autoAdvancePlay = autoAdvance;
        autoAdvance = false;
        _play((uint16_t)abs(requestP.payload)); 
        break;
      }
      case PR_TOGGLE: {
        toggle();
        break;
      }
      case PR_VOL: {
        _muted = false;   // explicit volume change is a user action: clear mute
        config.setVolume(requestP.payload);
        config.saveValueButWait(&config.store.volume, (uint8_t)requestP.payload, 3000);
        uint8_t i2sVol = volToI2S(requestP.payload);
        Audio::setVolume(i2sVol);
        #ifdef USE_ES8311
          es.setVolume((uint8_t)map(i2sVol, 0, VOLUME_SCALE, 0, 100)); /* Map I2S volume (already adjusted for station ovol) 0..VOLUME_SCALE -> codec 0..100 */
        #endif
        break;
      }
      #ifdef USE_SD
        case PR_CHECKSD: {
          // Debounced: one failed raw read is a busy card, not a removal, and acting on it dropped the mode to web
          // right after the SD File Manager closed on a freshly remounted card.
          if (config.getMode()==PM_SDCARD && !sdman.cardPresentStable()) {
            // The probe shares the bus with the file being read, so three misses mean "busy" as easily as "gone" -
            // and unmounting under an open handle strands the audio library with no way to report it.  Close the
            // file first, then ask once more with nothing reading the card.
            FUNCTIONLOG("SD", "card probe failed %u times - stopping the file before the verdict", (unsigned)sdman.cardGoneStrikes());
            stopSync();
            vTaskDelay(pdMS_TO_TICKS(50));  // let the card finish the block the closed read left in flight
            if (sdman.cardPresent()) {
              FUNCTIONLOG("SD", "card answers again - resuming the track");
              sendCommand({PR_PLAY, config.lastStation()});
            } else if (network.status == SDOFFLINE) {
              sdman.stop();  // changeMode() refuses to act offline, so this mode does it here
              display.putRequest(NEWMODE, PLAYER);
              setReady();
            } else {
              config.changeMode(PM_WEB);  // stops (already stopped), unmounts, re-indexes
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
  audioHandlers.tick();  // publishes the title line once a half has settled, or its partner has arrived

  #ifdef USE_SD
    // A card file that stops feeding prints no audio line at all, so this heartbeat is the record of when it stopped.
    // Re-armed on every entry, which makes the first line of a play land immediately.
    if (SD_PLAY_TRACE_MS > 0 && _status==PLAYING && isRunning() && config.getMode()==PM_SDCARD) {
      const uint32_t now = millis();
      if (_traceNextMs == 0 || (int32_t)(now - _traceNextMs) >= 0) {
        _traceNextMs = now + SD_PLAY_TRACE_MS;
        FUNCTIONLOG("SD", "play pos %u buffer %u card %u/%u", (unsigned)getFilePos(), (unsigned)inBufferFilled(),
            (unsigned)sdman.ready, (unsigned)sdman.cardGoneStrikes());
      }
    } else {
      _traceNextMs = 0;
    }
  #endif

  // Clear the "connecting" placeholder on player state
  if (isRunning() && isConnecting()) {
    config.setTitle("");
  }

  if (!playlistValidator.active() && !isRunning() && _status==PLAYING) {
    // Name the state whenever the engine goes down under a live PLAYING status.  A clean end of track passes through
    // here too - it clears m_f_running inside Audio::loop() a few statements earlier - so an in-flight EOF advance is
    // not a fault and gets no line of its own; the two [Audio.info] lines already record it.  Anything else is news.
    if (!autoAdvance) {
      #ifdef USE_SD
        FUNCTIONLOG("Player", "audio stopped while PLAYING: pos %u dur %u buffer %u mode %u card %u/%u",
            (unsigned)getFilePos(), (unsigned)getAudioFileDuration(), (unsigned)inBufferFilled(),
            (unsigned)config.getMode(), (unsigned)sdman.ready, (unsigned)sdman.cardGoneStrikes());
      #else
        FUNCTIONLOG("Player", "audio stopped while PLAYING: pos %u dur %u buffer %u mode %u",
            (unsigned)getFilePos(), (unsigned)getAudioFileDuration(), (unsigned)inBufferFilled(),
            (unsigned)config.getMode());
      #endif
    }
    // Stream died unexpectedly - trigger reconnection if WiFi is still up and this is not a dead URL
    if (!_fatalStream && config.getMode() == PM_WEB && WiFi.status() == WL_CONNECTED && !network.lostPlaying) {
      FUNCTIONLOG("Player", "Stream stopped unexpectedly. Starting reconnection attempts...");
      network.lostPlaying = true;
      // Launch retry task if not already running.  spawnStreamRetry() checks the create result, so a spawn that
      // failed on a fragmented heap cannot leave lostPlaying set with no task behind it - which used to close
      // this gate until a reboot (nothing else clears the flag except a fresh WiFi reconnect event).
      spawnStreamRetry();
    }
    _stop(true);
  }
  if (strlen(burl) > 0) browseUrl();
  playlistValidator.loop();
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
  if (config.store.lastStationUrl[0] != '\0') {
    uint16_t foundIdx = utility.findStationByUrl(config.store.lastStationUrl);
    if (foundIdx > 0) {
      sendCommand({PR_PLAY, foundIdx});
      return true;
    }
    return queueResolvedUrl(config.store.lastStationUrl);
  }

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
  #ifdef USE_SD
    // Nothing new starts while the SD File Manager is open
    if (filemanager.active()) {
      FUNCTIONLOG("SDFileManager", "play refused, SD File Manager is open");
      return;
    }
  #endif
  if (_status == PLAYING && stationId > 0 && stationId == _playingStationId) return;
  #ifdef USE_SD
    if(config.getMode()==PM_SDCARD && config.store.sdshuffle && stationId > 0 && !_shuffleWalkOwns) {
      _shuffleRestart(stationId);  // Selkecting track manually resets the shuffle
    }
    _shuffleWalkOwns = false;
  #endif
  setError("");
  _fatalStream = false;  // a new station clears a previous permanent-error stop
  setDefaults();
  audioHandlers.resetMetadata();  // a new station: no half of the previous one may pair with this one's
  remoteStationName = false;
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  if (!_autoAdvancePlay) config.setDspOn(1);   // setDspOn(true) wakes a blanked panel
  config.vuThreshold = 0;
  //display.putRequest(PSTOP);
  if (!_autoAdvancePlay) config.resetScreensaverTimers(SCREENSAVERSTARTUPDELAY);
  if (config.getMode()!=PM_SDCARD) {
    display.putRequest(PSTOP);
  }
  setOutputPins(false);
  //config.setTitle(config.getMode()==PM_WEB?const_PlConnect:"");
  if (!utility.loadStation(stationId)) return;
  config.setTitle(config.getMode()==PM_WEB?l10n(L10N_MSG_CONNECT):"[next track]");
  config.station.bitrate=0;
  config.setBitrateFormat(BF_UNKNOWN);

  _loadVol(config.store.volume);
  display.putRequest(DBITRATE);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
  bool isConnected = false;
  if (config.getMode()==PM_SDCARD && SD_CS!=255) {
    // Belt for the station table: a card-mode play with no file to point at must never reach the audio library,
    // which would try to open the mount point as a track and report a garbage format name.
    if (config.station.url[0] == '\0') {
      FUNCTIONLOG("SD", "nothing to play: station %u has no file (empty card?)", (unsigned)stationId);
      return;
    }
    uint32_t _t_cfs = millis();
    isConnected=connecttoFS(sdman,config.station.url,config.sdResumePos==0?_resumeFilePos:config.sdResumePos-player.sd_min);
    FUNCTIONLOG("SD", "connecttoFS: %lums", millis() - _t_cfs);
  } else {
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_WEB));
  }
  if (config.getMode()==PM_WEB) {
    uint32_t _t_cth = millis();
    isConnected=connecttohost(config.station.url);
    lastConnectMs = millis() - _t_cth;
    FUNCTIONLOG("Player", "connecttohost: %lums", (unsigned long)lastConnectMs);
  }
  if (isConnected) {
    _status = PLAYING;
    _playingStationId = stationId;
    _loadVol(config.store.volume);   // re-apply volume (respects _muted) after connect* — setDefaults() inside it must not win
    if (config.getMode()==PM_SDCARD) {
      config.sdResumePos = 0;
      config.saveValueButWait(&config.store.lastSdStation, stationId, 5000);  // an NVS write here stalls the loop
    } else {
      config.saveLastStationUrl(config.station.url);
    }
    //config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    if (!_autoAdvancePlay) display.putRequest(NEWMODE, PLAYER);   // NEWMODE PLAYER is what leaves the screensaver
    display.putRequest(PSTART);
    #ifdef RADIO_BROWSER_SEND_CLICKS
      if (config.getMode()==PM_WEB && !network.lostPlaying) radioBrowserSendClick(config.station.url);
    #endif
    network.lostPlaying = false;  // Clear flag - we're playing again!
    rgbled.playing();
    backlightControls.restart();
  } else {
    // Self-healing: if SD file vanished, force re-index so next/prev uses fresh index
    #ifdef USE_SD
      if (config.getMode()==PM_SDCARD && !sdman.exists(config.station.url)) {
        FUNCTIONLOG("SD", "File not found. Re-indexing.");
        display.putRequest(PSTOP);                   // clear playback screen
        display.putRequest(NEWMODE, SDCHANGE);       // show on-screen counter
        config.initSDPlaylist(true);
        display.putRequest(NEWMODE, PLAYER);         // restore player mode
        display.putRequest(NEWSTATION);
      }
    #endif
    ERRORLOG("Error connecting to %s", config.station.url);
    char errbuf[STATION_FIELD_LENGTH];
    snprintf_P(errbuf, sizeof(errbuf), l10n(L10N_MSG_CONNECT_ERROR), config.station.url);
    player.setError(errbuf);
    _muted = false;
    _stop(true);
    if (config.getMode() == PM_WEB && !network.lostPlaying) {
      network.noteConnectFailure(config.station.url);
    }
  };
}

void Player::browseUrl() {
  if (burl[0] == '\0') return;
  playUrl(burl);
  memset(burl, 0, sizeof(burl));
}

void Player::playUrl(const char* url) {
  setError("");
  _fatalStream = false;
  _muted = false;
  remoteStationName = true;
  audioHandlers.resetMetadata();
  if (audioHandlers.clearArtwork()) netserver.requestOnChange(ARTWORK, 0);
  config.setDspOn(1);
  resumeAfterUrl = _status==PLAYING;
  display.putRequest(PSTOP);
  setOutputPins(false);
  config.setTitle(l10n(L10N_MSG_CONNECT));
  uint32_t _t_ctu = millis();
  const bool _urlConnected = connecttohost(url);
  lastConnectMs = millis() - _t_ctu;
  if (_urlConnected) {
    _status = PLAYING;
    _loadVol(config.store.volume);
    config.saveLastStationUrl(url);
    config.setTitle("");
    netserver.requestOnChange(MODE, 0);
    setOutputPins(true);
    display.putRequest(PSTART);
    #ifdef RADIO_BROWSER_SEND_CLICKS
      if (!network.lostPlaying) radioBrowserSendClick(url);
    #endif
    rgbled.playing();
    backlightControls.restart();
  } else {
    ERRORLOG("Error connecting to %s", url);
    SET_PLAY_ERROR("Error connecting to %s", url);
    _muted = false;
    _stop(true);
    if (!network.lostPlaying) {
      network.noteConnectFailure(url);
    }
  }
}

// The shuffle walk:
// A Feistel network shuffle gives predictable shuffle without storing anything: the k-th track of a pass is computed from
// k and a per-pass key, and outputs >= n are skipped ("cycle-walking").  The domain is the smallest EVEN-bit power of two >= n,
// so fewer than half of all values are skipped and a step costs two checks on average.  Being a permutation it is invertible.

// The walk's raw value for counter x, before cycle-walking.  Four balanced Feistel rounds round the key.
uint32_t Player::_shuffleEnc(uint32_t x, uint32_t bits) {
    const uint32_t half = bits / 2;
    const uint32_t mask = (1ul << half) - 1ul;
    uint32_t l = (x >> half) & mask;
    uint32_t r = x & mask;
    for(uint32_t round = 0; round < 4; round++) {
        uint32_t f = (r ^ _shuffleKey ^ (0x9E3779B9ul * (round + 1ul))) * 2654435761ul;
        f ^= f >> 16;
        f &= mask;
        uint32_t t = l ^ f;
        l = r;
        r = t;
    }
    return ((l & mask) << half) | (r & mask);
}

// The first id this key would produce, without advancing the walk; -1 if the walk is degenerate.
int32_t Player::_shuffleFirst() {
    for(uint32_t c = 0; c < 4096; c++) {
        uint32_t v = _shuffleEnc(c, _shuffleBits);
        if(v < _shuffleN) return (int32_t)(v + 1);
    }
    return -1;
}

// Start a pass over the playlist.  Re-key until the pass does not OPEN with the track that just played, so the boundary
// between two passes cannot repeat a track either - the walk itself stays a permutation, nothing is dropped.
void Player::_shuffleStartPass(uint16_t avoidId) {
    for(uint8_t attempt = 0; attempt < 8; attempt++) {
        _shuffleKey = ((uint32_t)random(0x7FFFFFFF) << 1) ^ (uint32_t)micros() ^ ((uint32_t)millis() << 12);
        int32_t first = _shuffleFirst();
        if(first < 0 || (uint16_t)first != avoidId) break;
    }
    _shuffleRaw = 0;
    _shufflePass = 0;
    FUNCTIONLOG("Player", "shuffle pass over %u tracks", (unsigned)_shuffleN);
}

void Player::_shuffleRestart(uint16_t avoidId) {
    uint16_t n = utility.playlistLength();
    if(n < 2) return;
    _shuffleBits = 0;
    while(_shuffleBits < 32 && (1ul << _shuffleBits) < n) _shuffleBits++;
    if(_shuffleBits & 1) _shuffleBits++;  // a balanced Feistel split needs an even width
    _shuffleN = n;
    _shuffleStartPass(avoidId);
}

uint16_t Player::_shuffleNext() {
    uint16_t n = utility.playlistLength();
    if(n < 2) return n ? 1 : 0;  // nothing to shuffle
    if(_shuffleN != n) _shuffleRestart(config.lastStation());  // first shuffled next(), or the playlist changed
    if(_shufflePass >= _shuffleN) _shuffleStartPass(config.lastStation());  // that pass is complete
    for(uint16_t guard = 0; guard < 4096; guard++) {
        uint32_t v = _shuffleEnc(_shuffleRaw++, _shuffleBits);
        if(v < _shuffleN) { _shufflePass++; return (uint16_t)(v + 1); }
    }
    return (uint16_t)random(1, (long)n + 1);  // cannot happen: keep playing rather than stop
}

uint16_t Player::_shufflePrev() {
    uint16_t n = utility.playlistLength();
    if(n < 2) return n ? 1 : 0;
    if(_shuffleN != n) return 0;  // no walk for this playlist yet, so there is no "back"
    uint32_t c = (_shuffleRaw > 0) ? _shuffleRaw - 1 : 0;   // the counter that produced the current track
    uint16_t guard = 0;
    while(c > 0 && guard++ < 4096) {
        c--;
        uint32_t v = _shuffleEnc(c, _shuffleBits);
        if(v < _shuffleN) {
            _shuffleRaw = c + 1;  // a following next() reproduces the track we came from
            if(_shufflePass) _shufflePass--;
            return (uint16_t)(v + 1);
        }
    }
    return 0;  // this is the first track of the pass
}

void Player::prev() {
  network.cancelStreamRetry();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    uint16_t lastStation = config.lastStation();
    if (lastStation == 1) config.lastStation(utility.playlistLength()); else config.lastStation(lastStation-1);
  } else {
    uint16_t id = _shufflePrev();
    if (id) { _shuffleWalkOwns = true; config.lastStation(id); }  // 0 = nothing behind this one: it restarts the track
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::next() {
  network.cancelStreamRetry();
  if (config.getMode()==PM_WEB || !config.store.sdshuffle) {
    uint16_t lastStation = config.lastStation();
    if (lastStation == utility.playlistLength()) config.lastStation(1); else config.lastStation(lastStation+1);
  } else {
    uint16_t id = _shuffleNext();
    if (id) { _shuffleWalkOwns = true; config.lastStation(id); }
  }
  sendCommand({PR_PLAY, config.lastStation()});
}

void Player::toggle() {
  network.cancelStreamRetry();
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
    if (config.store.volume < VOLUME_SCALE) {
      setVol(config.store.volume + 1);
    } else {
      setVol(VOLUME_SCALE);
    }
  } else {
    if (config.store.volume > 0) {
      setVol(config.store.volume - 1);
    } else {
      setVol(0);
    }
  }
}

uint8_t Player::volToI2S(uint8_t volume) {
  #ifdef USE_ES8311
    // Apply gamma curve for ES3C28P to make low volumes more audible
    int maxIn = VOLUME_SCALE - config.station.ovol * 3;
    if (maxIn < 1) maxIn = 1; // avoid division by zero; treat invalid ovol as no reduction
    if (volume > (uint8_t)maxIn) volume = (uint8_t)maxIn;
    float vnorm = (float)volume / (float)maxIn; // 0..1
    if (vnorm < 0.0f) vnorm = 0.0f;
    if (vnorm > 1.0f) vnorm = 1.0f;
    // Apply a gamma curve (sqrt) to make low volumes more audible and the top end less aggressive
    const float gamma = 0.5f;
    float vout = powf(vnorm, gamma);
    int vol = (int)(vout * (float)VOLUME_SCALE + 0.5f);
    if (vol > VOLUME_SCALE) vol = VOLUME_SCALE;
    if (vol < 0) vol = 0;
    return (uint8_t)vol;
  #else
    int vol = map(volume, 0, VOLUME_SCALE - config.station.ovol * 3 , 0, VOLUME_SCALE);
    if (vol > VOLUME_SCALE) vol = VOLUME_SCALE;
    if (vol < 0) vol = 0;
    return vol;
  #endif
}

void Player::_loadVol(uint8_t volume) {
  uint8_t v = _muted ? 0 : volToI2S(volume);
  setVolume(v);
  #ifdef USE_ES8311
    es.setVolume((uint8_t)map(v, 0, VOLUME_SCALE, 0, 100));
  #endif
}

void Player::setVol(uint8_t volume) {
  player.sendCommand({PR_VOL, volume});
}

void Player::mute() {
  if (_muted) {
    _muted = false;
    _loadVol(_muteVol);
  } else {
    _muteVol = config.store.volume;
    _muted = true;
    _loadVol(config.store.volume);
  }
}
