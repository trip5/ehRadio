#include "core/options.h"
#include "core/heapq.h"
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
#ifndef MAIN_LOOP_STALL
  #define MAIN_LOOP_STALL 500 // ms: measure all Main Loop stalls above this number (<30 becomes very noisy); WARNINGS_LOG must be on to observe this
#endif
static constexpr uint32_t MAIN_LOOP_STALL_MS = MAIN_LOOP_STALL * 1000;

size_t psramFrameBufferBytes = 0;   // set by subsystems, read by Core Monitor

#ifdef CORE_MONITOR
  // The counters themselves are declared in displays/tools/dspstats.h
  extern TaskHandle_t dspTaskHandle;
  extern TaskHandle_t nsTaskHandle;
  static uint32_t cmMainCount     = 0;
  static uint32_t cmMaxMainLoop   = 0;
  static uint32_t cmLoopStart     = 0;
  static unsigned long cmLastPrint = 0;
#endif

// Internal-heap saple at boot phase: the same pair of figures as the [Heap] line, printed during boot phases only
static void bootHeapSnapshot(const char* tag) {
  const uint32_t caps = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
  BOOTLOG("Heap@%s:\t%uKB free, %uKB largest (getFreeHeap %u)", tag,
          (unsigned)(heap_caps_get_free_size(caps) / 1024),
          (unsigned)(heap_caps_get_largest_free_block(caps) / 1024),
          (unsigned)ESP.getFreeHeap());
}

// PSRAM free and the largest contiguous block, at the points the boot order takes its allocations, boot only
static void bootPsramSnapshot(const char* tag) {
  if (!psramFound()) { BOOTLOG("PSRAM@%s:\t[none]", tag); return; }
  BOOTLOG("PSRAM@%s:\t%uKB free, %uKB largest",
          tag,
          (unsigned)(ESP.getFreePsram() / 1024),
          (unsigned)(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) / 1024));
}

