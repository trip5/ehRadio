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
#include "../displays/themeregistry.h"
#include "../displays/layoutregistry.h"
#include "../displays/widgets/pages.h"
#include "../displays/widgets/widgets.h"
#include "../displays/widgets/widget_vu.h"
#include "../displays/tools/dspstats.h"
#include "battery.h"

extern const char batterytxtFmt[] PROGMEM;
static constexpr uint32_t DSP_TASK_DELAY = 10;  // ms; minimum sleep between display iterations
static constexpr uint32_t DSQ_SEND_DELAY = 200;  // ms; xQueueSend timeout in display.putRequest()
static constexpr uint32_t SS_LINE_HOLD_MS = 3000;  // ms; How long the screensaver's info line stays blank after a track change

// These three flags are per-layout booleans in LayoutData now, not per-model macros.

Display display;

// Layout switching — pointers initially point to PROGMEM defaults
LayoutData activeLayout;

// The layout editor's two temporary switches.  Runtime only, and defined out here rather than inside the
// real-display block below because netserver.cpp reads them for the WebUI battery row, and that runs in every
// build.  System variables rather than Display members because the VU painter, in its own translation unit,
// reads them too.  `editorOpt()` in widgetsconfig.h is the one reader's helper.
bool widgetsEditorForceOn  = false;
bool widgetsEditorForceOff = false;

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
// Point into activeLayout (the memcpy_P target), so no re-pointing on a layout switch.
const bool*           shareWeatherIP_ptr  = &activeLayout.shareWeatherIP;
const bool*           shareBattRSSI_ptr   = &activeLayout.shareBattRSSI;
const bool*           rssiDigit_ptr       = &activeLayout.rssiDigit;
const FillConfig*     underLineConf_ptr   = &_layouts[0].underLineConf;
const FillConfig*     overLineConf_ptr    = &_layouts[0].overLineConf;

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
// The rule itself is presentOf() in widgetsconfig.h - ONE rule, every cell zero means absent.  It is not
// restated here because the registry's save check, the editor's per-card checkbox and conf_tool.py all have
// to agree with the draw-time test, and four copies of "is this field on" could only ever drift.
// The clock and the digits are no longer the exception: their `fontsize` is 0 while present and it is their
// `align` that makes them present, which is the same rule as every other type.
// Consult it at every show/hide AND re-show site.  See plans/layout-widget-lifecycle.md and
// plans/layout-editor-round3.md

static inline bool metaInLayout()        { return presentOf(*metaConf_ptr); }
static inline bool title1InLayout()      { return presentOf(*title1Conf_ptr); }
static inline bool title2InLayout()      { return presentOf(*title2Conf_ptr); }
static inline bool playlistInLayout()    { return presentOf(*playlistConf_ptr); }
static inline bool weatherInLayout()     { return presentOf(*weatherConf_ptr); }
static inline bool vuInLayout()          { return presentOf(*vuConf_ptr) && presentOf(*bandsConf_ptr); }
static inline bool bitrateInLayout()     { return presentOf(*bitrateConf_ptr); }
static inline bool fullbitrateInLayout() { return presentOf(*fullbitrateConf_ptr); }
static inline bool volbarInLayout()      { return presentOf(*volbarConf_ptr); }
static inline bool bufferbarInLayout()   { return presentOf(*bufferbarConf_ptr); }
static inline bool voltxtInLayout()      { return presentOf(*voltxtConf_ptr); }
static inline bool ipInLayout()          { return presentOf(*iptxtConf_ptr); }
static inline bool rssiInLayout()        { return presentOf(*rssiConf_ptr); }
static inline bool batteryInLayout()     { return presentOf(*batteryConf_ptr); }
// The meter's two gates, each in one place.  The editor's force-on opens both, so a layout's VU can be positioned
// and sized with the device stopped and with the meter switched off in the WebUI; force-off closes both.
static inline bool vuLive()              { return vuInLayout() && editorOpt(config.store.vumeter); }
static inline bool vuDraws()             { return vuLive() && (widgetsEditorForceOn || player.isRunning()); }
// The weather's one gate, and the editor's switches work it the same way: the line has to be on the panel to be
// positioned, whether or not this device has weather configured at all.  What it SAYS is network.cpp's job, and
// no fetch is started by this - a device with real weather keeps showing that.
static inline bool weatherLive()         { return editorOpt(config.store.showweather); }
static inline bool clockInLayout()       { return presentOf(*clockConf_ptr); }
static inline bool numInLayout()         { return presentOf(*numConf_ptr); }
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
const bool*           shareWeatherIP_ptr  = nullptr;
const bool*           shareBattRSSI_ptr   = nullptr;
const bool*           rssiDigit_ptr       = nullptr;
#endif

#ifndef DUMMYDISPLAY
// Real build only: no widgets.h under DSP_DUMMY, so Widget is incomplete here and there is nothing to lock.

