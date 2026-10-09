#include "options.h"
#include <time.h>
#include <Arduino.h>
#include <Ticker.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include <SD.h>
#include "sdmanager.h"
#ifdef USE_SD
  #include "filemanager.h"   // the SD Manager screen and its countdown
#endif
#include "startup.h"
#include "utility.h"
#include "backlightcontrols.h"
#include "rgbled.h"
#include "../locale/dsplocale.h"
#include "../displays/dspcore.h"
#include "../displays/themes.h"
#include "../displays/widgets/pages.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/widget_vu.h"
#include "../displays/tools/dspstats.h"
#include "battery.h"

extern const char batterytxtFmt[] PROGMEM;

// These three flags are per-layout booleans in LayoutData now, not per-model macros.

Display display;

// Layout switching — pointers initially point to PROGMEM defaults
LayoutData activeLayout;
#ifndef DUMMYDISPLAY
const ScrollConfig*   metaConf_ptr        = &_layouts[0].metaConf;
const ScrollConfig*   title1Conf_ptr      = &_layouts[0].title1Conf;
const ScrollConfig*   title2Conf_ptr      = &_layouts[0].title2Conf;
const ScrollConfig*   playlistConf_ptr    = &_layouts[0].playlistConf;
const ScrollConfig*   weatherConf_ptr     = &_layouts[0].weatherConf;
const FillConfig*     metaBGConf_ptr      = &_layouts[0].metaBGConf;
const FillConfig*     metaBGConfInv_ptr   = &_layouts[0].metaBGConfInv;
const FillConfig*     volbarConf_ptr      = &_layouts[0].volbarConf;
const FillConfig*     playlBGConf_ptr     = &_layouts[0].playlBGConf;
const FillConfig*     bufferbarConf_ptr   = &_layouts[0].bufferbarConf;
const WidgetConfig*   bitrateConf_ptr     = &_layouts[0].bitrateConf;
const WidgetConfig*   voltxtConf_ptr      = &_layouts[0].voltxtConf;
const WidgetConfig*   batteryConf_ptr     = &_layouts[0].batteryConf;
const WidgetConfig*   iptxtConf_ptr       = &_layouts[0].iptxtConf;
const WidgetConfig*   rssiConf_ptr        = &_layouts[0].rssiConf;
const WidgetConfig*   numConf_ptr         = &_layouts[0].numConf;
const WidgetConfig*   clockConf_ptr       = &_layouts[0].clockConf;
const WidgetConfig*   vuConf_ptr          = &_layouts[0].vuConf;
const BitrateConfig*  fullbitrateConf_ptr = &_layouts[0].fullbitrateConf;
const VUBandsConfig*  bandsConf_ptr       = &_layouts[0].bandsConf;
const MoveConfig*     clockMove_ptr       = &_layouts[0].clockMove;
const MoveConfig*     weatherMove_ptr     = &_layouts[0].weatherMove;
const MoveConfig*     weatherMoveVU_ptr   = &_layouts[0].weatherMoveVU;
const bool*           fullClock_ptr       = &activeLayout.fullClock;
const bool*           seconds_ptr         = &activeLayout.seconds;
const bool*           boomboxVU_ptr       = &activeLayout.boomboxVU;
const bool*           rotateVU_ptr        = &activeLayout.rotateVU;
// Point into activeLayout (the memcpy_P target), so no re-pointing on a layout switch.
const bool*           shareWeatherIP_ptr  = &activeLayout.shareWeatherIP;
const bool*           shareBattRSSI_ptr   = &activeLayout.shareBattRSSI;
const bool*           rssiDigit_ptr       = &activeLayout.rssiDigit;
const FillConfig*     underLineConf_ptr   = &_layouts[0].underLineConf;
const FillConfig*     overLineConf_ptr    = &_layouts[0].overLineConf;
uint8_t layoutCount = (sizeof(_layoutNames) / sizeof(_layoutNames[0]));

// The active system font id.  dspfont.h cannot read config itself - config.h includes options.h, which
// includes dspfont.h - so the value lives here and displayFont() reads it; DISPLAYFONT is only the boot
// default now.  Kept in step with config.store.systemFontId below.
uint8_t activeSystemFontId = DISPLAYFONT;

// The same for the clock font STYLE (which of the three designs, not which size - the size is the
// layout's own clockConf.textsize).  clockFontStyle() in dspfont.h is the accessor, so every reader
// goes through here rather than testing a compile-time CLOCKFONT.
uint8_t activeClockFontId = CLOCKFONT;

// ---- Layout owns widget existence -------------------------------------------------------------
// An omitted widget is HIDDEN, never freed - the other core may be inside its _draw().  Three flags
// because the _draw() guards differ per type; _present is authoritative.  See plans/layout-widget-lifecycle.md
static void hideByLayout(Widget* w) { if (w) { w->setPresent(false); w->lock(true); w->setActive(false, true); } }
static void showByLayout(Widget* w) { if (w) { w->setPresent(true); w->unlock(); w->setActive(true); } }

// ---- Does the active layout provide this widget? ----------------------------------------------
// "Absent" is spelled differently per config type, so it is defined once per type and inlines to one
// comparison.  Consult it at every show/hide AND re-show site.  See plans/layout-widget-lifecycle.md
static inline bool present(const WidgetConfig&  c) { return c.textsize  > 0; }
// Scroll needs both fields: buffsize alone passes a layout that then divides by zero in init().
static inline bool present(const ScrollConfig&  c) { return c.buffsize > 0 && c.widget.textsize > 0; }
static inline bool present(const FillConfig&    c) { return c.height    > 0; }
static inline bool present(const BitrateConfig& c) { return c.dimension > 0; }
// A VU needs its geometry too: _draw() divides by bands.perheight, so zeroed bands would divide by zero.
static inline bool present(const VUBandsConfig& c) { return c.width > 0 && c.height > 0 && c.perheight > 0; }

static inline bool metaInLayout()        { return present(*metaConf_ptr); }
static inline bool title1InLayout()      { return present(*title1Conf_ptr); }
static inline bool title2InLayout()      { return present(*title2Conf_ptr); }
static inline bool playlistInLayout()    { return present(*playlistConf_ptr); }
static inline bool weatherInLayout()     { return present(*weatherConf_ptr); }
static inline bool vuInLayout()          { return present(*vuConf_ptr) && present(*bandsConf_ptr); }
static inline bool bitrateInLayout()     { return present(*bitrateConf_ptr); }
static inline bool fullbitrateInLayout() { return present(*fullbitrateConf_ptr); }
static inline bool volbarInLayout()      { return present(*volbarConf_ptr); }
static inline bool bufferbarInLayout()   { return present(*bufferbarConf_ptr); }
static inline bool voltxtInLayout()      { return present(*voltxtConf_ptr); }
static inline bool ipInLayout()          { return present(*iptxtConf_ptr); }
static inline bool rssiInLayout()        { return present(*rssiConf_ptr); }
static inline bool batteryInLayout()     { return present(*batteryConf_ptr); }
// The clock and the digits are the exception: both draw a GFX font, so their confs carry textsize 0 even
// when present.  Hence all-zero-means-absent below, not a textsize test - that boot-looped (plan section 9).
static inline bool zeroed(const WidgetConfig& c) { return c.left || c.top || c.textsize || c.align; }
static inline bool clockInLayout()       { return zeroed(*clockConf_ptr); }
static inline bool numInLayout()         { return zeroed(*numConf_ptr); }
#else
const ScrollConfig*   metaConf_ptr        = nullptr;
const ScrollConfig*   title1Conf_ptr      = nullptr;
const ScrollConfig*   title2Conf_ptr      = nullptr;
const ScrollConfig*   playlistConf_ptr    = nullptr;
const ScrollConfig*   weatherConf_ptr     = nullptr;
const FillConfig*     metaBGConf_ptr      = nullptr;
const FillConfig*     metaBGConfInv_ptr   = nullptr;
const FillConfig*     volbarConf_ptr      = nullptr;
const FillConfig*     playlBGConf_ptr     = nullptr;
const FillConfig*     bufferbarConf_ptr   = nullptr;
const WidgetConfig*   bitrateConf_ptr     = nullptr;
const WidgetConfig*   voltxtConf_ptr      = nullptr;
const WidgetConfig*   batteryConf_ptr     = nullptr;
const WidgetConfig*   iptxtConf_ptr       = nullptr;
const WidgetConfig*   rssiConf_ptr        = nullptr;
const WidgetConfig*   numConf_ptr         = nullptr;
const WidgetConfig*   clockConf_ptr       = nullptr;
const WidgetConfig*   vuConf_ptr          = nullptr;
const BitrateConfig*  fullbitrateConf_ptr = nullptr;
const VUBandsConfig*  bandsConf_ptr       = nullptr;
const MoveConfig*     clockMove_ptr       = nullptr;
const MoveConfig*     weatherMove_ptr     = nullptr;
const MoveConfig*     weatherMoveVU_ptr   = nullptr;
const bool*           fullClock_ptr       = nullptr;
const bool*           seconds_ptr         = nullptr;
const bool*           boomboxVU_ptr       = nullptr;
const bool*           rotateVU_ptr        = nullptr;
const bool*           shareWeatherIP_ptr  = nullptr;
const bool*           shareBattRSSI_ptr   = nullptr;
const bool*           rssiDigit_ptr       = nullptr;
uint8_t layoutCount = 0;
#endif

#ifndef DUMMYDISPLAY
// Real build only: no widgets.h under DSP_DUMMY, so Widget is incomplete here and there is nothing to lock.

// Widget::lock() is not idempotent - always change lock state through this (once re-cleared the IP).
static void lockIfChanged(Widget* w, bool hide) { if (w && w->locked() != hide) w->lock(hide); }

// Coming back from hidden needs an explicit redraw: unlock() does not draw, and the clock only ticks seconds.
static void redrawIfVisible(Widget* w) { if (w && !w->locked()) w->setActive(true); }

// The weather's hide is the one that has to clear _active as well as _locked.  ScrollWidget::setText()
// paints inline and, deliberately, consults only _active - the OTA progress label is locked at
// construction and is drawn by nothing but setText() - so a lock on its own is undone by the next
// weather refresh, which is how a line yielded to the VU reappears over the bitrate or the badge.
// The clock, the buffer bar and the VU keep using lockIfChanged(): their draw paths all test _locked.
// Order matters - lock(true) clears while the widget is still active, then the flag goes.
static void hideWeatherIfChanged(Widget* w, bool hide) {
  if (!w) return;
  // Hiding is a pair of flags, not just the lock: Pager::setPage() re-activates every widget without touching
  // the lock, so a lock-only test reads that resurrected pair as already hidden and returns - leaving the
  // widget locked but active for the next setText() to paint, which is the "Getting Weather..." line with the
  // option off.  Showing stays the lock alone: when showing, _active belongs to the page, and a widget on an
  // inactive page must not be drawn from here.
  if (hide) { if (w->locked() && !w->isActive()) return; }
  else if (!w->locked()) return;
  if (hide) { w->lock(true); w->setActive(false); }
  else      { w->lock(false); w->setActive(true); }   // re-show draws the text setText() kept recording
}
#endif

