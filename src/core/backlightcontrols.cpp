#include "backlightcontrols.h"

#if BRIGHTNESS_PIN!=255 && DSP_DIMMING_ENABLED
  #include "config.h"
  #include "display.h"
  #include "network.h"

  namespace {
    uint8_t brightnessPercentToPwm(uint8_t brightnessPercent) {
      return static_cast<uint8_t>(map(brightnessPercent, 0, 100, 0, 255));
    }
  }

  void BacklightControls::stepBacklightThunk() {
    backlightControls.stepBacklight();
  }

  void BacklightControls::backlightDownThunk() {
    backlightControls.backlightDown();
  }

  uint8_t BacklightControls::configuredBrightnessPwm() const {
    return brightnessPercentToPwm(config.store.brightness);
  }

  uint8_t BacklightControls::dimmedBrightnessPwm() const {
    const uint8_t dimmedBrightness = (config.store.dimmingBrightness > config.store.brightness)
      ? config.store.brightness
      : config.store.dimmingBrightness;
    return brightnessPercentToPwm(dimmedBrightness);
  }

  bool BacklightControls::dimmingEnabled() const {
    return config.store.dimmingEnabled;
  }

  void BacklightControls::applyConfiguredBrightness() {
    analogWrite(BRIGHTNESS_PIN, config.store.dspon ? configuredBrightnessPwm() : 0);
  }

  void BacklightControls::stepBacklight() {
    const uint8_t targetBrightness = dimmedBrightnessPwm();
    if (currentBrightness > targetBrightness) {
      currentBrightness = (currentBrightness - targetBrightness > 2)
        ? static_cast<uint8_t>(currentBrightness - 2)
        : targetBrightness;
      analogWrite(BRIGHTNESS_PIN, currentBrightness);
      return;
    }

    rampTicker.detach();
  }

  void BacklightControls::backlightDown() {
    backlightTicker.detach();
    if (!config.store.dspon || !dimmingEnabled() || network.status == SOFT_AP) {
      return;
    }

    currentBrightness = configuredBrightnessPwm();
    if (dimmedBrightnessPwm() >= currentBrightness) {
      return;
    }

    rampTicker.attach_ms(30, stepBacklightThunk);
  }

  void BacklightControls::init() {
    restart();
  }

  void BacklightControls::restart() {
    backlightTicker.detach();
    rampTicker.detach();
    applyConfiguredBrightness();

    if (config.store.dspon && dimmingEnabled()) {
      backlightTicker.attach(config.store.dimmingTimeout, backlightDownThunk);
    }
  }

  void BacklightControls::controlsLoop() {
    if (!config.isScreensaver) {
      if ((display.mode() != PLAYER) && (millis() - lastControlsWakeMillis > 1000)) {
        lastControlsWakeMillis = millis();
        restart();
      }
    }
  }
#endif

BacklightControls backlightControls;