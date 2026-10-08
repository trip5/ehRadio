#include "options.h"
#include <time.h>
#include <ArduinoJson.h>
#include <DNSServer.h>
#include <ehDP.h>
#include <ESPFileUpdater.h>
#include <ESPmDNS.h>
#include <ImprovWiFiLibrary.h>
#include "config.h"
#include "display.h"
#include "logging.h"
#include "mqtt.h"
#include "netserver.h"
#include "network.h"
#include "player.h"
#include "rtcsupport.h"
#include "sdmanager.h"
#include "startup.h"
#include "telnet.h"
#include "utility.h"
#include "../locale/dsplocale.h"

#define NETWORK_TASK_STACK_BYTES (NETWORK_TASK_STACK_SIZE * 1024)

MyNetwork network;

TaskHandle_t syncTaskHandle;
TaskHandle_t streamRetryTaskHandle = NULL;
static uint8_t streamResetsUsed = 0;

bool getWeather(char *wstr);
void doSync(void * pvParameters);
void retryStreamConnection(void * pvParameters);
static bool onImprovCustomConnect(const char* ssid, const char* password);
static bool shouldClearWeatherCacheOnFailure();

EhDP ehdp;

void MyNetwork::cancelStreamRetry() {
  if (streamRetryTaskHandle != NULL) {
    network.lostPlaying = false;
    vTaskDelete(streamRetryTaskHandle);
    streamRetryTaskHandle = NULL;
  }
  streamResetsUsed = 0;   // the user ended the outage, so the reset budget starts over
}

// The only place the ladder task is created.  The player's arm gate, WiFiReconnected and the self-heal in
// ticks() all used to create it themselves, so two of them could win the race and leave a stray task running
// with the handle already cleared.  A create that FAILED was silent and that was the worse bug: lostPlaying
// stayed set with no task behind it, which closes the arm gate because nothing else clears the flag except a
// fresh WiFi reconnect event.  The critical section covers only the check and the claim, not the create.
bool spawnStreamRetry() {
  static portMUX_TYPE spawnMux = portMUX_INITIALIZER_UNLOCKED;
  static volatile bool spawnPending = false;
  portENTER_CRITICAL(&spawnMux);
  const bool taken = (streamRetryTaskHandle != NULL) || spawnPending;
  if (!taken) spawnPending = true;
  portEXIT_CRITICAL(&spawnMux);
  if (taken) return false;   // already running, or a create is in flight

  const BaseType_t ok = xTaskCreatePinnedToCore(retryStreamConnection, "streamRetry", NETWORK_TASK_STACK_BYTES, NULL, NET_TASK_PRIORITY, &streamRetryTaskHandle, NETWORK_CORE);

  portENTER_CRITICAL(&spawnMux);
  spawnPending = false;
  if (ok != pdPASS) streamRetryTaskHandle = NULL;   // leave it NULL so the next attempt can try again
  portEXIT_CRITICAL(&spawnMux);
  if (ok != pdPASS) ERRORLOG("Network", "streamRetry task could not be created (%u bytes, free heap %u)", (unsigned)NETWORK_TASK_STACK_BYTES, (unsigned)ESP.getFreeHeap());
  return ok == pdPASS;
}