QueueHandle_t displayQueue;

#ifdef CORE_MONITOR
  volatile uint32_t cmDspLoopCount = 0;
  volatile uint32_t cmGlyphCount    = 0;
  volatile uint32_t cmPreTextCalls  = 0;
  volatile uint32_t cmPreTextHits   = 0;
  volatile uint32_t cmFillCount     = 0;
  volatile uint32_t cmPushCount     = 0;
  volatile uint8_t  cmDspCore       = 255;  // recorded by the task itself; 255 = not yet run
#endif

TaskHandle_t dspTaskHandle = NULL;

static void loopDspTask(void * pvParameters) {
  while(true) {
    #ifndef DUMMYDISPLAY
      if (displayQueue==NULL) break;
      display.loop();
    #endif
    // The task reports its own core, so the Core Monitor can never name the wrong core.
    cmCountDspLoop((uint8_t)xPortGetCoreID());
    vTaskDelay(pdMS_TO_TICKS(DSP_TASK_DELAY));
  }
  vTaskDelete(NULL);
}

void Display::_createDspTask() {
  xTaskCreatePinnedToCore(loopDspTask, "DspTask", (DSP_TASK_STACK_SIZE * 1024), NULL, DSP_TASK_PRIORITY, &dspTaskHandle, DSP_TASK_CORE_ID);
}

#ifndef DUMMYDISPLAY // ============================== DUMMYDISPLAY Below ==============================

DspCore dsp;

Page *pages[] = { new Page(), new Page(), new Page(), new Page() };

static uint32_t normalizeBufferbarValue(uint32_t rawValue, uint32_t maxValue) {
  // Raw audio buffer fill vs the KB threshold where the bar looks "full"
  if (maxValue == 0) return 0;
  return min(rawValue, maxValue);
}

// Every MOVE goes through this, so no call site can forget the `{ }` rule (declared above _switchMode()
// because C++ needs it first).  `{ }` yields to the VU.
//
// There is no "restore" variant any more.  `width = -1` was retired in favour of writing the widget's own
// coordinates (see plans/layout-widget-overlap.md), so a move that means "stay where the conf put you" IS
// a move to those coordinates - which is also what brings the clock back from the screensaver's random
// spot, the job the negative-width branch used to do through moveBack().
static inline bool moveZeroed(const MoveConfig& m) { return m.x == 0 && m.y == 0 && m.width == 0; }
static inline void applyMove(Widget* w, const MoveConfig& m) {
  if (!w || moveZeroed(m)) return;
  w->moveTo(m);
}

// ---- The screensaver's info line: the strip it occupies -------------------------------------------
// The line is a widget no layout provides.  It borrows metaConf's text size and scroll speed, but its
// width is the screen and its place is a strip along the bottom edge: one text row, with a clear gap of
// a hundredth of the panel height (rounded up) both below it and above it, so the clock or the meter
// never sits flush against it and the text never touches the panel edge.
static inline uint16_t ssGap()  { return (uint16_t)((dsp.height() + 99) / 100); }
// The line's text size, from the first layout row that states one: the weather line (the row the strip shares
// its look with), then title1, then the playlist, and metaConf last - meta is mandatory, so the chain resolves
// on any laid-out display.  The bare 1 is only for a caller running before the layout is applied.
static inline uint8_t ssTextSize() {
  const ScrollConfig* chain[] = { weatherConf_ptr, title1Conf_ptr, playlistConf_ptr, metaConf_ptr };
  for (const ScrollConfig* c : chain)
    if (c && c->widget.textsize) return c->widget.textsize;
  return 1;
}
static inline uint16_t ssRowH() { return (uint16_t)(ssTextSize() * CHARHEIGHT); }

// Whether the line would have anything in it.  The station name and the two title lines describe what is
// playing, and over silence there is nothing; the weather stands on its own.  With both gone the strip is
// refused outright, which is also how the clock gets the whole display back.
static bool ssLineHasText() {
  if (player.isRunning()) return true;
  return config.store.showweather && network.weatherBuf && network.weatherBuf[0];
}

uint16_t Display::_ssStripH() {
  // Nothing to show is no strip at all: the clock then wanders over the whole panel rather than around a
  // caption that would be empty.
  if (!config.store.screensaverText || !ssLineHasText()) return 0;
  const uint32_t strip = (uint32_t)ssRowH() + 2u * (uint32_t)ssGap();
  // Refused outright on a panel where the strip would leave the clock or the meter nothing: the line is
  // decoration, and a 32 px OLED has no room for a bottom row at all.
  if ((uint32_t)dsp.height() <= strip + 16u) return 0;
  return (uint16_t)strip;
}

uint16_t Display::_ssContentH() {
  const uint16_t strip = _ssStripH();
  return strip ? (uint16_t)(dsp.height() - strip) : dsp.height();
}

uint16_t Display::_ssTextTop() {
  if (!_ssStripH()) return 0;
  return (uint16_t)(dsp.height() - ssGap() - ssRowH());
}

// Every layout's geometry is built on the 6x8 metric class (CHARWIDTH/CHARHEIGHT), so a system font that
// breaks it mislays the whole screen rather than merely looking wrong - and a font is a runtime setting now,
// so one bad header would reach every panel.  Checked once at boot: the font's yAdvance, and the '0' glyph's
// xAdvance, which is the number the text widgets multiply by.  A log line, not a stop: the screen is still
// the best description of what a wrong font does.
static void validateSystemFonts() {
  for (uint8_t i = 0; i < _systemFontCount; i++) {
    const GFXfont* f = _systemFonts[i];
    if (!f) { SERIALLOG("FONT: id %u is null\n", i); continue; }
    const uint8_t yAdv  = pgm_read_byte(&f->yAdvance);
    const GFXglyph* g   = &f->glyph['0' - pgm_read_byte(&f->first)];
    const uint8_t xAdv  = pgm_read_byte(&g->xAdvance);
    if (yAdv != CHARHEIGHT || xAdv != CHARWIDTH)
      SERIALLOG("FONT: id %u is not on the 6x8 class (xAdvance %u, yAdvance %u; expected %u and %u)\n",
                i, xAdv, yAdv, CHARWIDTH, CHARHEIGHT);
  }
}


void returnPlayer() {
  display.putRequest(NEWMODE, PLAYER);
}

Display::~Display() {
  delete _pager;
  delete _footer;
  delete _plwidget;
  delete _nums;
  delete _clock;
  delete _meta;
  delete _title1;
  delete _title2;
  delete _plcurrent;
}

void Display::init() {
  BOOTLOGX("display.init\t");
  #if LIGHT_SENSOR!=255
    analogSetAttenuation(ADC_0db);
  #endif
  _activeLocale = l10n_findLocale(config.store.locale_display);
  dsp.initDisplay();
  dsp.setFont((GFXfont *)displayFont());
  displayQueue=NULL;
  displayQueue = xQueueCreate(5, sizeof(requestParams_t));
  if (displayQueue==NULL) { ERRORLOG("DISPLAY: displayQueue alloc failed. Rebooting."); delay(10); ESP.restart(); }
  _pager = new Pager();
  _createDspTask();
  while(_bootStep==0) { delay(10); }
  //_pager.begin();
  //_bootScreen()
  _footer = new Page();
  _plwidget = new PlayListWidget();
  _nums = new NumWidget();
  _clock = new ClockWidget();
  _meta = new ScrollWidget();
  _title1 = new ScrollWidget();
  _plcurrent = new ScrollWidget();
  memcpy_P(&activeLayout, &_layouts[config.store.layoutId], sizeof(LayoutData));
  _setLayoutPointers();
  SERIALLOG("done");
}

uint16_t Display::width() { return dsp.width(); }
uint16_t Display::height() { return dsp.height(); }

// Longest boot line we can build: a locale label, a 32 character SSID and the icon pair - and then some.
#define BOOTSTR_LEN 128

void Display::_bootScreen() {
  _boot = new Page();
  // The dots run between a pinned speaker and the boot glyph - see ProgressWidget::_progress().  startup.icon()
  // is read here, while the boot screen is built, and the widget keeps the literal for the session.
  _boot->addWidget(new ProgressWidget(_bootConfig.bootWdtConf, _bootConfig.bootPrgConf, BOOT_PRG_COLOR, 0,
                                      "\023", startup.icon()));
  // A ScrollWidget fed from bootstrConf's plain WidgetConfig: text that fits is drawn at the conf's align
  // (so WA_CENTER still rules), a longer string parks at the edge and scrolls.  Only the scroll fields are derived.
  ScrollConfig bootScroll;
  bootScroll.widget = _bootConfig.bootstrConf;  // left, top, textsize and align straight from the conf
  bootScroll.buffsize = BOOTSTR_LEN;
  bootScroll.uppercase = true;
  bootScroll.width = MAX_WIDTH;
  // The cadence is borrowed from the panel's own message line rather than hardcoded: apSettConf is the same
  // species of line and its step and scrolltime are tuned per panel.  Only these three fields are taken, and
  // textsize 0 is this codebase's "not present" marker, so fall back to a 1px step on the default tick.
  const bool apSettUsable = _bootConfig.apSettConf.widget.textsize > 0;
  bootScroll.startscrolldelay = apSettUsable ? _bootConfig.apSettConf.startscrolldelay : 0;
  bootScroll.scrolldelta      = apSettUsable ? _bootConfig.apSettConf.scrolldelta : 1;
  bootScroll.scrolltime       = apSettUsable ? _bootConfig.apSettConf.scrolltime : SCROLLTIME;
  _bootstring = (TextWidget*) &_boot->addWidget(new ScrollWidget(bootScroll, BOOT_TXT_COLOR, 0));
  _bootstring->setText(RADIOVERSION);
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  dsp.drawLogo(BOOTLOGOTOP);
  _bootStep = 1;
}

// The reference lines are the only truly optional widgets: an empty conf means no widget, so one is made only when a
// conf asks.  under = the line drawn before everything else on the player page, i.e. the one that sits behind the
// text; the over line goes to its own sub-page instead, attached after the footer, so it is drawn last of all.
void Display::_syncLineRule(FillWidget*& w, const FillConfig* conf, bool under) {
  if (!w && conf->height > 0) {
    w = new FillWidget(*conf, config.theme.line);
    if (under) pages[PG_PLAYER]->addWidgetFirst(w);
    else       _overLinePage->addWidget(w);
  }
  if (w) w->init(*conf, config.theme.line);   // a zeroed conf is re-inited too: it clears the old rect
}

