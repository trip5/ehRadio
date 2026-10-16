#include "layoutregistry.h"
#include "../core/options.h"

// A build with no display has no conf file, so there is no _layouts[] table to serve and nothing to
// switch between: display.h answers with its own stubs and never reaches this file.
#ifndef DUMMYDISPLAY

#include <string.h>
#include <stdio.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "dspcore.h"

// The compiled table's own length.  A file-local constant rather than a macro because the table only
// exists where the conf file was included, and layoutregistry.h has to stay includable without it.
static const uint8_t _builtinCount = (uint8_t)(sizeof(_layoutNames) / sizeof(_layoutNames[0]));

static const char* const _slotFiles[LAYOUT_SLOT_COUNT] = { LAYOUT_SLOT_FILES };

static LayoutData _userLayouts[LAYOUT_SLOT_COUNT];
static char      _userNames[LAYOUT_SLOT_COUNT][33];

// The preview scratch layout: see layoutStaging() in the header for why it is not a slot.
static LayoutData _staging;

static bool     _slotsLoaded = false;
static uint16_t _generation  = 1;

// A document is two to three kB.  Anything larger is a corrupt or hostile file, and refusing it here is
// cheaper than letting it ask for the allocation.
#define LAYOUT_FILE_MAX 6144

static void slotPath(uint8_t slot, char* out, size_t room) {
  snprintf(out, room, "/data/%s", _slotFiles[slot]);
}

// A slot with values but no name is still a layout, so a nameless file gets a default rather than being
// treated as an empty slot.
static void setSlotName(uint8_t slot, const char* name) {
  if (name == nullptr) name = "";
  while (*name == ' ') name++;
  strncpy(_userNames[slot], name, sizeof(_userNames[slot]) - 1);
  _userNames[slot][sizeof(_userNames[slot]) - 1] = '\0';
  if (_userNames[slot][0] == '\0')
    snprintf(_userNames[slot], sizeof(_userNames[slot]), "Custom Layout %u", (unsigned)(slot + 1));
}

// A slot is filled when it has a name.  The name and the values arrive together from the file loader,
// which supplies a default name for a file that carries none, so this one test cannot disagree with
// "a file was read successfully".
static bool slotFilled(uint8_t slot) {
  return (slot < LAYOUT_SLOT_COUNT) && (_userNames[slot][0] != '\0');
}

// ---------------------------------------------------------------------------
// The document
// ---------------------------------------------------------------------------
// A cell is written by INDEX into the cell list conf_index.h gives the field's type, so the shape of a
// record lives in exactly one place.  The clamps are the storage's own widths: a uint8_t cell cannot
// hold 300, and a MoveConfig width is signed.

static uint16_t clampU16(int32_t v)   { return (uint16_t)(v < 0 ? 0 : (v > 65535 ? 65535 : v)); }
static int16_t  clampI16(int32_t v)   { return (int16_t)(v < -32768 ? -32768 : (v > 32767 ? 32767 : v)); }
static uint8_t  clampU8 (int32_t v)   { return (uint8_t)(v < 0 ? 0 : (v > 255 ? 255 : v)); }
static uint8_t  clampAlign(int32_t v) { return (uint8_t)(v < 0 ? 0 : (v > 2 ? 2 : v)); }

