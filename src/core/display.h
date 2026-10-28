#ifndef display_h
#define display_h
#include <Ticker.h>
#include "common.h"
#include "../displays/widgets/widgetsconfig.h"  // WidgetConfig types + layout switching pointers

static constexpr uint32_t DSP_QUEUE_TICKS = 0; // xQueueReceive timeout inside display.loop(): 0 = non-blocking poll

// What a re-init reads its two halves from.  The stored ids are the ordinary path; a staged variant is an
// editor preview, where the web handler has already parsed the new document into a scratch copy.  They are
// separate modes rather than one flag because the two previews stage different things: a theme preview must
// keep the stored layout, and a layout preview must keep the live theme.
enum applyMode_e : uint8_t { APPLY_STORED = 0, APPLY_STAGED_THEME, APPLY_STAGED_LAYOUT };

#ifndef DUMMYDISPLAY
class ScrollWidget;
class PlayListWidget;
class BitrateWidget;
class FillWidget;
class SliderWidget;
class Pager;
class Page;
class VuWidget;
class NumWidget;
class ClockWidget;
class TextWidget;
    
class Display {
  public:
    uint16_t currentPlItem = 0;
    uint16_t numOfNextStation = 0;
    volatile displayMode_e _mode = PLAYER;  // volatile: read from ISR in controls.cpp
  public:
    Display() {};
    ~Display();
    void init();
    uint16_t width();
    uint16_t height();
    void _bootScreen();
    void _buildPager();
    void _apScreen();
    void _start();
    void resetQueue();
    void _drawPlaylist();
    void _drawNextStationNum(uint16_t num);
    void putRequest(displayRequestType_e type, int payload=0);
    // Same as putRequest(), but held until delayMs has passed - for a message that must not replace what is on
    // screen yet (the boot scan would otherwise overwrite the firmware version ~100ms in).
    void putRequestDelayed(displayRequestType_e type, int payload=0, uint32_t delayMs=0);
    void _layoutChange(bool played);
    void loop();
    void _setRSSI(int rssi);
    void _station();
    void _title();
    void _titleTexts();
    void _time(bool redraw = false);
    void _volume();
    void flip();
    void applyLayout(uint8_t id);
    uint8_t getLayoutCount();
    void applyTheme(uint8_t id);
    uint8_t getThemeCount();
    void applySystemFont(uint8_t id);
    uint8_t getSystemFontCount();
    void applyClockFont(uint8_t id);
    uint8_t getClockFontCount();
    void applyFonts(uint8_t systemId, uint8_t clockId);
    String getThemeListJson();
    String getLayoutListJson();
    String getSystemFontListJson();
    String getClockFontListJson();
    void applyInvertTitle();
    void invert();
    void setContrast();
    bool deepsleep();
    void wakeup();
    void updateProgress(const char* label, float progress);
    void clearProgress();
    #ifdef USE_SD
      // Redraws the SD Manager countdown
      void sdmanCountdown();
    #endif

