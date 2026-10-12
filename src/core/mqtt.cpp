#include "options.h"

#include <esp_log.h>
#include <WiFi.h>
#include "commandhandler.h"
#include "audiohandlers.h"
#include "config.h"
#include "logging.h"
#include "mqtt.h"
#include "player.h"
#include "startup.h"
#include "utility.h"

// Static storage because the will payload is kept by pointer and an "offline" publish is only acked later.
static const char MQTT_OFFLINE[] = "offline";
static const char MQTT_ONLINE[] = "online";
static const char* const MQTT_STATE_TOKENS[] = { "off", "playing", "idle", "buffering" };
enum mqttState_e : uint8_t { MQTT_ST_OFF = 0, MQTT_ST_PLAYING, MQTT_ST_IDLE, MQTT_ST_BUFFERING };

static constexpr uint32_t MQTT_STATUS_SETTLE_MS = 1500; // ms a status change must hold before it is published
static constexpr uint32_t MQTT_KEEPALIVE         = 60;   // s; keeps NAT entries alive (broker will fires at 1.5x)

void Mqtt::zeroBuf() { memset(topic, 0, sizeof(topic)); memset(status, 0, sizeof(status)); }

// ---- Requests: called from the AsyncTCP/WiFi-event tasks, so they must not block ----

void Mqtt::init() {
  applyRequestedMs = millis();
  applyRequested = true;
}

void Mqtt::publishStatus()   { statusRequested = true; }
void Mqtt::publishVolume()   { volumeRequested = true; }
void Mqtt::publishPlaylist() { playlistRequested = true; }

// ---- loop(): the only place that talks to the client library ----
void Mqtt::loop() {
  // Debounced from the last request, and held while the startup downloads run: applying the first command
  // of an "Apply Changes" burst would connect with stale values, and that is the boot path's TLS window.
  const bool quietLongEnough = (millis() - applyRequestedMs) >= applyDebounceMs;
  const bool servicesHolding = startup.servicesBusy() && (millis() - applyRequestedMs) < servicesWaitLimitMs;
  if (applyRequested && quietLongEnough && !servicesHolding) {
    applyRequested = false;
    applySettings();
  }

  if (rearmRequested) {
    // Raised by _onConnect: a fresh session may have lost every retained value, so republish all of them.
    rearmRequested = false;
    resetReported();
  }

  if (!config.store.mqttenable) {
    // nothing may be published once the session is down
    statusRequested = false;
    volumeRequested = false;
    playlistRequested = false;
    availabilityRequested = false;
    messagePending = false;
    return;
  }

  if (messagePending) {
    // Copy while the flag is still set: the MQTT task only writes incomingPayload with the flag clear,
    // so this cannot be scribbled on mid-copy; a payload arriving meanwhile is dropped.
    strlcpy(workPayload, incomingPayload, sizeof(workPayload));
    messagePending = false;
    processPendingMessage();
  }

  if (!mqttClient.connected()) return;

  // Publishing is state-driven: the request flags force a refresh (they matter after a (re)connect), and
  // otherwise a status waits for the values to settle, because a station switch walks through mismatches.
  bool settled = false;
  if (statusRequested) {
    statusRequested = false;
    storeCandidate();  // a forced refresh publishes the live values, with no settle wait
    settled = true;
  } else if (!reportedValid || differsFromReported()) {
    if (!candidateActive || !matchesCandidate()) {
      storeCandidate(); // a new combination: (re)start its settle window
      candidateSince = millis();
    } else if (millis() - candidateSince >= MQTT_STATUS_SETTLE_MS) {
      settled = true;   // unchanged for the whole window
    }
  } else {
    candidateActive = false; // nothing pending
  }

  if (settled) {
    // The attributes carry the display text; the state token is the machine half and never localized.
    if (wouldEraseTitle()) holdStatus(); else publishStatusNow();
    if (candidate.stateToken != lastReportedState) publishStateNow(candidate.stateToken);
  }

  // Connected but not known to be online means a session that has not announced itself yet - the sentinel
  // from resetReported() - so this publishes the announcement even if the flag was somehow missed.
  if (availabilityRequested || lastReportedAvailability != 1) {
    availabilityRequested = false;
    publishAvailabilityNow(true);
  }
  if (volumeRequested || lastReportedVolume != (int16_t)config.store.volume) {
    volumeRequested = false;
    publishVolumeNow();
  }
  if (lastReportedMode != (uint8_t)config.getMode()) publishModeNow();
  if (!(WiFi.localIP() == lastReportedIp)) publishIpNow();
  if (playlistRequested) {
    playlistRequested = false;
    publishPlaylistNow(); // the only publisher that reads a file, so it waits for its request
  }
}

