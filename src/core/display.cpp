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
#include "utility.h"
#include "backlightcontrols.h"
#include "rgbled.h"
#include "../locale/dsplocale.h"
#include "../displays/dspcore.h"
#include "../displays/themes.h"
#include "../displays/widgets/pages.h"
#include "../displays/widgets/widgets.h"
#include "battery.h"

extern const char batterytxtFmt[] PROGMEM;

#ifndef IP_WEATHER_SHARED
  #define IP_WEATHER_SHARED false
#endif
#ifndef RSSI_BATT_SHARED
  #define RSSI_BATT_SHARED false
#endif

Display display;

// Layout switching — pointers initially point to PROGMEM defaults
LayoutData activeLayout;
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
const bool*           boomboxStyle_ptr    = &activeLayout.boomboxStyle;
uint8_t layoutCount = (sizeof(_layoutNames) / sizeof(_layoutNames[0]));

QueueHandle_t displayQueue;

#ifdef CORE_MONITOR
  volatile uint32_t cmDspLoopCount = 0;
#endif

TaskHandle_t dspTaskHandle = NULL;

static void loopDspTask(void * pvParameters) {
  while(true) {
    #ifndef DUMMYDISPLAY
      if (displayQueue==NULL) break;
      display.loop();
    #endif
    #ifdef CORE_MONITOR
      cmDspLoopCount++;
    #endif
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
  dsp.setFont((GFXfont *)&DisplayFont);
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

void Display::_bootScreen() {
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(_bootConfig.bootWdtConf, _bootConfig.bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget*) &_boot->addWidget(new TextWidget(_bootConfig.bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  const char* icon;
  if ((network.offlineMode || config.store.SDoffline))
                                         icon = "\030\031";  // SD_A + SD_B
  else if (!config.store.lastBootGood)   icon = "\034";      // PAUSE (safe mode)
  else if (config.store.smartstart)      icon = "\035";      // PLAY (smart start)
  else                                   icon = "\026";      // VOL_75 (default)
  char buf[64];
  snprintf(buf, sizeof(buf), "\023 %s %s", RADIOVERSION, icon);
  _bootstring->setText(buf);
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  dsp.drawLogo(BOOTLOGOTOP);
  _bootStep = 1;
}

void Display::_buildPager() {
  if (title2Conf_ptr->buffsize > 0) {
    _title2 = new ScrollWidget("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
  }
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    _plbackground = new FillWidget(*playlBGConf_ptr, config.theme.plcurrentfill);
    _metabackground = new FillWidget(*metaBGConf_ptr, config.theme.metafill);
  #endif
  #if DSP_MODEL==DSP_NOKIA5110
    _plbackground = new FillWidget(*playlBGConf_ptr, 1);
    //_metabackground = new FillWidget(*metaBGConf_ptr, 1);
  #endif
  if (vuConf_ptr->textsize > 0) {
    _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.background);
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
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if (_title2) pages[PG_PLAYER]->addWidget(_title2);
  if (_weather) pages[PG_PLAYER]->addWidget(_weather);
  if (fullbitrateConf_ptr->dimension > 0) {
    _fullbitrate = new BitrateWidget(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_fullbitrate);
  } else {
    _bitrate = new TextWidget(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_bitrate);
  }
  if (_vuwidget) pages[PG_PLAYER]->addWidget(_vuwidget);
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);

  if (_metabackground) pages[PG_DIALOG]->addWidget(_metabackground);
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);
  #ifdef UPDATEURL
    // configure centred update label and progress bar with appropriate gap
    WidgetConfig updConf = _bootConfig.apNameConf;
    updConf.left = 0;
    updConf.align = WA_CENTER;
    updConf.top = (dsp.height() - (updConf.textsize * CHARHEIGHT)) / 2;
    updConf.top = max<int16_t>(0, updConf.top - CHARHEIGHT); // lift by one text line

    _updLabel = new TextWidget(updConf, 30, false,
                               config.theme.title1, config.theme.background);
    // remember config for later adjustments
    _updConf = updConf;
    MoveConfig mvLabel{0, updConf.top, (int16_t)dsp.width()};
    _updLabel->moveTo(mvLabel);

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
    if (origGap < 0) origGap = updConf.textsize * CHARHEIGHT + 2; // fallback
    valConf.top = updConf.top + origGap;
    _updValue = new TextWidget(valConf, (uint16_t)(_updBarWidth + 2), false,
                               config.theme.clock, config.theme.background);
    MoveConfig mvValue{0, valConf.top, (int16_t)dsp.width()};
    _updValue->moveTo(mvValue);
    pages[PG_DIALOG]->addWidget(_updLabel);
    pages[PG_DIALOG]->addWidget(_updValue);
  #endif
  
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    pages[PG_DIALOG]->addPage(_footer);
  #endif
  #if !defined(DSP_LCD) && !PLAYLIST_MODE_PAGED
    if (_plbackground) {
      pages[PG_PLAYLIST]->addWidget(_plbackground);
      _plbackground->setHeight(_plwidget->itemHeight());
      _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf_ptr->widget.textsize*2), (int16_t)playlBGConf_ptr->width});
    }
    pages[PG_PLAYLIST]->addWidget(_plcurrent);
  #endif
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for(const auto& p: pages) _pager->addPage(p);
}

