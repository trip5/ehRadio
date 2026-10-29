#include "../core/options.h"
#if DSP_MODEL==DSP_SSD1322
#include "dspcore.h"
#include "../core/config.h"

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        16000000UL      /*  set it to 0 for system default */
#endif

  DspCore::DspCore(): Jamis_SSD1322(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {}

// handle lighter colors on the playlist when paged mode is on / shades when off
// Declared before initDisplay()/clearDsp() — both call it, and C++ needs the
// declaration to appear before the first use.
static void playlistColors() {
  #if OLED_GREYSCALE
    if (config.store.playlistPages) {
      for(byte i=0;i<5;i++) config.theme.playlist[i] = GRAY_3;
    } else {
      config.theme.playlist[0] = GRAY_7;
      config.theme.playlist[1] = GRAY_3;
      config.theme.playlist[2] = GRAY_1;
      config.theme.playlist[3] = GRAY_1;
      config.theme.playlist[4] = GRAY_1;
    }
  #endif
}

void DspCore::initDisplay() {
  #if !OLED_GREYSCALE
    #include "tools/oledcolorfix.h"
  #else
    // Ordered to match theme_t in core/config.h
    config.theme.background  = BLACK;
    config.theme.meta        = GRAY_B;
    config.theme.metabg      = BLACK;
    config.theme.title1      = GRAY_B;
    config.theme.title2      = GRAY_5;
    config.theme.digit       = WHITE;
    config.theme.digitbg     = GRAY_1;
    config.theme.div         = GRAY_7;
    config.theme.line        = GRAY_7;
    config.theme.weather     = GRAY_3;
    config.theme.vuaxis      = GRAY_3;
    config.theme.vupeak      = WHITE;
    config.theme.vumax       = GRAY_3;
    config.theme.vumin       = GRAY_7;
    config.theme.clock       = WHITE;
    config.theme.clockbg     = GRAY_1;
    config.theme.seconds     = GRAY_9;
    config.theme.secondsbg   = GRAY_1;
    config.theme.dow         = GRAY_5;
    config.theme.date        = GRAY_7;
    config.theme.clockss     = WHITE;
    config.theme.clockbgss   = GRAY_1;
    config.theme.secondsss   = GRAY_9;
    config.theme.secondsbgss = GRAY_1;
    config.theme.dowss       = GRAY_7;
    config.theme.datess      = GRAY_7;
    config.theme.textss      = WHITE;
    config.theme.buffer      = WHITE;
    config.theme.ip          = GRAY_3;
    config.theme.vol         = WHITE;
    config.theme.rssi        = GRAY_3;
    config.theme.battery     = WHITE;
    config.theme.bitrate     = WHITE;
    config.theme.volbarout   = GRAY_9;
    config.theme.volbarin    = GRAY_9;
    config.theme.plcurrent   = BLACK;
    config.theme.plcurrentbg = GRAY_9;
    playlistColors();
  #endif

  begin();
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black) {
  playlistColors();
  clearDisplay();
}
void DspCore::flip(){
#if DSP_WIDTH==DSP_HEIGHT
  if(ROTATE_90){
    setRotation(config.store.flipscreen?3:1);
  }else{
    setRotation(config.store.flipscreen?2:0);
  }
#else
  setRotation(config.store.flipscreen?2:0);
#endif
}
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ oled_command(SSD1322_DISPLAYOFF); }
void DspCore::wake(void){ oled_command(SSD1322_DISPLAYON); }

#endif