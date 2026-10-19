#ifndef widgets_h
#define widgets_h
#if DSP_MODEL!=DSP_DUMMY
#include "widgetsconfig.h"

#define CHARWIDTH   6
#define CHARHEIGHT  8

// Widget text buffers: one size for every scrolling line and text widget
// Every conf used to carry this as ScrollConfig.buffsize and every one of them wrote 140...
#define WIDGET_BUFF_SIZE 140

class psFrameBuffer;

class Widget{
  public:
    Widget(){ _active   = false; }
    virtual ~Widget(){}
    virtual void loop(){}
    virtual void init(WidgetConfig conf, uint16_t fgcolor, uint16_t bgcolor){
      _config = conf;
      _fgcolor  = fgcolor;
      _bgcolor  = bgcolor;
      _width = _backMove.width = 0;
      _backMove.x = _config.offset;
      _backMove.y = _config.top;
      _moved = _locked = false;
    }
    // A number, like the member it writes: callers pass either a WA_* name or a stored cell, and an enum
    // converts to an integer but not the other way round.
    void setAlign(uint8_t align){
      _config.align = align;
    }
    // _present = the active layout provides this widget, set only by hideByLayout()/showByLayout(). It outranks
    // _active, which Pager::setPage() re-activates on every mode change.
    void setActive(bool act, bool clr=false) { if(act && !_present) return; _active = act; if(_active && !_locked) _draw(); if(clr && !_locked) _clear(); }
    // Locking is always allowed; unlocking a widget the layout dropped is not. Once absent, nothing but
    // showByLayout() can make it draw again.
    void lock(bool lck=true) { if(!lck && !_present) return; _locked = lck; if(_locked) _reset(); if(_locked && _active) _clear();  }
    void unlock() { if(_present) _locked = false; }
    bool locked() { return _locked; }
    // The other half of the pair hideWeatherIfChanged() sets: a page switch re-activates every widget without
    // touching the lock, so the two can disagree and the hide has to be able to see that.
    bool isActive() { return _active; }
    void setPresent(bool p) { _present = p; }
    bool present() { return _present; }
    void moveTo(MoveConfig mv){
      // Defensive only.  A negative width was the retired `-1` spelling ("keep the conf position"), and a
      // conf that still carries one must stay inert rather than jump to (0, 0).  Never make width
      // unsigned: a stale -1 would become a 65535 px span at the assignment below.
      if(mv.width<0) return;
      _moved = true;
      if(_active && !_locked) _clear();
      _config.offset = mv.x;
      _config.top = mv.y;
      if(mv.width>0) _width = mv.width;
      _reset();
      _draw();
    }
    // Would this MOVE change anything?  A conf spells "this state does not move this line" as the conf's OWN
    // position, because an ALL-ZERO move means something else: it is how a state HIDES its line
    // (_weatherHidden()'s yieldsToVU/yieldsNoVU, mirroring applyMove()'s moveZeroed() guard).  Repainting a move
    // that changes nothing is a visible flicker - the erase and the redraw are two passes over the panel and the
    // gap between them is a background-coloured tear across the line's span, which is the reported "line torn
    // through the row" on every start and stop of the stream.  A width of 0 in a move means "keep the width", so
    // only a move that carries one can change it.
    bool moveIsNoop(const MoveConfig& mv) const {
      return mv.x == _backMove.x && mv.y == _backMove.y
             && (mv.width <= 0 || mv.width == _backMove.width);
    }
    // The same question about where the widget IS rather than where it is being sent: the way back has nothing to
    // do once the line is already at the conf's position.
    bool atBackPosition() const {
      return _config.offset == _backMove.x && _config.top == _backMove.y
             && (int16_t)_width == _backMove.width;
    }
    void moveBack(){
      if(!_moved) return;
      // Already home: nothing to restore, so nothing to erase and nothing to draw.
      if(atBackPosition()) { _moved = false; return; }
      if(_active && !_locked) _clear();
      _config.offset = _backMove.x;
      _config.top = _backMove.y;
      _width = _backMove.width;
      _moved = false;
      _reset();
      _draw();
    }
  protected:
    bool _active, _moved, _locked;
    bool _present = true;   // false when the active layout does not provide this widget
    uint16_t _fgcolor, _bgcolor, _width;
    WidgetConfig _config;
    MoveConfig _backMove;
    virtual void _draw() {}
    virtual void _clear() {}
    virtual void _reset() {}
};

