#include "core/options.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include "core/battery.h"
#include "core/config.h"
#include "core/controls.h"
#include "core/display.h"
#include "core/logging.h"
#include "core/mqtt.h"
#include "core/netserver.h"
#include "core/network.h"
#include "core/player.h"
#include "core/rgbled.h"
#include "core/telnet.h"
#include "pluginsManager/pluginsManager.h"
#ifdef USE_NEXTION
  #include "displays/nextion.h"
#endif

SET_LOOP_TASK_STACK_SIZE(LOOP_TASK_STACK_SIZE * 1024);

#if DSP_HSPI || TS_HSPI || VS_HSPI
  SPIClass SPI2(HSPI);
#endif

extern __attribute__((weak)) void ehradio_on_setup();

/* Prototype for battery-driven dimming handler */
void battery_dim_loop();

/* Shared state for battery-driven dim/critical handling.
   These variables are accessed from both loop() and battery_loop() which run in
   the same thread but at different times. Volatile prevents compiler optimizations
   that could reorder reads/writes, ensuring consistent state across function calls.
   battery_loop() updates the battery status, then loop() reads it to apply dim/sleep policy. */
static volatile bool battery_low_handled = false;
static volatile bool battery_critical_handled = false;
static volatile bool battery_critical_skipped = false; /* true while charging and critical to avoid repeated logs */
static uint8_t battery_saved_brightness = 0;
static bool battery_saved_valid = false;

void setup() {
  Serial.begin(115200);
  #if CORE_DEBUG_LEVEL > 0
    if (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_EXT) { // checking if this is a poweron boot
      delay(1000);
      BOOTLOG("Delay 1 second after cold boot to ensure serial logs are completely available (only when CORE_DEBUG_LEVEL > 0)...");
    }
  #endif

  if (REAL_LEDBUILTIN!=255) pinMode(REAL_LEDBUILTIN, OUTPUT);
  rgbled.init();
  // Initialize battery monitoring
  battery.init();

  if (ehradio_on_setup) ehradio_on_setup();
  pm.on_setup();
  config.init();
  display.init();
  player.init();
  battery.bootStatus();
  if (rgbled.isInitialized()) {
    if (player.isRunning()) rgbled.playing(); else rgbled.stopped();
  }
  network.begin();
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
    controls.init();
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    netserver.setBootReady(true);
    return;
  }
  if (SDC_CS!=255 && config.store.play_mode==PM_SDCARD) {
    display.putRequest(WAITFORSD, 0);
    BOOTLOG("SD Search");
  }
  config.initPlaylistMode();
  netserver.begin();
  telnet.begin();
  controls.init();
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #ifdef MQTT_ENABLE
    if (config.store.mqttenable) mqtt.init();
  #endif
  #if LED_INVERT
    if (REAL_LEDBUILTIN!=255) digitalWrite(REAL_LEDBUILTIN, true);
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  if (config.store.smartstart) {  // If smart start is enabled
    delay(1000);  // Allow DNS/TCP/SSL stack to stabilize after WiFi connect (esp. after soft restart)
    uint16_t stn = config.lastStation();
    if (stn > 0) {  // Only play if there's a valid station
      player.sendCommand({PR_PLAY, stn});
    }
  }
  config.startupServices();
  pm.on_end_setup();
  netserver.setBootReady(true);
}

void loop() {
  if (network.status == SOFT_AP) {
    network.loopImprov();
    if (network.dnsServer) network.dnsServer->processNextRequest();
  } else {
    telnet.loop();
  }
  
  rgbled.loop();
  battery.loop();
  /* Check battery status and apply dimming/deepsleep policy if needed */
  #if BRIGHTNESS_PIN!=255
    battery_dim_loop();
  #endif

  if (network.status == CONNECTED || network.status==SDREADY) {
    player.loop();
    config.processDeferredSaves();
  }
  controls.loop();
  netserver.loop();
}

#include "core/audiohandlers.h"

/**************************************************************************
*   Plugin BacklightDown.
*   Ver.1.0 (Maleksm) for ёРадио 20.12.2024
*   Ver.1.1 (Trip5) 2025.07.19
*   Ver.1.2 (Kasperaitis/Trip5) 2026.02.01
***************************************************************************/
#if (BRIGHTNESS_PIN!=255) && (defined(DOWN_LEVEL) || defined(DOWN_INTERVAL))
#include <Ticker.h>

/* Основные константы настроек */
#ifdef DOWN_LEVEL
  const uint8_t brightness_down_level = DOWN_LEVEL;
#else
  const uint8_t brightness_down_level = 2;   /* lowest level brightness (from 0 to 255) */
#endif
#ifdef DOWN_INTERVAL
  const uint16_t Out_Interval = DOWN_INTERVAL;
#else
  const uint16_t Out_Interval = 60;         /* interval for BacklightDown in sec (60 sec = 1 min) */
