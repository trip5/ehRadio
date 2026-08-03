#ifndef netserver_h
#define netserver_h

#include <ESPAsyncWebServer.h>
#include "../displays/widgets/widgetsconfig.h"

enum requestType_e : uint8_t  { PLAYLIST=1, STATION=2, STATIONNAME=3, ITEM=4, TITLE=5, VOLUME=6, NRSSI=7, BITRATE=8, MODE=9, EQUALIZER=10, BALANCE=11, PLAYLISTSAVED=12, GETINDEX=13, GETACTIVE=14, GETSYSTEM=15, GETSCREEN=16, GETLOCALE=17, GETWEATHER=18, GETCONTROLS=19, DSPON=20, SDPOS=21, SDLEN=22, SDSHUFFLE=23, SDINIT=24, GETPLAYERMODE=25, CHANGEMODE=26, SEARCH_DONE=27, SEARCH_FAILED=28, CURATED_INDEX_DONE=29, CURATED_PLAYLIST_DONE=30, CURATED_FAILED=31, GETMQTT=32, GETBATTERY=33, ARTWORK=34 };

/* PSRAM-backed static file cache entry */
struct CachedFile {
    char path[32];          /* URL path e.g. "/script.js" */
    const char* data;       /* PSRAM pointer (plain data, or NULL) */
    size_t size;            /* Size of plain data */
    const char* gzData;     /* PSRAM pointer (gzipped data, or NULL) */
    size_t gzSize;          /* Size of gzipped data */
    const char* contentType; /* MIME type string */
};

