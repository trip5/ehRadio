#include "filemanager.h"

#ifdef USE_SD

#include <FS.h>
#include <errno.h>
#include <esp_heap_caps.h>
#include <vector>
#include "sdmanager.h"
#include "config.h"
#include "display.h"
#include "netserver.h"
#include "player.h"
#include "logging.h"

FileManager filemanager;

// ==== Path helpers ====

// The FS wants a rooted path with no trailing slash and the UI can send either form. FATFS treats "/Music"
// and "Music/" as one file, but every protection here is a string comparison, so the two spellings would
// give two different answers. Normalise at the edge.
static String normalisePath(const String &raw) {
  String p = raw;
  p.trim();
  if (p.length() == 0) return String("/");
  if (!p.startsWith("/")) p = "/" + p;
  while (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}

// One path component: no separators and no "."/"..", so it cannot escape the folder the user is looking at.
// Upload names and rename targets pass through here - the browser is not a security boundary.
static bool isSafeName(const String &name) {
  if (name.length() == 0 || name.length() > 200) return false;
  if (name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
  if (name == "." || name == "..") return false;
  return true;
}

static String basenameOf(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash >= 0) return path.substring(slash + 1);
  return path;
}

static String parentOf(const String &path) {
  int slash = path.lastIndexOf('/');
  if (slash < 0) return String("/");
  if (slash == 0) return String("/");
  return path.substring(0, slash);
}

// Query-string parameters are parsed with the request line, so unlike a body they are ready in the handler.
static String argOf(AsyncWebServerRequest *request, const char *name) {
  if (!request->hasParam(name, false)) return String();
  return request->getParam(name, false)->value();
}

// Every JSON answer is state as of that request, so none of it may be cached - a stored listing would show
// the folder as it was before the change. netserver.cpp's locale and visuals handlers send the same headers.
static void sendJson(AsyncWebServerRequest *request, int code, const String &body) {
  AsyncWebServerResponse *response = request->beginResponse(code, "application/json", body);
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}

static void sendOk(AsyncWebServerRequest *request) {
  sendJson(request, 200, F("{\"ok\":true}"));
}

// `landed` is the byte count the device can VOUCH FOR in the target file, and it is sent only when there is one.
// It is the whole of the resume protocol on the device's side: the page takes it and re-sends only the remainder.
static void sendError(AsyncWebServerRequest *request, int code, const char *reason, uint64_t landed = 0) {
  String body = F("{\"ok\":false,\"error\":\"");
  body += reason;
  body += F("\"");
  if (landed) {
    char num[24];
    snprintf(num, sizeof(num), "%llu", (unsigned long long)landed);
    body += F(",\"landed\":");
    body += num;
  }
  body += F("}");
  sendJson(request, code, body);
}

// Escapes into a caller-owned buffer and NUL-terminates. A filename off the card is user data: one unescaped
// quote would break the response, and control characters go out as \u00XX.
static int jsonEscapeTo(char *dst, size_t room, const char *src) {
  size_t n = 0;
  for (const char *p = src; *p && n + 7 < room; p++) {
    unsigned char c = (unsigned char)*p;
    switch (c) {
      case '"':  dst[n++] = '\\'; dst[n++] = '"';  break;
      case '\\': dst[n++] = '\\'; dst[n++] = '\\'; break;
      case '\n': dst[n++] = '\\'; dst[n++] = 'n';  break;
      case '\r': dst[n++] = '\\'; dst[n++] = 'r';  break;
      case '\t': dst[n++] = '\\'; dst[n++] = 't';  break;
      default:
        if (c < 0x20) n += snprintf(dst + n, room - n, "\\u%04x", (unsigned)c);
        else          dst[n++] = (char)c;
    }
  }
  dst[n] = '\0';
  return (int)n;
}

// Every route except the two entry points needs the mode open; without it a page left in a tab would browse
// the card with playback unblocked, which is the one thing the mode exists to prevent.
static bool requireActive(AsyncWebServerRequest *request) {
  if (filemanager.active()) return true;
  sendError(request, 409, "not_active");
  return false;
}

// ==== Guards ====

bool FileManager::isProtected(const String &path) {
  // ONLY the data folder, under its current name. The root is not protected here - deleting it is refused by name in
  // hDelete() - and neither is /data, the old name, which is now ordinary clutter the user may delete.
  if (path == SD_DATA_DIR || path.startsWith(String(SD_DATA_DIR) + "/")) return true;
  return false;
}

bool FileManager::isPlaying(const String &path) const {
  if (config.getMode() != PM_SDCARD) return false;
  // A backstop rather than the normal protection: enter() stops the player, so nothing is usually in use.
  if (!player.isRunning()) return false;
  String playing = config.station.url;
  if (playing.length() == 0) return false;
  if (playing.equalsIgnoreCase(path)) return true;
  // A directory holding the playing file counts, so the next character must be the separator - "/Music" must
  // not match "/Music2". FATFS would unlink the entry and leave the decoder on a dead cluster chain.
  if (path.length() > 1 && playing.length() > path.length() + 1 &&
      playing.charAt(path.length()) == '/' &&
      playing.substring(0, path.length()).equalsIgnoreCase(path)) return true;
  return false;
}

// A mutation makes two things wrong: the derived pair (playlistsd.csv + indexsd.dat, dropped together because a valid
// index beside a truncated playlist reads as an empty card) and the in-RAM SD playlist, which nothing rebuilds while
// the manager is open - one walk, after the mode closes.  The bit also suppresses the resume, since a station NUMBER
// means a different file once the list has changed.
// The pair describes the card's PLAYABLE files, so a change that cannot alter that set cannot invalidate it - the boot
// validation re-counts exactly those and re-indexes only on a mismatch. Not setting the card-changed flag also spares
// the close-time re-index; a FOLDER asks for the drop because nothing here walks it.
static void dropSdDerivedFiles(bool audioChanged) {
  // Silent when it keeps them: keeping the pair is the ORDINARY case (every document, photo or file the player cannot
  // play), so a line saying so arrived once per upload and told nobody anything. The line that matters is the one at
  // the end of this function, and it prints only when the pair really existed and was removed.
  if (!audioChanged) return;
  bool dropped = false;
  if (sdman.exists(INDEX_SD_PATH))    { sdman.remove(INDEX_SD_PATH);    dropped = true; }
  if (sdman.exists(PLAYLIST_SD_PATH)) { sdman.remove(PLAYLIST_SD_PATH); dropped = true; }
  // A build that was interrupted leaves its half-built pair under the temporary names, and nothing else would ever
  // clear them - the next build removes them again, but only if there is a next build.  Sweep them with the pair.
  if (sdman.exists(INDEX_SD_TMP_PATH))    sdman.remove(INDEX_SD_TMP_PATH);
  if (sdman.exists(PLAYLIST_SD_TMP_PATH)) sdman.remove(PLAYLIST_SD_TMP_PATH);
  if (dropped) FUNCTIONLOG("SDFileManager", "SD playlist and index dropped; they will be rebuilt when the mode closes");
  filemanager.markCardChanged();
}

// The SPI upload marker is gone: a card with a large allocation unit uploads fine once the write unit matches its
// cluster, so no card is declared unusable for uploads and nothing writes or reads a verdict.

// ==== Directory removal ====

static bool removeRecursive(const String &path) {
  File entry = sdman.open(path);
  if (!entry) return false;

  if (!entry.isDirectory()) {
    entry.close();
    return sdman.remove(path);
  }

  // Names are collected first: deleting while walking advances the directory position under the removal, so
  // the walk skips entries and can stop early, leaving a partial tree behind.
  std::vector<String> children;
  File child = entry.openNextFile();
  while (child) {
    sdFeedWatchdog();  // a delete walk costs as much as the listing walk - unfed, the task WDT would abort the device
    filemanager.touch();  // and it takes minutes: unfed, the idle timeout would close the mode mid-delete
    children.push_back(basenameOf(child.name()));  // name() is the full path on ESP32 Arduino
    child = entry.openNextFile();
  }
  entry.close();

  String base = path;
  if (!base.endsWith("/")) base += "/";
  for (const String &name : children) {
    sdFeedWatchdog();  // each call removes files or opens another directory
    filemanager.touch();  // same clock, same reason
    if (!removeRecursive(base + name)) return false;
  }
  return sdman.rmdir(path);
}

// ==== Handlers ====

// GET /sdman/enter - opens the mode and is the only way in: the page calls it as it loads, so /sdmanager.html works as
// well as the address on the display.  Idempotent.  No route at bare "/sdman" (a plain URI matches an exact path OR a
// prefix plus "/", so it would answer every sibling), and no network test - _switchMode() already refuses a change
// unless the status is CONNECTED or SDOFFLINE, and NetServer::begin() returns before the server when offline.
static void hEnterApi(AsyncWebServerRequest *request) {
  filemanager.enter();
  sendOk(request);
}

// POST /sdman/done - the Done button at the foot of the page.
static void hDone(AsyncWebServerRequest *request) {
  // Refused while a file is mid-write: leave() removes the half-written file by design, so a Done press from a
  // second tab would throw away an upload nobody finished watching.  Between files _upFile is already closed.
  if (filemanager.uploadOpen()) {
    // Deliberately inert: no touch(), so a Done press cannot be used to keep a stalled upload's mode alive.  The
    // line exists so a refusal - a second tab, a second device - is visible in the log rather than silent.
    FUNCTIONLOG("SDFileManager", "Done refused, upload in progress");
    sendError(request, 409, "uploading");
    return;
  }
  // Same rule for a delete batch, and the same reason to be inert: the page's own buttons are already dimmed and
  // inert while one runs (see busy() in sdmanager.html), so what arrives here is a second tab or a second device.
  if (filemanager.busy()) {
    FUNCTIONLOG("SDFileManager", "Done refused, a delete is in progress");
    sendError(request, 409, "busy");
    return;
  }
  // Named here, because leave() cannot tell a Done press from its own timeout: until this line existed, a close
  // the user asked for and a close the countdown asked for were identical in the log.
  filemanager.leave(true, "Done pressed");
  request->redirect("/");
}

// Declared here because it is a shape of the one request the page makes about the transfer state; defined with that
// state further down this file.
static void sendUploadState(AsyncWebServerRequest *request);

// GET /sdman/info - the header line and the empty/error states depend on this.
static void hInfo(AsyncWebServerRequest *request) {
  if (!requireActive(request)) return;
  filemanager.touch();
  // The transfer-in-flight question goes to its own function, which lives with the state it reports.
  if (argOf(request, "up") == "1") {
    sendUploadState(request);
    return;
  }
  // One walk of the FAT, not two: usedBytes() counts free clusters (a whole-FAT walk), while cardSize() reads the CSD
  // the card reported at init and costs nothing.  On a large card the duplicate is seconds of blocked network task.
  const uint32_t t0 = millis();
  const uint64_t used = sdman.usedBytes();
  sdFeedWatchdog();
  const uint64_t total = sdman.cardSize();
  // Said only when it has something to say: the handler runs on every load, click and idle poll, so a line each time
  // was noise. The two things worth seeing are the figure changing and a walk that took unusually long.
  static uint64_t lastUsed = 0;
  static bool told = false;
  const uint32_t walkMs = millis() - t0;
  if (!told || used != lastUsed || walkMs > SDMAN_INFO_SLOW_MS) {
    told = true;
    lastUsed = used;
    FUNCTIONLOG("SDFileManager", "Disk used: %llu of %llu bytes (%lu ms walk)",
                (unsigned long long)used, (unsigned long long)total, (unsigned long)walkMs);
  }
  // The idle deadline goes out with the card figures, so the page can end itself when the device does.
  String body = F("{\"ok\":true,\"idle\":");
  body += String((unsigned long)filemanager.idleRemainingMs());
  body += F(",\"mounted\":");
  body += sdman.ready ? "true" : "false";
  body += F(",\"total\":");
  body += String((unsigned long)total);
  body += F(",\"used\":");
  body += String((unsigned long)used);
  body += F(",\"free\":");
  body += String((unsigned long)(total >= used ? total - used : 0));
  // The allocation unit, kept as a diagnostic: 0 means unknown rather than a guess.
  body += F(",\"au\":");
  body += String((unsigned long)sdman.allocationUnit());
  body += F("}");
  sendJson(request, 200, body);
}

// POST /sdman/mkdir?path=/Music/New
static void hMkdir(AsyncWebServerRequest *request) {
  if (!requireActive(request)) return;
  filemanager.touch();
  String path = normalisePath(argOf(request, "path"));
  if (path == "/") { sendError(request, 400, "bad_path"); return; }
  if (FileManager::isProtected(path)) { sendError(request, 403, "protected"); return; }
  String parent = parentOf(path);
  if (!sdman.exists(parent)) { sendError(request, 404, "no_parent"); return; }
  if (sdman.exists(path)) { sendError(request, 409, "exists"); return; }
  if (!isSafeName(basenameOf(path))) { sendError(request, 400, "bad_name"); return; }
  if (!sdman.mkdir(path)) { sendError(request, 500, "mkdir_failed"); return; }
  FUNCTIONLOG("SDFileManager", "Mkdir %s", path.c_str());
  dropSdDerivedFiles(false);   // a folder that has just been created holds nothing, playable or otherwise
  sendOk(request);
}

// POST /sdman/rename?from=/a.mp3&to=b.mp3 - same directory
// POST /sdman/move?from=/a.mp3&to=/Music/a.mp3 - possibly another directory
static void doRename(AsyncWebServerRequest *request, bool sameDirectory) {
  if (!requireActive(request)) return;
  filemanager.touch();
  String from = normalisePath(argOf(request, "from"));
  String to   = argOf(request, "to");
  to.trim();

  if (from == "/" || from.length() < 2) { sendError(request, 400, "bad_source"); return; }
  if (to.length() == 0) { sendError(request, 400, "bad_target"); return; }
  if (FileManager::isProtected(from)) { sendError(request, 403, "protected"); return; }
  if (filemanager.isPlaying(from)) { sendError(request, 409, "in_use"); return; }
  if (!sdman.exists(from)) { sendError(request, 404, "not_found"); return; }

  // Built through the parent: parentOf() answers "/" at the root, so a blind "/" + "/" + name would give
  // "//name" - a doubled separator FATFS may reject and no other path here has.
  String parent = parentOf(from);
  String target = sameDirectory ? ((parent == "/") ? ("/" + to) : (parent + "/" + to)) : to;
  target = normalisePath(target);

  if (!sameDirectory && FileManager::isProtected(target)) { sendError(request, 403, "protected"); return; }
  if (!isSafeName(basenameOf(target))) { sendError(request, 400, "bad_name"); return; }
  if (target == from) { sendOk(request); return; }   // a rename to itself is a no-op, not a failure
  if (!sdman.exists(parentOf(target))) { sendError(request, 404, "no_parent"); return; }
  if (sdman.exists(target)) { sendError(request, 409, "exists"); return; }
  if (!sdman.rename(from, target)) { sendError(request, 500, "rename_failed"); return; }

  FUNCTIONLOG("SDFileManager", "%s %s -> %s", sameDirectory ? "Renamed" : "Moved", from.c_str(), target.c_str());
  // Either name can be the playable one - a .mp3 renamed to .txt leaves the set, a .txt to .mp3 joins it - and a
  // FOLDER is assumed to hold playable files, because nothing here walks it.
  bool audioChanged = sdman.isAudioName(basenameOf(from).c_str()) || sdman.isAudioName(basenameOf(target).c_str());
  if (!audioChanged) {
    File entry = sdman.open(target);
    if (entry) { audioChanged = entry.isDirectory(); entry.close(); }
  }
  dropSdDerivedFiles(audioChanged);
  sendOk(request);
}

static void hRename(AsyncWebServerRequest *request) { doRename(request, true); }
static void hMove(AsyncWebServerRequest *request)   { doRename(request, false); }

// POST /sdman/delete - the whole selection as one newline-separated body: a page is 50 rows, the handler
// runs once the body is complete, and one request means one reload and one place to decide what is allowed.
static String _deleteBody;

static void onDeleteBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
  if (index == 0) _deleteBody = "";
  _deleteBody.concat((const char *)data, len);
}

