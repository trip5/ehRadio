#ifndef player_h
#define player_h

#include "options.h"
#include "mqtt.h"

#if defined(USE_AUDIO_I2S)
 #include "../libraries/I2S_Audio/Audio.h"
#else
  #include "../libraries/VS1053_Audio/audioVS1053Ex.h"
#endif

#define PLERR_LN        64
#define SET_PLAY_ERROR(...) {char buff[576]; snprintf(buff, sizeof(buff), __VA_ARGS__); setError(buff);}

// Card-file heartbeat and the switch for both [SD] trace lines. 0 = off; set 30000 in myoptions.h for a soak.
#ifndef SD_PLAY_TRACE_MS
  #define SD_PLAY_TRACE_MS 0
#endif

#ifndef VS_CHIP_DEAD_STRIKES
  #define VS_CHIP_DEAD_STRIKES 3  // stalled feeds against a chip answering 0x0000 before the decoder is declared dead
#endif

enum playerRequestType_e : uint8_t { PR_PLAY = 1, PR_STOP = 2, PR_PREV = 3, PR_NEXT = 4, PR_VOL = 5, PR_CHECKSD = 6, PR_VUTONUS = 7, PR_BURL = 8, PR_TOGGLE = 9 };
struct playerRequestParams_t
{
  playerRequestType_e type;
  int payload;
};

enum plStatus_e : uint8_t{ PLAYING = 1, STOPPED = 2 };

class Player: public Audio {
  public:
    volatile bool lockOutput = true;  // volatile: written from WiFi callbacks, read from audio callbacks
    bool resumeAfterUrl = false;
    bool autoAdvance = false;
    uint32_t sd_min, sd_max;
    bool remoteStationName = false;
    uint32_t lastConnectMs = 0;
    char burl[MQTT_URL_SIZE + 1];  /* buffer for browseUrl  */
  public:
    Player();
    void init();
    void sendCommand(playerRequestParams_t request);
    void resetQueue();
    void stopSync();  // synchronous stop — closes audio file before SD unmount
    void stopStream();  // close the audio stream only, without _stop()'s title/notification side effects
    void stopInfo();
    void setError(const char *e);
    void setReady();
    void noteFatalStream();  // a permanent host error (404/410): stop and refuse to re-arm the retry ladder
    void initHeaders(const char *file);
    void loop();
    bool queueResolvedUrl(const char* url);
    bool resumeLastWebSource();
    void setOutputPins(bool isPlaying);
    void browseUrl();
    void playUrl(const char* url);
    void prev();
    void next();
    void toggle();
    void stepVol(bool up);
    uint8_t volToI2S(uint8_t volume);
    void setVol(uint8_t volume);
    void mute();
    bool isMuted() const { return _muted; }
    void clearMute() { _muted = false; }

    bool hasError() { return strlen(_plError)>0; }
    bool decoderDead() const { return _decoderDead; }  // latched: the decoder stopped answering
    plStatus_e status() { return _status; }
    bool isConnecting();  // the title still holds the localized "connecting" placeholder
    const char* validatingLabel();  // the playlist validator's localized indicator string
    void setResumeFilePos(uint32_t pos) { _resumeFilePos = pos; }
  private:
    uint32_t    _resumeFilePos = 0;
    plStatus_e  _status = STOPPED;
    bool        _fatalStream = false;  // 404/410 seen: the arm gate must not start the ladder
    bool        _decoderDead = false;  // VS1053 answered nothing: latched until the next _play()
    uint16_t    _playingStationId = 0;
    char        _plError[PLERR_LN];
    uint8_t     _muteVol = 30;
    bool        _muted = false;
    uint32_t    _traceNextMs = 0;  // SD_PLAY_TRACE_MS heartbeat due time, 0 = not armed
    bool        _autoAdvancePlay = false;  // this _play() came from the EOF callback, not from a user action

    uint32_t    _shuffleKey  = 0;  // per-pass key for the Feistel rounds
    uint32_t    _shuffleRaw  = 0;  // raw counter; outputs >= n are skipped by cycle-walking
    uint32_t    _shufflePass = 0;  // ids produced this pass, 0.._shuffleN
    uint32_t    _shuffleBits = 0;  // even bit width of the domain, the smallest 2^bits >= n
    uint16_t    _shuffleN    = 0;  // playlist length the walk was started for, 0 = no walk yet
    bool        _shuffleWalkOwns = false;  // the _play() now being queued came from next()/prev()'s walk

    uint32_t _shuffleEnc(uint32_t x, uint32_t bits);  // one Feistel round-trip, before cycle-walking
    int32_t  _shuffleFirst();                         // first id the current key produces, without advancing
    void     _shuffleStartPass(uint16_t avoidId);  // start a pass that does not open with avoidId
    void     _shuffleRestart(uint16_t avoidId);  // size the domain and start a pass (a hand-picked track)
    uint16_t _shuffleNext();
    uint16_t _shufflePrev();  // 0 = nothing behind this one in the current pass

    void _stop(bool alreadyStopped = false);
    void _play(uint16_t stationId);
    void _loadVol(uint8_t volume);
};

extern Player player;

#endif
