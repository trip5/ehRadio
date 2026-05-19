#include "options.h"
#include <time.h>
#include <Arduino.h>
#include <Ticker.h>
#include <WiFi.h>
#include "config.h"
#include "display.h"
#include "logging.h"
#include "locale.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "utility.h"
#include "backlightcontrols.h"
#include "rgbled.h"
#include "../displays/dspcore.h"
#include "../displays/widgets/pages.h"
#include "../displays/widgets/widgets.h"
#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
  #include "battery.h"
#endif
#ifdef USE_NEXTION
  #include "../displays/nextion.h"
#endif

Display display;
#ifdef USE_NEXTION
  Nextion nextion;
#endif

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

#if !((DSP_MODEL==DSP_ST7735 && DTYPE==INITR_BLACKTAB) || DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7796 || DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486 || DSP_MODEL==DSP_ILI9341 || DSP_MODEL==DSP_ILI9225)
  #undef  BITRATE_FULL
  #define BITRATE_FULL     false
#endif


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
  #ifdef USE_NEXTION
    nextion.begin();
  #endif
  #if LIGHT_SENSOR!=255
    analogSetAttenuation(ADC_0db);
  #endif
  dsp.initDisplay();
  displayQueue=NULL;
  displayQueue = xQueueCreate(5, sizeof(requestParams_t));
  if (displayQueue==NULL) { log_e("[display] displayQueue alloc failed — rebooting"); ESP.restart(); }
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
  SERIALLOG("done");
}

uint16_t Display::width() { return dsp.width(); }
uint16_t Display::height() { return dsp.height(); }
#if TIME_SIZE>19
  #if DSP_MODEL==DSP_SSD1322
    #define BOOT_PRG_COLOR    WHITE
    #define BOOT_TXT_COLOR    WHITE
    #define PINK              WHITE
  #elif DSP_MODEL==DSP_SSD1327
    #define BOOT_PRG_COLOR    0x07
    #define BOOT_TXT_COLOR    0x3f
    #define PINK              0x02
  #else
    #define BOOT_PRG_COLOR    0xE68B
    #define BOOT_TXT_COLOR    0xFFFF
    #define PINK              0xF97F
  #endif
#endif

void Display::_bootScreen() {
  _boot = new Page();
  _boot->addWidget(new ProgressWidget(bootWdtConf, bootPrgConf, BOOT_PRG_COLOR, 0));
  _bootstring = (TextWidget*) &_boot->addWidget(new TextWidget(bootstrConf, 50, true, BOOT_TXT_COLOR, 0));
  _pager->addPage(_boot);
  _pager->setPage(_boot, true);
  dsp.drawLogo(bootLogoTop);
  _bootStep = 1;
}

