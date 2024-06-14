#include "hid.h"
#include "../keyboard.h"
#ifdef __INTELLISENSE__
#include <tusb_config.h>
#define CFG_TUD_EXTERN
#endif
#include <tusb.h>
#include <string.h>

#ifdef __INTELLISENSE__
#define tud_hid_keyboard_report( ... )
#endif

/**
 * LED bits that the HID supports.
*/
#define KEYBOARD_LED_SUPPORTS ( \
    KEYBOARD_LED_NUMLOCK | KEYBOARD_LED_CAPSLOCK | \
    KEYBOARD_LED_SCROLLLOCK | KEYBOARD_LED_COMPOSE | \
    KEYBOARD_LED_KANA )

// --> store the received LED state.
static uint8_t g_usbHidLeds = 0;

UsbHid::UsbHid()
    : _modifiers(0), _blocked(0)
{
    _keyboard = nullptr;
    memset(_keycodes, 0, sizeof(_keycodes));
}

uint8_t UsbHid::leds() {
    uint8_t retval = EHLED_NONE;

#define USBHID_TRANSLATE_INDICATOR(kled, hled) \
    if ((g_usbHidLeds & (kled)) != 0) { \
        retval |= (hled); \
    }

    USBHID_TRANSLATE_INDICATOR(KEYBOARD_LED_NUMLOCK, EHLED_NUMLOCK);
    USBHID_TRANSLATE_INDICATOR(KEYBOARD_LED_CAPSLOCK, EHLED_CAPSLOCK);
    USBHID_TRANSLATE_INDICATOR(KEYBOARD_LED_SCROLLLOCK, EHLED_SCROLLLOCK);
    USBHID_TRANSLATE_INDICATOR(KEYBOARD_LED_COMPOSE, EHLED_COMPOSE);
    USBHID_TRANSLATE_INDICATOR(KEYBOARD_LED_KANA, EHLED_KANA);

#undef USBHID_TRANSLATE_INDICATOR

    return retval;
}

void UsbHid::init(Keyboard *kbd) {
    _keyboard = kbd;
}

bool UsbHid::updateKeyCodes() {
    uint8_t keycodes[MAX_KC] = {0, };
    uint8_t modifiers = 0;

    uint8_t index = 0;
    bool dirty = false;

    if (_blocked == 0) {
        for(uint8_t keyn : _keyboard->pressing()) {
            const SKey* ptr = _keyboard->getKeyPtr(EKey(keyn));

            if (ptr->kc && index < MAX_KC) {
                keycodes[index++] = ptr->kc;
            }

            if (ptr->km) {
                modifiers |= ptr->km;
            }
        }
    }

    dirty = memcmp(keycodes, _keycodes, sizeof(keycodes)) != 0 
        || _modifiers != modifiers;

    if (dirty) {
        _modifiers = modifiers;
        memcpy(_keycodes, keycodes, sizeof(keycodes));
    }

    return dirty;
}

void UsbHid::transmitOnce() {
    if (updateKeyCodes()) {
        tud_hid_keyboard_report(REPORT_ID, _modifiers, _keycodes);
    }
}


CFG_TUD_EXTERN void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize) {
  (void) instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT && report_id == UsbHid::REPORT_ID) {
        if (bufsize < 1) {
            return;
        }

        // --> receive keyboard LED status.
        g_usbHidLeds = buffer[0] & KEYBOARD_LED_SUPPORTS;
    }
}

CFG_TUD_EXTERN uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen) {
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;
  return 0;
}