void Display::_buildPager() {
  // Made first so the line rules below always have a target.  It is attached to the player page further down, after
  // the footer, which is what puts the over line above everything else that page draws.
  _overLinePage = new Page();
  if (title2Conf_ptr->buffsize > 0) {
    _title2 = new ScrollWidget(*title2Conf_ptr, config.theme.title2, config.theme.background);
  }
  _plbackground = new FillWidget(*playlBGConf_ptr, config.theme.plcurrentfill);
  _metabackground = new FillWidget(*metaBGConf_ptr, config.theme.metafill);
  // These two always exist - their confs are re-pointed behind them (invert, playlist mode) - so a zeroed
  // conf is a state here, not an absence.  The optional lines are made by _syncLineRule() below.
  if (vuConf_ptr->textsize > 0) {
    _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis);
  }
  if (volbarConf_ptr->height > 0) {
    _volbar = new SliderWidget(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
  }
  if (bufferbarConf_ptr->height > 0) {
    _bufferbarMax = 1024 * BUFFERBAR_VISUAL_FULL_KB;
    _bufferbar = new SliderWidget(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax);
  }
  if (voltxtConf_ptr->textsize > 0) {
    _voltxt = new TextWidget(*voltxtConf_ptr, 10, false, config.theme.vol, config.theme.background);
  }
  if (iptxtConf_ptr->textsize > 0) {
    _volip = new TextWidget(*iptxtConf_ptr, 48, false, config.theme.ip, config.theme.background);  // 48 bytes = 15 code points × 3 bytes (CJK) + 2 icons + null
  }
  if (rssiConf_ptr->textsize > 0) {
    _rssi = new TextWidget(*rssiConf_ptr, 20, false, config.theme.rssi, config.theme.background);
  }
  if (batteryConf_ptr->textsize > 0) {
    _battery = new TextWidget(*batteryConf_ptr, 10, false, config.theme.battery, config.theme.background);
  }
  if (weatherConf_ptr->buffsize > 0) {
    _weather = new ScrollWidget(*weatherConf_ptr, config.theme.weather, config.theme.background);
  }

  if (_volbar)   _footer->addWidget(_volbar);
  if (_voltxt)   _footer->addWidget(_voltxt);
  if (_volip)    _footer->addWidget(_volip);
  if (_battery)  _footer->addWidget( _battery);
  if (_rssi)     _footer->addWidget(_rssi);
  if (_bufferbar)  _footer->addWidget(_bufferbar);
  
  if (_metabackground) pages[PG_PLAYER]->addWidget(_metabackground);
  // Made here, early, so it paints beneath the text and the VU; the over line is added to its own page instead,
  // which is attached after the footer - see below.
  _syncLineRule(_underline, underLineConf_ptr, true);
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if (_title2) pages[PG_PLAYER]->addWidget(_title2);
  if (_weather) pages[PG_PLAYER]->addWidget(_weather);
  if (fullbitrateConf_ptr->dimension > 0) {
    _fullbitrate = new BitrateWidget(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_fullbitrate);
  }
  if (bitrateConf_ptr->textsize > 0) {
    _bitrate = new TextWidget(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_bitrate);
  }
  if (_vuwidget) pages[PG_PLAYER]->addWidget(_vuwidget);
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);
  // The footer is a sub-page, so it already paints after the page's own widgets - and sub-pages paint in insertion
  // order, which is what makes this line the last thing the player page draws, the bottom row included.  As a child
  // of _footer it would paint in every page that shares that footer (the dialog does), and as a plain widget of
  // PG_PLAYER it painted before the footer.  Player page only, as a reference line should be.
  pages[PG_PLAYER]->addPage(_overLinePage);
  _syncLineRule(_overline, overLineConf_ptr, false);

  if (_metabackground) pages[PG_DIALOG]->addWidget(_metabackground);
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);
  #ifdef UPDATEURL
    // configure scrolling update label and progress bar
    // copy apSettConf scroll params but center position on dialog page
    ScrollConfig updConf = _bootConfig.apSettConf;
    updConf.widget.left = 0;
    updConf.widget.align = WA_CENTER;
    updConf.widget.top = (dsp.height() - (updConf.widget.textsize * CHARHEIGHT)) / 2;
    updConf.widget.top = max<int16_t>(0, updConf.widget.top - CHARHEIGHT);
    _updLabel = new ScrollWidget(updConf,
                                 config.theme.title1, config.theme.background);
    _updLabel->lock(true);   // don't paint the label's background band unless updating

    // compute bar width once
    {
      uint8_t ts = _bootConfig.apPassConf.textsize > 0 ? _bootConfig.apPassConf.textsize : 1;
      uint16_t widgetPx = dsp.width() - _bootConfig.apPassConf.left;
      int chars = (int)(widgetPx / (CHARWIDTH * ts));
      _updBarWidth = (chars < 2) ? 2 : (chars > 64) ? 64 : chars;
    }

    // place progress widget under the label maintaining original spacing
    WidgetConfig valConf = _bootConfig.apPassConf;
    int16_t origGap = _bootConfig.apPassConf.top - _bootConfig.apNameConf.top;
    if (origGap < 0) origGap = updConf.widget.textsize * CHARHEIGHT + 2; // fallback
    valConf.top = updConf.widget.top + origGap;
    _updValue = new TextWidget(valConf, (uint16_t)(_updBarWidth + 2), false,
                               config.theme.clock, config.theme.background);
    MoveConfig mvValue{0, valConf.top, (int16_t)dsp.width()};
    _updValue->moveTo(mvValue);
    pages[PG_DIALOG]->addWidget(_updLabel);
    pages[PG_DIALOG]->addWidget(_updValue);
  #endif
  
  pages[PG_DIALOG]->addPage(_footer);
  #if !PLAYLIST_MODE_PAGED
    if (_plbackground) {
      pages[PG_PLAYLIST]->addWidget(_plbackground);
      _plbackground->setHeight(_plwidget->itemHeight());
      _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf_ptr->widget.textsize*2), (int16_t)playlBGConf_ptr->width});
    }
    pages[PG_PLAYLIST]->addWidget(_plcurrent);
  #endif
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for(const auto& p: pages) _pager->addPage(p);
  _buildJsonCache();
}

void Display::_apScreen() {
  if (_boot) {
    _pager->removePage(_boot);
    _boot = nullptr;
    _bootstring = nullptr;
  }
    _boot = new Page();
    // The boot screens own their band and ignore invert title - a layout is selectable, they are not.
    // Optional: { } means this panel wants no band, so no widget is made for one.
    if (_bootConfig.apTitleBGConf.height > 0) {
      _boot->addWidget(new FillWidget(_bootConfig.apTitleBGConf, config.theme.metafill));
    }
    uint16_t mfg = config.theme.meta;
    uint16_t mbg = config.theme.metabg;
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apTitleConf, mfg, mbg));
    bootTitle->setText(l10n(L10N_LBL_AP_IMPROV_MODE));
    TextWidget *apname = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apNameConf, 30, false, config.theme.title1, config.theme.background));
    apname->setText(l10n(L10N_LBL_APNAME));
    TextWidget *apname2 = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apName2Conf, 30, false, config.theme.clock, config.theme.background));
    apname2->setText(AP_SSID);
    TextWidget *appass = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apPassConf, 30, false, config.theme.title1, config.theme.background));
    #ifdef AP_PASSWORD
      appass->setText(l10n(L10N_LBL_APPASS));
    #else 
      appass->setText(l10n(L10N_LBL_APNOPASS));
    #endif
    TextWidget *appass2 = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apPass2Conf, 30, false, config.theme.clock, config.theme.background));
    #ifdef AP_PASSWORD
      appass2->setText(AP_PASSWORD);
    #endif
    ScrollWidget *bootSett = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apSettConf, config.theme.title2, config.theme.background));
    bootSett->setText(utility.ipToStr(WiFi.softAPIP()), l10n(L10N_MSG_CONNECT_OPEN));
    _pager->addPage(_boot);
    _pager->setPage(_boot);
}

#ifdef USE_SD
// The SD File Manager's screen: the address to open on one line, and how long the device will keep the mode open
void Display::_sdmanScreen() {
  if (_boot) {
    _pager->removePage(_boot);
    _boot = nullptr;
    _bootstring = nullptr;
  }
  _sdmanCountText = nullptr;
  _boot = new Page();
  // Own band, no invert title, skipped when empty - same rule as the AP screen.
  if (_bootConfig.apTitleBGConf.height > 0) {
    _boot->addWidget(new FillWidget(_bootConfig.apTitleBGConf, config.theme.metafill));
  }
  uint16_t mfg = config.theme.meta;
  uint16_t mbg = config.theme.metabg;
  ScrollWidget *sdTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apTitleConf, mfg, mbg));
  sdTitle->setText(l10n(L10N_LBL_SDMAN));
  ScrollWidget *sdUrl = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apSettConf, config.theme.title2, config.theme.background));
  sdUrl->setText(utility.ipToStr(WiFi.localIP()), l10n(L10N_MSG_OPEN));
  // apName2Conf, not apNameConf: the fontsize-2 title already occupies y=2..18 on a 128x64 and apNameConf's
  // top is 18, so the countdown drew over the title's last row.
  _sdmanCountText = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apName2Conf, 30, false, config.theme.clock, config.theme.background));
  _sdmanCountShown = false;   // a fresh widget holds nothing: the first paint is a transition whatever it shows
  sdmanCountdown();   // paint the first value, so the line is never blank
  _pager->addPage(_boot);
  _pager->setPage(_boot);
}

// Draws only while the manager's page is up, so a late tick costs one comparison
void Display::sdmanCountdown() {
  if (_mode != SDMAN || !_sdmanCountText) return;
  const uint32_t left = filemanager.idleRemainingMs();
  const bool show = (left <= SDMAN_COUNTDOWN_FROM_MS);
  char buf[12];
  if (show) snprintf(buf, sizeof(buf), "%lu:%02lu", (unsigned long)(left / 60000UL), (unsigned long)((left / 1000UL) % 60UL));
  else buf[0] = '\0';
  if (show != _sdmanCountShown) {
    FUNCTIONLOG("SDFileManager", "countdown %s (idle left %lu ms)", show ? "shown" : "blanked", (unsigned long)left);
    _sdmanCountShown = show;
  }
  _sdmanCountText->setText(buf);
  if (!show) _sdmanCountText->repaint();
}
#endif