static void setCell(uint8_t* rec, uint8_t type, uint8_t cell, int32_t v) {
  switch (type) {
    case CT_WIDGET: {
      WidgetConfig* c = (WidgetConfig*)rec;
      switch (cell) {
        case 0: c->offset = clampU16(v); break;
        case 1: c->top = clampU16(v); break;
        case 2: c->textsize = clampU16(v); break;
        case 3: c->align = (WidgetAlign)clampAlign(v); break;
      }
      break;
    }
    case CT_SCROLL: {
      ScrollConfig* c = (ScrollConfig*)rec;
      switch (cell) {
        case 0: c->widget.offset = clampU16(v); break;
        case 1: c->widget.top = clampU16(v); break;
        case 2: c->widget.textsize = clampU16(v); break;
        case 3: c->widget.align = (WidgetAlign)clampAlign(v); break;
        case 4: c->width = clampU16(v); break;
        case 5: c->startscrolldelay = clampU16(v); break;
        case 6: c->scrolldelta = clampU8(v); break;
        case 7: c->scrolltime = clampU16(v); break;
      }
      break;
    }
    case CT_FILL: {
      FillConfig* c = (FillConfig*)rec;
      switch (cell) {
        case 0: c->widget.offset = clampU16(v); break;
        case 1: c->widget.top = clampU16(v); break;
        case 2: c->widget.textsize = clampU16(v); break;
        case 3: c->widget.align = (WidgetAlign)clampAlign(v); break;
        case 4: c->width = clampU16(v); break;
        case 5: c->height = clampU16(v); break;
        case 6: c->outlined = (v != 0); break;
      }
      break;
    }
    case CT_BITRATE: {
      BitrateConfig* c = (BitrateConfig*)rec;
      switch (cell) {
        case 0: c->widget.offset = clampU16(v); break;
        case 1: c->widget.top = clampU16(v); break;
        case 2: c->widget.textsize = clampU16(v); break;
        case 3: c->widget.align = (WidgetAlign)clampAlign(v); break;
        case 4: c->dimension = clampU16(v); break;
      }
      break;
    }
    case CT_BANDS: {
      VUBandsConfig* c = (VUBandsConfig*)rec;
      switch (cell) {
        case 0: c->width = clampU16(v); break;
        case 1: c->height = clampU16(v); break;
        case 2: c->space = clampU8(v); break;
        case 3: c->vspace = clampU8(v); break;
        case 4: c->perheight = clampU8(v); break;
      }
      break;
    }
    case CT_MOVE: {
      MoveConfig* c = (MoveConfig*)rec;
      switch (cell) {
        case 0: c->x = clampU16(v); break;
        case 1: c->y = clampU16(v); break;
        case 2: c->width = clampI16(v); break;
      }
      break;
    }
    default: break;
  }
}

bool layoutParseDoc(const char* json, LayoutData* out, char* nameOut, size_t nameRoom, bool* screenOk) {
  if (json == nullptr || out == nullptr) return false;

  JsonDocument doc;
  if (deserializeJson(doc, json) != DeserializationError::Ok) return false;

  // Missing keys are zeroed: what a document does not say is a widget that is off, which is the state the
  // conf files spell { }.
  memset(out, 0, sizeof(LayoutData));

  // Normally the records live under "layout", but a hand-written file may put them at the top level, so
  // both are accepted and the wrapper is simply preferred when it is there.
  JsonObject rec = doc["layout"].is<JsonObject>() ? doc["layout"].as<JsonObject>() : doc.as<JsonObject>();

  for (uint8_t f = 0; f < CONF_FIELD_COUNT; f++) {
    const ConfField& fd = _confFields[f];
    JsonVariant v = rec[fd.key];
    if (v.isNull()) continue;
    uint8_t* base = (uint8_t*)out + fd.offset;
    if (fd.type == CT_BOOL) { *(bool*)base = (v.as<int32_t>() != 0); continue; }
    if (!v.is<JsonObject>()) continue;
    JsonObject o = v.as<JsonObject>();
    for (uint8_t cell = 0; cell < _confCellCounts[fd.type]; cell++) {
      JsonVariant cv = o[_confCells[fd.type][cell].key];
      if (cv.isNull()) continue;
      setCell(base, fd.type, cell, cv.as<int32_t>());
    }
  }

  if (nameOut != nullptr && nameRoom > 0) {
    const char* nm = doc["name"];
    if (nm != nullptr) {
      strncpy(nameOut, nm, nameRoom - 1);
      nameOut[nameRoom - 1] = '\0';
    }
  }

  if (screenOk != nullptr) {
    // Absent is accepted, a different panel size is not: the records are pixel positions for one screen, and
    // applying them elsewhere draws a layout nobody asked for.
    const char* s = doc["screen"];
    *screenOk = true;
    if (s != nullptr) {
      char want[16];
      snprintf(want, sizeof(want), "%ux%u", (unsigned)DSP_WIDTH, (unsigned)DSP_HEIGHT);
      *screenOk = (strcmp(s, want) == 0);
    }
  }
  // A document can carry a text size of 0 - the editor's font-size box used to accept one on any field, and a
  // hand-written file still can.  Corrected HERE as well as on the way into activeLayout, with the SAME helper,
  // so the stored slot and the copy the editor reads back agree with what the device will draw.  Everything
  // that parses a document - a slot file, the editor's save, its live preview - passes through this one exit.
  layoutSanitize(out);
  return true;
}