void ticks() {
  if (!display.ready()) return; //waiting for SD is ready
  // Self-heal the ladder.  lostPlaying is the IOU ("the user is owed a resume"): it is set BEFORE the task is
  // created, and a create can fail on a fragmented heap, while a task can also end early and leave the flag
  // set (a WiFi loss mid-outage does exactly that).  Either way the arm gate in player.loop() stays closed and
  // only a reboot used to help.  If the debt is owed, nothing is servicing it, and the link is up: start one.
  if (network.lostPlaying && streamRetryTaskHandle == NULL && !network.beginReconnect && WiFi.status() == WL_CONNECTED) {
    static uint32_t lastRespawnMs = 0;
    if (lastRespawnMs == 0 || (millis() - lastRespawnMs) >= STREAM_RETRY_RESPAWN_MS) {
      lastRespawnMs = millis() ? millis() : 1;
      FUNCTIONLOG("Network", "stream retry task is gone with a resume still owed - starting a new one");
      spawnStreamRetry();
    }
  }
  static uint32_t timeSyncTicks = 0;
  static uint16_t weatherSyncTicks = 0;
  static bool divrssi;
  timeSyncTicks++;
  weatherSyncTicks++;
  divrssi = !divrssi;
  if (network.status == CONNECTED) {
    if (config.store.ehdp) ehdp.loop();
    if (network.forceTimeSync || network.forceWeather) {
      xTaskCreatePinnedToCore(doSync, "doSync", NETWORK_TASK_STACK_BYTES, NULL, LOW_TASK_PRIORITY, &syncTaskHandle, NETWORK_CORE);
    }
    // check at :01s mark (fix network clock not matching system clock after Daylight Savings Time changes)
    if (network.timeinfo.tm_sec == 1) {
      time_t now = time(NULL);
      struct tm localNow;
      localtime_r(&now, &localNow);
      if ((network.timeinfo.tm_min != localNow.tm_min) || (network.timeinfo.tm_hour != localNow.tm_hour)) {
        timeSyncTicks = 0;
        network.forceTimeSync = true;
      }
    }
    // Time sync interval: config value is in hours, convert to seconds
    uint32_t timeSyncInterval = (uint32_t)config.store.timesyncinterval * 3600;
    if (timeSyncTicks >= timeSyncInterval) {
      timeSyncTicks=0;
      network.forceTimeSync = true;
    }
    uint16_t weatherSyncInterval = (uint16_t)config.store.weathersyncinterval * 60;
    if (config.store.showweather && weatherSyncTicks >= weatherSyncInterval) {
      weatherSyncTicks=0;
      network.forceWeather = true;
    }
  }
  bool connectingStream = display.mode()==PLAYER && !player.isRunning() && strcmp_P(config.station.title, l10n(L10N_MSG_CONNECT)) == 0;
  if (connectingStream) {
    config.screensaverTicks = 0;
    config.screensaverPlayingTicks = 0;
  } else {
    if (config.store.screensaverEnabled && display.mode()==PLAYER && !player.isRunning()) {
      config.screensaverTicks++;
      if (config.screensaverTicks > config.store.screensaverTimeout+SCREENSAVERSTARTUPDELAY) {
        if (config.store.screensaverBlank) {
          display.putRequest(NEWMODE, SCREENBLANK);
        } else {
          display.putRequest(NEWMODE, SCREENSAVER);
        }
      }
    }
    if (config.store.screensaverPlayingEnabled && display.mode()==PLAYER && player.isRunning()) {
      config.screensaverPlayingTicks++;
      if (config.screensaverPlayingTicks > config.store.screensaverPlayingTimeout*60+SCREENSAVERSTARTUPDELAY) {
        if (config.store.screensaverPlayingBlank) {
          display.putRequest(NEWMODE, SCREENBLANK);
        } else {
          display.putRequest(NEWMODE, SCREENSAVER);
        }
      }
    }
  }
  #if RTCSUPPORTED
    if (config.isRTCFound()) {
      rtc.getTime(&network.timeinfo);
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #else
    if (network.timeinfo.tm_year>100 || network.status == SDOFFLINE) {
      network.timeinfo.tm_sec++;
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #endif //#if RTCSUPPORTED
  if (player.isRunning() && config.getMode()==PM_SDCARD) {
    if (network.status == SDOFFLINE) player.getAudioCurrentTime();  // bypass netserver (not running in offline mode)
    else netserver.requestOnChange(SDPOS, 0);
  }
  if (divrssi) {
    if (network.status == CONNECTED) {
      netserver.setRSSI(WiFi.RSSI());
      netserver.requestOnChange(NRSSI, 0);
      display.putRequest(DSPRSSI, netserver.getRSSI());
    } else if (network.status == SDOFFLINE) {
      display.putRequest(DSPRSSI, 0);  // no RSSI offline, but keeps the buffer bar refreshed
    }
    #ifdef USE_SD
      { static uint32_t _lastCheckSD = 0;
        if (millis() - _lastCheckSD >= 2000) {
          _lastCheckSD = millis();
          // The manager's own loop asks the same question; SDManager debounces it, and the grace window after a
          // manager session keeps the first raw read off a card that was just written to and remounted.
          if (config.getMode()==PM_SDCARD && display.mode()!=SDCHANGE && sdman.presenceProbeAllowed())
            player.sendCommand({PR_CHECKSD, 0});
        }
      }
      #if SD_AUTOPLAY && SD_CARD_DETECT_PIN!=255
        if (config.getMode()!=PM_SDCARD && digitalRead(SD_CARD_DETECT_PIN)==LOW)
          config.changeMode(PM_SDCARD);
      #endif
    #endif
    { static uint32_t _lastVUTonus = 0;
      if (millis() - _lastVUTonus >= 200) {
        _lastVUTonus = millis();
        player.sendCommand({PR_VUTONUS, 0});
      }
    }
  }
}

// Wait before each attempt, by attempt index: nearly immediate at first, then close together for a cold DNS/ARP
// then the steady cadence.  A recovery starts in seconds; a long outage is retried patiently
static uint32_t streamRetryWaitMs(uint16_t attempt) {
  if (attempt == 0) return 1000;      // essentially now: is this just a dropped connection?
  if (attempt == 1) return 3000;
  if (attempt == 2) return 6000;
  if (attempt <= 4) return 12000;
  if (attempt < 40) return 15000;     // steady cadence through the fast budget
  return (uint32_t)STREAM_RETRY_SLOW_S * 1000UL;   // then the slow, indefinite tail
}

// Probe verdicts: only WEDGED resets the link, everything else means leave it alone
enum netVerdict_e : uint8_t { NET_STACK_OK, NET_STACK_WEDGED, NET_LINK_DOWN };

static const char* netVerdictName(netVerdict_e v) {
  switch (v) {
    case NET_STACK_OK:     return "stack ok - it can connect, so the stream host refused";
    case NET_STACK_WEDGED: return "STACK WEDGED - nothing can connect while the driver says connected";
    default:               return "link down";
  }
}

// Ask the stack if it is healthy: the driver saying connected while a bare-IP
// TCP connect cannot complete IS the wedge signature (requiring DNS too left the device unrecoverable)
static netVerdict_e probeNetworkStack() {
  if (WiFi.status() != WL_CONNECTED) return NET_LINK_DOWN;

  // Gateway first, with the shorter timeout: a LAN connect is tens of ms and if it answers nothing more
  // can be learned, so the expensive internet probe never runs.
  const uint32_t gwIp = (uint32_t)WiFi.gatewayIP();
  bool gwOk = false;
  uint32_t gwMs = 0;
  if (gwIp) {
    WiFiClient lan;
    const uint32_t t0 = millis();
    gwOk = lan.connect(IPAddress(gwIp), NETHEALTH_GATEWAY_PORT, NETHEALTH_GATEWAY_TIMEOUT_MS);
    gwMs = millis() - t0;
    lan.stop();
  }
  if (gwOk) {
    FUNCTIONLOG("Network", "stack probe: gateway ok %lums - our stack can connect", (unsigned long)gwMs);
    return NET_STACK_OK;
  }

  // Gateway silent: one bare-IP TCP connect (no DNS, no TLS) exercises netif, route and socket pool at once
  WiFiClient probe;
  const uint32_t t0 = millis();
  const bool netOk = probe.connect(IPAddress(1, 1, 1, 1), 53, NETHEALTH_TIMEOUT_MS);
  const uint32_t netMs = millis() - t0;
  probe.stop();
  FUNCTIONLOG("Network", "stack probe: gateway %s %lums, internet TCP %s %lums",
      gwIp ? "FAIL" : "unknown", (unsigned long)gwMs, netOk ? "ok" : "FAIL", (unsigned long)netMs);

  return netOk ? NET_STACK_OK : NET_STACK_WEDGED;
}

void retryStreamConnection(void * pvParameters) {
  const uint16_t fastAttempts = 40;  // after this many the cadence slows down; it never gives up
  const uint8_t maxResets = 3;       // link resets are the brute-force rung, capped across the outage
  uint16_t attemptCount = 0;
  bool slowed = false;
  for (;;) {
    // attemptCount is the number of attempts DONE, so it is also this attempt's index.
    delay(streamRetryWaitMs(attemptCount));
    // Check if we should still be retrying
    if (network.lostPlaying && WiFi.status() == WL_CONNECTED && !player.isRunning()) {
      attemptCount++;
      // The budget slows the cadence, it never ends the attempt: a LOST screen with a task still trying recovers by itself
      if (!slowed && attemptCount > fastAttempts) {
        slowed = true;
        FUNCTIONLOG("Network", "Stream retry budget spent - continuing every %u s while the stream is down", (unsigned)STREAM_RETRY_SLOW_S);
      }
      // A link reset costs a scan, a reassociation and DHCP, so it is spent only on evidence (see the
      // verdict below); eraseap stays false on every path or the saved credentials are erased.
      if (attemptCount <= fastAttempts) FUNCTIONLOG("Network", "Stream reconnect fast attempt %d/%d", attemptCount, fastAttempts);
      // The connect runs on the main loop, so wait on the connect TIMESTAMP changing, not a fixed sleep:
      // a fixed wait could expire early and the verdict would use the previous attempt's duration.
      const uint32_t connectBefore = player.lastConnectMs;
      player.resumeLastWebSource();
      for (uint16_t waited = 0; waited < 2500 && !player.isRunning() && player.lastConnectMs == connectBefore; waited += 100) {
        delay(100);
      }
      // Check if it worked
      if (player.isRunning()) {
        FUNCTIONLOG("Network", "Stream reconnected successfully!");
        streamResetsUsed = 0;   // a working stream means the next outage starts with a full budget
        network.lostPlaying = false;
        streamRetryTaskHandle = NULL;
        vTaskDelete(NULL);
        return;
      }

      // The LINK never dropped, so WiFiLostConnection never ran and nothing told the user: from the
      // second failure this is a LOST screen
      if (attemptCount == 1) startup.deferBootStable("stream lost");   // no-op unless the boot is unproven
      if (attemptCount == 2) display.putRequest(NEWMODE, LOST);

      // The connect DURATION is itself a verdict: a refusal returns in tens of ms and proves the path
      // works, a timeout means it is stale.  Only ambiguity probes further - see probeNetworkStack().
      const uint32_t connectMs = player.lastConnectMs;
      const netVerdict_e verdict = (connectMs && connectMs < NET_REFUSAL_MS) ? NET_STACK_OK : probeNetworkStack();
      FUNCTIONLOG("Network", "attempt %d/%d failed (connect %lums): %s", attemptCount, fastAttempts, (unsigned long)connectMs, netVerdictName(verdict));
      if (verdict == NET_STACK_WEDGED && streamResetsUsed < maxResets &&
          network.status == CONNECTED && !network.beginReconnect) {
        streamResetsUsed++;
        FUNCTIONLOG("Network", "forcing a link reset %d/%d", streamResetsUsed, maxResets);
        streamRetryTaskHandle = NULL;   // WiFiReconnected restarts this task, so it must look stopped
        WiFi.disconnect(true, false);   // wifioff: fires STA_DISCONNECTED, which runs the recovery chain
        vTaskDelete(NULL);
        return;
      }
    } else {
      // Conditions changed (user pressed stop, or already playing, or WiFi lost again)
      if (!network.lostPlaying || player.isRunning()) {
        network.lostPlaying = false;
      }
      streamRetryTaskHandle = NULL;
      vTaskDelete(NULL);
      return;
    }
  }
}

void MyNetwork::WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  network.beginReconnect = false;
  // Runs at GOT_IP after any path found an AP, so this is where the truth about where we landed is kept
  network.captureCurrentAp();
  // A link was just rebuilt, so the boot-stable countdown restarts from here, not from before the outage
  startup.deferBootStable("wifi reconnected");
  player.lockOutput = false;
  delay(100);
  display.putRequest(NEWMODE, PLAYER);
  if (config.getMode()==PM_SDCARD) {
    network.status=CONNECTED;
    display.putRequest(NEWIP, 0);
  } else {
    display.putRequest(NEWMODE, PLAYER);
    if (network.lostPlaying) {
      if (streamRetryTaskHandle != NULL) {
        // The running task cannot see this GOT_IP, so kick it once and let its cadence continue
        player.resumeLastWebSource();
      } else {
        // The task owns the resume and only it can verify it: resuming here too queued a second play
        // command mid-connect, costing a second TLS handshake and ~2 s of silence (measured).
        spawnStreamRetry();
      }
    }
  }
  if (config.store.mqttenable) mqtt.init();
}

void MyNetwork::WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info) {
  if (!network.beginReconnect) {
   // SSID is bounds-checked because this can fire before a first connection. */
    const char* lostSsid = (config.store.lastSSID > 0 && config.store.lastSSID <= config.ssidsCount) ? config.ssids[config.store.lastSSID - 1].ssid : "?";
    FUNCTIONLOG("Network", "WiFi Lost: %lu ms, event=%d, SSID=%s, RSSI=%d", millis(), (int)event, lostSsid, WiFi.RSSI());
    if (config.getMode()==PM_SDCARD) {
      display.putRequest(NEWIP, 0);
    } else {
      // OR, not assign: a forced reset arrives here with a resume already owed, and assigning
      // player.isRunning() (false) would clear it - WiFi back, radio silent.
      network.lostPlaying = network.lostPlaying || player.isRunning();
      if (network.lostPlaying) { player.lockOutput = true; player.sendCommand({PR_STOP, 0}); }
      // when we're in the middle of an update, keep the UPDATING dialog active
      if (display.mode() != UPDATING) {
        display.putRequest(NEWMODE, LOST);
      }
    }
    network.beginReconnect = true;
    // A link teardown inside the boot window proves nothing: re-arm the countdown instead of marking it stable
    startup.deferBootStable("wifi lost");
    // Spawn background task to run the full scan-best + sequential fallback strategy
    // instead of just retrying the same AP via WiFi.reconnect()
    if (xTaskCreatePinnedToCore(wifiReconnectionTask, "wifiReconn", NETWORK_TASK_STACK_BYTES, NULL, NET_TASK_PRIORITY, NULL, NETWORK_CORE) != pdPASS) {
      // No stream-ladder style retry exists for this one: the AP is only re-found on the next disconnect event,
      // so a failed create has to be visible rather than silent.
      ERRORLOG("Network", "wifiReconn task could not be created (%u bytes, free heap %u)", (unsigned)NETWORK_TASK_STACK_BYTES, (unsigned)ESP.getFreeHeap());
    }
  }
}