void Display::_apScreen() {
  if (_boot) _pager->removePage(_boot);
  #ifndef DSP_LCD
    _boot = new Page();
    #if DSP_MODEL!=DSP_NOKIA5110
      _boot->addWidget(new FillWidget(*metaBGConf_ptr, config.theme.metafill));
    #endif
    uint16_t mfg = config.store.inverttitle ? config.theme.metabg : config.theme.meta;
    uint16_t mbg;
    #ifdef DSP_TFT
      mbg = config.store.inverttitle ? config.theme.background : config.theme.metabg;
    #else
      mbg = config.store.inverttitle ? config.theme.metafill : config.theme.metabg;
    #endif
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", _bootConfig.apTitleConf, mfg, mbg));
    bootTitle->setText("AP/Improv Mode");
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
  #else
    dsp.apScreen();
  #endif
}

void Display::_start() {
  if (_boot) _pager->removePage(_boot);
  if (network.status != CONNECTED && network.status != SDOFFLINE) {
    _apScreen();
      _bootStep = 2;
    return;
  }
  _buildPager();
  _mode = PLAYER;
  _applyState();
  config.setTitle(network.status == SDOFFLINE && !sdman.ready ? l10n(L10N_MSG_NO_SD_CARD) : l10n(L10N_MSG_READY));
  
  if (_bufferbar)  _bufferbar->lock(!config.store.bufferbar);
  
  if (_weather)  _weather->lock(!config.store.showweather);
  if (_weather && config.store.showweather && network.status != SDOFFLINE) network.buildWeatherString();

  if (_clock && network.status == SDOFFLINE && !config.isRTCFound()) {
    _clock->lock(true);   // prevent redraws from CLOCK → _time()
    _clock->clear();       // erase current display
  }

  if (_vuwidget) _vuwidget->lock();
  if (_rssi) { if (network.status == SDOFFLINE) _setRSSI(0); else _setRSSI(WiFi.RSSI()); }
  #if RSSI_BATT_SHARED
    if (_battery && _rssi) {
      bool haveBattery = battery.isInitialized();
      #ifdef BATTERY_FORCE_DISPLAY
        haveBattery = true;
      #endif
      if (haveBattery) {
        _rssi->setText(""); _rssi->setActive(false);
        _battery->setActive(true); _updateBattery();
      } else {
        _battery->setText(""); _battery->setActive(false);
        _rssi->setActive(true);
      }
    }
  #endif
  if (iptxtConf_ptr->textsize > 0) {
    if (_volip) {
      if (network.status == SDOFFLINE) {
        _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
      } else {
        #if IP_WEATHER_SHARED
          if (config.store.showweather) _volip->setText("");
          else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
        #else
          _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
        #endif
      }
    }
  }
  if (batteryConf_ptr->textsize > 0) {
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
  #ifdef META_MOVE
    _meta->moveTo(metaMove);
  #endif
  _meta->setAlign(WA_CENTER);
  _meta->setText(title);
}

void Display::_setReturnTicker(uint8_t time_s) {
  _returnTicker.detach();
  _returnTicker.once(time_s, returnPlayer);
}

void Display::_swichMode(displayMode_e newmode) {
  if (newmode == CLEAR) { dsp.fillScreen(config.theme.background); _mode = CLEAR; return; }
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDOFFLINE)) return;
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER) {
    if (player.isRunning()){
      if (config.store.vumeter && _vuwidget) {
        if (clockMove_ptr->width<0) _clock->moveBack(); else _clock->moveTo(*clockMove_ptr);
        if (_weather) _weather->moveTo(*weatherMoveVU_ptr);
      } else {
        _clock->moveBack();  // restore from screensaver position
        if (_weather) _weather->moveTo(*weatherMove_ptr);
      }
    } else {
      _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
    #ifdef DSP_LCD
      dsp.clearDsp();
    #endif
    numOfNextStation = 0;
    #ifdef META_MOVE
      _meta->moveBack();
    #endif
    _meta->setAlign(metaConf_ptr->widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER]);
    if (_volip) {
        if (network.status == SDOFFLINE) {
          _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
        } else {
          #if IP_WEATHER_SHARED // weather and IP share the same bottom row; hide IP when weather is active
            if (config.store.showweather) _volip->setText("");
            else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
          #else
            _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
          #endif
        }
      }
    #if IP_WEATHER_SHARED // force weather repaint on return to PLAYER; larger displays repaint naturally
      if (config.store.showweather && _weather) {
        _weather->lock(false);
        // Force a clean repaint of the shared weather/IP row after overlays like VOL/SCREENSAVER.
        _weather->setText("");
        if (network.weatherBuf) _weather->setText(network.weatherBuf);
      }
    #endif
    #if RSSI_BATT_SHARED
      if (_battery && _rssi) {
        bool haveBattery = battery.isInitialized();
        #ifdef BATTERY_FORCE_DISPLAY
          haveBattery = true;
        #endif
        if (haveBattery) {
          _rssi->setText(""); _rssi->setActive(false);
          _battery->setActive(true); _updateBattery();
        } else {
          _battery->setText(""); _battery->setActive(false);
          _rssi->setActive(true);
        }
      }
    #endif
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
    #if IP_WEATHER_SHARED // weather and IP share the same bottom row; pause weather so VOL can show IP
      if (config.store.showweather && _weather) {
        // Pause weather updates while volume UI is active to avoid shared-line collisions.
        _weather->lock(true);
        _weather->setText("");
      }
    #endif
    #if RSSI_BATT_SHARED
      if (_battery && _rssi) {
        _battery->setText(""); _battery->setActive(false);
        _rssi->setActive(true);
      }
    #endif
    if (config.store.volumepage) {
      _showDialog(l10n(L10N_LBL_VOLUME));
    }
    if (_volip) {
        if (network.status == SDOFFLINE) _volip->setText(utf8_trim15(l10n(L10N_MSG_OFFLINE_15CHAR)), "\030\031%s");
        else _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
      }
    _nums->setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST)      _showDialog(l10n(L10N_LBL_LOST));
  if (newmode == UPDATING)  { _showDialog(l10n(L10N_LBL_UPDATE));
    #ifdef UPDATEURL
      _updFirstCall = true;
    #endif
  }
  if (newmode == SLEEPING)  _showDialog("SLEEPING");
  if (newmode == SDCHANGE)  _showDialog(l10n(L10N_LBL_WAITFORSD));
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog("");
  if (newmode == NUMBERS) _showDialog("");
  if (newmode == STATIONS) {
    _pager->setPage(pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    #if PLAYLIST_MODE_PAGED
    _plwidget->resetState();
    #endif
    _drawPlaylist();
  }
  
}

