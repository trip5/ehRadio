#ifndef themeregistry_h
#define themeregistry_h

// The theme registry — one flat list of theme ids that serves two completely different sources:
//
//   ids 0 .. THEME_BUILTIN_COUNT-1   the compiled _themes[] table in themes.h
//   ids THEME_BUILTIN_COUNT ..       the user slots whose files live in /data
//
// The point of the indirection is id stability.  config.store.themeId is a persisted uint8_t, so a
// count that could shrink — or an id that moved because a file was deleted — would silently
// re-point the device at a different theme.  themeCount() is therefore a constant, and "is there
// anything at this id" is answered by themeFilled() rather than by the count.
//
// Nothing outside this pair and its callers needs to know which source a theme came from: readers
// ask for a copy, not for the table.

#include <stdint.h>
#include <Arduino.h>
// themes.h declares its tables PROGMEM but includes nothing that defines it (the pre-existing
// includers, config.cpp and display.cpp, both reach Arduino.h first).  A registry that forgot this
// would fail to parse the array it measures, so it is paid for here rather than left as a trap.
#include "themes.h"

// Ten user slots is the whole budget: each one costs a ThemeData plus a name buffer, and each one
// has to be named in Config::dataFiles[] or pruneLittleFS() deletes it at the next cleanup.
#define THEME_CUSTOM_MAX 10
#define THEME_BUILTIN_COUNT ((uint8_t)(sizeof(_themes) / sizeof(_themes[0])))
#define THEME_TOTAL_MAX (THEME_BUILTIN_COUNT + THEME_CUSTOM_MAX)

// The slot files as BARE names, because that is what Config::dataFiles[] has to hold:
// pruneLittleFS() compares each /data file's basename against that list, so a path there would never
// match and the file would be deleted at the next cleanup.  The registry builds its /data path from
// this same list, so the two cannot drift apart.
#define THEME_SLOT_FILES "xtheme1.json", "xtheme2.json", "xtheme3.json", "xtheme4.json", "xtheme5.json", \
                         "xtheme6.json", "xtheme7.json", "xtheme8.json", "xtheme9.json", "xtheme10.json"

// The fixed length of the list — built-ins plus every custom slot, filled or not.
uint8_t themeCount();

// Does a theme actually live at this id?  False for an empty custom slot and for anything past the
// end, which is the only test a caller needs before using an id.
bool themeFilled(uint8_t id);

// The colours, or nullptr when the id holds nothing.  Never the caller's to modify.
const ThemeData* themeAt(uint8_t id);

// The display name, or "" when the id holds nothing.  Always a valid C string.
const char* themeName(uint8_t id);

// sizeof(ThemeData) bytes into the caller's storage.  void* because the caller is usually
// config.theme, which is config.h's parallel theme_t rather than a ThemeData — the copy is a
// field-for-field byte copy, exactly as the memcpy_P it replaces.  False, and nothing written, when
// the id is empty, so a caller can fall back.
bool themeCopy(uint8_t id, void* dst);

// An id that is guaranteed to hold a theme: the argument when it is filled, 0 otherwise.
uint8_t themeValidId(uint8_t id);

// Bumped whenever the filled set or any name changes, so a cache built from the registry can tell it
// is stale without the registry having to know the cache exists.
uint16_t themeGeneration();

// Read every slot file.  Called once LittleFS is mounted — which is AFTER config.loadTheme() has
// already run, hence the two-phase rule in themeValidId() — and the caller re-runs
// config.loadTheme() afterwards so a persisted custom id resolves on the same boot.
void themeLoadSlots();

// The element table, one entry per ThemeData field and in field order.  The pair exists so a caller
// never has to see the table to name an element:
//
//   key    the wire and file name, verbatim; what theme files on disk and the editor both use.
//   label  what a human reads, shown by the WebUI.  Never parsed, matched or stored.
//
// Both are valid C strings for any index ("" past the end), so a loop over themeElementCount() can
// call either without a guard.
const char* themeElementKey(size_t index);
const char* themeElementLabel(size_t index);

// How many elements there are, so a caller can iterate the table without including themes.h.
size_t themeElementCount();

// The one document shape, used by the slot files, the Save body and the Apply body alike:
//
//   { "name": "My Neon", "colors": { ".background": 0, ".playlist[0]": 2016 } }
//
// **Known keys, and a missing key is zeroed.**  A key the firmware does not recognise is discarded and a
// key the document leaves out comes back black, so there is one rule for the loader, Save and the preview
// and nothing to reason about: what a file does not say is black.  There is deliberately no
// inherit-from-a-base rule any more - it made a partial file mean different things in different places, and
// the editor always sends all 43 keys.  An [r,g,b] array is accepted in place of a 565 integer so a
// hand-edited file still loads.  nameOut is only written when the document carries a name.  Returns false
// on malformed JSON.
bool themeParseTheme(const char* json, ThemeData* out, char* nameOut, size_t nameRoom);

// The same shape back out, built by hand rather than by a serializer so the element keys are emitted
// exactly as themes.h spells them.  `name` may be nullptr.
//
// REPLACES whatever `out` held: it does not append.  A caller that wants several documents in one buffer
// must build each one into its own String and append that — handing this function a buffer that already
// holds framing destroys the framing.
void themeJsonFor(const ThemeData* data, const char* name, String& out);

// Every filled slot as ONE document: the filled slots in slot order and nothing else — no null for an
// empty slot, nothing padded on the end — or, when exactly one slot is filled, that slot's own document,
// so a single-theme device gets the identical file from either export button.  Replaces `out` in full.
//
// A backup is a SET of themes, not a map of where they sat, which is what lets a restore pack them into
// whatever slots happen to be free instead of insisting on the slots they came from.  That is why there
// is no placeholder: with position gone, a null would mean nothing.
void themeJsonForAllSlots(String& out);

// Slot files, addressed by slot (0 .. THEME_CUSTOM_MAX-1) rather than by theme id.  Both install the
// result in RAM immediately, so a save is visible to the display task without a reboot.
bool themeSaveSlot(uint8_t slot, const char* name, const ThemeData* data);
bool themeDeleteSlot(uint8_t slot);

// The slot's file name (a bare name, as in Config::dataFiles[]), or "" past the end.  The export download
// offers it to the browser when a theme's own name leaves nothing usable for a file name, and it comes from
// the same list everything else is built from, so the two can never disagree about what a slot is called.
const char* themeSlotFileName(uint8_t slot);

// The one scratch theme a live preview is parsed into.  The web handler fills it and queues
// THEMEPREVIEW; the display task copies it into the live theme with themeStagingCopyTo().  A second
// POST simply overwrites it, which is harmless — it carries newer colours for the same preview — and
// a torn read could only ever mix two of the user's own palettes.
ThemeData* themeStaging();
void themeStagingCopyTo(void* dst);

#endif