// Remember the AP we are on so the next forced reset can reconnect to it directly (GOT_IP, end of begin)
void MyNetwork::captureCurrentAp() {
  const uint8_t* bssid = WiFi.BSSID();
  if (bssid) {
    memcpy(lastBssid, bssid, sizeof(lastBssid));
    lastChannel = (uint8_t)WiFi.channel();
    lastBssidValid = true;
  }
}

// Directed reconnect to the AP we were last on, by BSSID and channel - the case a forced reset creates
// A failed directed attempt falls back to the full wifiBegin(), which is what a moved AP or a real dropout needs
bool MyNetwork::wifiBeginFast(bool silent) {
  if (network.lastBssidValid && config.store.lastSSID > 0 && config.store.lastSSID <= config.ssidsCount) {
    if (WiFi.mode(WIFI_STA) != WIFI_STA) delay(WIFI_SETTLE_MS);
    const uint8_t idx = config.store.lastSSID - 1;
    // Not gated on silent: this line proves the scan-free path ran (the task always passes silent)
    FUNCTIONLOG("Network", "direct reconnect to %s | Ch: %u", config.ssids[idx].ssid, network.lastChannel);
    WiFi.begin(config.ssids[idx].ssid, config.ssids[idx].password, network.lastChannel, network.lastBssid);
    uint8_t errcnt = 0;
    while (WiFi.status() != WL_CONNECTED) {
      if (!silent) SERIALLOGDOT();
      delay(500);
      network.loopImprov();
      // Deliberately impatient: a directed association takes a second or two, so past WIFI_FAST_ATTEMPTS
      // the AP has moved or gone and every extra second delays the scan that would find it.
      if (++errcnt > WIFI_FAST_ATTEMPTS) { SERIALLOGLF(); break; }
    }
    if (WiFi.status() == WL_CONNECTED) { SERIALLOGLF(); return true; }
    FUNCTIONLOG("Network", "direct reconnect failed, falling back to a scan");
  }
  return wifiBegin(silent);
}

static void wifiRestartForRetry(const char* ssid, const char* password,
                                uint8_t channel = 0, const uint8_t* bssid = nullptr) {
  WiFi.disconnect(true, false);  // wifioff: the netif goes down, so DHCP stops and can start clean
  for (uint32_t tDisc = millis(); WiFi.RSSI() != 0 && millis() - tDisc < WIFI_SETTLE_MS; ) delay(WIFI_CONNECT_POLL_MS);
  if (channel && bssid) WiFi.begin(ssid, password, channel, bssid);
  else WiFi.begin(ssid, password);
}