void Display::_start() {
  if (_boot) {
    _pager->removePage(_boot);
    _boot = nullptr;
    _bootstring = nullptr;
  }
  if (network.status != CONNECTED && network.status != SDOFFLINE) {
    _apScreen();
    _bootStep = 2;
    return;
  }
  // The system font is a stored preference now, so it is chosen before anything resolves layout or text:
  // every widget string is resolved after boot with the font already in place, which is why switching it
  // takes effect here and not live (Stage 6 of plans/font-overhaul.md adds the live walk).  The clock
  // font style is chosen the same way and for the same reason - _buildPager() below is what turns it
  // into widgets, and a widget reads the style once, when it is initialised.
  activeSystemFontId = (config.store.systemFontId < _systemFontCount) ? config.store.systemFontId : 0;
  activeClockFontId  = (config.store.clockFontId  < _clockFontCount)  ? config.store.clockFontId  : 0;
  validateSystemFonts();
  _buildPager();
  _mode = PLAYER;
  _applyState();
  #ifdef USE_SD
    config.setTitle(network.status == SDOFFLINE && !sdman.ready ? l10n(L10N_MSG_NO_SD_CARD) : l10n(L10N_MSG_READY));
  #else
    config.setTitle(l10n(L10N_MSG_READY));
  #endif
  
  if (_bufferbar)  _bufferbar->lock(!bufferbarInLayout() || !config.store.bufferbar);
  
  hideWeatherIfChanged(_weather, _weatherHidden());
  if (_weather && config.store.showweather && network.status != SDOFFLINE) network.buildWeatherString();

  // lockIfChanged, then clear() so an already-inactive clock is erased too.
  if (_clock) {
    lockIfChanged(_clock, _clockHidden());
    if (_clock->locked()) _clock->clear();
  }

  if (_vuwidget) _vuwidget->lock();
  if (_rssi) { if (network.status == SDOFFLINE) _setRSSI(0); else _setRSSI(WiFi.RSSI()); }
  // shareBattRSSI toggles _active to pick between RSSI and battery - a re-show site, hence the predicate.
  if (*shareBattRSSI_ptr && _battery && _rssi) {
    bool haveBattery = battery.isInitialized();
    #ifdef BATTERY_FORCE_DISPLAY
      haveBattery = true;
    #endif
    if (haveBattery) {
      _rssi->setText(""); _rssi->setActive(false);
      _battery->setActive(batteryInLayout()); _updateBattery();
    } else {
      _battery->setText(""); _battery->setActive(false);
      _rssi->setActive(rssiInLayout());
    }
  }
  if (ipInLayout()) {
    if (_volip) {
      if (network.status == SDOFFLINE) {
        _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
      } else {
        if (*shareWeatherIP_ptr && config.store.showweather) _volip->setText("");
        else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
      }
    }
  }
  if (batteryInLayout()) {
    if(_battery) _updateBattery();
  }
  _pager->setPage(pages[PG_PLAYER]);
  _volume();
  _station();
  if (!(network.status == SDOFFLINE && !config.isRTCFound())) _time(false);
  _bootStep = 2;
}

void Display::_showDialog(const char *title) {
  dsp.setScrollId(NULL);
  _pager->setPage(pages[PG_DIALOG]);
  _meta->setAlign(WA_CENTER);
  _meta->setText(title);
}

// The one way a big number goes on screen.  VOL, NUMBERS and SDCHANGE all come through here, so the widget-sharing
// behaviour cannot drift apart again and a layout can be checked from the volume screen alone.

// dialogPage=true switches to the dialog page: the panel is wiped and the number is the only thing on it, with the
// header on the meta line.  false is the volume overlay - the number is drawn over the live player page, where the
// row tidy-up below is what keeps it legible.  A negative value blanks the widget, which is how the card-change
// screen starts before its counter exists (SDFILEINDEX fills it in later). */
void Display::_showNumbers(const char* header, int32_t value, const char* fmt, bool dialogPage) {
  // The shared rows first, in both variants: several layouts put the number on a row the weather, IP, battery or
  // RSSI line normally uses, and those widgets draw themselves on request rather than through the page pass.
  if (*shareWeatherIP_ptr && config.store.showweather && _weather) {
    _weather->lock(true);   // pause weather updates while the number is up, so it cannot overwrite the IP line
    _weather->setText("");
  }
  if (*shareBattRSSI_ptr && _battery && _rssi) {
    _battery->setText(""); _battery->setActive(false);
    _rssi->setActive(rssiInLayout());
  }
  if (dialogPage) _showDialog(header);
  if (_volip) {
    if (network.status == SDOFFLINE) _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
    else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
  }
  if (value < 0) _nums->setText("");
  else _nums->setText(value, fmt);
}

void Display::_setReturnTicker(uint8_t time_s) {
  _returnTicker.detach();
  _returnTicker.once(time_s, returnPlayer);
}

void Display::_switchMode(displayMode_e newmode) {
  if (newmode == CLEAR) { dsp.fillScreen(config.theme.background); _mode = CLEAR; return; }
  if (newmode == VOL && !config.store.volumepage) return;  // no overlay — skip VOL mode to avoid a needless page switch
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDOFFLINE)) return;
  #ifdef USE_SD
    // While the SD card manager owns the screen nothing else may take it:  leave() is what asks for it
    if (filemanager.active() && newmode != SDMAN) return;
  #endif
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER) {
    #ifdef USE_SD
      // Tear down the manager's page - and only that page.  The boot and AP screens build the same _boot
      //  container, and _start() removes theirs; _sdmanCountText is what identifies ours.
      if (_boot && _sdmanCountText) {
        _pager->removePage(_boot);
        _boot = nullptr;
        _bootstring = nullptr;
        _sdmanCountText = nullptr;
      }
    #endif
    if (player.isRunning()){
      if (config.store.vumeter && _vuwidget && vuInLayout()) {
        applyMove(_clock, *clockMove_ptr);
        applyMove(_weather, *weatherMoveVU_ptr);
      } else {
        _clock->moveBack();  // restore from screensaver position
        applyMove(_weather, *weatherMove_ptr);
      }
    } else {
      _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
    numOfNextStation = 0;
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER]);
    // The text goes AFTER the page switch, never before it: setPage() fills the whole panel on its way in,
    // and a scroll widget repaints only from setText() or its own scroll tick - so a line painted first is
    // wiped and stays blank until something else redraws it.  That is the "static line does not come back"
    // half of the reset report; the widgets that repaint from _draw() were never affected.
    _meta->setAlign(metaConf_ptr->widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    _titleTexts();
    // The manager drops the player's state requests while it owns the screen, so widget state is re-derived
    // here, after the page switch, where the draws are wanted again.
    _layoutChange(player.isRunning());
    if (_volip) {
        if (network.status == SDOFFLINE) {
          _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
        } else {
          // weather and IP share the same bottom row; hide IP when weather is active
          if (*shareWeatherIP_ptr && config.store.showweather) _volip->setText("");
          else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
        }
      }
    // force weather repaint on return to PLAYER; larger displays repaint naturally
    if (*shareWeatherIP_ptr && config.store.showweather && _weather) {
      _weather->lock(_weatherHidden());
      // Force a clean repaint of the shared weather/IP row after overlays like VOL/SCREENSAVER.
      _weather->setText("");
      if (network.weatherBuf) _weather->setText(network.weatherBuf);
    }
    if (*shareBattRSSI_ptr && _battery && _rssi) {
      bool haveBattery = battery.isInitialized();
      #ifdef BATTERY_FORCE_DISPLAY
        haveBattery = true;
      #endif
      if (haveBattery) {
        _rssi->setText(""); _rssi->setActive(false);
        _battery->setActive(batteryInLayout()); _updateBattery();
      } else {
        _battery->setText(""); _battery->setActive(false);
        _rssi->setActive(rssiInLayout());
      }
    }
    config.setDspOn(config.store.dspon, false);
    display.putRequest(DBITRATE);  // refresh bitrate badge when returning to player (may have been cleared while on playlist page)
  }
  if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
    config.isScreensaver = true;
    _enterScreensaver(newmode);
  } else {
    config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
    config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
    config.isScreensaver = false;
    // Leaving the screensaver hands the clock back to the player page.  _clockHidden() re-locks it for
    // the states where it should stay away, so unlocking here is what keeps the two consistent.
    if (_clock) _clock->lock(false);
  }
  if (newmode == VOL) {
    _showNumbers(l10n(L10N_LBL_VOLUME), config.store.volume, numtxtFmt, config.store.volumepage);
  }
  if (newmode == LOST)      _showDialog(l10n(L10N_LBL_LOST));
  if (newmode == UPDATING)  { _showDialog(l10n(L10N_LBL_UPDATE));
    #ifdef UPDATEURL
      _updFirstCall = true;
    #endif
  }
  if (newmode == SLEEPING)  _showDialog(l10n(L10N_LBL_SLEEPING));
  if (newmode == SDCHANGE)  _showNumbers(l10n(L10N_LBL_WAITFORSD), -1, "%d", true);  // no count yet: SDFILEINDEX fills it in
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog("");
  if (newmode == NUMBERS)   _showNumbers("", -1, "%d", true);   // the header and the number arrive per digit
  if (newmode == STATIONS) {
    _pager->setPage(pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    #if PLAYLIST_MODE_PAGED
    _plwidget->resetState();
    #endif
    _drawPlaylist();
  }
  #ifdef USE_SD
    // The SD card manager holds the screen while its mode is open
    if (newmode == SDMAN) _sdmanScreen();
  #endif
  
}

// Bring the screensaver up, or rebuild it where it stands.  Factored out of _switchMode() so that a settings
// change can rebuild the picture without a mode change: that path drops a request for the mode the device is
// already in, and drops any request at all while the network is transient, and a rebuild needs neither.
void Display::_enterScreensaver(displayMode_e mode) {
  _screensaverWidgets();        // the widgets are built BEFORE the page switch, so the first pass draws
  _pager->setPage(pages[PG_SCREENSAVER], true);   // the picture the prefs describe, not last time's frame
  if (mode == SCREENBLANK) {
    _clock->clear();
    config.setDspOn(false, false);
  } else {
    config.setDspOn(config.store.dspon, false);   // a rebuild out of a blank must light the panel again
  }
}

void Display::resetQueue() {
  if (displayQueue!=NULL) xQueueReset(displayQueue);
  _deferredType = NOPE;  // a queue flush takes a stale deferred message with it
}

// Queue a request the display task must not apply yet.  delayMs 0 is just putRequest().
void Display::putRequestDelayed(displayRequestType_e type, int payload, uint32_t delayMs) {
  if (delayMs == 0) { putRequest(type, payload); return; }
  _deferredType = type;
  _deferredPayload = payload;
  _deferredDueMs = millis() + delayMs;
}

