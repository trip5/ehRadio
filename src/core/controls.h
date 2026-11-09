#ifndef controls_h
#define controls_h
#include "common.h"

#if IR_PIN!=255
  // IR button BEHAVIOUR ids. These are not storage indexes: IR codes live in the named fields of Config::irstore and
  // are selected by name ("power", "n7", ...) through the irKeyMap table in config.cpp.
  enum irAction_e : uint8_t {
    IRACT_POWER=0, IRACT_MUTE=1, IRACT_UP=2, IRACT_DOWN=3, IRACT_PREV=4,
    IRACT_NEXT=5, IRACT_PLAY=6, IRACT_MODE=7, IRACT_HASH=8, IRACT_DIGIT=9
  };
#endif

#if (ENC_DT!=255 && ENC_CLK!=255) || (ENC2_DT!=255 && ENC2_CLK!=255)
  class AiEsp32RotaryEncoder;
#endif

class Controls {
public:
  void init();
  void loop();
  void setEncAcceleration(uint8_t acc);
  void setIRTolerance(uint8_t tl);
  void flipTS();
  void controlsEvent(bool toRight, int8_t volDelta = 0);
  void onBtnClick(int id);
  void checkButtonsHeldOnBoot();  // bare GPIO check before controls.init() — hold MODE/ENC_SW to force SD mode
  bool screenSaverExit();
private:
  int lpId = -1;
  unsigned long lpDelay = 0;
#if IR_PIN!=255
  uint8_t irVolRepeat = 0;
#endif
#if (ENC_DT!=255 && ENC_CLK!=255) || (ENC2_DT!=255 && ENC2_CLK!=255)
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
