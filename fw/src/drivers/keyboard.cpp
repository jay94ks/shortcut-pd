#include "keyboard.h"
#include <string.h>
#include <hardware/gpio.h>
#include <bsp/board_api.h>

const uint8_t Keyboard::PIN_ROW[MAX_ROW] = {
    EGPIO_ROW1, EGPIO_ROW2
};

const uint8_t Keyboard::PIN_COL[MAX_COL] = {
    EGPIO_COL1, EGPIO_COL2, EGPIO_COL3
};

EKey KeyOrderIterator::operator*() const {
    if (_kbd) {
        return _kbd->getKeyInOrder(_cur);
    }

    return EKEY_INV;
}

Keyboard::Keyboard() {
    for(uint8_t i = 0; i < MAX_ROW; ++i) {
        const uint8_t pin = PIN_ROW[i];

        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        gpio_put(pin, 0);
    }

    for(uint8_t i = 0; i < MAX_COL; ++i) {
        const uint8_t pin = PIN_COL[i];

        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
    }

    memset(_next, 0, sizeof(_next));
    memset(_state, 0, sizeof(_state));
    
    _ordered = 0;
    _scantick = 0;

    for(uint8_t i = 0; i < EKEY_MAX; ++i) {
        _orders[i] = EKEY_INV;
    }
}

void Keyboard::scanOnce() {
    const uint32_t now = board_millis();
    if (_scantick != now) {
        _scantick = now;

        memset(_next, 0, sizeof(_next));

        // --> scan line levels.
        for(uint8_t i = 0; i < MAX_ROW; ++i) {
            const uint8_t row = PIN_ROW[i];
            
            gpio_put(row, 1);
            delayNs();

            for(uint8_t j = 0; j < MAX_COL; ++j) {
                const uint8_t mask = 1 << j;

                if (gpio_get(PIN_COL[j])) {
                    _next[i] |= mask;
                }
            }

            gpio_put(row, 0);
            delayNs();
        }
    }
    
    updateOnce();
}

void Keyboard::updateOnce() {
    // --> summarize key changes.
    for(uint8_t row = 0; row < MAX_ROW; ++row) { 
        const uint8_t offset = row * MAX_COL;

        for(uint8_t col = 0; col < MAX_COL; ++col) {
            const EKey key = EKey(offset + col);
            const uint8_t mask = 1 << col;

            const uint8_t prev 
                = _state[key].ls == EKSL_RISE || 
                  _state[key].ls == EKSL_HIGH;

            const uint8_t next = (_next[row] & mask) != 0;
            
            if (prev != next) {
                if (next) {
                    _state[key].ls = EKSL_RISE;
                    addOrder(key);
                } else {
                    _state[key].ls = EKSL_FALL;
                    removeOrder(key);
                }

                _state[key].lt = board_millis();
                continue;
            }

            if (_state[key].ls == EKSL_HIGH ||
                _state[key].ls == EKSL_LOW)
            {
                continue;   
            }

            if (_state[key].ls == EKSL_RISE) {
                _state[key].ls = EKSL_HIGH;
            } else {
                _state[key].ls = EKSL_LOW;
            }
            
            _state[key].lt = board_millis();
        }
    }

    // --> copy and fill keys.
    assignOrders();
}

int32_t Keyboard::findOrder(EKey key) {
    const uint8_t count
        = _ordered > EKEY_MAX
        ? EKEY_MAX : _ordered;

    for(uint8_t i = 0; i < count; ++i) {
        if (_orders[i] == key) {
            return i;
        }
    }

    return -1;
}

void Keyboard::addOrder(EKey key) {
    if (_ordered >= EKEY_MAX) {
        return;
    }

    const uint8_t count
        = _ordered > EKEY_MAX
        ? EKEY_MAX : _ordered;

    removeOrder(key);

    // --> insert the key to front.
    _orders[_ordered++] = key;
}

void Keyboard::removeOrder(EKey key) {
    int32_t n = findOrder(key);
    if (n < 0) {
        return;
    }

    const uint8_t count
        = _ordered > EKEY_MAX
        ? EKEY_MAX : _ordered;

    for (uint8_t i = n; i < count - 1; ++i) {
        _orders[i] = _orders[i + 1];
    }

    _ordered--;
}

void Keyboard::assignOrders() {
    const uint8_t count
        = _ordered > EKEY_MAX
        ? EKEY_MAX : _ordered;

    for (uint8_t i = 0; i < EKEY_MAX; ++i) {
        _state[i].ko = 0xff;
    }
    
    for (uint8_t i = 0; i < count; ++i) {
        const EKey key = _orders[i];
        if (SKey* ptr = getKeyPtr(key)) {
            ptr->ko = i;
        }
    }
}

uint8_t Keyboard::getKeyOrder(EKey key) const {
    if (SKey* ptr = getKeyPtr(key)) {
        return ptr->ko;
    }

    return 0xff;
}

EKey Keyboard::getKeyInOrder(uint8_t order) const {
    const uint8_t count
        = _ordered > EKEY_MAX
        ? EKEY_MAX : _ordered;

    if (order >= count) {
        return EKEY_INV;
    }

    return _orders[order];
}

uint8_t Keyboard::getKeys(EKeyState state, EKey* buf, uint8_t max) const {
    if (max > EKEY_MAX) {
        max = EKEY_MAX;
    }

    if (max <= 0) {
        return 0;
    }
    
    uint8_t index = 0;
    for(uint8_t key: _orders) {
        if (index >= max) {
            break;
        }

        buf[index++] = EKey(key);
    }

    return index;
}

SKey *Keyboard::getKeyPtr(EKey key) const {
    if (key >= EKEY_MAX) {
        return nullptr;
    }

    return &_state[key];
}

EKeyState Keyboard::getKeyState(EKey key) const {
    if (key >= EKEY_MAX) {
        return EKSL_LOW;
    }

    return EKeyState(_state[key].ls);
}

bool Keyboard::isKeyDown(EKey key) const {
    if (key >= EKEY_MAX) {
        return false;
    }

    return _state[key].ls == EKSL_HIGH;
}

bool Keyboard::isKeyUp(EKey key) const {
    if (key >= EKEY_MAX) {
        return false;
    }

    return _state[key].ls == EKSL_LOW;
}