class TextWidget: public Widget {
  public:
    TextWidget() {}
    TextWidget(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize = WIDGET_BUFF_SIZE) { init(wconf, fgcolor, bgcolor, buffsize); }
    ~TextWidget();
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize = WIDGET_BUFF_SIZE);
    // Virtual because a subclass held as a TextWidget* must still be entered through its own overloads. The boot line
    // is that case: a ScrollWidget kept in a TextWidget*, where only ScrollWidget::setText() sets _doscroll and
    // measures with its own _charWidth. Bound statically, the base ran instead, so the scroll state was never
    // initialised and an over-long string was painted at the underflowed centre offset - invisible.
    virtual void setText(const char* txt);
    virtual void setText(int val, const char *format);
    virtual void setText(const char* txt, const char *format);
    // Paints the CURRENT text again, whether or not it has changed. setText() deliberately does nothing when the
    // string is the one already up, which is right for a value that only moves on events and wrong for a line
    // something else may paint over: the SD manager's countdown blank was issued once and never repeated.
    // Virtual because a ScrollWidget must not go through _paint(): its erase is unclipped and as wide as the whole
    // string, which for a scrolling line reaches past its span.
    virtual void repaint();
  protected:
    char *_text = nullptr;
    char *_oldtext = nullptr;
    // Initialised, so a widget that has never been init()'d is inert rather than full of stack garbage.
    uint8_t _charWidth = 0;
    uint16_t  _buffsize = 0, _textwidth = 0, _oldtextwidth = 0, _oldleft = 0, _textheight = 0;
    // True once _paint() has put text on the panel, so the erase knows whether the recorded old rectangle
    // means anything - see _paint().  Not _painted: ProgressWidget has one of those of its own.
    bool _textPainted = false;
  protected:
    void _paint();
    void _draw();
    uint16_t _realLeft(bool w_fb=false);
    void _charSize(uint8_t textsize, uint8_t& width, uint16_t& height);
};

class FillWidget: public Widget {
  public:
    FillWidget() {}
    FillWidget(FillConfig conf, uint16_t bgcolor) { init(conf, bgcolor); }
    using Widget::init;
    void init(FillConfig conf, uint16_t bgcolor);
    void setHeight(uint16_t newHeight);
  protected:
    uint16_t _height;
    bool _outlined = false;
    void _draw();
};

// The mark a looping scroll line prints between its tail and its head, spaces included: " * ", or the holiday icon
// on the holiday dates.  It is also what the screensaver's info line joins its own parts with, so the character
// lives in exactly one place.  See plans/holiday-icons.md.
const char* scrollMark();

class ScrollWidget: public TextWidget {
  public:
    ScrollWidget(){}
    // The mark this line joins its tail and its head with is no longer the caller's business: scrollMark() owns
    // the character and the holiday swap in one place (see plans/holiday-icons.md).
    ScrollWidget(ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize = WIDGET_BUFF_SIZE);
    ~ScrollWidget();
    using Widget::init;
    void init(ScrollConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize = WIDGET_BUFF_SIZE);
    void loop();
    void setText(const char* txt) override;
    void setText(const char* txt, const char *format) override;
    void repaint() override;      // its own _draw(), not TextWidget's unclipped erase
  private:
    char *_sep = nullptr;
    char *_window = nullptr;
    int16_t _x;
    bool _doscroll;
    uint8_t _scrolldelta;
    uint16_t _scrolltime;
    uint32_t _scrolldelay;
    uint16_t _sepwidth, _startscrolldelay;
    uint8_t _charWidth;
    psFrameBuffer* _fb=nullptr;
    // The window this line paints through, on a framebuffer build.  _width stays what it has always
    // meant - the span from the conf or a MOVE, and what decides whether the line scrolls - while the
    // window is that span only WHILE the line scrolls.  A line that fits owns only its own glyphs, so
    // its window is the text's rectangle, and a repaint stops at the last glyph instead of wiping a
    // neighbour that shares the row (the codec badge, in the reported case).
    uint16_t _winx = 0, _winw = 0;   // the framebuffer window currently made
  private:
    void _windowRect(uint16_t& x, uint16_t& w);
    void _syncWindow(bool force = false);
    void _setTextParams();
    void _calcX();
    void _drawFrame();
    void _draw();
    void _setMark(const char* mark);   // import a mark into _sep and re-measure it
    void _refreshMark();               // re-ask while the line is live: a holiday can begin at midnight
    void _spanPainted();               // a scrolling paint owns the whole span, not just its own box
    bool _checkIsScrollNeeded();
    bool _checkDelay(int m, uint32_t &tstamp);
    void _clear();
    void _reset();
};