static void hDelete(AsyncWebServerRequest *request) {
  if (!requireActive(request)) return;
  filemanager.touch();
  // The whole selection is handled inside this one request, so this is the operation the mode cannot be closed in
  // the middle of: leave() would tear it down and leave the card's derived files half-written.
  filemanager.markBusy(true);
  int deleted = 0, failed = 0;
  bool audioChanged = false;   // whether anything removed can have changed the card's PLAYABLE set

  int start = 0;
  while (start < (int)_deleteBody.length()) {
    // The selection is N FATFS deletes in one handler, and a *file* never reaches the fed loops inside
    // removeRecursive() - it returns at that function's isDirectory() branch - so the batch loop is the only
    // place a many-file delete (a whole card) can be fed.  Without it the task WDT aborts mid-write, which also
    // leaves the mount dirty for the next boot.  A tick per item lets the display keep up as well.
    // THE PLAYER QUEUE IS DRAINED HERE FOR THE SAME REASON, and listSD() already does it: the main loop is the
    // only OTHER drain and it is gated on the network status, so through a long burst the ~1-per-2-seconds ticks
    // from ticks() (PR_CHECKSD, PR_VUTONUS) had nowhere to go and the 10-slot queue filled - the field logged
    // "playerQueue overflow, dropped cmd=7" on a whole-card delete.  A dropped tick is harmless and is re-issued;
    // the drain is here because a dropped PR_PLAY or PR_STOP would not be.  One receive per item, and the
    // commands execute in this task exactly as they already do from the index walk.
    sdFeedWatchdog();
    filemanager.touch();
    player.loop();
    int nl = _deleteBody.indexOf('\n', start);
    if (nl < 0) nl = _deleteBody.length();
    String raw = _deleteBody.substring(start, nl);
    start = nl + 1;
    raw.trim();
    if (raw.length() == 0) continue;

    String path = normalisePath(raw);
    // The root is not deletable, and this is now the ONLY place that says so: isProtected() was narrowed to the data
    // folder, so the guard against a recursive wipe of the whole card had to come here with it. Everything else at the
    // root - a new folder, a rename, an upload - is allowed as before.
    if (path == "/") {
      FUNCTIONLOG("SDFileManager", "Refused to delete the card root");
      failed++;
      continue;
    }
    // One count on the way out, but the log keeps them apart: a protected item and a failed rmdir differ.
    if (FileManager::isProtected(path) || filemanager.isPlaying(path)) {
      FUNCTIONLOG("SDFileManager", "Refused to delete %s (protected or in use)", path.c_str());
      failed++;
      continue;
    }
    if (!sdman.exists(path)) {
      FUNCTIONLOG("SDFileManager", "Nothing to delete at %s", path.c_str());
      failed++;
      continue;
    }
    // Could this change what the player can play? A playable name, or a FOLDER whose contents are not read here - and
    // it is asked BEFORE the removal, because afterwards there is nothing left to ask.
    if (sdman.isAudioName(basenameOf(path).c_str())) {
      audioChanged = true;
    } else {
      File entry = sdman.open(path);
      if (entry) { if (entry.isDirectory()) audioChanged = true; entry.close(); }
    }
    if (removeRecursive(path)) {
      deleted++;
      FUNCTIONLOG("SDFileManager", "Deleted %s", path.c_str());
    } else {
      failed++;
      FUNCTIONLOG("SDFileManager", "Could not delete %s", path.c_str());
    }
  }
  _deleteBody = "";
  filemanager.markBusy(false);   // the long part is over: the invalidation and the answer are both quick

  if (deleted) dropSdDerivedFiles(audioChanged);

  String body = F("{\"ok\":");
  body += (failed == 0) ? "true" : "false";
  body += F(",\"deleted\":");
  body += deleted;
  body += F(",\"failed\":");
  body += failed;
  body += F("}");
  sendJson(request, 200, body);
}

