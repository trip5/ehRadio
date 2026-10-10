#include "themeregistry.h"
#include <string.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

// Two parallel tables rather than one array of structs: the entries are pointers, so the built-ins
// can stay in flash and the user slots can live in RAM, and a single memcpy then serves both —
// PROGMEM is a no-op on ESP32 and memcpy_P is memcpy, so there is no second code path to keep in
// step.  The cost is one more translation-unit copy of _themes[] and _themeNames[] (about 2 KB of
// flash, since a namespace-scope const array has internal linkage); that was accepted over giving
// the tables a single definition, which would mean restructuring themes.h that importtheme.py
// generates into.
static const ThemeData* _slotData[THEME_TOTAL_MAX];
static const char*      _slotNames[THEME_TOTAL_MAX];

// The user slots.  These must be one shared copy — the loader and the readers are different
// translation units — so unlike the built-in tables they cannot live in the header.
static ThemeData _userData[THEME_CUSTOM_MAX];
static char      _userNames[THEME_CUSTOM_MAX][33];

static const char* const _slotFiles[THEME_CUSTOM_MAX] = { THEME_SLOT_FILES };

// A theme file is a few hundred bytes.  Anything larger is a corrupt or hostile file, and refusing it
// here is cheaper than letting it ask for the allocation.
#define THEME_FILE_MAX 4096

static bool _built = false;
static bool _slotsLoaded = false;
static uint16_t _generation = 1;

// The preview scratch theme: see themeStaging() in the header for why it is not part of the registry.
static ThemeData _staging;

static void buildRegistry() {
  for (uint8_t i = 0; i < THEME_BUILTIN_COUNT; i++) {
    _slotData[i]  = &_themes[i];
    _slotNames[i] = _themeNames[i];
  }
  for (uint8_t s = 0; s < THEME_CUSTOM_MAX; s++) {
    const uint8_t id = (uint8_t)(THEME_BUILTIN_COUNT + s);
    // A slot is filled when it has a name.  The name and the colours arrive together from the file
    // loader, which supplies a default name for a file that carries none, so this one test cannot
    // disagree with "a file was read successfully".
    _slotData[id]  = _userNames[s][0] ? &_userData[s] : nullptr;
    _slotNames[id] = _userNames[s];
  }
  _built = true;
}

// Built on first use rather than from an init call: config.loadTheme() runs long before anything
// that looks like a startup sequence, and a registry that silently needs initialising first is a
// trap.  It is a few dozen stores, once.
static inline const ThemeData* slotData(uint8_t id) {
  if (!_built) buildRegistry();
  return (id < THEME_TOTAL_MAX) ? _slotData[id] : nullptr;
}

static void slotPath(uint8_t slot, char* out, size_t room) {
  snprintf(out, room, "/data/%s", _slotFiles[slot]);
}

// A slot with colours but no name is still a theme, so a nameless file gets a default rather than
// being treated as an empty slot.
static void setSlotName(uint8_t slot, const char* name) {
  if (name == nullptr) name = "";
  while (*name == ' ') name++;
  strncpy(_userNames[slot], name, sizeof(_userNames[slot]) - 1);
  _userNames[slot][sizeof(_userNames[slot]) - 1] = '\0';
  if (_userNames[slot][0] == '\0') snprintf(_userNames[slot], sizeof(_userNames[slot]), "Custom %u", (unsigned)(slot + 1));
}

uint8_t themeCount() { return THEME_TOTAL_MAX; }

bool themeFilled(uint8_t id) { return slotData(id) != nullptr; }

const ThemeData* themeAt(uint8_t id) { return slotData(id); }

const char* themeName(uint8_t id) {
  if (!_built) buildRegistry();
  return (id < THEME_TOTAL_MAX) ? _slotNames[id] : "";
}

bool themeCopy(uint8_t id, void* dst) {
  const ThemeData* src = slotData(id);
  if (src == nullptr || dst == nullptr) return false;
  memcpy(dst, src, sizeof(ThemeData));
  return true;
}

