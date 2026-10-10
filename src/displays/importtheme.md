# Importing Themes with `importtheme.py`


## Want a Good Theme Editor?

Try: https://vip-cxema.org/index.php/online-kalkulyatory/yoradio-redaktor-tem

Which can be mostly be used to feed the importtheme.py script... read below for more information.

## What it does

[`importtheme.py`](../src/displays/importtheme.py) turns theme files into ehRadio's
[`themes.h`](../src/displays/themes.h) runtime entries. It always appends to the existing `themes.h` —
never overwrites.

Two input formats, told apart by the file itself:

* an **old-style yoRadio theme** (`#define COLOR_*  r,g,b`), which is the rest of this document, and
* an **ehRadio JSON export** from the WebUI theme editor — the single-theme `Export` file or the
  `Backup All` array. See *JSON input* below; it is the shorter read.

```
py importtheme.py mytheme.h --name "My Theme"      # old style, a name is required
py importtheme.py "ehRadioTheme-My Neon.json"      # one theme, named by the file
py importtheme.py ehRadioThemes-Backup.json        # a whole backup, numbered
```

Either way, every colour written is put onto the 565 steps the WebUI editor uses — see
*565 steps and `--quantise`* below.

## Input format (old yoRadio style)

```c
#define COLOR_BACKGROUND        0,0,0
#define COLOR_STATION_NAME      255,255,255
#define COLOR_STATION_BG        0,0,125
#define COLOR_STATION_FILL      0,0,125
#define COLOR_SNG_TITLE_1       255,255,255
#define COLOR_SNG_TITLE_2       105,105,105
// ... etc
```

## Output format (ehRadio runtime)

```c
{   // My Theme
    .background   = RGB(  0,   0,   0),
    .meta         = RGB(255, 255, 255),
    .metabg       = RGB(  0,   0, 125),
    .metafill     = RGB(  0,   0, 125),
    .title1       = RGB(255, 255, 255),
    .title2       = RGB(105, 105, 105),
    // ... etc
},
```

## Color field mapping

| Old `#define` | New `.field` |
|---|---|
| `COLOR_BACKGROUND` | `.background` |
| `COLOR_STATION_NAME` | `.meta` |
| `COLOR_STATION_BG` | `.metabg` |
| `COLOR_STATION_FILL` | `.metafill` |
| `COLOR_SNG_TITLE_1` | `.title1` |
| `COLOR_SNG_TITLE_2` | `.title2` |
| `COLOR_DIGITS` | `.digit` |
| `COLOR_DIVIDER` | `.div` |
| `COLOR_WEATHER` | `.weather` |
| `COLOR_VU_PEAK` | `.vupeak` |
| `COLOR_VU_MAX` | `.vumax` |
| `COLOR_VU_MIN` | `.vumin` |
| `COLOR_CLOCK` | `.clock` |
| `COLOR_CLOCK_BG` | `.clockbg` |
| `COLOR_SECONDS` | `.seconds` |
| `COLOR_DAY_OF_W` | `.dow` |
| `COLOR_DATE` | `.date` |
| `COLOR_CLOCK_SS` | `.clockss` |
| `COLOR_CLOCK_BG_SS` | `.clockbgss` |
| `COLOR_SECONDS_SS` | `.secondsss` |
| `COLOR_DAY_OF_W_SS` | `.dowss` |
| `COLOR_DATE_SS` | `.datess` |
| `COLOR_BUFFER` | `.buffer` |
| `COLOR_IP` | `.ip` |
| `COLOR_VOLUME_VALUE` | `.vol` |
| `COLOR_RSSI` | `.rssi` |
| `COLOR_BATTERY` | `.battery` |
| `COLOR_BITRATE` | `.bitrate` |
| `COLOR_VOLBAR_OUT` | `.volbarout` |
| `COLOR_VOLBAR_IN` | `.volbarin` |
| `COLOR_PL_CURRENT` | `.plcurrent` |
| `COLOR_PL_CURRENT_BG` | `.plcurrentbg` |
| `COLOR_PL_CURRENT_FILL` | `.plcurrentfill` |
| `COLOR_PLAYLIST_0..4` | `.playlist[0..4]` |

