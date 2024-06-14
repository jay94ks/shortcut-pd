#ifndef __APP_H__
#define __APP_H__

#include "main.h"
#include "drivers/keyboard.h"
#include "drivers/74hc595.h"
#include "drivers/w25qxx.h"

#include "drivers/usbd/hid.h"
#include "drivers/usbd/cdc.h"

#include "timers/timer.h"

/**
 * Application. 
 */
class App {
private:
    static const SKeyConf DEFAULT_KEYCONFS[EKEY_MAX];

private:
    Keyboard _keyboard;
    HC595 _ledctl;
    W25QXX _flash;
    UsbHid _hid;
    UsbCdc _cdc;

    STimer* _timer;
    bool _blocked;

    uint8_t _keyrpt[6];
    bool _needSave;
    uint32_t _saveTime;
    
public:
    App();

private:
    static void onTimerCore();

public:
    void runApp();

private:
    void tickToSave();

    // --> reserve to save conf.
    void reserveSave();

    // --> check toggle key mappings and, 
    //   : set toggle state from HID indicator.
    void checkToggle();

private:
    void runTimerCore();
    void tickTimer(uint32_t nowtick);

public:
    bool schedule(STimer* timer);
    bool unschedule(STimer* timer);
    
private:
    /* system panic. */
    void panic();

    /* load configuration. */
    void loadConf();

    /* save configuration. */
    void saveConf();

    /* update LED states. */
    void updateLeds();

    /* update key LED state. */
    void updateKeyLed(uint8_t led, EKey key);

    /* handle the CDC message. */
    void handleMsg(const SCdcMessage& msg);

    /* emit the key information. */
    void emitKeyInfo(uint8_t opcode = ECDCM_GET_KEYS);

    /* emit the capture state. */
    void emitCaptureState(uint8_t opcode = ECDCM_CHECK_CAPTURE);

    /* emit the key report. */
    void emitKeyReport(bool optimised);

};

#endif