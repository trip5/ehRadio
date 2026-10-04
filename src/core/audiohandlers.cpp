#include "options.h"
#include "audiohandlers.h"

#include <ctype.h>
#include <cstdlib>
#include <cstring>

#include "config.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "player.h"
#include "utility.h"

namespace {

char metadataUrlBuffer[MQTT_URL_SIZE + 1] = {0};

bool extractMetadataValue(const char* info, const char* key, char* output, size_t outputSize) {
  if (!info || !key || !output || outputSize == 0) return false;

  output[0] = '\0';
  const char* start = strstr(info, key);
  if (!start) return false;

  start += strlen(key);
  if (*start == '\'' || *start == '"') {
    char quote = *start++;
    const char* end = strchr(start, quote);
    size_t len = end ? static_cast<size_t>(end - start) : strlen(start);
    if (len >= outputSize) len = outputSize - 1;
    memcpy(output, start, len);
    output[len] = '\0';
    return true;
  }

  const char* end = strchr(start, ';');
  size_t len = end ? static_cast<size_t>(end - start) : strlen(start);
  if (len >= outputSize) len = outputSize - 1;
  memcpy(output, start, len);
  output[len] = '\0';
  return true;
}

void publishArtworkIfChanged(bool changed) {
  if (changed) netserver.requestOnChange(ARTWORK, 0);
}

void normalizeArtworkUrl(const char* input, char* output, size_t outputSize) {
  if (!output || outputSize == 0) return;
  output[0] = '\0';
  if (!input) return;

  strlcpy(output, input, outputSize);
  utility.stripWhitespace(output);
  utility.stripWrappingQuotes(output);
  utility.stripWhitespace(output);
}

bool startsWithHttpScheme(const char* url) {
  return url && (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0);
}

bool endsWithImageExtension(const char* url) {
  if (!url) return false;

  const char* query = strpbrk(url, "?#");
  size_t pathLen = query ? static_cast<size_t>(query - url) : strlen(url);
  static const char* exts[] = {".jpg", ".jpeg", ".png", ".gif", ".webp"};

  for (size_t extIndex = 0; extIndex < sizeof(exts) / sizeof(exts[0]); ++extIndex) {
    const char* ext = exts[extIndex];
    size_t extLen = strlen(ext);
    if (pathLen < extLen) continue;

    size_t offset = pathLen - extLen;
    bool matches = true;
    for (size_t i = 0; i < extLen; ++i) {
      char left = static_cast<char>(tolower(static_cast<unsigned char>(url[offset + i])));
      if (left != ext[i]) {
        matches = false;
        break;
      }
    }
    if (matches) return true;
  }

  return false;
}

bool isValidArtworkImageUrl(const char* url) {
  return startsWithHttpScheme(url) && endsWithImageExtension(url);
}

bool copyStringIfChanged(char* dest, size_t destSize, const char* src) {
  const char* next = src ? src : "";
  if (strcmp(dest, next) == 0) return false;
  memset(dest, 0, destSize);
  strlcpy(dest, next, destSize);
  return true;
}

static bool handleAccountError(const char* info) {
  if (strstr(info, "Account already in use")) {
    char errbuf[PLERR_LN];
    strncpy_P(errbuf, l10n(L10N_MSG_ACCOUNT_IN_USE), PLERR_LN - 1);
    errbuf[PLERR_LN - 1] = '\0';
    player.setError(errbuf);
    return true;
  }
  if (strstr(info, "HTTP/1.0 401")) {
    char errbuf[PLERR_LN];
    strncpy_P(errbuf, l10n(L10N_MSG_ACCOUNT_ERROR), PLERR_LN - 1);
    errbuf[PLERR_LN - 1] = '\0';
    player.setError(errbuf);
    return true;
  }
  return false;
}

} // namespace

AudioHandlers audioHandlers;

bool AudioHandlers::clearArtwork() {
  bool changed = false;
  changed = copyStringIfChanged(artworkUrl, sizeof(artworkUrl), "") || changed;
  changed = copyStringIfChanged(artworkImageUrl, sizeof(artworkImageUrl), "") || changed;
  changed = copyStringIfChanged(artworkFallbackUrl, sizeof(artworkFallbackUrl), "") || changed;
  return changed;
}

