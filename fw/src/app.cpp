#include "app.h"
#include "drivers/usbd/usbd.h"

#ifdef __INTELLISENSE__
#include <tusb_config.h>
#endif
#include <tusb.h>
#include <bsp/board_api.h>
#include <hardware/gpio.h>
#include <hardware/watchdog.h>
#include <pico/bootrom.h>
#include <pico/multicore.h>
#include <pico/mutex.h>

struct AppConf {
    uint32_t ver;
    SKeyConf keys[EKEY_MAX];
};

const SKeyConf App::DEFAULT_KEYCONFS[EKEY_MAX] = {
    { EKCM_NONE, KC_0, KM_NONE, 0 },
    { EKCM_NONE, KC_1, KM_NONE, 1 },
    { EKCM_NONE, KC_2, KM_NONE, 2 },
    { EKCM_NONE, KC_3, KM_NONE, 3 },
    { EKCM_NONE, KC_4, KM_NONE, 4 },
    { EKCM_NONE, KC_5, KM_NONE, 5 }
};

App::App()
    : _ledctl(EGPIO_595_DAT, EGPIO_595_LAT, EGPIO_595_CLK),
      _flash(spi0, EGPIO_SPI0_CSn, EGPIO_SPI0_SCK, EGPIO_SPI0_RX, EGPIO_SPI0_TX),
      _timer(nullptr), _blocked(false), _needSave(false), _saveTime(0)
{
    gpio_init(EGPIO_LED_CR);
    gpio_init(EGPIO_LED_CE);

    gpio_set_dir(EGPIO_LED_CR, GPIO_OUT);
    gpio_set_dir(EGPIO_LED_CE, GPIO_OUT);

    gpio_put(EGPIO_LED_CR, 1);
    gpio_put(EGPIO_LED_CE, 1);
}

void App::onTimerCore() {
    // --> receive `this` pointer through fifo, then run other core's main.
    ((App*) multicore_fifo_pop_blocking())
        ->runTimerCore();

    while(1);
}

void App::runApp() {
    if (_flash.init() == false)
    {
        gpio_put(EGPIO_LED_CR, 0);
        gpio_put(EGPIO_LED_CE, 0);
        while(1);
    }

    _flash.fastMode(true);
    _hid.init(&_keyboard);

    // --> TUD initialization.
    tud_init(0);

    // --> load configurations here.
    loadConf();

    // --> launch the other core and, pass `this` pointer.
    multicore_launch_core1(onTimerCore);
    multicore_fifo_push_blocking(uint32_t(this));
    
    // --> wait for other core, then notify about this core.
    multicore_fifo_pop_blocking();
    multicore_fifo_push_blocking(0);

    SCdcMessage msg;

    _saveTime = board_millis();

    // --> run timers here.
    while(1) {
        _keyboard.scanOnce();
        checkToggle();

        if (usbdIsResetRequired()) {
            usbdResetNow();
            _cdc.reset();
        }

        if (_blocked) {
            emitKeyReport(true);
        }

        _hid.transmitOnce();
        _cdc.updateOnce();
        tud_task();

        if (_cdc.read(msg)) {
            handleMsg(msg);
        }

        tickToSave();
    }
}

void App::tickToSave() {
    if (_needSave) {
        uint32_t now = board_millis();
        if ((now - _saveTime) < 1000) {
            return;
        }

        _saveTime = now;
        _needSave = false;

        saveConf();
    }
}

void App::reserveSave() {
    _saveTime = board_millis();
    _needSave = true;
}

void App::checkToggle() {
    const uint8_t leds = UsbHid::leds();
    for(uint8_t i = 0; i < EKEY_MAX; ++i) {
        if (SKey* key = _keyboard.getKeyPtr(EKey(i))) {
            switch (key->kc) {
                case KC_CAPS_LOCK:
                    key->ts = (leds & EHLED_CAPSLOCK) != 0 ? 1 : 0;
                    break;

                case KC_NUM_LOCK:
                    key->ts = (leds & EHLED_NUMLOCK) != 0 ? 1 : 0;
                    break;

                case KC_SCROLL_LOCK:
                    key->ts = (leds & EHLED_SCROLLLOCK) != 0 ? 1 : 0;
                    break;

                default:
                    break;
            }
        }
    }
}

void App::runTimerCore() {
    // --> notify about this core, then wait for other core.
    multicore_fifo_push_blocking(0);
    multicore_fifo_pop_blocking();

    uint32_t last = 0;
    while(1) {
        const uint32_t nowtick = board_millis();
        if (last == nowtick) {
            updateLeds();
            continue;
        }

        last = nowtick;

        // --
        tickTimer(nowtick);
        updateLeds();
    }
}

