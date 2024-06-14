#ifndef __DRIVERS_74HC595_H__
#define __DRIVERS_74HC595_H__

#include <stdint.h>

/**
 * 74HC595 driver.
*/
class HC595 {
private:
    /**
     * generate nano seconds delay.
     * this is used to ensure GPIO pin state to be applied.
     */
    static void __attribute__((optimize("O0"))) delayNs() {
        for(uint32_t i = 0; i < 10; ++i);
    }

public:
    HC595(uint8_t dat, uint8_t lat, uint8_t clk);
    ~HC595();

private:
    uint8_t _dat, _lat, _clk;
    uint8_t _bits, _prev;
    
public:
    /* get N'th bit state. */
    uint8_t bit(uint8_t n) const;

    /* set N'th bit state. */
    void bit(uint8_t n, uint8_t value);

    /* flush bits to the chipset. */
    bool flush();
};

#endif