bool MyNetwork::wifiBegin(bool silent) {
  uint8_t ls = (config.store.lastSSID == 0 || config.store.lastSSID > config.ssidsCount) ? 0 : config.store.lastSSID - 1;
  uint8_t startedls = ls;
  // The station needs a moment before a scan returns anything - after a reset the first scan finds nothing
  // WiFi.mode() reports the mode it replaced, so this waits only when the station starts.
  if (WiFi.mode(WIFI_STA) != WIFI_STA) delay(WIFI_SETTLE_MS);

  if (config.store.wifiscanbest) {
    struct MatchedNetwork {
      uint8_t configIndex;
      int scanIndex;
      int32_t rssi;
      uint8_t channel;
      uint8_t bssid[6];
    };
    MatchedNetwork matches[20];
    int matchCount = 0;
    // Two passes at most: the first at WIFI_SCAN_DWELL_MS, the second at the Arduino/IDF default.  The scan cost is
    //   dominated by the dwell (13 channels measured 6609-7010 ms at the default 300), and a dwell that is too short costs
    //   time rather than connectivity, because the old behaviour is always the last resort - only a failure there reaches
    //   the SoftAP.  This is one of only two things kept from the join-timing work; see the connect loops below.
    // Deferred, not immediate: the boot line still shows the firmware version, and replacing it at once is what made
    //   that flash past in ~100ms.  No pause needed here (unlike the LittleFS format message, whose flash erase halts the
    //   other core) - the display task keeps running throughout the scan.
    display.putRequestDelayed(SCANNINGWIFI, 0, 2000);
    if (!silent) BOOTLOG("Scanning for best available network...");
    int n = 0;
    for (int pass = 0; pass < 2 && n < 1; pass++) {
      if (pass > 0) {
        // An empty scan means it ran too soon or the dwell was too short, so scan once more rather than size WIFI_SETTLE_MS for the worst case.
        delay(WIFI_SETTLE_MS);
        if (!silent) BOOTLOG("Scan retry at the default dwell...");
      }
      n = WiFi.scanNetworks(false, false, false, pass == 0 ? WIFI_SCAN_DWELL_MS : 300);
      if (!silent) BOOTLOG("Scan complete: %d networks found", n);
    }
    if (n > 0) {
      // Find all matching networks and build sorted list
      for (int i = 0; i < n; i++) {
        String scannedSSID = WiFi.SSID(i);
        if (scannedSSID.length() == 0) continue;
        for (uint8_t j = 0; j < config.ssidsCount; j++) {
          if (strcmp(scannedSSID.c_str(), config.ssids[j].ssid) == 0) {
            // Found a match - add to array if there's space
            if (matchCount < 20) {
              matches[matchCount].configIndex = j;
              matches[matchCount].scanIndex = i;
              matches[matchCount].rssi = WiFi.RSSI(i);
              matches[matchCount].channel = WiFi.channel(i);
              uint8_t* bssid = WiFi.BSSID(i);
              memcpy(matches[matchCount].bssid, bssid, 6);
              matchCount++;
            }
            break;
          }
        }
      }
      // Sort matches by RSSI (strongest first) using bubble sort
      for (int i = 0; i < matchCount - 1; i++) {
        for (int j = 0; j < matchCount - i - 1; j++) {
          if (matches[j].rssi < matches[j + 1].rssi) {
            MatchedNetwork temp = matches[j];
            matches[j] = matches[j + 1];
            matches[j + 1] = temp;
          }
        }
      }
      // Log all matches
      if (!silent && matchCount > 0) {
        BOOTLOG("Available networks from your saved list (sorted by strength):");
        for (int i = 0; i < matchCount; i++) {
          BOOTLOG("  %d. %s | MAC: %02X:%02X:%02X:%02X:%02X:%02X | RSSI: %d dBm | Ch: %d", 
                  i+1, config.ssids[matches[i].configIndex].ssid,
                  matches[i].bssid[0], matches[i].bssid[1], matches[i].bssid[2],
                  matches[i].bssid[3], matches[i].bssid[4], matches[i].bssid[5],
                  matches[i].rssi, matches[i].channel);
        }
      }
    }
    // Try each matched network in RSSI order
    for (int attempt = 0; attempt < matchCount; attempt++) {
      uint8_t configIdx = matches[attempt].configIndex;
      if (!silent) {
        BOOTLOG("Attempt %d: connecting to %s | MAC: %02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d dBm)", 
                attempt + 1, config.ssids[configIdx].ssid,
                matches[attempt].bssid[0], matches[attempt].bssid[1], matches[attempt].bssid[2],
                matches[attempt].bssid[3], matches[attempt].bssid[4], matches[attempt].bssid[5],
                matches[attempt].rssi);
        BOOTLOGX("\t");
        display.putRequest(BOOTSTRING, configIdx);
      }
      WiFi.begin(config.ssids[configIdx].ssid, config.ssids[configIdx].password,
                 matches[attempt].channel, matches[attempt].bssid); // Connect to specific AP by BSSID
      // Time-based rather than attempt-based: 8 s at 100 ms.  That budget covers association AND the lease, since
      //   WL_CONNECTED is GOT_IP and that arrives 0.5 s or 4.8 s after association on this hardware
      uint32_t candidateWindowMs = (uint32_t)WIFI_ATTEMPTS * 500UL;
      uint32_t tCandidate = millis();
      bool assocLogged = false, retried = false;
      while (WiFi.status() != WL_CONNECTED) {
        if (!silent) SERIALLOGDOT();
        // Split the wait in the log: RSSI goes non-zero at association, the flag only at GOT_IP.
        if (!assocLogged && WiFi.RSSI() != 0) {
          assocLogged = true;
          if (!silent) {
            SERIALLOGLF();
            BOOTLOG("Associated after %lums (RSSI %d) - waiting for the address",
                    (unsigned long)(millis() - tCandidate), WiFi.RSSI());
            BOOTLOGX("\t");
          }
        }
        delay(WIFI_CONNECT_POLL_MS);
        network.loopImprov();
        if (LED_PIN!=255 && !silent) digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        if (millis() - tCandidate > candidateWindowMs) {
          if (!retried) {
            // Second/last attempt at this candidate: the network is restarted first (radio off, so the netif goes
            //   down and the DHCP client's DISCOVER backoff resets) and the window is then widened
            retried = true;
            if (!silent) {
              SERIALLOGLF();
              BOOTLOG("No address after %lums - restarting the network and trying %s once more",
                      (unsigned long)(millis() - tCandidate), config.ssids[configIdx].ssid);
              BOOTLOGX("\t");
            }
            wifiRestartForRetry(config.ssids[configIdx].ssid, config.ssids[configIdx].password,
                                matches[attempt].channel, matches[attempt].bssid);
            tCandidate = millis();
            candidateWindowMs = (uint32_t)WIFI_ATTEMPTS * 500UL * WIFI_RETRY_SCALE;
            assocLogged = false;
            continue;
          }
          SERIALLOGLF();
          break;  // Failed, try next match
        }
      }
      if (WiFi.status() == WL_CONNECTED) {
        SERIALLOGLF();
        WiFi.scanDelete();
        config.setLastSSID(configIdx + 1);
        return true;
      }
    }

    // All scanned matches failed
    WiFi.scanDelete();
    if (!silent) BOOTLOG("All scanned networks failed.");
    return false;
  } else {
    // Try all configured SSIDs sequentially (original behavior)
    while (true) {
      if (!silent) {
        BOOTLOG("Attempt to connect to %s", config.ssids[ls].ssid);
        BOOTLOGX("\t");
        display.putRequestDelayed(BOOTSTRING, ls, 1000);
      }
      WiFi.begin(config.ssids[ls].ssid, config.ssids[ls].password);
      // Same ceiling, same poll and the same reason as the scanned loop above: 8 s at 100 ms, and it has to cover the
      //   lease as well as the association because WL_CONNECTED is GOT_IP.  Same widened second window too.
      uint32_t candidateWindowMs = (uint32_t)WIFI_ATTEMPTS * 500UL;
      uint32_t tCandidate = millis();
      bool assocLogged = false, retried = false;
      while (WiFi.status() != WL_CONNECTED) {
        if (!silent) SERIALLOGDOT();
        // Split the wait in the log: RSSI goes non-zero at association, the flag only at GOT_IP.
        if (!assocLogged && WiFi.RSSI() != 0) {
          assocLogged = true;
          if (!silent) {
            SERIALLOGLF();
            BOOTLOG("Associated after %lums (RSSI %d) - waiting for the address",
                    (unsigned long)(millis() - tCandidate), WiFi.RSSI());
            BOOTLOGX("\t");
          }
        }
        delay(WIFI_CONNECT_POLL_MS);
        network.loopImprov();
        if (LED_PIN!=255 && !silent) digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        if (millis() - tCandidate > candidateWindowMs) {
          if (!retried) {
            // Second/last attempt at this candidate, with the same network restart and widened window
            retried = true;
            if (!silent) {
              SERIALLOGLF();
              BOOTLOG("No address after %lums - restarting the network and trying %s once more",
                      (unsigned long)(millis() - tCandidate), config.ssids[ls].ssid);
              BOOTLOGX("\t");
            }
            wifiRestartForRetry(config.ssids[ls].ssid, config.ssids[ls].password);
            tCandidate = millis();
            candidateWindowMs = (uint32_t)WIFI_ATTEMPTS * 500UL * WIFI_RETRY_SCALE;
            assocLogged = false;
            continue;
          }
          ls++;
          if (ls > config.ssidsCount - 1) ls = 0;
          break;
        }
      }
      if (WiFi.status() != WL_CONNECTED && ls == startedls) {
        SERIALLOGLF();
        return false;
      }
      if (WiFi.status() == WL_CONNECTED) {
        SERIALLOGLF();
        config.setLastSSID(ls + 1);
        return true;
      }
    }
  }
  return false;
}

