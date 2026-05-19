#ifndef controls_h
#define controls_h
#include "common.h"

#if IR_PIN!=255
  enum : uint8_t { IR_UP=0, IR_PREV=1, IR_PLAY=2, IR_NEXT=3, IR_DOWN=4, IR_1=5, IR_2=6, IR_3=7, IR_4=8, IR_5=9, IR_6=10, IR_7=11, IR_8=12, IR_9=13, IR_AST=14, IR_0=15, IR_HASH=16 };
#endif

#if (ENC_BTNL!=255 && ENC_BTNR!=255) || (ENC2_BTNL!=255 && ENC2_BTNR!=255)
  class AiEsp32RotaryEncoder;
#endif

class Controls {
public:
  void init();
  void loop();
  void setEncAcceleration(uint16_t acc);
  void setIRTolerance(uint8_t tl);
  void flipTS();
  void controlsEvent(bool toRight, int8_t volDelta = 0);
  void onBtnClick(int id);
private:
  int lpId = -1;
  unsigned long lpDelay = 0;
#if IR_PIN!=255
  uint8_t irVolRepeat = 0;
#endif
#if (ENC_BTNL!=255 && ENC_BTNR!=255) || (ENC2_BTNL!=255 && ENC2_BTNR!=255)
  void encodersLoop(AiEsp32RotaryEncoder *enc, bool first = true);
#endif
  void encoder1Loop();
  void encoder2Loop();
  void irBlink();
  void irNumber(uint8_t num);
  void irLoop();
  void onBtnLongPressStart(int id);
  void onBtnLongPressStop(int id);
  boolean checklpdelay(int m, unsigned long &tstamp);
  void onBtnDuringLongPress(int id);
  void onBtnDoubleClick(int id);
  static void btnClickCb(void* p);
  static void btnDoubleClickCb(void* p);
  static void btnLongPressStartCb(void* p);
  static void btnLongPressStopCb(void* p);
};

extern Controls controls;

#endif
