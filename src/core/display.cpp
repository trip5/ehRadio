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
const bool*           boomboxVU_ptr       = &activeLayout.boomboxVU;
const bool*           rotateVU_ptr        = &activeLayout.rotateVU;
// Point into activeLayout (the memcpy_P target), so no re-pointing on a layout switch.
const bool*           shareWeatherIP_ptr  = &activeLayout.shareWeatherIP;
const bool*           shareBattRSSI_ptr   = &activeLayout.shareBattRSSI;
const bool*           rssiDigit_ptr       = &activeLayout.rssiDigit;
const FillConfig*     underLineConf_ptr   = &_layouts[0].underLineConf;
const FillConfig*     overLineConf_ptr    = &_layouts[0].overLineConf;
uint8_t layoutCount = (sizeof(_layoutNames) / sizeof(_layoutNames[0]));

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

// Every MOVE goes through one of these, so no call site can forget the `{ }` rule (declared above
// _switchMode() because C++ needs them first).  `{ }` yields to the VU; width < 0 means "the conf's own
// position" and needs the restore, since moveTo() ignores a negative width.
static inline bool moveZeroed(const MoveConfig& m) { return m.x == 0 && m.y == 0 && m.width == 0; }
static inline void applyMove(Widget* w, const MoveConfig& m) {
  if (!w || moveZeroed(m)) return;
  w->moveTo(m);
}
static inline void applyMoveOrRestore(Widget* w, const MoveConfig& m) {
  if (!w || moveZeroed(m)) return;
  if (m.width < 0) w->moveBack(); else w->moveTo(m);
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
  _bootstring = (TextWidget*) &_boot->addWidget(new ScrollWidget(" ", bootScroll, BOOT_TXT_COLOR, 0));
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
    _title2 = new ScrollWidget("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
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
    _weather = new ScrollWidget("~", *weatherConf_ptr, config.theme.weather, config.theme.background);
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
    _updLabel = new ScrollWidget("  ", updConf,
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
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", _bootConfig.apTitleConf, mfg, mbg));
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
    ScrollWidget *bootSett = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", _bootConfig.apSettConf, config.theme.title2, config.theme.background));
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
  ScrollWidget *sdTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", _bootConfig.apTitleConf, mfg, mbg));
  sdTitle->setText(l10n(L10N_LBL_SDMAN));
  ScrollWidget *sdUrl = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", _bootConfig.apSettConf, config.theme.title2, config.theme.background));
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
  _buildPager();
  _mode = PLAYER;
  _applyState();
  #ifdef USE_SD
    config.setTitle(network.status == SDOFFLINE && !sdman.ready ? l10n(L10N_MSG_NO_SD_CARD) : l10n(L10N_MSG_READY));
  #else
    config.setTitle(l10n(L10N_MSG_READY));
  #endif
  
  if (_bufferbar)  _bufferbar->lock(!bufferbarInLayout() || !config.store.bufferbar);
  
  lockIfChanged(_weather, _weatherHidden());
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
        applyMoveOrRestore(_clock, *clockMove_ptr);
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
    _meta->setAlign(metaConf_ptr->widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER]);
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
    _pager->setPage(pages[PG_SCREENSAVER], true);
    if (newmode == SCREENBLANK) {
      //dsp.clearClock();
      _clock->clear();
      config.setDspOn(false, false);
    }
  } else {
    config.screensaverTicks=SCREENSAVERSTARTUPDELAY;
    config.screensaverPlayingTicks=SCREENSAVERSTARTUPDELAY;
    config.isScreensaver = false;
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
  xQueueSend(displayQueue, &request, pdMS_TO_TICKS(DSQ_SEND_DELAY));
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
// _layoutChange() redraws the full clock when the mode leaves.
bool Display::_clockHidden() {
  const bool noTimeSource = (network.status == SDOFFLINE && !config.isRTCFound());
  const bool yieldsToVU   = (config.store.vumeter && vuInLayout() && player.isRunning() && moveZeroed(*clockMove_ptr));
  if (_ownScreen()) return true;
  return noTimeSource || yieldsToVU || !clockInLayout();
}

// Same shape for the weather, plus shared-row suppression during the volume overlay.
bool Display::_weatherHidden() {
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
      applyMoveOrRestore(_clock, *clockMove_ptr);
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
  // Lock state last, from one definition.  lock() erases, so a yielded widget really disappears.
  const bool clockWasHidden = (_clock && _clock->locked());
  const bool weatherWasHidden = (_weather && _weather->locked());
  lockIfChanged(_clock, _clockHidden());
  lockIfChanged(_weather, _weatherHidden());
  if (clockWasHidden)   redrawIfVisible(_clock);     // full _printClock(true), not just the seconds
  if (weatherWasHidden) redrawIfVisible(_weather);
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
          lockIfChanged(_weather, _weatherHidden());
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
          break;
        }
        case NEWWEATHER: {
          // skip weather repaint during VOL to avoid overwriting the IP shown there
          if ((!*shareWeatherIP_ptr || _mode != VOL) && _weather && network.weatherBuf)
            _weather->setText(network.weatherBuf);
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

void Display::_title() {
  if (strlen(config.station.title) > 0) {
    char tmpbuf[strlen(config.station.title)+1];
    strlcpy(tmpbuf, config.station.title, strlen(config.station.title)+1);
    char *stitle = split(tmpbuf, " - ");
    if (stitle && _title2) {
      _title1->setText(tmpbuf);
      _title2->setText(stitle);
    } else {
      _title1->setText(config.station.title);
      if (_title2) _title2->setText("");
    }
    
  } else {
    _title1->setText("");
    if (_title2) _title2->setText("");
  }
  rgbled.trackChange();
  backlightControls.restart();
}

void Display::_time(bool redraw) {
  
  #if LIGHT_SENSOR!=255
    if (config.store.dspon) {
      config.store.brightness = AUTOBACKLIGHT(analogRead(LIGHT_SENSOR));
      config.setBrightness();
    }
  #endif
  if (config.isScreensaver && network.timeinfo.tm_sec % SCREENSAVERMOVE == 0) {
    int32_t clockH = _clock->clockHeight();
    int32_t minTop = max((int32_t)TFT_FRAMEWDT, (int32_t)_clock->timeHeight());
    int32_t maxTop = dsp.height() - clockH - TFT_FRAMEWDT;
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
    _meta->init("*", *metaConf_ptr, mfg, mbg);
  }
  // Title1 is optional like the rest: nothing else depends on it, and it has no other lock site.
  if (title1InLayout()) { _title1->init("*", *title1Conf_ptr, config.theme.title1, config.theme.background); showByLayout(_title1); }
  else hideByLayout(_title1);
  // Safe on a never-initialised clock: ClockWidget bails on !_present and guards every _fb deref.
  if (clockInLayout()) { _clock->init(*clockConf_ptr, 0, 0); showByLayout(_clock); }
  else hideByLayout(_clock);
  _plcurrent->init("*", *playlistConf_ptr, config.theme.plcurrent, config.theme.plcurrentbg);
  _plwidget->init(_plcurrent);
  _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf_ptr->width});
  // --- Player-page optional widgets (lazy-create if newly enabled) ---
  if (title2InLayout()) {
    if (!_title2) {
      _title2 = new ScrollWidget("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
      pages[PG_PLAYER]->addWidget(_title2);
    } else {
      _title2->init("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
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
      _weather = new ScrollWidget("~", *weatherConf_ptr, config.theme.weather, config.theme.background);
      pages[PG_PLAYER]->addWidget(_weather);
    } else {
      _weather->init("~", *weatherConf_ptr, config.theme.weather, config.theme.background);
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
  lockIfChanged(_weather, _weatherHidden());
  if (_bufferbar) _bufferbar->lock(!bufferbarInLayout() || !config.store.bufferbar);
  // The clock's feature lock must be re-applied too, or a `{}` clockMove layout stays visible.
  lockIfChanged(_clock, _clockHidden());
  if (_clock && _clock->locked()) _clock->clear();
  _volume();
  if (_battery) _updateBattery();
  if (_weather && config.store.showweather && network.weatherBuf) _weather->setText(network.weatherBuf);
  _station();
  _title();
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
}

String Display::getThemeListJson() { return _themeListJson; }
String Display::getLayoutListJson() { return _layoutListJson; }

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

