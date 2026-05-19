#include "../core/options.h"
#if DSP_MODEL==DSP_ILI9488 || DSP_MODEL==DSP_ILI9486
#include "dspcore.h"
#include "../core/config.h"

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        40000000UL      /*  set it to 0 for system default */
#endif

  DspCore::DspCore(): ILI9486_SPI(TFT_CS, TFT_DC, TFT_RST) {}

void DspCore::initDisplay() {
  setSpiKludge(false);
  init();
  // if(DEF_SPI_FREQ > 0) setSPISpeed(DEF_SPI_FREQ); // set by driver and overridden by DEF_SPI_FREQ directly
  cp437(true);
  setTextWrap(false);
  setTextSize(1);
  fillScreen(0x0000);
  invert();
  flip();
}

void DspCore::clearDsp(bool black){ fillScreen(black?0:config.theme.background); }
void DspCore::flip(){ setRotation(config.store.flipscreen?3:1); }
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ sendCommand(ILI9488_SLPIN); delay(150); sendCommand(ILI9488_DISPOFF); delay(150); }
void DspCore::wake(void){ sendCommand(ILI9488_DISPON); delay(150); sendCommand(ILI9488_SLPOUT); delay(150); }

#endif
