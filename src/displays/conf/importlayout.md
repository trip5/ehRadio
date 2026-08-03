# Importing Layouts with `importlayout.py`

## What it does

[`importlayout.py`](../src/displays/conf/importlayout.py) converts community yoRadio
display configuration files into ehRadio's conf file format. It can either:

- **Create a new conf file** — if the target doesn't exist, it generates a
  complete `displayTFT{W}x{H}conf.h` with `BootData`, `_layoutNames[]`, and
  `_layouts[]` arrays
- **Append to an existing file** — if the target exists, it adds the new layout
  as another entry in the `_layouts[]` array

```
py importlayout.py community_conf.h --name "Author Name"
```

## Input format (old yoRadio style)

The old yoRadio format used individual `const` declarations for each widget
config, often wrapped in `#ifdef` blocks for conditional features:

```c
const ScrollConfig apTitleConf PROGMEM = {{ 10, 10, 4, WA_CENTER }, 140, false, 460, 0, 3, 5000};

#ifdef BOOMBOX_STYLE
const FillConfig volbarConf PROGMEM = {{ 10, 302, 0, WA_LEFT }, 460, 6, true};
#else
const FillConfig volbarConf PROGMEM = {{ 10, 302, 0, WA_LEFT }, 460, 6, false};
#endif

#define HIDE_WEATHER
#define HIDE_BATTERY
```

## Output format (ehRadio struct arrays)

```c
const LayoutData _layouts[] PROGMEM = {
    {   // Author Name
        .metaConf   = {{ 10, 10, 4, WA_LEFT }, 140, true, 460, 2000, 3, 5000},
        .weatherConf = { },  // was HIDE_WEATHER
        .batteryConf = { },  // was HIDE_BATTERY
        // ...
    },
};
```

## What the script handles automatically

### `#ifdef BOOMBOX_STYLE` blocks

If the old conf has BoomBox variants, the script creates **two layout entries**:
- One standard layout
- One BoomBox layout (with `" (BoomBox)"` appended to the name)

### `#define HIDE_*` directives

Old-style hide macros are converted to empty configs (`{ }`). The original
value is preserved as a comment so you can restore it later:

```c
.batteryConf = { },  // was HIDE_BATTERY: { 320, 282, 2, WA_LEFT }
```

### `#if BITRATE_FULL` blocks

The old `TITLE_FIX` expression is extracted, the `#if BITRATE_FULL` block is
removed, and `TITLE_FIX` is substituted in expressions like
`MAX_WIDTH-(TITLE_FIX==0?6*2*7-6:TITLE_FIX)`.

### Boot/AP configuration

The first layout's boot/config fields (`apTitleConf`, `apSettConf`, etc.) are
extracted into a shared `BootData _bootConfig` block that applies to all
layouts. This avoids duplicating boot screen settings across every layout entry.

### Format strings

Format strings (`numtxtFmt[]`, `rssiFmt[]`, `iptxtFmt[]`, etc.) are carried
over from the original file. If any are missing, defaults are provided.

## What needs manual checking after import

1. **Coordinate values** — The script preserves coordinates as-is, but the
   original conf may have been designed for a different display resolution.
   Verify that widgets fit within the target display dimensions.

2. **`BOOTLOGOTOP` and other `#define` values** — These are carried over
   but may need adjustment for the target display.

3. **Empty configs from `HIDE_*`** — If you want these widgets visible,
   uncomment the original values shown in the trailing comment.

4. **Format strings** — Verify they match what your display expects
   (e.g., `rssiFmt` might be `"%d"` or `"WiFi %d"` depending on locale).

## Dry-run mode

```
py importlayout.py community_conf.h --name "Test" --dry-run
```

Writes to `<target>.new.h` without modifying the real file.
