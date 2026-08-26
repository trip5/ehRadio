#include "../core/options.h"
#if DSP_MODEL==DSP_ILI9225
#include "dspcore.h"
#include <SPI.h>
#include "../core/config.h"

DspCore::DspCore(): Adafruit_ILI9225(TFT_CS, TFT_DC, TFT_RST) {}

void DspCore::initDisplay() {
  begin();
  invert();
  cp437(true);
  flip();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black){ fillScreen(black?0:config.theme.background); }
void DspCore::flip(){ setRotation(config.store.flipscreen?1:3); }
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ setDisplay(false); }
void DspCore::wake(void){ setDisplay(true); }

#endif