// ---------------------------------------------------------------------------
// The document, the other way
// ---------------------------------------------------------------------------

static int32_t getCell(const uint8_t* rec, uint8_t type, uint8_t cell) {
  switch (type) {
    case CT_WIDGET: {
      const WidgetConfig* c = (const WidgetConfig*)rec;
      switch (cell) {
        case 0: return c->offset;
        case 1: return c->top;
        case 2: return c->textsize;
        case 3: return (int32_t)c->align;
      }
      break;
    }
    case CT_SCROLL: {
      const ScrollConfig* c = (const ScrollConfig*)rec;
      switch (cell) {
        case 0: return c->widget.offset;
        case 1: return c->widget.top;
        case 2: return c->widget.textsize;
        case 3: return (int32_t)c->widget.align;
        case 4: return c->width;
        case 5: return c->startscrolldelay;
        case 6: return c->scrolldelta;
        case 7: return c->scrolltime;
      }
      break;
    }
    case CT_FILL: {
      const FillConfig* c = (const FillConfig*)rec;
      switch (cell) {
        case 0: return c->widget.offset;
        case 1: return c->widget.top;
        case 2: return c->widget.textsize;
        case 3: return (int32_t)c->widget.align;
        case 4: return c->width;
        case 5: return c->height;
        case 6: return c->outlined ? 1 : 0;
      }
      break;
    }
    case CT_BITRATE: {
      const BitrateConfig* c = (const BitrateConfig*)rec;
      switch (cell) {
        case 0: return c->widget.offset;
        case 1: return c->widget.top;
        case 2: return c->widget.textsize;
        case 3: return (int32_t)c->widget.align;
        case 4: return c->dimension;
      }
      break;
    }
    case CT_BANDS: {
      const VUBandsConfig* c = (const VUBandsConfig*)rec;
      switch (cell) {
        case 0: return c->width;
        case 1: return c->height;
        case 2: return c->space;
        case 3: return c->vspace;
        case 4: return c->perheight;
      }
      break;
    }
    case CT_MOVE: {
      const MoveConfig* c = (const MoveConfig*)rec;
      switch (cell) {
        case 0: return c->x;
        case 1: return c->y;
        case 2: return c->width;
      }
      break;
    }
    default: break;
  }
  return 0;
}

void layoutDocFor(const LayoutData* data, const char* name, String& out) {
  const uint8_t* base = (const uint8_t*)data;
  out = "{\"name\":\"";
  for (const char* p = name; p != nullptr && *p != '\0'; p++) {
    if (*p == '"' || *p == '\\') out += '\\';
    if ((uint8_t)*p >= 0x20) out += *p;   // control characters have no place in the name
  }
  out += "\",\"screen\":\"";
  char scr[16];
  snprintf(scr, sizeof(scr), "%ux%u", (unsigned)DSP_WIDTH, (unsigned)DSP_HEIGHT);
  out += scr;
  out += "\",\"layout\":{";
  for (uint8_t f = 0; f < CONF_FIELD_COUNT; f++) {
    const ConfField& fd = _confFields[f];
    if (f > 0) out += ',';
    out += '"';
    out += fd.key;
    out += "\":";
    const uint8_t* rec = base + fd.offset;
    if (fd.type == CT_BOOL) {
      out += (*(const bool*)rec) ? '1' : '0';
      continue;
    }
    out += '{';
    for (uint8_t cell = 0; cell < _confCellCounts[fd.type]; cell++) {
      if (cell > 0) out += ',';
      out += '"';
      out += _confCells[fd.type][cell].key;
      out += "\":";
      out += getCell(rec, fd.type, cell);
    }
    out += '}';
  }
  out += "}}";
}

