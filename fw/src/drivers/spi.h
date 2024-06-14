#ifndef __DRIVERS_SPI_H__
#define __DRIVERS_SPI_H__

#include <stdint.h>
#include "hardware/spi.h"

/**
 * SPI class.
 */
class SPI {
private:
    static constexpr uint8_t INV_CS = 0xff;

    /**
     * generate nano seconds delay.
     * this is used to ensure GPIO pin state to be applied.
     */
    static void __attribute__((optimize("O0"))) delayNs() {
        for(uint32_t i = 0; i < 10; ++i);
    }

public:
    /* initialization parameters. */
    struct Params {
        uint8_t rx;
        uint8_t tx;
        uint8_t cs;
        uint8_t clk;
        uint32_t hz;
        uint8_t bits;
        spi_cpol_t cpol;   // --> polarity.
        spi_cpha_t cpha;   // --> phase.
        spi_order_t order;  // --> order, SPI_MSB_FIRST.
    };

    /* default parameters.*/
    static constexpr Params PARAMS_DEFAULT = {
        0, 0, 0, 0,     // --> rx, tx, cs, clk.
        100,
        // 4000 * 100,    // --> 1 MHz.
        8,              // --> Number of bits per transfer.
        SPI_CPOL_1,     // --> Polarity (CPOL).
        SPI_CPHA_1,     // --> Phase (CPHA).
        SPI_MSB_FIRST
    };

public:
    SPI() : _spi(nullptr), _cs(INV_CS) { }
    SPI(spi_inst_t* spi) : SPI(spi, INV_CS) { }
    SPI(spi_inst_t* spi, uint8_t cs) : _spi(spi), _cs(cs) { }
    SPI(spi_inst_t* spi, uint8_t rx, uint8_t tx, uint8_t cs, uint8_t clk);
    SPI(spi_inst_t* spi, const Params& params);
    SPI(uint8_t spi, uint8_t rx, uint8_t tx, uint8_t cs, uint8_t clk);
    SPI(uint8_t spi, const Params& params);

private:
    spi_inst_t* _spi;
    uint8_t _cs;

private:
    void prepare(spi_inst_t* spi, const Params& params);

public:
    /* returns whether the SPI is null device or not. */
    inline bool isNull() const { return _spi == nullptr; }

    /* get the raw SPI instance pointer. */
    inline spi_inst_t* raw() const { return _spi; }

public:
    void select();
    void deselect();

public:
    /* transmit bytes to SPI line. */
    uint32_t tx(const uint8_t* buf, int32_t len);

    /* receive bytes from SPI line. */
    uint32_t rx(uint8_t* buf, int32_t len);

    /* transmit bytes and receive bytes sequencially. */
    uint32_t txrx(const uint8_t* wbuf, uint8_t* rbuf, int32_t len, bool ctlCs = true);

    /* transmit bytes and receive bytes sequencially. */
    uint32_t txrx(const uint8_t* wbuf, int32_t wlen, uint8_t* rbuf, int32_t rlen, bool ctlCs = true);
    
    /* transmit both bytes sequencially. */
    uint32_t txtx(const uint8_t* wbuf, int32_t wlen, const uint8_t* rbuf, int32_t rlen, bool ctlCs = true);
};

#endif
