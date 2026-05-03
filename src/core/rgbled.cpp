#include "rgbled.h"

#if defined(RGB_LED_PIN) && (RGB_LED_PIN!=255) // ============================== Everything ignored if not defined ==============================

RgbLed::RgbLed()
  : strip(NUM_RGB_LEDS, RGB_LED_PIN, RGB_LED_ORDER + NEO_KHZ800),
    flashCount(0), lastFlash(0), inited(false),
    cycle(false), lastCycle(0), cycleState(0), state(RGB_OFF) {}

void RgbLed::init() {
  strip.begin();
  strip.setBrightness(64);
  strip.show();
  inited = true;
}

bool RgbLed::isInitialized() { return inited; }

void RgbLed::set(uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}

void RgbLed::playing() {
  state = RGB_PLAYING;
  if (!cycle) set(0, 128, 0);
}

void RgbLed::stopped() {
  state = RGB_STOPPED;
  if (!cycle) set(128, 0, 0);
}

void RgbLed::trackChange() {
  // flash blue a few times then restore previous state color
  flashCount = 6;
  lastFlash = millis();
}

void RgbLed::loop() {
  // track-change flash handling
  if (flashCount > 0) {
    if (millis() - lastFlash > 150) {
      // toggle between off and blue
      if (strip.getPixelColor(0) != 0) set(0,0,0); else set(0,0,128);
      lastFlash = millis();
      flashCount--;
      // when finished, restore color to current state
      if (flashCount == 0) {
        // restore according to state unless cycle is active
        if (!cycle) {
          if (state == RGB_PLAYING) set(0,128,0);
          else if (state == RGB_STOPPED) set(128,0,0);
          else set(0,0,0);
        }
      }
    }
  }

  // non-blocking cycle (Red -> Green -> Blue)
  if (cycle) {
    if (millis() - lastCycle > 1000) {
      lastCycle = millis();
      switch (cycleState) {
        case 0: set(255,0,0); break;
        case 1: set(0,255,0); break;
        default: set(0,0,255); break;
      }
      cycleState = (cycleState + 1) % 3;
    }
  }
}

#endif // defined(RGB_LED_PIN) && (RGB_LED_PIN!=255)

RgbLed rgbled;