// The blocking half of init(): (re)configuring, disconnecting and connecting the ESP-IDF client.
void Mqtt::applySettings() {
#ifndef MQTT_DEBUG
  // The IDF transport logs an error burst on every retry attempt; the [MQTT] lines already say what the
  // connection is doing. Define MQTT_DEBUG in myoptions.h (see options.h) to keep the transport detail.
  if (!idfLogsConfigured) {
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_NONE);
    esp_log_level_set("TRANSPORT_BASE", ESP_LOG_NONE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_NONE);
    esp_log_level_set("esp-tls", ESP_LOG_NONE);
    idfLogsConfigured = true;
    FUNCTIONLOG("MQTT", "IDF transport logs silenced (define MQTT_DEBUG to keep them)");
  }
#endif

  // Built before the enable check: the disable path needs it to publish "offline" before it disconnects.
  snprintf(availabilityTopic, sizeof(availabilityTopic), "%s%s", config.store.mqtttopic, "availability");

  if (!config.store.mqttenable || strlen(config.store.mqtthost) == 0) {
    if (clientStarted) {
      // The library's disconnect() waits for its own client task, and stops a client that is still
      // starting, so it can only run from here.
      publishOfflineBeforeDisconnect();
      FUNCTIONLOG("MQTT", "Disconnecting (%s)", config.store.mqttenable ? "no host" : "disabled");
      mqttClient.disconnect();
      clientStarted = false;
    }
    statusRequested = false;
    volumeRequested = false;
    playlistRequested = false;
    availabilityRequested = false;
    messagePending = false;
    resetReported(); // a later enable has to publish everything again
    return;
  }

  char uri[serverUriSize];
  if (strstr(config.store.mqtthost, "://") != nullptr) {
    strlcpy(uri, config.store.mqtthost, sizeof(uri)); // full URI given (mqtts://, ws://, ...)
  } else {
    snprintf(uri, sizeof(uri), "mqtt://%s:%u", config.store.mqtthost, config.store.mqttport);
  }

  // Re-applying an identical configuration to a live session would only tear it down and rebuild it,
  // which is what "Apply Changes" does when nothing changed; while disconnected an apply still runs.
  if (clientStarted && mqttClient.connected()
      && strcmp(uri, serverUri) == 0
      && strcmp(config.store.mqttuser, appliedUser) == 0
      && strcmp(config.store.mqttpass, appliedPass) == 0) {
    return;
  }

  if (!callbacksBound) {
    // The library keeps its callbacks in std::vector, so binding them on every apply (this runs again
    // on each mqttenable change) would stack duplicates and repeat subscribe/publish.
    mqttClient.onConnect(_onConnect);
    mqttClient.onDisconnect(_onDisconnect);
    mqttClient.onMessage(_onMessage); // the library reassembles multipart and NUL-terminates the payload
    callbacksBound = true;
  }

  if (clientStarted) {
    // Stop whatever the previous configuration left running: the library's start is not idempotent,
    // and connected() is still false while a client is coming up.
    publishOfflineBeforeDisconnect();
    mqttClient.disconnect();
    clientStarted = false;
  }

  strlcpy(serverUri, uri, sizeof(serverUri)); // setServer() keeps this pointer, so it has to outlive it
  strlcpy(appliedUser, config.store.mqttuser, sizeof(appliedUser));
  strlcpy(appliedPass, config.store.mqttpass, sizeof(appliedPass));

  mqttClient.setServer(serverUri);
  // The keepalive doubles as the availability latency: the broker publishes the will after 1.5x this.
  mqttClient.setKeepAlive(MQTT_KEEPALIVE);
  mqttClient.setCleanSession(true);
  mqttClient.setAutoReconnect(true); // the ESP-IDF client reconnects on its own; no timer needed
  mqttClient.setBufferSize(sizeof(status) + sizeof(topic) + 128);
  // Library default stack (6144) at a network-tier priority: commands run on the netserver loop, not
  // here, and a higher priority would preempt the stream and display tasks during their TLS work.
  mqttClient.setTaskStackAndPriority(6144, NET_TASK_PRIORITY);
  if (strlen(config.store.mqttuser) > 0) mqttClient.setCredentials(config.store.mqttuser, config.store.mqttpass);
  // The will is the whole point of the availability topic: it fires only when this session dies without a
  // DISCONNECT, which is what a power cut, a crash or deep sleep look like from the broker's side.
  mqttClient.setWill(availabilityTopic, 1, true, MQTT_OFFLINE, strlen(MQTT_OFFLINE));

  FUNCTIONLOG("MQTT", "Connecting to %s", serverUri);
  mqttClient.connect();
  clientStarted = true;
}

