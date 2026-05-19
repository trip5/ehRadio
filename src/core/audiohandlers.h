#ifndef audiohandlers_h
#define audiohandlers_h
#pragma once

#include <Arduino.h>
#include "options.h"

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

  bool clearArtwork();
  bool setArtworkUrl(const char* url);
  bool setArtworkFallbackImageUrl(const char* url);
  const char* getArtworkImageUrl() const { return artworkImageUrl; }

private:
  bool isPrintable(const char* info) const;

  char artworkUrl[MQTT_URL_SIZE + 1] = {0};
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
