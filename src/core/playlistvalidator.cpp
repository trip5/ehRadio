#include "options.h"
#include "playlistvalidator.h"
#include "config.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "utility.h"

PlaylistValidator playlistValidator;

// Called from the websocket task - stage only, no audio work here.
void PlaylistValidator::start() {
  if (active() || _wantRun) return;
  _wantRun = true;
}

void PlaylistValidator::stop(bool reportDone) {
  if (!active() && !_wantRun) return;
  _wantStop = true;
  _wantStopReport = reportDone;
}

void PlaylistValidator::loop() {
  if (_wantStop) {
    _wantStop = false;
    if (active() || _wantRun) { _wantRun = false; _abort(_wantStopReport); }
    return;
  }
  if (_wantRun && !active()) { _begin(); return; }
  if (_state == PLV_IDLE) return;
  if (config.getMode() != PM_WEB || (network.status != CONNECTED && network.status != SDOFFLINE)) { _abort(true); return; }

  if (_state == PLV_CONNECT) {
    if (!utility.loadStation(_index) || config.station.url[0] == '\0') { _next(false); return; }
    const bool ok = player.connecttohost(config.station.url);
    _attempt++;
    if (ok) {
      _listenUntil = millis() + PLAYLIST_VALIDATE_LISTEN_MS;
      _state = PLV_LISTEN;
    } else if (_attempt >= PLAYLIST_VALIDATE_ATTEMPTS) {
      _next(false);
    }
    return;
  }

  if (_state == PLV_LISTEN) {
    const bool died = !player.isRunning();
    const bool elapsed = (int32_t)(millis() - _listenUntil) >= 0;
    if (died) {
      _attempt++;
      if (_attempt >= PLAYLIST_VALIDATE_ATTEMPTS) { player.stopStream(); _next(false); }
      else _state = PLV_CONNECT;              // the stream dropped at once; try it again
      return;
    }
    if (elapsed) { player.stopStream(); _next(true); }
  }
}

// The audio work for a start request, on the main loop task.
void PlaylistValidator::_begin() {
  _wantRun = false;
  _total = (config.getMode() == PM_WEB) ? utility.playlistLength() : 0;
  if (_total == 0) { _post(PLV_MSG_DONE, 0, PLV_RESULT_NONE); return; }

  _savedVol = player.getVolume();
  _savedLast = config.lastStation();
  _muted = false;
  network.cancelStreamRetry();
  player.stopSync();                       // synchronous stop: closes any stream before the first probe
  if (player.getVolume() != 0) { player.setVolume(0); _muted = true; }
  config.setTitle(player.validatingLabel());   // localized via dsplocale in player.cpp, not here
  display.putRequest(NEWMODE, VALIDATING);
  _index = 1;
  _attempt = 0;
  _label[0] = '\0';
  _state = PLV_CONNECT;
  _post(PLV_MSG_START, 0, PLV_RESULT_NONE);
  FUNCTIONLOG("Playlist", "validator: %u station(s)", (unsigned)_total);
}

// Report this station and step to the next, or finish.
void PlaylistValidator::_next(bool pass) {
  strlcpy(_label, config.station.name, sizeof(_label));
  _post(PLV_MSG_ITEM, _index, pass ? PLV_RESULT_PASS : PLV_RESULT_FAIL);
  display.updateProgress(_label, (float)_index / (float)_total);
  _index++;
  _attempt = 0;
  if (_index > _total) { _finish(); return; }
  _state = PLV_CONNECT;
}

void PlaylistValidator::_finish() {
  _restore();
  _state = PLV_IDLE;
  _post(PLV_MSG_DONE, 0, PLV_RESULT_NONE);
  FUNCTIONLOG("Playlist", "validator: done");
}

void PlaylistValidator::_abort(bool reportDone) {
  _restore();
  _state = PLV_IDLE;
  if (reportDone) _post(PLV_MSG_DONE, 0, PLV_RESULT_NONE);
  FUNCTIONLOG("Playlist", "validator: stopped");
}

// Shared teardown for a completed run, an early stop or a mode/network change: quiet the audio, unmute,
// drop the indicator and put the user's station back so the panel and the WebUI agree afterwards.
void PlaylistValidator::_restore() {
  player.stopStream();
  if (_muted) { player.setVolume(_savedVol); _muted = false; }
  network.cancelStreamRetry();
  if (_savedLast > 0) utility.loadStation(_savedLast);
  // The same landing every other exit uses - [ready], or the no-SD-card text offline - instead of the empty
  // title this used to write, which came back to a blank station line.  AFTER loadStation(), because setReady()
  // tests the station url it has just restored.
  player.setReady();
  display.clearProgress();   // the bar shares the dialog page with the volume overlay, so blank it
  display.putRequest(NEWMODE, PLAYER);
}

void PlaylistValidator::_post(uint8_t message, uint16_t index, uint8_t result) {
  _message = message;
  _reportIndex = index;
  _reportResult = result;
  netserver.requestOnChange(PLVALIDATE, 0);
}
