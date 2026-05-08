#ifndef sdmanager_h
#define sdmanager_h

class SDManager : public SDFS {
  public:
    bool ready = false;
  public:
    SDManager(FSImplPtr impl) : SDFS(impl) {}
    bool start();
    void stop();
    bool cardPresent();
    void listSD(File &plSDfile, File &plSDindex, const char * dirname, uint8_t levels);
    void indexSDPlaylist();
  private:
    uint32_t _sdFCount = 0;
    bool _checkNoMedia(const char* path);
    bool _endsWith (const char* base, const char* str);
};

extern SDManager sdman;
#endif