// ---- State tracking: report what settled, not what happened to be true for a moment ----

// The state token is fixed English, whatever the display locale shows: it is what automations bind to.
uint8_t Mqtt::stateToken() {
  if (!config.store.dspon) return MQTT_ST_OFF;
  if (player.status() == PLAYING) return MQTT_ST_PLAYING;
  if (player.isConnecting()) return MQTT_ST_BUFFERING; // the player owns the localized placeholder check
  return MQTT_ST_IDLE;
}

void Mqtt::storeCandidate() {
  strlcpy(candidate.name, config.station.name, sizeof(candidate.name));
  strlcpy(candidate.title, config.station.title, sizeof(candidate.title));
  strlcpy(candidate.imageUrl, audioHandlers.getArtworkImageUrl(), sizeof(candidate.imageUrl));
  candidate.station = config.lastStation();
  candidate.stateToken = stateToken();
  candidateActive = true;
}

// Field by field rather than building a snapshot to compare with: that keeps ~850 bytes off the
// netserver loop task's stack on every pass.
bool Mqtt::matchesCandidate() {
  return candidateActive
      && candidate.station == config.lastStation()
      && candidate.stateToken == stateToken()
      && strcmp(candidate.name, config.station.name) == 0
      && strcmp(candidate.title, config.station.title) == 0
      && strcmp(candidate.imageUrl, audioHandlers.getArtworkImageUrl()) == 0;
}

bool Mqtt::differsFromReported() {
  if (!reportedValid) return true;
  return !(reported.station == config.lastStation()
        && reported.stateToken == stateToken()
        && strcmp(reported.name, config.station.name) == 0
        && strcmp(reported.title, config.station.title) == 0
        && strcmp(reported.imageUrl, audioHandlers.getArtworkImageUrl()) == 0);
}

// True when publishing would replace a title already reported for this station with an empty one: the
// device blanks its title for an instant, and a *retained* message would keep that blank until the song.
bool Mqtt::wouldEraseTitle() {
  return config.station.title[0] == '\0'
      && reportedValid                       // something was reported already ...
      && reported.title[0] != '\0'           // ... and it had a real title
      && reported.station == config.lastStation();
}

// Consume a status change without publishing it, so it is not re-evaluated on every pass.
void Mqtt::holdStatus() {
  FUNCTIONLOG("MQTT", "Holding status: publishing it would erase the reported track");
  markReported();
}

// What the broker holds is exactly the settled candidate.
void Mqtt::markReported() {
  reported = candidate;
  reportedValid = true;
  candidateActive = false;
}

