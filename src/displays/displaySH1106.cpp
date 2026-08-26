#include "../core/options.h"
#if DSP_MODEL==DSP_SH1106 || DSP_MODEL==DSP_SH1107
#include "dspcore.h"
#include "../core/config.h"
#include "../core/logging.h"

#ifndef SCREEN_ADDRESS
  #define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32 or scan it https://create.arduino.cc/projecthub/abdularbi17/how-to-scan-i2c-address-in-arduino-eaadda
#endif

// Auto-detect interface from pins
#if I2C_SDA!=255 && I2C_SCL!=255
#include <Wire.h>

#ifndef I2CFREQ_HZ
  #define I2CFREQ_HZ   4000000UL
#endif

TwoWire I2CSH1106 = TwoWire(0);
#if DSP_MODEL==DSP_SH1106
DspCore::DspCore(): Adafruit_SH1106G(DSP_WIDTH, DSP_HEIGHT, &I2CSH1106, -1, I2CFREQ_HZ) {

}
#else
DspCore::DspCore(): Adafruit_SH1107(DSP_WIDTH, DSP_HEIGHT, &I2CSH1106, -1) {

}
#endif

void DspCore::initDisplay() {
  I2CSH1106.begin(I2C_SDA, I2C_SCL);
  if (!begin(SCREEN_ADDRESS, true)) {
    ERRORLOG("SH110X allocation failed");
    for (;;); // Don't proceed, loop forever
  }
#else
#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        8000000UL      /*  set it to 0 for system default */
#endif

#if DSP_MODEL==DSP_SH1106
DspCore::DspCore(): Adafruit_SH1106G(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {

}
#else
DspCore::DspCore(): Adafruit_SH1107(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {

}
#endif

void DspCore::initDisplay() {
  if (!begin(SCREEN_ADDRESS, true)) {
    ERRORLOG("SH110X allocation failed");
    for (;;);
  }
#endif
#include "tools/oledcolorfix.h"
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black){ fillScreen(TFT_BG); }
void DspCore::flip(){
#if DSP_MODEL==DSP_SH1107
  #if DSP_WIDTH==DSP_HEIGHT
    if(ROTATE_90){
      setRotation(config.store.flipscreen?0:2);
    }else{
      setRotation(config.store.flipscreen?3:1);
    }
  #else
    setRotation(config.store.flipscreen?3:1);
  #endif
#endif
#if DSP_MODEL==DSP_SH1106
  #if DSP_WIDTH==DSP_HEIGHT
    if(ROTATE_90){
      setRotation(config.store.flipscreen?3:1);
    }else{
      setRotation(config.store.flipscreen?2:0);
    }
  #else
    setRotation(config.store.flipscreen?2:0);
  #endif
#endif
}
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ oled_command(SH110X_DISPLAYOFF); }
void DspCore::wake(void){ oled_command(SH110X_DISPLAYON); }

#endif