void Display::_drawPlaylist() {
  if (currentPlItem < 1) currentPlItem = 1;
  //dsp.drawPlaylist(currentPlItem);
  _plwidget->drawPlaylist(currentPlItem);
  _setReturnTicker(30);
}

void Display::_drawNextStationNum(uint16_t num) {
  _setReturnTicker(30);
  _showNumbers(utility.stationByNum(num), (int32_t)num, "%d", true);
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (displayQueue==NULL) return;
  // A later boot-line message supersedes a deferred one, or a scan could overwrite "Wi-fi: <ssid>" mid-delay.
  if (_deferredType != NOPE &&
      (type == BOOTSTRING || type == FORMATTING || type == WAITFORSD || type == SCANNINGWIFI)) {
    _deferredType = NOPE;
  }
  requestParams_t request;
  request.type = type;
  request.payload = payload;
  // Waiting, then dropping, is the deliberate choice for a saturated queue - but silently dropping hides the
  // cause behind whatever ran last.  A full reset queued six requests into a five-deep queue and looked like a
  // battery problem; this line is what tells that story next time (see plans/display-repaint-order.md).
  if (xQueueSend(displayQueue, &request, pdMS_TO_TICKS(DSQ_SEND_DELAY)) != pdTRUE) {
    _droppedRequests++;
    ERRORLOG("DISPLAY: request %d dropped, queue full (%u so far)", (int)type, (unsigned)_droppedRequests);
  }
}

void Display::updateProgress(const char* label, float progress) {
  #ifdef UPDATEURL
    if (_updFirstCall) {
      _updFirstCall = false;
      delay(50); // allow display task to process NEWMODE/UPDATING queue item before drawing
    }
    if (_updLabel) {
      _updLabel->setText(label);
    }
    if (_updValue) {
      int bars = (int)(progress * _updBarWidth + 0.5f);
      if (bars < 0) bars = 0;
      if (bars > _updBarWidth) bars = _updBarWidth;
      const char barChar = '\016'; // play icon is progress
      char buf[68];
      memset(buf, barChar, bars);
      memset(buf + bars, ' ', _updBarWidth - bars); // empty space is empty space
      buf[_updBarWidth] = '\0';
      _updValue->setText(buf);
    }
  #endif
}

// Screens the display owns rather than borrows.  Both are holding patterns, not player pages: the self-drawing
// widget requests are dropped while one is up (drawsOverOwnScreen) and the clock and weather are hidden, because
// nothing repaints them afterwards and their page no longer ticks.
bool Display::_ownScreen() const {
  if (_mode == SDCHANGE || _mode == NUMBERS) return true;
  if (_mode == VOL && config.store.volumepage) return true;
  #ifdef USE_SD
    return filemanager.active();
  #endif
  return false;
}

// The clock hides with no time source, when the layout omits it, when it yields to the VU, and while a screen the
// display owns is up.  The clock is only advanced in PLAYER and SCREENSAVER, so one left in a holding pattern's
// layout is painted once and then sits frozen - which is what the manager exposed (the player stop that mode causes
// re-un-hid it) and what the card-change wait screen shows when the index is valid and no counter ever appears.
// _layoutChange() redraws the full clock when the mode leaves.  While the panel is the screensaver's, the answer is
// _screensaverWidgets()'s to give: it is the place that locks the clock for the meter, and a settings change from
// the WebUI must not unlock one that was put away.
bool Display::_clockHidden() {
  if (config.isScreensaver) return _ssMeterUp;
  const bool noTimeSource = (network.status == SDOFFLINE && !config.isRTCFound());
  const bool yieldsToVU   = (config.store.vumeter && vuInLayout() && player.isRunning() && moveZeroed(*clockMove_ptr));
  if (_ownScreen()) return true;
  return noTimeSource || yieldsToVU || !clockInLayout();
}

// Same shape for the weather, plus shared-row suppression during the volume overlay.  The widget belongs to the
// player page alone, so while the screensaver owns the panel it is never drawn: the screensaver's own line is what
// carries the weather there (see plans/screensaver-panel-ownership.md).
bool Display::_weatherHidden() {
  if (config.isScreensaver) return true;
  const bool featureOff = !config.store.showweather;
  const bool yieldsToVU = (config.store.vumeter && vuInLayout() && player.isRunning() && moveZeroed(*weatherMoveVU_ptr));
  bool volOverlay = false;
  volOverlay = *shareWeatherIP_ptr && (_mode == VOL);
  if (_ownScreen()) return true;
  return featureOff || yieldsToVU || volOverlay || !weatherInLayout();
}

void Display::_layoutChange(bool played) {
  if (config.store.vumeter && _vuwidget && vuInLayout()) {
    if (played) {
      if (_vuwidget) _vuwidget->unlock();
      applyMove(_clock, *clockMove_ptr);
      applyMove(_weather, *weatherMoveVU_ptr);
    } else {
      if (_vuwidget) if (!_vuwidget->locked()) _vuwidget->lock();
      _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
  } else {
    if (played) {
      _clock->moveBack();  // restore clock from VU-shifted position
      applyMove(_weather, *weatherMove_ptr);
      //_clock->moveBack();
    } else {
      if (_weather) _weather->moveBack();
      _clock->moveBack();
    }
  }
  // Lock state last, from one definition.  lock() erases for every class that implements _clear() - the
  // clock, the weather, the VU and the bars - so a yielded one of those really disappears.  A FillWidget
  // and a plain TextWidget override neither _clear() nor _reset(), so for them lock() only hides: their
  // pixels stay until something else clears them (see plans/layout-widget-overlap.md).
  const bool clockWasHidden = (_clock && _clock->locked());
  lockIfChanged(_clock, _clockHidden());
  // The weather re-shows through hideWeatherIfChanged() itself, so it needs no redrawIfVisible() here.
  hideWeatherIfChanged(_weather, _weatherHidden());
  if (clockWasHidden)   redrawIfVisible(_clock);     // full _printClock(true), not just the seconds
}

#ifdef USE_SD
// Requests that draw a player-page widget directly, which a page switch cannot stop: a page knows only its own
// widgets, so nothing tells the clock or the footer that they are no longer on screen.  Dropped while a screen the
// display owns is up (_ownScreen): the manager, and for the same reason the card-change wait screen, which is also
// a holding pattern where the count is the only thing that should change.  Arriving late matters here - the title,
// the IP line, the RSSI and battery icons and the VU all have their own refresh paths, and none of them is gated on
// the mode.  _switchMode(PLAYER) re-derives the state from the live player on the way out.
bool Display::_drawsOverOwnScreen(displayRequestType_e type) const {
  if (type == DRAWVOL) return !(_mode == VOL || _mode == NUMBERS);
  switch (type) {
    case PSTART: case PSTOP: case SHOWVUMETER: case SHOWWEATHER: case NEWWEATHER:
    case NEWTITLE: case NEWSTATION: case SHOWBUFFERBAR:
    case DSPRSSI: case DSPBATTERY: case NEWIP:
      return true;
    default:
      return false;
  }
}
#endif

void Display::loop() {
  if (_bootStep==0) {
    _pager->begin();
    _bootScreen();
    return;
  }
  if (_bootStep==2) {
    // Inversion applies only outside the screensaver; the screensaver is always un-inverted.
    bool shouldInvert = config.isScreensaver ? false : config.store.invertdisplay;
    if (shouldInvert != config.displayIsInverted) {
      config.displayIsInverted = shouldInvert;
      display.invert();
    }
  }
  // Fire a deferred request once its delay has elapsed, through this queue so it takes the identical path; if the
  // queue is momentarily full the pending slot is kept for the next pass.
  if (_deferredType != NOPE && (int32_t)(millis() - _deferredDueMs) >= 0 && displayQueue != NULL && !_locked) {
    requestParams_t deferred;
    deferred.type = _deferredType;
    deferred.payload = _deferredPayload;
    if (xQueueSend(displayQueue, &deferred, 0) == pdTRUE) _deferredType = NOPE;
  }
  if (displayQueue==NULL || _locked) return;
  _pager->loop();
  requestParams_t request;
  if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)) {
    #ifdef USE_SD
      // One pass without dsp.loop(), like the early return further down this switch.
      if (_ownScreen() && _drawsOverOwnScreen(request.type)) return;
    #endif
    switch (request.type) {
        case NEWMODE: _switchMode((displayMode_e)request.payload); break;
        // A screensaver setting changed while the screensaver is on screen.  Deliberately not a mode change,
        // so it does not go through _switchMode() and its two guards: the widgets are re-derived from the
        // prefs and the page is repainted where it stands.
        case SSREBUILD: {
          const displayMode_e m = (displayMode_e)request.payload;
          if (_mode == SCREENSAVER || _mode == SCREENBLANK) { _mode = m; _enterScreensaver(m); }
          break;
        }
        // The whole layout/theme/font re-init, on the display task.  applyFont() below is the only
        // thing that queues it; it used to be an enum value with no handler at all.
        case APPLYSTATE: _applyState(); break;
        case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, request.payload});
        case CLOCK:
          if ((_mode==PLAYER || _mode==SCREENSAVER) && !(network.status == SDOFFLINE && !config.isRTCFound()))
            _time(request.payload);
          break;
        case NEWTITLE: _title(); break;
        case NEWSTATION: _station(); break;
        case NEXTSTATION: _drawNextStationNum(request.payload); break;
        case DRAWPLAYLIST: _drawPlaylist(); break;
        case DRAWVOL: _volume(); break;
        case DBITRATE: {
            if (_mode != PLAYER) break;  // skip draws when player page isn't visible (e.g., SD file list)
            char buf[20];
            snprintf(buf, 20, bitrateFmt, config.station.bitrate);
            if (_bitrate) { _bitrate->setText(config.station.bitrate==0?"":buf); }
            if (_fullbitrate) {
              _fullbitrate->setBitrate(config.station.bitrate);
              _fullbitrate->setFormat(config.configFmt);  // UNKNOWN clears badge via _draw() → _clear()
            }
          }
          break;
        case SHOWBUFFERBAR: if (_bufferbar)  {
            _bufferbar->lock(!bufferbarInLayout() || !config.store.bufferbar);
            _bufferbar->setValue(normalizeBufferbarValue(player.inBufferFilled(), _bufferbarMax));
          }
          break;
        case SHOWVUMETER: {
          if (_vuwidget) {
            _vuwidget->lock(!vuInLayout() || !config.store.vumeter);
            _layoutChange(player.isRunning());
          }
          break;
        }
        case SHOWWEATHER: {
          hideWeatherIfChanged(_weather, _weatherHidden());
          if (!config.store.showweather) {
            if (_weather) _weather->setText("");
            if (_volip) _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
          } else {
            // weather and IP share a row; suppress weather text and IP together based on mode
            if (*shareWeatherIP_ptr) {
              if (_mode == VOL) {
                if (_weather) _weather->setText("");
              } else {
                if (_volip) _volip->setText("");
                network.buildWeatherString();
              }
            } else { // larger displays have separate rows; just update weather, leave IP alone
              network.buildWeatherString();
            }
          }
          // The strip is the weather's only home there, and its existence follows the flag - with no page switch
          // behind a checkbox.  A strip that stays just needs the new text, which nothing else delivers here.
          if (config.isScreensaver) {
            if ((_sstext != nullptr) != (_ssStripH() != 0)) putRequest(SSREBUILD, _mode);
            else                                           _screensaverLine();
          }
          break;
        }
        case NEWWEATHER: {
          // skip weather repaint during VOL to avoid overwriting the IP shown there
          if ((!*shareWeatherIP_ptr || _mode != VOL) && _weather && network.weatherBuf)
            _weather->setText(network.weatherBuf);
          // A weather refresh is one of the things the screensaver's line is built from, and it arrives on
          // its own schedule - which is why that line is re-built rather than set once.  If the screensaver came
          // up with no strip to re-build - nothing playing and no weather cached - this refresh is what makes
          // _ssStripH() true, and only a rebuild can create the widget.
          if (config.isScreensaver) {
            if (_sstext) _screensaverLine();
            else if (_ssStripH()) putRequest(SSREBUILD, _mode);
          }
          break;
        }
        case BOOTSTRING: {
          if (_bootstring) _bootstring->setText(config.ssids[request.payload].ssid, l10n(L10N_MSG_WIFI));
          break;
        }
        case WAITFORSD: {
          if (_bootstring) _bootstring->setText(l10n(L10N_LBL_WAITFORSD));
          break;
        }
        // Same shape as WAITFORSD: a fixed boot-line message, asked for by whoever does the slow work.
        case FORMATTING: {
          if (_bootstring) _bootstring->setText(l10n(L10N_MSG_FORMATTING));
          break;
        }
        case SCANNINGWIFI: {
          if (_bootstring) _bootstring->setText(l10n(L10N_MSG_SCANNING_WIFI));
          break;
        }
        case SDFILEINDEX: {
          if (_mode == SDCHANGE) _nums->setText(request.payload, "%d");
          break;
        }
        case DSPRSSI:
          if (_rssi) { _setRSSI(request.payload); }
          if (_bufferbar && config.store.bufferbar) {
            _bufferbar->setValue(normalizeBufferbarValue(player.isRunning() ? player.inBufferFilled() : 0, _bufferbarMax));
          }
          break;
        case DSPBATTERY: {
          if(_battery) _updateBattery();
          break;
        }
        case PSTART: _layoutChange(true);   break;
        case PSTOP:  _layoutChange(false);  break;
        case DSP_START: _start();  break;
        case NEWIP: {
          if (_volip) {
              if (network.status == SDOFFLINE) {
                _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
              } else {
                // skip IP repaint in PLAYER when weather owns the shared row
                if (!*shareWeatherIP_ptr || !(_mode == PLAYER && config.store.showweather))
                  _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
              }
            }
          break;
        }
        default: break;

        // check if there are more messages waiting in the queue, in this case break the loop() and go
        // for another round to evict next message, do not waste time to redraw the screen, etc...
        if (uxQueueMessagesWaiting(displayQueue))
          return;
      }
  }

  dsp.loop();