void Mqtt::resetReported() {
  reportedValid = false;
  candidateActive = false;
  lastReportedRevision[0] = '\0';
  lastReportedVolume = -1;
  lastReportedIp = IPAddress(0, 0, 0, 0);
  lastReportedState = stateUnknown;
  lastReportedMode = stateUnknown;
  lastReportedAvailability = -1;
}

// Runs in loop(): parse the payload copied out of the MQTT task and hand it to the command router.
void Mqtt::processPendingMessage() {
  utility.stripWhitespace(workPayload);
  if (workPayload[0] == '\0') return;

  command[0] = '\0';
  value[0] = '\0';
  if (!utility.parseCommandLine(workPayload, command, sizeof(command), value, sizeof(value))) {
    FUNCTIONLOG("MQTT", "Ignored unparsed payload: %s", workPayload);
    return;
  }

  if (cmd.isBlockedForSource(command, CommandSource::Mqtt)) {
    FUNCTIONLOG("MQTT", "Rejected blocked command: %s", command);
    return;
  }

  if (!cmd.exec(command, value, 0, CommandSource::Mqtt)) {
    FUNCTIONLOG("MQTT", "Unsupported command: %s (value: %s)", command, value);
  }
}

// Runs on the library's MQTT task, which uses a local buffer because topic[]/status[] belong to loop().
void Mqtt::_onConnect(bool sessionPresent) {
  FUNCTIONLOG("MQTT", "Connected (session present: %s)", sessionPresent ? "yes" : "no");
  // Only topics registered through onTopic() are resubscribed by the library, and this command topic is
  // configured at runtime, so subscribe explicitly on every (re)connect.
  char sub[topicSize];
  snprintf(sub, sizeof(sub), "%s%s", config.store.mqtttopic, "command");
  mqtt.mqttClient.subscribe(sub, 2);
  mqtt.rearmRequested = true;          // loop() answers this with resetReported(), then republishes all
  mqtt.availabilityRequested = true;   // the retained online matters most: the will published offline
  mqtt.publishStatus();                // forced, so the attributes do not wait out the settle window
  mqtt.playlistRequested = true;       // and the playlist revision, which may have changed while offline
}

// ---- Publishers: only ever called from loop() ----
// Every non-empty payload goes through publishRetained(): retained, explicit length, never empty, because
// an empty retained message *deletes* the topic and the consumer would lose the value entirely.
void Mqtt::publishRetained(const char* payload, uint8_t qos, bool async) {
  if (topic[0] == '\0' || payload == nullptr || payload[0] == '\0') {
    FUNCTIONLOG("MQTT", "Refusing to publish an empty payload to %s", topic);
    return;
  }
  mqttClient.publish(topic, qos, true, payload, (int)strlen(payload), async);
}

// A clean DISCONNECT means the broker will not fire the will, so the retained "online" has to be replaced
// by hand - blocking on QoS 1, so it is acknowledged before the client is stopped underneath it.
void Mqtt::publishOfflineBeforeDisconnect() {
  if (!clientStarted || !mqttClient.connected() || lastReportedAvailability != 1) return;
  strlcpy(topic, availabilityTopic, sizeof(topic));
  FUNCTIONLOG("MQTT", "Publishing offline before disconnecting");
  publishRetained(MQTT_OFFLINE, 1, false);
  lastReportedAvailability = 0;
}

void Mqtt::publishAvailabilityNow(bool online) {
  if (!mqttClient.connected()) return;
  strlcpy(topic, availabilityTopic, sizeof(topic));
  publishRetained(online ? MQTT_ONLINE : MQTT_OFFLINE, 1);
  lastReportedAvailability = online ? 1 : 0;
}

// The attributes topic: the same text the display shows, title included, and nothing about the state.
void Mqtt::publishStatusNow() {
  if (mqttClient.connected()) {
    zeroBuf();
    sprintf(topic, "%s%s", config.store.mqtttopic, "status");
    char name[statusNameSize] = {0};
    char title[statusTitleSize] = {0};
    char imageUrl[statusImageUrlSize] = {0};
    utility.escapeQuotes(config.station.name, name, sizeof(name));
    utility.escapeQuotes(config.station.title, title, sizeof(title));
    utility.escapeQuotes(audioHandlers.getArtworkImageUrl(), imageUrl, sizeof(imageUrl));
    snprintf(status, sizeof(status), "{\"station\": %d, \"name\": \"%s\", \"title\": \"%s\", \"image_url\": \"%s\", \"max_volume\": %u}", config.lastStation(), name, title, imageUrl, (unsigned)VOLUME_SCALE);
    publishRetained(status);
    markReported();
  }
}