bool AudioHandlers::setArtworkUrl(const char* url) {
  artworkScratchUrl[0] = '\0';
  artworkScratchImageUrl[0] = '\0';

  normalizeArtworkUrl(url, artworkScratchUrl, sizeof(artworkScratchUrl));
  if (isValidArtworkImageUrl(artworkScratchUrl)) {
    strlcpy(artworkScratchImageUrl, artworkScratchUrl, sizeof(artworkScratchImageUrl));
  } else if (isValidArtworkImageUrl(artworkFallbackUrl)) {
    strlcpy(artworkScratchImageUrl, artworkFallbackUrl, sizeof(artworkScratchImageUrl));
  }

  bool changed = false;
  changed = copyStringIfChanged(artworkUrl, sizeof(artworkUrl), artworkScratchUrl) || changed;
  changed = copyStringIfChanged(artworkImageUrl, sizeof(artworkImageUrl), artworkScratchImageUrl) || changed;
  return changed;
}

bool AudioHandlers::setArtworkFallbackImageUrl(const char* url) {
  artworkScratchUrl[0] = '\0';
  normalizeArtworkUrl(url, artworkScratchUrl, sizeof(artworkScratchUrl));

  if (!isValidArtworkImageUrl(artworkScratchUrl)) {
    return false;
  }

  bool changed = false;
  changed = copyStringIfChanged(artworkFallbackUrl, sizeof(artworkFallbackUrl), artworkScratchUrl) || changed;
  if (!isValidArtworkImageUrl(artworkUrl)) {
    changed = copyStringIfChanged(artworkImageUrl, sizeof(artworkImageUrl), artworkScratchUrl) || changed;
  }
  return changed;
}

bool AudioHandlers::isPrintable(const char* info) const {
  if (!info || *info == '\0') return false;

  const unsigned char* text = reinterpret_cast<const unsigned char*>(info);
  while (*text) {
    if (*text >= 0x20) {
      ++text;
      continue;
    }
    if (*text == '\r' || *text == '\n' || *text == '\t') {
      ++text;
      continue;
    }
    return false;
  }
  return true;
}