void App::tickTimer(uint32_t nowtick) {
    STimer* current = _timer;
    while(current) {
        STimer* next = current->link;

        // --> timer is already shot.
        if (current->type == ETIMER_NONE || 
            current->trig != TIMER_TRIG_PENDING)
        {
            current = next;
            continue;
        }
        
        const uint32_t time = nowtick - current->base;

        TimerCb cb = nullptr;
        TimerCleanupCb ccb = nullptr;

        if (time >= current->time) {
            cb = current->cb;

            // --> time reached.
            if (current->type == ETIMER_ONESHOT) {
                // --> unschedule the timer.
                current->trig = TIMER_TIRG_ONESHOT;
                ccb = current->ccb;
            }
        }

        // --> the callback is reserved.
        if (cb) {
            cb(current);
        }

        if (ccb && current->trig != TIMER_TRIG_CLEANUP) {
            current->trig = TIMER_TRIG_CLEANUP;
            ccb(current);
        }

        current = next;
    }
}

bool App::schedule(STimer* timer) {
    // --> reject to register `NONE` type.
    if (timer->type == ETIMER_NONE) {
        return false;
    }

    const STimer* current = _timer;
    while (current) {
        if (current == timer) {
            return false;
        }

        current = current->link;
    }

    timer->trig = TIMER_TRIG_PENDING;
    timer->link = _timer;

    _timer = timer;

    return true;
}

bool App::unschedule(STimer* timer) {
    STimer* current = _timer;
    STimer* prev = nullptr;

    while (current) {
        if (current == timer) {
            if (prev) {
                prev->link = timer->link;
            } else {
                _timer = timer->link;
            }

            timer->link = nullptr;
            return true;
        }

        prev = current;
        current = current->link;
    }

    return false;
}

void App::panic() {
    uint8_t n = 0;
    uint32_t systick = board_millis();
    while(1) {
        uint32_t nowtick = board_millis();
        uint32_t elapsed = nowtick - systick;

        if (elapsed >= 100) {
            uint8_t s = (n++) % 2;

            gpio_put(EGPIO_LED_CR, s);
            gpio_put(EGPIO_LED_CE, 1 - s);

            systick = nowtick;
        }
    }
}

void App::loadConf() {
    AppConf conf;

    // --> read full configuration.
    if (!_flash.read(0, &conf)) {
        // --> flash corrupted.
        panic();
        return;
    }

    // --> not initialized: use default.
    if (conf.ver != 1) {
        conf.ver = 1;

        // --> copy default configurations,
        memcpy(conf.keys, DEFAULT_KEYCONFS, sizeof(DEFAULT_KEYCONFS));

        reserveSave();
    }

    // --> set configurations to key pointers.
    for(uint8_t i = 0; i < EKEY_MAX; ++i) {
        const EKey key = EKey(i);
        const SKeyConf& each = conf.keys[i];

        if (SKey* ptr = _keyboard.getKeyPtr(key)) {
            ptr->cm = each.cm;
            ptr->kc = each.kc;
            ptr->km = each.km;
            ptr->id = each.id;
        }
    }
}

void App::saveConf() {
    AppConf conf = { 0, };

    conf.ver = 1;
    
    // --> copy defaults to configurations.
    memcpy(conf.keys, DEFAULT_KEYCONFS, sizeof(DEFAULT_KEYCONFS));
    
    // --> get configurations from the key pointer.
    for(uint8_t i = 0; i < EKEY_MAX; ++i) {
        const EKey key = EKey(i);
        
        if (SKey* ptr = _keyboard.getKeyPtr(key)) {
            conf.keys[i].cm = ptr->cm;
            conf.keys[i].kc = ptr->kc;
            conf.keys[i].km = ptr->km;
            conf.keys[i].id = ptr->id;
        }
    }

    // --> store configurations to the flash memory.
    _flash.eraseSector(0);
    _flash.write(0, &conf);
}

void App::updateLeds() {
    uint8_t leds = UsbHid::leds();

    gpio_put(EGPIO_LED_CR, usbdIsMounted() == false);
    gpio_put(EGPIO_LED_CE, _blocked ? 0 : 1);
    
    updateKeyLed(ELED_00, EKEY_00);
    updateKeyLed(ELED_01, EKEY_01);
    updateKeyLed(ELED_02, EKEY_02);
    updateKeyLed(ELED_10, EKEY_10);
    updateKeyLed(ELED_11, EKEY_11);
    updateKeyLed(ELED_12, EKEY_12);

    _ledctl.bit(ELED_TL, (leds & EHLED_NUMLOCK) == 0);
    _ledctl.bit(ELED_TR, (leds & EHLED_CAPSLOCK) == 0);

    _ledctl.flush();
}

