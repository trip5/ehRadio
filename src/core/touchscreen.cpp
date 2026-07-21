#include "options.h"
#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
#include <Arduino.h>
#include "config.h"
#include "controls.h"
#include "display.h"
#include "logging.h"
#include "network.h"
#include "utility.h"
#include "player.h"
#include "touchscreen.h"

#ifndef TS_X_MIN
  #define TS_X_MIN              400
#endif
#ifndef TS_X_MAX
  #define TS_X_MAX              3800
#endif
#ifndef TS_Y_MIN
  #define TS_Y_MIN              260
#endif
#ifndef TS_Y_MAX
  #define TS_Y_MAX              3800
#endif
#ifndef TS_STEPS
  #define TS_STEPS              40
#endif
#ifndef TS_SWIPE_THRESHOLD_PX
  #if TS_MODEL == TS_MODEL_XPT2046
    #define TS_SWIPE_THRESHOLD_PX  20   // resistive: needs jitter filtering
  #else
    #define TS_SWIPE_THRESHOLD_PX  12   // capacitive (GT911, FT6336): clean signal
  #endif
#endif
#ifndef TS_COOLDOWN_MS
  #define TS_COOLDOWN_MS         150
#endif
#ifndef TS_DEEPSLEEP_MS
  #define TS_DEEPSLEEP_MS        5000
#endif
#ifndef TS_DOUBLETAP_MS
  #define TS_DOUBLETAP_MS        400
#endif

#if TS_MODEL==TS_MODEL_XPT2046
  #include <XPT2046_Touchscreen.h>
  XPT2046_Touchscreen ts(TS_CS);
  typedef TS_Point TSPoint;
#elif TS_MODEL==TS_MODEL_GT911
 #include <TAMC_GT911.h>
  TAMC_GT911 ts = TAMC_GT911(TS_SDA, TS_SCL, TS_INT, TS_RST, 0, 0);
  typedef TP_Point TSPoint;
#elif TS_MODEL==TS_MODEL_FT6336
  #include "../libraries/FT6336_Touchscreen/FT6336.h"
  FT6336 ts = FT6336(TS_SDA, TS_SCL, TS_INT, TS_RST, 0, 0);
  typedef FT_Point TSPoint;
#endif

void TouchScreen::init(uint16_t w, uint16_t h) {
  
#if TS_MODEL==TS_MODEL_XPT2046
  #if defined(TS_SPI) && (TS_SPI == 'B') && defined(SPIB_SCK)
    ts.begin(SPIB);
  #elif defined(TS_SPI) && (TS_SPI == 'A')
    ts.begin(SPIA);
  #else
    ts.begin();
  #endif
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.begin();
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.begin();
  ts.setRotation(config.store.fliptouch?2:0);
#endif
  _width  = w;
  _height = h;
#if TS_MODEL==TS_MODEL_GT911
  ts.setResolution(_width, _height);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.setResolution(_width, _height);
#endif
}

tsDirection_e TouchScreen::_tsDirection(uint16_t x, uint16_t y) {
  int16_t dX = x - _oldTouchX;
  int16_t dY = y - _oldTouchY;
  if (abs(dX) > TS_SWIPE_THRESHOLD_PX || abs(dY) > TS_SWIPE_THRESHOLD_PX) {
    if (abs(dX) > abs(dY)) {
      if (dX > 0) {
        return TSD_RIGHT;
      } else {
        return TSD_LEFT;
      }
    } else {
      if (dY > 0) {
        return TSD_DOWN;
      } else {
        return TSD_UP;
      }
    }
  } else {
    return TSD_REQUEST;
  }
}

void TouchScreen::flip() {
#if TS_MODEL==TS_MODEL_XPT2046
  ts.setRotation(config.store.fliptouch?3:1);
#endif
#if TS_MODEL==TS_MODEL_GT911
  ts.setRotation(config.store.fliptouch?0:2);
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.setRotation(config.store.fliptouch?2:0);
#endif
}

