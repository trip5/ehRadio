#ifndef mqtt_h
#define mqtt_h

#include "options.h"
#include "config.h"            // STATION_FIELD_LENGTH
#include <PsychicMqttClient.h> // elims/PsychicMqttClient - the ESP-IDF MQTT client wrapper

static constexpr size_t MQTT_URL_SIZE = 512; // Shared URL buffer cap for MQTT commands, artwork URLs and browse URLs

class Mqtt {
public:
  // Public entry points only record a request: their callers are the watchdog-enrolled AsyncTCP task and
  // the WiFi event task, and the library blocks; loop() does the work, serviced by one consumer only.
  void init();
  // The only publish another module asks for, because a playlist's contents change without its topic
  // changing; status, volume, state, mode and ip are published by loop() itself when they change.
  void publishPlaylist();
  void loop();
private:
  void publishStatus();
  void publishVolume();
  static constexpr size_t statusNameSize = (STATION_FIELD_LENGTH / 2) + 1;
  static constexpr size_t statusTitleSize = 129;
  static constexpr size_t statusImageUrlSize = MQTT_URL_SIZE + 1;
  static constexpr size_t statusJsonOverhead = 96;
  static constexpr size_t serverUriSize = 128;
  static constexpr size_t topicSize = 100;
  static constexpr size_t revisionSize = 12;         // "%08x" plus room for "none"
  static constexpr uint8_t stateUnknown = 0xFF;      // nothing published yet, so force one
  // Measured from the LAST request, not the last apply: one "Apply Changes" sends six commands, and
  // applying the first immediately would connect with the values from before the rest arrived.
  static constexpr uint32_t applyDebounceMs = 500;
  // Give up waiting for the startup downloads after this long, so a stuck busy flag cannot keep MQTT off.
  static constexpr uint32_t servicesWaitLimitMs = 60000;

  // The published state that loop() compares, the derived state token included so one settle window covers
  // everything.  In the object, never on the stack: loop() runs on the netserver loop task, which is 4 KB
  // on the non-S3 builds and shared with the queue work and deferred command execution.
  struct StatusSnapshot {
    char name[STATION_FIELD_LENGTH];
    char title[STATION_FIELD_LENGTH];
    char imageUrl[MQTT_URL_SIZE + 1];
    uint16_t station;
    uint8_t stateToken;
  };

  PsychicMqttClient mqttClient;
  char serverUri[serverUriSize];           // setServer() keeps this pointer, so it must outlive the call
  char availabilityTopic[topicSize];       // setWill() keeps this pointer too, so it cannot be a local
  bool callbacksBound = false;
  bool clientStarted = false;              // we called connect(); the library's start is not idempotent
  bool idfLogsConfigured = false;          // IDF transport tags silenced (see MQTT_DEBUG)
  char appliedUser[40];                    // what the running client was configured with, for the no-op skip
  char appliedPass[48];

  // One flag per request: a plain store is atomic and loop() clears a flag before acting on it, so a
  // request arriving meanwhile is served on the next pass instead of being lost in a race.
  volatile bool applyRequested = false;
  volatile uint32_t applyRequestedMs = 0; // when applyRequested was last raised, for the services wait above
  volatile bool statusRequested = false;
  volatile bool volumeRequested = false;
  volatile bool playlistRequested = false;
  volatile bool availabilityRequested = false;
  volatile bool rearmRequested = false;    // raised by _onConnect; loop() answers it with resetReported()
  volatile bool messagePending = false;    // single-slot mailbox from the MQTT task to loop()

  // reported = what the broker holds for us; candidate = the live values, published only once they stay
  // unchanged for MQTT_STATUS_SETTLE_MS - a station switch mutates name, title and artwork separately.
  StatusSnapshot reported;
  StatusSnapshot candidate;
  uint32_t candidateSince = 0;
  bool candidateActive = false;
  bool reportedValid = false;
  char lastReportedRevision[revisionSize]; // playlist CRC32, so the fetch only happens on a real change
  IPAddress lastReportedIp;
  int16_t lastReportedVolume = -1;
  uint8_t lastReportedState = stateUnknown;
  uint8_t lastReportedMode = stateUnknown;
  int8_t lastReportedAvailability = -1;    // -1 unknown, 0 offline, 1 online

  char incomingPayload[MQTT_URL_SIZE + 1]; // written by the MQTT task while messagePending is false
  char workPayload[MQTT_URL_SIZE + 1];     // loop()'s private copy, out of the MQTT task's reach
  char command[65];
  char value[MQTT_URL_SIZE + 1];
  char topic[topicSize];
  char status[statusNameSize + statusTitleSize + statusImageUrlSize + statusJsonOverhead];
  void zeroBuf();
  void applySettings();
  void processPendingMessage();
  uint8_t stateToken();      // the machine-readable state, in HA's vocabulary
  void storeCandidate();     // copy the live values into candidate
  bool matchesCandidate();   // do the live values still equal candidate?
  bool differsFromReported();// do the live values differ from what the broker has?
  void markReported();       // the settled candidate is what the broker now holds
  bool wouldEraseTitle();
  void holdStatus();
  void resetReported();
  void publishRetained(const char* payload, uint8_t qos = 0, bool async = true);
  void publishOfflineBeforeDisconnect();
  void publishAvailabilityNow(bool online);
  void publishStatusNow();
  void publishStateNow(uint8_t state);
  void publishModeNow();
  void publishIpNow();
  void publishPlaylistNow();
  void publishVolumeNow();
  static void _onConnect(bool sessionPresent);
  static void _onDisconnect(bool sessionPresent);
  static void _onMessage(char* topic, char* payload, int retain, int qos, bool dup);
};

extern Mqtt mqtt;

#endif // mqtt_h