class SliderWidget: public Widget {
  public:
    SliderWidget(){}
    SliderWidget(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor=0){
      init(conf, fgcolor, bgcolor, maxval, oucolor);
    }
    using Widget::init;
    void init(FillConfig conf, uint16_t fgcolor, uint16_t bgcolor, uint32_t maxval, uint16_t oucolor=0);
    void setValue(uint32_t val);
  protected:
    uint16_t _height, _oucolor, _oldvalwidth;
    uint32_t _max, _value;
    bool _outlined;
    void _draw();
    void _drawslider();
    void _clear();
    void _reset();
};

// The clock font a widget is drawing with, resolved from its own WidgetConfig.textsize.  This is a
// struct rather than three globals because a layout sizes the clock (clockConf) and the number page
// (numConf) independently now, so there is no single "current" clock font any more.  textsize is the
// SIZE INDEX, not a pixel height: 0 is the system font (font stays nullptr and the built-in cell is
// used at textPx = 1), and 1..4 are 15, 35, 52 and 70 px — see clockSizePx() in dspfont.h.
struct ClockFontSel {
  const GFXfont* font;    // nullptr for index 0 — draw with the display font
  uint8_t        index;   // 0..4, what the layout asked for
  uint8_t        textPx;  // the built-in cell's multiplier when font is nullptr (always 1)
};

class NumWidget: public TextWidget {
  public:
    using Widget::init;
    void init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor, uint16_t buffsize = WIDGET_BUFF_SIZE);
    void setText(const char* txt) override;
    void setText(int val, const char *format) override;
    // The widest value this field will show.  With the glow on, that many digit cells are reserved, so a
    // one-digit value keeps its column instead of jumping when it becomes two.  The source owns the figure:
    // VOLUME_SCALE for the volume, the playlist total for a station/track number, 0 for "no field".
    void setFieldMax(uint32_t maxval);
  protected:
    ClockFontSel _cf {nullptr, 0, 1};
    uint32_t _fieldmax = 0;
    // _textwidth is the FIELD (what is placed and erased), _digitwidth is what is printed inside it.
    uint16_t _digitwidth = 0;
    char _glowField[6] = "";   // one glow cell per reserved digit + NUL; a uint16_t total is at most 5
    bool _glowOn();
    void _getBounds();
    void _draw();
};