void MyNetwork::ehDPinit() {
  if (strlen(config.store.ehdpname) > 0) {
    ehdp.setName(config.store.ehdpname);
    #ifdef FIRMWARE_NAME
      ehdp.setFirmware(FIRMWARE_NAME);
    #elif defined(FIRMWARE)
      String fw = FIRMWARE;
      if (fw.endsWith(".bin")) fw.remove(fw.length() - 4);
      ehdp.setFirmware(fw.c_str());
    #endif
  } else {
    #ifdef FIRMWARE_NAME
      ehdp.setName(FIRMWARE_NAME);
    #endif
    #ifdef FIRMWARE
      String fw = FIRMWARE;
      if (fw.endsWith(".bin")) fw.remove(fw.length() - 4);
      ehdp.setFirmware(fw.c_str());
    #endif
  }
  ehdp.setProject("ehRadio");
  ehdp.setVersion(RADIOVERSION);
  ehdp.setUIPort(80);
  ehdp.setMaterialSymbol("0xe03e");
  if (strlen(config.store.mdnsname) > 0) ehdp.setMdns(config.store.mdnsname);
  if (ehdp.begin()) {
    BOOTLOG("ehDP listening");
  } else {
    BOOTLOG("ehDP failed to start");
  }
}

// Rounds get further apart as an outage drags on: the first is unchanged at 5 s, so a brief drop still recovers
static uint16_t wifiReconnectWaitS(uint8_t round) {
  switch (round) {
    case 0:  return 5;
    case 1:  return 10;
    case 2:  return 30;
    default: return 60;   // the cap
  }
}

void wifiReconnectionTask(void * pvParameters) {
  FUNCTIONLOG("Network", "WiFi.reconnect: starting smart reconnection (scan + sequential fallback)");
  uint8_t round = 0;
  while (network.beginReconnect && network.status != SOFT_AP) {
    // Run the full wifiBegin strategy: scan, match against saved SSIDs, sort by RSSI,
    // connect to strongest by BSSID, fall back to sequential trial of all saved SSIDs
    if (network.wifiBeginFast(true)) {
      // Connection established. The ARDUINO_EVENT_WIFI_STA_GOT_IP event fires,
      // WiFiReconnected() handles display restore, stream resume, MQTT reconnect, etc.
      FUNCTIONLOG("Network", "WiFi.reconnect: reconnected successfully");
      break;
    }
    // Full cycle failed: wait the round's interval, polled each second so the task still aborts when the
    // link returns by another route or the user goes to AP mode
    const uint16_t waitS = wifiReconnectWaitS(round);
    if (round < 255) round++;
    FUNCTIONLOG("Network", "WiFi.reconnect: no known networks available, retrying in %u s", (unsigned)waitS);
    for (uint16_t i = 0; i < waitS; i++) {
      if (!network.beginReconnect || network.status == SOFT_AP) {
        vTaskDelete(NULL);
        return;
      }
      delay(1000);
    }
  }
  vTaskDelete(NULL);
}

#define DBGAP false

void MyNetwork::begin() {
  // Initialize Improv early if not already done, so it's always available via Serial
  if (!improv) {
    BOOTLOG("improv.begin");
    improv = new ImprovWiFi(&Serial);
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S3;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_C3;
    #else
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32;
    #endif
    char deviceUrl[64];
    strlcpy(deviceUrl, "http://{LOCAL_IPV4}/", sizeof(deviceUrl));
    improv->setDeviceInfo(chip, "ehRadio", RADIOVERSION, "ehRadio", deviceUrl);
    improv->setCustomConnectWiFi(onImprovCustomConnect);
  }
  BOOTLOG("network.begin");

  startup.initNetwork();
  ctimer.detach();
  if (config.ssidsCount == 0 || DBGAP) {
    raiseSoftAP();
    return;
  }
  if (false) {
    // unreachable — placeholder for structure
  } else {
    // Regular SD (from NVS) or Web mode — Wi-Fi as normal; if fails, go to AP
    if (!wifiBegin()) {
      raiseSoftAP();
      SERIALLOGLF();
      BOOTLOG("Raise SoftAP done");
      return;
    }
    status = CONNECTED;
    setWifiParams();
    // setWifiParams() registers the handlers after this boot connect, so capture here too or the first reset scans
    captureCurrentAp();
  }
  BOOTLOG("Wifi done");
  ehDPinit();
  if (LED_PIN!=255) digitalWrite(LED_PIN, LOW);
  
  #if RTCSUPPORTED
    if (config.isRTCFound()) {
      rtc.getTime(&network.timeinfo);
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
    }
  #endif
  ctimer.attach(1, ticks);
}

void MyNetwork::loopImprov() {
  if (!improv) return;
  improv->handleSerial();
}

static Ticker improvRebootTicker;

static void triggerImprovReboot() {
  FUNCTIONLOG("REBOOT", "Improv Reboot.");
  ESP.restart();
}

static bool onImprovCustomConnect(const char* ssid, const char* password) {
  // Try to connect briefly to verify if credentials work before saving
  WiFi.begin(ssid, password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    delay(500);
    network.loopImprov();
  }

  if (WiFi.status() != WL_CONNECTED) {
    // Revert to AP if we were in AP mode, or just return false to signal error to browser
    // This will notify the user in the browser that connection failed
    return false;
  }

  // CONNECTION SUCCESSFUL - Proceed with saving logic
  if (utility.addSsid(ssid, password)) {
    // Update the URL immediately before returning success to browser
    IPAddress ip = WiFi.localIP();
    char deviceUrl[64];
    snprintf(deviceUrl, sizeof(deviceUrl), "http://%d.%d.%d.%d/", ip[0], ip[1], ip[2], ip[3]);
    #if defined(CONFIG_IDF_TARGET_ESP32S3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_S3;
    #elif defined(CONFIG_IDF_TARGET_ESP32C3)
      ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32_C3;
    #else
    ImprovTypes::ChipFamily chip = ImprovTypes::ChipFamily::CF_ESP32;
  #endif
    if (network.improv) network.improv->setDeviceInfo(chip, "ehRadio", RADIOVERSION, "ehRadio", deviceUrl);

    improvRebootTicker.once(3, triggerImprovReboot);
    return true;
  }
  return false;
}

