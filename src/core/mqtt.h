#ifndef mqtt_h
#define mqtt_h

#ifdef MQTT_ENABLE // ============================== Everything ignored if not defined ==============================
#include "options.h"
#include <AsyncMqttClient.h>

class Mqtt {
public:
  void init();
  void connect();
  void publishStatus();
  void publishPlaylist();
  void publishVolume();
private:
  static constexpr size_t statusNameSize = (STATION_FIELD_LENGTH / 2) + 1;
  static constexpr size_t statusTitleSize = 129;
  static constexpr size_t statusImageUrlSize = MQTT_URL_SIZE + 1;
  static constexpr size_t statusJsonOverhead = 96;

  AsyncMqttClient mqttClient;
  TimerHandle_t mqttReconnectTimer = nullptr;
  char topic[100];
  char status[statusNameSize + statusTitleSize + statusImageUrlSize + statusJsonOverhead];
  void zeroBuf();
  static void _connectCb();
  static void _onConnect(bool sessionPresent);
  static void _onDisconnect(AsyncMqttClientDisconnectReason reason);
  static void _onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
};

extern Mqtt mqtt;

#endif // #ifdef MQTT_ENABLE

#endif // mqtt_h