class ProgressWidget: public TextWidget {
  public:
    ProgressWidget() {}
    // pconf.width is the whole line budget in characters, frame included: the dot runway is what remains after the
    // frame, so a conf author only has to know how many characters this line may occupy. Both glyphs are used exactly
    // as given and must outlive the widget - each is a string literal, the speaker from display.cpp and the boot-mode
    // glyph from startup.icon().
    ProgressWidget(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor,
                   const char* frameLeft = nullptr, const char* frameGlyph = nullptr) {
      init(conf, pconf, fgcolor, bgcolor, frameLeft, frameGlyph);
    }
    using Widget::init;
    void init(WidgetConfig conf, ProgressConfig pconf, uint16_t fgcolor, uint16_t bgcolor,
              const char* frameLeft = nullptr, const char* frameGlyph = nullptr);
    void loop();
  protected:
    // Full paint, and the only path that draws the two static glyphs: activation, layout changes and screensaver
    // restarts come through here, while the animation paints single cells in _progress(). That split keeps a TFT from
    // flashing the whole line - the glyphs never change, so they are never redrawn.
    void _draw();
  private:
    // The two glyphs framing the line. Both are string literals owned by the caller - the speaker from display.cpp
    // and the boot-mode glyph from startup.icon() - so no copy of either is kept here.
    const char* _frameL = nullptr;
    const char* _frameR = nullptr;
    // The runway in CHARACTERS. Both glyphs count as one character each however many bytes they are: the SD pair
    // renders one column wider and that is invisible on a single row. The buffer is sized separately in BYTES,
    // because a U+00B7 dot is two of them and a character count cuts the line mid-dot.
    uint16_t _runway = 0;
    uint16_t _fieldX = 0;                  // x of the first dot column, recorded by the last full paint
    uint16_t _oldLead = 0, _oldDots = 0;   // what the last painted frame showed, for the cell delta
    bool _painted = false;                 // false until _draw() has put a known picture on the panel
    uint8_t _pg;
    uint16_t _speed, _barwidth;
    uint32_t _scrolldelay;
    void _blob(uint16_t& lead, uint16_t& dots) const;
    void _dotCell(uint16_t col, bool on);
    void _progress();
    bool _checkDelay(int m, uint32_t &tstamp);
};

class ClockWidget: public Widget {
  public:
    using Widget::init;
    ~ClockWidget();
    void init(WidgetConfig wconf, uint16_t fgcolor, uint16_t bgcolor);
    // Rebuild for the size the current screen state gives this clock (see _clockConfSize() in widgets.cpp), doing
    // nothing when that is the size it is already drawn at.  The Display calls it entering and leaving the
    // screensaver: the Smaller Clock option is not a layout change, and init() is the only place a size is resolved.
    void refreshSize(WidgetConfig conf);
    void draw();
    void forceDraw() { _draw(); }
    uint8_t textsize(){ return _config.textsize; }
    void clear(){ _clearClock(); }
    inline uint16_t dateSize(){ return _space+ _dateheight; }
    inline uint16_t clockWidth(){ return _clockwidth; }
    inline uint16_t clockHeight(){ return _clockheight; }
    inline uint16_t timeHeight(){ return _timeheight; }
    bool _compactClock() const { return _superfont <= 1; }
  private:
    Adafruit_GFX &getRealDsp();
  protected:
    char  _timebuffer[20]="00:00";
    char _tmp[64], _datebuf[30];
    // The size INDEX (0..4), not a pixel height and no longer TIME_SIZE/17.  It is the built-in
    // font's textSize for the day-of-week string and the multiplier behind _space and _clockheight.
    uint8_t _superfont;
    // The two shapes the clock draws with, built in init() from the active style's ONE glow
    // character (ClockFontStyle.glowChar): the clock's <g><g>:<g><g>, which is ALSO its width
    // template, and the seconds' <g><g>.  Arrays, not pointers, because the character is a runtime
    // value.  The initialisers are the size-index-0 case — no clock font to ask, so the digit '0'
    // stands in, which every system font is guaranteed to have at CHARWIDTH; there the clock is
    // drawn by the display font and the string is only ever measured, never printed.  _glowOn()
    // enforces that, and the characters must NOT be blanked for size 0 instead: they are the width
    // templates _timewidth and _secwidth are read off.
    char _glow[6] = "00:00";
    char _glowSec[3] = "00";
    // The seconds block's width IN PIXELS, measured in _getTimeBounds() from the SECONDS font's own
    // advances.  It used to be computed as CHARWIDTH * _superfont * 2, which was right for every
    // rung until the 10 px one arrived: a digit's width is not proportional to its height (8 px ->
    // 6, 10 px -> 8, 15 px -> 12, 21 px -> 18, 28 px -> 24), so measuring is the only safe rule -
    // the same reason the clock's own width is measured from its glow string rather than assumed.
    uint16_t _secwidth = 0;
    // The clock font resolved from this widget's own clockConf.textsize.
    ClockFontSel _cf {nullptr, 0, 1};
    uint16_t _clockleft, _clockwidth, _timewidth, _dotsleft, _linesleft;
    // The clock's OWN row: the time, plus the seconds when they follow it on that row.  The compact sizes
    // align the two rows against each other inside the box (see _rowLeft), so this width has to be known to
    // the print as well as to _getTimeBounds(), which is where it is measured.
    uint16_t _rowwidth = 0;
    uint8_t  _clockheight, _timeheight, _dateheight, _space;
    // One row's worth of gap for this size, used for the seconds' gap and for the date's row.  Identical to
    // _space for indices 1..4; at index 0, where _space is 0, it is a whole built-in character row - a clock
    // font's ink sits above its cursor and the built-in cell's below it, so that is the honest gap there.
    uint8_t  _rowgap = 0;
    // The date line, built into _datebuf.  _getTimeBounds() builds it as well as the print, because at the
    // compact sizes the date is usually the wider of the clock's two rows and the box is sized for it.
    void _dateString();
    // Where a row of width rowW starts.  At the compact sizes the two rows are aligned INDEPENDENTLY inside the
    // box - the box is the wider of them - so a centred clock centres its time row and its date row separately
    // rather than centring the block as a whole.  Above those sizes it is the box's own left edge, because
    // there the box is one row and is placed as a whole.
    uint16_t _rowLeft(uint16_t rowW);
    // Whether this clock glows: the stored setting AND a clock font to glow with.  One predicate because three
    // draws ask it - the time's glow, the seconds' glow and the colon's own off colour.
    bool _glowOn();
    uint16_t _forceflag = 0;
    bool dots = true;
    // The layout's two clock transforms (LayoutData.fullClock / .seconds).  Neither is derived from
    // the size any more: a panel's clock size says nothing about whether its layout wants a date
    // column or a seconds block, and the ILI9225 model hack is gone with them.
    bool _fullclock;
    bool _showSeconds;
    psFrameBuffer* _fb=nullptr;
    void _draw();
    void _clear();
    void _reset();
    void _getTimeBounds();
    void _printClock(bool force=false);
    // `push` is whether the erase itself must take the panel (see the definition): every caller wants that
    // except _printClock()'s force path, which repaints and displays the same window a few lines later.
    void _clearClock(bool push = true);
    bool _getTime();
    uint16_t _left();
    uint16_t _top();
    void _begin();
};

