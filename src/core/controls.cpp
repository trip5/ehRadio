#include "options.h"
#include <Arduino.h>
#include "config.h"
#include "backlightcontrols.h"
#include "controls.h"
#include "display.h"
#include "logging.h"
#include "netserver.h"
#include "network.h"
#include <SD.h>
#include "sdmanager.h"

#if IR_PIN!=255
  #include <assert.h>
  #include <IRrecv.h>
  #include <IRremoteESP8266.h>
  #include <IRac.h>
  #include <IRtext.h>
  #include <IRutils.h>
  const uint16_t kCaptureBufferSize = 1024;
  const uint8_t kTimeout = IR_TIMEOUT;
  const uint16_t kMinUnknownSize = 12;
  #define LEGACY_TIMING_INFO false
  IRrecv irrecv(IR_PIN, kCaptureBufferSize, kTimeout, true);
  decode_results irResults;
#endif

#include "player.h"
#include "utility.h"


#define ISPUSHBUTTONS BTN_DOWN!=255 || BTN_PLAY!=255 || BTN_UP!=255 || ENC_SW!=255 || BTN_PREV!=255 || BTN_NEXT!=255 || ENC2_SW!=255 || BTN_MODE!=255
#if ISPUSHBUTTONS
  #include <OneButton.h>
  OneButton button[] {OneButton(BTN_DOWN, true, BTN_DOWN_PULLUP), OneButton(BTN_PLAY, true, BTN_PLAY_PULLUP), OneButton(BTN_UP, true, BTN_UP_PULLUP), OneButton(ENC_SW, true, ENC_SW_PULLUP), OneButton(BTN_PREV, true, BTN_PREV_PULLUP), OneButton(BTN_NEXT, true, BTN_NEXT_PULLUP), OneButton(ENC2_SW, true, ENC2_SW_PULLUP), OneButton(BTN_MODE, true, BTN_MODE_PULLUP)};
  constexpr uint8_t nrOfButtons = sizeof(button) / sizeof(button[0]);
#endif

#if (ENC_DT!=255 && ENC_CLK!=255) || (ENC2_DT!=255 && ENC2_CLK!=255)
  #include <AiEsp32RotaryEncoder.h>
  #if (ENC_DT!=255 && ENC_CLK!=255)
    AiEsp32RotaryEncoder encoder = AiEsp32RotaryEncoder(ENC_DT, ENC_CLK, ENC_SW, -1, ENC_STEPS, !ENC_PULLUP);
  #endif
  #if (ENC2_DT!=255 && ENC2_CLK!=255)
    AiEsp32RotaryEncoder encoder2 = AiEsp32RotaryEncoder(ENC2_DT, ENC2_CLK, ENC2_SW, -1, ENC2_STEPS, !ENC2_PULLUP);
  #endif
#endif

#if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
  #include "touchscreen.h"
  TouchScreen touchscreen;
#endif

void IRAM_ATTR readEncoderISR() {
  #if ENC_DT!=255
    if ((SD_CS==255 && display.mode()==LOST) || display.mode()==UPDATING) return;
    encoder.readEncoder_ISR();
  #endif
}

void IRAM_ATTR readEncoder2ISR() {
  #if ENC2_DT!=255
    if ((SD_CS==255 && display.mode()==LOST) || display.mode()==UPDATING) return;
    encoder2.readEncoder_ISR();
  #endif
}

void Controls::btnClickCb(void* p)           { controls.onBtnClick((int)p); }
void Controls::btnDoubleClickCb(void* p)     { controls.onBtnDoubleClick((int)p); }
void Controls::btnLongPressStartCb(void* p)  { controls.onBtnLongPressStart((int)p); }
void Controls::btnLongPressStopCb(void* p)   { controls.onBtnLongPressStop((int)p); }

// Lookup table: user-facing 0-7 scale ??raw encoder acceleration 0-700
static const uint16_t encAccelLUT[8] = {0, 5, 11, 26, 59, 135, 307, 700};