/* PSRAM-backed static file cache — loads all WebUI files from SPIFFS once at boot */
class StaticFileCache {
public:
    StaticFileCache() : count(0) { memset(entries, 0, sizeof(entries)); }
    void loadAll();                 /* Read all wwwFiles[] from SPIFFS into PSRAM */
    const CachedFile* find(const char* urlPath) const;  /* Lookup by URL path */
    bool invalidate(const char* urlPath);                /* Reload single file after update */
    void freeAll();                 /* Free all PSRAM allocations */
    size_t totalBytes() const;      /* Total PSRAM used by cached files */
private:
    static constexpr int MAX_ENTRIES = 32;
    CachedFile entries[MAX_ENTRIES];
    int count;
    void loadOne(const char* filename, int index);  /* Load single file into entries[index] */
};
enum import_e      : uint8_t  { IMDONE=0, IMWIFI=2 };
// the only place we use the 32 pixel .png icon is here for empty_fs
const char emptyfs_html[] PROGMEM = R"(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1, minimum-scale=0.25"><meta charset="UTF-8">
<link rel="icon" type="image/png" href="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYAAABzenr0AAAACXBIWXMAABJ0AAASdAHeZh94AAAAlElEQVRYw+2XSw6AIAxEG9fe/0zeSuOCBI2FVqafKCTdFeYFBmiJBse2LjtFjn8DnOIlwgHcIe7iLgC1SA/ABKgW4gBMd+RJlAuTq2QKIJmIBLjkaRYYFWdzW6ZCAHTzpItqAMSgmrNFxmuDTQC4+ARI4QFvCMh7Dxf3gDD5+9NvecobEAqT34DN8klQS7r1Cp9oTA8ah+h47LQOmQAAAABJRU5ErkJggg==">
<title>ehRadio - WEB Board Uploader</title><style>html, body{margin: 0; padding: 0; height: 100%; background-color:#000; color:#eecccc; font-size:20px; display:flex; flex-direction:column;}
hr{margin:20px 0;border:0; border-top:#555 1px solid;} p{text-align:center; margin-bottom:10px;} section{max-width:500px; text-align:center; margin:0 auto 30px auto; padding:20px; flex:1;}
.hidden{display:none;} a{color: #ccccee; font-size:14px; text-decoration: none; font-weight: bold;} a:hover{text-decoration: underline}
#copy{text-align: center; padding: 14px; font-size: 14px;}
input[type=file]{color:#ccc;} input[type=file]::file-selector-button, input[type=submit]{border:2px solid #eecccc;color:#000;padding:6px 16px; border-radius:25px; background-color:#eecccc;margin:0 6px; cursor:pointer;} input[type=file]:hover::file-selector-button, input[type=submit]:hover{background-color:#ccccee;border-color:#ccccee}
input[type=submit]{font-size:18px; padding:8px 26px; margin-top:10px; font-family:Times;} span{color:#ccc} .flex{display:flex; justify-content: space-around;margin-top:10px;}
input[type=text],input[type=password]{width:170px; background:#272727; color:#eecccc; padding:6px 12px; font-size:20px; border:#2d2d2d 1px solid; margin:4px 0 0 4px; border-radius:4px; outline:none;}
select{background:#272727; color:#eecccc; padding:4px 8px; font-size:16px; border:#2d2d2d 1px solid; border-radius:4px; margin-left:8px;}
@media screen and (max-width:480px) {section{zoom:0.7; -moz-transform:scale(0.7);}}
</style>
<script type="text/javascript" src="/variables.js"></script>
<script type="text/javascript" src="/locale.js"></script>
</head><body><section>
<div style="text-align:center;padding:10px 20px 0;">
<span data-i18n="lbl_webui_locale">WebUI Locale</span>
<select id="localePicker"><option value="">Loading...</option></select>
</div>
<hr />
<div id="uploader">
<h2>ehRadio - <span data-i18n="z_webui_uploader">WebUI Files Uploader</span></h2>
<hr />
<span data-i18n="z_select_www_files">Select ALL files from data/www/ and upload them using the form below</span>
<form action="/webboard" method="post" enctype="multipart/form-data">
<p><label for="www" data-i18n="z_www_files">www Files:</label> <input type="file" name="www" id="www" multiple></p>
<span data-i18n="z_upload_csv">You can also upload playlist.csv and wifi.csv files from your backup</span>
<p><label for="data" data-i18n="z_csv_files">CSV Files:</label><input type="file" name="data" id="data" multiple></p>
<p><input type="submit" name="submit" value="Upload Files" data-i18n="z_upload_files"></p>
</form>
</div>
<div style="padding:10px 0 0;" id="wupload">
<div id="credtitle-x">
<hr />
<span data-i18n="z_set_wifi">Setup Wi-Fi connection first!</span>
</div>
<div id="credtitle" class="hidden">
<h2>ehRadio - <span data-i18n="z_credentials">Credentials</span></h2>
<hr />
</div>
<form name="wifiform" method="post" enctype="multipart/form-data">
<div class="flex"><div><label for="ssid" data-i18n="z_ssid">SSID:</label><input type="text" id="ssid" name="ssid" value="" maxlength="30" autocomplete="off"></div>
<div><label for="pass" data-i18n="z_password">password:</label><input type="password" id="pass" name="pass" value="" maxlength="40" autocomplete="off"></div>
</div>
<p><input type="submit" name="submit" value="Save Credentials" data-i18n="z_save_credentials"></p>
</form>
</div>
<div id="downloader" class="hidden">
<h2>ehRadio - <span data-i18n="z_webui_downloader">WebUI Files Downloader</span></h2>
<hr />
<span data-i18n="z_downloading">The WebUI files are currently downloading. Please wait a few moments. The device will restart and this page will reload when everything is ready.</span>
</div>
</section>
<p><a href="/emergency" data-i18n="z_emergency_firmware_uploader">Emergency Firmware Uploader</a></p>
<div id="copy">powered by <a target="_blank" href="https://trip5.github.io/ehRadio/">ehRadio</a> | <span id="version"></span></div>
</body>
<script>
document.wifiform.action = `/${formAction}`;
if (playMode === 'player') {
document.getElementById("wupload").classList.add("hidden");
if (onlineUpdCapable) {
document.getElementById('downloader').classList.remove("hidden");
document.getElementById('uploader').classList.add("hidden");
setTimeout(function(){ window.location.reload(true); }, 10000);
}
} else if (onlineUpdCapable) {
document.getElementById('credtitle').classList.remove("hidden");
document.getElementById('credtitle-x').classList.add("hidden");
document.getElementById('uploader').classList.add("hidden");
}
document.getElementById("version").innerHTML=radioVersion;
var activeLocale=(window.location.search.match(/l=([^&]+)/)||[])[1]||(typeof currentLocale!=='undefined'?currentLocale:'');
fetch('/wwwlocale.json').then(function(r){return r.json();}).then(function(locales){
var sel=document.getElementById('localePicker');
sel.innerHTML='';
Object.entries(locales).sort().forEach(function(e){
var o=document.createElement('option');
o.value=e[0];o.textContent=e[0]+': '+e[1];
sel.appendChild(o);
});
sel.value=activeLocale;
sel.onchange=function(){window.location='/?l='+this.value;};
});
</script>
</html>
)";
const char index_html[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta name="theme-color" content="#eecccc">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="default">
  <link rel="icon" type="image/png" href="icon.png">
  <link rel="stylesheet" id="themeCSS" href="theme.css" type="text/css" />
  <link rel="stylesheet" id="styleCSS" href="style.css" type="text/css" />
  <script type="text/javascript" src="/variables.js"></script>
  <script type="text/javascript" src="/locale.js"></script>
  <script type="text/javascript" src="/script.js"></script>
  <script type="text/javascript" src="/script2.js"></script>
  </head>
<body>
<div id="content" class="hidden progmem"></div><!--content--><div id="progress"><span id="loader"></span></div>
</body>
</html>
)";
const char emergency_form[] PROGMEM = R"(
<form method="POST" action="/update" enctype="multipart/form-data">
  <input type="hidden" name="updatetarget" value="fw" />
  <label for="uploadfile">upload firmware</label>
  <input type="file" id="uploadfile" accept=".bin,.hex" name="update" />
  <input type="submit" value="Update" />
</form>
)";
struct nsRequestParams_t
{
  requestType_e type;
  uint8_t clientId;
};

void mqttplaylistSend();
char* updateError();
void handleSearch(AsyncWebServerRequest *request);
void handleSearchPost(AsyncWebServerRequest *request);
void handleReady(AsyncWebServerRequest *request);
const char *getFormat(BitrateFormat _format);
void handleUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
void selectRadioBrowserServer();
void vTaskSearchRadioBrowser(void *pvParameters);
void vTaskFetchCuratedIndex(void *pvParameters);
void vTaskFetchCuratedPlaylist(void *pvParameters);
void launchPlaybackTask(const String& url, const String& name);
void radioBrowserSendClick(const char* stationUrl);
void processRadioBrowserClick();
void checkForOnlineUpdate();
void startOnlineUpdate();
void handleNotFound(AsyncWebServerRequest * request);
void handleIndex(AsyncWebServerRequest * request);

class NetServer {
  public:
    import_e importRequest = IMDONE;
    bool resumePlay = false;
    char chunkedPathBuffer[40] = {0};
    bool irRecordEnable = false;
    String newVersion = String(RADIOVERSION);
    bool newVersionAvailable = false;
  public:
    NetServer() {};
    bool begin(bool quiet=false);
    void startLoopTask();
    void restartMdns();
    void chunkedHtmlPage(const String& contentType, AsyncWebServerRequest *request, const char * path);
    void loop();
    void irToWs(const char* protocol, uint64_t irvalue);
    void irValsToWs();
    void onWsMessage(void *arg, uint8_t *data, size_t len, uint8_t clientId);
    void requestOnChange(requestType_e request, uint8_t clientId);
    void resetQueue();
    void triggerMqttPlaylistSync();
    void setBootReady(bool val) { bootReady = val; }
    bool isBootReady() const { return bootReady; }

    void setRSSI(int val) { rssi = val; };
    int  getRSSI()        { return rssi; };

    /* PSRAM static file cache access */
    void invalidateCache(const char* urlPath) { fileCache.invalidate(urlPath); }
    void reloadCache() { fileCache.loadAll(); }
    const StaticFileCache& getFileCache() const { return fileCache; }
    StaticFileCache& getFileCache() { return fileCache; } // non-const overload for free functions

  private:
    requestType_e request = PLAYLIST;
    QueueHandle_t nsQueue;
    int rssi = 0;
    bool bootReady = false;
    StaticFileCache fileCache;   /* PSRAM-backed WebUI file cache */

    static size_t chunkedHtmlPageCallback(uint8_t* buffer, size_t maxLen, size_t index);
    void processQueue();
    void getPlaylist(uint8_t clientId);
    int _readPlaylistLine(File &file, char * line, size_t size);
};

extern NetServer netserver;
extern AsyncWebSocket websocket;
extern TaskHandle_t nsTaskHandle;

#endif
