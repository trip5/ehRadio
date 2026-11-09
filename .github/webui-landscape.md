# WebUI landscape

Parked idea, kept here so it is not lost. Goal for `player.html` only: on a phone held sideways, **the playlist alone
sits on the right** and everything else stays on the left. Portrait keeps the current single column, untouched.

## Why it is possible without moving markup

Visual placement is decoupled from DOM order, so a media query can re-place an element per breakpoint while the markup
and every id stay put. The WebUI works by id throughout (`showById`, `classEach`, the playlist render,
`alignPlaylistStripes`), so `script.js` does not care where an element sits.

Gate every rule on `@media (orientation: landscape) and (max-height: 520px)`. The height test is the important half:
`orientation: landscape` alone also matches desktop windows, and a phone held sideways has its short side as the height.

## The DOM as it stands

`#playerwrap` children, in order: `#logow`, the name/meta block, `#playernav`, `#equalizerwrap`, `#copy`.

`#equalizerwrap` is both the problem and the opportunity: it already holds the EQ popup (`#equalizerbg`, `#equalizer`),
the volume slider (`#volslider`), the info row (`#info`) and `#playlist`. So "playlist on the right, everything else on
the left" requires the slider and the info row to leave that wrapper.

## Sketch A - no markup change

Promote the wrapper's children into the outer grid with `display: contents`:

    #playerwrap { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1.15fr); column-gap: 14px; max-width: 100%; }
    #equalizerwrap { display: contents; }
    #playlist { grid-column: 2; grid-row: 1 / -1; height: auto; min-height: 0; }
    #logow, #playernav, #info, #volslider, #copy { grid-column: 1; }

Caveat: `#equalizerbg` is `position: absolute` against `#equalizerwrap`, the wrapper that has just stopped making a box.
It would re-anchor to the nearest positioned ancestor, so the EQ dim layer has to be checked - probably by giving
`#playerwrap` `position: relative`, or by moving the popup's anchor.

## Sketch B - one small markup move

Move `#volslider` and `#info` out of `#equalizerwrap` to be direct children of `#playerwrap`. The wrapper then holds
only the EQ popup and the playlist, and no `display: contents` is needed:

    #equalizerwrap { grid-column: 2; grid-row: 1 / -1; height: 100%; min-height: 0; }
    #playlist { height: auto; min-height: 0; }

Ids do not change, so the JS stays untouched, and the EQ overlay keeps its containing block. This is the version worth
trying first.

## Geometry that must be fixed either way

- `#playlist` is `height: 0; flex: 1 1 auto`: inside a grid cell it needs `height: auto; min-height: 0` to fill.
- `#volslider` bleeds with `width: 440px; margin: 26px -20px 0 -20px`: in a narrower column it becomes `width: 100%`
  with the side margins at 0 - the same fix the max-width 1100px block already applies.
- `#playerwrap` is `max-width: 440px; margin: 0 auto`: the query has to lift the cap before two columns can share it.
- `alignPlaylistStripes()` reads a row's real `offsetTop`/`offsetHeight`; that survives re-parenting, but an
  `orientationchange` listener is worth adding so the stripe phase is recomputed after the split.

## Other pages

`search.html`, `settings.html`/`options.html` and the SD manager were built portrait-first, so each needs its own
decision rather than one blanket rule; the technique is the same query-scoped grid. A second, landscape-specific
fragment per page is not worth it - every file in `Config::wwwFiles[]` is a separate HTTPS fetch for the OTA updater.