void Controls::init() {
  #if ENC_DT!=255
    encoder.begin();
    encoder.setup(readEncoderISR);
    encoder.setBoundaries(0, 254, true);
    encoder.setAcceleration(encAccelLUT[config.store.encacc]);
  #endif
  #if ENC2_DT!=255
    encoder2.begin();
    encoder2.setup(readEncoder2ISR);
    encoder2.setBoundaries(0, 254, true);
    encoder2.setAcceleration(encAccelLUT[config.store.encacc]);
  #endif

  #if ISPUSHBUTTONS
    for (int i = 0; i < nrOfButtons; i++) {
      if ((i == EVT_BTN_DOWN && BTN_DOWN == 255) || (i == EVT_BTN_PLAY && BTN_PLAY == 255) || (i == EVT_BTN_UP && BTN_UP == 255) || (i == EVT_ENC_SW && ENC_SW == 255) || (i == EVT_BTN_PREV && BTN_PREV == 255) || (i == EVT_BTN_NEXT && BTN_NEXT == 255) || (i == EVT_ENC2_SW && ENC2_SW == 255) || (i == EVT_BTN_MODE && BTN_MODE == 255)) continue;
      button[i].attachClick(btnClickCb, (void*)i);
      button[i].attachDoubleClick(btnDoubleClickCb, (void*)i);
      button[i].attachLongPressStart(btnLongPressStartCb, (void*)i);
      button[i].attachLongPressStop(btnLongPressStopCb, (void*)i);
      button[i].setClickMs(BTN_CLICK_TICKS);
      button[i].setPressMs(BTN_PRESS_TICKS);
    }
  #endif

  #if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
    touchscreen.init(display.width(), display.height());
  #endif
  #if IR_PIN!=255
    pinMode(IR_PIN, INPUT);
    assert(irutils::lowLevelSanityCheck() == 0);
    #if DECODE_HASH
      irrecv.setUnknownThreshold(kMinUnknownSize);
    #endif  // DECODE_HASH
    irrecv.setTolerance(config.store.irtlp);
    irrecv.enableIRIn();
  #endif // IR_PIN!=255
}

void Controls::loop() {
  if (display.mode()==UPDATING || display.mode()==SDCHANGE) return;
  if (SD_CS==255 && display.mode()==LOST) return;
  backlightControls.controlsLoop();
  #if ENC_DT!=255
    encoder1Loop();
  #endif
  #if ENC2_DT!=255
    encoder2Loop();
  #endif
  #if ISPUSHBUTTONS
    for (unsigned i = 0; i < nrOfButtons; i++) {
      if ((i == EVT_BTN_DOWN && BTN_DOWN == 255) || (i == EVT_BTN_PLAY && BTN_PLAY == 255) || (i == EVT_BTN_UP && BTN_UP == 255) || (i == EVT_ENC_SW && ENC_SW == 255) || (i == EVT_BTN_PREV && BTN_PREV == 255) || (i == EVT_BTN_NEXT && BTN_NEXT == 255) || (i == EVT_ENC2_SW && ENC2_SW == 255) || (i == EVT_BTN_MODE && BTN_MODE == 255)) continue;
      button[i].tick();
      if (lpId >= 0) {
        if (DSP_MODEL == DSP_DUMMY && (lpId == EVT_BTN_PREV || lpId == EVT_BTN_NEXT)) continue;
        onBtnDuringLongPress(lpId);
      }
    }
  #endif
  #if IR_PIN!=255
    irLoop();
  #endif
  #if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
    if (network.status == CONNECTED || network.status == SDOFFLINE) touchscreen.loop();
  #endif
}

