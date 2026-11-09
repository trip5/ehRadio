#ifndef audiohandlers_h
#define audiohandlers_h
#pragma once

#include <Arduino.h>
#include "options.h"
#include "mqtt.h"

// One half of the title line: the song, or the second line (artist, else album).
#define META_HALF_LN 64

class AudioHandlers {
public:
  void handleInfo(const char* info);
  void handleBitrate(const char* info);
  void handleShowStation(const char* info);
  void handleShowStreamTitle(const char* info);
  void handleError(const char* info);
  void handleIcyLogo(const char* info);
  void handleId3Artist(const char* info);
  void handleId3Album(const char* info);
  void handleId3Title(const char* info);
  void handleBeginSdRead();
  void handleId3Data(const char* info);
  void handleEofMp3(const char* info);
  void handleEofStream(const char* info);
  void handleProgress(uint32_t startPos, uint32_t endPos);

  void tick();            // settles and publishes the title line
  void resetMetadata();   // a new source: both halves are dropped, nothing is owed

  bool clearArtwork();
  bool setArtworkUrl(const char* url);
  bool setArtworkFallbackImageUrl(const char* url);
  const char* getArtworkImageUrl() const { return artworkImageUrl; }

private:
  // The two title lines are assembled from two independent sources, each of which may supply one half in
  // either order.  ID3 outranks the playlist within one item, and a half older than the settle window is
  // never paired with a newer partner - which is what keeps a value from a previous item, or a previous
  // source, out of the pair.
  enum MetaSource : uint8_t { META_NONE = 0, META_PLAYLIST, META_ID3 };
  struct MetaHalf {
    char     text[META_HALF_LN] = {0};
    uint32_t ms = 0;
    uint8_t  src = META_NONE;
    bool     artist = false;
  };

  bool isPrintable(const char* info) const;
  bool _isFresh(const MetaHalf& half) const;
  bool _pairComplete() const;
  uint32_t _settleMs() const;
  void _clearHalf(MetaHalf& half);
  void _setHalf(MetaHalf& half, const char* value, uint8_t src, bool artist);
  void _putHalf(MetaHalf& half, const char* value, uint8_t src, bool artist);
  void _schedulePublish();
  void _publishComposed();

  char artworkUrl[MQTT_URL_SIZE + 1] = {0};
  MetaHalf _song;
  MetaHalf _second;
  uint32_t _publishAt = 0;
  bool _publishPending = false;
  char artworkImageUrl[MQTT_URL_SIZE + 1] = {0};
  char artworkFallbackUrl[MQTT_URL_SIZE + 1] = {0};
  char artworkScratchUrl[MQTT_URL_SIZE + 1] = {0};
  char artworkScratchImageUrl[MQTT_URL_SIZE + 1] = {0};
};

void audio_info(const char* info);
void audio_bitrate(const char* info);
void audio_showstation(const char* info);
void audio_showstreamtitle(const char* info);
void audio_error(const char* info);
void audio_icylogo(const char* info);
void audio_id3artist(const char* info);
void audio_id3album(const char* info);
void audio_id3title(const char* info);
void audio_beginSDread();
void audio_id3data(const char* info);
void audio_eof_mp3(const char* info);
void audio_eof_stream(const char* info);
void audio_progress(uint32_t startPos, uint32_t endPos);

extern AudioHandlers audioHandlers;

#endif // audiohandlers_h