/*
  #if defined(USE_AUDIO_VS1053)
  player.computeVUlevel();
  #endif
*/
}

void Display::_setRSSI(int rssi) {
  #if SD_CS!=255
    if (network.status == SDOFFLINE) {
      _rssi->setText(config.store.sdshuffle ? "\032\033" : "  ");
      return;
    }
  #endif
  if (!_rssi) return;
  if (*rssiDigit_ptr) {
    _rssi->setText(rssi, rssiFmt);
    return;
  }
  // Level 0-4 comes from RSSI_STEPS in one place (utility.cpp); these are the same four steps the WebUI bars show.
  static const char* const rssiGlyphs[5] = { "\001\002", "\003\002", "\004\002", "\004\005", "\004\006" };
  _rssi->setText(rssiGlyphs[rssiLevel(rssi)]);
}

void Display::_updateBattery() {
  if(!_battery) return;

  #ifdef BATTERY_FORCE_DISPLAY // force it to display (fake it)
    int pct = BATTERY_FORCE_DISPLAY;
    if (pct > 100) pct = 100;
  #else
    BatteryStatus bat = battery.getStatus();
    if(!bat.present && battery.isInitialized()) {
      battery.recalcNow();
      bat = battery.getStatus();
    }
    if(!battery.isInitialized() || !bat.present) {
      _battery->setText("");
      return;
    }
    int pct = bat.percentage;
  #endif

  // 2-glyph 4-segment battery, RSSI-style
  static const char leftGlyphs[3]  = { '\013', '\015', '\016' }; // 0,1,2 segs
  static const char rightGlyphs[3] = { '\014', '\017', '\020' }; // 0,1,2 segs

  int segs = (pct + 12) / 25;  // 0-4  (0-12,13-37,38-62,63-87,88-100)
  if (segs > 4) segs = 4;
  int left  = (segs > 2) ? 2 : segs;
  int right = (segs > 2) ? (segs - 2) : 0;

  char buf[16];
  buf[0] = leftGlyphs[left];
  buf[1] = rightGlyphs[right];
  buf[2] = '\0';

  if (batterytxtFmt[0] != '\0') {
    strlcat(buf, " ", sizeof(buf));  // space before number
    char numbuf[8];
    snprintf(numbuf, sizeof(numbuf), batterytxtFmt, pct);
    strlcat(buf, numbuf, sizeof(buf));
  }

  _battery->setText(buf);
}

void Display::_station() {
  _meta->setAlign(metaConf_ptr->widget.align);
  _meta->setText(config.station.name);
}

char *split(char *str, const char *delim) {
  char *dmp = strstr(str, delim);
  if (dmp == NULL) return NULL;
  *dmp = '\0';
  return dmp + strlen(delim);
}

// config.station.title arrives as "artist - song" and the layout has two title lines for it.  Both the
// player page and the screensaver's info line want the two halves, so the rule lives here rather than in
// either caller.  False when there is no title at all; otherwise `first` is the whole string with
// `second` null unless the separator was found.  Both point into `dst`.
static bool splitTitle(char* dst, size_t dstSize, char*& first, char*& second) {
  first = second = nullptr;
  if (!config.station.title[0]) return false;
  strlcpy(dst, config.station.title, dstSize);
  second = split(dst, " - ");
  first = dst;
  return true;
}

void Display::_titleTexts() {
  char titlebuf[STATION_FIELD_LENGTH];
  char *t1 = nullptr, *t2 = nullptr;
  if (splitTitle(titlebuf, sizeof(titlebuf), t1, t2) && t2 && _title2) {
    _title1->setText(t1);
    _title2->setText(t2);
  } else {
    _title1->setText(config.station.title);
    if (_title2) _title2->setText("");
  }
}

void Display::_title() {
  _titleTexts();
  // The screensaver's own line carries the same text on a different page, so nothing else would refresh
  // it when a song changes while the device is asleep.
  if (config.isScreensaver) _screensaverLine();
  rgbled.trackChange();
  backlightControls.restart();
}

// The info line's text: the station name, the two title lines and the weather, joined with the shared scroll mark
// (scrollMark(): " * ", or the holiday icon).  The
// first three describe what is playing, so over silence they are skipped and only the weather is left -
// which is the same rule _ssStripH() refuses the whole strip on.  Every empty part is skipped outright
// rather than left as a bare separator, which is why they all go through one append.
void Display::_screensaverLine() {
  if (!_sstext) return;
  // Two ways the line must stay off: the settings refuse the strip, and the widget was locked for a reason of its
  // own.  A locked widget is still _active and setText() paints on _active alone, so a new metadata string
  // repainted the line while the strip was refused - _ssStripH() is the authoritative test, the lock the backstop.
  if (!_ssStripH() || _sstext->locked()) return;
  // One buffer for the display task's own use rather than a heap copy per refresh: the worst case is the
  // station name and the title (two station fields) plus the 512-byte weather string, plus three marks of up to
  // four bytes each.
  static char line[STATION_FIELD_LENGTH * 2 + WEATHER_STRING_L + 16];
  line[0] = '\0';
  auto append = [](char* dst, size_t room, const char* part) {
    if (!part || !part[0]) return;
    if (dst[0]) strlcat(dst, scrollMark(), room);
    strlcat(dst, part, room);
  };
  if (player.isRunning()) {
    char titlebuf[STATION_FIELD_LENGTH];
    char *t1 = nullptr, *t2 = nullptr;
    splitTitle(titlebuf, sizeof(titlebuf), t1, t2);
    append(line, sizeof(line), config.station.name);
    append(line, sizeof(line), t1);
    append(line, sizeof(line), t2);
  }
  if (config.store.showweather && network.weatherBuf) append(line, sizeof(line), network.weatherBuf);
  _sstext->setText(line);
}