uint8_t themeValidId(uint8_t id) {
  // Before the slot files have been read there is no way to tell whether a custom id is real, and
  // config.loadTheme() runs before the filesystem is even mounted.  Answering "0" then would forget
  // the user's choice for the whole session, so an unjudged id is left alone and the caller
  // re-resolves it after themeLoadSlots().
  if (id >= THEME_BUILTIN_COUNT && !_slotsLoaded) return id;
  return themeFilled(id) ? id : 0;
}

uint16_t themeGeneration() { return _generation; }

const char* themeSlotFileName(uint8_t slot) {
  return (slot < THEME_CUSTOM_MAX) ? _slotFiles[slot] : "";
}

ThemeData* themeStaging() { return &_staging; }

void themeStagingCopyTo(void* dst) {
  if (dst == nullptr) return;
  memcpy(dst, &_staging, sizeof(ThemeData));
}

// ---------------------------------------------------------------------------
// The element table
// ---------------------------------------------------------------------------

// The only way in or out of the table.  Past the end is "" rather than a trap, so either one can be
// called from a loop that got its bound from themeElementCount() without a guard at every use.
size_t themeElementCount() {
  return (size_t)THEME_ELEMENT_COUNT;
}

const char* themeElementKey(size_t index) {
  return (index < THEME_ELEMENT_COUNT) ? _themeElementNames[index].key : "";
}

const char* themeElementLabel(size_t index) {
  return (index < THEME_ELEMENT_COUNT) ? _themeElementNames[index].label : "";
}

// ---------------------------------------------------------------------------
// The document
// ---------------------------------------------------------------------------

// The element keys are the contract with the files, so a lookup is a comparison against that table
// and nothing else.  Reading the document by iterating its own members, rather than by looking up
// each key of ours, keeps any key text legal — including the dots and brackets the names carry.
static int elementIndex(const char* key) {
  for (size_t i = 0; i < THEME_ELEMENT_COUNT; i++)
    if (strcmp_P(key, _themeElementNames[i].key) == 0) return (int)i;
  return -1;
}

bool themeParseTheme(const char* json, ThemeData* out, char* nameOut, size_t nameRoom) {
  if (json == nullptr || out == nullptr) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

  // Missing keys are zeroed, not inherited: what the document does not say is black.
  memset(out, 0, sizeof(ThemeData));

  // Normally the keys live under "colors", but a hand-written file may put them at the top level, so
  // both are accepted and the wrapper is simply preferred when it is there.
  JsonObject colors = doc["colors"].is<JsonObject>() ? doc["colors"].as<JsonObject>() : doc.as<JsonObject>();

  uint16_t* field = (uint16_t*)out;
  for (JsonPair kv : colors) {
    const int idx = elementIndex(kv.key().c_str());
    if (idx < 0) continue;                       // a key we do not know: ignore, stay liberal
    JsonVariant v = kv.value();
    if (v.is<JsonArray>()) {
      JsonArray a = v.as<JsonArray>();
      if (a.size() < 3) continue;
      field[idx] = RGB(a[0].as<uint8_t>(), a[1].as<uint8_t>(), a[2].as<uint8_t>());
    } else if (v.is<uint32_t>()) {
      field[idx] = (uint16_t)(v.as<uint32_t>() & 0xFFFFu);
    }
  }

  if (nameOut != nullptr && nameRoom > 0) {
    const char* nm = doc["name"];
    if (nm != nullptr) {
      strncpy(nameOut, nm, nameRoom - 1);
      nameOut[nameRoom - 1] = '\0';
    }
  }
  return true;
}

// A name is free text from the editor, so it has to survive being written into JSON.
static void appendJsonString(String& out, const char* s) {
  for (; s != nullptr && *s != '\0'; s++) {
    const char c = *s;
    if (c == '"' || c == '\\') out += '\\';
    if ((uint8_t)c >= 0x20) out += c;   // control characters have no place in the name
  }
}

void themeJsonFor(const ThemeData* data, const char* name, String& out) {
  const uint16_t* field = (const uint16_t*)data;
  out = "{\"name\":\"";
  appendJsonString(out, name);
  out += "\",\"colors\":{";
  for (size_t i = 0; i < THEME_ELEMENT_COUNT; i++) {
    if (i > 0) out += ',';
    out += '"';
    out += themeElementKey(i);
    out += "\":";
    out += field[i];
  }
  out += "}}";
}

