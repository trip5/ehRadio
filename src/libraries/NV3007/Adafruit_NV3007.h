#ifndef _ADAFRUIT_NV3007H_
#define _ADAFRUIT_NV3007H_

#include "Adafruit_ST77xx.h"

/// Subclass of ST77XX for the NV3007 TFT controller
/// (2.79" 142x428 bar display), a close relative of the ST7789 that shares the
/// ST77xx SPI transport and most MIPI-DBI basics (0x11/0x29/0x3A/0x36) but
/// requires a vendor-specific init sequence and a 12-pixel column offset.
/// Derived from kc-dev's yoRadio_kc-dev ST7789 port (Krzysztof Cielma).
class Adafruit_NV3007 : public Adafruit_ST77xx {
public:
  Adafruit_NV3007(int8_t cs, int8_t dc, int8_t mosi, int8_t sclk,
                  int8_t rst = -1);
  Adafruit_NV3007(int8_t cs, int8_t dc, int8_t rst);
#if !defined(ESP8266)
  Adafruit_NV3007(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst);
#endif // end !ESP8266

  void setRotation(uint8_t m);
  void init(uint16_t width = 142, uint16_t height = 428,
            uint8_t spiMode = SPI_MODE0);

protected:
  uint8_t _colstart2 = 0, ///< Offset from the right
      _rowstart2 = 0;     ///< Offset from the bottom

private:
  uint16_t windowWidth;
  uint16_t windowHeight;
};

#endif // _ADAFRUIT_NV3007H_
