#include "options.h"
#include <stdarg.h>
#include "commandhandler.h"
#include "config.h"
#include "logging.h"
#include "network.h"
#include "telnet.h"
#include "utility.h"

Telnet telnet;

namespace {

char serialInputBuffer[STATION_FIELD_LENGTH] = {0};
size_t serialInputLength = 0;
char clientInputBuffer[MAX_TLN_CLIENTS][STATION_FIELD_LENGTH] = {{0}};
size_t clientInputLength[MAX_TLN_CLIENTS] = {0};

void resetInputBuffer(char* buffer, size_t& len) {
  if (buffer && len > 0) {
    buffer[0] = '\0';
  }
  len = 0;
}

bool readStreamLine(Stream& stream, char* buffer, size_t bufferSize, size_t& len, char* outLine, size_t outLineSize) {
  while (stream.available()) {
    int ch = stream.read();
    if (ch < 0) break;

    char c = static_cast<char>(ch);
    if (c == '\r' || c == '\n') {
      // Consume a paired delimiter (CRLF or LFCR) so one Enter yields one line event.
      if (stream.available()) {
        int next = stream.peek();
        if ((next == '\r' || next == '\n') && next != c) {
          stream.read();
        }
      }

      size_t copyLen = (len < outLineSize - 1) ? len : (outLineSize - 1);
      memcpy(outLine, buffer, copyLen);
      outLine[copyLen] = '\0';
      resetInputBuffer(buffer, len);
      return true;
    }

    if (len + 1 < bufferSize) {
      buffer[len++] = c;
      buffer[len] = '\0';
    }
  }

  return false;
}

} // namespace

bool Telnet::_isIPSet(IPAddress ip) {
  return ip.toString() == "0.0.0.0";
}

bool Telnet::begin(bool quiet) {
  Serial.setTimeout(TELNET_INPUT_TIMEOUT_MS);
  resetInputBuffer(serialInputBuffer, serialInputLength);
  for (int i = 0; i < MAX_TLN_CLIENTS; i++) {
    resetInputBuffer(clientInputBuffer[i], clientInputLength[i]);
  }

  if (network.status==SDOFFLINE) {
    BOOTLOG("Ready in SD Mode!");
    BOOTLOG("-----------------");
    return true;
  }
  if (!quiet) BOOTLOGX("telnet.begin\t");
  if (WiFi.status() == WL_CONNECTED || _isIPSet(WiFi.softAPIP())) {
    server.begin();
    server.setNoDelay(true);
    if (!quiet) SERIALLOG("done");
    return true;
  } else {
    return false;
  }
}

void Telnet::stop() {
  server.stop();
}

void Telnet::emptyClientStream(WiFiClient client) {
  client.flush();
  delay(50);
  while (client.available()) {
    client.read();
  }
}

void Telnet::cleanupClients() {
  for (int i = 0; i < MAX_TLN_CLIENTS; i++) {
    if (!clients[i].connected()) {
      if (clients[i]) {
        FUNCTIONLOG("Telnet", "Client [%d] is %s", i, clients[i].connected() ? "connected" : "disconnected");
        clients[i].stop();
      }
      resetInputBuffer(clientInputBuffer[i], clientInputLength[i]);
    }
  }
}

void Telnet::handleSerial() {
  char request[STATION_FIELD_LENGTH] = {0};
  while (readStreamLine(Serial, serialInputBuffer, sizeof(serialInputBuffer), serialInputLength, request, sizeof(request))) {
    on_input(request, 100);
  }
}

