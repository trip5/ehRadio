#include "../core/options.h"
#if DSP_MODEL==DSP_SSD1305
#include "dspcore.h"
#include "../core/config.h"
#include "../core/logging.h"

#ifndef SCREEN_ADDRESS
  #define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32 or scan it https://create.arduino.cc/projecthub/abdularbi17/how-to-scan-i2c-address-in-arduino-eaadda
#endif

#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        8000000UL      /*  set it to 0 for system default */
#endif

// Auto-detect interface from pins: I2C if both I2C pins defined, else SPI
#if I2C_SDA!=255 && I2C_SCL!=255
#include <Wire.h>
TwoWire I2CSSD1305 = TwoWire(0);
DspCore::DspCore(): Adafruit_SSD1305(DSP_WIDTH, DSP_HEIGHT, &I2CSSD1305, -1){

}
#else
  DspCore::DspCore(): Adafruit_SSD1305(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {}
#endif

void DspCore::initDisplay() {
#if I2C_SDA!=255 && I2C_SCL!=255
  I2CSSD1305.begin(I2C_SDA, I2C_SCL);
#endif
  if (!begin(SCREEN_ADDRESS)) {
    ERRORLOG("SSD1305 allocation failed");
    for (;;); // Don't proceed, loop forever
  }
#include "tools/oledcolorfix.h"
  
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
}

void DspCore::clearDsp(bool black){ fillScreen(TFT_BG); }
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
void DspCore::sleep(void){ oled_command(SSD1305_DISPLAYOFF); }
void DspCore::wake(void){ oled_command(SSD1305_DISPLAYON); }

#endif