#if (ENC_DT!=255 && ENC_CLK!=255) || (ENC2_DT!=255 && ENC2_CLK!=255)
  void Controls::encodersLoop(AiEsp32RotaryEncoder *enc, bool first) {
    if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
    if (display.mode()==LOST) return;
    int8_t encoderDelta = enc->encoderChanged();
    if (encoderDelta!=0) {
      uint8_t encBtnState = digitalRead(first?ENC_SW:ENC2_SW);
      if (DSP_MODEL == DSP_DUMMY) {
        first = first?(first && encBtnState):(!encBtnState);
        if (first) {
          int nv = config.store.volume+encoderDelta;
          if (nv<0) nv=0;
          if (nv>VOLUME_SCALE) nv=VOLUME_SCALE;
          player.setVol((uint8_t)nv);
        } else {
          if (encoderDelta > 0) player.next(); else player.prev();
        }
      } else {
        if (first) {
        controlsEvent(encoderDelta > 0, encoderDelta);
        } else {
          if (encBtnState == HIGH && display.mode() == PLAYER) {
            if (config.store.oneclickswitch) {
              if (encoderDelta > 0) player.next(); else player.prev();
              return;
            }
            display.putRequest(NEWMODE, STATIONS);
            unsigned long _modeWaitStart = millis();
            while(display.mode() != STATIONS && millis()-_modeWaitStart<2000) {delay(10);}
          }
          controlsEvent(encoderDelta > 0, encoderDelta);
        }
      }
    }
  }
#endif //#if (ENC_DT!=255 && ENC_CLK!=255) || (ENC2_DT!=255 && ENC2_CLK!=255)

void Controls::encoder1Loop() {
  #if ENC_DT!=255
   encodersLoop(&encoder, true);
  #endif
}

void Controls::encoder2Loop() {
  #if ENC2_DT!=255
    encodersLoop(&encoder2, false);
  #endif
}

void Controls::irBlink() {
  #if IR_PIN!=255
    if (LED_PIN==255) return;
    if (player.status() == STOPPED) {
      for (uint8_t i = 0; i < 7; i++) {
        digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        delay(100);
      }
    }
  #endif
}

void Controls::irNumber(uint8_t num) {
  #if IR_PIN!=255
    uint16_t s;
    if (display.numOfNextStation == 0 && num == 0) return;
    display.putRequest(NEWMODE, NUMBERS);
    if (display.numOfNextStation > UINT16_MAX / 10) return;
    s = display.numOfNextStation * 10 + num;
    if (s > utility.playlistLength()) return;
    display.numOfNextStation = s;
    display.putRequest(NEXTSTATION, s);
  #endif
}

