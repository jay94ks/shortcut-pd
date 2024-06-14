#ifndef __DRIVERS_KEYBOARD_H__
#define __DRIVERS_KEYBOARD_H__

#include <stdint.h>
#include "../main.h"

// --> forward decls.
class Keyboard;

enum EKeyState {
    EKSL_LOW = 0,
    EKSL_RISE = 1,
    EKSL_HIGH = 2,
    EKSL_FALL = 3
};

enum EKeyControls {
    EKCM_NONE = 0,
    EKCM_INVERT,
    EKCM_TOGGLE,
    EKCM_TOGGLE_INVERT,
    EKCM_REMOTECTL
};

// --> `None` key order.
constexpr uint8_t KO_NONE = 0xffu;

/**
 * Key state structure.
 */
struct SKey {
    // ------------------------- BY KEYBOARD INSTANCE -------------------------
    uint8_t         ls;     // --> level state: EKeyState.
    uint8_t         ko;     // --> key order, 0 ~ 5.
    uint32_t        lt;     // --> last tick.

    // ----------------------------- CUSTOMIZABLE -----------------------------
    uint8_t         cm;     // --> control mode.
    uint8_t         kc;     // --> key code.
    uint8_t         km;     // --> key modifier.
    uint8_t         ts;     // --> toggle state, 0 or 1.
    uint8_t         id;     // --> key id.

    union {
        uint64_t    u64;
        uint32_t    u32;
        uint16_t    u16;
        uint8_t     u8;
        void*       ptr;
    } data;
};

/**
 * Key configuration.
 */
struct SKeyConf {
    uint8_t         cm;     // --> control mode.
    uint8_t         kc;     // --> key code.
    uint8_t         km;     // --> key modifier.
    uint8_t         id;     // --> key id.
};

/**
 * Key iterator.
 */
class KeyIterator {
private:
    EKey _key;

public:
    KeyIterator() : _key(EKEY_MAX) { }
    KeyIterator(EKey key) : _key(key) { }

public:
    KeyIterator operator++() {
        KeyIterator self = *this;
        if (_key < EKEY_MAX) {
            _key = EKey(_key + 1);
        }

        return self;
    }
    KeyIterator& operator++(int) {
        if (_key < EKEY_MAX) {
            _key = EKey(_key + 1);
        }

        return *this;
    }

    EKey operator*() const { return _key; }

    /* test whether other iterator is same with this or not. */
    bool operator == (const KeyIterator& other) const {
        return _key == other._key;
    }

    /* test whether other iterator is different with this or not. */
    bool operator != (const KeyIterator& other) const {
        return _key != other._key;
    }
};

/**
 * Key order iterator.
*/
class KeyOrderIterator {
private:
    const Keyboard* _kbd;
    uint8_t _cur;
    uint8_t _max;

public:
    KeyOrderIterator() : _kbd(nullptr), _cur(0), _max(0) { }
    KeyOrderIterator(const Keyboard* kbd, uint8_t cur, uint8_t max)
        : _kbd(kbd), _cur(cur), _max(max) { }

public:
    KeyOrderIterator operator++() {
        KeyOrderIterator self = *this;
        if (_cur < _max) {
            _cur = _cur + 1;
        }

        return self;
    }
    KeyOrderIterator& operator++(int) {
        if (_cur < _max) {
            _cur = _cur + 1;
        }

        return *this;
    }

    EKey operator*() const;

    /* test whether other iterator is same with this or not. */
    bool operator == (const KeyOrderIterator& other) const {
        return _cur == other._cur;
    }

    /* test whether other iterator is different with this or not. */
    bool operator != (const KeyOrderIterator& other) const {
        return _cur != other._cur;
    }
};

/**
 * Key order delegate.
 * This provides range access.
*/
class KeyOrderDelegate {
private:
    const Keyboard* _kbd;
    const uint8_t _max;

public:
    KeyOrderDelegate() : _kbd(nullptr), _max(0) { }
    KeyOrderDelegate(const Keyboard* kbd, uint8_t max) : _kbd(kbd), _max(max) { }

public:
    KeyOrderIterator begin() const {
        return KeyOrderIterator(_kbd, 0, _max);
    }

    KeyOrderIterator end() const {
        return KeyOrderIterator(_kbd, _max, _max);
    }
};

/**
 * Keyboard. 
 */
class Keyboard {
private:
    static constexpr uint8_t MAX_ROW = 2;
    static constexpr uint8_t MAX_COL = 3;
    static const uint8_t PIN_ROW[MAX_ROW];
    static const uint8_t PIN_COL[MAX_COL];

    /**
     * generate nano seconds delay.
     * this is used to ensure GPIO pin state to be applied.
     */
    static void __attribute__((optimize("O0"))) delayNs() {
        for(uint32_t i = 0; i < 10; ++i);
    }

private:
    uint8_t _next[MAX_ROW];
    EKey _orders[EKEY_MAX];
    uint8_t _ordered;
    uint32_t _scantick;

    mutable SKey _state[EKEY_MAX];

public:
    Keyboard();

public:
    void scanOnce();

private:
    void updateOnce();

    /* find order of the key.*/
    int32_t findOrder(EKey key);

    /* add a key into order set. */
    void addOrder(EKey key);

    /* remove a key from order set. */
    void removeOrder(EKey key);

    /* copy orders to key pointer. */
    void assignOrders();

public:
    /* get the begining key iterator. */
    KeyIterator begin() const { return KeyIterator(EKey(0)); }

    /* get the ending key iterator. */
    KeyIterator end() const { return KeyIterator(EKEY_MAX); }

    /* get all pressing key delegate for `range` iterations. */
    KeyOrderDelegate pressing() const {
        const uint8_t count
            = _ordered > EKEY_MAX
            ? EKEY_MAX : _ordered;
            
        return KeyOrderDelegate(this, count);
    }

public:
    /* get the key order. */
    uint8_t getKeyOrder(EKey key) const;

    /* get the n'th key in order set. */
    EKey getKeyInOrder(uint8_t order) const;

    /* get all keys in pressing order. */
    uint8_t getKeys(EKeyState state, EKey* buf, uint8_t max) const;

    /* get the key pointer. */
    SKey* getKeyPtr(EKey key) const;

    /* get the key state. */
    EKeyState getKeyState(EKey key) const;

    /* test whether the key is down or not. */
    bool isKeyDown(EKey key) const;

    /* test whether the key is up or not. */
    bool isKeyUp(EKey key) const;
};

#endif