void Display::resetQueue() {
  if (displayQueue!=NULL) xQueueReset(displayQueue);
}

void Display::_drawPlaylist() {
  if (currentPlItem < 1) currentPlItem = 1;
  //dsp.drawPlaylist(currentPlItem);
  _plwidget->drawPlaylist(currentPlItem);
  _setReturnTicker(30);
}

void Display::_drawNextStationNum(uint16_t num) {
  _setReturnTicker(30);
  _meta->setText(utility.stationByNum(num));
  _nums->setText(num, "%d");
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (displayQueue==NULL) return;
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
      // adjust textsize so label fits within display width
      WidgetConfig conf = _updConf;  // use stored copy instead of protected member
      // start from current size and shrink until it fits or reaches 1
      for (uint8_t ts = conf.textsize; ts > 1; ts--) {
        // compute width for this size
        uint16_t w = 0;
        uint8_t charW = ts * CHARWIDTH;
        for (const char *p = label; *p; ++p) {
          uint8_t b = (unsigned char)*p;
          if (b == (uint8_t)DSP_PIXEL_SPACER) w += 2;    // 2-px spacer glyph
          else if ((b & 0xC0) != 0x80) w += charW;       // skip UTF-8 continuation bytes (0x80–0xBF)
        }
        if (w <= dsp.width()) {
          if (ts != conf.textsize) {
            conf.textsize = ts;
            _updLabel->init(conf, 30, false, config.theme.title1, config.theme.background);
            MoveConfig mv2 = {0, conf.top, (int16_t)dsp.width()};
            _updLabel->moveTo(mv2);
            _updConf = conf; // update stored config
          }
          break;
        }
      }
      _updLabel->setText(label);
    }
    if (_updValue) {
      int bars = (int)(progress * _updBarWidth + 0.5f);
      if (bars < 0) bars = 0;
      if (bars > _updBarWidth) bars = _updBarWidth;
      const char barChar = '\x10'; // play icon is progress
      char buf[68];
      memset(buf, barChar, bars);
      memset(buf + bars, ' ', _updBarWidth - bars); // empty space is empty space
      buf[_updBarWidth] = '\0';
      _updValue->setText(buf);
    }
  #endif
}

