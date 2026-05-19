#ifndef touchscreen_h
#define touchscreen_h

enum tsDirection_e { TSD_STAY, TSD_LEFT, TSD_RIGHT, TSD_UP, TSD_DOWN, TSD_REQUEST };

class TouchScreen {
  public:
    TouchScreen() {}
    void init(uint16_t w, uint16_t h);
    void flip();
    void loop();
  private:
    uint16_t _oldTouchX = 0, _oldTouchY = 0, _width = 0, _height = 0;
    uint32_t _touchdelay = 0;
    tsDirection_e _tsDirection(uint16_t x, uint16_t y);
    bool _checklpdelay(int m, uint32_t &tstamp);
    bool _istouched();
};

extern TouchScreen touchscreen;

#endif