// Widget::lock() is not idempotent - always change lock state through this (once re-cleared the IP).
static void lockIfChanged(Widget* w, bool hide) {
  if (!w) return;
  // An unchanged lock state does NOTHING, so a widget that managed to get drawn while it was already hidden is
  // not erased by this call - the caller is the one that has to have got the state right before it draws.
  if (w->locked() == hide) return;
  w->lock(hide);
}

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
  // A move that names the position the conf ALREADY gives this widget is a no-op, not a repaint: the confs write
  // their own numbers to say "this state leaves the line alone", and moving it to where it is costs an erase and
  // a redraw over identical pixels - a visible tear on a panel that writes as slowly as this one.  See
  // Widget::moveIsNoop().
  if (w->moveIsNoop(m)) return;
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
  return weatherLive() && network.weatherBuf && network.weatherBuf[0];
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
  layoutCopy(config.store.layoutId, &activeLayout);
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
  // (so WA_CENTER still rules), a longer string parks at the span's edge and scrolls.  Only the scroll fields are
  // derived.
  ScrollConfig bootScroll;
  bootScroll.widget = _bootConfig.bootstrConf;  // top, textsize and align straight from the conf
  // `offset` is the span's LEFT EDGE, not a margin (see WidgetConfig::offset), and it is dead in the plain centred
  // widget this conf is declared as - which is why every conf leaves it 0.  It goes live the moment the line
  // becomes a scroll, so centring inside [0, MAX_WIDTH) put the text one TFT_BORDER left of the glass: a character
  // or two at the boot sizes, and the only line on the boot screen that was off.  The span is MAX_WIDTH wide, so
  // its edge is the border inset itself.
  bootScroll.widget.offset = TFT_BORDER;
  bootScroll.width = MAX_WIDTH;
  // The cadence is borrowed from the panel's own message line rather than hardcoded: apSettConf is the same
  // species of line and its step and scrolltime are tuned per panel.  Only these three fields are taken, and a
  // zeroed line is absent by the one rule, so fall back to a 1px step on the default tick.
  const bool apSettUsable = presentOf(_bootConfig.apSettConf);
  bootScroll.startscrolldelay = apSettUsable ? _bootConfig.apSettConf.startscrolldelay : 0;
  bootScroll.scrolldelta      = apSettUsable ? _bootConfig.apSettConf.scrolldelta : 1;
  bootScroll.scrolltime       = apSettUsable ? _bootConfig.apSettConf.scrolltime : SCROLLTIME;
  // BOOTSTR_LEN, not the widget default: this line's window is its own budget, sized for a locale label, a
  // 32 character SSID and the icon pair.
  _bootstring = (TextWidget*) &_boot->addWidget(new ScrollWidget(bootScroll, BOOT_TXT_COLOR, 0, BOOTSTR_LEN));
  _bootstring->setText(""); // empty until player.init happens
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
  if (presentOf(*title2Conf_ptr)) {
    _title2 = new ScrollWidget(*title2Conf_ptr, config.theme.title2, config.theme.background);
  }
  _plbackground = new FillWidget(*playlBGConf_ptr, config.theme.plcurrentbg);
  _metabackground = new FillWidget(*metaBGConf_ptr, config.theme.metabg);
  // These two always exist - their confs are re-pointed behind them (invert, playlist mode) - so a zeroed
  // conf is a state here, not an absence.  The optional lines are made by _syncLineRule() below.
  if (presentOf(*vuConf_ptr)) {
    _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis, !_vuNotDrawable);
  }
  if (presentOf(*volbarConf_ptr)) {
    _volbar = new SliderWidget(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
  }
  if (presentOf(*bufferbarConf_ptr)) {
    _bufferbarMax = 1024 * BUFFERBAR_VISUAL_FULL_KB;
    // The fifth argument is the outlined frame's colour.  Without it SliderWidget falls back to 0, so a buffer bar
    // with `outlined` true drew a BLACK frame - invisible on a black panel, which is why the setting looked ignored.
    // The volume bar has always passed its own theme colour; the buffer bar frames in its own.
    _bufferbar = new SliderWidget(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax, config.theme.buffer);
  }
  if (presentOf(*voltxtConf_ptr)) {
    _voltxt = new TextWidget(*voltxtConf_ptr, config.theme.vol, config.theme.background, 10);
  }
  if (presentOf(*iptxtConf_ptr)) {
    _volip = new TextWidget(*iptxtConf_ptr, config.theme.ip, config.theme.background, 48);  // 48 bytes = 15 code points × 3 bytes (CJK) + 2 icons + null
  }
  if (presentOf(*rssiConf_ptr)) {
    _rssi = new TextWidget(*rssiConf_ptr, config.theme.rssi, config.theme.background, 20);
  }
  if (presentOf(*batteryConf_ptr)) {
    _battery = new TextWidget(*batteryConf_ptr, config.theme.battery, config.theme.background, 10);
  }
  if (presentOf(*weatherConf_ptr)) {
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
  if (presentOf(*fullbitrateConf_ptr)) {
    _fullbitrate = new BitrateWidget(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_fullbitrate);
  }
  if (presentOf(*bitrateConf_ptr)) {
    _bitrate = new TextWidget(*bitrateConf_ptr, config.theme.bitrate, config.theme.background, 30);
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
    // scrolling update/validate label and progress bar.  Built in every build - the online update drives it,
    // and so does the playlist validator.
    // Copy apSettConf's scroll params, but centre on the GLASS.  A WA_CENTER line centres inside its span, and
    // that span starts at `offset`, so zeroing offset while keeping the conf's inset width left the label one
    // border left of centre - the same rule the boot line below now honours.  Centre the span itself, whatever
    // width this panel's apSettConf asks for.
    ScrollConfig updConf = _bootConfig.apSettConf;
    updConf.widget.offset = (dsp.width() > updConf.width) ? (uint16_t)((dsp.width() - updConf.width) / 2) : 0;
    updConf.widget.align = WA_CENTER;
    updConf.widget.top = (dsp.height() - (updConf.widget.textsize * CHARHEIGHT)) / 2;
    updConf.widget.top = max<int16_t>(0, updConf.widget.top - CHARHEIGHT);
    _updLabel = new ScrollWidget(updConf,
                                 config.theme.title1, config.theme.background);
    _updLabel->lock(true);   // don't paint the label's background band unless updating

    // compute bar width once
    {
      uint8_t ts = _bootConfig.apPassConf.textsize > 0 ? _bootConfig.apPassConf.textsize : 1;
      uint16_t widgetPx = dsp.width() - _bootConfig.apPassConf.offset;
      int chars = (int)(widgetPx / (CHARWIDTH * ts));
      _updBarWidth = (chars < 2) ? 2 : (chars > 64) ? 64 : chars;
    }

    // place progress widget under the label maintaining original spacing
    WidgetConfig valConf = _bootConfig.apPassConf;
    int16_t origGap = _bootConfig.apPassConf.top - _bootConfig.apNameConf.top;
    if (origGap < 0) origGap = updConf.widget.textsize * CHARHEIGHT + 2; // fallback
    valConf.top = updConf.widget.top + origGap;
    _updValue = new TextWidget(valConf, config.theme.clock, config.theme.background,
                               (uint16_t)(_updBarWidth + 2));
    MoveConfig mvValue{0, valConf.top, (int16_t)dsp.width()};
    _updValue->moveTo(mvValue);
    pages[PG_DIALOG]->addWidget(_updLabel);
    pages[PG_DIALOG]->addWidget(_updValue);
  
  pages[PG_DIALOG]->addPage(_footer);
  // Both playlist widgets go on the page always: playlistPages is a runtime option now, so the page cannot decide at
  // boot.  Only the fade drawer updates them, and the paged painter fills the list area with the background on every
  // draw, so a paged playlist simply paints over them.
  if (_plbackground) {
    pages[PG_PLAYLIST]->addWidget(_plbackground);
    _plbackground->setHeight(_plwidget->itemHeight());
    _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf_ptr->widget.textsize*2), (int16_t)playlBGConf_ptr->width});
  }
  pages[PG_PLAYLIST]->addWidget(_plcurrent);
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for(const auto& p: pages) _pager->addPage(p);
  _buildJsonCache();
}