// ---------------------------------------------------------------------------
// The slot files
// ---------------------------------------------------------------------------

void layoutLoadSlots() {
  for (uint8_t s = 0; s < LAYOUT_SLOT_COUNT; s++) {
    _userNames[s][0] = '\0';
    memset(&_userLayouts[s], 0, sizeof(LayoutData));

    char path[40];
    slotPath(s, path, sizeof(path));
    File f = LittleFS.open(path, "r");
    if (!f) continue;
    const size_t size = f.size();
    if (size == 0 || size > LAYOUT_FILE_MAX) { f.close(); continue; }
    char* buf = (char*)malloc(size + 1);
    if (buf == nullptr) { f.close(); continue; }
    const size_t got = f.readBytes(buf, size);
    f.close();
    buf[got] = '\0';

    LayoutData parsed;
    bool screenOk = false;
    if (layoutParseDoc(buf, &parsed, _userNames[s], sizeof(_userNames[s]), &screenOk) && screenOk) {
      memcpy(&_userLayouts[s], &parsed, sizeof(LayoutData));
      if (_userNames[s][0] == '\0') setSlotName(s, nullptr);
    } else {
      _userNames[s][0] = '\0';   // unreadable, or made for another panel: the slot reads as empty
    }
    free(buf);
  }
  _slotsLoaded = true;
  _generation++;
}

uint8_t layoutCount() { return (uint8_t)(_builtinCount + LAYOUT_SLOT_COUNT); }

bool layoutFilled(uint8_t id) {
  if (id < _builtinCount) return true;
  return slotFilled((uint8_t)(id - _builtinCount));
}

const char* layoutName(uint8_t id) {
  if (id < _builtinCount) return _layoutNames[id];
  const uint8_t slot = (uint8_t)(id - _builtinCount);
  return (slot < LAYOUT_SLOT_COUNT) ? _userNames[slot] : "";
}

const LayoutData* layoutSource(uint8_t id) {
  if (id < _builtinCount) return &_layouts[id];
  const uint8_t slot = (uint8_t)(id - _builtinCount);
  return slotFilled(slot) ? &_userLayouts[slot] : nullptr;
}

bool layoutCopy(uint8_t id, void* dst) {
  const LayoutData* src = layoutSource(id);
  if (src == nullptr || dst == nullptr) return false;   // an empty id: the caller keeps what it had
  memcpy(dst, src, sizeof(LayoutData));
  return true;
}