void MyNetwork::setWifiParams() {
  WiFi.setSleep(false);
  WiFi.onEvent(WiFiReconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
  WiFi.onEvent(WiFiLostConnection, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
  weatherBuf=NULL;
  #if (DSP_MODEL!=DSP_DUMMY)
    if (weatherBuf) { free(weatherBuf); weatherBuf = nullptr; }
    weatherBuf = (char *) malloc(sizeof(char) * WEATHER_STRING_L);
    memset(weatherBuf, 0, WEATHER_STRING_L);
  #endif
  if (strlen(config.store.sntp1)>0 && strlen(config.store.sntp2)>0) {
    configTzTime(config.store.tzposix, config.store.sntp1, config.store.sntp2);
  } else if (strlen(config.store.sntp1)>0) {
    configTzTime(config.store.tzposix, config.store.sntp1);
  }
}

void MyNetwork::requestTimeSync(bool withTelnetOutput, uint8_t clientId) {
  if (withTelnetOutput) {
    (void)clientId;
    if (strlen(config.store.sntp1) > 0 && strlen(config.store.sntp2) > 0)
      configTzTime(config.store.tzposix, config.store.sntp1, config.store.sntp2);
    else if (strlen(config.store.sntp1) > 0)
      configTzTime(config.store.tzposix, config.store.sntp1);
    char timeStringBuff[50];
    strftime(timeStringBuff, sizeof(timeStringBuff), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    FUNCTIONLOG("Time.sync", "Date Time: %s", timeStringBuff);
    FUNCTIONLOG("Time.sync", "Time Zone Name & POSIX: %s, %s", config.store.tz_name, config.store.tzposix);
  }
}

void rebootTime() {
  FUNCTIONLOG("REBOOT", "Reboot time!");
  ESP.restart();
}

void MyNetwork::raiseSoftAP() {
  WiFi.mode(WIFI_AP);
  #ifdef AP_PASSWORD
    WiFi.softAP(AP_SSID, AP_PASSWORD);
  #else
    WiFi.softAP(AP_SSID);
  #endif
  dnsServer = new DNSServer();
  dnsServer->start(53, "*", WiFi.softAPIP());
  SERIALLOGLF();
  BOOTLOG("************************************************");
  BOOTLOG("Running in AP/Improv mode");
  #ifdef AP_PASSWORD
    BOOTLOG("Connect to AP %s with password %s", AP_SSID, AP_PASSWORD);
  #else
    BOOTLOG("Connect to AP %s with no password", AP_SSID);
  #endif
  BOOTLOG("and go to http://192.168.4.1/ to configure");
  BOOTLOG("Improv WiFi provisioning available via serial");
  BOOTLOG("************************************************");
  
  status = SOFT_AP;
  // Disabled by value: SOFTAP_REBOOT_DELAY is 0 in options.h, so this never runs.  Set it in myoptions.h (1-20 minutes)
  //   to bring the AP-mode reboot back - the WebUI field, the stored key and the softap command are gone.
  if (SOFTAP_REBOOT_DELAY > 0)
    rtimer.once((uint32_t)SOFTAP_REBOOT_DELAY * 60, rebootTime);
}

void MyNetwork::requestWeatherSync() {
  display.putRequest(NEWWEATHER);
}


void doSync(void * pvParameters) {
  static uint8_t tsFailCnt = 0;
  //static uint8_t wsFailCnt = 0;
  if (network.forceTimeSync) {
    network.forceTimeSync = false;
    if (getLocalTime(&network.timeinfo)) {
      tsFailCnt = 0;
      network.forceTimeSync = false;
      mktime(&network.timeinfo);
      display.putRequest(CLOCK);
      network.requestTimeSync(true);
      #if RTCSUPPORTED
        if (config.isRTCFound()) rtc.setTime(&network.timeinfo);
      #endif
    } else {
      if (tsFailCnt<4) {
        network.forceTimeSync = true;
        tsFailCnt++;
      } else {
        network.forceTimeSync = false;
        tsFailCnt=0;
      }
    }
  }
  if (network.weatherBuf && config.store.showweather && network.forceWeather) {
    // Fetch weather without interrupting display (keep showing cached data)
    network.forceWeather = false;
    if (!getWeather(network.weatherBuf) && shouldClearWeatherCacheOnFailure()) {
      network.buildWeatherString();
    }
  }
  vTaskDelete(NULL);
}

// Helper: Download URL to temporary file using EspFileUpdater (handles chunked encoding)
bool downloadToTempFile(const char* url) {
  // Delete old temp file if exists
  if (LittleFS.exists(TMP_PATH)) {
    LittleFS.remove(TMP_PATH);
  }
  
  ESPFileUpdater* downloader = new ESPFileUpdater(LittleFS);
  downloader->setUserAgent(ESPFILEUPDATER_USERAGENT);
  downloader->setMaxSize(2048);  // Weather JSON responses are small
  
  ESPFileUpdater::UpdateStatus result = downloader->checkAndUpdate(
    TMP_PATH,
    url,
    "",
    ESPFILEUPDATER_VERBOSE
  );
  
  delete downloader;
  return (result == ESPFileUpdater::UPDATED);
}

// WMO Weather Code to Description (for Open-Meteo)
const char* getWMODescription(int code) {
  switch(code) {
    case 0:  return l10n(L10N_MSG_W_CLEAR_SKY);
    case 1: case 2: case 3: return l10n(L10N_MSG_W_OVERCAST);
    case 45: case 48: return l10n(L10N_MSG_W_FOGGY);
    case 51: case 53: case 55: return l10n(L10N_MSG_W_DRIZZLE);
    case 56: case 57: return l10n(L10N_MSG_W_FREEZING_DRIZZLE);
    case 61: case 63: case 65: return l10n(L10N_MSG_W_RAIN);
    case 66: case 67: return l10n(L10N_MSG_W_FREEZING_RAIN);
    case 71: case 73: case 75: return l10n(L10N_MSG_W_SNOW);
    case 77: return l10n(L10N_MSG_W_SNOW_GRAINS);
    case 80: case 81: case 82: return l10n(L10N_MSG_W_RAIN_SHOWERS);
    case 85: case 86: return l10n(L10N_MSG_W_SNOW_SHOWERS);
    case 95: return l10n(L10N_MSG_W_THUNDERSTORM);
    case 96: case 99: return l10n(L10N_MSG_W_THUNDERSTORM_HAIL);
    default: return "Unknown";
  }
}

// Weather data cache (stores raw metric data from last API fetch)
namespace WeatherCache {
  bool valid = false;
  bool failedLastRefresh = false;
  float temp_c = 0;
  float feels_like_c = 0;
  int humidity = 0;
  float pressure_hpa = 0;
  float wind_speed_ms = 0;  // Always stored in m/s (meters per second) for both APIs
  int wind_deg = 0;
  char description[64] = "";
  int wmo_code = 0;    // For OpenMeteo
  bool has_wmo = false;
}

static void markWeatherFetchSuccess() {
  WeatherCache::valid = true;
  WeatherCache::failedLastRefresh = false;
}

static bool shouldClearWeatherCacheOnFailure() {
  if (!WeatherCache::valid) {
    FUNCTIONLOG("Weather", "Refresh failed with no cached weather available");
    return true;
  }

  if (!WeatherCache::failedLastRefresh) {
    WeatherCache::failedLastRefresh = true;
    FUNCTIONLOG("Weather", "Refresh failed, keeping cached weather until the next interval");
    return false;
  }

  WeatherCache::valid = false;
  WeatherCache::failedLastRefresh = false;
  FUNCTIONLOG("Weather", "Refresh failed twice, clearing cached weather");
  return true;
}

// Build weather display string from cached data (no API refetch)
bool MyNetwork::buildWeatherString() {
  #if (DSP_MODEL!=DSP_DUMMY)
    if (!weatherBuf) return false;

    // If no cached data or cache expired, show loading message
    if (!WeatherCache::valid) {
      snprintf(weatherBuf, WEATHER_STRING_L, "%s", l10n(L10N_LBL_W_LOADING));
      display.putRequest(NEWWEATHER);
      return false;
    }
    
    FUNCTIONLOG("Weather", "Rebuilding display string from cached data");
    
    // Convert temperature based on user preference
    float temp_display = config.store.weathertempimp ? (WeatherCache::temp_c * 9.0 / 5.0 + 32.0) : WeatherCache::temp_c;
    float feels_display = config.store.weathertempimp ? (WeatherCache::feels_like_c * 9.0 / 5.0 + 32.0) : WeatherCache::feels_like_c;
    const char *tempUnit = config.store.weathertempimp ? "°F" : "°C";
    
    // Convert pressure based on user preference
    float press_display = config.store.weatherpressimp ? (WeatherCache::pressure_hpa * 0.750062) : WeatherCache::pressure_hpa;
    const char *pressUnit = config.store.weatherpressimp ? "mmHg" : "hPa";
    
    // Convert wind speed from cached m/s to user's preferred display unit
    float wind_display;
    const char *windUnit;
    
    if (strcmp(config.store.weatherwindspeed, "kmh") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 3.6;
      windUnit = "km/h";
    } else if (strcmp(config.store.weatherwindspeed, "mph") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 2.23694;
      windUnit = "mph";
    } else if (strcmp(config.store.weatherwindspeed, "kn") == 0) {
      wind_display = WeatherCache::wind_speed_ms * 1.94384;
      windUnit = "kn";
    } else {  // ms
      wind_display = WeatherCache::wind_speed_ms;
      windUnit = "m/s";
    }
    
    int wind_dir_idx = (int)(WeatherCache::wind_deg / 22.5) % 16;
    
    // Build weather string dynamically based on enabled fields
    char *p = weatherBuf;
    size_t remaining = WEATHER_STRING_L;
    int written;
    const char* desc = WeatherCache::has_wmo ? getWMODescription(WeatherCache::wmo_code) : WeatherCache::description;
    written = snprintf(p, remaining, "%s, %.1f%s", desc, temp_display, tempUnit);
    if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    
    if (config.store.weatherfeels && remaining > 1) {
      written = snprintf(p, remaining, " ~ %s %.1f%s", l10n(L10N_LBL_W_FEELSLIKE), feels_display, tempUnit);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherpressure && remaining > 1) {
      written = snprintf(p, remaining, " ~ %s %.0f %s", l10n(L10N_LBL_W_PRESSURE), press_display, pressUnit);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherhumidity && remaining > 1) {
      written = snprintf(p, remaining, " ~ %s %d%%", l10n(L10N_LBL_W_HUMIDITY), WeatherCache::humidity);
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= written; }
    }
    if (config.store.weatherwind && remaining > 1) {
      written = snprintf(p, remaining, " ~ %s %.1f %s [%s]", l10n(L10N_LBL_W_WIND), wind_display, windUnit, l10n_wind(wind_dir_idx));
      if (written > 0 && (size_t)written < remaining) { p += written; remaining -= (size_t)written; }
    }
    
    FUNCTIONLOG("Weather", "%s", weatherBuf);
    display.putRequest(NEWWEATHER);
    return true;
  #endif
  return false;
}

// Get weather from Open-Meteo API (free, no API key)
bool getWeather_OpenMeteo(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY)
    FUNCTIONLOG("Weather", "Calling Open-Meteo v1 API for current weather...");
    
    // Build URL - always request metric (Celsius, m/s, hPa) for consistent processing
    // Wind speed: always request in m/s so we can cache and convert to any display unit
    char url[512];
    sprintf(url, "http://api.open-meteo.com/v1/forecast?latitude=%s&longitude=%s&models=best_match&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,surface_pressure,wind_direction_10m,wind_speed_10m&forecast_days=1&wind_speed_unit=ms",
            config.store.weatherlat, config.store.weatherlon);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      FUNCTIONLOG("Weather", "Failed to download Open-Meteo data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = LittleFS.open(TMP_PATH, "r");
    if (!file) {
      FUNCTIONLOG("Weather", "Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    LittleFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      FUNCTIONLOG("Weather", "Open-Meteo JSON parse error: %s", error.c_str());
      return false;
    }
    
    // Cache elevation if available and not already cached
    if (doc["elevation"].is<float>() && config.store.weatherelevation == 0) {
      float elevation = doc["elevation"];
      config.store.weatherelevation = (int16_t)elevation;
      config.saveValue(&config.store.weatherelevation, config.store.weatherelevation);
      FUNCTIONLOG("Weather", "Elevation retrieved from Open-Meteo: %d meters", config.store.weatherelevation);
    }
    
    JsonObject current = doc["current"];
    if (current.isNull()) {
      FUNCTIONLOG("Weather", "No current data in Open-Meteo response");
      return false;
    }
    
    // Get raw data from API (always in Celsius from Open-Meteo)
    float temp_c = current["temperature_2m"];
    float feels_like_c = current["apparent_temperature"];
    int humidity = current["relative_humidity_2m"];
    int wmo_code = current["weather_code"];
    float pressure_hpa = current["surface_pressure"];  // hPa
    float wind_speed_ms = current["wind_speed_10m"];  // Now always in m/s
    int wind_deg = current["wind_direction_10m"];
    
    const char* description = getWMODescription(wmo_code);
    
    // Cache raw weather data for later string rebuilding
    markWeatherFetchSuccess();
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in consistent m/s
    WeatherCache::wind_deg = wind_deg;
    WeatherCache::wmo_code = wmo_code;
    WeatherCache::has_wmo = true;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

// Get weather from OpenWeather API 2.5 (legacy)
bool getWeather_OpenWeather25(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY)
    FUNCTIONLOG("Weather", "Calling OpenWeather API 2.5 for current weather...");
    
    // Check for API key
    if (strlen(config.store.weatherkey) == 0) {
      FUNCTIONLOG("Weather", "OpenWeather requires API key");
      return false;
    }
    
    // Build URL - always request metric for consistent processing
    char url[512];
    sprintf(url, "http://api.openweathermap.org/data/2.5/weather?lat=%s&lon=%s&units=metric&lang=%s&appid=%s",
            config.store.weatherlat, config.store.weatherlon,
            config.store.weatherlang, config.store.weatherkey);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      FUNCTIONLOG("Weather", "Failed to download OpenWeather 2.5 data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = LittleFS.open(TMP_PATH, "r");
    if (!file) {
      FUNCTIONLOG("Weather", "Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    LittleFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      FUNCTIONLOG("Weather", "OpenWeather 2.5 JSON parse error: %s", error.c_str());
      return false;
    }
    
    // Extract data (metric: Celsius, m/s, hPa)
    const char* description = doc["weather"][0]["description"];
    float temp_c = doc["main"]["temp"];
    float feels_like_c = doc["main"]["feels_like"];
    
    // Use grnd_level if available, otherwise sea_level pressure
    float pressure_hpa;
    if (doc["main"]["grnd_level"].is<float>()) {
      pressure_hpa = doc["main"]["grnd_level"];
    } else if (doc["main"]["pressure"].is<float>()) {
      pressure_hpa = doc["main"]["pressure"];
    } else {
      FUNCTIONLOG("Weather", "No pressure data in OpenWeather 2.5 response");
      return false;
    }
    
    int humidity = doc["main"]["humidity"];
    float wind_speed_ms = doc["wind"]["speed"];  // m/s from metric API
    int wind_deg = doc["wind"]["deg"];
    
    // Cache raw weather data for later string rebuilding
    markWeatherFetchSuccess();
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in m/s for OpenWeather
    WeatherCache::wind_deg = wind_deg;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

// Helper: Fetch elevation from open-elevation.com API (fallback for OW 3.0)
// Helper: Fetch and cache elevation from APIs (Open-Elevation with Open-Meteo fallback)
void fetchAndCacheElevation() {
  float lat = atof(config.store.weatherlat);
  float lon = atof(config.store.weatherlon);
  float elevation = 0.0;
  bool success = false;
  
  // Try Open-Elevation API first
  FUNCTIONLOG("Weather", "Getting elevation from Open-Elevation...");
  char url[256];
  sprintf(url, "http://api.open-elevation.com/api/v1/lookup?locations=%.4f,%.4f", lat, lon);
  
  if (downloadToTempFile(url)) {
    File file = LittleFS.open(TMP_PATH, "r");
    if (file) {
      String response = file.readString();
      file.close();
      
      JsonDocument doc;
      if (deserializeJson(doc, response) == DeserializationError::Ok) {
        if (doc["results"][0]["elevation"].is<float>()) {
          elevation = doc["results"][0]["elevation"];
          success = true;
        }
      }
    }
  }
  
  // Fall back to Open-Meteo if Open-Elevation failed
  if (!success) {
    FUNCTIONLOG("Weather", "Getting elevation from Open-Meteo...");
    sprintf(url, "https://api.open-meteo.com/v1/elevation?latitude=%.4f&longitude=%.4f", lat, lon);
    
    if (downloadToTempFile(url)) {
      File file = LittleFS.open(TMP_PATH, "r");
      if (file) {
        String response = file.readString();
        file.close();
        
        JsonDocument doc;
        if (deserializeJson(doc, response) == DeserializationError::Ok) {
          if (doc["elevation"].is<float>()) {
            elevation = doc["elevation"];
            success = true;
          }
        }
      }
    }
  }
  
  // Clean up temp file
  LittleFS.remove(TMP_PATH);
  
  // Cache elevation if successfully retrieved
  if (success && elevation > 0.0) {
    config.store.weatherelevation = (int16_t)elevation;
    config.saveValue(&config.store.weatherelevation, config.store.weatherelevation);
    FUNCTIONLOG("Weather", "Caching elevation: %d meters", config.store.weatherelevation);
  } else {
    FUNCTIONLOG("Weather", "Failed to retrieve elevation from all sources");
  }
}

// Helper: Calculate ground-level pressure from sea-level pressure using elevation
float calculateGroundPressure(float seaLevelPressure, float elevationMeters) {
  // Barometric formula: P_ground = P_sea * (1 - elevation / 44330)^5.255
  if (elevationMeters == 0.0) return seaLevelPressure;
  return seaLevelPressure * pow((1.0 - elevationMeters / 44330.0), 5.255);
}

// Get weather from OpenWeather API 3.0 (current)
bool getWeather_OpenWeather30(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY)
    FUNCTIONLOG("Weather", "Calling OpenWeather API 3.0 for current weather...");
    
    // Check for API key
    if (strlen(config.store.weatherkey) == 0) {
      FUNCTIONLOG("Weather", "OpenWeather requires API key");
      return false;
    }
    
    // Build URL - always request metric for consistent processing
    char url[512];
    sprintf(url, "http://api.openweathermap.org/data/3.0/onecall?exclude=minutely,hourly,daily&lat=%s&lon=%s&units=metric&lang=%s&appid=%s",
            config.store.weatherlat, config.store.weatherlon,
            config.store.weatherlang, config.store.weatherkey);
    
    // Download JSON response to temp file (EspFileUpdater handles chunked encoding)
    if (!downloadToTempFile(url)) {
      FUNCTIONLOG("Weather", "Failed to download OpenWeather 3.0 data");
      return false;
    }
    
    // Read the downloaded JSON file
    File file = LittleFS.open(TMP_PATH, "r");
    if (!file) {
      FUNCTIONLOG("Weather", "Failed to open temp file");
      return false;
    }
    
    String response = file.readString();
    file.close();
    LittleFS.remove(TMP_PATH);
    
    // Parse JSON with ArduinoJson
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    if (error) {
      FUNCTIONLOG("Weather", "OpenWeather 3.0 JSON parse error: %s", error.c_str());
      return false;
    }
    
    JsonObject current = doc["current"];
    if (current.isNull()) {
      FUNCTIONLOG("Weather", "No current data in OpenWeather 3.0 response");
      return false;
    }
    
    // Extract data (metric: Celsius, m/s, hPa)
    const char* description = current["weather"][0]["description"];
    float temp_c = current["temp"];
    float feels_like_c = current["feels_like"];
    float pressure_sea_hpa = current["pressure"];  // Sea-level pressure
    int humidity = current["humidity"];
    float wind_speed_ms = current["wind_speed"];  // m/s from metric API
    int wind_deg = current["wind_deg"];
    int wind_dir_idx = (int)(wind_deg / 22.5) % 16;
    
    // Get or fetch elevation for barometric adjustment
    float elevation = 0.0;
    if (config.store.weatherelevation != 0) {
      elevation = (float)config.store.weatherelevation;
      FUNCTIONLOG("Weather", "Using cached elevation: %d meters", config.store.weatherelevation);
    } else {
      // Fetch and cache elevation
      fetchAndCacheElevation();
      elevation = (float)config.store.weatherelevation;
    }
    
    // Calculate ground-level pressure from sea-level pressure
    float pressure_hpa = calculateGroundPressure(pressure_sea_hpa, elevation);
    FUNCTIONLOG("Weather", "Adjusted pressure from %.0f hPa (sea) to %.0f hPa (ground) using %.0f m elevation",
                  pressure_sea_hpa, pressure_hpa, elevation);
    
    // Cache raw weather data for later string rebuilding
    markWeatherFetchSuccess();
    WeatherCache::temp_c = temp_c;
    WeatherCache::feels_like_c = feels_like_c;
    WeatherCache::humidity = humidity;
    WeatherCache::pressure_hpa = pressure_hpa;  // Ground-level adjusted
    WeatherCache::wind_speed_ms = wind_speed_ms;  // Stored in m/s for OpenWeather
    WeatherCache::wind_deg = wind_deg;
    strncpy(WeatherCache::description, description, sizeof(WeatherCache::description) - 1);
    WeatherCache::description[sizeof(WeatherCache::description) - 1] = '\0';
    
    // Build display string from cached data
    network.requestWeatherSync();
    return network.buildWeatherString();
  #endif
  return false;
}

bool getWeather(char *wstr) {
  #if (DSP_MODEL!=DSP_DUMMY)
    // Provider dispatcher - route to appropriate weather API
    if (strcmp(config.store.weatherapi, "OW30") == 0) {
      return getWeather_OpenWeather30(wstr);
    } else if (strcmp(config.store.weatherapi, "OW25") == 0) {
      return getWeather_OpenWeather25(wstr);
    } else {  // Default: "OM1" or any other value
      return getWeather_OpenMeteo(wstr);
    }
  #endif
  return false;
}
