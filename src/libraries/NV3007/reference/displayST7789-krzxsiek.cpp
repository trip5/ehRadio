//v0.9.720
#include "../core/options.h"
#if DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7789_240 || DSP_MODEL==DSP_ST7789_76 || DSP_MODEL==DSP_ST7789_170 || DSP_MODEL==DSP_NV3007_142
#include "dspcore.h"
#include "../core/config.h"

#if DSP_MODEL==DSP_NV3007_142

static void sendCmd(Adafruit_ST7789* tft, uint8_t cmd,
                    std::initializer_list<uint8_t> data = {}) {
    if (data.size() == 0) {
        tft->sendCommand(cmd);
    } else {
        uint8_t buf[8];
        uint8_t i = 0;
        for (auto b : data) buf[i++] = b;
        tft->sendCommand(cmd, buf, i);
    }
}

void initNV3007(Adafruit_ST7789* tft) {

    sendCmd(tft, 0xFF, {0xA5});

    sendCmd(tft, 0x9A, {0x08});
    sendCmd(tft, 0x9B, {0x08});
    sendCmd(tft, 0x9C, {0xB0});
    sendCmd(tft, 0x9D, {0x16});
    sendCmd(tft, 0x9E, {0xC4});
    sendCmd(tft, 0x8F, {0x55,0x04});
    sendCmd(tft, 0x84, {0x90});
    sendCmd(tft, 0x83, {0x7B});
    sendCmd(tft, 0x85, {0x33});

    sendCmd(tft, 0x60, {0x00});
    sendCmd(tft, 0x70, {0x00});
    sendCmd(tft, 0x61, {0x02});
    sendCmd(tft, 0x71, {0x02});
    sendCmd(tft, 0x62, {0x04});
    sendCmd(tft, 0x72, {0x04});
    sendCmd(tft, 0x6C, {0x29});
    sendCmd(tft, 0x7C, {0x29});
    sendCmd(tft, 0x6D, {0x31});
    sendCmd(tft, 0x7D, {0x31});
    sendCmd(tft, 0x6E, {0x0F});
    sendCmd(tft, 0x7E, {0x0F});
    sendCmd(tft, 0x66, {0x21});
    sendCmd(tft, 0x76, {0x21});
    sendCmd(tft, 0x68, {0x3A});
    sendCmd(tft, 0x78, {0x3A});
    sendCmd(tft, 0x63, {0x07});
    sendCmd(tft, 0x73, {0x07});
    sendCmd(tft, 0x64, {0x05});
    sendCmd(tft, 0x74, {0x05});
    sendCmd(tft, 0x65, {0x02});
    sendCmd(tft, 0x75, {0x02});
    sendCmd(tft, 0x67, {0x23});
    sendCmd(tft, 0x77, {0x23});
    sendCmd(tft, 0x69, {0x08});
    sendCmd(tft, 0x79, {0x08});
    sendCmd(tft, 0x6A, {0x13});
    sendCmd(tft, 0x7A, {0x13});
    sendCmd(tft, 0x6B, {0x13});
    sendCmd(tft, 0x7B, {0x13});
    sendCmd(tft, 0x6F, {0x00});
    sendCmd(tft, 0x7F, {0x00});

    sendCmd(tft, 0x50, {0x00});
    sendCmd(tft, 0x52, {0xD6});
    sendCmd(tft, 0x53, {0x08});
    sendCmd(tft, 0x54, {0x08});
    sendCmd(tft, 0x55, {0x1E});
    sendCmd(tft, 0x56, {0x1C});

    sendCmd(tft, 0xA0, {0x2B,0x24,0x00});
    sendCmd(tft, 0xA1, {0x87});
    sendCmd(tft, 0xA2, {0x86});

    sendCmd(tft, 0xA5, {0x00});
    sendCmd(tft, 0xA6, {0x00});
    sendCmd(tft, 0xA7, {0x00});

    sendCmd(tft, 0xA8, {0x36});
    sendCmd(tft, 0xA9, {0x7E});
    sendCmd(tft, 0xAA, {0x7E});

    sendCmd(tft, 0xB9, {0x85});
    sendCmd(tft, 0xBA, {0x84});
    sendCmd(tft, 0xBB, {0x83});
    sendCmd(tft, 0xBC, {0x82});
    sendCmd(tft, 0xBD, {0x81});
    sendCmd(tft, 0xBE, {0x80});
    sendCmd(tft, 0xBF, {0x01});
    sendCmd(tft, 0xC0, {0x02});

    sendCmd(tft, 0xC1, {0x00});
    sendCmd(tft, 0xC2, {0x00});
    sendCmd(tft, 0xC3, {0x00});

    sendCmd(tft, 0xC4, {0x33});
    sendCmd(tft, 0xC5, {0x7E});
    sendCmd(tft, 0xC6, {0x7E});

    sendCmd(tft, 0xC8, {0x33,0x33});

    sendCmd(tft, 0xC9, {0x68});
    sendCmd(tft, 0xCA, {0x69});
    sendCmd(tft, 0xCB, {0x6A});
    sendCmd(tft, 0xCC, {0x6B});

    sendCmd(tft, 0xCD, {0x33,0x33});

    sendCmd(tft, 0xCE, {0x6C});
    sendCmd(tft, 0xCF, {0x6D});
    sendCmd(tft, 0xD0, {0x6E});
    sendCmd(tft, 0xD1, {0x6F});

    sendCmd(tft, 0xAB, {0x03,0x67});
    sendCmd(tft, 0xAC, {0x03,0x6B});
    sendCmd(tft, 0xAD, {0x03,0x68});
    sendCmd(tft, 0xAE, {0x03,0x6C});

    sendCmd(tft, 0xB3, {0x00});
    sendCmd(tft, 0xB4, {0x00});
    sendCmd(tft, 0xB5, {0x00});

    sendCmd(tft, 0xB6, {0x32});
    sendCmd(tft, 0xB7, {0x7E});
    sendCmd(tft, 0xB8, {0x7E});

    sendCmd(tft, 0xE0, {0x00});
    sendCmd(tft, 0xE1, {0x03,0x0F});
    sendCmd(tft, 0xE2, {0x04});
    sendCmd(tft, 0xE3, {0x01});
    sendCmd(tft, 0xE4, {0x0E});
    sendCmd(tft, 0xE5, {0x01});
    sendCmd(tft, 0xE6, {0x19});
    sendCmd(tft, 0xE7, {0x10});
    sendCmd(tft, 0xE8, {0x10});
    sendCmd(tft, 0xEA, {0x12});
    sendCmd(tft, 0xEB, {0xD0});
    sendCmd(tft, 0xEC, {0x04});
    sendCmd(tft, 0xED, {0x07});
    sendCmd(tft, 0xEE, {0x07});
    sendCmd(tft, 0xEF, {0x09});
    sendCmd(tft, 0xF0, {0xD0});
    sendCmd(tft, 0xF1, {0x0E});

    sendCmd(tft, 0xF9, {0x17});

    sendCmd(tft, 0xF2, {0x2C,0x1B,0x0B,0x20});

    sendCmd(tft, 0xE9, {0x29});
    sendCmd(tft, 0xEC, {0x04});

    sendCmd(tft, 0x35, {0x00});
    sendCmd(tft, 0x44, {0x00,0x10});
    sendCmd(tft, 0x46, {0x10});

    sendCmd(tft, 0x3A, {0x05});

    sendCmd(tft, 0xFF, {0x00});

    sendCmd(tft, 0x11);
    delay(120);

    sendCmd(tft, 0x29);
    delay(20);
}