void setup() {
  #if !defined(ARDUINO_USB_CDC_ON_BOOT) && ARDUINO_USB_CDC_ON_BOOT
    Serial.setTxBufferSize(4096); // activate ring-buffer
  #endif
  Serial.begin(115200);
  bootHeapSnapshot("setup-start");
  {
    const bool dumpedCrash = crashDumpAvailable();
    #if (CORE_DEBUG_LEVEL > 0) || defined(BOOTLOG_TIME) || defined(CONFIG_KEY_DEBUG)
      if (dumpedCrash || esp_reset_reason() == ESP_RST_POWERON || esp_reset_reason() == ESP_RST_EXT) {
        delay(1000);
        BOOTLOG("1 second delay after cold boot to ensure serial logs are available (CORE_DEBUG_LEVEL > 0 or BOOTLOG_TIME or CONFIG_KEY_DEBUG)...");
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
  bootHeapSnapshot("before-display");
  bootPsramSnapshot("before-display");
  display.init();
  bootPsramSnapshot("display");     // 1 + 2 + 3: the framebuffer windows and both VU canvases
  BOOTTIMELOG("display.init");
  const bool offlineBoot = (network.offlineMode || config.store.SDoffline);
  player.init();
  bootPsramSnapshot("player.init"); // 3: the VU/FFT scratch
  BOOTTIMELOG("player.init");
  display.putRequest(BOOTVERSION); // follows player.init because of "VS1053" message
  startup.checkLittleFSandVer();
  BOOTTIMELOG("checkLittleFSandVer");
  // The order finishes here: the WebUI cache first, because its size is the only unpredictable one, then the
  // de-ogger scratch and the ring.  Both calls are idempotent and degrade to a warning if memory is short.
  if (psramFound()) netserver.primeFileCache();
  bootPsramSnapshot("webui-cache");
  player.reservePsram();
  bootPsramSnapshot("audio-ring");
  BOOTTIMELOG("psram order (cache, de-ogger, ring)");
  battery.bootStatus();
  BOOTTIMELOG("battery.bootStatus");
  if (offlineBoot) {
    startup.sdOfflineMode();
    BOOTTIMELOG("sdOfflineMode");
  } else {
    startup.checkSafeMode();
    network.begin();
    bootHeapSnapshot("after-wifi");
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
  if (SD_CS!=255 && config.getMode()==PM_SDCARD) {
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
  bootHeapSnapshot("after-display");
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
  bootHeapSnapshot("after-services");
  BOOTTIMELOG("startupServices kickoff");
  netserver.setBootReady(true);
  config.saveValue(&config.store.SDoffline, false);
  BOOTTIMELOG("setBootReady");
}

void loop() {
  #ifdef CORE_MONITOR
    // Total iteration length, measured to the next start: the difference against the stage sum is this monitor's own
    // cost, which is invisible to the stage timers and to Max Main Loop Time alike.
    static uint32_t cmPrevStart = 0;
    static uint32_t cmMaxIter   = 0;
    const uint32_t cmNow = micros();
    if (cmPrevStart) {
      const uint32_t iterUs = cmNow - cmPrevStart;
      if (iterUs > cmMaxIter) cmMaxIter = iterUs;
    }
    cmPrevStart = cmNow;
    cmLoopStart = cmNow;
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

  uint32_t tStage4a = tStage3;
  if (network.status == CONNECTED || network.status == SDOFFLINE) {
    player.loop();
    tStage4a = micros();
    // A track change is the only window the corruption has ever appeared in, so on a slow player stage look at the heap
    // the moment control comes back: it is either already broken here, or the writer is in the saves below.
    if ((tStage4a - tStage3) > 500000UL && !heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true))
      ERRORLOG("heap corrupt when player.loop() returned, after %lums", (unsigned long)((tStage4a - tStage3) / 1000UL));
    config.processDeferredSaves();
  }
  const uint32_t tStage4 = micros();

  startup.loop();
  const uint32_t tStage5 = micros();

  {
    const uint32_t total = tStage5 - tStage0;
    if (total > ((uint32_t)MAIN_LOOP_STALL_MS * 1000UL)) {
      const uint32_t staged[6] = {
        tStage1 - tStage0,   // net: improv or telnet
        tStage2 - tStage1,   // rgbled + battery
        tStage3 - tStage2,   // controls
        tStage4a - tStage3,  // player
        tStage4 - tStage4a,  // deferred config saves
        tStage5 - tStage4,   // startup
      };
      static const char* const stageNames[6] = {"net/telnet", "rgbled+battery", "controls", "player", "saves", "startup"};
      uint8_t worst = 0;
      for (uint8_t i = 1; i < 6; i++) if (staged[i] > staged[worst]) worst = i;
      WARNINGLOG("Main loop stalled %lums, worst stage %s %lums - net %lu, hw %lu, ctl %lu, player %lu, saves %lu, startup %lu",
          (unsigned long)(total / 1000UL), stageNames[worst], (unsigned long)(staged[worst] / 1000UL),
          (unsigned long)(staged[0] / 1000UL), (unsigned long)(staged[1] / 1000UL),
          (unsigned long)(staged[2] / 1000UL), (unsigned long)(staged[3] / 1000UL),
          (unsigned long)(staged[4] / 1000UL), (unsigned long)(staged[5] / 1000UL));
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
      uint32_t d = cmDspLoopCount;   cmDspLoopCount = 0;
      uint32_t m = cmMainCount;      cmMainCount = 0;
      uint32_t mx = cmMaxMainLoop;   cmMaxMainLoop = 0;
      uint32_t dw = cmMaxDspWorkUs;  cmMaxDspWorkUs = 0;
      // The same maximum split by stage, so a long pass is attributed rather than guessed at.
      uint32_t dp = cmMaxDspPagerUs; cmMaxDspPagerUs = 0;
      uint32_t dr = cmMaxDspReqUs;   cmMaxDspReqUs = 0;
      uint32_t di = cmMaxDspIoUs;    cmMaxDspIoUs = 0;
      uint8_t  dt = cmWorstReqType;  cmWorstReqType = 0xFF;
      uint32_t gl = cmGlyphCount;    cmGlyphCount = 0;
      uint32_t pc = cmPreTextCalls;  cmPreTextCalls = 0;
      uint32_t ph = cmPreTextHits;   cmPreTextHits = 0;
      uint32_t fi = cmFillCount;     cmFillCount = 0;
      uint32_t pu = cmPushCount;     cmPushCount = 0;
      // Both figures are per TASK, labelled with the core each one runs on: field 1 the display task, field 2 this loop.
      // The two maxima answer different questions.  Max Main Loop Time is this loop's worst iteration.  Max Dsp Work
      // is the display's worst single display.loop() call - one queue request's handler, excluding its vTaskDelay.
      // Comparing them separates "the drawing held the core" from "this loop blocked on something of its own": if Max
      // Dsp Work is close to Max Main Loop Time, the display was the blocker; if it is small, it was not.  The
      // per-loop ms figures cannot tell you either way, because they are periods and average the peaks away.
      // Both maxima are WALL-CLOCK.  When the two tasks share a priority, each figure includes the slices the other
      // one took, so Max Dsp Work rises simply from being interleaved.  Compare the two; read neither as CPU time.
      FUNCTIONLOG("Core.monitor", "DspTask(core%u) loops/s: %u (%.2fms/loop), Main(core%u) loops/s: %u (%.2fms/loop), Max Main Loop Time: %.3fms, Max Dsp Work: %.3fms, Max Iteration: %.3fms, Free Heap: %u",
          (unsigned)cmDspCore,
          (unsigned)(d * perSec), d ? (float)elapsed / (float)d : 0.0f,
          (unsigned)xPortGetCoreID(),
          (unsigned)(m * perSec), m ? (float)elapsed / (float)m : 0.0f,
          mx / 1000.0f, dw / 1000.0f, cmMaxIter / 1000.0f, (unsigned)ESP.getFreeHeap());
      cmMaxIter = 0;
      // What the display task did with the time: work units per second, and the preText hit rate.
      FUNCTIONLOG("Core.monitor", "DspTask work/s: glyphs %u, fills %u, preText %u (hit %u%%), fb flushes %u (%.2f/loop), max stage pager %.1fms, req %.1fms (type %u), dsp %.1fms",
          (unsigned)(gl * perSec), (unsigned)(fi * perSec), (unsigned)(pc * perSec),
          pc ? (unsigned)(ph * 100UL / pc) : 0U,
          (unsigned)(pu * perSec), d ? (float)pu / (float)d : 0.0f,
          dp / 1000.0f, dr / 1000.0f, (unsigned)dt, di / 1000.0f);
      #ifndef CONFIG_FREERTOS_UNICORE
        FUNCTIONLOG("Core.monitor", "Core layout: core0 " CORE_0 ", core1 " CORE_1);
      #endif
      FUNCTIONLOG("Core.monitor", "High Water Mark (free bytes in stacks): Main: %u, Display: %u, Netserver: %u",
          (unsigned)uxTaskGetStackHighWaterMark(NULL),
          (unsigned)(dspTaskHandle ? uxTaskGetStackHighWaterMark(dspTaskHandle) : 0),
          (unsigned)(nsTaskHandle  ? uxTaskGetStackHighWaterMark(nsTaskHandle)  : 0));
      // Every heap figure costs a full pool walk: get_free_size, get_largest_free_block and check_integrity all funnel
      // into tlsf_walk_pool - ESP.getPsramSize included - and a walk of a corrupt heap *panics*.  Three builds running,
      // this monitor died in the act of reporting a corruption instead of describing it.  So every figure below is
      // refreshed once a minute and printed from cache, and the only per-print check left is heapq::verify(), which
      // walks nothing.  Losing up to a minute of detection is worth a run that survives to produce evidence.
      heapq::verify();
      const uint32_t internal8 = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
      static uint16_t heapTick = 12;     // start due; then one walk every 12 prints (about a minute)
      static uint8_t  lfsTick  = 10;     // start due, then every 10th walk: the LittleFS query is the expensive one
      static bool    heapInternalOk = true;
      static bool    heapPsramOk = true;
      static size_t  i8Free = 0, i8Largest = 0, pFree = 0, pLargest = 0;
      static size_t  prevPsramUsed = 0, prevI8Free = 0;
      bool heapWalkedNow = false;
      if (++heapTick >= 12) {
        heapTick = 0;
        heapWalkedNow = true;
        heapInternalOk = heap_caps_check_integrity(MALLOC_CAP_INTERNAL, true);
        i8Free = heap_caps_get_free_size(internal8);
        i8Largest = heap_caps_get_largest_free_block(internal8);
        pFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        pLargest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
        heapPsramOk = !psramFound() || heap_caps_check_integrity(MALLOC_CAP_SPIRAM, true);
      }
      const bool psramImpossible = heapWalkedNow && psramFound() && pFree > (1024UL * 1024UL) && pLargest == 0;
      const char* heapVerdict =
            psramImpossible ? " | heap CORRUPT (PSRAM free but no largest block)"
          : (heapInternalOk && heapPsramOk) ? (heapWalkedNow ? " | heap ok" : " | heap ok (refreshed every minute)")
          : (!heapInternalOk && !heapPsramOk) ? " | heap CORRUPT (internal+PSRAM)"
          : !heapInternalOk                   ? " | heap CORRUPT (internal)"
                                              : " | heap CORRUPT (PSRAM)";
      // internal8 is internal DRAM that can serve a byte: MALLOC_CAP_DEFAULT also matches PSRAM, plain INTERNAL also
      // matches IRAM, which is 32-bit only - so neither reports the heap wifi_malloc allocates from.
      FUNCTIONLOG("Heap", "8bit: %uKB free, %uKB largest | PSRAM: %uKB free, %uKB largest%s",
          (unsigned)(i8Free / 1024), (unsigned)(i8Largest / 1024),
          (unsigned)(pFree / 1024), (unsigned)(pLargest / 1024),
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
                 (unsigned)(i8Free / 1024),
                 (unsigned)(i8Largest / 1024),
                 heapInternalOk ? "ok" : "CORRUPT", heapPsramOk ? "ok" : "CORRUPT",
                 (unsigned long)(millis() / 1000UL));
        coredump::noteHeapCorruption(heapNote);
      }
      // The LittleFS query stays on a tenth of the heap ticks: it is what made Max Iteration spike to ~590 ms, the
      // largest recurring stall on this loop, and nothing it reports changes in under a minute.
      if (heapWalkedNow) {
        if (++lfsTick >= 10) {
          lfsTick = 0;
          const uint32_t etcStart = micros();
          FUNCTIONLOG("LittleFS", "Used: %u / %u bytes, Free: %u bytes, took %lums", LittleFS.usedBytes(), LittleFS.totalBytes(), LittleFS.totalBytes() - LittleFS.usedBytes(), (unsigned long)((micros() - etcStart) / 1000UL));
        }
        if (psramFound()) {
          // All three of these walk the PSRAM pool - ESP.getPsramSize() included, which reads like a constant and is
          // not - so they come from the cache the [Heap] line refills, rather than walking again here every print.
          static const size_t psramTotalCached = ESP.getPsramSize();
          const size_t psramTotal = psramTotalCached;
          const size_t psramUsed  = psramTotal - pFree;
          const size_t audioFill  = player.inBufferFilled();
          // Everything the line cannot name individually: the audio ring (allocated once at boot, so it is a constant
          // here it also carries) plus the de-ogger scratch and the decoder set.  A step in this figure is a step in one
          // of those, and the figures it is built from are printed beside it.
          const size_t named = psramFrameBufferBytes + vuPsramBytes + netserver.getFileCache().totalBytes();
          const size_t other = psramUsed > named ? psramUsed - named : 0;
          const long   dUsed = (long)(psramUsed / 1024) - (long)(prevPsramUsed / 1024);
          const long   dI8   = (long)(i8Free / 1024) - (long)(prevI8Free / 1024);
          // A step is the event this line exists to catch: one contiguous block leaving the pool, which a slow refresh
          // reported minutes after the fact and could not attribute at all.
          if (prevPsramUsed && (dUsed > 16 || dUsed < -16)) {
            WARNINGLOG("PSRAM step: %+ldKB used since the last refresh; framebuffer+canvas %uKB, cache %uKB, ring+other %uKB",
                       dUsed, (unsigned)(psramFrameBufferBytes / 1024),
                       (unsigned)(netserver.getFileCache().totalBytes() / 1024), (unsigned)(other / 1024));
          }
          if (prevI8Free && dI8 < -8) WARNINGLOG("Heap step: internal 8-bit free fell %ldKB since the last refresh", -dI8);
          prevPsramUsed = psramUsed; prevI8Free = i8Free;
          // Every one of these is size_t or uint32_t, which is `long unsigned` here, so each is cast rather than
          // changing the format: the reader wants KB, not the width of the type.
          FUNCTIONLOG("PSRAM", "Used: %uKB / %uKB: Framebuffer+canvas: %uKB, VU FFT: %uKB, WebUI Cache: %uKB, Audio buffered: %uKB, Ring+other: %uKB, Contiguous Free: %uKB",
              (unsigned)(psramUsed / 1024),
              (unsigned)(psramTotal / 1024),
              (unsigned)(psramFrameBufferBytes / 1024),
              (unsigned)(vuPsramBytes / 1024),
              (unsigned)(netserver.getFileCache().totalBytes() / 1024),
              (unsigned)(audioFill / 1024),
              (unsigned)(other / 1024),
              (unsigned)(pLargest / 1024));
        }
      }
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


