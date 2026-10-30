#include "core/options.h"
#include "core/coredump.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>   // heap_caps_get_largest_free_block, for the [PSRAM] contiguous figure
#include "core/battery.h"
#include "core/backlightcontrols.h"
#include "core/config.h"
#include "core/controls.h"
#include "core/crashreport.h"
#include "core/display.h"
#include "core/logging.h"
#include "core/mqtt.h"
#include "core/netserver.h"
#include "core/network.h"
#include "core/player.h"
#include "core/rgbled.h"
#include "core/sdmanager.h"
#include "core/utility.h"   // Utility::serviceSleep(): the main loop performs a sleep a Ticker asked for
#ifdef USE_SD
  #include "core/filemanager.h"   // SD card file manager: /sdman and its mode
#endif
#include "core/startup.h"
#include "core/telnet.h"
#include "displays/tools/psframebuffer.h"
#include "displays/tools/dspstats.h"

SET_LOOP_TASK_STACK_SIZE(LOOP_TASK_STACK_SIZE * 1024);

// ms an iteration of loop() may take before it reports which stage blocked (moved out of options.h).
static constexpr uint32_t MAIN_LOOP_STALL_MS = 500;

size_t psramFrameBufferBytes = 0;   // set by subsystems, read by Core Monitor

#ifdef CORE_MONITOR
  // The counters themselves are declared in displays/tools/dspstats.h
  extern TaskHandle_t dspTaskHandle;
  extern TaskHandle_t nsTaskHandle;
  static uint32_t cmMainCount     = 0;
  static uint32_t cmMaxMainLoop   = 0;
  static uint32_t cmLoopStart     = 0;
  static unsigned long cmLastPrint = 0;
  static uint8_t cmEtcCount      = 0;  // CORE_MONITOR_ETC_LOOPS counter
#endif