void Display::_buildPager() {
  _meta->init("*", metaConf, config.theme.meta, config.theme.metabg);
  _title1->init("*", title1Conf, config.theme.title1, config.theme.background);
  _clock->init(clockConf, 0, 0);
  #if DSP_MODEL==DSP_NOKIA5110
    _plcurrent->init("*", playlistConf, 0, 1);
  #else
    _plcurrent->init("*", playlistConf, config.theme.plcurrent, config.theme.plcurrentbg);
  #endif
  _plwidget->init(_plcurrent);
  #if !defined(DSP_LCD)
    _plcurrent->moveTo({TFT_FRAMEWDT, (uint16_t)(_plwidget->currentTop()), (int16_t)playlistConf.width});
  #endif
  #ifndef HIDE_TITLE2
    _title2 = new ScrollWidget("*", title2Conf, config.theme.title2, config.theme.background);
  #endif
  #if !defined(DSP_LCD) && DSP_MODEL!=DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, config.theme.plcurrentfill);
    #if DSP_INVERT_TITLE || defined(DSP_OLED)
      _metabackground = new FillWidget(metaBGConf, config.theme.metafill);
    #else
      _metabackground = new FillWidget(metaBGConfInv, config.theme.metafill);
    #endif
  #endif
  #if DSP_MODEL==DSP_NOKIA5110
    _plbackground = new FillWidget(playlBGConf, 1);
    //_metabackground = new FillWidget(metaBGConf, 1);
  #endif
  #ifndef HIDE_VU
    _vuwidget = new VuWidget(vuConf, bandsConf, config.theme.vumax, config.theme.vumin, config.theme.background);
  #endif
  #ifndef HIDE_VOLBAR
    _volbar = new SliderWidget(volbarConf, config.theme.volbarin, config.theme.background, 254, config.theme.volbarout);
  #endif
  #ifndef HIDE_HEAPBAR
    _heapbar = new SliderWidget(heapbarConf, config.theme.buffer, config.theme.background, psramInit()?300000:1600 * 10);
  #endif
  #ifndef HIDE_VOL
    _voltxt = new TextWidget(voltxtConf, 10, false, config.theme.vol, config.theme.background);
  #endif
  #ifndef HIDE_IP
    _volip = new TextWidget(iptxtConf, 30, false, config.theme.ip, config.theme.background);
  #endif
  #ifndef HIDE_RSSI
    _rssi = new TextWidget(rssiConf, 20, false, config.theme.rssi, config.theme.background);
  #endif
  #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
    _battery = new TextWidget(batteryConf, 10, false, config.theme.battery, config.theme.background);
  #endif
  _nums->init(numConf, 10, false, config.theme.digit, config.theme.background);
  #ifndef HIDE_WEATHER
    _weather = new ScrollWidget("\007", weatherConf, config.theme.weather, config.theme.background);
  #endif
  
  if (_volbar)   _footer->addWidget(_volbar);
  if (_voltxt)   _footer->addWidget(_voltxt);
  if (_volip)    _footer->addWidget(_volip);
  if (_battery)  _footer->addWidget( _battery);
  if (_rssi)     _footer->addWidget(_rssi);
  if (_heapbar)  _footer->addWidget(_heapbar);
  
  if (_metabackground) pages[PG_PLAYER]->addWidget(_metabackground);
  pages[PG_PLAYER]->addWidget(_meta);
  pages[PG_PLAYER]->addWidget(_title1);
  if (_title2) pages[PG_PLAYER]->addWidget(_title2);
  if (_weather) pages[PG_PLAYER]->addWidget(_weather);
  #if BITRATE_FULL
    _fullbitrate = new BitrateWidget(fullbitrateConf, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_fullbitrate);
  #else
    _bitrate = new TextWidget(bitrateConf, 30, false, config.theme.bitrate, config.theme.background);
    pages[PG_PLAYER]->addWidget(_bitrate);
  #endif
  if (_vuwidget) pages[PG_PLAYER]->addWidget(_vuwidget);
  pages[PG_PLAYER]->addWidget(_clock);
  pages[PG_SCREENSAVER]->addWidget(_clock);
  pages[PG_PLAYER]->addPage(_footer);

  if (_metabackground) pages[PG_DIALOG]->addWidget(_metabackground);
  pages[PG_DIALOG]->addWidget(_meta);
  pages[PG_DIALOG]->addWidget(_nums);
  #ifdef UPDATEURL
    // configure centred update label and progress bar with appropriate gap
    WidgetConfig updConf = apNameConf;
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
      uint8_t ts = apPassConf.textsize > 0 ? apPassConf.textsize : 1;
      uint16_t widgetPx = dsp.width() - apPassConf.left;
      int chars = (int)(widgetPx / (CHARWIDTH * ts));
      _updBarWidth = (chars < 2) ? 2 : (chars > 64) ? 64 : chars;
    }

    // place progress widget under the label maintaining original spacing
    WidgetConfig valConf = apPassConf;
    int16_t origGap = apPassConf.top - apNameConf.top;
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
  #if !defined(DSP_LCD)
    if (_plbackground) {
      pages[PG_PLAYLIST]->addWidget(_plbackground);
      _plbackground->setHeight(_plwidget->itemHeight());
      _plbackground->moveTo({0,(uint16_t)(_plwidget->currentTop()-playlistConf.widget.textsize*2), (int16_t)playlBGConf.width});
    }
  #endif
  pages[PG_PLAYLIST]->addWidget(_plcurrent);
  pages[PG_PLAYLIST]->addWidget(_plwidget);
  for(const auto& p: pages) _pager->addPage(p);
}