#endif

#if DSP_HSPI
DspCore::DspCore(): Adafruit_ST7789(&SPI2, TFT_CS, TFT_DC, TFT_RST) {}
#else
DspCore::DspCore(): Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST) {}
#endif

void DspCore::initDisplay() {

  #if DSP_MODEL==DSP_NV3007_142
  init(142,428);
  initNV3007(this);
  #else
    if(DSP_MODEL==DSP_ST7789_76){
      init(76,284);
    }else if(DSP_MODEL==DSP_ST7789_170){
      init(170,320);
    }else{
      init(240,(DSP_MODEL==DSP_ST7789)?320:240);
    }
  #endif

  invert();
  cp437(true);
  flip();
  setTextWrap(false);
  setTextSize(1);
  fillScreen(0x0000);
}

void DspCore::clearDsp(bool black){ fillScreen(black?0:config.theme.background); }
void DspCore::flip(){
#if DSP_MODEL==DSP_ST7789 || DSP_MODEL==DSP_ST7789_76 || DSP_MODEL==DSP_ST7789_170 || DSP_MODEL==DSP_NV3007_142
  setRotation(config.store.flipscreen?3:1);
#endif
#if DSP_MODEL==DSP_ST7789_240
  if(ROTATE_90){
    setRotation(config.store.flipscreen?3:1);
  }else{
    setRotation(config.store.flipscreen?2:0);
  }
#endif
}
void DspCore::invert(){ invertDisplay(
#if DSP_MODEL==DSP_ST7789_170
!
#endif
config.store.invertdisplay); }
void DspCore::sleep(void){ enableSleep(true); delay(150); enableDisplay(false); delay(150); }
void DspCore::wake(void){ enableDisplay(true); delay(150); enableSleep(false); delay(150); }

#endif