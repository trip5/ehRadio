#ifndef backlightcontrols_h
#define backlightcontrols_h

#include "options.h"
#include <Arduino.h>

#if BRIGHTNESS_PIN!=255 && DSP_DIMMING_ENABLED
#include <Ticker.h>

class BacklightControls {
public:
  void init();
  void restart();
  void controlsLoop();

private:
  static void stepBacklightThunk();
  static void backlightDownThunk();
  void stepBacklight();
  void backlightDown();
  void applyConfiguredBrightness();
  uint8_t configuredBrightnessPwm() const;
  uint8_t dimmedBrightnessPwm() const;
  bool dimmingEnabled() const;

  Ticker backlightTicker;
  Ticker rampTicker;
  uint8_t currentBrightness = 0;
  unsigned long lastControlsWakeMillis = 0;
};

#else

class BacklightControls {
public:
  void init() {}
  void restart() {}
  void controlsLoop() {}
};

#endif

extern BacklightControls backlightControls;

#endif