void Controls::irLoop() {
  #if IR_PIN!=255
    if (irrecv.decode(&irResults)) {
      if (irResults.value<256) return;
      if (netserver.irRecordEnable) {
        String irText = resultToHumanReadableBasic(&irResults);
        FUNCTIONLOG("Controls.IR", "%s", irText.c_str());
        FUNCTIONLOG("Controls.IR", "--------------------------");
        config.ircodes.irVals[config.irindex][config.irchck]=irResults.value;
        netserver.irToWs(typeToString(irResults.decode_type, irResults.repeat).c_str(), irResults.value);
        return;
      }
      if (!irResults.repeat/* && irResults.command!=0*/) {
        irVolRepeat = 0;
      }
      switch (irVolRepeat) {
        case 1: {
            controlsEvent(display.mode() == STATIONS ? false : true);
            break;
          }
        case 2: {
            controlsEvent(display.mode() == STATIONS ? true : false);
            break;
          }
      }
      for(int target=0; target<17; target++) {
        for(int j=0; j<3; j++) {
          if (config.ircodes.irVals[target][j]==irResults.value) {
            if (network.status != CONNECTED && network.status!=SDOFFLINE && target!=IR_AST) return;
            if (target!=IR_AST && display.mode()==LOST) return;
            if (screenSaverExit()) delay(200); // give it time to exit before doing the action
            switch (target) {
              case IR_PLAY: {
                  irBlink();
                  if (display.mode() == NUMBERS) {
                    display.putRequest(NEWMODE, PLAYER);
                    player.sendCommand({PR_PLAY, display.numOfNextStation});
                    display.numOfNextStation = 0;
                    break;
                  }
                  onBtnClick(1);
                  break;
                }
              case IR_PREV: {
                  player.prev();
                  break;
                }
              case IR_NEXT: {
                  player.next();
                  break;
                }
              case IR_UP: {
                  controlsEvent(display.mode() == STATIONS ? false : true);
                  irVolRepeat = 1;
                  break;
                }
              case IR_DOWN: {
                  controlsEvent(display.mode() == STATIONS ? true : false);
                  irVolRepeat = 2;
                  break;
                }
              case IR_HASH: {
                  if (display.mode() == NUMBERS) {
                    display.putRequest(NEWMODE, PLAYER);
                    display.numOfNextStation = 0;
                    break;
                  }
                  display.putRequest(NEWMODE, display.mode() == PLAYER ? STATIONS : PLAYER);
                  break;
                }
              case IR_0: {
                  irNumber(0);
                  break;
                }
              case IR_1: {
                  irNumber(1);
                  break;
                }
              case IR_2: {
                  irNumber(2);
                  break;
                }
              case IR_3: {
                  irNumber(3);
                  break;
                }
              case IR_4: {
                  irNumber(4);
                  break;
                }
              case IR_5: {
                  irNumber(5);
                  break;
                }
              case IR_6: {
                  irNumber(6);
                  break;
                }
              case IR_7: {
                  irNumber(7);
                  break;
                }
              case IR_8: {
                  irNumber(8);
                  break;
                }
              case IR_9: {
                  irNumber(9);
                  break;
                }
              case IR_AST: {
                  //ESP.restart();
                  onBtnClick(EVT_BTN_MODE);
                  break;
                }
            } /* switch (target) */
            target=17;
            break;
          } /* if (config.ircodes.irVals[target][j]==irResults.value) */
        }   /* for(int j=0; j<3; j++) */
      }     /* for(int target=0; target<16; target++) */
    }       /* if (irrecv.decode(&irResults)) */
  #endif // if IR_PIN!=255
}

void Controls::onBtnLongPressStart(int id) {
  switch ((controlEvt_e)id) {
    case EVT_BTN_DOWN:
    case EVT_BTN_UP:
    case EVT_BTN_PREV:
    case EVT_BTN_NEXT: {
        lpId = id;
        break;
      }
    case EVT_BTN_PLAY:
    case EVT_ENC_SW: {
        if (DSP_MODEL == DSP_DUMMY) break;
        display.putRequest(NEWMODE, display.mode() == PLAYER ? STATIONS : PLAYER);
        break;
    }
    case EVT_ENC2_SW:
    case EVT_BTN_MODE: {
        if (network.status == SDOFFLINE) break;
        #ifndef DEEP_SLEEP_DISABLE
          display.putRequest(NEWMODE, SLEEPING);
        #endif
        break;
      }
    default: break;
  }
}

void Controls::onBtnLongPressStop(int id) {
  switch ((controlEvt_e)id) {
    case EVT_BTN_DOWN:
    case EVT_BTN_UP:
    case EVT_BTN_PREV:
    case EVT_BTN_NEXT: {
        lpId = -1;
        break;
      }
    case EVT_BTN_MODE:
    case EVT_ENC2_SW: {
        if (network.status == SDOFFLINE) break;
        #ifndef DEEP_SLEEP_DISABLE
          utility.doSleepW();
        #endif
        break;
      }
    case EVT_ENC_SW: {
        break;  // do nothing on release
      }
    default:
        break;
  }
}

boolean Controls::checklpdelay(int m, unsigned long &tstamp) {
  if (millis() - tstamp > m) {
    tstamp = millis();
    return true;
  } else {
    return false;
  }
}

bool Controls::screenSaverExit() {
  if (display.mode() != SCREENSAVER && display.mode() != SCREENBLANK) {
    return false;
  }
  display.resetQueue();
  config.screensaverTicks = 0;
  config.screensaverPlayingTicks = 0;
  display.putRequest(NEWMODE, PLAYER);
  #ifdef DSP_LCD
    delay(200);
  #endif
  return true;
}