// Bring both screensaver widgets in line with the prefs and the active layout.  Called on every entry
// into SCREENSAVER rather than once at boot: the meter's segmentation comes from bandsConf, its palette
// from the theme and the line's size from metaConf, and the WebUI stays reachable while the device is
// asleep, so a layout or theme switch has to land.  A layout with no VU box of its own is fine here -
// the screensaver's meter is this widget, not the player page's.
void Display::_screensaverWidgets() {
  // --- the info line ---
  if (_ssStripH()) {
    ScrollConfig conf = *metaConf_ptr;      // the scroll speed is the meta line's; the size is the weather conf's
    conf.widget.left = 0;
    conf.widget.top = _ssTextTop();
    conf.widget.textsize = ssTextSize();
    conf.width = dsp.width();
    // The mark is the widget's own now, so what a wrapping line shows reads exactly like the joins inside the
    // screensaver line above.
    // Black, never config.theme.background: the screensaver paints its own picture and its background is
    // always black - the clock does the same while isScreensaver.  With the theme's colour the line laid a
    // coloured band over the black the rest of the panel shows.
    if (!_sstext) {
      _sstext = new ScrollWidget(conf, config.theme.textss, 0);
      pages[PG_SCREENSAVER]->addWidget(_sstext);
    } else {
      _sstext->init(conf, config.theme.textss, 0);
    }
    _sstext->lock(false);
    _screensaverLine();
  } else if (_sstext) {
    _sstext->setText("");
    _sstext->lock(true);        // ScrollWidget::_clear() erases the window it owns
  }

  // --- the meter, or the clock ---
  // No playback, no meter: with nothing to read it is a still picture that still costs the panel 4-40 ms a
  // frame, so the clock keeps it.  Decided here, on entry, like the strip above - and recorded, because it is
  // the only answer to "does the clock show" while the panel is the screensaver's.
  _ssMeterUp = false;
  if (!config.store.screensaverVU || !player.isRunning()) {
    if (_clock) _clock->lock(false);
    if (_ssvu) _ssvu->lock(true);
    return;
  }
  if (!_ssvu) {
    _ssvu = new VuWidget();
    pages[PG_SCREENSAVER]->addWidget(_ssvu);
  }
  const uint16_t W = dsp.width();
  const uint16_t CH = _ssContentH();
  uint8_t style = config.store.screensaverVUStyle;
  if (style >= (uint8_t)VU_STYLE_COUNT) style = VU_STYLE_BARS;   // a stale stored id
  // How big the meter may be and where it sits is the widget's business, not the screen's: the box comes
  // back sized for the style - the bar family's two rows, the Lissajous's square, the budget rectangle -
  // and already centred in the content area.  See VuWidget::resolveScreensaverBox().
  const VuBox box = VuWidget::resolveScreensaverBox(W, CH, style);
  if (!_ssvu->initScreensaver(box, *bandsConf_ptr, style)) {
    if (!_ssFailed) {
      _ssFailed = true;
      ERRORLOG("Screensaver: no PSRAM for the VU canvas (%ux%u) - keeping the clock screensaver",
               (unsigned)box.w, (unsigned)box.h);
    }
    if (_clock) _clock->lock(false);
    return;
  }
  _ssMeterUp = true;
  if (_clock) _clock->lock(true);
}

void Display::_time(bool redraw) {
  
  #if LIGHT_SENSOR!=255
    if (config.store.dspon) {
      config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
      config.setBrightness();
    }
  #endif
  // The clock only walks while it IS the screensaver, and "is it" is _ssMeterUp - the recorded answer to "does
  // the meter own the panel", which is also what _clockHidden() returns while asleep.  Asking the option
  // instead meant VU-on with nothing playing showed the clock but left it parked, while every other route to
  // the clock screensaver walked.  With the meter genuinely up the clock is locked, so its walk would be
  // wasted work even so.
  if (config.isScreensaver && !_ssMeterUp && network.timeinfo.tm_sec % SCREENSAVERMOVE == 0) {
    int32_t clockH = _clock->clockHeight();
    int32_t minTop = max((int32_t)TFT_FRAMEWDT, (int32_t)_clock->timeHeight());
    // The floor is the content area rather than the panel: the info line's strip is not the clock's to
    // cross, which is the whole reason the two bounds are derived from the same number.
    int32_t maxTop = (int32_t)_ssContentH() - clockH - TFT_FRAMEWDT;
    uint16_t ft = (maxTop > minTop) ? static_cast<uint16_t>(random(minTop, maxTop + 1)) : static_cast<uint16_t>(minTop);

    int32_t minLeft = TFT_FRAMEWDT;
    int32_t maxLeft = dsp.width() - _clock->clockWidth() - TFT_FRAMEWDT;
    int32_t left = (maxLeft > minLeft) ? random(minLeft, maxLeft + 1) : minLeft;
    if (clockConf_ptr->align == WA_CENTER) left -= (dsp.width() - _clock->clockWidth()) / 2;
    if (left < 0) left = 0;
    uint16_t lt = static_cast<uint16_t>(left);
    //_clock->moveTo({clockConf_ptr->left, ft, 0});
    _clock->moveTo({lt, ft, 0});
  }
  if (redraw) _clock->forceDraw(); else _clock->draw();
}

void Display::_updateVolume() {
  if(!_voltxt) return;

  uint8_t vol = config.store.volume;

  // 2-glyph: speaker + volume waves
  // \023 = speaker (always)
  // Volume 0 → space (no waves). >0 → 1-4 waves via \024-\027.
  static const char waveGlyphs[4] = { '\024', '\025', '\026', '\027' };

  char buf[16];
  buf[0] = '\023';             // speaker
  if (vol == 0) {
    buf[1] = ' ';              // silence: no waves
  } else {
    int level = (vol * 4) / (VOLUME_SCALE + 1);  // 0-3
    if (level > 3) level = 3;
    buf[1] = waveGlyphs[level];
  }
  buf[2] = '\0';

  if (voltxtFmt[0] != '\0') {
    strlcat(buf, "\x1E", sizeof(buf));  // 2-pixel spacer before number
    char numbuf[8];
    snprintf(numbuf, sizeof(numbuf), voltxtFmt, vol);
    strlcat(buf, numbuf, sizeof(buf));
  }

  _voltxt->setText(buf);
}

void Display::_volume() {
  if (_volbar) _volbar->setValue(config.store.volume);
  _updateVolume();
  if (_mode==VOL) {
    _setReturnTicker(3);
    _nums->setText(config.store.volume, numtxtFmt);
  }
}

void Display::flip() { dsp.flip(); }