void setup() {
  #if !defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxBufferSize(4096); // activate ring-buffer
  #endif
  Serial.begin(115200);
  {
    const bool dumpedCrash = crashDumpAvailable();
    #if (CORE_DEBUG_LEVEL > 0) || defined(ALL_DEBUG_LOGS)
      if (dumpedCrash || esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_EXT) {
        delay(1000);
        BOOTLOG("1 second delay after cold boot to ensure serial logs are available (CORE_DEBUG_LEVEL > 0 or ALL_DEBUG_LOGS)...");
      }
    #else
      // And a del
      BOOTLOG("1 second delay after cold boot to ensure crash log is available...");
      if (dumpedCrash) delay(1000);
    #endif
    if (dumpedCrash) crashDumpReport();   // the previous boot's crash, reported into this boot's serial log
  }
  #if defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxTimeoutMs(BOOTLOG_TX_TIMEOUT_MS);
  #endif

  startup.deassertCsPins();
  if (LED_PIN!=255) pinMode(LED_PIN, OUTPUT);
  rgbled.init();
  battery.init();
  BOOTTIMELOG("serial & deassert & rgbled & battery");
  config.init();
  controls.checkButtonsHeldOnBoot(); // check for hold-to-SD before network decision
  backlightControls.init();
  BOOTTIMELOG("config.init & controls & backlight");
  display.init();
  BOOTTIMELOG("display.init");
  const bool offlineBoot = (network.offlineMode || config.store.SDoffline);
  player.init();
  BOOTTIMELOG("player.init");
  display.putRequest(BOOTVERSION); // follows player.init because of "VS1053" message
  startup.checkLittleFSandVer();
  BOOTTIMELOG("checkLittleFSandVer");
  battery.bootStatus();
  BOOTTIMELOG("battery.bootStatus");
  if (offlineBoot) {
    startup.sdOfflineMode();
    BOOTTIMELOG("sdOfflineMode");
  } else {
    startup.checkSafeMode();
    network.begin();
    BOOTTIMELOG("network.begin (scan & join)");
  }
  if (network.status != CONNECTED && network.status != SDOFFLINE) {
    netserver.begin();
    BOOTTIMELOG("netserver.begin (AP)");
    netserver.startLoopTask();
    controls.init();
    BOOTTIMELOG("netserver task & controls.init");
    display.putRequest(DSP_START);
    while(!display.ready()) delay(10);
    BOOTTIMELOG("display DSP_START");
    netserver.setBootReady(true);
    BOOTTIMELOG("setBootReady");
    return;
  }
  startup.getDefaultPlaylist();
  BOOTTIMELOG("getDefaultPlaylist");
  if (SD_CS!=255 && config.store.play_mode==PM_SDCARD) {
    display.putRequest(WAITFORSD, 0);
    BOOTLOG("SD Search");
  }
  startup.cleanStaleSearchResults();
  BOOTTIMELOG("cleanStaleSearchResults");
  config.initPlaylistMode();
  BOOTTIMELOG("initPlaylistMode");
  // The WebUI is deliberately held back until the startup services are out of the way.  netserver.begin() is
  // where the request queue, the listener, mDNS and the PSRAM WebUI file cache (up to ~300 KB) are allocated,
  // and a WebUI connection landing while a service is making its own TLS connection is what overwhelms the
  // network stack.  Startup::loop() starts it the moment nothing is borrowing that memory; nothing needs the
  // server in between (requestOnChange is null-guarded), and the footer IP is the indicator, because it is not
  // drawn until the server is listening.
  if (network.status == SDOFFLINE) netserver.begin();   // offline mode runs no services, so nothing to wait for
  if (network.status != SDOFFLINE) telnet.begin();      // a listening socket and no task: telnet stays early
  BOOTTIMELOG("telnet (netserver deferred)");
  controls.init();
  BOOTTIMELOG("controls.init");
  display.putRequest(DSP_START);
  while(!display.ready()) delay(10);
  BOOTTIMELOG("display DSP_START");
  if (config.store.mqttenable && network.status != SDOFFLINE) mqtt.init();
  BOOTTIMELOG("mqtt.init");
  #if LED_INVERT
    if (LED_PIN!=255) digitalWrite(LED_PIN, true);
  #endif
  if (config.getMode()==PM_SDCARD) player.initHeaders(config.station.url);
  BOOTTIMELOG("player.initHeaders");
  player.lockOutput=false;
  if (!startup.safeMode() && config.store.smartstart) {  // If smart start is enabled (suppressed on a safe-mode boot)
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
  BOOTTIMELOG("smartstart (incl. 1s settle)");
  if (network.status != SDOFFLINE) startup.startupServices();  // needs WiFi — skip in offline SD mode
  BOOTTIMELOG("startupServices kickoff");
  netserver.setBootReady(true);
  config.saveValue(&config.store.SDoffline, false);
  BOOTTIMELOG("setBootReady");
}

void loop() {
  #ifdef CORE_MONITOR
    cmLoopStart = micros();
  #endif
  // Stage attribution, always on: an iteration over MAIN_LOOP_STALL_MS names the blocking stage, which is
  // the whole diagnosis (this is what found the stream connect in the player stage).
  const uint32_t tStage0 = micros();

  if (network.status == SOFT_AP) {
    network.loopImprov();
    if (network.dnsServer) network.dnsServer->processNextRequest();
  } else {
    telnet.loop();
  }
  const uint32_t tStage1 = micros();

  rgbled.loop();
  battery.loop();
  const uint32_t tStage2 = micros();

  controls.loop();
  const uint32_t tStage3 = micros();

  if (network.status == CONNECTED || network.status == SDOFFLINE) {
    player.loop();
    config.processDeferredSaves();
  }
  const uint32_t tStage4 = micros();

  startup.loop();
  const uint32_t tStage5 = micros();

  {
    const uint32_t total = tStage5 - tStage0;
    if (total > ((uint32_t)MAIN_LOOP_STALL_MS * 1000UL)) {
      const uint32_t staged[5] = {
        tStage1 - tStage0,   // net: improv or telnet
        tStage2 - tStage1,   // rgbled + battery
        tStage3 - tStage2,   // controls
        tStage4 - tStage3,   // player + deferred saves
        tStage5 - tStage4,   // startup
      };
      static const char* const stageNames[5] = {"net/telnet", "rgbled+battery", "controls", "player+saves", "startup"};
      uint8_t worst = 0;
      for (uint8_t i = 1; i < 5; i++) if (staged[i] > staged[worst]) worst = i;
      WARNINGLOG("Main loop stalled %lums, worst stage %s %lums - net %lu, hw %lu, ctl %lu, player %lu, startup %lu",
          (unsigned long)(total / 1000UL), stageNames[worst], (unsigned long)(staged[worst] / 1000UL),
          (unsigned long)(staged[0] / 1000UL), (unsigned long)(staged[1] / 1000UL),
          (unsigned long)(staged[2] / 1000UL), (unsigned long)(staged[3] / 1000UL),
          (unsigned long)(staged[4] / 1000UL));
    }
  }

  #ifdef CORE_MONITOR
    cmMainCount++;
    uint32_t cmDur = micros() - cmLoopStart;
    if (cmDur > cmMaxMainLoop) cmMaxMainLoop = cmDur;
    if (millis() - cmLastPrint >= 5000) {
      // Rates are per MEASURED second: the window is 5000ms plus this block's own printing time, stamped at the
      // end of it, so a fixed "/5" inflated every figure by 5-7%.
      const uint32_t elapsed = (uint32_t)(millis() - cmLastPrint);
      const float perSec = 1000.0f / (float)(elapsed ? elapsed : 1);
      uint32_t d = cmDspLoopCount;  cmDspLoopCount = 0;
      uint32_t m = cmMainCount;     cmMainCount = 0;
      uint32_t mx = cmMaxMainLoop;  cmMaxMainLoop = 0;
      uint32_t gl = cmGlyphCount;   cmGlyphCount = 0;
      uint32_t pc = cmPreTextCalls; cmPreTextCalls = 0;
      uint32_t ph = cmPreTextHits;  cmPreTextHits = 0;
      uint32_t fi = cmFillCount;    cmFillCount = 0;
      uint32_t pu = cmPushCount;    cmPushCount = 0;
      // Both figures are per TASK, labelled with the core each one runs on: field 1 the display task, field 2 this loop.
      FUNCTIONLOG("Core.monitor", "DspTask(core%u) loops/s: %u (%.2fms/loop), Main(core%u) loops/s: %u (%.2fms/loop), Max Main Loop Time: %.3fms, Free Heap: %u",
          (unsigned)cmDspCore,
          (unsigned)(d * perSec), d ? (float)elapsed / (float)d : 0.0f,
          (unsigned)xPortGetCoreID(),
          (unsigned)(m * perSec), m ? (float)elapsed / (float)m : 0.0f,
          mx / 1000.0f, (unsigned)ESP.getFreeHeap());
      // What the display task did with the time: work units per second, and the preText hit rate.
      FUNCTIONLOG("Core.monitor", "DspTask work/s: glyphs %u, fills %u, preText %u (hit %u%%), fb flushes %u (%.2f/loop)",
          (unsigned)(gl * perSec), (unsigned)(fi * perSec), (unsigned)(pc * perSec),
          pc ? (unsigned)(ph * 100UL / pc) : 0U,
          (unsigned)(pu * perSec), d ? (float)pu / (float)d : 0.0f);
      #ifndef CONFIG_FREERTOS_UNICORE
        FUNCTIONLOG("Core.monitor", "Core layout: core0 " CORE_0 ", core1 " CORE_1);
      #endif
      FUNCTIONLOG("Core.monitor", "High Water Mark (free bytes in stacks): Main: %u, Display: %u, Netserver: %u",
          (unsigned)uxTaskGetStackHighWaterMark(NULL),
          (unsigned)(dspTaskHandle ? uxTaskGetStackHighWaterMark(dspTaskHandle) : 0),
          (unsigned)(nsTaskHandle  ? uxTaskGetStackHighWaterMark(nsTaskHandle)  : 0));
      // LittleFS + PSRAM info — rate-limited by CORE_MONITOR_ETC_LOOPS
      // Integrity every cycle, not every fifth: the TLSF assertion that fires inside block_locate_free trips within
      // seconds of a corruption appearing, so a 25 s window usually misses it.  LittleFS and PSRAM detail stays
      // rate-limited.
      const bool heapInternalOk = heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true);
      const bool heapPsramOk = !psramFound() || heap_caps_check_integrity(MALLOC_CAP_SPIRAM, true);
      const char* heapVerdict =
            (heapInternalOk && heapPsramOk)  ? " | heap ok"
          : (!heapInternalOk && !heapPsramOk) ? " | heap CORRUPT (internal+PSRAM)"
          : !heapInternalOk                   ? " | heap CORRUPT (internal)"
                                              : " | heap CORRUPT (PSRAM)";
      // Internal DRAM that can serve a byte: MALLOC_CAP_DEFAULT also matches PSRAM (it reported 7.9 MB), plain INTERNAL
      // also matches IRAM, which is 32-bit only - so neither reports the heap wifi_malloc allocates from.
      const uint32_t internal8 = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
      FUNCTIONLOG("Heap", "8bit: %uKB free, %uKB largest | PSRAM: %uKB free, %uKB largest%s",
          heap_caps_get_free_size(internal8) / 1024,
          heap_caps_get_largest_free_block(internal8) / 1024,
          heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024,
          heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024,
          heapVerdict);
      if (!heapInternalOk || !heapPsramOk) {
        // One ERROR per boot, plus a note that outlives the panic: the archive prints it above the crashdump text, so
        // the record says the heap was already broken before the allocation that tripped over it.
        static bool heapErrorLogged = false;
        if (!heapErrorLogged) {
          heapErrorLogged = true;
          ERRORLOG("heap corruption detected %lus after boot: internal %s, PSRAM %s",
                   (unsigned long)(millis() / 1000UL),
                   heapInternalOk ? "ok" : "CORRUPT", heapPsramOk ? "ok" : "CORRUPT");
        }
        char heapNote[160];
        snprintf(heapNote, sizeof(heapNote), "8bit free %uKB largest %uKB, internal %s, PSRAM %s, uptime %lus",
                 (unsigned)heap_caps_get_free_size(internal8) / 1024,
                 (unsigned)heap_caps_get_largest_free_block(internal8) / 1024,
                 heapInternalOk ? "ok" : "CORRUPT", heapPsramOk ? "ok" : "CORRUPT",
                 (unsigned long)(millis() / 1000UL));
        coredump::noteHeapCorruption(heapNote);
      }
      if (++cmEtcCount >= CORE_MONITOR_ETC_LOOPS) {
        cmEtcCount = 0;
        FUNCTIONLOG("LittleFS", "Used: %u / %u bytes, Free: %u bytes", LittleFS.usedBytes(), LittleFS.totalBytes(), LittleFS.totalBytes() - LittleFS.usedBytes());
        if (psramFound()) {
          size_t psramTotal = ESP.getPsramSize();
          size_t psramUsed  = psramTotal - ESP.getFreePsram();
          size_t audioFill  = player.inBufferFilled();
          FUNCTIONLOG("PSRAM", "Used: %uKB / %uKB: Framebuffer: %uKB, VU FFT: %uKB, WebUI Cache: %uKB, Audio buffered: %uKB, Contiguous Free: %uKB",
              psramUsed / 1024,
              psramTotal / 1024,
              psramFrameBufferBytes / 1024,
              vuPsramBytes / 1024,
              netserver.getFileCache().totalBytes() / 1024,
              audioFill / 1024,
              heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024);
        }
      }
      SERIALLOGLF();   // blank line, so consecutive reports are separable at a glance
      cmLastPrint = millis();
    }
  #endif

  #ifdef USE_SD
    filemanager.loop();
  #endif

  /* A sleep the timers asked for is performed HERE, on the main loop.  Both requesters (the sleep timer in ticks() and
     the Ticker armed by sleepForAfter()) are esp_timer callbacks, and display.deepsleep() plus a telnet-capable log line
     do not belong on that task. */
  utility.serviceSleep();
  coredump::service();   // black box: archive the previous boot's crash once the clock can date it
}


