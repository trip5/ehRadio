#ifndef telnet_h
#define telnet_h
#include <WiFi.h>

#define MAX_TLN_CLIENTS 5
#define MAX_PRINTF_LEN 220
#define BOOTLOG(...) { char buf[256]; snprintf(buf, sizeof(buf), __VA_ARGS__); telnet.printf("##[BOOT]#\t%s\r\n",buf); }

class Telnet {
  public:
    Telnet() {};
    bool begin(bool quiet=false);
    void stop();
    void cleanupClients();
    void loop();
    void print(const char *buf);
    void print(uint8_t id, const char *buf);
    void printf(const char *format, ...);
    void printf(uint8_t id, const char *format, ...);
    void info();
    void showPromptNow(uint8_t clientId);
  protected:
    WiFiServer server = WiFiServer(23);
    WiFiClient clients[MAX_TLN_CLIENTS];
    void emptyClientStream(WiFiClient client);
    void on_connect(const char* str, uint8_t clientId);
    void on_input(const char* str, uint8_t clientId);
  private:
    bool _isIPSet(IPAddress ip);
    void handleSerial();
};

extern Telnet telnet;

#endif
