#ifndef network_h
#define network_h
#include <Ticker.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ImprovWiFiLibrary.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDREADY };

class MyNetwork {
  public:
    n_Status_e status = FAILED;
// Ensure DNSServer full definition is available
    struct tm timeinfo = {0};
    bool firstRun = true, forceTimeSync = true, forceWeather = true;
    volatile bool lostPlaying = false, beginReconnect = false;  // volatile: accessed from multiple tasks/cores (WiFi callbacks, player loop, retry task)
    //uint8_t tsFailCnt, wsFailCnt;
    Ticker ctimer;
    char *weatherBuf = nullptr;
    bool trueWeather = false;
    DNSServer* dnsServer = nullptr;
    ImprovWiFi *improv = nullptr;
  public:
    MyNetwork() : improv(nullptr) {};
    bool wifiBegin(bool silent=false);
    void begin();
    void loopImprov();
    void setWifiParams();
    void requestTimeSync(bool withTelnetOutput=false, uint8_t clientId=0);
    void raiseSoftAP();
    void requestWeatherSync();
    bool buildWeatherString();
    void ehDPinit();
  private:
    Ticker rtimer;
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
};

void ticks();
void retryStreamConnection(void * pvParameters);
void searchWiFi(void * pvParameters);
void rebootTime();
void doSync(void * pvParameters);
bool getWeather(char *wstr);

extern MyNetwork network;
extern TaskHandle_t streamRetryTaskHandle;

extern __attribute__((weak)) void network_on_connect();

#endif