void Display::_apScreen() {
  dsp.setScrollId(NULL);   // the page below may own the slot, and its widget is about to be deleted
  if (_boot) {
    _pager->removePage(_boot);
    _boot = nullptr;
    _bootstring = nullptr;
  }
    _boot = new Page();
    // The boot screens own their band and ignore invert title - a layout is selectable, they are not.
    // Optional: { } means this panel wants no band, so no widget is made for one.
    if (_bootConfig.apTitleBGConf.height > 0) {
      _boot->addWidget(new FillWidget(_bootConfig.apTitleBGConf, config.theme.metabg));
    }
    uint16_t mfg = config.theme.meta;
    uint16_t mbg = config.theme.metabg;
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apTitleConf, mfg, mbg));
    bootTitle->setText(l10n(L10N_LBL_AP_IMPROV_MODE));
    TextWidget *apname = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apNameConf, config.theme.title1, config.theme.background, 30));
    apname->setText(l10n(L10N_LBL_APNAME));
    TextWidget *apname2 = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apName2Conf, config.theme.clock, config.theme.background, 30));
    apname2->setText(AP_SSID);
    TextWidget *appass = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apPassConf, config.theme.title1, config.theme.background, 30));
    #ifdef AP_PASSWORD
      appass->setText(l10n(L10N_LBL_APPASS));
    #else 
      appass->setText(l10n(L10N_LBL_APNOPASS));
    #endif
    TextWidget *appass2 = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apPass2Conf, config.theme.clock, config.theme.background, 30));
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
  dsp.setScrollId(NULL);   // the page below may own the slot, and its widget is about to be deleted
  if (_boot) {
    _pager->removePage(_boot);
    _boot = nullptr;
    _bootstring = nullptr;
  }
  _sdmanCountText = nullptr;
  _boot = new Page();
  // Own band, no invert title, skipped when empty - same rule as the AP screen.
  if (_bootConfig.apTitleBGConf.height > 0) {
    _boot->addWidget(new FillWidget(_bootConfig.apTitleBGConf, config.theme.metabg));
  }
  uint16_t mfg = config.theme.meta;
  uint16_t mbg = config.theme.metabg;
  ScrollWidget *sdTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apTitleConf, mfg, mbg));
  sdTitle->setText(l10n(L10N_LBL_SDMAN));
  ScrollWidget *sdUrl = (ScrollWidget*) &_boot->addWidget(new ScrollWidget(_bootConfig.apSettConf, config.theme.title2, config.theme.background));
  sdUrl->setText(utility.ipToStr(WiFi.localIP()), l10n(L10N_MSG_OPEN));
  // apName2Conf, not apNameConf: the fontsize-2 title already occupies y=2..18 on a 128x64 and apNameConf's
  // top is 18, so the countdown drew over the title's last row.
  _sdmanCountText = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.apName2Conf, config.theme.clock, config.theme.background, 30));
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
  dsp.setScrollId(NULL);   // the boot page may own the slot, and its widget is about to be deleted
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
  
  if (_bufferbar)  lockIfChanged(_bufferbar, !bufferbarInLayout() || !editorOpt(config.store.bufferbar));
  
  hideWeatherIfChanged(_weather, _weatherHidden());
  if (_weather && weatherLive() && network.status != SDOFFLINE) network.buildWeatherString();

  // lockIfChanged, then clear() so an already-inactive clock is erased too.
  if (_clock) {
    lockIfChanged(_clock, _clockHidden());
    if (_clock->locked()) _clock->clear();
  }

  // A direct lock() here re-cleared the meter's box on every boot path that had already locked it in
  // _applyState(); lockIfChanged() is the one owner of "only a state change may erase".
  if (_vuwidget) lockIfChanged(_vuwidget, true);
  if (_rssi) { if (network.status == SDOFFLINE) _setRSSI(0); else _setRSSI(WiFi.RSSI()); }
  // shareBattRSSI toggles _active to pick between RSSI and battery - a re-show site, hence the predicate.
  if (*shareBattRSSI_ptr && _battery && _rssi) {
    bool haveBattery = !widgetsEditorForceOff && (battery.isInitialized() || widgetsEditorForceOn);
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
        if (*shareWeatherIP_ptr && weatherLive()) _volip->setText("");
        else _footerIp();
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

// The footer IP, written in one place.  It is the user's indicator that the WebUI is up.
void Display::_footerIp() {
  if (!_volip) return;
  if (!netserver.isListening()) { _volip->setText(""); return; }
  _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
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
void Display::_showNumbers(const char* header, int32_t value, const char* fmt, bool dialogPage, uint32_t fieldmax) {
  // The shared rows first, in both variants: several layouts put the number on a row the weather, IP, battery or
  // RSSI line normally uses, and those widgets draw themselves on request rather than through the page pass.
  if (*shareWeatherIP_ptr && weatherLive() && _weather) {
    // pause weather updates while the number is up, so it cannot overwrite the IP line
    lockIfChanged(_weather, true);
    _weather->setText("");
  }
  if (*shareBattRSSI_ptr && _battery && _rssi) {
    _battery->setText(""); _battery->setActive(false);
    _rssi->setActive(rssiInLayout());
  }
  if (dialogPage) _showDialog(header);
  if (_volip) {
    if (network.status == SDOFFLINE) _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
    else _footerIp();
  }
  // The field the glow reserves: the widest value this screen can show, not the one it happens to be showing.
  _nums->setFieldMax(fieldmax);
  if (value < 0) _nums->setText("");
  else _nums->setText(value, fmt);
}

void Display::_setReturnTicker(uint8_t time_s) {
  _returnTicker.detach();
  _returnTicker.once(time_s, returnPlayer);
}

void Display::_switchMode(displayMode_e newmode) {
  if (newmode == CLEAR) { dsp.fillScreen(config.theme.background); _mode = CLEAR; return; }
  if (newmode == VOL && !editorOpt(config.store.volumepage)) return;  // no overlay — skip VOL mode to avoid a needless page switch
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
    // Leaving the screensaver settles the clock BEFORE the player page is painted.  The clock is the last
    // widget _buildPager() adds, so an erase issued after setPage() lands on the freshly drawn page - the
    // reported "chunk out of the top bar".  Two things have to happen first: the clock has to come back
    // from the screensaver's random spot, and its feature lock has to be applied, because a yield-to-VU
    // hide is an erase.  The screensaver page is still up for both, and setPage() clears it on the way out.
    const bool fromScreensaver = config.isScreensaver;
    config.isScreensaver = false;
    // Away from the screensaver the clock's own rung comes back.  Rebuilt before the move, because init()
    // resets the widget to its conf position and a MOVE applied before it would be lost.
    if (_clock && clockInLayout()) _clock->refreshSize(*clockConf_ptr);
    if (fromScreensaver && _clock) _clock->moveBack();
    if (player.isRunning()) {
      if (vuLive() && _vuwidget) {
        applyMove(_clock, *clockMove_ptr);
        applyMove(_weather, *weatherMoveVU_ptr);
      } else {
        if (!fromScreensaver) _clock->moveBack();
        applyMove(_weather, *weatherMove_ptr);
      }
    } else {
      if (_clock) _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
    if (_clock) lockIfChanged(_clock, _clockHidden());
    numOfNextStation = 0;
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
          if (*shareWeatherIP_ptr && weatherLive()) _volip->setText("");
          else _footerIp();
        }
      }
    // force weather repaint on return to PLAYER; larger displays repaint naturally
    if (*shareWeatherIP_ptr && weatherLive() && _weather) {
      lockIfChanged(_weather, _weatherHidden());
      // Force a clean repaint of the shared weather/IP row after overlays like VOL/SCREENSAVER.
      _weather->setText("");
      if (network.weatherBuf) _weather->setText(network.weatherBuf);
    }
    if (*shareBattRSSI_ptr && _battery && _rssi) {
      bool haveBattery = !widgetsEditorForceOff && (battery.isInitialized() || widgetsEditorForceOn);
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
    // A fresh entry is not a track change: the strip comes up with whatever the state is now.  _enterScreensaver()
    // deliberately does NOT clear this - SSREBUILD re-enters through it, and a settings change during the three
    // seconds must not cancel the hold.
    _ssHoldUntil = 0;
    config.isScreensaver = true;
    _enterScreensaver(newmode);
  } else {
    // This branch is EVERY mode change to something that is not the screensaver
    _ssHoldUntil = 0;   // the page the line belongs to is gone, so nothing is held
    config.resetScreensaverTimers(SCREENSAVERSTARTUPDELAY);
    config.isScreensaver = false;
    
    lockIfChanged(_clock, _clockHidden());
  }
  if (newmode == VOL) {
    _showNumbers(l10n(L10N_LBL_VOLUME), config.store.volume, numtxtFmt, editorOpt(config.store.volumepage), VOLUME_SCALE);
  }
  if (newmode == LOST)      _showDialog(l10n(L10N_LBL_LOST));
  if (newmode == UPDATING)  { _showDialog(l10n(L10N_LBL_UPDATE)); _updFirstCall = true; }
  if (newmode == VALIDATING) { _showDialog(l10n(L10N_LBL_VALIDATING)); _updFirstCall = true; }
  if (newmode == SLEEPING)  _showDialog(l10n(L10N_LBL_SLEEPING));
  // no count yet: SDFILEINDEX fills it in, and countStation is the last known total - the pre-count or the
  // previous index - so the counter's field is as wide as the card needs for most of the walk.
  if (newmode == SDCHANGE)  _showNumbers(l10n(L10N_LBL_WAITFORSD), -1, "%d", true, config.store.countStation);
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog("");
  if (newmode == NUMBERS)   _showNumbers("", -1, "%d", true, config.store.countStation);   // the header and the number arrive per digit
  if (newmode == STATIONS) {
    _pager->setPage(pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    if (editorOpt(config.store.playlistPages)) _plwidget->resetState();
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
  // The clock's size index is resolved in its init(), and the screensaver's Smaller Clock option changes it, so the
  // clock is rebuilt before anything here paints it.  config.isScreensaver is already true on every route in.
  if (_clock && clockInLayout()) _clock->refreshSize(*clockConf_ptr);
  _screensaverWidgets();        // the widgets are built BEFORE the page switch, so the first pass draws
                                // the picture the prefs describe, not last time's frame
  // Place the clock inside the walk area before the page paints.  The layout's position is not on the
  // walk's map - it can sit inside the info line's strip - and the first timed step waits for
  // tm_sec % SCREENSAVERMOVE == 0, so without this the clock could overlap the strip for that whole
  // interval.  moveToQuiet() leaves the panel alone: the page activation below draws the clock at its
  // new spot, in the widget order that keeps it under the info line and the meter.  With _ssMeterUp the
  // meter owns the panel and the clock is locked, so there is nothing to place.
  if (_clock && !_ssMeterUp) {
    MoveConfig mv;
    _screensaverClockTarget(mv);
    _clock->moveToQuiet(mv);
  }
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
  _previewPending = false; // ...and any preview waiting on it, or the next one would be coalesced away
  _layoutPreviewPending = false;
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
  _showNumbers(utility.stationByNum(num), (int32_t)num, "%d", true, config.store.countStation);
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (displayQueue==NULL) return;
  // One preview of each kind at a time.  A preview that is already waiting is redundant whichever way you
  // look at it - the staging copy is where the newest values live, and the waiting request reads it when it
  // runs - so a second one would only queue another full widget re-init.  Each kind has its own flag because
  // they stage different things, so coalescing across them would lose an edit outright.  A flag is taken
  // from the send result below rather than set before it, because a saturated queue drops requests (see the
  // note under the send) and a flag left set would block previews for good.
  if (type == THEMEPREVIEW && _previewPending) return;
  if (type == LAYOUTPREVIEW && _layoutPreviewPending) return;
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
    WARNINGLOG("DISPLAY: request %d dropped, queue full (%u so far)", (int)type, (unsigned)_droppedRequests);
  } else if (type == THEMEPREVIEW) {
    _previewPending = true;
  } else if (type == LAYOUTPREVIEW) {
    _layoutPreviewPending = true;
  }
}

void Display::updateProgress(const char* label, float progress) {
    if (_updFirstCall) {
      _updFirstCall = false;
      delay(50); // allow display task to process NEWMODE/UPDATING queue item before drawing
    }
    if (_updLabel) {
      _updLabel->setText(label);
    }
    if (_updValue) {
      const char barChar = '\016'; // play icon is progress
      char buf[68];
      // Clamp the ratio BEFORE the float -> int cast: an out-of-range or NaN progress makes that conversion
      // undefined, which leaves `bars` unconstrained for the analyser - that is what -Wstringop-overflow was
      // reporting here, a negative int read as memset's size_t (a hypothetical ~4 GB).  `!(progress > 0.0f)` is
      // deliberately NaN-safe.
      if (!(progress > 0.0f))   progress = 0.0f;
      else if (progress > 1.0f) progress = 1.0f;
      // The width is bounded HERE because GCC cannot see the 2..64 clamp _showDialog() applies to the member:
      // `int _updBarWidth` is set in another function, so all three sizes below would otherwise be unbounded.
      // Bound it to the buffer, which is what those three uses actually need.
      int width = _updBarWidth;
      if (width < 0) width = 0;
      if (width > (int)sizeof(buf) - 1) width = (int)sizeof(buf) - 1;
      int bars = (int)(progress * width + 0.5f);
      if (bars < 0) bars = 0;
      if (bars > width) bars = width;
      memset(buf, barChar, bars);
      memset(buf + bars, ' ', width - bars); // empty space is empty space
      buf[width] = '\0';
      _updValue->setText(buf);
    }
}

// Blank the OTA/validator progress.  The label and the bar live on the dialog page, which the volume overlay and
// the LOST/UPDATE dialogs reuse, so a finished or cancelled run would otherwise leave its bar on those screens.
// updateProgress("", 0) draws the bar as all spaces - the same path a zero-progress update takes, so the widget
// is genuinely repainted rather than merely emptied.
void Display::clearProgress() {
  _updFirstCall = false;
  updateProgress("", 0.0f);
}

// Screens the display owns rather than borrows.  Both are holding patterns, not player pages: the self-drawing
// widget requests are dropped while one is up (drawsOverOwnScreen) and the clock and weather are hidden, because
// nothing repaints them afterwards and their page no longer ticks.
bool Display::_ownScreen() const {
  if (_mode == SDCHANGE || _mode == NUMBERS || _mode == VALIDATING) return true;
  if (_mode == VOL && editorOpt(config.store.volumepage)) return true;
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
  const bool yieldsToVU   = (vuLive() && player.isRunning() && moveZeroed(*clockMove_ptr));
  if (_ownScreen()) return true;
  return noTimeSource || yieldsToVU || !clockInLayout();
}

// Same shape for the weather, plus shared-row suppression during the volume overlay.  The widget belongs to the
// player page alone, so while the screensaver owns the panel it is never drawn: the screensaver's own line is what
// carries the weather there (see plans/screensaver-panel-ownership.md).
bool Display::_weatherHidden() {
  if (config.isScreensaver) return true;
  // The editor's force-on: the line has to be on the panel to be positioned, whether or not this device has
  // weather configured at all.
  if (widgetsEditorForceOn) return false;
  const bool featureOff = !weatherLive();
  // A zeroed move hides its widget in the state that move governs, and the two weather moves split those states:
  // weatherMoveVU covers playing with the meter live, weatherMove covers playing with it off.  applyMove()'s guard
  // is what keeps a hidden widget from ALSO being dragged to (0, 0) first - it stays where the conf put it and is
  // simply erased.  While stopped neither applies: the line sits at weatherConf, which is what that field is for.
  const bool yieldsToVU = (vuLive()      && player.isRunning() && moveZeroed(*weatherMoveVU_ptr));
  const bool yieldsNoVU = (!vuLive()     && player.isRunning() && moveZeroed(*weatherMove_ptr));
  bool volOverlay = false;
  volOverlay = *shareWeatherIP_ptr && (_mode == VOL);
  if (_ownScreen()) return true;
  return featureOff || yieldsToVU || yieldsNoVU || volOverlay || !weatherInLayout();
}

void Display::_layoutChange(bool played) {
  // While the screensaver owns the panel the WALK owns the clock's position, and this function must not touch it.
  const bool ssOwnsPanel = config.isScreensaver;
  if (vuLive() && _vuwidget) {
    if (played) {
      if (_vuwidget) _vuwidget->unlock();
      if (!ssOwnsPanel) applyMove(_clock, *clockMove_ptr);
      applyMove(_weather, *weatherMoveVU_ptr);
    } else {
      if (_vuwidget) if (!_vuwidget->locked()) _vuwidget->lock();
      if (!ssOwnsPanel) _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
  } else {
    if (played) {
      if (!ssOwnsPanel) _clock->moveBack();  // restore clock from VU-shifted position
      applyMove(_weather, *weatherMove_ptr);
      //_clock->moveBack();
    } else {
      if (_weather) _weather->moveBack();
      if (!ssOwnsPanel) _clock->moveBack();
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
  // A hide HERE erases on a page that is already drawn, and that erase is a background push over the clock's
  // whole window - the band the moved weather line shares.  hideWeatherIfChanged() returns early for a line that
  // is already visible (its lock is what it tests), so the "bite" it leaves has nothing to repair it: repaint the
  // weather's own text explicitly.  Safe as a no-op when the line is hidden or the two never overlapped.
  if (_clock && _clock->locked() && !clockWasHidden && _weather && !_weather->locked()) _weather->repaint();
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
  // A held info line comes back on its own.  The requests that would have repainted it were refused, and nothing is
  // guaranteed to arrive at the deadline, so this tick restores it - once, from the state as it then stands.
  if (_ssHoldUntil && (int32_t)(millis() - _ssHoldUntil) >= 0) {
    _ssHoldUntil = 0;
    if (config.isScreensaver) _screensaverLine();
  }
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
        // The same re-init for the theme editor's live preview, but repainted in place: the mode, the
        // overlays and the screensaver countdowns are all deliberately left alone.
        case THEMEPREVIEW:
          _previewPending = false;  // taken off the queue, so the next preview may be queued again
          // config.theme is written inside _applyState() - the only place allowed to - and it takes the
          // staged colours rather than re-reading the stored theme.  See the theme copy there: reading
          // the stored theme on this path is what made the first version of the preview a no-op.
          _applyState(APPLY_STAGED_THEME);
          break;
        // The layout editor's live preview, the same shape with the other half staged: activeLayout takes
        // the staged layout and the theme is left exactly as it is, which is what the two modes keep apart.
        case LAYOUTPREVIEW:
          _layoutPreviewPending = false;
          _applyState(APPLY_STAGED_LAYOUT);
          break;
        case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, request.payload});
        case CLOCK:
          if ((_mode==PLAYER || _mode==SCREENSAVER) && !(network.status == SDOFFLINE && !config.isRTCFound()))
            _time(request.payload);
          break;
        case NEWTITLE: _title(); break;
        case NEWSTATION:
          // An SD track change is announced by the new track's station name, queued by _play() BEFORE it opens the
          // file - so this hold is already up when handleBeginSdRead() clears the title, which is the moment the
          // weather would otherwise take the strip.  Only while the screensaver owns the panel: that is the only
          // place the line exists, and a manual skip has already left the screensaver by the time this is handled
          // (screenSaverExit() queues its mode change first), so only an EOF advance is held.
          if (config.isScreensaver) _ssHoldLine(SS_LINE_HOLD_MS);
          _station();
          break;
        case NEXTSTATION: _drawNextStationNum(request.payload); break;
        case DRAWPLAYLIST: _drawPlaylist(); break;
        case DRAWVOL: _volume(); break;
        case DBITRATE:
          if (_mode != PLAYER) break;  // skip draws when player page isn't visible (e.g., SD file list)
          _applyBitrate();
          break;
        case SHOWBUFFERBAR: if (_bufferbar)  {
            lockIfChanged(_bufferbar, !bufferbarInLayout() || !editorOpt(config.store.bufferbar));
            _bufferbar->setValue(normalizeBufferbarValue(player.inBufferFilled(), _bufferbarMax));
          }
          break;
        case SHOWVUMETER: {
          if (_vuwidget) {
            // An unchanged lock state must not erase.  While stopped vuDraws() is false in BOTH toggle
            // directions, so a plain lock(true) re-cleared the meter's box over the clock - the reported
            // bite - and nothing repainted the clock because it had never been hidden.  lockIfChanged()
            // is the one owner of the rule (see its definition); _layoutChange() handles the repaint.
            lockIfChanged(_vuwidget, !vuDraws());
            _layoutChange(player.isRunning());
          }
          break;
        }
        case SHOWWEATHER: {
          hideWeatherIfChanged(_weather, _weatherHidden());
          if (!weatherLive()) {
            if (_weather) _weather->setText("");
            _footerIp();
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
        case BOOTVSPATCH: {
          if (_bootstring) _bootstring->setText("VS1053");
          break;
        }
        case BOOTVERSION: {
          if (_bootstring) _bootstring->setText(RADIOVERSION);
          break;
        }
        case SDFILEINDEX: {
          if (_mode == SDCHANGE) _nums->setText(request.payload, "%d");
          break;
        }
        case DSPRSSI:
          if (_rssi) { _setRSSI(request.payload); }
          if (_bufferbar && editorOpt(config.store.bufferbar)) {
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
                if (!*shareWeatherIP_ptr || !(_mode == PLAYER && weatherLive()))
                  _footerIp();
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

// dBm -> a 0..100 reading for the numeric signal form: 2% per dB from -100 (0%) up to -50 (100%), and 100% above
// that.  The bar form is already a coarse five-step scale, so this makes the number read the same way rather than
// in raw dBm.  The conf's `rssiFmt` supplies the wording, and a `%%` in it prints the percent sign.
static uint8_t rssiPercent(int rssi) {
  if (rssi <= -100) return 0;
  if (rssi >= -50)  return 100;
  return (uint8_t)((rssi + 100) * 2);
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
    _rssi->setText(rssiPercent(rssi), rssiFmt);
    return;
  }
  // Level 0-4 comes from RSSI_STEPS in one place (utility.cpp); these are the same four steps the WebUI bars show.
  static const char* const rssiGlyphs[5] = { "\001\002", "\003\002", "\004\002", "\004\005", "\004\006" };
  _rssi->setText(rssiGlyphs[rssiLevel(rssi)]);
}

void Display::_updateBattery() {
  if(!_battery) return;

  // The editor's force-on fakes a half-full battery, which is also the only way to see this widget on a board
  // with none fitted.  Force-off needs no branch here: the widget is never made active in that case.
  int pct;
  if (widgetsEditorForceOn) {
    pct = 50;
  } else {
    BatteryStatus bat = battery.getStatus();
    if(!bat.present && battery.isInitialized()) {
      battery.recalcNow();
      bat = battery.getStatus();
    }
    if(!battery.isInitialized() || !bat.present) {
      _battery->setText("");
      return;
    }
    pct = bat.percentage;
  }

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

// The editor's Temporary-On filler: about one panel width of " ~]" (plus a little) appended to a managed line, so
// it always overflows its window and scrolls while the switch is set and the stream is playing.  The layout cannot
// be edited from the panel, so this is how the three text widgets' widths are made visible on it.  The widget
// truncates to WIDGET_BUFF_SIZE (140) regardless, so over-filling is harmless and under-filling would not scroll.
static void editorFiller(char* dst, size_t room, const char* src) {
  strlcpy(dst, src, room);
  if (!widgetsEditorForceOn || !player.isRunning()) return;
  size_t n = strlen(dst);
  for (int i = 0; i < (DSP_WIDTH / 18 + 12); i++) {   // 18 px per unit at the system font's 6 px cell, plus spare
    if (n + 3 >= room) break;
    memcpy(dst + n, " ~]", 3);
    n += 3;
  }
  dst[n] = '\0';
}

void Display::_station() {
  _meta->setAlign(metaConf_ptr->widget.align);
  char buf[STATION_FIELD_LENGTH];
  editorFiller(buf, sizeof(buf), config.station.name);
  _meta->setText(buf);
}

char *split(char *str, const char *delim) {
  char *dmp = strstr(str, delim);
  if (dmp == NULL) return NULL;
  *dmp = '\0';
  return dmp + strlen(delim);
}

// A title that came from a FILENAME carries its extension; one that came from a tag never does.  Stripping a known
// audio suffix in place is therefore safe and is what stops the second title line reading "Nothingman.mp3" on a card
// whose files have no readable tags (ID3v1 lives at the end of the file and is only read when the track ends).
static void stripAudioExtension(char* s) {
  static const char* const ext[] = { ".mp3", ".flac", ".ogg", ".opus", ".m4a", ".m4b", ".aac", ".wav", ".mp4" };
  const size_t n = strlen(s);
  for (size_t i = 0; i < sizeof(ext) / sizeof(ext[0]); ++i) {
    const size_t e = strlen(ext[i]);
    if (n <= e) continue;
    size_t k = 0;
    while (k < e && tolower((unsigned char)s[n - e + k]) == ext[i][k]) ++k;
    if (k == e) { s[n - e] = '\0'; return; }
  }
}

// config.station.title arrives as "artist - song" and the layout has two title lines for it.  Both the
// player page and the screensaver's info line want the two halves, so the rule lives here rather than in
// either caller.  False when there is no title at all; otherwise `first` is the whole string with
// `second` null unless the separator was found.  Both point into `dst`.
static bool splitTitle(char* dst, size_t dstSize, char*& first, char*& second) {
  first = second = nullptr;
  if (!config.station.title[0]) return false;
  strlcpy(dst, config.station.title, dstSize);
  stripAudioExtension(dst);
  if (!dst[0]) return false;
  second = split(dst, " - ");
  first = dst;
  return true;
}

void Display::_titleTexts() {
  char titlebuf[STATION_FIELD_LENGTH];
  char *t1 = nullptr, *t2 = nullptr;
  char b1[STATION_FIELD_LENGTH], b2[STATION_FIELD_LENGTH];
  if (splitTitle(titlebuf, sizeof(titlebuf), t1, t2) && t2 && _title2) {
    // Filler on each half separately, into its own buffer: t1 and t2 both point into titlebuf, so padding t1 in
    // place would overwrite the separator t2 was found past.
    editorFiller(b1, sizeof(b1), t1);
    editorFiller(b2, sizeof(b2), t2);
    _title1->setText(b1);
    _title2->setText(b2);
  } else {
    editorFiller(b1, sizeof(b1), config.station.title);
    _title1->setText(b1);
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

// The info line's text, joined with the shared scroll mark (scrollMark(): " * ", or the holiday icon), and it is
// one state or the other, never both: while something is playing the line describes what is playing - the station
// name and the two title lines - and the weather is left out, because a single-row strip that mixes a forecast
// into a track name pushes the track off it.  Over silence there is nothing to describe, so the weather is the
// whole line.  That is the same split _ssStripH() refuses the strip on: either half being absent leaves nothing
// to show.  Every empty part is skipped outright rather than left as a bare separator, which is why they all go
// through one append.
// Blank the info line and refuse to paint it for a while.  An SD track change clears the title and then re-fills it
// over two or three requests, so the strip visibly resets, and in the gap the line would show the weather.  So it
// goes blank once, when the new track starts loading, and stays blank; Display::loop() is what puts it back, because
// the requests that would have repainted it are exactly the ones being refused.
void Display::_ssHoldLine(uint32_t ms) {
  // The whole span, on both draw paths: _windowRect() answers the span when the text is empty, so the framebuffer
  // path fills the strip rather than nothing, and the non-framebuffer path erases the union with the span the
  // scrolling paint recorded.
  if (_sstext) _sstext->setText("");
  _ssHoldUntil = millis() + ms;
}

void Display::_screensaverLine() {
  if (!_sstext) return;
  // A track change blanks the line and holds it: _ssHoldLine() sets the deadline, Display::loop() clears it.
  if (_ssHoldUntil) return;
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
  } else if (weatherLive() && network.weatherBuf) {
    // Weather only over silence: playing, the line is the track's, not the forecast's.
    append(line, sizeof(line), network.weatherBuf);
  }
  // The screensaver's line is the one that must MOVE: it is the whole bottom row of a panel whose only job here is
  // to look alive, and a short station name or a one-line forecast would otherwise leave it standing still.  The
  // widget repeats the text, joined with its own mark, until it is wider than the strip - the width maths, the mark
  // and the buffer ceiling are all its own, so this stays a one-copy composition.  See plans/screensaver-line-loop.md.
  _sstext->setTextLooped(line);
}

// Bring both screensaver widgets in line with the prefs and the active layout.  Called on every entry
// into SCREENSAVER rather than once at boot: the meter's segmentation comes from bandsConf, its palette
// from the theme and the line's size and scroll speed from weatherConf, and the WebUI stays reachable while
// the device is asleep, so a layout or theme switch has to land.  A layout with no VU box of its own is
// fine here - the screensaver's meter is this widget, not the player page's.
void Display::_screensaverWidgets() {
  // --- the info line ---
  if (_ssStripH()) {
    ScrollConfig conf = *weatherConf_ptr;
    conf.widget.offset = 0;
    conf.widget.top = _ssTextTop();
    conf.widget.textsize = ssTextSize();
    conf.width = dsp.width();
    // Three quarters of the weather row's speed.  scrolldelta is pixels per step and scrolltime the ms between
    // steps, so speed = delta/time and either one can carry the reduction; the step is preferred because it keeps
    // the finer motion, but only where it divides exactly - a 1 px step (every OLED row) or the TFTs' 2 px scaled
    // by 3/4 would truncate to 0, or to half the speed asked for, so those stretch the interval instead.
    // startscrolldelay is left as weatherConf states it (0): the line moves as soon as it is up.
    if (weatherConf_ptr->scrolldelta % 4 == 0) {
      conf.scrolldelta = (uint8_t)(weatherConf_ptr->scrolldelta * 3 / 4);
    } else {
      conf.scrolltime = (uint16_t)(weatherConf_ptr->scrolltime * 4 / 3);
    }
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
    lockIfChanged(_sstext, true);   // ScrollWidget::_clear() erases the window it owns
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
      WARNINGLOG("Screensaver: no PSRAM for the VU canvas (%ux%u) - keeping the clock screensaver",
                 (unsigned)box.w, (unsigned)box.h);
    }
    if (_clock) _clock->lock(false);
    return;
  }
  _ssMeterUp = true;
  lockIfChanged(_clock, true);
}

// The random spot the screensaver's clock walks to.  One definition for the timed step and for the
// placement on entry, so the two cannot disagree about the info line's strip: the floor is the content
// area (_ssContentH()), which is the panel minus that strip when the line is on, and the whole panel
// when it is off.
void Display::_screensaverClockTarget(MoveConfig& mv) {
  int32_t clockH = _clock->clockHeight();
  int32_t minTop = max((int32_t)TFT_BORDER, (int32_t)_clock->timeHeight());
  // The floor is the content area rather than the panel: the info line's strip is not the clock's to
  // cross, which is the whole reason the two bounds are derived from the same number.
  int32_t maxTop = (int32_t)_ssContentH() - clockH - TFT_BORDER;
  uint16_t ft = (maxTop > minTop) ? static_cast<uint16_t>(random(minTop, maxTop + 1)) : static_cast<uint16_t>(minTop);

  int32_t minLeft = TFT_BORDER;
  int32_t maxLeft = dsp.width() - _clock->clockWidth() - TFT_BORDER;
  int32_t left = (maxLeft > minLeft) ? random(minLeft, maxLeft + 1) : minLeft;
  if (clockConf_ptr->align == WA_CENTER) left -= (dsp.width() - _clock->clockWidth()) / 2;
  if (left < 0) left = 0;
  mv = {static_cast<uint16_t>(left), ft, 0};
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
    MoveConfig mv;
    _screensaverClockTarget(mv);
    _clock->moveTo(mv);
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
    _nums->setFieldMax(VOLUME_SCALE);
    _nums->setText(config.store.volume, numtxtFmt);
  }
}

void Display::flip() { dsp.flip(); }

// Both of these keep the last values they were handed rather than reading the station, so a rebuild that
// forgets to re-apply them leaves the badge blank until the next track.  One copy, two callers.
void Display::_applyBitrate() {
  char buf[20];
  snprintf(buf, 20, bitrateFmt, config.station.bitrate);
  if (_bitrate) _bitrate->setText(config.station.bitrate == 0 ? "" : buf);
  if (_fullbitrate) {
    _fullbitrate->setBitrate(config.station.bitrate);
    _fullbitrate->setFormat(config.configFmt);  // UNKNOWN clears the badge via _draw() → _clear()
  }
}

// Re-init all widgets with current pointer values (called after layout switch)
void Display::_reinitWidgets() {
  {
    uint16_t mfg = editorOpt(config.store.inverttitle) ? config.theme.metabg : config.theme.meta;
    uint16_t mbg;
    #ifdef DSP_TFT
      mbg = editorOpt(config.store.inverttitle) ? config.theme.background : config.theme.metabg;
    #else
      mbg = editorOpt(config.store.inverttitle) ? config.theme.meta : config.theme.metabg;
      // mbg = config.store.inverttitle ? config.theme.line : config.theme.metabg;
    #endif
    _meta->init(*metaConf_ptr, mfg, mbg);
  }
  // Title1 is optional like the rest: nothing else depends on it, and it has no other lock site.
  if (title1InLayout()) { _title1->init(*title1Conf_ptr, config.theme.title1, config.theme.background); showByLayout(_title1); }
  else hideByLayout(_title1);
  // Safe on a never-initialised clock: ClockWidget bails on !_present and guards every _fb deref.
  //
  // The clock's feature lock is applied HERE - right after init() and before any widget that shares its band is
  // drawn - rather than by the lockIfChanged() at the end of _applyState().  Two reasons, both seen on the panel.
  // Widget::init() clears _locked, so showByLayout() would DRAW a clock this state hides; and hiding it later is
  // an ERASE, which on a framebuffer is a background push over the clock's whole window - the band the VU and the
  // moved weather line share.  Erasing last punched a background rectangle through the weather (nothing repaints
  // it: hideWeatherIfChanged() returns early when the line is already visible, and ScrollWidget::setText() does
  // nothing while the string is unchanged) and left the clock sitting behind the VU whenever that push lost the
  // ordering race.  Locking here lets the weather and the VU, drawn below, land on top of the rectangle instead.
  if (clockInLayout()) {
    _clock->init(*clockConf_ptr, 0, 0);
    if (_clockHidden()) { _clock->lock(true); _clock->setActive(false); }
    else                showByLayout(_clock);
  } else hideByLayout(_clock);
  _plcurrent->init(*playlistConf_ptr, config.theme.plcurrent, config.theme.plcurrentbg);
  _plwidget->init(_plcurrent);
  _plcurrent->moveTo({TFT_BORDER, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf_ptr->width});
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
      _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis, !_vuNotDrawable);
      pages[PG_PLAYER]->addWidget(_vuwidget);
    } else {
      _vuwidget->init(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.vupeak, config.theme.background, config.theme.vuaxis, !_vuNotDrawable);
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
      _bitrate = new TextWidget(*bitrateConf_ptr, config.theme.bitrate, config.theme.background, 30);
      pages[PG_PLAYER]->addWidget(_bitrate);
    } else {
      _bitrate->init(*bitrateConf_ptr, config.theme.bitrate, config.theme.background, 30);
      showByLayout(_bitrate);
    }
  } else hideByLayout(_bitrate);
  // Creating or re-initing either widget wipes what it held, so the cached values go straight back in: without
  // this a layout change or a live preview shows an empty badge until the next track starts.
  _applyBitrate();

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
      _bufferbar = new SliderWidget(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax, config.theme.buffer);
      _footer->addWidget(_bufferbar);
    } else {
      _bufferbar->init(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax, config.theme.buffer);
      showByLayout(_bufferbar);
    }
  } else hideByLayout(_bufferbar);
  if (voltxtInLayout()) {
    if (!_voltxt) {
      _voltxt = new TextWidget(*voltxtConf_ptr, config.theme.vol, config.theme.background, 10);
      _footer->addWidget(_voltxt);
    } else {
      _voltxt->init(*voltxtConf_ptr, config.theme.vol, config.theme.background, 10);
      showByLayout(_voltxt);
    }
  } else hideByLayout(_voltxt);
  if (ipInLayout()) {
    if (!_volip) {
      _volip = new TextWidget(*iptxtConf_ptr, config.theme.ip, config.theme.background, 48);
      _footer->addWidget(_volip);
    } else {
      _volip->init(*iptxtConf_ptr, config.theme.ip, config.theme.background, 48);
      showByLayout(_volip);
    }
  } else hideByLayout(_volip);
  if (rssiInLayout()) {
    if (!_rssi) {
      _rssi = new TextWidget(*rssiConf_ptr, config.theme.rssi, config.theme.background, 20);
      _footer->addWidget(_rssi);
    } else {
      _rssi->init(*rssiConf_ptr, config.theme.rssi, config.theme.background, 20);
      showByLayout(_rssi);
    }
  } else hideByLayout(_rssi);
  if (batteryInLayout()) {
    if (!_battery) {
      _battery = new TextWidget(*batteryConf_ptr, config.theme.battery, config.theme.background, 10);
      _footer->addWidget(_battery);
    } else {
      _battery->init(*batteryConf_ptr, config.theme.battery, config.theme.background, 10);
      showByLayout(_battery);
    }
  } else hideByLayout(_battery);
  // Conditional on the zeroed-conf rule, like the clock
  if (numInLayout()) { _nums->init(*numConf_ptr, config.theme.digit, config.theme.background, 10); showByLayout(_nums); }
  else hideByLayout(_nums);
  // Background fills
  if (_plbackground) _plbackground->init(*playlBGConf_ptr, config.theme.plcurrentbg);
  #ifdef DSP_TFT
    if (_metabackground) _metabackground->init(*metaBGConf_ptr, editorOpt(config.store.inverttitle) ? config.theme.line : config.theme.metabg);
  #else
    if (_metabackground) _metabackground->init(*metaBGConf_ptr, editorOpt(config.store.inverttitle) ? config.theme.meta : config.theme.line);
  #endif
  _syncLineRule(_underline, underLineConf_ptr, true);
  _syncLineRule(_overline, overLineConf_ptr, false);
  // The fade playlist's highlight is only fitted while the fade mode is in use; the paged one ignores it.
  if (!editorOpt(config.store.playlistPages)) {
    if (_plbackground) {
      _plbackground->setHeight(_plwidget->itemHeight());
      _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf_ptr->widget.textsize*2), (int16_t)playlBGConf_ptr->width});
    }
  }
}

