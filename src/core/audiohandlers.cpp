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
#include "playlistvalidator.h"

namespace {

// How long a title half is held for its partner before it is shown alone.  The two halves of one item arrive
// together - in one ID3 tag block, or in one #EXTINF line - so this only ever decides a lone half, and a half
// older than this is never paired with a newer partner.
const uint32_t META_SETTLE_MS = 1500;

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

// Both audio libraries pass the raw HTTP status line ("HTTP/1.1 404 File Not Found") through
// audio_showstreamtitle on any status above 310.  Returns the code, or 0 if this is not a status line.
static int httpStatusCode(const char* info) {
  if (!info || strncmp(info, "HTTP/", 5) != 0 || strlen(info) < 12) return 0;
  const char* p = info + 9;
  if (!isdigit(static_cast<unsigned char>(p[0])) ||
      !isdigit(static_cast<unsigned char>(p[1])) ||
      !isdigit(static_cast<unsigned char>(p[2]))) return 0;
  return (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
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
  if (playlistValidator.active()) return;   // the validator owns the indicator while it sweeps
  bool printable = isPrintable(info) && (strlen(info) > 0);
  if (player.remoteStationName) {
    config.setStation(printable ? info : config.station.name);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
  }
}

void AudioHandlers::handleShowStreamTitle(const char* info) {
  if (playlistValidator.active()) return;   // the validator owns the indicator while it sweeps
  if (handleAccountError(info)) return;  // error handled, don't display raw HTTP response as title
  const int status = httpStatusCode(info);
  if (status == 404 || status == 410) { player.noteFatalStream(); return; }
  #ifdef DEBUG_TITLES
    config.setTitle(DEBUG_TITLES);
    return;
  #endif
  if (!isPrintable(info) || info[0] == '\0') {
    // A stream with nothing to say shows its own name in the first line, and no second line survives it.
    _setHalf(_song, config.station.name, META_PLAYLIST, false);
    _clearHalf(_second);
    _schedulePublish();
    return;
  }
  // "song - second": both halves arrive in one line, so they are written as one item.
  const char* sep = strstr(info, " - ");
  if (sep == nullptr) { _putHalf(_song, info, META_PLAYLIST, false); return; }
  char song[META_HALF_LN];
  size_t len = static_cast<size_t>(sep - info);
  if (len >= sizeof(song)) len = sizeof(song) - 1;
  memcpy(song, info, len);
  song[len] = '\0';
  _putHalf(_song, song, META_PLAYLIST, false);
  _putHalf(_second, sep + 3, META_PLAYLIST, false);
}

void AudioHandlers::handleError(const char* info) {
  if (playlistValidator.active()) return;   // a failed probe is expected, not an error to display
  player.setError(info);
}

void AudioHandlers::handleIcyLogo(const char* info) {
  if (player.lockOutput) return;
  publishArtworkIfChanged(setArtworkFallbackImageUrl(info));
}

void AudioHandlers::handleId3Artist(const char* info) {
  if (playlistValidator.active()) return;
  if (config.getMode() == PM_SDCARD) {
    // SD: the artist names the station, never the second title line.
    if (isPrintable(info)) config.setStation(info);
    display.putRequest(NEWSTATION);
    netserver.requestOnChange(STATION, 0);
    return;
  }
  if (player.lockOutput) return;
  if (isPrintable(info)) _putHalf(_second, info, META_ID3, true);
}

void AudioHandlers::handleId3Album(const char* info) {
  if (playlistValidator.active()) return;
  if (player.lockOutput) return;
  if (!isPrintable(info)) return;
  if (_second.artist && _second.text[0]) return;  // artist outranks album for the current item
  _putHalf(_second, info, META_ID3, false);
}

void AudioHandlers::handleId3Title(const char* info) {
  if (playlistValidator.active()) return;
  if (player.lockOutput) return;
  if (isPrintable(info)) _putHalf(_song, info, META_ID3, false);
}

void AudioHandlers::handleBeginSdRead() {
  resetMetadata();
  config.setTitle("");
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

/* ==== The title line: two halves, two sources ==== */

void AudioHandlers::tick() {
  if (!_publishPending) return;
  if ((int32_t)(millis() - _publishAt) < 0) return;
  _publishComposed();
}

void AudioHandlers::resetMetadata() {
  _clearHalf(_song);
  _clearHalf(_second);
  _publishPending = false;
}

// An SD file's two tags are one item by construction, so the window does not apply to it.
uint32_t AudioHandlers::_settleMs() const {
  return (config.getMode() == PM_SDCARD) ? 0 : META_SETTLE_MS;
}

bool AudioHandlers::_isFresh(const MetaHalf& half) const {
  const uint32_t settle = _settleMs();
  if (settle == 0) return true;
  return (int32_t)(millis() - half.ms) <= (int32_t)settle;
}

bool AudioHandlers::_pairComplete() const {
  if (!_song.text[0] || !_second.text[0]) return false;
  const uint32_t settle = _settleMs();
  if (settle == 0) return true;
  const int32_t gap = (int32_t)(_song.ms - _second.ms);
  return gap >= -(int32_t)settle && gap <= (int32_t)settle;
}

void AudioHandlers::_clearHalf(MetaHalf& half) {
  half.text[0] = '\0';
  half.ms = 0;
  half.src = META_NONE;
  half.artist = false;
}

void AudioHandlers::_setHalf(MetaHalf& half, const char* value, uint8_t src, bool artist) {
  strlcpy(half.text, value ? value : "", sizeof(half.text));
  half.ms = millis();
  half.src = src;
  half.artist = artist;
}

void AudioHandlers::_putHalf(MetaHalf& half, const char* value, uint8_t src, bool artist) {
  const char* next = value ? value : "";
  if (strcmp(half.text, next) == 0) {
    half.ms = millis();  // the same value: only its arrival time moves
    if (src == META_ID3) half.src = META_ID3;
  } else if (src == META_PLAYLIST && half.src == META_ID3 && _isFresh(half)) {
    return;  // ID3 outranks the playlist for the same item
  } else {
    _setHalf(half, next, src, artist);
    // A new song starts a new item, so any artist known for the previous one stops claiming the second line.
    if (&half == &_song) _second.artist = false;
  }
  _schedulePublish();
}

void AudioHandlers::_schedulePublish() {
  const uint32_t settle = _settleMs();
  if (settle == 0 || _pairComplete()) { _publishComposed(); return; }  // nothing else is coming
  _publishPending = true;
  _publishAt = millis() + settle;  // give the partner half its window before showing a lone half
}

void AudioHandlers::_publishComposed() {
  char line[META_HALF_LN * 2 + 4];
  if (_pairComplete())      snprintf(line, sizeof(line), "%s - %s", _song.text, _second.text);
  else if (_song.text[0])   strlcpy(line, _song.text, sizeof(line));
  else if (_second.text[0]) strlcpy(line, _second.text, sizeof(line));  // nothing else to show it in
  else                      line[0] = '\0';
  _publishPending = false;
  if (strcmp(line, config.station.title) == 0) return;  // the live field is the last string published
  config.setTitle(line);
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