void AudioHandlers::handleInfo(const char* info) {
  if (player.lockOutput) return;

  if (extractMetadataValue(info, "StreamUrl=", metadataUrlBuffer, sizeof(metadataUrlBuffer))) {
    publishArtworkIfChanged(setArtworkUrl(metadataUrlBuffer));
  }

  if (strcmp(info, "StreamTitle=''") != 0) FUNCTIONLOG("Audio.info", "%s", info);
  

  if (strstr(info, "format is mp3") != NULL) { config.setBitrateFormat(BF_MP3); display.putRequest(DBITRATE); }
  if (strstr(info, "format is aac") != NULL) { config.setBitrateFormat(BF_AAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is flac") != NULL) { config.setBitrateFormat(BF_FLAC); display.putRequest(DBITRATE); }
  if (strstr(info, "format is wav") != NULL) { config.setBitrateFormat(BF_WAV); display.putRequest(DBITRATE); }
  if (strstr(info, "format is ogg") != NULL) { config.setBitrateFormat(BF_VOR); display.putRequest(DBITRATE); }
  if (strstr(info, "format is vorbis") != NULL) { config.setBitrateFormat(BF_VOR); display.putRequest(DBITRATE); }
  if (strstr(info, "format is opus") != NULL) { config.setBitrateFormat(BF_OPU); display.putRequest(DBITRATE); }
  if (strstr(info, "skip metadata") != NULL) config.setTitle(config.station.name);
  // Belt and braces only: the connecting placeholder is owned by Player::loop(), which clears it from player state
  // because this message is not emitted on every library route (an HLS playlist stream never sends it). Kept because on
  // the routes that DO announce it, this clears the placeholder a loop earlier.
  if (strstr(info, "stream ready") != NULL) {
    if (strcmp_P(config.station.title, l10n(L10N_MSG_CONNECT)) == 0) config.setTitle("");
  }
  handleAccountError(info);

  char* ici;
  if ((ici = strstr(info, "BitRate: ")) != NULL) {
    handleBitrate(ici + 9);
  }
}

void AudioHandlers::handleBitrate(const char* info) {
  FUNCTIONLOG("Audio.bitrate", "%s", info);
  config.station.bitrate = atoi(info) / 1000;
  display.putRequest(DBITRATE);
  netserver.requestOnChange(BITRATE, 0);
}

void AudioHandlers::handleShowStation(const char* info) {
  bool printable = isPrintable(info) && (strlen(info) > 0);
  if (player.remoteStationName) {
    config.setStation(printable ? info : config.station.name);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
  }
}

void AudioHandlers::handleShowStreamTitle(const char* info) {
  if (handleAccountError(info)) return;  // error handled, don't display raw HTTP response as title
  bool printable = isPrintable(info) && (strlen(info) > 0);
  #ifdef DEBUG_TITLES
    config.setTitle(DEBUG_TITLES);
  #else
    config.setTitle(printable ? info : config.station.name);
  #endif
}

void AudioHandlers::handleError(const char* info) {
  player.setError(info);
}

void AudioHandlers::handleIcyLogo(const char* info) {
  if (player.lockOutput) return;
  publishArtworkIfChanged(setArtworkFallbackImageUrl(info));
}

void AudioHandlers::handleId3Artist(const char* info) {
  if (config.getMode() != PM_SDCARD) return;
  if (isPrintable(info)) config.setStation(info);
  display.putRequest(NEWSTATION);
  netserver.requestOnChange(STATION, 0);
}

void AudioHandlers::handleId3Album(const char* info) {
  if (player.lockOutput) return;
  if (isPrintable(info)) {
    strlcpy(_albumBuf, info, sizeof(_albumBuf));
    if (strlen(config.station.title) == 0) {
      config.setTitle(info);
    } else {
      char tmp[STATION_FIELD_LENGTH];
      size_t titleLen = strlen(config.station.title);
      size_t infoLen = strlen(info);
      if (titleLen + 3 + infoLen + 1 <= STATION_FIELD_LENGTH) {
        snprintf(tmp, STATION_FIELD_LENGTH, "%s - %s", config.station.title, info);
        config.setTitle(tmp);
      } else {
        config.setTitle(info);
      }
    }
  }
}

void AudioHandlers::handleId3Title(const char* info) {
  if (player.lockOutput) return;
  if (isPrintable(info)) {
    strlcpy(_titleBuf, info, sizeof(_titleBuf));
    _tryCombineTitleAlbum();
  }
}

void AudioHandlers::_tryCombineTitleAlbum() {
  if (_titleBuf[0] == '\0') return;  // no title yet
  if (_albumBuf[0] != '\0') {
    char tmp[STATION_FIELD_LENGTH];
    snprintf(tmp, sizeof(tmp), "%s - %s", _titleBuf, _albumBuf);
    config.setTitle(tmp);
  } else {
    config.setTitle(_titleBuf);
  }
}

void AudioHandlers::handleBeginSdRead() {
  config.setTitle("");
  _titleBuf[0] = '\0';
  _albumBuf[0] = '\0';
}

void AudioHandlers::handleId3Data(const char* info) {
  if (player.lockOutput) return;
  FUNCTIONLOG("Audio.id3", "%s", info);
}

void AudioHandlers::handleEofMp3(const char* info) {
  (void)info;
  config.sdResumePos = 0;
  player.next();
}

void AudioHandlers::handleEofStream(const char* info) {
  (void)info;
  player.sendCommand({PR_STOP, 0});
  if (!player.resumeAfterUrl) return;
  if (config.getMode() == PM_WEB) {
    player.resumeLastWebSource();
  } else {
    player.setResumeFilePos(config.sdResumePos == 0 ? 0 : config.sdResumePos - player.sd_min);
    player.sendCommand({PR_PLAY, config.lastStation()});
  }
}

void AudioHandlers::handleProgress(uint32_t startPos, uint32_t endPos) {
  player.sd_min = startPos;
  player.sd_max = endPos;
  netserver.requestOnChange(SDLEN, 0);
}

void audio_info(const char* info) { audioHandlers.handleInfo(info); }
void audio_bitrate(const char* info) { audioHandlers.handleBitrate(info); }
void audio_showstation(const char* info) { audioHandlers.handleShowStation(info); }
void audio_showstreamtitle(const char* info) { audioHandlers.handleShowStreamTitle(info); }
void audio_error(const char* info) { audioHandlers.handleError(info); }
void audio_icylogo(const char* info) { audioHandlers.handleIcyLogo(info); }
void audio_id3artist(const char* info) { audioHandlers.handleId3Artist(info); }
void audio_id3album(const char* info) { audioHandlers.handleId3Album(info); }
void audio_id3title(const char* info) { audioHandlers.handleId3Title(info); }
void audio_beginSDread() { audioHandlers.handleBeginSdRead(); }
void audio_id3data(const char* info) { audioHandlers.handleId3Data(info); }
void audio_eof_mp3(const char* info) { audioHandlers.handleEofMp3(info); }
void audio_eof_stream(const char* info) { audioHandlers.handleEofStream(info); }
void audio_progress(uint32_t startPos, uint32_t endPos) { audioHandlers.handleProgress(startPos, endPos); }