class BitrateWidget: public Widget {
  public:
    BitrateWidget() {}
    BitrateWidget(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor) { init(bconf, fgcolor, bgcolor); }
    ~BitrateWidget(){}
    using Widget::init;
    void init(BitrateConfig bconf, uint16_t fgcolor, uint16_t bgcolor);
    void setBitrate(uint16_t bitrate);
    void setFormat(BitrateFormat format);
  protected:
    BitrateFormat _format;
    char _buf[6];
    uint8_t _charWidth;
    uint16_t _dimension, _bitrate, _textheight;
    void _draw();
    void _clear();
    void _charSize(uint8_t textsize, uint8_t& width, uint16_t& height);
};

class PlayListWidget: public Widget {
  public:
    using Widget::init;
    void init(ScrollWidget* current);
    void drawPlaylist(uint16_t currentItem);
    inline uint16_t itemHeight(){ return _plItemHeight; }

    // Both playlist paths are compiled now: playlistPages is a runtime option (it was PLAYLIST_MODE_PAGED), so the
    // state and the two painters always exist and currentTop()/drawPlaylist() choose between them.
    void resetState() { _plPrevItem = 0; }
    uint16_t currentTop();

  private:
    ScrollWidget* _current;
    uint16_t _plItemHeight;
    int _plYStart;
    uint8_t _fillPlMenu(int from, uint8_t count);
    void _printPLitem(uint8_t pos, const char* item);

    uint8_t  _plPageSize;
    uint16_t _plPageStart, _plPrevItem;
    uint16_t _plPlaylistTop, _plPlaylistBottom;
    uint16_t _plTtemsCount, _plCurrentPos;
    bool     _paged;          // playlistPages() as init() read it; the option is a runtime setting now
    void _drawPaged(uint16_t currentItem);
    void _printPLitemPaged(uint16_t stationId, uint16_t y, bool selected, const char* name);
    void _drawFade(uint16_t currentItem);
};

#endif
#endif




