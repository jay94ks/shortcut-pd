#ifndef __DRIVERS_USBD_HID_H__
#define __DRIVERS_USBD_HID_H__

#include <stdint.h>
#include <vector>
#include "hid_kc.h"

// --> forward decls.
class Keyboard;

/**
 * Usb HID LED indicator bits.
 */
enum EUsbHidLed {
    EHLED_NONE = 0,
    EHLED_NUMLOCK = 1,
    EHLED_CAPSLOCK = 2,
    EHLED_SCROLLLOCK = 4,
    EHLED_COMPOSE = 8,
    EHLED_KANA = 16
};

/**
 * Usb HID transmitter. 
 */
class UsbHid {
public:
    static constexpr uint8_t REPORT_ID = 1;

private:
    static constexpr uint8_t MAX_KC = 6;

private:
    Keyboard* _keyboard;
    uint8_t _keycodes[MAX_KC];
    uint8_t _modifiers;
    uint8_t _blocked;
    
public:
    UsbHid();

public:
    /**
     * get all HID led states.
     * refer `EUsbHidLed` enum listings.
     */
    static uint8_t leds();

public:
    /* initialize the HID instance. */
    void init(Keyboard* kbd);

private:
    bool updateKeyCodes();

public:
    void transmitOnce();
    void setBlocked(bool value) {
        _blocked = value ? 1 : 0;
    }
};

#endif