#include "Adafruit_NV3007.h"

#include <initializer_list>

// NV3007 (New Vision) TFT controller driver.
// The NV3007 shares the ST77xx SPI transport and the standard MIPI-DBI basics
// (0x11 sleep-out, 0x29 display-on, 0x3A COLMOD, 0x36 MADCTL) but needs a
// vendor-specific init sequence and a 12-pixel column offset. This class keeps
// the ST7789-family plumbing (Adafruit_ST77xx base) while owning the NV3007
// init sequence and rotation mapping directly, rather than patching the
// generic ST7789 driver with 142x428 special cases.
// Register sequence derived from kc-dev's yoRadio_kc-dev port (Krzysztof
// Cielma, 12.06.26).

// CONSTRUCTORS ************************************************************

Adafruit_NV3007::Adafruit_NV3007(int8_t cs, int8_t dc, int8_t mosi,
                                 int8_t sclk, int8_t rst)
    : Adafruit_ST77xx(142, 428, cs, dc, mosi, sclk, rst) {}

Adafruit_NV3007::Adafruit_NV3007(int8_t cs, int8_t dc, int8_t rst)
    : Adafruit_ST77xx(142, 428, cs, dc, rst) {}

#if !defined(ESP8266)
Adafruit_NV3007::Adafruit_NV3007(SPIClass *spiClass, int8_t cs, int8_t dc,
                                 int8_t rst)
    : Adafruit_ST77xx(142, 428, spiClass, cs, dc, rst) {}
#endif // end !ESP8266

// SCREEN INITIALIZATION ***************************************************