void TouchScreen::loop() {
  uint16_t touchX, touchY;
  static bool wastouched = true;
  static uint32_t touchLongPress;
  static tsDirection_e direct;
  static uint16_t touchVol, touchStation;
  if (!_checklpdelay(20, _touchdelay)) return;
#if TS_MODEL==TS_MODEL_GT911
  ts.read();
#endif
#if TS_MODEL==TS_MODEL_FT6336
  ts.read();
#endif
  bool istouched = _istouched();
  if (istouched) {
    if (!_tapPending && _touchCooldown && (millis() - _touchCooldown < TS_COOLDOWN_MS)) return;
    _touchCooldown = 0;
  #if TS_MODEL==TS_MODEL_XPT2046
    TSPoint p = ts.getPoint();
    touchX = map(p.x, TS_X_MIN, TS_X_MAX, 0, _width);
    touchY = map(p.y, TS_Y_MIN, TS_Y_MAX, 0, _height);
  #elif TS_MODEL==TS_MODEL_GT911
    TSPoint p = ts.points[0];
    touchX = p.x;
    touchY = p.y;
  #elif TS_MODEL==TS_MODEL_FT6336
    TSPoint p = ts.points[0];
    touchX = p.x;
    touchY = p.y;
  #endif
  if (!wastouched) { /*     START TOUCH     */
      if (_tapPending && (millis() - _tapPendingTime < TS_DOUBLETAP_MS)) {
        _isDoubleTap = true;
        _tapPending = false;
      } else {
        _tapPending = false;
      }
      _oldTouchX = touchX;
      _oldTouchY = touchY;
      touchVol = touchX;
      touchStation = touchY;
      direct = TSD_REQUEST;
      touchLongPress=millis();
    } else { /*     SWIPE TOUCH     */
      if (direct == TSD_REQUEST) {
        direct = _tsDirection(touchX, touchY);
        if (direct != TSD_REQUEST) {
          _tapPending = false;
          _isDoubleTap = false;
        }
      }
      if (direct == TSD_REQUEST && (millis() - touchLongPress >= TS_DEEPSLEEP_MS)) {
        if (network.status != SDOFFLINE) {
          #ifndef DEEP_SLEEP_DISABLE
            display.putRequest(NEWMODE, SLEEPING);
          #endif
        }
      }
      switch (direct) {
        case TSD_LEFT:
        case TSD_RIGHT: {
            touchLongPress=millis();
            if (display.mode()==PLAYER || display.mode()==VOL) {
              int16_t xDelta = map(abs(touchVol - touchX), 0, _width, 0, TS_STEPS);
              display.putRequest(NEWMODE, VOL);
              if (xDelta>1) {
                controls.controlsEvent((touchVol - touchX)<0);
                touchVol = touchX;
              }
            }
            break;
          }
        case TSD_UP:
        case TSD_DOWN: {
            touchLongPress=millis();
            if (display.mode()==PLAYER || display.mode()==STATIONS) {
              int16_t yDelta = map(abs(touchStation - touchY), 0, _height, 0, TS_STEPS);
              display.putRequest(NEWMODE, STATIONS);
              if (yDelta>1) {
                controls.controlsEvent((touchStation - touchY)<0);
                touchStation = touchY;
              }
            }
            break;
          }
        default:
            break;
      }
    }
    if (config.store.dbgtouch) {
      FUNCTIONLOG("Touch", "x = %d, y = %d", p.x, p.y);
    }
  } else {
    if (wastouched) {/*     END TOUCH     */
      if (direct == TSD_REQUEST) {
        uint32_t pressTicks = millis()-touchLongPress;
        if (pressTicks >= TS_DEEPSLEEP_MS) {
          #ifndef DEEP_SLEEP_DISABLE
            utility.doSleepW();
          #endif
        } else if (pressTicks > 50) {
          if (_isDoubleTap) {
            controls.onBtnClick(EVT_BTN_MODE);
            _isDoubleTap = false;
          } else {
            _tapPending = true;
            _tapPendingTime = millis();
          }
        }
      }
      direct = TSD_STAY;
      _touchCooldown = millis();
    }
    if (_tapPending && (millis() - _tapPendingTime >= TS_DOUBLETAP_MS)) {
      controls.onBtnClick(EVT_BTN_PLAY);
      _tapPending = false;
    }
  }
  wastouched = istouched;
}

bool TouchScreen::_checklpdelay(int m, uint32_t &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

bool TouchScreen::_istouched() {
#if TS_MODEL==TS_MODEL_XPT2046
  return ts.touched();
#elif TS_MODEL==TS_MODEL_GT911
  return ts.isTouched;
#elif TS_MODEL==TS_MODEL_FT6336
  return ts.isTouched;
#endif
}

#endif  // TS_MODEL!=TS_MODEL_UNDEFINED