void Display::_apScreen() {
  if (_boot) _pager->removePage(_boot);
  #ifndef DSP_LCD
    _boot = new Page();
    #if DSP_MODEL!=DSP_NOKIA5110
      #if DSP_INVERT_TITLE || defined(DSP_OLED)
        _boot->addWidget(new FillWidget(metaBGConf, config.theme.metafill));
      #else
        _boot->addWidget(new FillWidget(metaBGConfInv, config.theme.metafill));
      #endif
    #endif
    ScrollWidget *bootTitle = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", apTitleConf, config.theme.meta, config.theme.metabg));
    bootTitle->setText("AP/Improv Mode");
    TextWidget *apname = (TextWidget*) &_boot->addWidget(new TextWidget(apNameConf, 30, false, config.theme.title1, config.theme.background));
    apname->setText(LANG::apNameTxt);
    TextWidget *apname2 = (TextWidget*) &_boot->addWidget(new TextWidget(apName2Conf, 30, false, config.theme.clock, config.theme.background));
    apname2->setText(AP_SSID);
    TextWidget *appass = (TextWidget*) &_boot->addWidget(new TextWidget(apPassConf, 30, false, config.theme.title1, config.theme.background));
    appass->setText(LANG::apPassTxt);
    TextWidget *appass2 = (TextWidget*) &_boot->addWidget(new TextWidget(apPass2Conf, 30, false, config.theme.clock, config.theme.background));
    #ifdef AP_PASSWORD
      appass2->setText(AP_PASSWORD);
    #endif
    ScrollWidget *bootSett = (ScrollWidget*) &_boot->addWidget(new ScrollWidget("*", apSettConf, config.theme.title2, config.theme.background));
    bootSett->setText(utility.ipToStr(WiFi.softAPIP()), LANG::apSettFmt);
    _pager->addPage(_boot);
    _pager->setPage(_boot);
  #else
    dsp.apScreen();
  #endif
}