    displayMode_e mode() { return _mode; }
    void mode(displayMode_e m) { _mode=m; }
    bool ready() { return _bootStep==2; }
    void lock()   { _locked=true; }
    void unlock() { _locked=false; }
  private:
    ScrollWidget *_meta = nullptr, *_title1 = nullptr, *_plcurrent = nullptr, *_weather = nullptr, *_title2 = nullptr;
    PlayListWidget *_plwidget = nullptr;
    ScrollWidget *_updLabel = nullptr;
    TextWidget *_updValue = nullptr;
    bool _updFirstCall = true;
    int _updBarWidth = 10;
    BitrateWidget *_fullbitrate = nullptr;
    FillWidget *_metabackground = nullptr, *_plbackground = nullptr, *_underline = nullptr, *_overline = nullptr;
    SliderWidget *_volbar = nullptr, *_bufferbar = nullptr;
    uint32_t _bufferbarMax = 0;
    Pager *_pager = nullptr;
    Page *_footer = nullptr;
    Page *_overLinePage = nullptr;
    VuWidget *_vuwidget = nullptr;
    bool _vuNotDrawable = false;
    VuWidget *_ssvu = nullptr;
    ScrollWidget *_sstext = nullptr;
    bool _ssFailed = false;
    bool _ssMeterUp = false;
    uint32_t _ssHoldUntil = 0;
    NumWidget *_nums = nullptr;
    ClockWidget *_clock = nullptr;
    Page *_boot = nullptr;
    TextWidget *_bootstring = nullptr, *_volip = nullptr, *_voltxt = nullptr, *_battery = nullptr, *_rssi = nullptr, *_bitrate = nullptr;
    #ifdef USE_SD
      TextWidget *_sdmanCountText = nullptr;
      bool _sdmanCountShown = false;
      void _sdmanScreen();
    #endif
    Ticker _returnTicker;
    bool _locked = false;
    uint8_t _bootStep = 0;
    displayRequestType_e _deferredType = NOPE;
    int _deferredPayload = 0;
    uint32_t _droppedRequests = 0;
    uint32_t _deferredDueMs = 0;
    void _createDspTask();
    void _applyBitrate();
    void _reinitWidgets();
    void _setLayoutPointers();
    void _syncLineRule(FillWidget*& w, const FillConfig* conf, bool under);
    void _applyMetaInvert();
    void _applyState(applyMode_e mode = APPLY_STORED);
    void _showDialog(const char *title);
    void _showNumbers(const char* header, int32_t value, const char* fmt, bool dialogPage, uint32_t fieldmax = 0);
    void _setReturnTicker(uint8_t time_s);
    bool _ownScreen() const;
    bool _drawsOverOwnScreen(displayRequestType_e type) const;
    // The footer sub-page's widgets, in insertion order, one bit each.  A bitmask rather than six flags because
    // ONE table decides which screen keeps which, and every reader asks that table the same question.
    enum FooterWidget_e : uint8_t {
      FW_VOLBAR = 1u << 0, FW_VOLTXT = 1u << 1, FW_VOLIP = 1u << 2,
      FW_BATT   = 1u << 3, FW_RSSI   = 1u << 4, FW_BUFBAR = 1u << 5,
      FW_ALL    = FW_VOLBAR | FW_VOLTXT | FW_VOLIP | FW_BATT | FW_RSSI | FW_BUFBAR
    };
    uint8_t _footerMask() const;
    bool _footerShown(uint8_t bit) const { return (_footerMask() & bit) != 0; }
    void _applyFooter();
    bool _clockHidden();
    bool _weatherHidden();
    void _switchMode(displayMode_e newmode);
    void _updateBattery();
    void _updateVolume();
    void _footerIp();
    uint16_t _ssStripH();
    uint16_t _ssContentH();
    uint16_t _ssTextTop();
    void _screensaverLine();
    void _ssHoldLine(uint32_t ms);
    void _screensaverWidgets();
    void _screensaverClockTarget(MoveConfig& mv);
    void _enterScreensaver(displayMode_e mode);
    void _buildJsonCache();
    void _buildThemeListJson();
    void _buildLayoutListJson();
    String _themeListJson;
    uint16_t _themeListGen;
    uint16_t _layoutListGen;
    bool _previewPending;
    bool _layoutPreviewPending;
    String _layoutListJson;
    String _systemFontListJson;
    String _clockFontListJson;
  };

#else

class Display {
  public:
    uint16_t currentPlItem;
    uint16_t numOfNextStation;
    displayMode_e _mode = PLAYER;
  public:
    Display() {};
    void init();
    void _start();
    void putRequest(displayRequestType_e type, int payload=0);
    void putRequestDelayed(displayRequestType_e type, int payload=0, uint32_t delayMs=0) { putRequest(type, payload); }

    displayMode_e mode() { return _mode; }
    void mode(displayMode_e m) { _mode=m; }
    void loop() {}
    bool ready() { return true; }
    void resetQueue() {}
    void centerText(const char* text, uint8_t y, uint16_t fg, uint16_t bg) {}
    void rightText(const char* text, uint8_t y, uint16_t fg, uint16_t bg) {}
    void flip() {}
    void applyLayout(uint8_t) {}
    uint8_t getLayoutCount() { return 1; }
    String getThemeListJson()  { return "[]"; }
    String getLayoutListJson() { return "[]"; }
    String getSystemFontListJson() { return "[]"; }
    String getClockFontListJson()  { return "[]"; }
    void applyTheme(uint8_t) {}
    void applySystemFont(uint8_t) {}
    uint8_t getSystemFontCount() { return 1; }
    void applyClockFont(uint8_t) {}
    uint8_t getClockFontCount() { return 1; }
    void applyFonts(uint8_t, uint8_t) {}
    uint8_t getThemeCount() { return 1; }
    void applyInvertTitle() {}
    void invert() {}
    bool deepsleep() {return true;}
    void wakeup() {}
    void lock()   {}
    void unlock() {}
    uint16_t width() { return 0; }
    uint16_t height() { return 0; }
    void updateProgress(const char* label, float progress) {}
    void clearProgress() {}
    #ifdef USE_SD
      void sdmanCountdown() {}
    #endif
  private:
    void _createDspTask();
};

#endif

void returnPlayer();

extern Display display;
extern TaskHandle_t dspTaskHandle;


#endif