void Controls::onBtnDuringLongPress(int id) {
  if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
  if (checklpdelay(BTN_LONGPRESS_LOOP_DELAY, lpDelay)) {
    switch ((controlEvt_e)id) {
      case EVT_BTN_DOWN: {
          controlsEvent(false);
          break;
        }
      case EVT_BTN_UP: {
          controlsEvent(true);
          break;
        }
      case EVT_BTN_PREV: {
          if (config.store.oneclickswitch) {
            controlsEvent(false);
          } else {
            if (display.mode() == PLAYER) {
              display.putRequest(NEWMODE, STATIONS);
            }
            if (display.mode() == STATIONS) {
              controlsEvent(false);
            }
          }
          break;
        }
      case EVT_BTN_NEXT: {
          if (config.store.oneclickswitch) {
            controlsEvent(true);
          } else {
            if (display.mode() == PLAYER) {
              display.putRequest(NEWMODE, STATIONS);
            }
            if (display.mode() == STATIONS) {
              controlsEvent(true);
            }
          }
          break;
        }
      default:
          break;
    }
  }
}

void Controls::controlsEvent(bool toRight, int8_t volDelta) {
  if (screenSaverExit()) {
    return;  // Don't perform action, just exit screensaver
  }
  if (display.mode() == NUMBERS) {
    display.numOfNextStation = 0;
    display.putRequest(NEWMODE, PLAYER);
  }
  if (display.mode() != STATIONS) {
    if (DSP_MODEL != DSP_DUMMY) display.putRequest(NEWMODE, VOL);
    if (volDelta!=0) {
      int nv = config.store.volume+volDelta;
      if (nv<0) nv=0;
      if (nv>VOLUME_SCALE) nv=VOLUME_SCALE;
      player.setVol((uint8_t)nv);
    } else {
      player.stepVol(toRight);
    }
  }
  if (display.mode() == STATIONS) {
    display.resetQueue();
    int p = toRight ? display.currentPlItem + 1 : display.currentPlItem - 1;
    uint16_t cs = utility.playlistLength();
    if (p < 1) p = cs;
    if (p > cs) p = 1;
    display.currentPlItem = p;
    display.putRequest(DRAWPLAYLIST, p);
  }
}

void Controls::onBtnClick(int id) {
  if (screenSaverExit()) return;
  bool passBnCenter = (controlEvt_e)id==EVT_BTN_PLAY || (controlEvt_e)id==EVT_ENC_SW || (controlEvt_e)id==EVT_ENC2_SW;
  controlEvt_e btnid = static_cast<controlEvt_e>(id);
  if (network.status != CONNECTED && network.status!=SDOFFLINE && (controlEvt_e)id!=EVT_BTN_MODE && !passBnCenter) return;
  switch (btnid) {
    case EVT_BTN_DOWN: {
        controlsEvent(false);
        break;
      }
    case EVT_BTN_PLAY:
    case EVT_ENC_SW:
    case EVT_ENC2_SW: {
        if (display.mode() == NUMBERS) {
          display.numOfNextStation = 0;
          display.putRequest(NEWMODE, PLAYER);
        }
        if (display.mode() == VOL) {
          display.putRequest(NEWMODE, PLAYER);
        }
        if (display.mode() == PLAYER) {
          player.toggle();
        }
        if (display.mode() == STATIONS) {
          display.putRequest(NEWMODE, PLAYER);
          #ifdef DSP_LCD
            delay(200);
          #endif
          display.putRequest(CLOSEPLAYLIST, display.currentPlItem);
          //player.sendCommand({PR_PLAY, display.currentPlItem});
        }
        if (network.status == SDOFFLINE) {
          #ifdef USE_SD
            sdman.trySdRemount(); break;
          #endif
        }
        if (network.status==SOFT_AP || display.mode()==LOST) {
          #ifdef USE_SD
            config.saveValue(&config.store.bootStableMarker, true);
            config.saveValue(&config.store.SDoffline, true);
            config.saveValue(&config.store.play_mode, static_cast<uint8_t>(PM_SDCARD));
            ESP.restart();
          #endif
        }
        break;
      }
    case EVT_BTN_UP: {
        controlsEvent(true);
        break;
      }
    case EVT_BTN_PREV:
    case EVT_BTN_NEXT: {
        if (display.mode() == PLAYER) {
          if (config.store.oneclickswitch) {
            if (id == EVT_BTN_PREV) {
              player.prev();
            } else {
              player.next();
            }
          } else {
            display.putRequest(NEWMODE, STATIONS);
          }
        }
        if (display.mode() == STATIONS) {
          controlsEvent(id == EVT_BTN_NEXT);
        }
        break;
      }
    case EVT_BTN_MODE: {
        #ifdef USE_SD
          if (network.status == SDOFFLINE) {
            if (sdman.ready) {
              config.store.sdshuffle = !config.store.sdshuffle;
              display.putRequest(DSPRSSI, 0);
            } else {
              sdman.trySdRemount();
            }
            break;
          }
          config.changeMode();
          break;
        #endif
        }

    default: break;
  }
}

