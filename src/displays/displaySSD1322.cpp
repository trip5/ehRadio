#include "../core/options.h"
#if DSP_MODEL==DSP_SSD1322
#include "dspcore.h"
#include "../core/config.h"

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        16000000UL      /*  set it to 0 for system default */
#endif

  DspCore::DspCore(): Jamis_SSD1322(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {}

void DspCore::initDisplay() {
#if !OLED_GREYSCALE
  #include "tools/oledcolorfix.h"
#else
  // Ordered to match theme_t in core/config.h
    config.theme.background = TFT_BG;
    config.theme.meta       = GRAY_9;
    config.theme.metabg     = TFT_BG;
    config.theme.metafill   = GRAY_9;
    config.theme.title1     = GRAY_B;
    config.theme.title2     = GRAY_3;
    config.theme.digit      = TFT_FG;
    config.theme.div        = GRAY_9;
    config.theme.line       = GRAY_9;
    config.theme.weather    = GRAY_2;
    config.theme.vuaxis     = GRAY_3;
    config.theme.vupeak     = GRAY_2;
    config.theme.vumax      = GRAY_2;
    config.theme.vumin      = GRAY_B;
    config.theme.clock      = TFT_FG;
    config.theme.clockbg    = GRAY_1;
    config.theme.seconds    = GRAY_9;
    config.theme.secondsbg  = GRAY_1;   // mirrors clockbg, as oledcolorfix.h does
    config.theme.dow        = GRAY_7;
    config.theme.date       = GRAY_7;
    config.theme.clockss    = TFT_FG;
    config.theme.clockbgss  = GRAY_1;
    config.theme.secondsss  = GRAY_9;
    config.theme.secondsbgss = GRAY_1;
    config.theme.dowss      = GRAY_7;
    config.theme.datess     = GRAY_7;
    config.theme.buffer     = TFT_FG;
    config.theme.ip         = GRAY_2;
    config.theme.vol        = TFT_FG;
    config.theme.rssi       = GRAY_5;
    config.theme.battery    = TFT_FG;
    config.theme.bitrate    = TFT_FG;
    config.theme.volbarout  = GRAY_9;
    config.theme.volbarin   = GRAY_9;
    config.theme.playlist[0] = GRAY_7;
    config.theme.playlist[1] = GRAY_3;
    config.theme.playlist[2] = GRAY_1;
    config.theme.playlist[3] = GRAY_1;
    config.theme.playlist[4] = GRAY_1;
    config.theme.plcurrent     = TFT_BG;   // reversed selector so the current row stands out
    config.theme.plcurrentbg   = GRAY_9;
    config.theme.plcurrentfill = GRAY_9;
    // for(byte i=0;i<5;i++) config.theme.playlist[i] = GRAY_3;
#endif

  begin();
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black){ clearDisplay(); }
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
