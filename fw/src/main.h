#ifndef __MAIN_H__
#define __MAIN_H__

enum {

    EGPIO_ROW1 = 5,
    EGPIO_ROW2 = 6,
    EGPIO_COL1 = 7,
    EGPIO_COL2 = 8,
    EGPIO_COL3 = 9,

    EGPIO_595_LAT = 10,
    EGPIO_595_CLK = 11,
    EGPIO_595_DAT = 12,

    EGPIO_SPI0_RX = 16,
    EGPIO_SPI0_CSn = 17,
    EGPIO_SPI0_SCK = 18,
    EGPIO_SPI0_TX = 19,

    EGPIO_LED_CR = 22,      // --> USB mount.
    EGPIO_LED_CE = 23,      // --> USB config.

    EGPIO_VSENS = 29
    
};

enum EKey {
    EKEY_00 = 0,
    EKEY_01,
    EKEY_02,
    EKEY_10,
    EKEY_11,
    EKEY_12,
    EKEY_MAX,
    EKEY_INV = 0xff,
};

enum ELed {
    ELED_00 = 7,
    ELED_01 = 6,
    ELED_02 = 1,
    ELED_10 = 4,
    ELED_11 = 3,
    ELED_12 = 5,
    ELED_TR = 2,    // --> D13. CAPSLOCK
    ELED_TL = 0     // --> D14. NUMLOCK
};

#endif
