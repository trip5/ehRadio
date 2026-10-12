#ifndef playlistvalidator_h
#define playlistvalidator_h
#pragma once

#include "options.h"
#include "config.h"

// Sequential playlist check, driven from the main loop (the task that owns the audio object).
// One station at a time: connect, listen briefly, report pass/fail.  A redirect inside connecttohost()
// counts as a pass; a 404 / connect failure is retried up to PLAYLIST_VALIDATE_ATTEMPTS.
enum plValidateMessage_e : uint8_t { PLV_MSG_NONE = 0, PLV_MSG_START, PLV_MSG_ITEM, PLV_MSG_DONE };
enum plValidateResult_e  : uint8_t { PLV_RESULT_NONE = 0, PLV_RESULT_PASS, PLV_RESULT_FAIL };

class PlaylistValidator {
  public:
    // start()/stop() are called from the websocket task, so they only stage a request; the audio work is done
    // by loop(), which runs on the main loop task that owns the audio object.
    void start();
    void stop(bool reportDone = true);
    void loop();                          // state machine tick; call every main-loop pass
    bool active() const { return _state != PLV_IDLE; }

    // Read by netserver when it serialises the PLVALIDATE event.
    uint8_t  message() const { return _message; }
    uint16_t reportIndex() const { return _reportIndex; }
    uint8_t  reportResult() const { return _reportResult; }
    uint16_t total() const { return _total; }
    const char* reportLabel() const { return _label; }

  private:
    enum state_e : uint8_t { PLV_IDLE = 0, PLV_CONNECT, PLV_LISTEN };
    void _begin();
    void _abort(bool reportDone);
    void _next(bool pass);
    void _finish();
    void _restore();
    void _post(uint8_t message, uint16_t index, uint8_t result);

    bool     _wantRun = false;            // staged by start(), consumed by loop()
    bool     _wantStop = false;           // staged by stop(), consumed by loop()
    bool     _wantStopReport = true;

    state_e  _state = PLV_IDLE;
    uint16_t _index = 0;      // 1-based station being probed
    uint16_t _total = 0;
    uint8_t  _attempt = 0;    // probes spent on the current station
    uint32_t _listenUntil = 0;
    uint8_t  _savedVol = 0;
    uint16_t _savedLast = 0;
    bool     _muted = false;

    uint8_t  _message = PLV_MSG_NONE;
    uint16_t _reportIndex = 0;
    uint8_t  _reportResult = PLV_RESULT_NONE;
    char     _label[STATION_FIELD_LENGTH] = {0};
};

extern PlaylistValidator playlistValidator;

#endif
