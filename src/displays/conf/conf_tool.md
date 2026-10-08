# `conf_tool.py` — the layout conf file tool

[`conf_tool.py`](conf_tool.py) maintains the `display*conf.h` files. It has two modes, and both
work from one master.

## The master

[`../widgets/widgetsconfig.h`](../widgets/widgetsconfig.h) — `struct LayoutData` and
`struct BootData` — is the authority for:

- the **field order** (the compiler demands it anyway: these are designated initialisers),
- each field's **type**, which decides what an empty value looks like (`{ }`, or `false`),
- the **section headers**, one set per struct. `BootData` has `/* SCROLLS ... */`,
  `/* LINES + RECTANGLES ... */`, `/* WIDGETS ... */` and `/* BOOT PROGRESS ... */`; `LayoutData` has
  `/* SCROLLS ... */`, `/* SLIDER BARS ... */`, `/* LINES + RECTANGLES ... */`, `/* WIDGETS ... */`,
  `/* NUMBERS FONT ... */`, `/* VU BARS WIDGET ... */`, `/* CODEC BADGE ... */`, `/* VU BANDS ... */`,
  `/* MOVES ... */` and `/* TRANSFORMS ... */`. A label both structs use is not a conflict: each block
  is read and written on its own,
- the **per-field comments** (for example `// VU rotated 90 degrees`).

The tool keeps no list of its own, so a field added to the master appears in every conf the next
time `--clean` runs. Edit the master with its comments and re-run; do not hand-edit the field lists
in the tool.

## Usage

```
py conf_tool.py --clean [--comments=keep|fill|master] [--dry-run]
py conf_tool.py --check [<conf_file>] [--strict]
py conf_tool.py --import <conf_file> --name "Name" [--target <file.h>] [--dry-run]
```

There is no bare invocation on purpose: run with no mode and it prints a two-line pointer and
exits 2. `--help` (or `-h`) prints the full help, which is also the top of the script.

The two writing modes each put a **`*.new.h` temp next to every file** and never touch an original
until you say so. The `.h` extension is deliberate — it opens in the editor with syntax highlighting,
so the result can be read before it is installed. `--check` writes nothing at all.

---

## Mode 1 — `--clean`, the wildcard pass

Repairs every `display*conf.h` in this directory against the master.

### 1. `_layoutNames[]` against `_layouts[]`

The two arrays must be the same length, in the same order: a layout's index is its position in
both. Too few names are **appended**, taken from each entry's own `{   // Name` comment. Too many
are reported and the surplus is cut. `_layoutNames[]` wins, so afterwards every entry comment is
rewritten to match its name.

### 2. Every field, in master order

Each layout entry is rewritten with **all** of `struct LayoutData`, and `_bootConfig` with all of
`struct BootData`. A field the file did not have is written as `{ }` — or `false` for a boolean,
with the master's comment where it has one. Values that were there are kept verbatim.

### 3. Section headers

