#include "options.h"
#include <ctype.h>
#include <stdarg.h>
#include "commandhandler.h"
#include "config.h"
#include "logging.h"
#include "network.h"
#include "telnet.h"

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

static void normalize_to_crlf(const char *buf, char *outbuf, size_t outbuf_len) {
  size_t oi = 0;
  size_t buf_len = strlen(buf);
  
  // Quick optimization: check if conversion is needed (look for lone \n without \r)
  bool needs_conversion = false;
  for (size_t i = 0; i < buf_len; ++i) {
    if (buf[i] == '\n' && (i == 0 || buf[i-1] != '\r')) {
      needs_conversion = true;
      break;
    }
  }
  
  // If no conversion needed, just copy the string
  if (!needs_conversion) {
    size_t copy_len = (buf_len < outbuf_len - 1) ? buf_len : outbuf_len - 1;
    memcpy(outbuf, buf, copy_len);
    outbuf[copy_len] = '\0';
    return;
  }
  
  // Perform conversion
  for (size_t i = 0; i < buf_len && oi + 1 < outbuf_len; ++i) {
    char c = buf[i];
    if (c == '\n') {
      if (i > 0 && buf[i-1] == '\r') {
        // already CRLF, copy '\n'
        outbuf[oi++] = '\n';
      } else {
        // insert CR then LF - ensure space for both characters and null terminator
        if (oi + 2 < outbuf_len) {
          outbuf[oi++] = '\r';
          outbuf[oi++] = '\n';
        } else {
          break;  // Not enough space
        }
      }
    } else {
      outbuf[oi++] = c;
    }
  }
  // Null-terminate
  if (oi < outbuf_len) outbuf[oi] = '\0'; else outbuf[outbuf_len-1] = '\0';
}

static void trimInPlace(char* text) {
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

static void stripWrappingQuotes(char* text) {
  size_t len = strlen(text);
  if (len < 2) return;
  if ((text[0] == '"' && text[len - 1] == '"') || (text[0] == '\'' && text[len - 1] == '\'')) {
    memmove(text, text + 1, len - 2);
    text[len - 2] = '\0';
  }
}

static bool startsWithHttp(const char* text) {
  return strncmp(text, "http://", 7) == 0 || strncmp(text, "https://", 8) == 0;
}

static bool parseTelnetCommand(const char* input, char* command, size_t commandSize, char* value, size_t valueSize) {
  if (!input || input[0] == '\0') return false;

  command[0] = '\0';
  value[0] = '\0';

  const char* eq = strchr(input, '=');
  if (eq) {
    size_t commandLen = static_cast<size_t>(eq - input);
    if (commandLen >= commandSize) commandLen = commandSize - 1;
    memcpy(command, input, commandLen);
    command[commandLen] = '\0';
    strlcpy(value, eq + 1, valueSize);
  } else {
    const char* open = strchr(input, '(');
    const char* close = strrchr(input, ')');
    if (open && close && close > open) {
      size_t commandLen = static_cast<size_t>(open - input);
      if (commandLen >= commandSize) commandLen = commandSize - 1;
      memcpy(command, input, commandLen);
      command[commandLen] = '\0';

      size_t valueLen = static_cast<size_t>(close - open - 1);
      if (valueLen >= valueSize) valueLen = valueSize - 1;
      memcpy(value, open + 1, valueLen);
      value[valueLen] = '\0';
    } else {
      const char* space = strchr(input, ' ');
      if (space) {
        size_t commandLen = static_cast<size_t>(space - input);
        if (commandLen >= commandSize) commandLen = commandSize - 1;
        memcpy(command, input, commandLen);
        command[commandLen] = '\0';
        strlcpy(value, space + 1, valueSize);
      } else {
        strlcpy(command, input, commandSize);
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
  if (strcmp(command, "mode") == 0 && strcmp(value, "2") == 0) {
    strlcpy(value, "-1", valueSize);
  }

  return command[0] != '\0';
}

bool Telnet::_isIPSet(IPAddress ip) {
  return ip.toString() == "0.0.0.0";
}

bool Telnet::begin(bool quiet) {
  Serial.setTimeout(TELNET_INPUT_TIMEOUT_MS);
  resetInputBuffer(serialInputBuffer, serialInputLength);
  for (int i = 0; i < MAX_TLN_CLIENTS; i++) {
    resetInputBuffer(clientInputBuffer[i], clientInputLength[i]);
  }

  if (network.status==SDREADY) {
    BOOTLOG("Ready in SD Mode!");
    BOOTLOG("------------------------------------------------");
    BOOTLOG("");
    return true;
  }
  if (!quiet) BOOTLOGX("telnet.begin\t");
  if (WiFi.status() == WL_CONNECTED || _isIPSet(WiFi.softAPIP())) {
    server.begin();
    server.setNoDelay(true);
    if (!quiet) {
      SERIALLOG("done");
      BOOTLOG("");
      BOOTLOG("Ready! Go to http:/%s/ to configure", WiFi.localIP().toString().c_str());
      BOOTLOG("------------------------------------------------");
      BOOTLOG("");
    }
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
  if (network.status==SDREADY || network.status!=CONNECTED) {
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
    delay(1000);
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
  normalize_to_crlf(buf, outbuf, sizeof(outbuf));

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
  normalize_to_crlf(buf, outbuf, sizeof(outbuf));

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
  if (parseTelnetCommand(str, fallbackCommand, sizeof(fallbackCommand), fallbackValue, sizeof(fallbackValue))) {
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
      printf(clientId, "  volume <0-254>         Set volume\r\n");
      printf(clientId, "  volup | voldown        Step volume\r\n");
      printf(clientId, "  play <index>           Play station number\r\n");
      printf(clientId, "  start | stop           Start/Stop playback\r\n");
      printf(clientId, "  sleep <for>[,<after>]  Sleep timer\r\n");
      printf(clientId, "  mode <0|1|2>           0=Radio(Web), 1=SD Card, 2=Cycle\r\n");
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
