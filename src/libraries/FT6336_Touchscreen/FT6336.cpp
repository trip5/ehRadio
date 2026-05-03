#include "FT6336.h"
#include "../../core/logging.h"

FT_Point::FT_Point(void){
  id = 0; x = 0; y = 0; size = 0;
}
FT_Point::FT_Point(uint8_t _id, uint16_t _x, uint16_t _y, uint8_t _size){
  id = _id; x = _x; y = _y; size = _size;
}
bool FT_Point::operator==(FT_Point p){
  return id==p.id && x==p.x && y==p.y;
}
bool FT_Point::operator!=(FT_Point p){
  return !(*this==p);
}

FT6336::FT6336(uint8_t _sda, uint8_t _scl, uint8_t _int, uint8_t _rst, uint16_t _width, uint16_t _height){
  pinSda = _sda; pinScl = _scl; pinInt = _int; pinRst = _rst;
  width = _width; height = _height;
}

void FT6336::begin(){
  if(pinSda != 255 && pinScl != 255) {
    Wire.begin(pinSda, pinScl);
  } else {
    Wire.begin();
  }
  FUNCTIONLOG("FT6336", "begin SDA=%d SCL=%d INT=%d RST=%d addr=0x%02X", pinSda, pinScl, pinInt, pinRst, addr);
  // Basic probe
  Wire.beginTransmission(addr);
  if (Wire.endTransmission() != 0) {
    // no device
  }
  // No fancy init here; driver is simple
}

void FT6336::reset(){
  if (pinRst!=255) {
    pinMode(pinRst, OUTPUT);
    digitalWrite(pinRst, LOW);
    delay(10);
    digitalWrite(pinRst, HIGH);
    delay(50);
  }
}

void FT6336::setRotation(uint8_t rot){
  rotation = rot;
}

void FT6336::setResolution(uint16_t _width, uint16_t _height){
  width = _width; height = _height;
}

void FT6336::read(void){
  // Minimal read for single touch (FT6236/FT6336 typical registers)
  // 0x02 is the number of touch points
  Wire.beginTransmission(addr);
  Wire.write(0x02);
  if (Wire.endTransmission() != 0) {
    touches = 0; isTouched = false; return;
  }
  Wire.requestFrom((int)addr, 1);
  if (!Wire.available()) { touches = 0; isTouched = false; return; }
  uint8_t n = Wire.read();
  if (n==0){
    touches = 0; isTouched = false; return;
  }
  touches = n;
  // read first touch data at 0x03..0x06
  Wire.beginTransmission(addr);
  Wire.write(0x03);
  if (Wire.endTransmission() != 0) { touches = 0; isTouched = false; return; }
  Wire.requestFrom((int)addr, 4);
  if (Wire.available() < 4) { touches = 0; isTouched = false; return; }
  uint8_t buf[4];
  for (int i=0;i<4;i++) buf[i] = Wire.read();
  // decode: x = ((buf[0] & 0x0F) << 8) | buf[1]; y mirrored in buf[2]/buf[3]
  uint16_t x = ((buf[0] & 0x0F) << 8) | buf[1];
  uint16_t y = ((buf[2] & 0x0F) << 8) | buf[3];
  uint16_t tmp;
  // Apply rotation mapping similar to GT911: 0=Left,1=Inverted,2=Right,3=Normal
  switch(rotation){
    case 0: // ROTATION_LEFT
      tmp = x; x = width - y; y = tmp; break;
    case 1: // ROTATION_INVERTED
      // keep as-is
      break;
    case 2: // ROTATION_RIGHT
      tmp = x; x = y; y = height - tmp; break;
    case 3: // ROTATION_NORMAL
      x = width - x; y = height - y; break;
    default:
      break;
  }
  points[0].id = 1; points[0].x = x; points[0].y = y; points[0].size = 0;
  isTouched = true;
}