// Re-init all widgets with current pointer values (called after layout switch)
void Display::_reinitWidgets() {
  {
    uint16_t mfg = config.store.inverttitle ? config.theme.metabg : config.theme.meta;
    uint16_t mbg;
    #ifdef DSP_TFT
      mbg = config.store.inverttitle ? config.theme.background : config.theme.metabg;
    #else
      mbg = config.store.inverttitle ? config.theme.metafill : config.theme.metabg;
    #endif
    _meta->init(*metaConf_ptr, mfg, mbg);
  }
  // Title1 is optional like the rest: nothing else depends on it, and it has no other lock site.
  if (title1InLayout()) { _title1->init(*title1Conf_ptr, config.theme.title1, config.theme.background); showByLayout(_title1); }
  else hideByLayout(_title1);
  // Safe on a never-initialised clock: ClockWidget bails on !_present and guards every _fb deref.
  if (clockInLayout()) { _clock->init(*clockConf_ptr, 0, 0); showByLayout(_clock); }
  else hideByLayout(_clock);
  _plcurrent->init(*playlistConf_ptr, config.theme.plcurrent, config.theme.plcurrentbg);
  _plwidget->init(_plcurrent);
  _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf_ptr->width});
  // --- Player-page optional widgets (lazy-create if newly enabled) ---
  if (title2InLayout()) {
    if (!_title2) {
      _title2 = new ScrollWidget(*title2Conf_ptr, config.theme.title2, config.theme.background);
      pages[PG_PLAYER]->addWidget(_title2);
    } else {
      _title2->init(*title2Conf_ptr, config.theme.title2, config.theme.background);
      showByLayout(_title2);
    }
  } else hideByLayout(_title2);
  if (vuInLayout()) {
    if (!_vuwidget) {
      _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis);
      pages[PG_PLAYER]->addWidget(_vuwidget);
    } else {
      _vuwidget->init(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis);
      showByLayout(_vuwidget);
    }
  } else hideByLayout(_vuwidget);
  if (weatherInLayout()) {
    if (!_weather) {
      _weather = new ScrollWidget(*weatherConf_ptr, config.theme.weather, config.theme.background);
      pages[PG_PLAYER]->addWidget(_weather);
    } else {
      _weather->init(*weatherConf_ptr, config.theme.weather, config.theme.background);
      showByLayout(_weather);
    }
  } else hideByLayout(_weather);
  // Two independent widgets, not alternatives: the layout may ask for the bitrate text, the codec badge, both
  // or neither, and the one it does not ask for is hidden, never freed.  Nothing is deleted here on purpose -
  // the page owns the widgets it holds and DspTask may be inside one - and this pair is what used to reboot the
  // board: Page::removeWidget() already deletes, and the removed widget was deleted a second time on top.
  if (fullbitrateInLayout()) {
    if (!_fullbitrate) {
      _fullbitrate = new BitrateWidget(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
      pages[PG_PLAYER]->addWidget(_fullbitrate);
    } else {
      _fullbitrate->init(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
      showByLayout(_fullbitrate);
    }
  } else hideByLayout(_fullbitrate);
  if (bitrateInLayout()) {
    if (!_bitrate) {
      _bitrate = new TextWidget(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
      pages[PG_PLAYER]->addWidget(_bitrate);
    } else {
      _bitrate->init(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
      showByLayout(_bitrate);
    }
  } else hideByLayout(_bitrate);

  // --- Footer widgets (lazy-create if newly enabled) ---
  if (volbarInLayout()) {
    if (!_volbar) {
      _volbar = new SliderWidget(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
      _footer->addWidget(_volbar);
    } else {
      _volbar->init(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
      showByLayout(_volbar);
    }
  } else hideByLayout(_volbar);
  if (bufferbarInLayout()) {
    _bufferbarMax = 1024 * BUFFERBAR_VISUAL_FULL_KB;
    if (!_bufferbar) {
      _bufferbar = new SliderWidget(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax);
      _footer->addWidget(_bufferbar);
    } else {
      _bufferbar->init(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax);
      showByLayout(_bufferbar);
    }
  } else hideByLayout(_bufferbar);
  if (voltxtInLayout()) {
    if (!_voltxt) {
      _voltxt = new TextWidget(*voltxtConf_ptr, 10, false, config.theme.vol, config.theme.background);
      _footer->addWidget(_voltxt);
    } else {
      _voltxt->init(*voltxtConf_ptr, 10, false, config.theme.vol, config.theme.background);
      showByLayout(_voltxt);
    }
  } else hideByLayout(_voltxt);
  if (ipInLayout()) {
    if (!_volip) {
      _volip = new TextWidget(*iptxtConf_ptr, 48, false, config.theme.ip, config.theme.background);
      _footer->addWidget(_volip);
    } else {
      _volip->init(*iptxtConf_ptr, 48, false, config.theme.ip, config.theme.background);
      showByLayout(_volip);
    }
  } else hideByLayout(_volip);
  if (rssiInLayout()) {
    if (!_rssi) {
      _rssi = new TextWidget(*rssiConf_ptr, 20, false, config.theme.rssi, config.theme.background);
      _footer->addWidget(_rssi);
    } else {
      _rssi->init(*rssiConf_ptr, 20, false, config.theme.rssi, config.theme.background);
      showByLayout(_rssi);
    }
  } else hideByLayout(_rssi);
  if (batteryInLayout()) {
    if (!_battery) {
      _battery = new TextWidget(*batteryConf_ptr, 10, false, config.theme.battery, config.theme.background);
      _footer->addWidget(_battery);
    } else {
      _battery->init(*batteryConf_ptr, 10, false, config.theme.battery, config.theme.background);
      showByLayout(_battery);
    }
  } else hideByLayout(_battery);
  // Conditional on the zeroed-conf rule, like the clock
  if (numInLayout()) { _nums->init(*numConf_ptr, 10, false, config.theme.digit, config.theme.background); showByLayout(_nums); }
  else hideByLayout(_nums);
  // Background fills
  if (_plbackground) _plbackground->init(*playlBGConf_ptr, config.theme.plcurrentfill);
  if (_metabackground) _metabackground->init(*metaBGConf_ptr, config.theme.metafill);
  _syncLineRule(_underline, underLineConf_ptr, true);
  _syncLineRule(_overline, overLineConf_ptr, false);
  #if !PLAYLIST_MODE_PAGED
    if (_plbackground) {
      _plbackground->setHeight(_plwidget->itemHeight());
      _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf_ptr->widget.textsize*2), (int16_t)playlBGConf_ptr->width});
    }
  #endif
}

void Display::_setLayoutPointers() {
  metaConf_ptr       = &activeLayout.metaConf;
  title1Conf_ptr     = &activeLayout.title1Conf;
  title2Conf_ptr     = &activeLayout.title2Conf;
  playlistConf_ptr   = &activeLayout.playlistConf;
  weatherConf_ptr    = &activeLayout.weatherConf;
  metaBGConf_ptr     = &activeLayout.metaBGConf;
  metaBGConfInv_ptr  = &activeLayout.metaBGConfInv;
  volbarConf_ptr     = &activeLayout.volbarConf;
  playlBGConf_ptr    = &activeLayout.playlBGConf;
  bufferbarConf_ptr  = &activeLayout.bufferbarConf;
  bitrateConf_ptr    = &activeLayout.bitrateConf;
  voltxtConf_ptr     = &activeLayout.voltxtConf;
  batteryConf_ptr    = &activeLayout.batteryConf;
  iptxtConf_ptr      = &activeLayout.iptxtConf;
  rssiConf_ptr       = &activeLayout.rssiConf;
  numConf_ptr        = &activeLayout.numConf;
  clockConf_ptr      = &activeLayout.clockConf;
  vuConf_ptr         = &activeLayout.vuConf;
  fullbitrateConf_ptr= &activeLayout.fullbitrateConf;
  bandsConf_ptr      = &activeLayout.bandsConf;
  clockMove_ptr      = &activeLayout.clockMove;
  weatherMove_ptr    = &activeLayout.weatherMove;
  weatherMoveVU_ptr  = &activeLayout.weatherMoveVU;
  underLineConf_ptr  = &activeLayout.underLineConf;
  overLineConf_ptr   = &activeLayout.overLineConf;
}

void Display::_applyMetaInvert() {
  if (config.store.inverttitle) {
    #ifdef DSP_TFT
      config.theme.metafill = config.theme.div;
    #endif
    // No fallback: an empty metaBGConfInv is the layout declining a bar when inverted.
    metaBGConf_ptr = &activeLayout.metaBGConfInv;
  } else {
    metaBGConf_ptr = &activeLayout.metaBGConf;
  }
}

void Display::_applyState() {
  memcpy_P(&activeLayout, &_layouts[config.store.layoutId], sizeof(LayoutData));
  _setLayoutPointers();
  #ifdef DSP_TFT
    memcpy_P(&config.theme, &_themes[config.store.themeId], sizeof(ThemeData));
  #endif
  _applyMetaInvert();
  _reinitWidgets();
  // Re-apply every feature lock: _reinitWidgets() may just have shown a widget the layout brought back.
  if (_vuwidget)  _vuwidget->lock(!vuInLayout() || !config.store.vumeter || !player.isRunning());
  // Mirrors SHOWWEATHER exactly, including the shared-row suppression, so a switch cannot drop it mid-overlay.
  hideWeatherIfChanged(_weather, _weatherHidden());
  if (_bufferbar) _bufferbar->lock(!bufferbarInLayout() || !config.store.bufferbar);
  // The clock's feature lock must be re-applied too, or a `{}` clockMove layout stays visible.
  lockIfChanged(_clock, _clockHidden());
  if (_clock && _clock->locked()) _clock->clear();
  _volume();
  if (_battery) _updateBattery();
  if (_weather && config.store.showweather && network.weatherBuf) _weather->setText(network.weatherBuf);
  _station();
  _title();
  // A layout or a theme change has to land while the device is asleep as well, and both feed the
  // screensaver's own widgets - the meter's geometry and its palette - so the screensaver is rebuilt where
  // it stands rather than the screen being woken to the player page.  Queued, because this runs on
  // whichever task asked for the change.
  if (config.isScreensaver) { putRequest(SSREBUILD, _mode); return; }
  putRequest(NEWMODE, CLEAR);
  putRequest(NEWMODE, PLAYER);
}

void Display::applyLayout(uint8_t id) {
  if (id >= layoutCount) return;
  config.store.layoutId = id;
  _applyState();
}

uint8_t Display::getLayoutCount() { return layoutCount; }

void Display::applyTheme(uint8_t id) {
  if (id >= sizeof(_themes)/sizeof(_themes[0])) return;
  config.store.themeId = id;
  _applyState();
}

uint8_t Display::getThemeCount() { return sizeof(_themes) / sizeof(_themes[0]); }

// The font tables are macros in dspfont.h rather than private statics, but the handler asks the display
// for every count it clamps against: one shape for the four index commands, and this is the one that is
// safe in a DSP_DUMMY build, where display.h's stubs answer instead (builds/no_display).
uint8_t Display::getSystemFontCount() { return (uint8_t)_systemFontCount; }
uint8_t Display::getClockFontCount()  { return (uint8_t)_clockFontCount;  }

// Live system font switch.  Re-resolving every widget string is the obligation here (see
// plans/font-overhaul.md 9): the resolver memo is keyed on the font pointer, so a new pointer
// invalidates it for free, but the strings already sitting in the widgets were resolved with the
// old one.  There is no way to re-resolve them in place - a widget keeps only the resolved copy -
// so the widgets are re-initialised, which is exactly what APPLYSTATE does: it re-runs each
// widget's init() and its setText() with the source text.  Metrics cannot move (all system fonts
// are validated onto the 6x8 class at boot), so no layout re-measurement is needed.
void Display::applySystemFont(uint8_t id) {
  if (id >= _systemFontCount) id = 0;
  config.store.systemFontId = id;
  activeSystemFontId = id;
  putRequest(APPLYSTATE);
}

// The clock font STYLE, same shape.  Its size index does not change, so the only thing that has to
// be re-read is the style: ClockFontStyle() and the widget's own _superfont/_timeheight/_clockwidth
// all come out of init(), which APPLYSTATE re-runs.
void Display::applyClockFont(uint8_t id) {
  if (id >= _clockFontCount) id = 0;
  config.store.clockFontId = id;
  activeClockFontId = id;
  putRequest(APPLYSTATE);
}

// Both font ids in one request: what a screen reset wants, since the theme and layout ids are in the store by
// then and APPLYSTATE reads all four.
void Display::applyFonts(uint8_t systemId, uint8_t clockId) {
  if (systemId >= _systemFontCount) systemId = 0;
  if (clockId >= _clockFontCount) clockId = 0;
  config.store.systemFontId = systemId;
  config.store.clockFontId = clockId;
  activeSystemFontId = systemId;
  activeClockFontId = clockId;
  putRequest(APPLYSTATE);
}

void Display::_buildJsonCache() {
  // Build theme list JSON once — theme names are PROGMEM constants
  _themeListJson = "{";
  for (uint8_t i = 0; i < sizeof(_themes)/sizeof(_themes[0]); i++) {
    if (i > 0) _themeListJson += ',';
    _themeListJson += '"' + String(i) + "\":\"";
    char buf[33];
    strncpy_P(buf, _themeNames[i], 32);
    buf[32] = 0;
    _themeListJson += buf;
    _themeListJson += '"';
  }
  _themeListJson += '}';

  // Build layout list JSON once
  _layoutListJson = "{";
  for (uint8_t i = 0; i < layoutCount; i++) {
    if (i > 0) _layoutListJson += ',';
    _layoutListJson += '"' + String(i) + "\":\"";
    char buf[33];
    strncpy_P(buf, _layoutNames[i], 32);
    buf[32] = 0;
    _layoutListJson += buf;
    _layoutListJson += '"';
    }
    _layoutListJson += '}';
  
    // The two font lists, straight off the tables in dspfont.h.  The names are proper nouns, so they
    // are not translated and need no locale key - only the two labels above the dropdowns have one.
    _systemFontListJson = "{";
    for (uint8_t i = 0; i < _systemFontCount; i++) {
      if (i > 0) _systemFontListJson += ',';
      _systemFontListJson += '"' + String(i) + "\":\"";
      char buf[65];
      strncpy_P(buf, _systemFontNames[i], 64);
      buf[64] = 0;
      _systemFontListJson += buf;
      _systemFontListJson += '"';
    }
    _systemFontListJson += '}';
  
    _clockFontListJson = "{";
    for (uint8_t i = 0; i < _clockFontCount; i++) {
      if (i > 0) _clockFontListJson += ',';
      _clockFontListJson += '"' + String(i) + "\":\"";
      _clockFontListJson += _clockFontStyles[i]->name;
      _clockFontListJson += '"';
    }
    _clockFontListJson += '}';
  }
  
  String Display::getThemeListJson() { return _themeListJson; }
  String Display::getLayoutListJson() { return _layoutListJson; }
  String Display::getSystemFontListJson() { return _systemFontListJson; }
  String Display::getClockFontListJson() { return _clockFontListJson; }

void Display::applyInvertTitle() {
  _applyState();
}

void Display::invert() { dsp.invert(); }

bool Display::deepsleep() {
#if defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.sleep();
  return true;
#endif
  return false;
}

void Display::wakeup() {
  #if defined(DSP_OLED) || BRIGHTNESS_PIN!=255
    dsp.wake();
  #endif
}

#else // ============================== DUMMYDISPLAY Begins ==============================

void Display::init() {
  _activeLocale = l10n_findLocale(config.store.locale_display);
  _createDspTask();
}
void Display::_start() {
  #ifdef USE_SD
    config.setTitle(network.status == SDOFFLINE && !sdman.ready ? l10n(L10N_MSG_NO_SD_CARD) : l10n(L10N_MSG_READY));
  #else
    config.setTitle(l10n(L10N_MSG_READY));
  #endif
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (type==DSP_START) _start();
}

#endif // ============================== DUMMYDISPLAY Ends ==============================