Three fields in `ThemeData` have **no** old-format counterpart, so no `COLOR_*` maps to them: `.line`
(sits between `.div` and `.weather`), `.vuaxis` (between `.weather` and `.vupeak`) and `.textss`
(between `.datess` and `.buffer`). All three are derived by rule - the first two from `.div`, the third
from `.clockss`, see below. They still occupy their place in `FIELD_ORDER`, because the emitted entries
are designated initialisers and have to follow the struct's own order.

## Smart fallbacks

If a color is missing from the old file, the script fills it in automatically:

| Missing field | Falls back to |
|---|---|
| `.vupeak` | `.title1` (same color) |
| `.dow` | `.date` (same color) |
| `.battery` | `.rssi` (same color) |

Any *other* missing field is filled with `.meta` and flagged with a `// needs fixing?` comment
on the generated line, so the values that need a human eye are easy to spot.

## Screensaver colors — COMPUTED, CHECK MANUALLY!

The old yoRadio format did **not** have separate screensaver colors. ehRadio
added six fields for the screensaver display:

```c
.clockss       // Screensaver clock digits
.clockbgss     // Screensaver clock background
.secondsss     // Screensaver seconds
.dowss         // Screensaver day-of-week
.datess        // Screensaver date
.textss        // Screensaver info line (station, titles, weather)
```

If the old theme file has `COLOR_CLOCK_SS` etc., those values are used directly.
**Otherwise the script computes fallback values:**

| Field | Computed from | Multiplier |
|---|---|---|
| `.clockss` | `.clock` | 50% |
| `.clockbgss` | `.clockss` | 15% |
| `.secondsss` | `.seconds` | 50% |
| `.dowss` | `.dow` | 50% |
| `.datess` | `.date` | 50% |
| `.clockbg` | `.clock` | 15% |

`.textss` is deliberately **not** in that table. It is a rule rather than a computed guess - the
screensaver's info line is the same ink as the screensaver clock, so it is `.clockss` verbatim, which
is why it is listed under the derived rules below and carries no `// needs fixing?` marker.

**These computed values are a starting point — they should be reviewed and
tweaked by hand.** The screensaver runs on a black background and often benefits
from dimmer, less saturated colors than the main clock display.

Example of hand-adjusted screensaver colors in a theme entry:

```c
.clockss       = RGB(100, 112, 255),   // dimmed blue
.clockbgss     = RGB( 10,  10,  10),   // near-black
.secondsss     = RGB(100, 112, 255),
.dowss         = RGB(255, 255, 255),   // white for visibility
.datess        = RGB(255, 255, 255),
```

## Line, VU axis and info line — DERIVED, NO REVIEW NEEDED

Three palette entries were added to ehRadio after the old theme format was defined, so no old theme file
can carry them. `.line` and `.vuaxis` derive from `.div`, which every theme file sets, and `.textss`
from `.clockss` - which makes them rules rather than guesses:

| Field | Derived from | Multiplier | Consumed by |
|---|---|---|---|
| `.line` | `.div` | 100% (the divider verbatim) | the under and over line widgets (`underLineConf` / `overLineConf` in the layout) |
| `.vuaxis` | `.div` | 25% | every reference line the VU draws: the centre cross, the bar baseline, the histogram line and the Spectrum divider |
| `.textss` | `.clockss` | 100% (verbatim) | the scrolling info line the screensaver draws when `screensavertext` is on |

`.vuaxis` is deliberately a quarter of the divider and **not** a copy of `.clockbg`. The axis is a
reference line that has to read against the theme's own background, and `.clockbg` is a background
shade: copying it left the axis nearly invisible on Graphite (10,10,10), UltraPerfect (0,0,0), White and
Black (229,229,229 on 255 white), Ocean (0,0,62 on 0,0,91) and vip-cxema (29,29,0). A quarter of the
divider keeps real contrast on black and on white alike - white dividers give 63, Graphite's 91 gives
22 - and it reads as a relationship, the axis being a dimmed divider.