void Telnet::loop() {
  if (network.status==SDOFFLINE || network.status!=CONNECTED) {
    handleSerial();
    return;
  }
  uint8_t i;
  if (WiFi.status() == WL_CONNECTED) {
    if (server.hasClient()) {
      for (i = 0; i < MAX_TLN_CLIENTS; i++) {
        if (!clients[i] || !clients[i].connected()) {
          if (clients[i]) {
            clients[i].stop();
          }
          clients[i] = server.available();
          if (!clients[i]) FUNCTIONLOG("Telnet", "Error: available broken");
          on_connect(clients[i].remoteIP().toString().c_str(), i);
          clients[i].setNoDelay(true);
          clients[i].setTimeout(TELNET_INPUT_TIMEOUT_MS);
          resetInputBuffer(clientInputBuffer[i], clientInputLength[i]);
          emptyClientStream(clients[i]);
          break;
        }
      }
      if (i >= MAX_TLN_CLIENTS) {
        server.available().stop();
      }
    }
    for (i = 0; i < MAX_TLN_CLIENTS; i++) {
      if (clients[i] && clients[i].connected() && clients[i].available()) {
        char inputLine[STATION_FIELD_LENGTH] = {0};
        while (readStreamLine(clients[i], clientInputBuffer[i], sizeof(clientInputBuffer[i]), clientInputLength[i], inputLine, sizeof(inputLine))) {
          on_input(inputLine, i);
        }
      }
    }
  } else {
    for (i = 0; i < MAX_TLN_CLIENTS; i++) {
      if (clients[i]) {
        clients[i].stop();
      }
      resetInputBuffer(clientInputBuffer[i], clientInputLength[i]);
    }
  }
  handleSerial();
}

void Telnet::print(const char *buf) {
  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      print(id, buf);
    }
  }
  Serial.print(buf);
}   

void Telnet::print(uint8_t id, const char *buf) {
  if (id >= MAX_TLN_CLIENTS) return; // Bounds check
  if (clients[id] && clients[id].connected()) {
    clients[id].print(buf);
  }
}

void Telnet::logLine(const char *buf) {
  if (!buf) return;

  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      clients[id].print("\r");
      clients[id].print(buf);
      clients[id].print("\r\n");
      clients[id].print("> ");
    }
  }
}

void Telnet::logRaw(const char *buf) {
  if (!buf) return;

  size_t len = strlen(buf);
  bool endsWithNewline = (len > 0 && buf[len - 1] == '\n');

  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      clients[id].print("\r");
      clients[id].print(buf);
      if (endsWithNewline) {
        clients[id].print("> ");
      }
    }
  }
}

void Telnet::printf(const char *format, ...) {
  char buf[MAX_PRINTF_LEN];
  va_list args;
  va_start (args, format);
  vsnprintf(buf, MAX_PRINTF_LEN, format, args);
  va_end (args);

  // Normalize line endings: convert lone '\n' to "\r\n"
  // Use larger buffer to handle worst-case CRLF expansion (every \n -> \r\n doubles size)
  char outbuf[MAX_PRINTF_LEN * 2];
  utility.normalizeToCRLF(buf, outbuf, sizeof(outbuf));

  // Check if this is a prompt or a message
  bool isPrompt = (strcmp(outbuf, "> ") == 0);
  
  // Send to all connected clients
  for (int id = 0; id < MAX_TLN_CLIENTS; id++) {
    if (clients[id] && clients[id].connected()) {
      // For broadcasts (not prompts), clear any existing prompt first, then redraw after
      if (!isPrompt) {
        clients[id].print("\r");  // Move to start of line
        clients[id].print(outbuf);
        // If message ends with newline, redraw prompt
        size_t len = strlen(outbuf);
        if (len > 0 && outbuf[len-1] == '\n') {
          clients[id].print("> ");
        }
      } else {
        clients[id].print(outbuf);
      }
    }
  }
}

void Telnet::printf(uint8_t id, const char *format, ...) {
  char buf[MAX_PRINTF_LEN];
  va_list argptr;
  va_start(argptr, format);
  vsnprintf(buf, MAX_PRINTF_LEN, format, argptr);
  va_end(argptr);

  // Normalize line endings
  // Use larger buffer to handle worst-case CRLF expansion (every \n -> \r\n doubles size)
  char outbuf[MAX_PRINTF_LEN * 2];
  utility.normalizeToCRLF(buf, outbuf, sizeof(outbuf));

  if (id >= MAX_TLN_CLIENTS) return;

  if (clients[id] && clients[id].connected()) {
    clients[id].print(outbuf);
  }
}