void layoutSanitize(LayoutData* data) {
  if (data == nullptr) return;
  for (uint8_t f = 0; f < CONF_FIELD_COUNT; f++) {
    const ConfField& fd = _confFields[f];
    // 0 is the system font on these two and is wanted: the 128x32 OLED, the 284x76 and the 160x80 set both
    // clockConf and numConf to it, and a clamp here would turn those panels' clocks and numbers into 15 px
    // clock fonts.
    if (fd.flags & CONF_FONT_INDEX) continue;
    const uint8_t cells = _confCellCounts[fd.type];
    // Which cell is the font size, by NAME.  The cell lists own that order, so a second copy of "2" here would
    // be exactly the kind of thing that drifts the day a type gains a cell.
    uint8_t fsCell = cells;
    for (uint8_t c = 0; c < cells; c++) {
      if (strcmp(_confCells[fd.type][c].key, "fontsize") == 0) { fsCell = c; break; }
    }
    if (fsCell >= cells) continue;                    // this type has no font size cell at all
    // The struct carries the cell but the code never reads it: a fill's is a literal 0 and the VU's is a
    // presence marker.  Clamping one would not merely be pointless - presentOf() counts textsize as one of its
    // presence bits, so writing a 1 into an empty fill would turn "absent" into "present".
    if (fd.unused & CONF_CELL_BIT(fsCell)) continue;
    WidgetConfig* wc = nullptr;
    bool present = false;
    switch (fd.type) {
      case CT_WIDGET:  wc = (WidgetConfig *)((uint8_t *)data + fd.offset); present = presentOf(*wc); break;
      case CT_SCROLL:  {
        ScrollConfig* sc = (ScrollConfig *)((uint8_t *)data + fd.offset);
        wc = &sc->widget; present = presentOf(*sc); break;
      }
      case CT_BITRATE: {
        BitrateConfig* bc = (BitrateConfig *)((uint8_t *)data + fd.offset);
        wc = &bc->widget; present = presentOf(*bc); break;
      }
      default: continue;                              // no other type has a readable font size
    }
    // The field has to be PRESENT first: an all-zero conf is how an omitted widget is spelled, and this is read
    // before the clamp, so `present` is exactly "present, ignoring textsize" - which is the test this needs.
    if (wc != nullptr && present && wc->textsize == 0) wc->textsize = 1;
  }
}

bool vuResolveBox(LayoutData& data) {
  if (!presentOf(data.vuConf) || !presentOf(data.bandsConf)) return true;   // absent: nothing to resolve
  // The clamps VuWidget::init() used to make, kept so a hand-made conf cannot divide by a zero later.
  if (data.bandsConf.width == 0)  data.bandsConf.width  = 1;
  if (data.bandsConf.height == 0) data.bandsConf.height = 1;

  // The box as the painters resolve it: the bar family's two channels are `width * 2 + space` across one
  // side and `height` along the other, and WA_RIGHT swaps which is which.  32-bit sums so a wild conf
  // cannot wrap them into "fits".
  const bool rotate = (data.vuConf.align == WA_RIGHT);
  const uint16_t gap = data.bandsConf.space;
  uint32_t boxW = rotate ? data.bandsConf.height : (uint32_t)data.bandsConf.width * 2 + gap;
  uint32_t boxH = rotate ? (uint32_t)data.bandsConf.width * 2 + gap : data.bandsConf.height;

  const uint32_t budget = (uint32_t)VU_MAX_WIDTH * VU_MAX_HEIGHT;
  if (boxW * boxH > budget) {
    if (boxW > VU_MAX_WIDTH)  boxW = VU_MAX_WIDTH;
    if (boxH > VU_MAX_HEIGHT) boxH = VU_MAX_HEIGHT;
    // Write the clamped box back as the two channels plus the gap; the gap is kept when it fits, so the
    // clamp lands on the band width (the side the two channels share, doubled) and then on the band height.
    const uint16_t across = (uint16_t)(rotate ? boxH : boxW);
    const uint16_t along  = (uint16_t)(rotate ? boxW : boxH);
    uint16_t width = (across > gap) ? (uint16_t)((across - gap) / 2) : 1;
    if (width < 1) width = 1;
    data.bandsConf.width  = width;
    data.bandsConf.height = along;
    // Recomputed from what was actually written, so the edge test below is of the stored box.
    boxW = rotate ? along : (uint32_t)width * 2 + gap;
    boxH = rotate ? (uint32_t)width * 2 + gap : along;
  }

  // The panel edge, on the box as it now stands.  left/top are the conf's own, un-moved.
  const uint32_t left = data.vuConf.offset, top = data.vuConf.top;
  return (left + boxW <= DSP_WIDTH) && (top + boxH <= DSP_HEIGHT);
}