void Display::_setLayoutPointers() {
  layoutSanitize(&activeLayout);
  // The VU's box is resolved HERE, the one place every copy into activeLayout passes through - layoutCopy()
  // for a compiled conf or a LittleFS slot, layoutStagingCopyTo() for the editor's own push.  Resolving once,
  // at load, is what lets the widget draw what it is given and nothing re-test the numbers per frame.  It
  // edits the ACTIVE copy only, so the stored slot the editor reads keeps the values the user typed.
  _vuNotDrawable = !vuResolveBox(activeLayout);
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
  if (editorOpt(config.store.inverttitle)) {
    // No fallback: an empty metaBGConfInv is the layout declining a bar when inverted.
    metaBGConf_ptr = &activeLayout.metaBGConfInv;
  } else {
    metaBGConf_ptr = &activeLayout.metaBGConf;
  }
}

void Display::_applyState(applyMode_e mode) {
  const bool staged = (mode != APPLY_STORED);
  // A staged layout is where the editor's newest values live, and the stored id is what the device is really
  // using; taking the wrong one on a preview repaints what is already on screen.  An empty id writes
  // nothing, so activeLayout keeps its previous contents rather than going blank.
  if (mode == APPLY_STAGED_LAYOUT) layoutStagingCopyTo(&activeLayout);
  else                             layoutCopy(config.store.layoutId, &activeLayout);
  _setLayoutPointers();
  #ifdef DSP_TFT
    // A preview arrives with its colours already staged, so on that path the stored theme must NOT be
    // re-read: doing so replaces them with the very colours the panel is already showing, which is a
    // repaint of what was there rather than a preview of what the user just edited.
    if (mode == APPLY_STAGED_THEME) themeStagingCopyTo(&config.theme);
    else                            themeCopy(config.store.themeId, &config.theme);
  #endif
  _applyMetaInvert();
  _reinitWidgets();
  // Re-apply every feature lock: _reinitWidgets() may just have shown a widget the layout brought back.
  if (_vuwidget)  lockIfChanged(_vuwidget, !vuDraws());
  // Mirrors SHOWWEATHER exactly, including the shared-row suppression, so a switch cannot drop it mid-overlay.
  hideWeatherIfChanged(_weather, _weatherHidden());
  if (_bufferbar) lockIfChanged(_bufferbar, !bufferbarInLayout() || !editorOpt(config.store.bufferbar));
  // The clock's lock came with _reinitWidgets(), the only place it can be applied early enough for the hide's
  // erase to be painted over by the weather and the VU that follow it.  lockIfChanged() is still here as a guard
  // for the case where something re-showed the clock behind _reinitWidgets()' back.  The unconditional clear()
  // that used to follow is gone deliberately: it erased AFTER the page was drawn, which is the "bite" out of the
  // weather line - and with the lock applied early there is nothing left on the panel to erase.
  lockIfChanged(_clock, _clockHidden());
  _volume();
  if (_battery) _updateBattery();
  if (_weather && weatherLive() && network.weatherBuf) _weather->setText(network.weatherBuf);
  _station();
  _title();
  // A layout or a theme change has to land while the device is asleep as well, and both feed the
  // screensaver's own widgets - the meter's geometry and its palette - so the screensaver is rebuilt where
  // it stands rather than the screen being woken to the player page.
  if (config.isScreensaver) {
    // A preview is already ON the display task, so the rebuild is done right here.  Queueing it - which is
    // right for a change some other task asked for, hence the queue below - left the widget re-init above
    // painted over the picture with the screensaver only returning a pass later, which is the flash the
    // user sees as "shows the main screen, then re-enters".
    if (staged) { _enterScreensaver(_mode); return; }
    putRequest(SSREBUILD, _mode);
    return;
  }
  if (staged) {
    // The MOVEs are re-applied on this path for the same reason the locks above are: _reinitWidgets() has just
    // put the clock and the weather back at their conf positions, and a playing, metered device wants them where
    // its state's move says.  Without this a preview draws the weather line where the STOPPED layout puts it -
    // over the meter - and the next play/stop snaps it back, which reads as "my edit was reverted".
    // _switchMode() already ends with exactly this call, which is why only the staged path needs it.
    _layoutChange(player.isRunning());
    // The editor's live preview.  _switchMode() is deliberately never called: that is what leaves the mode,
    // the clock's hand-over and both screensaver countdowns exactly as they were, since a mode change would
    // reset the countdowns and cancel an overlay.
    //
    // The page the user is looking at has to be put BACK, because _reinitWidgets() above does not only
    // rebuild the objects - it re-applies their layout visibility, and those calls paint as they go.  The
    // page that is up therefore ends up with fresh player and dialog widgets drawn over it; the playlist is
    // where that is unmissable (RSSI and the buffer bar appearing over a half-drawn list).
    switch (_mode) {
      case PLAYER:
        _pager->setPage(pages[PG_PLAYER]);
        _station();
        _title();
        _volume();
        if (_battery) _updateBattery();
        // A page switch fills the panel and the IP line only repaints from setText, so it is written again
        // here - the same rule _switchMode(PLAYER) and the boot painter follow.
        if (_volip) {
          if (network.status == SDOFFLINE) {
            _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
          } else if (*shareWeatherIP_ptr && weatherLive()) {
            _volip->setText("");   // the weather owns that shared row
          } else {
            _footerIp();
          }
        }
        break;
      case STATIONS:
        // _drawPlaylist(), not the playlist widget's own draw: it also re-arms the 30-second return ticker,
        // and that is wanted here - editing while the list is up should hold it there rather than let it
        // time out mid-edit.  The scroll position is untouched, since currentPlItem is not reset.
        _pager->setPage(pages[PG_PLAYLIST]);
        _drawPlaylist();
        break;
      default:
        // The volume page, a dialog and the rest draw from the same widgets the re-init just repainted on
        // their own page, so they are already correct.  Anything else is left alone rather than
        // half-rebuilt, and that is safe: the palette is applied, so the next time that page is entered it
        // comes up in the new colours.
        break;
    }
    return;
  }
  putRequest(NEWMODE, CLEAR);
  putRequest(NEWMODE, PLAYER);
}