// GET /sdman/download?path=/Music/a.mp3 - the row's download icon. The response carries the open File, so
// the transfer reads straight off the card with no buffer of our own.
static void hDownload(AsyncWebServerRequest *request) {
  if (!requireActive(request)) return;
  filemanager.touch();
  String path = normalisePath(argOf(request, "path"));
  if (!sdman.exists(path)) { sendError(request, 404, "not_found"); return; }
  File file = sdman.open(path, FILE_READ);
  if (!file) { sendError(request, 404, "not_found"); return; }
  bool isDir = file.isDirectory();
  file.close();
  if (isDir) { sendError(request, 400, "is_dir"); return; }
  // The download flag adds Content-Disposition, so the browser saves rather than renders the file.
  request->send(request->beginResponse(sdman, path, "application/octet-stream", true));
}

// ==== Listing ====

// The walk hands over one entry at a time and the send window decides how much a call may emit, so pending bytes sit in
// _listOut and drain a windowful at a time: memory is bounded to one entry and the document cannot be cut short.
// One listing at a time, as the playlist export assumes: the UI loads a folder and waits, and the browser
// does the sorting and the paging.
static File   _listDir;
static String _listPath;
static String _listOut;
static bool   _listStarted = false;
static bool   _listFirst   = true;
static bool   _listDone    = false;
// How long a listing may hold the card before a new request may declare it abandoned and take the handle over. A live
// walk finishes in a second or two of network time, so this only ever catches a client that left.
static constexpr uint32_t SDMAN_LIST_ABANDON_MS = 5000;
static uint32_t _listT0    = 0;   // when this walk started, for the cost line
static uint32_t _listCount = 0;   // entries emitted, for the same

static size_t sdmanListFiller(uint8_t *buffer, size_t maxLen, size_t index) {
  if (maxLen == 0) return 0;

  if (_listOut.length() == 0) {
    if (_listDone) return 0;   // the footer has gone out; this is what ends the response

    if (!_listStarted) {
      char esc[264];
      jsonEscapeTo(esc, sizeof(esc), _listPath.c_str());
      char head[48];
      snprintf(head, sizeof(head), "{\"ok\":true,\"idle\":%lu,\"path\":\"",
               (unsigned long)filemanager.idleRemainingMs());
      _listStarted = true;
      _listOut  = head;
      _listOut += esc;
      _listOut += F("\",\"entries\":[");
    } else if (_listDir) {
      // One entry per call, but the calls come back to back while the socket window is open, so a large folder
      // turns into seconds of blocked network task here - which is why the watchdog is fed every few entries.
      if ((_listCount & 0x07) == 0) sdFeedWatchdog();
      File entry = _listDir.openNextFile();
      if (entry) {
        String name = basenameOf(entry.name());   // name() is the full path on ESP32 Arduino
        const bool isDir = entry.isDirectory();
        const size_t size = isDir ? 0 : entry.size();
        entry.close();
        char esc[264];
        jsonEscapeTo(esc, sizeof(esc), name.c_str());
        char tail[48];
        snprintf(tail, sizeof(tail), "\",\"d\":%u,\"s\":%lu}", isDir ? 1u : 0u, (unsigned long)size);
        _listOut  = _listFirst ? "" : ",";
        _listOut += F("{\"n\":\"");
        _listOut += esc;
        _listOut += tail;
        _listFirst = false;
        _listCount++;
      } else {
        _listDir.close();   // the walk is over
        _listOut = F("]}");
        _listDone = true;
        FUNCTIONLOG("SDFileManager", "Walked %u entries in %lu ms",
                    (unsigned)_listCount, (unsigned long)(millis() - _listT0));
      }
    } else {
      _listOut = F("]}");
      _listDone = true;
    }
  }

  size_t n = _listOut.length();
  if (n > maxLen) n = maxLen;
  memcpy(buffer, _listOut.c_str(), n);
  _listOut.remove(0, n);
  return n;
}

// GET /sdman/list?path=/Music
static void hList(AsyncWebServerRequest *request) {
  if (!requireActive(request)) return;
  filemanager.touch();
  if (!sdman.ready) { sendError(request, 409, "no_card"); return; }
  // One walker at a time, and never while a file is being written: a listing keeps its directory handle in _listDir
  // between callbacks, so a second one would close the handle the first is reading from, and an upload writing at the
  // same time drops connections on an SPI card. The page retries both quietly.
  if (filemanager.uploadOpen()) { sendError(request, 409, "uploading"); return; }
  if (_listDir) {
    // A listing ABANDONED by its client leaves its handle open with no more callbacks coming, and this is the only
    // place that can close it.  So the refusal is time-limited: past the window the stale handle is closed.
    if (!_listDone && (millis() - _listT0) < SDMAN_LIST_ABANDON_MS) { sendError(request, 409, "busy"); return; }
    _listDir.close();
  }
  String path = normalisePath(argOf(request, "path"));
  if (!sdman.exists(path)) { sendError(request, 404, "not_found"); return; }
  _listDir = sdman.open(path);
  if (!_listDir || !_listDir.isDirectory()) {
    if (_listDir) _listDir.close();
    sendError(request, 400, "not_dir");
    return;
  }
  _listPath = path;
  _listOut = "";        // nothing may survive from an attempt that was cut off
  _listStarted = false;
  _listCount = 0;
  _listT0 = millis();
  _listFirst = true;
  _listDone = false;
  AsyncWebServerResponse *response = request->beginChunkedResponse("application/json", sdmanListFiller);
  // Same reason as sendJson(): a listing is state, not an asset.
  response->addHeader("Cache-Control", "no-cache, no-store, must-revalidate");
  response->addHeader("Pragma", "no-cache");
  response->addHeader("Expires", "0");
  request->send(response);
}

// ==== Upload ====

// Multipart chunks are written straight to the card, so a large file never lands in RAM. The target is the
// folder being viewed plus a sanitised name; the multipart filename is only a fallback, since a browser may
// send a path in it.
static File     _upFile;
static String   _upPath;
static uint64_t _upFree = 0;
static uint64_t _upBytes = 0;
static bool     _upSkipped = false;
static const char *_upReason = nullptr;
static int      _upCode = 400;
static uint32_t _upLastChunkMs = 0;   // the last chunk that arrived, for the stall deadline
static uint32_t _upLastIndex = 0;     // the highest multipart index seen, to recognise a superseded request's chunks
static bool     _upInWrite = false;   // a write is executing right now - never close the handle under it
static uint16_t _upRetries = 0;       // chunks the card refused outright and the retry rescued, per file
static uint16_t _upRetryBurst = 0;    // refusals IN A ROW with no successful chunk between them
static uint16_t _upPartialRescues = 0; // partial writes put back together from the card's own position
static bool     _upLagSettled = false; // "the reading was early" has been said once for this file
// Per-file totals across attempts: _filePath tells a retry of the same file, which continues the count, from a new one.
// This is what the "Finished Upload" line reports.
static String   _filePath;             // the file the totals below belong to
static uint32_t _fileStartedMs = 0;    // when its FIRST attempt began
static uint64_t _fileMoved = 0;        // bytes moved by every attempt of it, summed
static uint16_t _fileAttempts = 0;     // attempts started
static uint16_t _fileRetries = 0;      // rescued refusals, summed
static uint16_t _filePartialRescues = 0; // repaired partial writes, summed
static bool     _fileFolded = false;   // this attempt's bytes are already in the file totals

static uint32_t _upStartedMs = 0;      // when the file was opened, for the per-file transfer timing
static uint64_t _upVerified = 0;       // the last position PROVEN to be on the card by the verification check
static String   _upResumePath;         // the file that verified position belongs to
static uint64_t _upResumeAt = 0;       // what the next request may resume from (0 = nothing to resume from)
static uint64_t _upResumedFrom = 0;    // what THIS request resumed from, for the log
static uint64_t _upNextCheck = 0;     // the byte count at which the stream must next be proved real

// Add the attempt that is ENDING to the file's totals, ONCE: it can end three ways (a response, a stall release, a
// cut-off) and two of them can run for the same attempt, which is why this is guarded.
static void fileFoldAttempt() {
  if (_fileFolded || _upPath.length() == 0) return;
  _fileFolded = true;
  _fileMoved += (_upBytes - _upResumedFrom);
  _fileRetries += _upRetries;
  _filePartialRescues += _upPartialRescues;
}
/* ==== The cluster assembly ====
   ONE FILE SYSTEM WRITE PER CLUSTER.  A multipart chunk is ~1436 bytes, never a whole number of sectors, so writing
   each as it arrives leaves the position misaligned and the file system falls back to one single-sector command per
   512 bytes.  Holding bytes until a whole CLUSTER is ready makes FatFs take its direct multi-sector path - one
   WRITE_MULTIPLE_BLOCK (CMD25) per cluster.  The unit is the card's own allocation unit (a 512-byte card stands aside);
   the buffer is one allocation of unit + slack; and the accounting follows the WRITE, which is why resumeFloor()
   rounds to this unit and the tail is flushed at the end of the stream and again before the final size check. */
#define SDMAN_ASM_SLACK    4096UL    // room for one chunk on top of a whole unit, so a feed never needs a split
#define SDMAN_ASM_UNIT_MAX 65536UL   // the largest unit worth assembling; a larger cluster is not worth the RAM

static uint8_t *_asmBuf = nullptr;   // unit + slack; taken when an upload opens, released when the manager closes
static size_t   _asmCap = 0;         // its size, so the next file in a batch reuses it instead of reallocating
static size_t   _asmStart = 0;       // first byte in it not yet written
static size_t   _asmEnd = 0;         // one past the last byte received
static uint32_t _asmUnit = 0;        // the unit being assembled; 0 = hand each chunk straight to the write
static const uint8_t *_asmPayload = nullptr;   // what the next write call is given
static size_t   _asmPayloadLen = 0;
static bool     _asmAnnounced = false;   // the decision for this session has been logged once
// What the writes cost, for the one line an attempt prints - the instrument the cluster-size question is settled with.
static uint16_t _asmWrites = 0;
static uint64_t _asmBytes = 0;      // what those writes actually carried - NOT the unit multiplied by the count
static uint32_t _asmMsMin = 0, _asmMsMax = 0, _asmMsTotal = 0;