Because the two are derived by a stated rule, the script emits them **without** the
`// needs fixing?` marker (they are listed in `DERIVED_RULES`); every other computed fallback keeps its
marker, because those are still eyeballed.

On reduced palettes the rule is overridden by hand, in the palette rather than the theme:
`displaySSD1322.cpp` sets `.line = GRAY_9` (equal to its `.div`) and `.vuaxis = GRAY_3`, since a
four-shade grey palette has no quarter of `GRAY_9`, and `oledcolorfix.h` sets both to `TFT_FG`, because a
one-bit panel has a single ink.

## `#ifdef` / `#ifndef` branching

The script handles old themes that use preprocessor conditions to create
variants (e.g., `#ifdef INVERT_COLORS`). Each branch produces a separate
theme entry with its own name suffix (e.g., "My Theme" and "My Theme (Inverted)").

## JSON input (a WebUI export)

`Export` in the theme editor downloads one theme as a document; `Backup All` downloads every saved slot as
an array of them. Both are accepted, and the shape is what says which is which: a `{` is one theme, a `[`
is a list. A `null` in an older backup is skipped rather than trusted.

```json
{"name":"My Neon","colors":{".background":0,".meta":63422}}
```

* The **keys are the element names** from `themes.h` with a leading dot, so the mapping is a strip and
  nothing else. A key that is not one of ours is reported as unknown, and a field the document leaves out
  goes through exactly the same fallbacks as an old colour file.
* The **values are 565 integers**, converted to the numbers the editor shows — each level times its own
  step — so a colour arriving this way is already on the grid. An `[r, g, b]` array is accepted in place of
  a 565 integer, the same tolerance the firmware's own loader has for a hand-edited file.
* The **name comes from the document**; `--name` overrides it, and a theme with neither falls back to the
  file name. A multi-theme file is numbered the way the old format's variants are (`... 1`, `... 2`),
  because a backup often holds several saves of one theme that all carry the same name inside.
* A name already in `_themeNames` is reported, since re-importing the same export is the easy way to end
  up with two entries that read alike.

## 565 steps and `--quantise`

The panel stores RGB565 — 32 red levels, 64 green, 32 blue — so the 8-bit values are only buckets: every
one of 248..255 is red level 31 and the panel draws the identical colour for all of them. The WebUI editor
therefore shows a level as `level * step`: 0, 8, 16 ... 248 for red and blue, 0, 4, 8 ... 252 for green.
This script writes those same numbers, so a palette, the editor and the panel all agree.

Everything the script writes is snapped onto those steps, including the computed fallbacks, which are
percentages of another colour and so land anywhere.

`--quantise` does the same for the colours **already** in `themes.h`:

```
py importtheme.py --quantise --dry-run     # report, then write themes.h.new.h
py importtheme.py --quantise               # rewrite themes.h
```

It cannot change a pixel: `(r >> 3) * 8` packs back to the same 565 that `r` did, so the panel draws the
same colour and only the numbers move, each one within its own bucket. A number keeps the width it had, so
the file's alignment survives, and a value already on a step is left alone — which is what makes a second
run report nothing to do. It says how many colours it moved, and it refuses to write at all if any of them
would have changed its 565.

## Dry-run mode

Always test first with `--dry-run`:

```
py importtheme.py mytheme.h --name "Test" --dry-run
```

This writes to `themes.h.new.h` without modifying the real file. It is what `--quantise --dry-run` uses
too.

## Note Regarding "Invert Title"

When turning on "Invert Title" in the WebUI, the following colors and widgets are affected:

| Theme | Ordinary use | Invert Title On |
|---|---|---|
| `.meta` | the text of `.metaConf` (the station name) | is ignored |
| `.metabg` | the background color of `.metaConf`| is used to color the text |
| `.metafill` | the color of `.metaBGConf` (the box that surrounds `.metaConf`) | the color of `.metaBGConfInv` (the line under `.metaConf`) |
| `.background` | the background color of the rest of the display | the background color of `.metaBGConf` too |