void App::updateKeyLed(uint8_t led, EKey key) {
    SKey* ptr = _keyboard.getKeyPtr(key);
    if (!ptr) {
        _ledctl.bit(led, 0); // --> turn on the key LED.
        return;
    }

    switch(ptr->cm) {
        case EKCM_NONE:
            // --> turn off for released state.
            _ledctl.bit(led, ptr->ls == EKSL_LOW);
            break;

        case EKCM_INVERT:
            // --> turn off for pressed state.
            _ledctl.bit(led, ptr->ls == EKSL_HIGH);
            break;

        case EKCM_TOGGLE_INVERT:
            // --> controlled by inverted toggle state.
            _ledctl.bit(led, ptr->ts);
            break;

        default:
            // --> controlled by toggle state.
            _ledctl.bit(led, !ptr->ts);
            break;
    }
}

void App::handleMsg(const SCdcMessage& msg) {
    switch(msg.opcode) {
        case ECDCM_ECHO: /* echo. */
            _cdc.write(msg);
            break;

        case ECDCM_GET_KEYS: {
            emitKeyInfo(ECDCM_GET_KEYS);
            break;
        }

        case ECDCM_SET_KEYS: { // KEY + CONF
            const int32_t count = msg.length / 5;
            for(uint8_t i = 0; i < count; ++i) {
                const EKey key = EKey(msg.data[i * 5 + 0]);
                if (SKey* ptr = _keyboard.getKeyPtr(key)) {
                    ptr->cm = msg.data[i * 5 + 1];
                    ptr->kc = msg.data[i * 5 + 2];
                    ptr->km = msg.data[i * 5 + 3];
                    ptr->id = msg.data[i * 5 + 4];
                }
            }

            emitKeyInfo(ECDCM_SET_KEYS);
            reserveSave();
            break;   
        }

        case ECDCM_RESET_KEYS: {
            for(uint8_t i = 0; i < EKEY_MAX; ++i) {
                SKey* ptr = _keyboard.getKeyPtr(EKey(i));
                SKeyConf conf = DEFAULT_KEYCONFS[i];

                // --> copy key configurations.
                ptr->cm = conf.cm;
                ptr->kc = conf.kc;
                ptr->km = conf.km;
                ptr->id = conf.id;
            }

            emitKeyInfo(ECDCM_RESET_KEYS);
            reserveSave();
            break;
        }

        case ECDCM_SAVE_CONF: {
            reserveSave();
            _cdc.write(msg); // --> echo once.
            break;
        }

        case ECDCM_CHECK_CAPTURE: {
            emitCaptureState(ECDCM_CHECK_CAPTURE);
            break;
        }

        case ECDCM_ENTER_CAPTURE:
            _blocked = true;
            _hid.setBlocked(true);
            emitCaptureState(ECDCM_ENTER_CAPTURE);
            emitKeyReport(false);
            break;

        case ECDCM_LEAVE_CAPTURE:
            _hid.setBlocked(false);
            _blocked = false;
            emitCaptureState(ECDCM_LEAVE_CAPTURE);
            break;

        case ECDCM_REBOOT: {
            watchdog_reboot(0, 0, 0);
            break;
        }

        case ECDCM_UPLOAD: {
#ifndef __INTELLISENSE__
            reset_usb_boot(0, 0);
#endif
            break;
        }
    }
}

void App::emitKeyInfo(uint8_t opcode) {
    SCdcMessage reply;
    reply.opcode = opcode;
    reply.length = 4 * EKEY_MAX;

    for(uint8_t i = 0; i < EKEY_MAX; ++i) {
        SKey* ptr = _keyboard.getKeyPtr(EKey(i));

        // --> copy key configurations.
        reply.data[i * 4 + 0] = ptr->cm;
        reply.data[i * 4 + 1] = ptr->kc;
        reply.data[i * 4 + 2] = ptr->km;
        reply.data[i * 4 + 3] = ptr->id;
    }

    reply.checksum = _cdc.checksum(reply);
    _cdc.write(reply);
}

void App::emitCaptureState(uint8_t opcode) {
    SCdcMessage reply;
    reply.opcode = opcode;
    reply.length = 1;
    reply.data[0] = _blocked ? 255 : 0;
    reply.checksum = _cdc.checksum(reply);
    _cdc.write(reply);
}

void App::emitKeyReport(bool optimised) {
    uint8_t keyrpt[6] = {0, };
    for(uint8_t i = 0; i < EKEY_MAX; ++i) { 
        SKey* ptr = _keyboard.getKeyPtr(EKey(i));

        // --> key report.
        keyrpt[i] = ptr->ls;
    }

    if (!optimised || memcmp(keyrpt, _keyrpt, sizeof(_keyrpt))) {
        SCdcMessage message;
        memcpy(_keyrpt, keyrpt, sizeof(keyrpt));

        message.opcode = ECDCM_KEY_REPORT;
        message.length = 6;

        memcpy(message.data, keyrpt, sizeof(keyrpt));
        message.checksum = _cdc.checksum(message);

        _cdc.write(message);
    }
}