void Adafruit_NV3007::init(uint16_t width, uint16_t height, uint8_t mode) {
  // Save SPI data mode; commonInit() calls begin() -> initSPI() and pulses
  // the hardware reset pin when one is configured.
  spiMode = mode;
  commonInit(NULL);

  // NV3007 2.79" panel geometry: 12-pixel column offset, no row offset.
  _rowstart = _rowstart2 = 0;
  _colstart = _colstart2 = 12;
  windowWidth = width;
  windowHeight = height;

  // Vendor register sequence (unlock -> vendor registers -> lock).
  // Helper sends a command with an optional brace-enclosed data list.
  auto nvCmd = [this](uint8_t cmd, std::initializer_list<uint8_t> data = {}) {
    if (data.size() == 0) {
      sendCommand(cmd);
    } else {
      uint8_t buf[8];
      uint8_t i = 0;
      for (uint8_t b : data)
        buf[i++] = b;
      sendCommand(cmd, buf, i);
    }
  };

  sendCommand(ST77XX_SWRESET);
  delay(150);

  nvCmd(0xFF, {0xA5}); // unlock vendor command page

  nvCmd(0x9A, {0x08});
  nvCmd(0x9B, {0x08});
  nvCmd(0x9C, {0xB0});
  nvCmd(0x9D, {0x16});
  nvCmd(0x9E, {0xC4});
  nvCmd(0x8F, {0x55, 0x04});
  nvCmd(0x84, {0x90});
  nvCmd(0x83, {0x7B});
  nvCmd(0x85, {0x33});

  nvCmd(0x60, {0x00});
  nvCmd(0x70, {0x00});
  nvCmd(0x61, {0x02});
  nvCmd(0x71, {0x02});
  nvCmd(0x62, {0x04});
  nvCmd(0x72, {0x04});
  nvCmd(0x6C, {0x29});
  nvCmd(0x7C, {0x29});
  nvCmd(0x6D, {0x31});
  nvCmd(0x7D, {0x31});
  nvCmd(0x6E, {0x0F});
  nvCmd(0x7E, {0x0F});
  nvCmd(0x66, {0x21});
  nvCmd(0x76, {0x21});
  nvCmd(0x68, {0x3A});
  nvCmd(0x78, {0x3A});
  nvCmd(0x63, {0x07});
  nvCmd(0x73, {0x07});
  nvCmd(0x64, {0x05});
  nvCmd(0x74, {0x05});
  nvCmd(0x65, {0x02});
  nvCmd(0x75, {0x02});
  nvCmd(0x67, {0x23});
  nvCmd(0x77, {0x23});
  nvCmd(0x69, {0x08});
  nvCmd(0x79, {0x08});
  nvCmd(0x6A, {0x13});
  nvCmd(0x7A, {0x13});
  nvCmd(0x6B, {0x13});
  nvCmd(0x7B, {0x13});
  nvCmd(0x6F, {0x00});
  nvCmd(0x7F, {0x00});

  nvCmd(0x50, {0x00});
  nvCmd(0x52, {0xD6});
  nvCmd(0x53, {0x08});
  nvCmd(0x54, {0x08});
  nvCmd(0x55, {0x1E});
  nvCmd(0x56, {0x1C});

  nvCmd(0xA0, {0x2B, 0x24, 0x00});
  nvCmd(0xA1, {0x87});
  nvCmd(0xA2, {0x86});

  nvCmd(0xA5, {0x00});
  nvCmd(0xA6, {0x00});
  nvCmd(0xA7, {0x00});

  nvCmd(0xA8, {0x36});
  nvCmd(0xA9, {0x7E});
  nvCmd(0xAA, {0x7E});

  nvCmd(0xB9, {0x85});
  nvCmd(0xBA, {0x84});
  nvCmd(0xBB, {0x83});
  nvCmd(0xBC, {0x82});
  nvCmd(0xBD, {0x81});
  nvCmd(0xBE, {0x80});
  nvCmd(0xBF, {0x01});
  nvCmd(0xC0, {0x02});

  nvCmd(0xC1, {0x00});
  nvCmd(0xC2, {0x00});
  nvCmd(0xC3, {0x00});

  nvCmd(0xC4, {0x33});
  nvCmd(0xC5, {0x7E});
  nvCmd(0xC6, {0x7E});

  nvCmd(0xC8, {0x33, 0x33});

  nvCmd(0xC9, {0x68});
  nvCmd(0xCA, {0x69});
  nvCmd(0xCB, {0x6A});
  nvCmd(0xCC, {0x6B});

  nvCmd(0xCD, {0x33, 0x33});

  nvCmd(0xCE, {0x6C});
  nvCmd(0xCF, {0x6D});
  nvCmd(0xD0, {0x6E});
  nvCmd(0xD1, {0x6F});

  nvCmd(0xAB, {0x03, 0x67});
  nvCmd(0xAC, {0x03, 0x6B});
  nvCmd(0xAD, {0x03, 0x68});
  nvCmd(0xAE, {0x03, 0x6C});

  nvCmd(0xB3, {0x00});
  nvCmd(0xB4, {0x00});
  nvCmd(0xB5, {0x00});

  nvCmd(0xB6, {0x32});
  nvCmd(0xB7, {0x7E});
  nvCmd(0xB8, {0x7E});

  nvCmd(0xE0, {0x00});
  nvCmd(0xE1, {0x03, 0x0F});
  nvCmd(0xE2, {0x04});
  nvCmd(0xE3, {0x01});
  nvCmd(0xE4, {0x0E});
  nvCmd(0xE5, {0x01});
  nvCmd(0xE6, {0x19});
  nvCmd(0xE7, {0x10});
  nvCmd(0xE8, {0x10});
  nvCmd(0xEA, {0x12});
  nvCmd(0xEB, {0xD0});
  nvCmd(0xEC, {0x04});
  nvCmd(0xED, {0x07});
  nvCmd(0xEE, {0x07});
  nvCmd(0xEF, {0x09});
  nvCmd(0xF0, {0xD0});
  nvCmd(0xF1, {0x0E});

  nvCmd(0xF9, {0x17});

  nvCmd(0xF2, {0x2C, 0x1B, 0x0B, 0x20});

  nvCmd(0xE9, {0x29});
  nvCmd(0xEC, {0x04});

  nvCmd(0x35, {0x00});
  nvCmd(0x44, {0x00, 0x10});
  nvCmd(0x46, {0x10});

  nvCmd(0x3A, {0x05}); // COLMOD: 16-bit color

  nvCmd(0xFF, {0x00}); // lock vendor command page

  sendCommand(ST77XX_SLPOUT);
  delay(120);
  sendCommand(ST77XX_DISPON);
  delay(20);

  setRotation(0);
}

// SCREEN ORIENTATION ******************************************************

void Adafruit_NV3007::setRotation(uint8_t m) {
  uint8_t madctl = 0;

  rotation = m & 3; // can't be higher than 3

  switch (rotation) {
  case 0:
    madctl = ST77XX_MADCTL_RGB;
    _xstart = 12;
    _ystart = 0;
    _width = windowWidth;   // 142
    _height = windowHeight; // 428
    break;

  case 1:
    madctl = ST77XX_MADCTL_MY | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    _xstart = 0;
    _ystart = 12;
    _width = windowHeight; // 428
    _height = windowWidth; // 142
    break;

  case 2:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MY | ST77XX_MADCTL_RGB;
    _xstart = 12;
    _ystart = 0;
    _width = windowWidth;   // 142
    _height = windowHeight; // 428
    break;

  case 3:
    madctl = ST77XX_MADCTL_MX | ST77XX_MADCTL_MV | ST77XX_MADCTL_RGB;
    _xstart = 0;
    // NOTE (kc-dev): the original code uses _ystart = 14 here while the
    // symmetric case 1 uses _ystart = 12. This 2px asymmetry may be a typo
    // or a deliberate correction for the 270° orientation. If the flipped
    // landscape is vertically shifted by ~2 pixels, try changing 14 -> 12
    // to match case 1.
    _ystart = 14;
    _width = windowHeight; // 428
    _height = windowWidth; // 142
    break;
  }

  sendCommand(ST77XX_MADCTL, &madctl, 1);
}