#endif

  Ticker backlightTicker;
  Ticker rampTicker;
  uint8_t current_brightness;

  void stepBacklight() {
    if (current_brightness > brightness_down_level) {
      current_brightness -= 2;
      if (current_brightness < brightness_down_level) current_brightness = brightness_down_level;
      analogWrite(BRIGHTNESS_PIN, current_brightness);
    } else {
      rampTicker.detach();
    }
  }

  void backlightDown() {
    if (network.status != SOFT_AP) {
      backlightTicker.detach();
      current_brightness = map(config.store.brightness, 0, 100, 0, 255);
      rampTicker.attach_ms(30, stepBacklight);
    }
  }

  void brightnessOn() {
    backlightTicker.detach();
    rampTicker.detach();
    analogWrite(BRIGHTNESS_PIN, map(config.store.brightness, 0, 100, 0, 255));
    backlightTicker.attach(Out_Interval, backlightDown);
  }

  /* battery_dim_loop() unified below */

  /* Backlight callback functions were here; moved below to ensure RGB callbacks are available even when backlight plugin isn't enabled */
  void ctrls_on_loop() {                            /* Backlight ON for reg. operations */
    if (!config.isScreensaver) {
      static uint32_t prevBlPinMillis;
      if ((display.mode() != PLAYER) && (millis() - prevBlPinMillis > 1000)) {
        prevBlPinMillis = millis();
        brightnessOn();
      }
    }
  }
#else  /*  #if BRIGHTNESS_PIN!=255 */
  void brightnessOn() { } /* No-op stub */
#endif

/* Unified battery dim/critical handler (works both with BacklightDown plugin or without).
   When plugin is available (DOWN_LEVEL/DOWN_INTERVAL) we call brightnessOn(); otherwise use config.setBrightness(false). */
#if BRIGHTNESS_PIN!=255
void battery_dim_loop() {
  BatteryStatus bat = battery.getStatus();

  /* Decide charging based on explicit detection or the existing inference logic only. */
  bool is_charging_present = bat.charging || bat.charging_inferred;

  /* Critical battery: stop playback, blank display and go to deep sleep (once)
     If a charger (TP4054) is present and the battery is charging, skip deep sleep
     while charging and notify once. Deep-sleep will occur once charging stops.
     Grace period: wait ~5 minutes after boot before allowing deep sleep. */
  if (bat.critical_battery && !battery_critical_handled && millis() > 300000) {
    if (is_charging_present) {
      if (!battery_critical_skipped) {
        battery_critical_skipped = true;
        FUNCTIONLOG("Battery", "Critical (%d%%) but charging - skipping deep sleep", bat.percentage);
      }
    } else {
      battery_critical_skipped = false;
      battery_critical_handled = true;
      FUNCTIONLOG("Battery", "Critical (%d%%) - entering deep sleep", bat.percentage);
      player.sendCommand({PR_STOP, 0});
      display.putRequest(NEWMODE, SCREENBLANK);
      delay(200);
      display.deepsleep();
      // Re-check battery status immediately before sleep to avoid race condition
      // (charging may have started during shutdown sequence)
      BatteryStatus final_check = battery.getStatus();
      if (final_check.charging) {
        FUNCTIONLOG("Battery", "Charging detected before sleep - aborting deep sleep");
        battery_critical_handled = false;
        return;
      }
      config.doSleepW();
    }
  }

  // Low battery: force fixed brightness percentage (skip dimming while charging)
  if (bat.low_battery && !battery_low_handled) {
    if (is_charging_present) {
      /* When charging (or charging trend detected), do not force a low-battery brightness — avoid flicker. */
      #ifdef BATTERY_DEBUG
        FUNCTIONLOG("Battery", "Low (%d%%) but charging/trend detected - skipping forced brightness", bat.percentage);
      #endif
    } else {
      battery_low_handled = true;
      if (!battery_saved_valid) { battery_saved_brightness = config.store.brightness; battery_saved_valid = true; }
      uint8_t target_pct = (uint8_t)BATTERY_DIM_BRIGHTNESS;
      if (target_pct > 100) target_pct = 100;
      FUNCTIONLOG("Battery", "Low (%d%%) - forcing brightness to %d%%", bat.percentage, target_pct);
      config.store.brightness = target_pct;
      #if defined(DOWN_LEVEL) || defined(DOWN_INTERVAL)
        brightnessOn();
      #else
        config.setBrightness(false);
      #endif
    }
  }

  // Restore when battery OK (with hysteresis) or charging/trend detected
  {
    int recover = (int)BATTERY_LOW_THRESHOLD + (int)BATTERY_RECOVER_HYSTERESIS_PCT;
    if (recover > 100) recover = 100;
    uint8_t recover_pct = (uint8_t)recover;
    bool recovered_by_pct = (bat.percentage >= recover_pct) && !bat.critical_battery;

    if ((battery_low_handled && recovered_by_pct) || (battery_critical_handled && !bat.critical_battery) || is_charging_present) {
      if (battery_low_handled) {
        battery_low_handled = false;
        if (battery_saved_valid) {
          config.store.brightness = battery_saved_brightness;
          battery_saved_valid = false;
          FUNCTIONLOG("Battery", "Recovered - restoring brightness to %d%%", config.store.brightness);
        }
        #if defined(DOWN_LEVEL) || defined(DOWN_INTERVAL)
          brightnessOn();
        #else
          config.setBrightness(false);
        #endif
      }
      if (battery_critical_handled && !bat.critical_battery) {
        battery_critical_handled = false;
        #if defined(DOWN_LEVEL) || defined(DOWN_INTERVAL)
          brightnessOn();
        #else
          config.setBrightness(false);
        #endif
      }
    }
  }
}
#endif

void ehradio_on_setup() {
  brightnessOn();
}

void player_on_track_change() {
  rgbled.trackChange();
  brightnessOn();
}

void player_on_start_play() {
  rgbled.playing();
  brightnessOn();
}

void player_on_stop_play() {
  rgbled.stopped();
  brightnessOn();
}