void Telnet::disconnectClient(uint8_t clientId) {
  if (clientId >= MAX_TLN_CLIENTS) return;

  if (clients[clientId]) {
    clients[clientId].stop();
  }
  resetInputBuffer(clientInputBuffer[clientId], clientInputLength[clientId]);
}

void Telnet::on_connect(const char* str, uint8_t clientId) {
  FUNCTIONLOG("Telnet", "[%d] %s connected", clientId, str);
  print(clientId, "Welcome to ehRadio!\r\n(Use ^] + q  ( Ctrl+] + q ) to disconnect.)\r\n");
  showPromptNow(clientId);
}

void Telnet::showPromptNow(uint8_t clientId) {
  if (clientId < MAX_TLN_CLIENTS && clients[clientId] && clients[clientId].connected()) {
    clients[clientId].print("> ");
  }
}

void Telnet::on_input(const char* str, uint8_t clientId) {
  if (strlen(str) == 0) {
    showPromptNow(clientId);
    return;
  }

  char fallbackCommand[65];
  char fallbackValue[STATION_FIELD_LENGTH];

  auto dispatchCommand = [&](const char* command, const char* value) {
    if (cmd.isBlockedForSource(command, CommandSource::Telnet)) {
      printf(clientId, "Command is not available from telnet: %s\r\n", command);
      return true;
    }
    return cmd.exec(command, value, 0, CommandSource::Telnet);
  };

  memset(fallbackCommand, 0, sizeof(fallbackCommand));
  memset(fallbackValue, 0, sizeof(fallbackValue));
  if (utility.parseCommandLine(str, fallbackCommand, sizeof(fallbackCommand), fallbackValue, sizeof(fallbackValue))) {
    // Telnet-only alias: `mode 2` / `mode=2` means "cycle" here, whereas the board uses -1.
    if (strcmp(fallbackCommand, "mode") == 0 && strcmp(fallbackValue, "2") == 0) {
      strlcpy(fallbackValue, "-1", sizeof(fallbackValue));
    }

    if (strcmp(fallbackCommand, "quit") == 0 || strcmp(fallbackCommand, "bye") == 0) {
      disconnectClient(clientId);
      return;
    }

    if (strcmp(fallbackCommand, "help") == 0) {
      printf(clientId, "Basic commands:\r\n");
      printf(clientId, "  help                   Show this help\r\n");
      printf(clientId, "  quit | bye             Disconnect this telnet session\r\n");
      printf(clientId, "  toggle                 Play/Pause\r\n");
      printf(clientId, "  next | prev            Change station\r\n");
      printf(clientId, "  volume <0-%d>          Set volume\r\n", VOLUME_SCALE);
      printf(clientId, "  volup | voldown        Step volume\r\n");
      printf(clientId, "  play <index>           Play station number\r\n");
      printf(clientId, "  start | stop           Start/Stop playback\r\n");
      #ifndef DEEP_SLEEP_DISABLE
        printf(clientId, "  sleep <for>[,<after>]  Sleep timer\r\n");
      #endif
      printf(clientId, "  mode <0|1|2>           0=Radio(Web), 1=SD card, 2=Cycle\r\n");
      printf(clientId, "\r\n");
      printf(clientId, "For a full list, consult the documentation.\r\n");
      goto show_prompt;
    }

    if (dispatchCommand(fallbackCommand, fallbackValue)) {
      goto show_prompt;
    }
  }

  telnet.printf(clientId, "Unknown command: %s\r\n", str);
  
show_prompt:
  showPromptNow(clientId);
}
