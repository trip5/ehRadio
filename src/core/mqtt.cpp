#include "options.h"
#ifdef MQTT_ENABLE // ============================== Everything ignored if not defined ==============================

#include <ctype.h>
#include <WiFi.h>
#include "commandhandler.h"
#include "audiohandlers.h"
#include "config.h"
#include "logging.h"
#include "mqtt.h"
#include "player.h"
#include "utility.h"

namespace {

void trimInPlace(char* text) {
  if (!text || text[0] == '\0') return;

  char* begin = text;
  while (*begin && isspace(static_cast<unsigned char>(*begin))) {
    ++begin;
  }
  if (begin != text) {
    memmove(text, begin, strlen(begin) + 1);
  }

  size_t len = strlen(text);
  while (len > 0 && isspace(static_cast<unsigned char>(text[len - 1]))) {
    text[--len] = '\0';
  }
}

void stripWrappingQuotes(char* text) {
  size_t len = strlen(text);
  if (len < 2) return;
  if ((text[0] == '"' && text[len - 1] == '"') || (text[0] == '\'' && text[len - 1] == '\'')) {
    memmove(text, text + 1, len - 2);
    text[len - 2] = '\0';
  }
}

bool startsWithHttp(const char* text) {
  return strncmp(text, "http://", 7) == 0 || strncmp(text, "https://", 8) == 0;
}

bool parsePayloadToCommand(const char* payload, char* command, size_t commandSize, char* value, size_t valueSize) {
  if (!payload || payload[0] == '\0') return false;

  command[0] = '\0';
  value[0] = '\0';

  if (startsWithHttp(payload)) {
    strlcpy(command, "burl", commandSize);
    strlcpy(value, payload, valueSize);
    return true;
  }

  const char* eq = strchr(payload, '=');
  if (eq) {
    size_t commandLen = static_cast<size_t>(eq - payload);
    if (commandLen >= commandSize) commandLen = commandSize - 1;
    memcpy(command, payload, commandLen);
    command[commandLen] = '\0';
    strlcpy(value, eq + 1, valueSize);
  } else {
    const char* open = strchr(payload, '(');
    const char* close = strrchr(payload, ')');
    if (open && close && close > open) {
      size_t commandLen = static_cast<size_t>(open - payload);
      if (commandLen >= commandSize) commandLen = commandSize - 1;
      memcpy(command, payload, commandLen);
      command[commandLen] = '\0';

      size_t valueLen = static_cast<size_t>(close - open - 1);
      if (valueLen >= valueSize) valueLen = valueSize - 1;
      memcpy(value, open + 1, valueLen);
      value[valueLen] = '\0';
    } else {
      const char* space = strchr(payload, ' ');
      if (space) {
        size_t commandLen = static_cast<size_t>(space - payload);
        if (commandLen >= commandSize) commandLen = commandSize - 1;
        memcpy(command, payload, commandLen);
        command[commandLen] = '\0';
        strlcpy(value, space + 1, valueSize);
      } else {
        strlcpy(command, payload, commandSize);
      }
    }
  }

  trimInPlace(command);
  trimInPlace(value);
  stripWrappingQuotes(value);

  if (strcmp(command, "play") == 0) {
    if (value[0] == '\0') {
      strlcpy(command, "start", commandSize);
    } else if (startsWithHttp(value)) {
      strlcpy(command, "burl", commandSize);
    }
  }

  return command[0] != '\0';
}

} // namespace

void Mqtt::zeroBuf() { memset(topic, 0, sizeof(topic)); memset(status, 0, sizeof(status)); }

void Mqtt::_connectCb() { mqtt.connect(); }

void Mqtt::connect() { mqttClient.connect(); }

void Mqtt::init() {
  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void*)0, reinterpret_cast<TimerCallbackFunction_t>(_connectCb));
  mqttClient.onConnect(_onConnect);
  mqttClient.onDisconnect(_onDisconnect);
  mqttClient.onMessage(_onMessage);
  if (strlen(config.store.mqttuser)>0) mqttClient.setCredentials(config.store.mqttuser, config.store.mqttpass);
  mqttClient.setServer(config.store.mqtthost, config.store.mqttport);
  connect();
}

void Mqtt::_onConnect(bool sessionPresent) {
  mqtt.zeroBuf();
  sprintf(mqtt.topic, "%s%s", config.store.mqtttopic, "command");
  mqtt.mqttClient.subscribe(mqtt.topic, 2);
  mqtt.publishStatus();
  mqtt.publishVolume();
  mqtt.publishPlaylist();
}

void Mqtt::publishStatus() {
  if (mqttClient.connected()) {
    zeroBuf();
    sprintf(topic, "%s%s", config.store.mqtttopic, "status");
    char name[statusNameSize] = {0};
    char title[statusTitleSize] = {0};
    char imageUrl[statusImageUrlSize] = {0};
    utility.escapeQuotes(config.station.name, name, sizeof(name));
    utility.escapeQuotes(config.station.title, title, sizeof(title));
    utility.escapeQuotes(audioHandlers.getArtworkImageUrl(), imageUrl, sizeof(imageUrl));
    snprintf(status, sizeof(status), "{\"status\": %d, \"station\": %d, \"name\": \"%s\", \"title\": \"%s\", \"image_url\": \"%s\", \"on\": %d}", player.status()==PLAYING?1:0, config.lastStation(), name, title, imageUrl, config.store.dspon);
    mqttClient.publish(topic, 0, true, status);
  }
}

void Mqtt::publishPlaylist() {
  if (mqttClient.connected()) {
    zeroBuf();
    sprintf(topic, "%s%s", config.store.mqtttopic, "playlist");
    sprintf(status, "http://%s%s", WiFi.localIP().toString().c_str(), PLAYLIST_PATH);
    mqttClient.publish(topic, 0, true, status);
  }
}

void Mqtt::publishVolume() {
  if (mqttClient.connected()) {
    zeroBuf();
    char vol[5];
    memset(vol, 0, 5);
    sprintf(topic, "%s%s", config.store.mqtttopic, "volume");
    sprintf(vol, "%d", config.store.volume);
    mqttClient.publish(topic, 0, true, vol);
  }
}

void Mqtt::_onDisconnect(AsyncMqttClientDisconnectReason reason) {
  if (WiFi.isConnected()) {
    xTimerStart(mqtt.mqttReconnectTimer, 0);
  }
}

void Mqtt::_onMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
  if (len == 0) return;
  if (len > MQTT_URL_SIZE) return;

  char raw[MQTT_URL_SIZE + 1];
  memcpy(raw, payload, len);
  raw[len] = '\0';
  trimInPlace(raw);
  if (raw[0] == '\0') return;

  char command[65] = {0};
  char value[MQTT_URL_SIZE + 1] = {0};
  if (!parsePayloadToCommand(raw, command, sizeof(command), value, sizeof(value))) {
    FUNCTIONLOG("MQTT", "Ignored unparsed payload: %s", raw);
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

Mqtt mqtt;

#endif //  #ifdef MQTT_ENABLE