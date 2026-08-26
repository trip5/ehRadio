#include "../core/options.h"
#if DSP_MODEL==DSP_ST7789
#include "dspcore.h"
#include "../core/config.h"

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        40000000UL      /*  set it to 0 for system default */
#endif

DspCore::DspCore(): Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST) {}

void DspCore::initDisplay() {
  init(DSP_HEIGHT, DSP_WIDTH);
  if(DEF_SPI_FREQ > 0) setSPISpeed(DEF_SPI_FREQ);
  invert();
  cp437(true);
  flip();
  setTextWrap(false);
  setTextSize(1);
  fillScreen(0x0000);
}

void DspCore::clearDsp(bool black){ fillScreen(black?0:config.theme.background); }
void DspCore::flip(){
#if DSP_WIDTH==DSP_HEIGHT
  if(ROTATE_90){
    setRotation(config.store.flipscreen?3:1);
  }else{
    setRotation(config.store.flipscreen?2:0);
  }
#else
  setRotation(config.store.flipscreen?3:1);
#endif
}
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ enableSleep(true); delay(150); enableDisplay(false); delay(150); }
void DspCore::wake(void){ enableDisplay(true); delay(150); enableSleep(false); delay(150); }

#endif
