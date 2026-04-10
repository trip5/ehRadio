#include "rgbled.h"

#if defined(RGB_LED_PIN) && (RGB_LED_PIN!=255) // ============================== Everything ignored if not defined ==============================

#include <Adafruit_NeoPixel.h>

#ifndef RGB_LED_ORDER
  #define RGB_LED_ORDER NEO_GRB
#endif

#define NUM_RGB_LEDS 1
static Adafruit_NeoPixel strip(NUM_RGB_LEDS, RGB_LED_PIN, RGB_LED_ORDER + NEO_KHZ800);
static uint8_t flashCount = 0;
static unsigned long lastFlash = 0;
static bool _rgb_inited = false;

// cycle state
static bool _cycle = false;
static unsigned long _lastCycle = 0;
static uint8_t _cycleState = 0;

// current state tracking
enum rgb_state_e { RGB_OFF=0, RGB_PLAYING=1, RGB_STOPPED=2 };
static rgb_state_e _state = RGB_OFF;

void rgbled_init() {
  #if defined(RGB_LED_PIN) && (RGB_LED_PIN!=255)
    strip.begin();
    strip.setBrightness(64);
    strip.show();
    _rgb_inited = true;
  #endif
}

bool rgbled_is_initialized() { return _rgb_inited; }

void rgbled_set(uint8_t r, uint8_t g, uint8_t b) {
  strip.setPixelColor(0, strip.Color(r, g, b));
  strip.show();
}

void rgbled_playing() {
  _state = RGB_PLAYING;
  if (!_cycle) rgbled_set(0, 128, 0);
}

void rgbled_stopped() {
  _state = RGB_STOPPED;
  if (!_cycle) rgbled_set(128, 0, 0);
}

void rgbled_trackchange() {
  // flash blue a few times then restore previous state color
  flashCount = 6;
  lastFlash = millis();
}

void rgbled_loop() {
  // track-change flash handling
  if (flashCount > 0) {
    if (millis() - lastFlash > 150) {
      // toggle between off and blue
      if (strip.getPixelColor(0) != 0) rgbled_set(0,0,0); else rgbled_set(0,0,128);
      lastFlash = millis();
      flashCount--;
      // when finished, restore color to current state
      if (flashCount == 0) {
        // restore according to _state unless cycle is active
        if (!_cycle) {
          if (_state == RGB_PLAYING) rgbled_set(0,128,0);
          else if (_state == RGB_STOPPED) rgbled_set(128,0,0);
          else rgbled_set(0,0,0);
        }
      }
    }
  }

  // non-blocking cycle (Red -> Green -> Blue)
  if (_cycle) {
    if (millis() - _lastCycle > 1000) {
      _lastCycle = millis();
      switch (_cycleState) {
        case 0: rgbled_set(255,0,0); break;
        case 1: rgbled_set(0,255,0); break;
        default: rgbled_set(0,0,255); break;
      }
      _cycleState = (_cycleState + 1) % 3;
    }
  }
}

#else // ============================== No-op stubs when RGB_LED_PIN not defined ==============================

void rgbled_init() {}
void rgbled_loop() {}
void rgbled_set(uint8_t r, uint8_t g, uint8_t b) {}
void rgbled_playing() {}
void rgbled_stopped() {}
void rgbled_trackchange() {}

bool rgbled_is_initialized() { return false; }
#endif
