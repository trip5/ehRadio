#ifndef network_h
#define network_h
#include <Ticker.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ImprovWiFiLibrary.h>

enum n_Status_e { CONNECTED, SOFT_AP, FAILED, SDOFFLINE };

class MyNetwork {
  public:
    n_Status_e status = FAILED;
    bool offlineMode = false;  // set by checkButtonsHeldOnBoot(); never persisted
// Ensure DNSServer full definition is available
    struct tm timeinfo = {0};
    bool firstRun = true, forceTimeSync = true, forceWeather = true;
    volatile bool lostPlaying = false, beginReconnect = false;  // volatile: accessed from multiple tasks/cores (WiFi callbacks, player loop, retry task)
    uint8_t lastBssid[6] = {0}; // captured for reconnect later
    uint8_t lastChannel = 0;
    bool lastBssidValid = false;
    //uint8_t tsFailCnt, wsFailCnt;
    Ticker ctimer;
    char *weatherBuf = nullptr;
    DNSServer* dnsServer = nullptr;
    ImprovWiFi *improv = nullptr;
  public:
    MyNetwork() : improv(nullptr) {};
    bool wifiBegin(bool silent=false);
    bool wifiBeginFast(bool silent=false); // for reconnect
    void begin();
    void loopImprov();
    void setWifiParams();
    void requestTimeSync(bool withTelnetOutput=false, uint8_t clientId=0);
    void raiseSoftAP();
    void requestWeatherSync();
    bool buildWeatherString();
    void ehDPinit();
    void cancelStreamRetry();
    void noteConnectFailure(const char* url);  // a failed user play: probe the stack and enter the ladder on WEDGED
  private:
    Ticker rtimer;
    void captureCurrentAp();
    static void WiFiReconnected(WiFiEvent_t event, WiFiEventInfo_t info);
    static void WiFiLostConnection(WiFiEvent_t event, WiFiEventInfo_t info);
};

void ticks();
bool spawnStreamRetry();
void retryStreamConnection(void * pvParameters);
void wifiReconnectionTask(void * pvParameters);
void rebootTime();
void doSync(void * pvParameters);
bool getWeather(char *wstr);

extern MyNetwork network;
extern TaskHandle_t syncTaskHandle;
extern TaskHandle_t streamRetryTaskHandle;

#endif
