#include "../core/options.h"
#if DSP_MODEL==DSP_SSD1327
#include "dspcore.h"
#include "../core/config.h"
#include "../core/logging.h"

#ifndef SCREEN_ADDRESS
  #define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; or scan it https://create.arduino.cc/projecthub/abdularbi17/how-to-scan-i2c-address-in-arduino-eaadda
#endif

// Auto-detect interface from pins
#if I2C_SDA!=255 && I2C_SCL!=255
#include <Wire.h>

#ifndef I2CFREQ_HZ
  #define I2CFREQ_HZ   6000000UL
#endif

TwoWire tw = TwoWire(0);

DspCore::DspCore(): Adafruit_SSD1327(DSP_WIDTH, DSP_HEIGHT, &tw, I2C_RST/*, I2CFREQ_HZ*/) {}

void DspCore::initDisplay() {
  tw.begin(I2C_SDA, I2C_SCL);
  if (!begin(SCREEN_ADDRESS)) {
    ERRORLOG("SSD1327 allocation failed");
    for (;;);
  }
#else
#ifndef DEF_SPI_FREQ
  #define DEF_SPI_FREQ        8000000UL      /*  set it to 0 for system default */
#endif

DspCore::DspCore(): Adafruit_SSD1327(DSP_WIDTH, DSP_HEIGHT, &SPI, TFT_DC, TFT_RST, TFT_CS, DEF_SPI_FREQ) {}

void DspCore::initDisplay() {
  if (!begin(SCREEN_ADDRESS)) {
    ERRORLOG("SSD1327 allocation failed");
    for (;;);
  }
#endif

  // --- Maximum brightness settings ---
  oled_command(0xB8);  // Set Gray Scale Table
  for (uint8_t i = 0; i < 16; i++) {
    oled_command(0x7F);  // 127 = max brightness for all 16 gray levels
  }
  oled_command(0x81);  // Set Contrast Current
  oled_command(0xFF);  // 255 = maximum
  oled_command(0xDB);  // Set VCOMH Deselect Level
  oled_command(0x40);  // 64 = maximum
  // -----------------------------------

  #include "tools/oledcolorfix.h"
  cp437(true);
  flip();
  invert();
  setTextWrap(false);
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
  setRotation(config.store.flipscreen?2:0);
#endif
}
void DspCore::invert(){ invertDisplay(config.displayIsInverted != DSP_INVERT_QUIRK); }
void DspCore::sleep(void){ oled_command(SSD1327_DISPLAYOFF); }
void DspCore::wake(void){ oled_command(SSD1327_DISPLAYON); }

#endif