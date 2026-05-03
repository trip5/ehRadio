#ifndef AUDIOHANDLERS_H
#define AUDIOHANDLERS_H

#include "logging.h"

//=============================================//
//              Audio handlers                 //
//=============================================//

void audio_info(const char *info) {
  if (player.lockOutput) return;
  if (config.store.audioinfo) {
    // Skip empty StreamTitle='' — emitted by library when switching streams
    if (strcmp(info, "StreamTitle=''") != 0)
      FUNCTIONLOG("Audio.info", "%s", info);
  }
  #ifdef USE_NEXTION
    nextion.audioinfo(info);
  #endif
  if (strstr(info, "format is mp3")  != NULL) { config.setBitrateFormat(BF_MP3); display.putRequest(DBITRATE); }
  if (strstr(info, "format is aac")  != NULL) { config.setBitrateFormat(BF_AAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is flac") != NULL) { config.setBitrateFormat(BF_FLAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is wav")  != NULL) { config.setBitrateFormat(BF_WAV); display.putRequest(DBITRATE); }
  if (strstr(info, "format is ogg")  != NULL) { config.setBitrateFormat(BF_VOR); display.putRequest(DBITRATE); }
  if (strstr(info, "format is vorbis")  != NULL) { config.setBitrateFormat(BF_VOR); display.putRequest(DBITRATE); }
  if (strstr(info, "format is opus")  != NULL) { config.setBitrateFormat(BF_OPU); display.putRequest(DBITRATE); }
  if (strstr(info, "skip metadata") != NULL) config.setTitle(config.station.name);
  if (strstr(info, "stream ready") != NULL) {
    if (strcmp_P(config.station.title, LANG::const_PlConnect) == 0) config.setTitle("");  // Clear connecting message
  }
  if (strstr(info, "Account already in use") != NULL || strstr(info, "HTTP/1.0 401") != NULL) {
    player.setError(info);
  }
  char* ici; char b[BUFLEN/2]={0};  // Increased buffer to safely hold bitrate string
  if ((ici = strstr(info, "BitRate: ")) != NULL) {
    strlcpy(b, ici + 9, sizeof(b));
    audio_bitrate(b);
  }
}

void audio_bitrate(const char *info)
{
  if (config.store.audioinfo) FUNCTIONLOG("Audio.bitrate", "%s", info);
  config.station.bitrate = atoi(info) / 1000;
  display.putRequest(DBITRATE);
  #ifdef USE_NEXTION
    nextion.bitrate(config.station.bitrate);
  #endif
  netserver.requestOnChange(BITRATE, 0);
}

bool printable(const char *info) {
  // only reject empty strings or embedded C0 control codes (whitespace
  // such as newline, carriage return, tab or the 0x1E pixel‑spacer are
  // allowed).  Our font and transliterator already cover ASCII, Latin‑1,
  // and Cyrillic glyphs, so let the display layer decide how to render
  // high‑bit characters.
  if (!info || *info == '\0') return false;

  const unsigned char *p = (const unsigned char*)info;
  while (*p) {
    if (*p >= 0x20) {           // printable ASCII or any high‑bit byte
      ++p;
      continue;
    }
    if (*p == '\r' || *p == '\n' || *p == '\t') { // common whitespace
      ++p;
      continue;
    }
    // control code found -> not printable
    return false;
  }
  return true;
}

void audio_showstation(const char *info) {
  bool p = printable(info) && (strlen(info) > 0);(void)p;
  if (player.remoteStationName) {
    config.setStation(p?info:config.station.name);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
  }
}

void audio_showstreamtitle(const char *info) {
  if (strstr(info, "Account already in use") != NULL || strstr(info, "HTTP/1.0 401") != NULL) player.setError(info);
  bool p = printable(info) && (strlen(info) > 0);
  #ifdef DEBUG_TITLES
    config.setTitle(DEBUG_TITLES);
  #else
    config.setTitle(p?info:config.station.name);
  #endif
}

void audio_error(const char *info) {
  player.setError(info);
}

void audio_id3artist(const char *info) {
  if (config.getMode() != PM_SDCARD) return; // web/HLS: don't overwrite station name from ID3 artist tag
  if (printable(info)) config.setStation(info);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
}

void audio_id3album(const char *info) {
  if (player.lockOutput) return;
  if (printable(info)) {
    if (strlen(config.station.title)==0) {
      config.setTitle(info);
    } else {
      char tmp[BUFLEN];
      // Prevent buffer overflow: reserve space for " - " and null terminator
      size_t title_len = strlen(config.station.title);
      size_t info_len = strlen(info);
      if (title_len + 3 + info_len + 1 <= BUFLEN) {
        snprintf(tmp, BUFLEN, "%s - %s", config.station.title, info);
        config.setTitle(tmp);
      } else {
        // Title + album would overflow, just use album
        config.setTitle(info);
      }
    }
  }
}

void audio_id3title(const char *info) {
  if (player.lockOutput) return;
  if (printable(info)) config.setTitle(info);
}

void audio_beginSDread() {
  config.setTitle("");
}

void audio_id3data(const char *info) {  //id3 metadata
    if (player.lockOutput) return;
  FUNCTIONLOG("Audio.id3", "%s", info);
}

void audio_eof_mp3(const char *info) {  //end of file
    config.sdResumePos = 0;
    player.next();
}

void audio_eof_stream(const char *info) {
  player.sendCommand({PR_STOP, 0});
  if (!player.resumeAfterUrl) return;
  if (config.getMode()==PM_WEB) {
    player.sendCommand({PR_PLAY, config.lastStation()});
  } else {
    player.setResumeFilePos(config.sdResumePos==0?0:config.sdResumePos-player.sd_min);
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
}

void audio_progress(uint32_t startpos, uint32_t endpos) {
  player.sd_min = startpos;
  player.sd_max = endpos;
  netserver.requestOnChange(SDLEN, 0);
}

#endif
