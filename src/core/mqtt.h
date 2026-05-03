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
  AsyncMqttClient mqttClient;
  TimerHandle_t mqttReconnectTimer = nullptr;
  char topic[100];
  char status[BUFLEN+50];
  void zeroBuf();
  static void _connectCb();
  static void _onConnect(bool sessionPresent);
  static void _onDisconnect(AsyncMqttClientDisconnectReason reason);
  static void _onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total);
};

extern Mqtt mqtt;

#endif // #ifdef MQTT_ENABLE

#endif // mqtt_h