void Display::_start() {
  if (_boot) _pager->removePage(_boot);
  #ifdef USE_NEXTION
    nextion.wake();
  #endif
  if (network.status != CONNECTED && network.status != SDREADY) {
    _apScreen();
    #ifdef USE_NEXTION
      nextion.apScreen();
    #endif
      _bootStep = 2;
    return;
  }
  #ifdef USE_NEXTION
    //nextion.putcmd("page player");
    nextion.start();
  #endif
  _buildPager();
  _mode = PLAYER;
  config.setTitle(LANG::const_PlReady);
  
  if (_heapbar)  _heapbar->lock(!config.store.audioinfo);
  
  if (_weather)  _weather->lock(!config.store.showweather);
  if (_weather && config.store.showweather) network.buildWeatherString();

  if (_vuwidget) _vuwidget->lock();
  if (_rssi)     _setRSSI(WiFi.RSSI());
  #ifndef HIDE_IP
    if (_volip) _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
  #endif
  #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
    if(_battery) _updateBattery();
  #endif
  _pager->setPage(pages[PG_PLAYER]);
  _volume();
  _station();
  _time(false);
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
  #ifdef USE_NEXTION
    //nextion.swichMode(newmode);
    nextion.putRequest({NEWMODE, newmode});
  #endif
  if (newmode == _mode || (network.status != CONNECTED && network.status != SDREADY)) return;
  _mode = newmode;
  dsp.setScrollId(NULL);
  if (newmode == PLAYER) {
    if (player.isRunning())
      if (clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
    else
      _clock->moveBack();
    #ifdef DSP_LCD
      dsp.clearDsp();
    #endif
    numOfNextStation = 0;
    #ifdef META_MOVE
      _meta->moveBack();
    #endif
    _meta->setAlign(metaConf.widget.align);
    _meta->setText(config.station.name);
    _nums->setText("");
    config.isScreensaver = false;
    _pager->setPage(pages[PG_PLAYER]);
    config.setDspOn(config.store.dspon, false);
  }
  if (newmode == SCREENSAVER || newmode == SCREENBLANK) {
    config.isScreensaver = true;
    _pager->setPage(pages[PG_SCREENSAVER]);
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
    if (config.store.volumepage) {
      #ifndef HIDE_IP
        _showDialog(LANG::const_DlgVolume);
      #else
        _showDialog(utility.ipToStr(WiFi.localIP()));
      #endif
    }
    _nums->setText(config.store.volume, numtxtFmt);
  }
  if (newmode == LOST)      _showDialog(LANG::const_DlgLost);
  if (newmode == UPDATING)  { _showDialog(LANG::const_DlgUpdate);
    #ifdef UPDATEURL
      _updFirstCall = true;
    #endif
  }
  if (newmode == SLEEPING)  _showDialog("SLEEPING");
  if (newmode == SDCHANGE)  _showDialog(LANG::const_waitForSD);
  if (newmode == INFO || newmode == SETTINGS || newmode == TIMEZONE || newmode == WIFI) _showDialog(LANG::const_DlgNextion);
  if (newmode == NUMBERS) _showDialog("");
  if (newmode == STATIONS) {
    _pager->setPage(pages[PG_PLAYLIST]);
    _plcurrent->setText("");
    currentPlItem = config.lastStation();
    _drawPlaylist();
  }
  
}

void Display::resetQueue() {
  if (displayQueue!=NULL) xQueueReset(displayQueue);
}

void Display::_drawPlaylist() {
  uint16_t count = utility.playlistLength();
  if (count == 0) currentPlItem = 0;
  else if (currentPlItem < 1) currentPlItem = 1;
  else if (currentPlItem > count) currentPlItem = count;

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
  #ifdef USE_NEXTION
    nextion.putRequest(request);
  #endif
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
      //_clock->moveTo(clockMove);
      if (clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if (_weather) _weather->moveTo(weatherMoveVU);
    } else {
      if (_vuwidget) if (!_vuwidget->locked()) _vuwidget->lock();
      _clock->moveBack();
      if (_weather) _weather->moveBack();
    }
  } else {
    if (played) {
      if (clockMove.width<0) _clock->moveBack(); else _clock->moveTo(clockMove);
      if (_weather) _weather->moveTo(weatherMove);
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
      if (DSP_INVERT_QUIRK && config.displayIsInverted) {
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
  #ifdef USE_NEXTION
    nextion.loop();
  #endif
  requestParams_t request;
  if (xQueueReceive(displayQueue, &request, DSP_QUEUE_TICKS)) {
    switch (request.type) {
        case NEWMODE: _swichMode((displayMode_e)request.payload); break;
        case CLOSEPLAYLIST: player.sendCommand({PR_PLAY, request.payload});
        case CLOCK: 
          if (_mode==PLAYER || _mode==SCREENSAVER) _time(); 
          /*#ifdef USE_NEXTION
            if (_mode==TIMEZONE) nextion.localTime(network.timeinfo);
            if (_mode==INFO)     nextion.rssi();
          #endif*/
          break;
        case NEWTITLE: _title(); break;
        case NEWSTATION: _station(); break;
        case NEXTSTATION: _drawNextStationNum(request.payload); break;
        case DRAWPLAYLIST: _drawPlaylist(); break;
        case DRAWVOL: _volume(); break;
        case DBITRATE: {
            char buf[20]; 
            snprintf(buf, 20, bitrateFmt, config.station.bitrate); 
            if (_bitrate) { _bitrate->setText(config.station.bitrate==0?"":buf); } 
            if (_fullbitrate) { 
              _fullbitrate->setBitrate(config.station.bitrate); 
              _fullbitrate->setFormat(config.configFmt); 
            } 
          }
          break;
        case AUDIOINFO: if (_heapbar)  { _heapbar->lock(!config.store.audioinfo); _heapbar->setValue(player.inBufferFilled()); } break;
        case SHOWVUMETER: {
          if (_vuwidget) {
            _vuwidget->lock(!config.store.vumeter); 
            _layoutChange(player.isRunning());
          }
          break;
        }
        case SHOWWEATHER: {
          if (_weather) _weather->lock(!config.store.showweather);
          if (!config.store.showweather) {
            if (_weather) _weather->setText("");
            #ifndef HIDE_IP
              if (_volip) _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
            #endif
          } else {
            network.buildWeatherString();
          }
          break;
        }
        case NEWWEATHER: {
          if (_weather && network.weatherBuf) _weather->setText(network.weatherBuf);
          break;
        }
        case BOOTSTRING: {
          if (_bootstring) _bootstring->setText(config.ssids[request.payload].ssid, LANG::bootstrFmt);
          /*#ifdef USE_NEXTION
            char buf[50];
            snprintf(buf, 50, bootstrFmt, config.ssids[request.payload].ssid);
            nextion.bootString(buf);
          #endif*/
          break;
        }
        case WAITFORSD: {
          if (_bootstring) _bootstring->setText(LANG::const_waitForSD);
          break;
        }
        case SDFILEINDEX: {
          if (_mode == SDCHANGE) _nums->setText(request.payload, "%d");
          break;
        }
        case DSPRSSI: if (_rssi) { _setRSSI(request.payload); } if (_heapbar && config.store.audioinfo) _heapbar->setValue(player.isRunning()?player.inBufferFilled():0); break;
        #if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
          case DSPBATTERY: {
            if(_battery) _updateBattery();
            break;
          }
        #endif
        case PSTART: _layoutChange(true);   break;
        case PSTOP:  _layoutChange(false);  break;
        case DSP_START: _start();  break;
        case NEWIP: {
          #ifndef HIDE_IP
            if (_volip) _volip->setText(utility.ipToStr(WiFi.localIP()), iptxtFmt);
          #endif
          break;
        }
        default: break;

        // check if there are more messages waiting in the Q, in this case break the loop() and go
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

#if defined(BATTERY_PIN) && (BATTERY_PIN!=255)
  void Display::_updateBattery() {
    if(_battery) {
      BatteryStatus bat = battery.getStatus();
      if(!bat.valid && battery.isInitialized()) {
        battery.recalcNow();
        bat = battery.getStatus();
      }
      if(battery.isInitialized() || bat.valid) {
        const char *baseFmt;
        if(bat.percentage < 25) baseFmt = batteryRangeLowFmt;
        else if(bat.percentage < 75) baseFmt = batteryRangeMidFmt;
        else baseFmt = batteryRangeHighFmt;
        char buf[48];
        snprintf(buf, sizeof(buf), baseFmt, bat.percentage);

        // Insert warning marker before numeric percentage (so it appears after the icon)
        // Only show single exclamation on LOW battery; critical state triggers deep-sleep so
        // an explicit visual critical marker is unnecessary.
        if(bat.low_battery) {
          const char *mark = "!";
          char *p = buf;
          while(*p && !isdigit((unsigned char)*p)) p++; // find start of digits
          if(*p) {
            size_t len = strlen(buf);
            size_t mlen = strlen(mark);
            if(len + mlen < sizeof(buf)) {
              memmove(p + mlen, p, len - (p - buf) + 1); // include null
              memcpy(p, mark, mlen);
            } else {
              /* append safely to avoid overflow */
              strncat(buf, mark, sizeof(buf) - strlen(buf) - 1);
            }
          } else {
            /* append safely to avoid overflow */
            strncat(buf, mark, sizeof(buf) - strlen(buf) - 1);
          }
        }

        /* Add charging/discharging icon prefix before the battery icon if available.
           Icons are single-byte glyphs in `glcdfont.c`:
             - decimal 24 (octal \030) => charging icon
             - decimal 25 (octal \031) => discharging icon
           Use inferred flags when a charge pin isn't present. */
        const char *chg_prefix = NULL;
        if (bat.charging || bat.charging_inferred) chg_prefix = "\030"; /* dec24 */
        else if (bat.discharging_inferred) chg_prefix = "\031"; /* dec25 */
        if (chg_prefix) {
          /* Insert prefix glyph then a 2-pixel spacer control char (0x1E) before the battery text. */
          size_t len = strlen(buf);
          size_t plen = strlen(chg_prefix);
          if (len + plen + 1 < sizeof(buf)) {
            memmove(buf + plen + 1, buf, len + 1); /* include null */
            memcpy(buf, chg_prefix, plen);
            buf[plen] = '\x1E'; /* 2-pixel spacer */
          }
        }

        _battery->setText(buf);
      } else {
        _battery->setText("");
      }
    }
  }
#endif

void Display::_station() {
  _meta->setAlign(metaConf.widget.align);
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
    /*#ifdef USE_NEXTION
      nextion.newTitle(config.station.title);
    #endif*/
    
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
  if (config.isScreensaver && network.timeinfo.tm_sec % 60 == 0) {
    #if TIME_SIZE<19
      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.height()-TIME_SIZE*CHARHEIGHT-TFT_FRAMEWDT)));
    #else
      uint16_t ft=static_cast<uint16_t>(random(TFT_FRAMEWDT+TIME_SIZE, (dsp.height()-_clock->dateSize()-TFT_FRAMEWDT*2)));
    #endif
    uint16_t lt=static_cast<uint16_t>(random(TFT_FRAMEWDT, (dsp.width()-_clock->clockWidth()-TFT_FRAMEWDT)));
    if (clockConf.align==WA_CENTER) lt-=(dsp.width()-_clock->clockWidth())/2;
    //_clock->moveTo({clockConf.left, ft, 0});
    _clock->moveTo({lt, ft, 0});
  }
  _clock->draw();
  /*#ifdef USE_NEXTION
    nextion.printClock(network.timeinfo);
  #endif*/
}

void Display::_volume() {
  if (_volbar) _volbar->setValue(config.store.volume);
  #ifndef HIDE_VOL
    if (_voltxt) _voltxt->setText(config.store.volume, voltxtFmt);
  #endif
  if (_mode==VOL) {
    _setReturnTicker(3);
    _nums->setText(config.store.volume, numtxtFmt);
  }
  /*#ifdef USE_NEXTION
    nextion.setVol(config.store.volume, _mode == VOL);
  #endif*/
}

void Display::flip() { dsp.flip(); }

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
  _createDspTask();
  #ifdef USE_NEXTION
    nextion.begin(true);
  #endif
}
void Display::_start() {
  #ifdef USE_NEXTION
    //nextion.putcmd("page player");
    nextion.start();
  #endif
  config.setTitle(LANG::const_PlReady);
}

void Display::putRequest(displayRequestType_e type, int payload) {
  if (type==DSP_START) _start();
  #ifdef USE_NEXTION
    requestParams_t request;
    request.type = type;
    request.payload = payload;
    nextion.putRequest(request);
  #else
    if (type==NEWMODE) mode((displayMode_e)payload);
  #endif
}

#endif // ============================== DUMMYDISPLAY Ends ==============================

