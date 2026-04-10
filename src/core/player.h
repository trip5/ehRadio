#ifndef player_h
#define player_h

#if I2S_DOUT!=255 || I2S_INTERNAL
 #include "../libraries/I2S_Audio/Audio.h"
#else
  #include "../libraries/VS1053_Audio/audioVS1053Ex.h"
#endif

#ifndef MQTT_BURL_SIZE
  #define MQTT_BURL_SIZE  512
#endif

#ifndef PLQ_SEND_DELAY
	#define PLQ_SEND_DELAY pdMS_TO_TICKS(1000)
#endif

#define PLERR_LN        64
#define SET_PLAY_ERROR(...) {char buff[576]; snprintf(buff, sizeof(buff), __VA_ARGS__); setError(buff);}

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
    uint32_t sd_min, sd_max;
    bool remoteStationName = false;
    char burl[MQTT_BURL_SIZE];  /* buffer for browseUrl  */
  public:
    Player();
    void init();
    void sendCommand(playerRequestParams_t request);
    void resetQueue();
    void stopInfo();
    void setError(const char *e);
    void initHeaders(const char *file);
    void loop();
    void setOutputPins(bool isPlaying);
    void browseUrl();
    void playUrl(const char* url);
    void prev();
    void next();
    void toggle();
    void stepVol(bool up);
    uint8_t volToI2S(uint8_t volume);
    void setVol(uint8_t volume);

    bool hasError() { return strlen(_plError)>0; }
    plStatus_e status() { return _status; }
    void setResumeFilePos(uint32_t pos) { _resumeFilePos = pos; }
  private:
    uint32_t    _resumeFilePos = 0;
    plStatus_e  _status = STOPPED;
    char        _plError[PLERR_LN];

    void _stop(bool alreadyStopped = false);
    void _play(uint16_t stationId);
    void _loadVol(uint8_t volume);
};

extern Player player;

extern __attribute__((weak)) void player_on_start_play();
extern __attribute__((weak)) void player_on_stop_play();
extern __attribute__((weak)) void player_on_track_change();
extern __attribute__((weak)) void player_on_station_change();

#endif