void Display::_layoutChange(bool played) {
  if (config.store.vumeter && _vuwidget) {
    if (played) {
      if (_vuwidget) _vuwidget->unlock();
      //_clock->moveTo(*clockMove_ptr);
      if (clockMove_ptr->width<0) _clock->moveBack(); else _clock->moveTo(*clockMove_ptr);
      if (_weather) _weather->moveTo(*weatherMoveVU_ptr);
    } else {
      if (_vuwidget) if (!_vuwidget->locked()) _vuwidget->lock();
      _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
  } else {
    if (played) {
      _clock->moveBack();  // restore clock from VU-shifted position
      if (_weather) _weather->moveTo(*weatherMove_ptr);
      //_clock->moveBack();
    } else {
      if (_weather) _weather->moveBack();
      _clock->moveBack();
    }
  }
}

void Display::loop() {
  if (_bootStep==0) {
    _pager->begin();
    _bootScreen();
    return;
  }
  if (_bootStep==2) {
    if (config.isScreensaver) {
      if (config.displayIsInverted) {
        config.displayIsInverted = false;
        display.invert();
      }
    } else {
      if (config.store.invertdisplay != config.displayIsInverted) {
        config.displayIsInverted = config.store.invertdisplay;
        display.invert();
      }
    }
  }
  if (displayQueue==NULL || _locked) return;
  _pager->loop();
  requestParams_t request;
  if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)) {
    switch (request.type) {
        case NEWMODE: _swichMode((displayMode_e)request.payload); break;
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
            _bufferbar->lock(!config.store.bufferbar);
            _bufferbar->setValue(normalizeBufferbarValue(player.inBufferFilled(), _bufferbarMax));
          }
          break;
        case SHOWVUMETER: {
          if (_vuwidget) {
            _vuwidget->lock(!config.store.vumeter); 
            _layoutChange(player.isRunning());
          }
          break;
        }
        case SHOWWEATHER: {
          #if IP_WEATHER_SHARED // also lock weather during VOL to prevent shared-row collision with IP
            if (_weather) _weather->lock(!config.store.showweather || _mode == VOL);
          #else
            if (_weather) _weather->lock(!config.store.showweather);
          #endif
          if (!config.store.showweather) {
            if (_weather) _weather->setText("");
            if (_volip) _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
          } else {
            #if IP_WEATHER_SHARED // weather and IP share a row; suppress weather text and IP together based on mode
              if (_mode == VOL) {
                if (_weather) _weather->setText("");
              } else {
                if (_volip) _volip->setText("");
                network.buildWeatherString();
              }
	          #else // larger displays have separate rows; just update weather, leave IP alone
              network.buildWeatherString();
            #endif
          }
          break;
        }
        case NEWWEATHER: {
          #if IP_WEATHER_SHARED // skip weather repaint during VOL to avoid overwriting the IP shown there
            if (_mode != VOL && _weather && network.weatherBuf) _weather->setText(network.weatherBuf);
          #else
            if (_weather && network.weatherBuf) _weather->setText(network.weatherBuf);
          #endif
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
                #if IP_WEATHER_SHARED // skip IP repaint in PLAYER when weather owns the shared row
                  if (!(_mode == PLAYER && config.store.showweather))
                #endif
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
  #if RSSI_DIGIT
    _rssi->setText(rssi, rssiFmt);
    return;
  #endif
  char rssiG[3];
  int rssi_steps[] = {RSSI_STEPS};
  if (rssi >= rssi_steps[0]) strlcpy(rssiG, "\004\006", 3);
  if (rssi >= rssi_steps[1] && rssi < rssi_steps[0]) strlcpy(rssiG, "\004\005", 3);
  if (rssi >= rssi_steps[2] && rssi < rssi_steps[1]) strlcpy(rssiG, "\004\002", 3);
  if (rssi >= rssi_steps[3] && rssi < rssi_steps[2]) strlcpy(rssiG, "\003\002", 3);
  if (rssi <  rssi_steps[3] || rssi >=  0) strlcpy(rssiG, "\001\002", 3);
  _rssi->setText(rssiG);
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
  //   BATTERY_00=\013, BATTERY_10=\015, BATTERY_11=\016
  //   BATTERY__00=\014, BATTERY__10=\017, BATTERY__11=\020
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
  _title1->init("*", *title1Conf_ptr, config.theme.title1, config.theme.background);
  _clock->init(*clockConf_ptr, 0, 0);
  #if DSP_MODEL==DSP_NOKIA5110
    _plcurrent->init("*", *playlistConf_ptr, 0, 1);
  #else
    _plcurrent->init("*", *playlistConf_ptr, config.theme.plcurrent, config.theme.plcurrentbg);
  #endif
  _plwidget->init(_plcurrent);
  #if !defined(DSP_LCD)
    _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf_ptr->width});
  #endif
  // --- Player-page optional widgets (lazy-create if newly enabled) ---
  if (title2Conf_ptr->buffsize > 0) {
    if (!_title2) {
      _title2 = new ScrollWidget("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
      pages[PG_PLAYER]->addWidget(_title2);
    } else _title2->init("*", *title2Conf_ptr, config.theme.title2, config.theme.background);
  }
  if (vuConf_ptr->textsize > 0) {
    if (!_vuwidget) {
      _vuwidget = new VuWidget(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.background);
      pages[PG_PLAYER]->addWidget(_vuwidget);
    } else _vuwidget->init(*vuConf_ptr, *bandsConf_ptr, config.theme.vumax, config.theme.vumin, config.theme.background);
  }
  if (weatherConf_ptr->buffsize > 0) {
    if (!_weather) {
      _weather = new ScrollWidget("~", *weatherConf_ptr, config.theme.weather, config.theme.background);
      pages[PG_PLAYER]->addWidget(_weather);
    } else _weather->init("~", *weatherConf_ptr, config.theme.weather, config.theme.background);
  }
  if (fullbitrateConf_ptr->dimension > 0) {
    if (!_fullbitrate) {
      if (_bitrate) { pages[PG_PLAYER]->removeWidget(_bitrate); delete _bitrate; _bitrate = nullptr; }
      _fullbitrate = new BitrateWidget(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
      pages[PG_PLAYER]->addWidget(_fullbitrate);
    } else _fullbitrate->init(*fullbitrateConf_ptr, config.theme.bitrate, config.theme.background);
  } else {
    if (!_bitrate) {
      if (_fullbitrate) { pages[PG_PLAYER]->removeWidget(_fullbitrate); delete _fullbitrate; _fullbitrate = nullptr; }
      _bitrate = new TextWidget(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
      pages[PG_PLAYER]->addWidget(_bitrate);
    } else _bitrate->init(*bitrateConf_ptr, 30, false, config.theme.bitrate, config.theme.background);
  }

  // --- Footer widgets (lazy-create if newly enabled) ---
  if (volbarConf_ptr->height > 0) {
    if (!_volbar) {
      _volbar = new SliderWidget(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
      _footer->addWidget(_volbar);
    } else _volbar->init(*volbarConf_ptr, config.theme.volbarin, config.theme.background, VOLUME_SCALE, config.theme.volbarout);
  }
  if (bufferbarConf_ptr->height > 0) {
    _bufferbarMax = 1024 * BUFFERBAR_VISUAL_FULL_KB;
    if (!_bufferbar) {
      _bufferbar = new SliderWidget(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax);
      _footer->addWidget(_bufferbar);
    } else _bufferbar->init(*bufferbarConf_ptr, config.theme.buffer, config.theme.background, _bufferbarMax);
  }
  if (voltxtConf_ptr->textsize > 0) {
    if (!_voltxt) {
      _voltxt = new TextWidget(*voltxtConf_ptr, 10, false, config.theme.vol, config.theme.background);
      _footer->addWidget(_voltxt);
    } else _voltxt->init(*voltxtConf_ptr, 10, false, config.theme.vol, config.theme.background);
  }
  if (iptxtConf_ptr->textsize > 0) {
    if (!_volip) {
      _volip = new TextWidget(*iptxtConf_ptr, 48, false, config.theme.ip, config.theme.background);
      _footer->addWidget(_volip);
    } else _volip->init(*iptxtConf_ptr, 48, false, config.theme.ip, config.theme.background);
  }
  if (rssiConf_ptr->textsize > 0) {
    if (!_rssi) {
      _rssi = new TextWidget(*rssiConf_ptr, 20, false, config.theme.rssi, config.theme.background);
      _footer->addWidget(_rssi);
    } else _rssi->init(*rssiConf_ptr, 20, false, config.theme.rssi, config.theme.background);
  }
  if (batteryConf_ptr->textsize > 0) {
    if (!_battery) {
      _battery = new TextWidget(*batteryConf_ptr, 10, false, config.theme.battery, config.theme.background);
      _footer->addWidget(_battery);
    } else _battery->init(*batteryConf_ptr, 10, false, config.theme.battery, config.theme.background);
  }
  _nums->init(*numConf_ptr, 10, false, config.theme.digit, config.theme.background);
  // Background fills
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    if (_plbackground) _plbackground->init(*playlBGConf_ptr, config.theme.plcurrentfill);
    if (_metabackground) _metabackground->init(*metaBGConf_ptr, config.theme.metafill);
  #endif
  #if DSP_MODEL==DSP_NOKIA5110
    if (_plbackground) _plbackground->init(*playlBGConf_ptr, 1);
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
}

void Display::_applyState() {
  memcpy_P(&activeLayout, &_layouts[config.store.layoutId], sizeof(LayoutData));
  _setLayoutPointers();
  #ifdef DSP_TFT
    memcpy_P(&config.theme, &_themes[config.store.themeId], sizeof(ThemeData));
  #endif
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    if (config.store.inverttitle) {
      #ifdef DSP_TFT
        config.theme.metafill = config.theme.div;
      #endif
      metaBGConf_ptr = (activeLayout.metaBGConfInv.height > 0) ? &activeLayout.metaBGConfInv : &activeLayout.metaBGConf;
    } else {
      metaBGConf_ptr = &activeLayout.metaBGConf;
    }
  #endif
  _reinitWidgets();
  if (_vuwidget && !config.store.vumeter) _vuwidget->lock();  // keep VU off when disabled
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

String Display::getThemeListJson() {
  String json = "{";
  for (uint8_t i = 0; i < sizeof(_themes)/sizeof(_themes[0]); i++) {
    if (i > 0) json += ',';
    json += '"' + String(i) + "\":\"";
    char buf[33];
    strncpy_P(buf, _themeNames[i], 32);
    buf[32] = 0;
    json += buf;
    json += '"';
  }
  json += '}';
  return json;
}

String Display::getLayoutListJson() {
  String json = "{";
  for (uint8_t i = 0; i < layoutCount; i++) {
    if (i > 0) json += ',';
    json += '"' + String(i) + "\":\"";
    char buf[33];
    strncpy_P(buf, _layoutNames[i], 32);
    buf[32] = 0;
    json += buf;
    json += '"';
  }
  json += '}';
  return json;
}

void Display::applyInvertTitle() {
  _applyState();
}

void Display::invert() { dsp.invert(); }

void  Display::setContrast() {
  #if DSP_MODEL==DSP_NOKIA5110
    dsp.setContrast(config.store.contrast);
  #endif
}

bool Display::deepsleep() {
#if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
  dsp.sleep();
  return true;
#endif
  return false;
}

void Display::wakeup() {
  #if defined(LCD_I2C) || defined(DSP_OLED) || BRIGHTNESS_PIN!=255
    dsp.wake();
  #endif
}

#else // ============================== DUMMYDISPLAY Begins ==============================

void Display::init() {
  _activeLocale = l10n_findLocale(config.store.locale_display);
  _createDspTask();
}
void Display::_start() {
  config.setTitle(network.status == SDOFFLINE && !sdman.ready ? l10n(L10N_MSG_NO_SD_CARD) : l10n(L10N_MSG_READY));
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (type==DSP_START) _start();
}

#endif // ============================== DUMMYDISPLAY Ends ==============================