void Display::applyLayout(uint8_t id) {
  // Filled, not merely in range: an empty slot is a valid id with nothing behind it, and applying it
  // would leave the previous layout up under a new id.
  if (!layoutFilled(id)) return;
  config.store.layoutId = id;
  _applyState();
}

uint8_t Display::getLayoutCount() { return layoutCount(); }

void Display::applyTheme(uint8_t id) {
  // Filled, not merely in range: an empty custom slot is a valid id with nothing behind it, and
  // applying it would leave the previous colours up under a new id.
  if (!themeFilled(id)) return;
  config.store.themeId = id;
  _applyState();
}

uint8_t Display::getThemeCount() { return themeCount(); }

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

void Display::_buildThemeListJson() {
  // One entry per FILLED id, and the keys are the real ids rather than positions.  A custom slot with
  // no file behind it is therefore simply absent, with no gap and no placeholder: options.js builds
  // the select from the object's entries and uses each key as the option value, so a sparse list
  // needs no change on that side at all.
  String json = "{";
  bool first = true;
  for (uint8_t i = 0; i < themeCount(); i++) {
    #if !defined(DSP_TFT)
      if (i >= THEME_BUILTIN_COUNT) break;
    #endif
    if (!themeFilled(i)) continue;
    if (!first) json += ',';
    first = false;
    json += '"' + String(i) + "\":\"";
    char buf[33];
    strncpy_P(buf, themeName(i), 32);
    buf[32] = 0;
    json += buf;
    json += '"';
  }
  json += '}';
  // One assignment, so a request reading the cache never sees a half-built list.
  _themeListJson = json;
  _themeListGen = themeGeneration();
}