void Controls::onBtnDoubleClick(int id) {
  if (screenSaverExit()) return;
  switch ((controlEvt_e)id) {
    case EVT_BTN_DOWN: {
        if (display.mode() != PLAYER) return;
        if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
        player.prev();
        break;
      }
    case EVT_BTN_PLAY:
    case EVT_ENC_SW: {
        onBtnClick(EVT_BTN_MODE);
        break;
      }
    case EVT_BTN_MODE:
    case EVT_ENC2_SW: {
        if (DSP_MODEL == DSP_DUMMY) break;
        static uint8_t savedVolume = 30;  // preserve the last active volume level before muting
        // Dynamic state check based on real-time core volume instead of a blind boolean flag
        if (player.getVolume() == 0) {
          player.setVolume(savedVolume);  // Restore audio if currently at absolute zero
        } else {
          savedVolume = player.getVolume();  // Capture current volume level before muting
          player.setVolume(0);  // Trigger software/hardware mute via standard core volume method
        }
        break;
      }
    case EVT_BTN_UP: {
        if (display.mode() != PLAYER) return;
        if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
        player.next();
        break;
      }
    case EVT_BTN_PREV: {
        if (display.mode() != PLAYER) return;
        if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
        player.prev();
        break;
      }
    case EVT_BTN_NEXT: {
        if (display.mode() != PLAYER) return;
        if (network.status != CONNECTED && network.status!=SDOFFLINE) return;
        player.next();
        break;
      }
    default:
        break;
  }
}

void Controls::setIRTolerance(uint8_t tl) {
  config.saveValue(&config.store.irtlp, tl);
  #if IR_PIN!=255
    irrecv.setTolerance(config.store.irtlp);
  #endif
}

void Controls::setEncAcceleration(uint8_t userVal) {
  uint16_t raw = (userVal <= 7) ? encAccelLUT[userVal] : 700;
  config.saveValue(&config.store.encacc, userVal);
  #if ENC_DT!=255
    encoder.setAcceleration(raw);
  #endif
  #if ENC2_DT!=255
    encoder2.setAcceleration(raw);
  #endif
}

void Controls::flipTS() {
  #if (TS_MODEL!=TS_MODEL_UNDEFINED) && (DSP_MODEL!=DSP_DUMMY)
    touchscreen.flip();
  #endif
}

