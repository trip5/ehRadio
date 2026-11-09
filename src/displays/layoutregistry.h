#ifndef layoutregistry_h
#define layoutregistry_h

// The layout registry: one flat list of ids serving the compiled _layouts[] table and the ten user
// slots whose files live in /data.  Same shape as themeregistry, and the same reason for it -
// config.store.layoutId is persisted, so the id set has to be stable and "is anything at this id"
// must be a separate question from "how many ids are there".  Readers ask for a COPY or a name,
// never for the table, which is why the rest of the firmware keeps reading activeLayout alone.

#include <stddef.h>
#include <stdint.h>
#include <Arduino.h>   // String, for the one document builder below
#include "widgets/widgetsconfig.h"

#define LAYOUT_SLOT_COUNT 10

// Bare names, because that is what Config::dataFiles[] holds: pruneLittleFS() compares a /data file's
// basename against that list, so a path there would never match and the file would be deleted.
#define LAYOUT_SLOT_FILES "layout1.json", "layout2.json", "layout3.json", "layout4.json", "layout5.json", \
                          "layout6.json", "layout7.json", "layout8.json", "layout9.json", "layout10.json"

// The fixed length of the list: the compiled table plus every slot, filled or not.
uint8_t layoutCount();

// Does a layout actually live at this id?  False for an empty slot and for anything past the end,
// which is the only test a caller needs before using an id.
bool layoutFilled(uint8_t id);

// The display name, or "" when the id holds nothing.  Always a valid C string.
const char* layoutName(uint8_t id);

// The values, or nullptr when the id holds nothing.  Never the caller's to modify.
const LayoutData* layoutSource(uint8_t id);

// sizeof(LayoutData) bytes into the caller's storage.  False, and nothing written, when the id is
// empty, so a caller can fall back rather than apply a zeroed layout.
bool layoutCopy(uint8_t id, void* dst);

// Correct a zero text size in place: 0 is not a size on a field that multiplies the system font's cell (it
// collapses the row, and on a scroll line it is the widget's own "init has not run" sentinel, which is the
// crash a font size of 0 caused), while it IS the system font on the two CONF_FONT_INDEX fields - the clock and
// the large number, which the 128x32 OLED, the 284x76 and the 160x80 ask for - and those are left alone.
// Called from the same place vuResolveBox() is, for the same reason: every copy into activeLayout passes
// through it, so a compiled conf, a LittleFS slot and the editor's push are all corrected, including a slot
// that already holds a 0.
void layoutSanitize(LayoutData* data);

// Resolve a layout's VU box ONCE, in place, and say whether the meter can be drawn at all.  The display
// calls this right after every copy into activeLayout, so a compiled conf, a LittleFS slot and the editor's
// own push are all resolved here and nowhere else - the widget then draws what it is handed, and nothing
// re-tests the numbers per frame.  Two rules, in this order:
//
//   budget  An over-budget box (by AREA, VU_MAX_WIDTH x VU_MAX_HEIGHT) is clamped to that rectangle by its
//           SIDES, and the clamped sides are written back into bandsConf.  That is the same rectangle
//           VuWidget::resolveScreensaverBox() already produces, so the player's meter and the screensaver's
//           stop being two different answers to one question.  vuConf.offset/top are NOT moved: the box
//           shrinks from its own corner.
//   edge    A box that then runs past the panel - left + boxW, top + boxH - cannot be drawn.  The verdict is
//           false, and the caller keeps the widget present but painting nothing, so the clock and the weather
//           still hide exactly as if the meter were there.
//
// Every shipped layout is inside both limits, so this leaves them all alone.
bool vuResolveBox(LayoutData& data);

// An id that is guaranteed to hold a layout: the argument when it is filled, 0 otherwise.
uint8_t layoutValidId(uint8_t id);

// The one document shape, used by the slot files and by the editor's Save and Apply bodies alike:
//   { "name": "My Layout", "screen": "320x240", "layout": { "metaConf": { "offset": 8 }, "fullClock": 1 } }
// Known keys, and a missing key is zeroed - what a document does not say is a widget that is off.  screenOk
// (may be nullptr) reports false when the document names a different panel size; a document with no screen
// field is accepted.  Returns false on malformed JSON.
bool layoutParseDoc(const char* json, LayoutData* out, char* nameOut, size_t nameRoom, bool* screenOk);

// Read every slot file.  Called once LittleFS is mounted - which is AFTER config.loadLayout() has already
// run, hence the two-phase rule in layoutValidId() - and the caller re-runs config.loadLayout() afterwards
// so a persisted custom id resolves on the same boot.
void layoutLoadSlots();

// False when a field marked CONF_REQUIRED is empty.  Dialogs write into the meta line and the playlist page
// is built on the playlist line, so an empty one is not an empty widget but a broken screen.
bool layoutRequiredOk(const LayoutData* data);

// The same document back out, built by hand rather than by a serializer so the keys are emitted exactly as
// conf_index.h spells them.  `name` may be nullptr.  REPLACES whatever `out` held; it does not append.
void layoutDocFor(const LayoutData* data, const char* name, String& out);

// The one scratch layout a live preview is parsed into.  The web handler fills it and queues
// LAYOUTPREVIEW; the display task copies it into the live layout with layoutStagingCopyTo().  A second POST
// simply overwrites it, which is harmless - it carries newer values for the same repaint.
LayoutData* layoutStaging();
void layoutStagingCopyTo(void* dst);

// Every filled slot as ONE document - the filled slots in slot order and nothing else, or, when exactly one
// is filled, that slot's own document.  REPLACES `out` in full.
void layoutDocForAllSlots(String& out);

// The slot's file name (a bare name, as in Config::dataFiles[]), or "" past the end.  The export download
// offers it when a layout's own name leaves nothing usable for a file name.
const char* layoutSlotFileName(uint8_t slot);

// Write the slot, or delete it.  Both install the result in RAM immediately, so the display task sees the
// change without a reboot; a save is the whole document and is refused by the caller, not here.
bool layoutSaveSlot(uint8_t slot, const char* name, const LayoutData* data);
bool layoutDeleteSlot(uint8_t slot);

// Bumped whenever the filled set changes, so a cache built from the registry can tell it is stale.
uint16_t layoutGeneration();

#endif
