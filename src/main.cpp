#include "core/options.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <esp_system.h>
#include "core/battery.h"
#include "core/backlightcontrols.h"
#include "core/config.h"
#include "core/controls.h"
#include "core/display.h"
#include "core/logging.h"
#include "core/mqtt.h"
#include "core/netserver.h"
#include "core/network.h"
#include "core/player.h"
#include "core/rgbled.h"
#include "core/startup.h"
#include "core/telnet.h"
#ifdef USE_NEXTION
  #include "displays/nextion.h"
#endif

SET_LOOP_TASK_STACK_SIZE(LOOP_TASK_STACK_SIZE * 1024);

#ifdef CORE_MONITOR
  extern volatile uint32_t cmDspLoopCount;
  extern TaskHandle_t dspTaskHandle;
  extern TaskHandle_t nsTaskHandle;
  static uint32_t cmMainCount    = 0;
  static uint32_t cmMaxMainLoop  = 0;
  static uint32_t cmLoopStart    = 0;
  static unsigned long cmLastPrint = 0;
#endif

void setup() {
  Serial.begin(115200);
  #if CORE_DEBUG_LEVEL > 0
    if (esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_EXT) { // checking if this is a poweron boot
      delay(1000);
      BOOTLOG("Delay 1 second after cold boot to ensure serial logs are completely available (only when CORE_DEBUG_LEVEL > 0)...");
    }
  #endif

  if (LED_PIN!=255) pinMode(LED_PIN, OUTPUT);
  rgbled.init();
  // Initialize battery monitoring
  battery.init();
  config.init();
  backlightControls.init();
  display.init();
  player.init();
  battery.bootStatus();
  network.begin();
  if (network.status != CONNECTED && network.status!=SDREADY) {
    netserver.begin();
    netserver.startLoopTask();
    controls.init();
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    netserver.setBootReady(true);
    return;
  }
  if (SD_CS!=255 && config.store.play_mode==PM_SDCARD) {
    display.putRequest(WAITFORSD, 0);
    BOOTLOG("SD Search");
  }
  config.initPlaylistMode();
  netserver.begin();
  netserver.startLoopTask();
  telnet.begin();
  controls.init();
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  #ifdef MQTT_ENABLE
    if (config.store.mqttenable) mqtt.init();
  #endif
  #if LED_INVERT
    if (LED_PIN!=255) digitalWrite(LED_PIN, true);
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  player.lockOutput=false;
  if (config.store.smartstart) {  // If smart start is enabled
    delay(1000);  // Allow DNS/TCP/SSL stack to stabilize after WiFi connect (esp. after soft restart)
    if (config.getMode() == PM_WEB) {
      player.resumeLastWebSource();
    } else {
      uint16_t stn = config.lastStation();
      if (stn > 0) {  // Only play if there's a valid station
        player.sendCommand({PR_PLAY, stn});
      }
    }
  }
  startup.startupServices();
  netserver.setBootReady(true);
}

void loop() {
  #ifdef CORE_MONITOR
    cmLoopStart = micros();
  #endif
  if (network.status == SOFT_AP) {
    network.loopImprov();
    if (network.dnsServer) network.dnsServer->processNextRequest();
  } else {
    telnet.loop();
  }
  
  rgbled.loop();
  battery.loop();
  battery.applyPowerPolicy();

  if (network.status == CONNECTED || network.status==SDREADY) {
    player.loop();
    config.processDeferredSaves();
  }
  controls.loop();
  #ifdef CORE_MONITOR
    cmMainCount++;
    uint32_t cmDur = micros() - cmLoopStart;
    if (cmDur > cmMaxMainLoop) cmMaxMainLoop = cmDur;
    if (millis() - cmLastPrint >= 5000) {
      uint32_t d = cmDspLoopCount;  cmDspLoopCount = 0;
      uint32_t m = cmMainCount;     cmMainCount = 0;
      uint32_t mx = cmMaxMainLoop;  cmMaxMainLoop = 0;
      #ifdef CONFIG_FREERTOS_UNICORE
        FUNCTIONLOG("Core.monitor", "Core0 loops/s: %u (%.2fms/loop) | Core0(Main) loops/s: %u (%.2fms/loop) | MaxMainLoopUs: %u | Heap: %u",
            d/5, d>0 ? 5000.0f/d : 0.0f, m/5, m>0 ? 5000.0f/m : 0.0f, mx, (unsigned)ESP.getFreeHeap());
      #else
        FUNCTIONLOG("Core.monitor", "Core0" CORE_0 " loops/s: %u (%.2fms/loop) | Core1" CORE_1 " loops/s: %u (%.2fms/loop) | MaxMainLoopUs: %u | Heap: %u",
            d/5, d>0 ? 5000.0f/d : 0.0f, m/5, m>0 ? 5000.0f/m : 0.0f, mx, (unsigned)ESP.getFreeHeap());
      #endif
      FUNCTIONLOG("Core.monitor", "HWM: Loop=%u Disp=%u nsLoop=%u",
          (unsigned)uxTaskGetStackHighWaterMark(NULL),
          (unsigned)(dspTaskHandle ? uxTaskGetStackHighWaterMark(dspTaskHandle) : 0),
          (unsigned)(nsTaskHandle  ? uxTaskGetStackHighWaterMark(nsTaskHandle)  : 0));
      cmLastPrint = millis();
    }
  #endif
}