// Bare GPIO check run before controls.init()
// Instant read (no blocking): if user is holding a button at boot time, force SD Offline Mode boot
void Controls::checkButtonsHeldOnBoot() {
  #if defined(SDOFFLINE_BTN) && SDOFFLINE_BTN == 255
    #define USE_SD_OFFLINE_MODE false // use no buttons ^^
  #else
    #define USE_SD_OFFLINE_MODE true
  #endif
  #if defined(USE_SD) && USE_SD_OFFLINE_MODE
    bool held = false;
    bool gpioheld = false;
    #ifdef SDOFFLINE_BTN // use 1 button
      #if SDOFFLINE_BTN == BTN_MODE && BTN_MODE != 255
        pinMode(BTN_MODE, BTN_MODE_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_MODE) == LOW) held = true;
      #elif SDOFFLINE_BTN == ENC_SW && ENC_SW != 255
        pinMode(ENC_SW, ENC_SW_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(ENC_SW) == LOW) held = true;
      #elif SDOFFLINE_BTN == ENC2_SW && ENC2_SW != 255
        pinMode(ENC2_SW, ENC2_SW_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(ENC2_SW) == LOW) held = true;
      #elif SDOFFLINE_BTN == BTN_PLAY && BTN_PLAY != 255
        pinMode(BTN_PLAY, BTN_PLAY_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_PLAY) == LOW) held = true;
      #elif SDOFFLINE_BTN == BTN_PREV && BTN_PREV != 255
        pinMode(BTN_PREV, BTN_PREV_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_PREV) == LOW) held = true;
      #elif SDOFFLINE_BTN == BTN_NEXT && BTN_NEXT != 255
        pinMode(BTN_NEXT, BTN_NEXT_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_NEXT) == LOW) held = true;
      #elif SDOFFLINE_BTN == BTN_UP && BTN_UP != 255
        pinMode(BTN_UP, BTN_UP_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_UP) == LOW) held = true;
      #elif SDOFFLINE_BTN == BTN_DOWN && BTN_DOWN != 255
        pinMode(BTN_DOWN, BTN_DOWN_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_DOWN) == LOW) held = true;
      #else
        #ifndef SDOFFLINE_BTN_ACTIVE_LOW
          #define SDOFFLINE_BTN_ACTIVE_LOW true
        #endif
        #ifndef SDOFFLINE_BTN_PULLUP
          #define SDOFFLINE_BTN_PULLUP true
        #endif
        pinMode(SDOFFLINE_BTN, SDOFFLINE_BTN_PULLUP ? INPUT_PULLUP : INPUT);
        bool state = digitalRead(SDOFFLINE_BTN);
        if (SDOFFLINE_BTN_ACTIVE_LOW ? (state == LOW) : (state == HIGH)) gpioheld = true;
      #endif
    #else // use all buttons
      #if ENC_SW != 255
        pinMode(ENC_SW, ENC_SW_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(ENC_SW) == LOW) held = true;
      #endif
      #if ENC2_SW != 255
        pinMode(ENC2_SW, ENC2_SW_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(ENC2_SW) == LOW) held = true;
      #endif
      #if BTN_MODE != 255
        pinMode(BTN_MODE, BTN_MODE_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_MODE) == LOW) held = true;
      #endif
      #if BTN_PLAY != 255
        pinMode(BTN_PLAY, BTN_PLAY_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_PLAY) == LOW) held = true;
      #endif
      #if BTN_PREV != 255
        pinMode(BTN_PREV, BTN_PREV_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_PREV) == LOW) held = true;
      #endif
      #if BTN_NEXT != 255
        pinMode(BTN_NEXT, BTN_NEXT_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_NEXT) == LOW) held = true;
      #endif
      #if BTN_UP != 255
        pinMode(BTN_UP, BTN_UP_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_UP) == LOW) held = true;
      #endif
      #if BTN_DOWN != 255
        pinMode(BTN_DOWN, BTN_DOWN_PULLUP ? INPUT_PULLUP : INPUT);
        if (digitalRead(BTN_DOWN) == LOW) held = true;
      #endif
    #endif
    if (held || gpioheld) {
      network.offlineMode = true;  // signals network.begin() to skip Wi-Fi
      config.store.play_mode = PM_SDCARD;
      config.syncSDFS();  // update _SDplaylistFS so SDPLFS() returns &sdman not &SPIFFS
      if (gpioheld) BOOTLOG("SD Offline Mode triggered by gpio hold");
      else BOOTLOG("SD Offline Mode triggered by button hold");
    }
  #endif
}

Controls controls;