void Display::_buildLayoutListJson() {
  // One entry per FILLED id, keyed by the real id: a custom slot with no file behind it is simply
  // absent, so the WebUI builds the select from the object's entries with no placeholder.
  String json = "{";
  bool first = true;
  for (uint8_t i = 0; i < layoutCount(); i++) {
    if (!layoutFilled(i)) continue;
    if (!first) json += ',';
    first = false;
    json += '"' + String(i) + "\":\"";
    char buf[33];
    snprintf(buf, sizeof(buf), "%s", layoutName(i));
    json += buf;
    json += '"';
  }
  json += '}';
  _layoutListJson = json;
  _layoutListGen = layoutGeneration();
}

void Display::_buildJsonCache() {
  _buildThemeListJson();

  // Build layout list JSON once
  _buildLayoutListJson();
  
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
  
  String Display::getThemeListJson() {
    // The registry can move under this cache: the slot files are read after the first build, and the
    // editor can save or delete a slot at any time.  The generation says when to rebuild, so no
    // caller has to know it changed.
    if (_themeListGen != themeGeneration()) _buildThemeListJson();
    return _themeListJson;
  }
  String Display::getLayoutListJson() {
    // The registry can move under this cache once the slot files are read, so the generation decides
    // when to rebuild - the same rule the theme list follows.
    if (_layoutListGen != layoutGeneration()) _buildLayoutListJson();
    return _layoutListJson;
  }
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