static void asmRelease() {
  if (_asmBuf) { heap_caps_free(_asmBuf); _asmBuf = nullptr; }
  _asmCap = 0;
  _asmStart = 0;
  _asmEnd = 0;
  _asmUnit = 0;
  _asmPayload = nullptr;
  _asmPayloadLen = 0;
  _asmAnnounced = false;
}

// Between files and attempts: the buffer stays where it is, its contents do not.
static void asmReset() {
  _asmStart = 0;
  _asmEnd = 0;
  _asmPayload = nullptr;
  _asmPayloadLen = 0;
}

// Settles the unit for this upload and allocates a buffer if needed. Called when the handle opens, so a refused or
// skipped file never pays; the buffer outlives the file, so a batch allocates once.
static void asmBegin() {
  uint32_t unit = sdman.allocationUnit();
  if (unit < 512) unit = 512;                 // unknown (0) and any nonsense are treated as a plain sector
  if (unit > SDMAN_ASM_UNIT_MAX) unit = SDMAN_ASM_UNIT_MAX;
  if (unit <= 512) {
    // Already a whole sector per write: the assembly would add a copy and a buffer and nothing else. Said once, because
    // "did it turn on" is the first question asked of this code.
    if (!_asmAnnounced) {
      _asmAnnounced = true;
      FUNCTIONLOG("SDFileManager", "Cluster assembly off: the allocation unit is %lu bytes, so one write is already one unit",
                  (unsigned long)unit);
    }
    _asmUnit = 0;
    return;
  }
  const size_t need = (size_t)unit + SDMAN_ASM_SLACK;
  if (_asmCap >= need) { _asmUnit = unit; return; }   // this session's buffer is already big enough
  asmRelease();
  uint8_t *buf = nullptr;
  const char *where = nullptr;
  if (psramFound()) {
    buf = (uint8_t *)heap_caps_malloc(need, MALLOC_CAP_SPIRAM);
    if (buf) where = "PSRAM";
  }
  if (!buf) {
    buf = (uint8_t *)heap_caps_malloc(need, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (buf) where = "the internal heap";
  }
  if (!buf) {
    // No room: the upload runs exactly as before. A held cluster must never become a condition of uploading at all.
    FUNCTIONLOG("SDFileManager", "Cluster assembly off: no %u-byte buffer (largest free internal block %u bytes) - chunks go to the card as they arrive",
                (unsigned)need, (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
    _asmUnit = 0;
    return;
  }
  _asmBuf = buf;
  _asmCap = need;
  _asmUnit = unit;
  _asmAnnounced = true;
  FUNCTIONLOG("SDFileManager", "Cluster assembly: %lu-byte unit, %u-byte buffer from %s",
              (unsigned long)unit, (unsigned)need, where);
}

// Takes one chunk and works out the payload for the next write: whole units only, never more than the buffer holds.
// Returns false when the chunk cannot be assembled (the assembly is off, or the chunk is bigger than the whole buffer),
// and the caller then writes its own chunk as it always has.
static bool asmFeed(const uint8_t *chunk, size_t chunkLen) {
  if (_asmUnit == 0 || _asmBuf == nullptr) return false;
  if (_asmEnd + chunkLen > _asmCap) {
    // Compaction, the only case that copies: the unwritten remainder moves to the front. In the steady state nothing
    // moves at all.
    const size_t hold = _asmEnd - _asmStart;
    if (hold) memmove(_asmBuf, _asmBuf + _asmStart, hold);
    _asmStart = 0;
    _asmEnd = hold;
    if (_asmEnd + chunkLen > _asmCap) return false;   // only a chunk larger than the assembly buffer gets here
  }
  memcpy(_asmBuf + _asmEnd, chunk, chunkLen);
  _asmEnd += chunkLen;
  const size_t have  = _asmEnd - _asmStart;
  const size_t whole = have - (have % _asmUnit);
  if (whole == 0) {
    _asmPayload = nullptr;   // nothing whole yet: the bytes wait here for the next chunk
    _asmPayloadLen = 0;
    return true;
  }
  _asmPayload = _asmBuf + _asmStart;
  _asmPayloadLen = whole;
  return true;
}

// Retires the bytes this write call accepted. A short write leaves the rest in the buffer, uncounted - the attempt is
// ending anyway.
static void asmCommit(size_t wrote) {
  if (_asmUnit == 0) return;
  const size_t retired = (wrote < _asmPayloadLen) ? wrote : _asmPayloadLen;
  _asmStart += retired;
  _asmBytes += retired;      // the bytes this write carried, which is what the reported KB/s divides
  _asmPayload = nullptr;
  _asmPayloadLen = 0;
  if (_asmStart >= _asmEnd) _asmStart = _asmEnd = 0;
}

static void asmNote(uint32_t ms) {
  _asmWrites++;
  _asmMsTotal += ms;
  if (_asmWrites == 1 || ms < _asmMsMin) _asmMsMin = ms;
  if (ms > _asmMsMax) _asmMsMax = ms;
}

// ONE line per ATTEMPT, only when the assembly wrote something: how many writes went to the card, what each cost, and
// the throughput inside them. Reset here, so a second call for the same attempt prints nothing.
static void asmLogAttempt(const char *path) {
  if (_asmWrites == 0) return;
  const uint32_t n = _asmWrites;
  // The bytes the writes CARRIED, never the unit times the count: a 1151-byte file is one write of 1151 bytes, and
  // "up to 65536 bytes ... 64000 KB/s" said nothing true. A short write contributes only what it accepted.
  const uint32_t kbPerSec = (_asmMsTotal && _asmBytes)
                              ? (uint32_t)((_asmBytes / 1024ULL) * 1000ULL / _asmMsTotal) : 0;
  FUNCTIONLOG("SDFileManager", "Cluster writes for %s: %u write(s) of up to %lu bytes, %lums..%lums each, %llu bytes in %lums (%lu KB/s inside them)",
              path, (unsigned)n, (unsigned long)_asmUnit, (unsigned long)_asmMsMin, (unsigned long)_asmMsMax,
              (unsigned long long)_asmBytes, (unsigned long)_asmMsTotal, (unsigned long)kbPerSec);
  _asmWrites = 0;
  _asmBytes = 0;
  _asmMsMin = _asmMsMax = _asmMsTotal = 0;
}

// The offset a retry may resume from: the last proven position, rounded DOWN to the assembly unit so a resumed transfer
// stays aligned. With the assembly off (a 512-byte unit) this is the position itself.
static uint64_t resumeFloor(uint64_t verified) {
  if (_asmUnit == 0) return verified;
  return verified - (verified % _asmUnit);
}

// Writes the part-cluster tail still held, so a file's LAST few kilobytes reach the card. Called when the stream ends
// and again inside hUploadDone, before the size check.
static void sdFlushTail() {
  if (_asmUnit == 0 || _asmBuf == nullptr) return;
  if (_asmEnd == _asmStart) return;
  const size_t n = _asmEnd - _asmStart;
  const uint8_t *from = _asmBuf + _asmStart;
  _asmStart = _asmEnd = 0;   // the tail is retired whatever happens next: it is never written twice
  // Nothing to write it into, or the transfer is already over: the bytes are dropped, which is correct - the resume
  // position is a VERIFIED one and never depends on a tail that was never on the card.
  if (!_upFile || _upReason != nullptr) return;
  errno = 0;
  const uint32_t tailStart = millis();
  _upInWrite = true;
  const size_t wrote = _upFile.write(from, n);
  _upInWrite = false;
  const uint32_t tailMs = millis() - tailStart;
  _upBytes += wrote;
  _asmBytes += wrote;   // the tail is a write like any other, so it counts toward the figure the line reports
  asmNote(tailMs);
  if (wrote != n) {
    _upReason = "write_failed";
    _upCode = 500;
    FUNCTIONLOG("SDFileManager", "The last %u bytes of %s would not go down (%u of %u written, errno %d)",
                (unsigned)n, _upPath.c_str(), (unsigned)wrote, (unsigned)n, errno);
  }
}

// One owner for every way an upload ends badly: a chunk that never came, the mode closing, a browser that vanished.
// The reason is set BEFORE the close so a late hUploadDone() reports it, and the partial file is taken away - a
// truncated track under a listed name is worse than no track.  `resumable` keeps it instead, but only from a VERIFIED
// position: appending onto bytes we cannot vouch for would splice two versions of a file together.
static void abortUpload(const char *reason, int code, bool resumable = false) {
  if (!_upFile) return;
  const uint32_t wrote = (uint32_t)_upBytes;
  const uint64_t verified = _upVerified;
  // Fold this attempt into the file's totals BEFORE anything is cleared: this is one of the three ways an attempt can
  // end and the one that never reaches hUploadDone(), so a released 8.6 MB attempt was missing from its file's total.
  fileFoldAttempt();
  asmLogAttempt(_filePath.c_str());
  _upFile.close();
  // The held-back bytes are OURS, not the card's: a resume starts from the proven position and these were never
  // written. Only the contents go; the buffer is kept for the next file.
  asmReset();
  if (_upPath.length()) {
    if (resumable && verified) {
      _upResumePath = _upPath;
      _upResumeAt = resumeFloor(verified);
      FUNCTIONLOG("SDFileManager", "Upload stopped at %lu bytes with %llu verified on the card - keeping %s so the next attempt sends only the remainder",
                  (unsigned long)wrote, (unsigned long long)verified, _upPath.c_str());
    } else {
      sdman.remove(_upPath);
      FUNCTIONLOG("SDFileManager", "Upload aborted at %lu bytes - removed %s (%s)",
                  (unsigned long)wrote, _upPath.c_str(), reason);
    }
  }
  _upPath = "";
  // _upBytes is deliberately NOT cleared: hUploadDone() may still run and reports how far the transfer got, and zeroing
  // it here made a released 6.4 MB transfer print "0 bytes moved". The next request's index==0 resets it.
  _upVerified = 0;
  // A reason already set is the honest one and is kept: the stall branch runs after a chunk may have failed, and
  // overwriting write_failed with stalled destroyed the only evidence of why the transfer died.
  if (_upReason == nullptr) {
    _upReason = reason;
    _upCode = code;
  }
}

// ?up=1: the state of the transfer in flight, and NOTHING else - /sdman/info's body walks the FAT and this is the
// page's transfer watchdog asking every two seconds, so the page learns a transfer is lost without waiting for the
// request to end while the browser is still pushing the rest of the file.
//   open   - a handle is open for writing; the retry waits on this, because a retry sent before the stall branch closes
//            it and records the resume offset would be taken as a FRESH file and truncate the target.
//   lost   - the mode holds an open transfer that has already failed (tied to the handle, so a stale `lost` cannot
//            abort the next file's request).
//   up     - the path that offset belongs to.
//   landed - the position PROVEN to be on the card: what a retry may resume from.
static void sendUploadState(AsyncWebServerRequest *request) {
  char esc[256];
  jsonEscapeTo(esc, sizeof(esc), _upPath.c_str());
  String up = F("{\"ok\":true,\"open\":");
  up += _upFile ? "true" : "false";
  up += F(",\"lost\":");
  up += (_upFile && _upReason) ? "true" : "false";
  up += F(",\"landed\":");
  // Floored to the assembly unit, exactly as the resume record is: the page sends this figure back as its offset and
  // the device only accepts an offset it advertised, so the two must be the same number.
  up += String((unsigned long long)resumeFloor(_upVerified));
  up += F(",\"up\":\"");
  up += esc;
  up += F("\"}");
  sendJson(request, 200, up);
}

// Forget a resume record AND take the file away. Keeping the partial makes a resume possible, but only while the page
// is coming back for it: once a different file starts - or the mode closes - an abandoned partial is a truncated track
// under a name the page has listed, which the design forbids. A field run left a 9.25 MB half-file that way.
static void discardResume() {
  if (_upResumeAt && _upResumePath.length() && sdman.remove(_upResumePath)) {
    FUNCTIONLOG("SDFileManager", "Discarded the abandoned partial %s (%llu bytes)",
                _upResumePath.c_str(), (unsigned long long)_upResumeAt);
  }
  _upResumeAt = 0;
  _upResumePath = "";
  _upVerified = 0;
}

static void onUploadChunk(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
  // Re-tested per chunk, not just at the start: the mode can end (timeout, card removal) mid-file. The
  // reason is reported by the request handler at the end.
  if (!filemanager.active()) {
    // Close what this upload opened.  The next attempt's index == 0 would close it, but a batch that stops
    // here would otherwise hold a card handle until then.
    if (_upFile) _upFile.close();
    _upReason = "not_active";
    _upCode = 409;
    return;
  }
  // A chunk from a superseded request: the multipart index restarts at 0 with every request, so an index below
  // the highest seen cannot belong to this transfer and is dropped before the clock is stamped.
  if (index && index < _upLastIndex) return;
  _upLastIndex = index;
  // Refreshes the idle clock and feeds the task watchdog: a chunk write can block for seconds on an SPI card,
  // and AsyncTCP's task is subscribed to the WDT, so an unfed slow write reboots the device mid-file.
  filemanager.touch();
  sdFeedWatchdog();
  // Stamped first: loop()'s stall deadline measures from the last chunk that arrived, so a transfer that goes
  // quiet is silence, not an upload still in flight.
  _upLastChunkMs = millis();

  if (index == 0) {
    _upReason = nullptr;
    _upFree = 0;
    _upBytes = 0;
    _upSkipped = false;
    _upStartedMs = 0;
    // A cut-off attempt leaves the handle open and the file half-written; this closes it, standing in for the
    // missing UPLOAD_FILE_ABORTED notification, and reports how far the file got.
    if (_upFile) {
      FUNCTIONLOG("SDFileManager", "Previous upload was cut off: %s stopped at %lu bytes",
                  _upPath.length() ? _upPath.c_str() : "(name unknown)", (unsigned long)_upBytes);
      _upFile.close();
    }
    _upPath = "";
    _upRetries = 0;
    _upRetryBurst = 0;
    _upPartialRescues = 0;
    _upNextCheck = 0;
    _upVerified = 0;
    _upResumedFrom = 0;
    asmReset();          // a new file starts with an empty buffer: nothing is held over from the last one
    _asmWrites = 0;      // and its write statistics belong to it alone
    _asmBytes = 0;
    _asmMsMin = _asmMsMax = _asmMsTotal = 0;

    String dir = normalisePath(argOf(request, "path"));
    String name = argOf(request, "name");
    if (name.length() == 0) name = basenameOf(filename);
    // Set by the Skip Existing button; Replace sends nothing.
    bool skip = argOf(request, "skip") == "1";

    if (name.length() == 0 || !isSafeName(name)) { _upReason = "bad_name";    _upCode = 400; }
    else if (!sdman.ready)                       { _upReason = "no_card";     _upCode = 409; }
    else if (!sdman.exists(dir))                 { _upReason = "no_dir";      _upCode = 404; }
    else {
      String target = (dir == "/") ? ("/" + name) : (dir + "/" + name);
      // The per-file counters are keyed by path: a retried file continues them, a different file starts over.
      if (target != _filePath) {
        _filePath = target;
        _fileStartedMs = millis();
        _fileAttempts = 0;
        _fileMoved = 0;
        _fileRetries = 0;
        _filePartialRescues = 0;
      }
      _fileAttempts++;
      _fileFolded = false;   // the attempt that just ended has been folded; this one is not yet
      _upLagSettled = false;
      // RESUME: the page may send only the remainder of a file whose previous attempt the card ended, with
      // offset=N. N must be exactly the value advertised for the same path - appending at a guess would splice two
      // versions of the file together - and a mismatch simply restarts the file. Computed before the refusal chain
      // because keeping the partial makes the target exist, so Skip Existing would otherwise refuse its own retry.
      const uint64_t wantOffset = strtoull(argOf(request, "offset").c_str(), nullptr, 10);
      const bool canResume = (wantOffset > 0 && wantOffset == _upResumeAt &&
                              target == _upResumePath && sdman.exists(target));
      if (FileManager::isProtected(target))      { _upReason = "protected";   _upCode = 403; }
      else if (filemanager.isPlaying(target))    { _upReason = "in_use";      _upCode = 409; }
      else if (skip && !canResume && sdman.exists(target)) {
        // Skip Existing: nothing is opened, so nothing is truncated and no room is reserved for a write that
        // will not happen. The request answers "skipped" and the page names the file after the batch.
        _upSkipped = true;
      }
      else {
        if (_upResumePath.length() && _upResumePath != target) {
          // A different file: that position means nothing here, and the file it referred to must not be left
          // half-written on the card either.
          discardResume();
        }
        bool resuming = false;
        if (canResume) {
          _upFile = sdman.open(target, "r+");   // read/write and NO truncate, which is what makes a resume possible
          if (_upFile) {
            resuming = _upFile.seek((uint32_t)wantOffset);
            if (!resuming) _upFile.close();
          }
        }
        if (wantOffset > 0 && !resuming) {
          // An offset that cannot be honoured is refused: the request carries only a slice, and writing it as a
          // fresh upload would truncate the target and put the tail at position zero.
          _upReason = "resume_refused";
          _upCode = 409;
          FUNCTIONLOG("SDFileManager", "Resume refused for %s (asked for %llu, we can vouch for %llu) - the page will send it whole",
                      target.c_str(), (unsigned long long)wantOffset, (unsigned long long)_upResumeAt);
        }
        if (!resuming && _upReason == nullptr) {
          // No exists() test here: FILE_WRITE truncates, so this replaces an existing name, which is the point of
          // re-uploading a corrected track.
          _upFile = sdman.open(target, FILE_WRITE);
        }
        if (!_upFile) { _upReason = "open_failed"; _upCode = 500; }
        else {
          // Measured after the open: the truncate releases the room the replacement needs, so measuring first
          // would refuse an overwrite that fits once the old copy goes.
          uint64_t total = sdman.totalBytes();
          uint64_t used  = sdman.usedBytes();
          _upFree = (total > used) ? (total - used) : 0;
          if (_upFree == 0) {
            // A zero free figure switches the limit check off below, so it is reported.
            FUNCTIONLOG("SDFileManager", "Free space reads as 0 (total %llu, used %llu) - the limit check is off",
                        (unsigned long long)total, (unsigned long long)used);
          }
          _upPath = target;
          _upStartedMs = millis();
          // The write unit and its buffer, settled only now so a refused or skipped file never pays for them.
          asmBegin();
          if (resuming) {
            // Accounting continues from the vouchable position, so the checks below keep their meaning: _upBytes
            // is what the file should hold.
            _upBytes = wantOffset;
            _upVerified = wantOffset;
            _upResumedFrom = wantOffset;
            _upNextCheck = wantOffset + SDMAN_UPLOAD_VERIFY_BYTES;
            FUNCTIONLOG("SDFileManager", "Resuming %s at %llu bytes - only the remainder is sent",
                        target.c_str(), (unsigned long long)wantOffset);
          }
          // Logged at the start so "upload refused" and "the request never arrived" are distinguishable.
          FUNCTIONLOG("SDFileManager", "Upload start %s (client name '%s')", target.c_str(), filename.c_str());
        }
      }
    }
  }

  if (_upReason == nullptr && len) {
    // THE CLUSTER ASSEMBLY. Hold this chunk until a whole unit is ready, so the file system is handed one aligned write
    // per cluster instead of one single-sector command per 512 bytes - see the note beside the buffer. When the unit is
    // a plain sector, or the buffer could not be allocated, asmFeed() says no and this chunk goes straight to the write
    // below. Nothing is advanced here: the accounting follows the WRITE, which asmCommit() does. Guarded on the handle
    // too, because a SKIPPED upload still sends its chunks and there is nothing to write them to.
    if (_upFile && asmFeed(data, len)) {
      if (_asmPayloadLen) {
        data = (uint8_t *)_asmPayload;
        len = _asmPayloadLen;
      } else {
        len = 0;   // nothing whole yet: the bytes sit in the buffer and there is nothing to write
      }
    }
    // Stop at the free-space limit rather than filling the card: a half-written file the user is told about
    // beats a full disk. 507 is the storage audit's code and the page has its own message for it.
    // _upBytes, not _upFile.position(): an ftell on a buffered FATFS write stream is a second opinion about a
    // position this function already owns, and asking the stream while it is being written is a hazard with no
    // payoff - the count is right here.
    if (_upFree && (_upBytes + len) > _upFree) {
      _upReason = "no_space";
      _upCode = 507;
    } else if (_upFile && len) {
      // The result is checked: a short write ends this file and hUploadDone reports it as failed. errno and the
      // duration go in the line because a byte count cannot tell a refusal, a dirty mount and a slow failure apart.
      errno = 0;
      const uint32_t writeStart = millis();
      _upInWrite = true;
      size_t wrote = _upFile.write(data, len);
      uint32_t writeMs = millis() - writeStart;
      int writeErr = errno;
      // Taken on the first attempt, so a rescue below cannot replace the figure the cluster burst itself took.
      if (_asmPayloadLen) asmNote(writeMs);
      // A PARTIAL write is put back together, not failed: after a flush, size() is what is really on the card, so
      // a shortfall that has not fallen behind _upBytes is the tail of this chunk and is re-sent from the card's own
      // figure. If the shortfall reaches back past _upBytes those bytes are gone and the file is failed.
      if (wrote > 0 && wrote < len) {
        _upFile.flush();
        const uint64_t onCard = (uint64_t)_upFile.size();
        const uint64_t want   = _upBytes + len;
        if (onCard >= want) {
          wrote = len;                  // nothing was actually missing; the short return was the only symptom
        } else if (onCard >= _upBytes) {
          const size_t lost = (size_t)(want - onCard);
          if (_upFile.seek((uint32_t)onCard)) {
            errno = 0;
            const size_t again = _upFile.write(data + (len - lost), lost);
            if (again == lost) {
              wrote = len;              // whole on the medium now, so _upBytes + len is the truth again
              writeErr = 0;
              _upPartialRescues++;
              _upRetryBurst = 0;
              // A repaired write proves nothing: a card that shortened one write can shorten the next, so the
              // stream is verified on this chunk rather than at the next checkpoint.
              _upNextCheck = _upBytes + wrote;
              if (_upPartialRescues == 1) {
                FUNCTIONLOG("SDFileManager", "A partial write at %s was repaired: %llu bytes were on the card, its last %u were re-sent",
                            _upPath.c_str(), (unsigned long long)onCard, (unsigned)lost);
              }
            } else {
              FUNCTIONLOG("SDFileManager", "A partial write at %s could not be repaired (%u of %u bytes re-sent)",
                          _upPath.c_str(), (unsigned)again, (unsigned)lost);
            }
          }
        }
      }
      // A refused chunk is retried, because nothing was accepted and the stream has not moved. EIO arrives when the
      // card will not take the next write while programming the previous block, a timing failure that clears itself.
      // A burst of consecutive refusals means the card is gone; otherwise the budget is generous by design.
      if (wrote == 0 && len) {
        const uint16_t rescueBudget = (uint16_t)(SDMAN_UPLOAD_RETRY_FLOOR + (_upBytes / SDMAN_UPLOAD_RETRY_PER_BYTES));
        const bool burst      = (_upRetryBurst + 1) >= SDMAN_UPLOAD_RETRY_BURST;
        const bool overBudget = _upRetries >= rescueBudget;
        if (burst || overBudget) {
          // Named, because the two mean opposite things about the card: a burst is it giving up.
          FUNCTIONLOG("SDFileManager", "The card refused a chunk at %s and it was not retried (%s: %u of %u rescues used, refusal %u in a row)",
                      _upPath.c_str(), burst ? "burst - the card is not recovering" : "rescue budget reached",
                      (unsigned)_upRetries, (unsigned)rescueBudget, (unsigned)(_upRetryBurst + 1));
          _upRetryBurst++;
        } else {
          // The pause grows with consecutive refusals, so an isolated one pays only the base delay and a card that
          // refuses again at once gets twice as long to finish what it is programming.
          uint32_t pause = SDMAN_UPLOAD_RETRY_DELAY_MS;
          for (uint8_t i = 0; i < _upRetryBurst && pause && pause < SDMAN_UPLOAD_RETRY_MAX_DELAY_MS; i++) pause *= 2;
          if (pause > SDMAN_UPLOAD_RETRY_MAX_DELAY_MS) pause = SDMAN_UPLOAD_RETRY_MAX_DELAY_MS;
          if (pause) vTaskDelay(pdMS_TO_TICKS(pause));
          errno = 0;
          const uint32_t retryStart = millis();
          wrote = _upFile.write(data, len);
          writeMs = millis() - retryStart;
          writeErr = errno;
          if (wrote == len) {
            _upRetries++;
            _upRetryBurst = 0;
            // A rescued refusal proves nothing either, so the stream is verified now: the field pattern is refusal,
            // rescue accepted, then the card swallowing the whole next window. Asking here costs one flush and a
            // size(), fails when the card stopped telling the truth, and leaves the resume as short as possible.
            _upNextCheck = _upBytes + wrote;
            // One line per file, on the first rescue: a flaky card can rescue many chunks.
            if (_upRetries == 1) {
              FUNCTIONLOG("SDFileManager", "The card refused a chunk at %s; the retry wrote it (%u bytes in %lums) - further rescues this file are counted, not logged",
                          _upPath.c_str(), (unsigned)len, (unsigned long)writeMs);
            }
          } else {
            // The rescue was refused too: named with the burst count, because the next refusal ends the file.
            _upRetryBurst++;   // a failed retry counts toward the burst
            FUNCTIONLOG("SDFileManager", "The rescue of a refused chunk at %s was refused too (%u bytes, errno %d, %u in a row)",
                        _upPath.c_str(), (unsigned)len, writeErr, (unsigned)_upRetryBurst);
          }
        }
      }
      // Verified every so many bytes, not only at the end: flush() then size() is the only pair that tells the
      // truth about the medium, and a card that discards the stream buffer otherwise surfaces megabytes later.
      bool streamOk = true;
      if (wrote == len && SDMAN_UPLOAD_VERIFY_BYTES && (_upBytes + wrote) >= _upNextCheck) {
        const uint32_t verifyStart = millis();
        _upFile.flush();
        const uint64_t onCard = (uint64_t)_upFile.size();
        if (onCard < _upBytes + wrote) {
          // A shortfall here is either a card still programming or a lost write, and the two want opposite fixes,
          // so the card is given time to finish and the size is read again.
          vTaskDelay(pdMS_TO_TICKS(SDMAN_UPLOAD_LAG_SETTLE_MS));
          _upFile.flush();
          const uint64_t onCardLater = (uint64_t)_upFile.size();
          if (onCardLater >= _upBytes + wrote) {
            // It caught up: nothing was lost. Said once per file, so the log keeps its shape.
            if (!_upLagSettled) {
              _upLagSettled = true;
              FUNCTIONLOG("SDFileManager", "The card read %llu bytes short at %s and had caught up after %u ms - an early reading, not a lost write",
                          (unsigned long long)((_upBytes + wrote) - onCard), _upPath.c_str(),
                          (unsigned)SDMAN_UPLOAD_LAG_SETTLE_MS);
            }
          } else {
            streamOk = false;
            // The card really is behind and those bytes are gone, so the file fails at once rather than being ground
            // down with rescues. card_lagging is the reason the page retries as a whole file.
            _upReason = "card_lagging";
            _upCode = 500;
            // The resume position is the card's own size, a lower bound on what is committed, not the last passing
            // checkpoint: a checkpoint is tens of KB behind and made a stalling card re-send the same bytes for
            // ever. resumeFloor() aligns it to the unit the writes are assembled to.
            _upVerified = onCard;
            FUNCTIONLOG("SDFileManager", "The card is %llu bytes behind at %s (%llu of %llu on the card, %llu after %u ms more, errno %d, check took %lums) - stopping here; the retry continues from %llu",
                        (unsigned long long)((_upBytes + wrote) - onCard), _upPath.c_str(),
                        (unsigned long long)onCard, (unsigned long long)(_upBytes + wrote),
                        (unsigned long long)onCardLater, (unsigned)SDMAN_UPLOAD_LAG_SETTLE_MS, errno,
                        (unsigned long)(millis() - verifyStart), (unsigned long long)_upVerified);
            writeErr = errno;
          }
        } else {
          _upNextCheck = _upBytes + wrote + SDMAN_UPLOAD_VERIFY_BYTES;
          // The only position we have proved is on the card, so the only one a later attempt may resume from.
          _upVerified = _upBytes + wrote;
        }
      }
      _upInWrite = false;
      _upBytes += wrote;
      // Retire the bytes this write accepted; a short write leaves the rest uncounted for the next index == 0.
      asmCommit(wrote);
      if (!streamOk) {
        _upReason = "write_failed";
        _upCode = 500;
      } else if (wrote != len) {
        FUNCTIONLOG("SDFileManager", "Short write at %s (%u of %u bytes, errno %d, after %lums)%s",
                    _upPath.c_str(), (unsigned)wrote, (unsigned)len, writeErr, (unsigned long)writeMs,
                    (wrote == 0) ? " - the card refused it" : " - stopping here; the retry sends the remainder");
        _upReason = "write_failed";
        _upCode = 500;
      } else if (writeMs >= 1000) {
        // A chunk that takes a second or more is the card answering slowly - the shape of the write that never
        // returns.
        FUNCTIONLOG("SDFileManager", "Slow write at %s (%u bytes took %lums)",
                    _upPath.c_str(), (unsigned)len, (unsigned long)writeMs);
      }
    }
  }

  if (final && _upFile) {
    // The stream ended, so the part-cluster tail has nowhere else to go - and hUploadDone measures the card, so it
    // must be written before this returns.
    sdFlushTail();
    _upFile.close();
  }
}

// The request handler runs after the last chunk, so the outcome is reported here.
static void hUploadDone(AsyncWebServerRequest *request) {
  filemanager.touch();
  // Before anything is counted, measured or reported: the held-back tail must be on the card first. A no-op when
  // onUploadChunk already flushed it; the safety net for streams that end without the last callback.
  sdFlushTail();
  // Printed only when it says what the outcome line cannot: the handle still open (the browser abandoned the
  // stream mid-file, and nothing else will report that transfer) or a transfer already failing.
  if (_upFile || _upReason != nullptr) {
    FUNCTIONLOG("SDFileManager", "Upload request finished (%s, %lu bytes, %u rescued, %u recovered, handle %s)",
                _upReason ? _upReason : "ok", (unsigned long)_upBytes, (unsigned)_upRetries,
                (unsigned)_upPartialRescues, _upFile ? "open" : "closed");
  }
  // The multipart stream ended without its final chunk - the browser abandoned it or the connection died - so
  // nothing is complete and "uploaded" must never be printed for a truncated file. A reason already set is kept.
  if (_upFile) {
    _upFile.close();
    if (_upReason == nullptr) { _upReason = "cut_off"; _upCode = 409; }
  }
  // Before the mode test and the index invalidation: a skip changed nothing, so there is no write to protect and
  // the playlist and index stay as they were.
  if (_upReason == nullptr && _upSkipped) {
    _upSkipped = false;
    _upPath = "";
    FUNCTIONLOG("SDFileManager", "Upload skipped, name already present");
    sendJson(request, 200, F("{\"ok\":true,\"skipped\":true}"));
    return;
  }
  if (_upReason != nullptr) {
    const char *reason = _upReason;
    int code = _upCode;
    _upReason = nullptr;
    // A failure the CARD caused KEEPS the file when a position has been verified, so the browser can send only the
    // remainder - that is what the landed field is for, and it is where minutes are saved rather than just bytes.
    // Anything else (no space, a protected name, a refusal this page caused) removes the file, because a truncated
    // track under a listed name is worse than no track at all.
    const bool cardFailure = (strcmp(reason, "write_failed") == 0) ||
                             (strcmp(reason, "card_lagging")  == 0) ||
                             (strcmp(reason, "stalled")       == 0);
    uint64_t landed = 0;
    if (cardFailure && _upVerified && _upPath.length()) {
      _upResumePath = _upPath;
      _upResumeAt   = resumeFloor(_upVerified);   // on an assembly-unit boundary, so the retry stays aligned
      landed        = _upResumeAt;
    } else if (_upResumeAt && _upResumePath.length()) {
      // The stall branch in loop() already released THIS transfer and recorded the position a retry may use - the page
      // aborts the request the moment it hears the transfer is lost, and the request can outlive the release. Nothing
      // to remove: the file is still on the card, and forgetting the record here would refuse the very retry it was
      // kept for.
      landed = _upResumeAt;
    } else {
      if (_upPath.length()) sdman.remove(_upPath);
      _upResumeAt = 0;
      _upResumePath = "";
      _upVerified = 0;
    }
    const uint32_t failedMs = _upStartedMs ? (millis() - _upStartedMs) : 0;
    const uint32_t written  = (uint32_t)_upBytes;
    // Copied out with the byte count, before either is cleared below: the line reports BYTES MOVED, and reading
    // _upResumedFrom after it was zeroed made every resumed failure print the file POSITION instead.
    const uint32_t resumedFrom = (uint32_t)_upResumedFrom;
    _upPath = "";
    _upBytes = 0;
    _upResumedFrom = 0;
    // The time is reported for a failure too, because a comparison run has to count the cost of the failures, not only
    // the files that went through. ERROR is kept for a LOSS: nothing was vouched for and the partial is gone, the only
    // case the user has to act on. A card-side failure retries itself from `landed`, so it is an ordinary line.
    if (landed) {
      FUNCTIONLOG("SDFileManager", "Upload did not finish (%s) after %lu.%02lus - %llu bytes moved, %llu is on the card; continuing from there",
                  reason, (unsigned long)(failedMs / 1000), (unsigned long)((failedMs % 1000) / 10),
                  (unsigned long long)(written - resumedFrom), (unsigned long long)landed);
    } else {
      ERRORLOG("SDFileManager: upload refused (%s) after %lu.%02lus, %llu bytes moved, nothing vouchable - the partial file is gone",
               reason, (unsigned long)(failedMs / 1000), (unsigned long)((failedMs % 1000) / 10),
               (unsigned long long)(written - resumedFrom));
    }
    // Guarded: an attempt first released by the stall branch is folded by abortUpload() and must not be counted twice.
    fileFoldAttempt();
    asmLogAttempt(_filePath.c_str());   // what the writes cost, while the path is still known
    sendError(request, code, reason, landed);
    return;
  }
  // Still answered inside the mode: "ok" from a mode that has since closed would leave the page thinking it
  // can carry on reading the card.
  if (!requireActive(request)) return;
  // _upBytes is what the WRITE CALLS accepted, not what reached the card: on FATFS a write goes into a stream buffer
  // and the flush behind it can fail later. So the file is re-opened by name and only the measured size is reported.
  {
    File landed = _upPath.length() ? sdman.open(_upPath) : File();
    const uint64_t onCard = landed ? (uint64_t)landed.size() : 0;
    if (landed) landed.close();
    if (onCard != _upBytes) {
      // The same loss as card_lagging by a different route, so the same recovery: the file is kept when a position
      // has been verified, and a retry resumes there instead of sending the whole file again.
      const uint64_t keep = resumeFloor(_upVerified);
      if (keep) {
        FUNCTIONLOG("SDFileManager", "%s landed %llu of %llu bytes - short on the card, so the remainder will be sent",
                    _upPath.c_str(), (unsigned long long)onCard, (unsigned long long)_upBytes);
      } else {
        ERRORLOG("SDFileManager: %s landed %llu of %llu bytes: short on the card and nothing vouchable - removing it",
                 _upPath.c_str(), (unsigned long long)onCard, (unsigned long long)_upBytes);
      }
      if (keep) {
        _upResumePath = _upPath;
        _upResumeAt   = keep;
      } else {
        sdman.remove(_upPath);
        _upResumePath = "";
        _upResumeAt   = 0;
      }
      _upPath = "";
      _upBytes = 0;
      _upVerified = 0;
      _upResumedFrom = 0;
      sendError(request, 500, "write_failed", keep);
      return;
    }
  }
  // How long the file took and how fast that is, for comparison - SPI against SDMMC, a clock good at reading and bad
  // at writing. Bytes MOVED, not bytes in the file: a resumed transfer starts at the offset, and dividing the whole
  // file size by the tail's time flattered the average into 1086 KB/s. Seconds rather than milliseconds, for reading.
  // One line per FILE, with the file's figures: the clock runs from the first attempt's start and the counters are
  // the sum of every attempt, so a retried file reports what it really cost.
  fileFoldAttempt();
  const uint32_t fileMs = _fileStartedMs ? (millis() - _fileStartedMs) : 0;
  const uint32_t kbPerSec = (fileMs && _fileMoved) ? (uint32_t)((_fileMoved / 1024ULL) * 1000ULL / fileMs) : 0;
  FUNCTIONLOG("SDFileManager", "Finished Upload %s (%llu bytes moved in %lu.%02lus, %lu KB/s, %u attempt(s), %u refused chunk(s) rescued, %u partial(s) recovered)",
              _upPath.c_str(), (unsigned long long)_fileMoved, (unsigned long)(fileMs / 1000),
              (unsigned long)((fileMs % 1000) / 10), (unsigned long)kbPerSec, (unsigned)_fileAttempts,
              (unsigned)_fileRetries, (unsigned)_filePartialRescues);
  asmLogAttempt(_upPath.c_str());   // and what its writes cost, which the throughput above cannot separate out
  // Decided while the path is known: an audio file joins the playable set, anything else is clutter.
  const bool upWasAudio = sdman.isAudioName(basenameOf(_upPath).c_str());
  _filePath = "";   // the file is done: the next different path starts a fresh count
  // A whole file leaves nothing to resume from, so the record goes with it.
  _upResumeAt = 0;
  _upResumePath = "";
  _upVerified = 0;
  _upResumedFrom = 0;
  _upPath = "";
  dropSdDerivedFiles(upWasAudio);
  sendOk(request);
}

// ==== Mode ====

bool FileManager::uploadOpen() const {
  return _upFile ? true : false;
}

uint32_t FileManager::uploadIdleMs() const {
  if (!_upFile) return 0;
  // One read of the clock and one of the stamp, then a clamp: the stamp is written by the AsyncTCP task, and a chunk
  // landing between the two reads made the subtraction wrap and released a transfer that was still sending.
  const uint32_t now  = millis();
  const uint32_t last = _upLastChunkMs;
  return (now >= last) ? (now - last) : 0;
}

void FileManager::touch() {
  _lastActivity = millis();
  // The manager stops the player, so any API call also resets the screensaver countdown the display watches -
  // the mode is the user being here. Only SDMAN can take the screen while the mode is open, which is the other half.
  config.screensaverTicks = 0;
  config.screensaverPlayingTicks = 0;
}

void FileManager::markCardChanged() {
  _cardChanged = true;
}

uint32_t FileManager::idleRemainingMs() const {
  if (!_active) return 0;
  // One clock, and it is the USER's: the upload deadline is enforced by the stall branch in loop(), and what is worth
  // showing is decided where the showing happens. Keeping every chunk's touch() makes an upload hold this clock open,
  // so the display draws no countdown during an upload or a delete.
  uint32_t elapsed = millis() - _lastActivity;
  if (elapsed >= SDMAN_AUTO_EXIT_MS) return 0;
  return SDMAN_AUTO_EXIT_MS - elapsed;
}

void FileManager::enter() {
  // The stop and the capture belong to the transition INTO the mode, because the page calls this route on load and
  // again after it replaces itself with "/". _wasPlaying is remembered because the mode is not a play button, and the
  // stop is what lets the manager assume nothing is reading the card.
  if (!_active) {
    _wasPlaying = player.isRunning();
    // Stop the player synchronously, before anything unmounts the card: a remount discards every handle, and a
    // queued PR_STOP only ASKS the player task to stop - so the audio's handle outlived the filesystem and closing it
    // corrupted the heap. stopSync() is the order changeMode() uses; _wasPlaying must be read before it.
    player.stopSync();
  }
  // Mount on demand, after the stop so it cannot fight the player for the volume. THIS IS THE ONLY SPEED CHANGE A
  // SESSION HAS: ensureSpeed() moves to SDSPISPEED_MANAGER and remounts if it must, and leave() puts it back.
  if (!sdman.ready) sdman.start(SDSPISPEED_MANAGER);
  else sdman.ensureSpeed(SDSPISPEED_MANAGER);
  _lastActivity = millis();
  if (_active) return;
  _active = true;
  display.putRequest(NEWMODE, SDMAN);
  #if defined(SD_USE_MMC)
    const char* transport = "MMC";
  #else
    const char* transport = "SPI";
  #endif
  // Transport, card type and capacity in one line, so a card report needs no second round trip. The allocation unit
  // earns its place: nothing ever printed it, so a mis-formatted card was invisible in every log collected - the 8 KB
  // default failed on everything but an empty card, while the same card at 512-byte units took everything.
  char au[24];
  const uint32_t auBytes = sdman.allocationUnit();
  if (auBytes == 0)                                   snprintf(au, sizeof(au), "unknown");
  else if (auBytes >= 1024 && (auBytes % 1024) == 0)  snprintf(au, sizeof(au), "%luKB", (unsigned long)(auBytes / 1024));
  else                                                snprintf(au, sizeof(au), "%lu bytes", (unsigned long)auBytes);
  FUNCTIONLOG("SDFileManager", "Open (%s transport, SD %s, type %d, %lu MB, Allocation unit size: %s)", transport,
              sdman.ready ? "mounted" : "NOT mounted", (int)sdman.cardType(),
              (unsigned long)(sdman.cardSize() / (1024ULL * 1024ULL)), au);
}

void FileManager::leave(bool resumeAudio, const char *why) {
  if (!_active) return;
  // An upload still open means the browser was cut off mid-file and hUploadDone() may never run. abortUpload() closes
  // it, takes the partial away, and leaves the reason set so a late hUploadDone() reports it instead of a success.
  abortUpload("interrupted", 409, false);
  // Nothing may resume into a mode that has ended, and a partial kept for a resume goes with it.
  discardResume();
  _active = false;
  _busy = false;   // backstop: a handler that died mid-operation must not be able to lock the mode closed
  // Hand the card back at the PLAYER's clock: a session may have stepped down and the player's reads want the high
  // speed. This is the second boundary a remount is allowed at; skipped when the card has already left the slot.
  // Any handle the manager still holds is closed first, as in enter(): a listing keeps its directory open across the
  // walk, and the remount below discards the filesystem under it.
  if (_listDir) _listDir.close();
  // The assembly buffer goes with the session, but not while a write is executing: a chunk in flight in the AsyncTCP
  // task holds a pointer into it. A buffer that survives this is reused by the next session.
  if (!_upInWrite) asmRelease();
  if (resumeAudio && sdman.ready) sdman.ensureSpeed(SDSPISPEED);
  // The card was just written to and possibly remounted, so it is busy: hold the shared presence probe off for a
  // moment, or the first raw read can fail and drop the mode to web.
  if (sdman.ready) sdman.grantPresenceGrace();
  display.putRequest(NEWMODE, PLAYER);
  // The manager ALWAYS hands back to SD mode: this mode exists to edit the CARD, and the old resume target read from
  // config.getMode() put the radio back on a web stream that "leaving the file manager" cannot mean. Forced only when
  // the card is mounted - with no card there is nothing to be in SD mode for.
  if (sdman.ready) {
    config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_SDCARD));
    config.syncSDFS();
  }
  // SmartStart hands the audio back by station index, which stops meaning anything once the card has changed.
  const bool smartStart = resumeAudio && _wasPlaying && config.store.smartstart;
  const bool cardChanged = smartStart && _cardChanged;
  if (smartStart && !cardChanged && sdman.ready) {
    FUNCTIONLOG("SDFileManager", "Resuming card playback (offset %lu)", (unsigned long)config.sdResumePos);
    player.sendCommand({PR_PLAY, config.lastStation()});
  } else {
    // One line for the whole close, naming the reason; the numbers say why the player was not handed back.
    FUNCTIONLOG("SDFileManager", "Closed the SD File Manager (%s) without resuming: was playing %d, smartstart %d, resume allowed %d, card changed %d",
                why, (int)_wasPlaying, (int)config.store.smartstart, (int)resumeAudio, (int)cardChanged);
  }
}

void FileManager::loop() {
  // A session's mutations owe ONE re-index, paid here after the mode has closed - never while it is open and never
  // from a handler, so no handler blocks AsyncTCP on the walk. The card must still be there, or the debt is dropped.
  if (!_active && _cardChanged && (int32_t)(millis() - _reindexNotBeforeMs) >= 0) {
    _cardChanged = false;
    if (sdman.ready && config.getMode() == PM_SDCARD) {
      // Before the first flash write, so the page can blank and spin for the whole walk; a flag read back by the case
      // in netserver, so a page connecting mid-walk is told the same thing.
      _rebuilding = true;
      netserver.requestOnChange(PLAYLISTREADY, 0);
      FUNCTIONLOG("SDFileManager", "Re-indexing the card after a change");
      display.putRequest(NEWMODE, SDCHANGE);   // same handshake as changeMode(): show the screen, then wait for it
      const unsigned long waitStart = millis();
      while (display.mode() != SDCHANGE && millis() - waitStart < 2000) delay(10);
      config.initSDPlaylist(true);             // forced: the index file is exactly what the mutations dropped
      display.putRequest(NEWMODE, PLAYER);
      // After the mode switch, because a screen we own drops both requests; the re-index above may just have reset
      // station.name/title to the "nothing to play" state.
      display.putRequest(NEWSTATION);
      display.putRequest(NEWTITLE);
      // The list is ready as far as this pass can make it; sent even when the build failed, because that page must
      // never wait for a build to succeed.
      netserver.requestOnChange(PLAYLISTREADY, 0);   // and the flag is already false again: this one says "ready"
      _rebuilding = false;
      // ...but a failed build is owed another pass: it left no index at all, which is the state initSDPlaylist()
      // repairs at mode entry. The flag was consumed at the top of this block, so it is set again here, with a delay
      // and a cap so a card that cannot be written does not turn loop() into an endless walk.
      if (!sdman.exists(INDEX_SD_PATH)) {
        if (_reindexTries < SD_REINDEX_MAX_RETRIES) {
          _reindexTries++;
          _cardChanged = true;
          _reindexNotBeforeMs = millis() + SD_REINDEX_RETRY_MS;
          FUNCTIONLOG("SDFileManager", "No index was written: retry %u of %u in %lums",
                      (unsigned)_reindexTries, (unsigned)SD_REINDEX_MAX_RETRIES, (unsigned long)SD_REINDEX_RETRY_MS);
        } else {
          FUNCTIONLOG("SDFileManager", "No index after %u attempts: leaving it empty until the next mode entry",
                      (unsigned)_reindexTries);
        }
      } else {
        _reindexTries = 0;
      }
    }
  }

  if (!_active) return;

  // A card leaving the slot ends the mode. The probe is shared with the player and debounced there (a busy card times
  // out one raw read), and it is skipped while a transfer or a delete batch is in flight.
  if (sdman.ready && !uploadOpen() && !_busy && !sdman.cardPresentStable()) {
    FUNCTIONLOG("SDFileManager", "Card gone for %u checks, closing", (unsigned)sdman.cardGoneStrikes());
    leave(false, "card removed");   // nothing to give back - the media it would play from is what vanished
    return;
  }

  // At most once a second: the main loop turns over far faster than the line changes.
  if (millis() - _lastCountdownMs >= 1000) {
    _lastCountdownMs = millis();
    display.sdmanCountdown();
  }

  // An upload that has gone quiet is released here - the one close allowed over an open upload, and bounded, unlike
  // the 180 s user clock that used to end a live transfer mid-file. Never while a write is executing: a handle closed
  // under another task is the cross-task hazard the log ring was fixed for, so a wedged write goes to the watchdog.
  // The reason is set before the close so a late hUploadDone() reports "stalled", not a success.
  // The quiet time is read once, before the test, and the value tested is the value logged: reading it again in the
  // arguments let a chunk arriving between the two make a 30 s silence report "quiet for 0ms".
  const uint32_t quietMs = uploadIdleMs();
  const uint32_t quietBudget = (_upReason ? SDMAN_UPLOAD_STALL_FAILED_MS : SDMAN_UPLOAD_STALL_MS);
  if (_active && uploadOpen() && quietMs >= quietBudget) {
    if (_upInWrite) {
      if (_upReason == nullptr) {
        _upReason = "stalled";
        _upCode = 409;
        FUNCTIONLOG("SDFileManager", "No chunks for %lums (%lu bytes in), but a write is in flight - leaving it to the watchdog",
                    (unsigned long)quietMs, (unsigned long)_upBytes);
      }
    } else {
      FUNCTIONLOG("SDFileManager", "No chunks for %lums (%lu bytes in) - releasing the transfer so the page can resume it",
                  (unsigned long)quietMs, (unsigned long)_upBytes);
      abortUpload("stalled", 409, true);   // the card may be perfectly well; only the browser went quiet
    }
  }

  // The user clock. An upload holds the mode open and leave() names the reason, so this branch reports nothing of its
  // own; it used to print the elapsed figures here, which is how "idle for 180000ms ... elapsed 1" came to exist.
  if (_active && !uploadOpen() && idleRemainingMs() == 0) {
    leave(true, "idle timeout");
  }
}

// ==== Routes ====

void FileManager::registerRoutes(AsyncWebServer &server) {
  server.on("/sdman/enter", HTTP_GET, hEnterApi);
  server.on("/sdman/done", HTTP_POST, hDone);
  server.on("/sdman/info", HTTP_GET, hInfo);
  server.on("/sdman/list", HTTP_GET, hList);
  server.on("/sdman/mkdir", HTTP_POST, hMkdir);
  server.on("/sdman/rename", HTTP_POST, hRename);
  server.on("/sdman/move", HTTP_POST, hMove);
  server.on("/sdman/delete", HTTP_POST, hDelete, nullptr, onDeleteBody);
  server.on("/sdman/download", HTTP_GET, hDownload);
  // Its own upload handler takes precedence over the global one, keeping /sdman multipart data out of /webboard.
  server.on("/sdman/upload", HTTP_POST, hUploadDone, onUploadChunk);
}

#endif  // USE_SD