void themeJsonForAllSlots(String& out) {
  // A backup is the set of filled slots, in order, and nothing else.  No null for an empty slot and
  // nothing padded on the end: declaring an emptiness would be describing a layout the restore does not
  // honour anyway, since it packs these themes into whatever slots are free.
  uint8_t count = 0;
  for (uint8_t s = 0; s < THEME_CUSTOM_MAX; s++)
    if (themeFilled((uint8_t)(THEME_BUILTIN_COUNT + s))) count++;

  if (count == 0) { out = "[]"; return; }

  // One theme is that theme's own document, so the two export buttons agree on a one-theme device; more
  // than one is an array.  The wrapper is decided before anything is written, which keeps this to one pass.
  const bool wrapped = (count > 1);
  out = wrapped ? "[" : "";

  bool first = true;
  for (uint8_t s = 0; s < THEME_CUSTOM_MAX; s++) {
    const uint8_t id = (uint8_t)(THEME_BUILTIN_COUNT + s);
    if (!themeFilled(id)) continue;
    if (!first && wrapped) out += ',';
    first = false;
    // Built into its own String because themeJsonFor() REPLACES the buffer it is given.  Handing it this
    // one would overwrite the "[" with the first theme, and then every element would overwrite the last,
    // leaving nothing but the final theme and the closing bracket - which is exactly the invalid JSON this
    // used to emit, invisible until two slots were filled.
    String one;
    themeJsonFor(themeAt(id), themeName(id), one);
    out += one;
  }
  if (wrapped) out += ']';
}

// ---------------------------------------------------------------------------
// The slot files
// ---------------------------------------------------------------------------

void themeLoadSlots() {
  for (uint8_t s = 0; s < THEME_CUSTOM_MAX; s++) {
    _userNames[s][0] = '\0';
    memset(&_userData[s], 0, sizeof(ThemeData));

    char path[40];
    slotPath(s, path, sizeof(path));
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    const size_t size = f.size();
    if (size == 0 || size > THEME_FILE_MAX) { f.close(); continue; }
    char* buf = (char*)malloc(size + 1);
    if (buf == nullptr) { f.close(); continue; }
    const size_t got = f.readBytes(buf, size);
    f.close();
    buf[got] = '\0';

    ThemeData parsed;
    if (themeParseTheme(buf, &parsed, _userNames[s], sizeof(_userNames[s]))) {
      memcpy(&_userData[s], &parsed, sizeof(ThemeData));
      if (_userNames[s][0] == '\0') setSlotName(s, nullptr);
    } else {
      _userNames[s][0] = '\0';                 // unreadable file: the slot reads as empty
    }
    free(buf);
  }
  _slotsLoaded = true;
  buildRegistry();
  _generation++;
}

bool themeSaveSlot(uint8_t slot, const char* name, const ThemeData* data) {
  if (slot >= THEME_CUSTOM_MAX || data == nullptr) return false;

  String json;
  themeJsonFor(data, name, json);

  char path[40];
  slotPath(slot, path, sizeof(path));
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const size_t wrote = f.print(json);
  f.close();
  if (wrote != json.length()) {
    // A short write leaves a file that cannot be parsed, and an unparseable slot reads as empty;
    // removing it is the honest outcome rather than leaving a half theme behind.
    LittleFS.remove(path);
    return false;
  }

  memcpy(&_userData[slot], data, sizeof(ThemeData));
  setSlotName(slot, name);
  buildRegistry();
  _generation++;
  return true;
}

bool themeDeleteSlot(uint8_t slot) {
  if (slot >= THEME_CUSTOM_MAX) return false;
  char path[40];
  slotPath(slot, path, sizeof(path));
  LittleFS.remove(path);                       // a missing file still clears the slot
  memset(&_userData[slot], 0, sizeof(ThemeData));
  _userNames[slot][0] = '\0';
  buildRegistry();
  _generation++;
  return true;
}
