# Importing Themes with `importtheme.py`


## Want a Good Theme Editor?

Try: https://vip-cxema.org/index.php/online-kalkulyatory/yoradio-redaktor-tem

Which can be mostly be used to feed the importtheme.py script... read below for more information.

## What it does

[`importtheme.py`](../src/displays/importtheme.py) converts old-style yoRadio theme files
(`#define COLOR_*` format) into ehRadio's [`themes.h`](../src/displays/themes.h) runtime
theme entries. It always appends to the existing `themes.h` — never overwrites.

```
py importtheme.py mytheme.h --name "My Theme"
```

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

## Smart fallbacks

If a color is missing from the old file, the script fills it in automatically:

| Missing field | Falls back to |
|---|---|
| `.dow` | `.date` (same color) |
| `.battery` | `.rssi` (same color) |

## Screensaver colors — COMPUTED, CHECK MANUALLY!

The old yoRadio format did **not** have separate screensaver clock colors. ehRadio
added five new fields for the screensaver clock display:

```c
.clockss       // Screensaver clock digits
.clockbgss     // Screensaver clock background
.secondsss     // Screensaver seconds
.dowss         // Screensaver day-of-week
.datess        // Screensaver date
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

## `#ifdef` / `#ifndef` branching

The script handles old themes that use preprocessor conditions to create
variants (e.g., `#ifdef INVERT_COLORS`). Each branch produces a separate
theme entry with its own name suffix (e.g., "My Theme" and "My Theme (Inverted)").

## Dry-run mode

Always test first with `--dry-run`:

```
py importtheme.py mytheme.h --name "Test" --dry-run
```

This writes to `themes.new.h` without modifying the real file.

## Note Regarding "Invert Title"

When turning on "Invert Title" in the WebUI, the following colors and widgets are affected:

| Theme | Ordinary use | Invert Title On |
|---|---|---|
| `.meta` | the text of `.metaConf` (the station name) | is ignored |
| `.metabg` | the background color of `.metaConf`| is used to color the text |
| `.metafill` | the color of `.metaBGConf` (the box that surrounds `.metaConf`) | the color of `.metaBGConfInv` (the line under `.metaConf`) |
| `.background` | the background color of the rest of the display | the background color of `.metaBGConf` too |
