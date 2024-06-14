#include "74HC595.h"
#include "hardware/gpio.h"
#include <stddef.h>
#include <string.h>
#include <bsp/board_api.h>

HC595::HC595(uint8_t dat, uint8_t lat, uint8_t clk) 
    : _dat(dat), _lat(lat), _clk(clk), _bits(0)
{
    _prev = 0xff;

    // --> initialize GPIO.
    gpio_init(dat);
    gpio_init(lat);
    gpio_init(clk);

    // --> configure direction.
    gpio_set_dir(dat, GPIO_OUT);
    gpio_set_dir(lat, GPIO_OUT);
    gpio_set_dir(clk, GPIO_OUT);

    // --> set low for all out pins.
    gpio_put(dat, 0);
    gpio_put(lat, 0);
    gpio_put(clk, 0);
}

HC595::~HC595() {
}

uint8_t HC595::bit(uint8_t n) const {
    return (_bits & (1 << n)) != 0;
}

void HC595::bit(uint8_t n, uint8_t value) {
    if (value) {
        _bits |= (1 << n);
    }

    else {
        _bits &= ~(1 << n);
    }
}

bool HC595::flush() {
    if (_prev != _bits) {
        _prev = _bits;
        
        gpio_put(_lat, 0);
        delayNs();
        
        for(uint8_t i = 0; i < 8; ++i) {
            gpio_put(_clk, 0);
            gpio_put(_dat, bit(i));
            delayNs();

            gpio_put(_clk, 1);
            delayNs();
        }

        gpio_put(_clk, 0);
        delayNs();

        gpio_put(_lat, 1);
        gpio_put(_lat, 0);
        delayNs();
        
        gpio_put(_dat, 0);
    }

    return true;
}