void Mqtt::publishStateNow(uint8_t state) {
  if (!mqttClient.connected() || state > MQTT_ST_BUFFERING) return;
  zeroBuf();
  sprintf(topic, "%s%s", config.store.mqtttopic, "state");
  publishRetained(MQTT_STATE_TOKENS[state]);
  lastReportedState = state;
}

void Mqtt::publishModeNow() {
  if (!mqttClient.connected()) return;
  zeroBuf();
  sprintf(topic, "%s%s", config.store.mqtttopic, "mode");
  sprintf(status, "%u", (unsigned)config.getMode()); // 0 web radio, 1 SD card, raw like the volume topic
  publishRetained(status);
  lastReportedMode = (uint8_t)config.getMode();
}

void Mqtt::publishIpNow() {
  if (!mqttClient.connected()) return;
  zeroBuf();
  sprintf(topic, "%s%s", config.store.mqtttopic, "ip");
  strlcpy(status, WiFi.localIP().toString().c_str(), sizeof(status));
  publishRetained(status);
  lastReportedIp = WiFi.localIP();
}

// Carries a revision rather than the URL: the file is served over HTTP, and the CRC32 is the signal that
// a consumer should fetch it again.  Reading the file is why this runs on request instead of every pass.
void Mqtt::publishPlaylistNow() {
  if (!mqttClient.connected()) return;

  char revision[revisionSize] = {0};
  File playlist = config.SDPLFS()->open(REAL_PLAYL, "r");
  if (playlist) {
    snprintf(revision, sizeof(revision), "%08x", (unsigned)fileCRC32(playlist, playlist.size()));
    playlist.close();
  } else {
    strlcpy(revision, "none", sizeof(revision));
  }
  if (strcmp(revision, lastReportedRevision) == 0) return;

  zeroBuf();
  sprintf(topic, "%s%s", config.store.mqtttopic, "playlist");
  publishRetained(revision);
  strlcpy(lastReportedRevision, revision, sizeof(lastReportedRevision));
}

void Mqtt::publishVolumeNow() {
  if (mqttClient.connected()) {
    zeroBuf();
    sprintf(topic, "%s%s", config.store.mqtttopic, "volume");
    sprintf(status, "%d", config.store.volume); // the shared payload buffer, so publishRetained() guards it too
    publishRetained(status);
    lastReportedVolume = (int16_t)config.store.volume;
  }
}

void Mqtt::_onDisconnect(bool sessionPresent) {
  // Reconnecting is the library's job (setAutoReconnect), so this is informational only.
  FUNCTIONLOG("MQTT", "Disconnected (session present: %s)", sessionPresent ? "yes" : "no");
}

// Runs on the library's MQTT task and must return immediately, so the payload is only copied into the
// single-slot mailbox loop() drains; no length is passed, but the payload is NUL-terminated.
void Mqtt::_onMessage(char* topic, char* payload, int retain, int qos, bool dup) {
  (void)topic; (void)retain; (void)qos; (void)dup;
  if (payload == nullptr) return;
  const size_t len = strlen(payload);
  if (len == 0) return;
  if (len > MQTT_URL_SIZE) {
    FUNCTIONLOG("MQTT", "Ignored oversized payload (%d bytes)", (int)len);
    return;
  }
  if (mqtt.messagePending) {
    FUNCTIONLOG("MQTT", "Dropping payload, previous command still queued");
    return;
  }
  memcpy(mqtt.incomingPayload, payload, len + 1); // includes the terminator
  mqtt.messagePending = true;
}

Mqtt mqtt;
