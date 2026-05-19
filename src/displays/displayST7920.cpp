#include "../core/options.h"
#if DSP_MODEL==DSP_ST7920
#include "dspcore.h"
#include "../core/config.h"

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        8000000UL
#endif

  DspCore::DspCore(): ST7920(&SPI, TFT_CS, DEF_SPI_FREQ) {}

void DspCore::initDisplay() {
#include "tools/oledcolorfix.h"
  begin();
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black){ fillScreen(TFT_BG); }
void DspCore::flip(){ setRotation(config.store.flipscreen?2:0); }
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ doSleep(true); }
void DspCore::wake(void){ doSleep(false); }

#endif