bool layoutRequiredOk(const LayoutData* data) {
  if (data == nullptr) return false;
  for (uint8_t f = 0; f < CONF_FIELD_COUNT; f++) {
    const ConfField& fd = _confFields[f];
    if ((fd.flags & CONF_REQUIRED) == 0) continue;
    const uint8_t* rec = (const uint8_t*)data + fd.offset;
    bool empty = false;
    // The very same rule the display draws by (presentOf() in widgetsconfig.h), not a second copy of it: a
    // layout a save accepts must not be one the device would render as nothing.  Only the scrolls carry the
    // flag today; the rest of the switch is what a field that gains it would use.
    switch (fd.type) {
      case CT_SCROLL:  empty = !presentOf(*(const ScrollConfig*)rec);  break;
      case CT_WIDGET:  empty = !presentOf(*(const WidgetConfig*)rec);  break;
      case CT_FILL:    empty = !presentOf(*(const FillConfig*)rec);    break;
      case CT_BITRATE: empty = !presentOf(*(const BitrateConfig*)rec); break;
      default: break;
    }
    if (empty) return false;
  }
  return true;
}

void layoutDocForAllSlots(String& out) {
  // A backup is the set of filled slots, in order, with no null for an empty one and nothing padded on the
  // end: a restore packs them into whatever slots are free, so declaring an emptiness would describe an
  // intent the reader does not honour.
  uint8_t count = 0;
  for (uint8_t s = 0; s < LAYOUT_SLOT_COUNT; s++)
    if (slotFilled(s)) count++;

  if (count == 0) { out = "[]"; return; }

  const bool wrapped = (count > 1);
  out = wrapped ? "[" : "";
  bool first = true;
  for (uint8_t s = 0; s < LAYOUT_SLOT_COUNT; s++) {
    if (!slotFilled(s)) continue;
    if (!first && wrapped) out += ',';
    first = false;
    // Built into its own String because layoutDocFor() REPLACES the buffer it is given.
    String one;
    layoutDocFor(&_userLayouts[s], _userNames[s], one);
    out += one;
  }
  if (wrapped) out += ']';
}

const char* layoutSlotFileName(uint8_t slot) {
  return (slot < LAYOUT_SLOT_COUNT) ? _slotFiles[slot] : "";
}

bool layoutSaveSlot(uint8_t slot, const char* name, const LayoutData* data) {
  if (slot >= LAYOUT_SLOT_COUNT || data == nullptr) return false;

  String json;
  layoutDocFor(data, name, json);

  char path[40];
  slotPath(slot, path, sizeof(path));
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const size_t wrote = f.print(json);
  f.close();
  if (wrote != json.length()) {
    // A short write leaves a file that cannot be parsed, and an unparseable slot reads as empty; removing it
    // is the honest outcome rather than leaving half a layout behind.
    LittleFS.remove(path);
    return false;
  }

  memcpy(&_userLayouts[slot], data, sizeof(LayoutData));
  setSlotName(slot, name);
  _generation++;
  return true;
}

bool layoutDeleteSlot(uint8_t slot) {
  if (slot >= LAYOUT_SLOT_COUNT) return false;
  char path[40];
  slotPath(slot, path, sizeof(path));
  LittleFS.remove(path);                        // a missing file still clears the slot
  memset(&_userLayouts[slot], 0, sizeof(LayoutData));
  _userNames[slot][0] = '\0';
  _generation++;
  return true;
}

LayoutData* layoutStaging() { return &_staging; }

void layoutStagingCopyTo(void* dst) {
  if (dst == nullptr) return;
  memcpy(dst, &_staging, sizeof(LayoutData));
}

uint8_t layoutValidId(uint8_t id) {
  // Before the slot files have been read there is no way to tell whether a custom id is real, and
  // config.loadLayout() runs before the filesystem is mounted.  Answering "0" then would forget the user's
  // choice for the whole session, so an unjudged id is left alone and the caller re-resolves it after
  // layoutLoadSlots().
  if (id >= _builtinCount && !_slotsLoaded) return id;
  return layoutFilled(id) ? id : 0;
}

uint16_t layoutGeneration() { return _generation; }

#endif // DUMMYDISPLAY