Restored where missing, normalised where the text has drifted (a file's `/* BANDS ... */` becomes
the master's `/* VU BANDS ... */`), and a duplicated header collapses to one.

### 4. Comments

Every comment is preserved:

- a trailing comment stays as the file has it (`// unused`, `// <--------- NEEDS EDITING!`,
  `// clock disappears when VU is on`) — unless `--comments=master` is given, see below,
- a commented-out alternative stays on the side of the field it was found on — above the field, or
  below it, whichever the file used,
- a comment that names a field (`// .clockConf = {...}`) travels with that field,
- an unknown or obsolete field (`// ??? (Unused by ehRadio) ...`) travels with the field it
  followed,
- a note above a field stays with the field below it, and a note that ends a group with a blank
  line stays with the field above it.

Three values, and the default is `keep`:

| value | what it does |
|---|---|
| `keep` | the conf's own text, always. Nothing is added. |
| `fill` | as `keep`, plus the master's comment on a field the conf has none for, so a conf that never documented a field comes to match the master |
| `master` | `fill`, plus the master's wording where the conf already carried a comment, so a sentence that drifted is corrected |

Only a field the **master itself comments** can be affected, and today that is the seven `TRANSFORMS`
booleans at the end of `struct LayoutData`. Every other field carries a section header and no
per-field comment, so there is nothing for `fill` or `master` to apply - which is why `keep` is the
default: the two other modes are a correction tool for drifted sentences, not a mass documentation
pass. A comment only the confs have is left alone either way.

Every change is named in the run report before anything is installed:

```
displayOLED128x64conf.h
  entry 2 ("Compact"): .shareWeatherIP trailing comment re-texted from the master:
      was: // IP and weather share the bottom row
      now: // IP and weather share one row (was the IP_WEATHER_SHARED macro)
```

Read those lines in `--dry-run` first. If a conf's own wording is worth keeping even so, put a
marker in it — see **What it refuses to do**. `--comments` applies to `--clean` only: `--import`
already writes the master's comments for the fields it fills in, so passing the switch there is
refused, and an unknown value dies listing the three valid ones.

### 5. MOVEs: the retired `width = -1`

`clockMove = { 0, 0, -1 }` used to mean "keep the conf position", and it was a trap: the entry is not
empty as far as the code is concerned, so the widget neither moved **nor** yielded to the VU. It is
retired, and `--clean` rewrites every one it finds as the widget's **own coordinates** —
`{ <clockConf left>, <clockConf top>, 0 }` for the clock, the `weatherConf` pair for the two weather
moves — copying the text verbatim, so an expression such as `DSP_HEIGHT-50` comes across unchanged.

That is an identity, because a MOVE writes the same `left`/`top` the conf does and a `width` of 0 keeps
the conf's own span. The rewrite is named in the report like any other change:

```
entry 0 ("Default"): .clockMove written out from the retired width -1:
      was: { 0, 94, -1 }
      now: { 0, 94, 0 }   (from .clockConf)
```

One case is deliberately left alone: an owner whose own `left` and `top` are both `0` would expand to
`{ 0, 0, 0 }`, which is the `{ }` spelling for "yield to the VU" — quietly turning "stay put" into
"disappear" is worse than leaving one line for the author to read. `--import` does the same expansion,
from the entry it is building.

### 6. What it refuses to do

A layout entry with no `.metaConf` or no `.playlistConf` is **not repaired**. Those two are load
bearing — dialogs write into the meta line and the playlist page is built on `playlistConf` — so
the tool prints a serious warning naming the file, the entry index, its name and the missing
field, leaves that entry byte-identical, and lists the file as **NEEDS HAND EDITING** at the end.
Nothing is ever removed: deleting a layout would shift every index after it, and that index is
persisted in `config.store.layoutId`.

A trailing comment that carries a marker — `NEEDS EDITING`, `DO NOT EDIT`, `DO NOT REMOVE`,
`SAME AS ABOVE` — is **never rewritten**, even under `--comments=master`, and the run names that
line along with what the master wanted to say there instead. A protected marker is the way to pin a
conf's own wording that the master cannot know, such as a sentence that is true on one panel only.

### 7. Install

The run prints a per-file report (fields written, headers normalised, order violations, fields with
no master slot), then a summary, then:

```
Update all 14 conf file(s)? [y/N]
```

Only `y` copies the temps over the originals and deletes them; declining offers to delete the temps
instead. **`--dry-run`** writes the same temps, never installs, and ends by offering to delete
them — read them first, then re-run without `--dry-run` to install.

`--clean` is idempotent: run it twice and the second run reports every file unchanged.

---

## Mode 2 — `--check`, intersecting rectangles

Read-only: it writes nothing, takes no `--dry-run`, and is safe on a tree you are still editing.

```
py conf_tool.py --check                        # every conf file
py conf_tool.py --check displayTFT428x142conf.h --strict
```

A widget that updates itself repaints **its own rectangle**, background included, so two rectangles
that overlap mean one eats the other every time it updates — which is how a weather line can cut a band
out of the codec badge that shares its row (see [`layout_key.md`](layout_key.md), "The rectangle each
of those owns"). This mode finds those pairs before the panel does.

Three things make the report usable rather than a wall of geometry:

- **Only pairs a self-updating widget is part of are reported.** A band, a rule or the playlist
  background is drawn once per page pass, in an order the pass itself decides, so a title sitting on
  its own underline is by design and saying so would bury the real findings.
- **Every layout is checked in all three states** — stopped, playing with the meter off, and playing
  with it on. That is where the MOVEs come in: the clock and the weather are only displaced while the
  meter is on, a `{ }` MOVE takes its widget off the screen entirely, and the meter's own box only
  exists while it is showing.
- **An edge the tool cannot know is not guessed.** A runtime string's width, a centred line and the
  clock's glyph run come back as `?`, and a pair that touches one is reported as **possibly
  collides** rather than as a collision. A plain `collides` means the conf itself states both
  rectangles.

```
displayTFT320x240conf.h

Layout 1: BoomBox (VaraiTamas)

.clockConf possibly collides with .fullbitrateConf
      while stopped:  clockConf x ?, y 118-171;  fullbitrateConf x 8-49, y 124-165
      while playing, no VU:  clockConf x ?, y 118-171;  fullbitrateConf x 8-49, y 124-165
      while playing, VU on:  clockConf x ?, y 118-171;  fullbitrateConf x 8-49, y 124-165

.rssiConf possibly collides with .batteryConf
      while stopped:  rssiConf x ?, y 214-222;  batteryConf x ?, y 214-222
But OK because .shareBattRSSI is active
```

The last two lines are the point of the exemptions: the pair is real, the layout declares it, and
`.shareWeatherIP` / `.shareBattRSSI` are named so the reader knows it is deliberate. Fixing a genuine
one is a conf change — narrow the span with a MOVE, or give one of the two a different `top`.

### One file, and the `--strict` switch

The file is **optional**. Naming one keeps the report to a screenful — a bare name is looked for next
to this script, with or without the `.h`, so `--check displayTFT428x142conf.h` and
`--check displayTFT428x142conf` both work. Leaving it out walks all fourteen, which is what you want
before a release rather than while working on one layout.

**`--strict`** drops the pairs that model (b) cannot pin down — two runtime strings on one row, which
on a dense panel is most of the output — and names them on a single line per layout instead:

```
Layout 0: Default

      held back by --strict: 6 pair(s) an edge of which is runtime text, so they may or may not touch: .voltxtConf/.iptxtConf, .voltxtConf/.rssiConf, .voltxtConf/.batteryConf, .iptxtConf/.rssiConf, .iptxtConf/.batteryConf, .rssiConf/.batteryConf
```

Nothing is dropped in silence and the summary counts them, so a layout whose *only* finding is a
possible one still says so. Be aware of what it holds back: a pair counts as unsure when **either**
side has an unknown edge, and the clock's own width can never be known (its glyphs are as wide as their
own run), so **anything against `.clockConf` runs through that line** even when the two boxes plainly
overlap in the vertical band. Read the held-back line, not just the groups.

---

## Mode 3 — `--import`, community conf files

Converts an old-style yoRadio display conf into ehRadio's format. It either

- **appends a layout** to an existing `displayTFT{W}x{H}conf.h` (chosen by `DSP_WIDTH` ×
  `DSP_HEIGHT`, or by `--target`), or
- **creates the file** if the target does not exist, with `Define` lines, the `BootData` block,
  `_layoutNames[]`, the entries and the `STRINGS` section.

```
py conf_tool.py --import community_conf.h --name "Author Name"
py conf_tool.py --import community_conf.h --name "Author Name" --dry-run
```

### Input format (old yoRadio style)

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

### What it handles

- **`#ifdef BOOMBOX_STYLE` blocks** produce two entries: the standard one and one named
  `" (BoomBox)"`, the latter emitted as `.boomboxVU = true`. The old format spelled the flag
  `BOOMBOX_STYLE`; that detection and the layout **names** are unchanged, because they describe the
  old file, not ehRadio.
- **`#define HIDE_*`** zeroes the matching config and keeps the original as a comment below it:

  ```c
  .batteryConf = { }, // unused
  // .batteryConf = { 320, 282, 2, WA_LEFT },
  ```

- **`HIDE_IP_ONLY_MAIN_SCREEN` and `RSSI_DIGIT`** are switches rather than hides, so they become
  `.shareWeatherIP = true` and `.rssiDigit = true`. An explicit `false`/`0` is not a request.
- **`#if BITRATE_FULL` blocks** are stripped, `TITLE_FIX` is read from them and substituted into the
  values that use it.
- **`bandsConf`** loses an obsolete sixth value (`fadespeed`; the fade rate now comes from the band
  length and `VU_FADE_MS`). Only a plain integer is dropped, so a real expression is never lost.
- **OLED targets** get `metaBGConf` / `metaBGConfInv` sorted by height, because ehRadio selects
  `metaBGConfInv` when *invert title* is on and OLEDs default to inverted: the hairline (height 3 or
  less) goes to `metaBGConf`, the rectangle to `metaBGConfInv`. Two hairlines or two rectangles are left
  as they are rather than guessed at, a lone one moves to the slot its own height calls for, and a height
  that is not a plain integer stops the sort with a note. A yoRadio conf cannot say whether it is for an
  OLED or a TFT — an OLED conf and a TFT conf are both a band plus a rule — so when a **new** conf is
  being created and no file here matches the source size, the tool asks.
- **`.apTitleBGConf`** is derived from `metaBGConf` after that sort — the band on a TFT, the hairline on
  an OLED — and the emitted line says where it came from (`// from metaBGConf`, or
  `// was metaBGConfInv (ehRadio flips yoRadio's metaBGConf to metaBGConfInv)` when it was moved). It is
  the band the AP and SD-manager screens draw, and they ignore *invert title*.
- **What the source did not provide** is written as `{ }` and flagged, so it is easy to find:

  ```c
  .clockConf = { },                                                   // <--------- NEEDS EDITING!
  ```

- **Names** are truncated to 50 characters (`_layoutNames[][64]`).

Every emitted entry lists every master field, in master order, exactly like a `--clean` result, so an
imported layout is instantly readable next to the shipped ones.

### After importing

Read the temp, fill in the `// <--------- NEEDS EDITING!` values by hand, then install it. Run
`py conf_tool.py --clean` afterwards to be sure the file matches the master.

See [`layout_key.md`](layout_key.md) for what every field means and how to choose the numbers.
