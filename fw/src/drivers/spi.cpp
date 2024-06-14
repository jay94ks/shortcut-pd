#include "SPI.h"
#include "hardware/gpio.h"
#include "hardware/spi.h"

SPI::SPI(spi_inst *spi, uint8_t rx, uint8_t tx, uint8_t cs, uint8_t clk)
    : _spi(spi), _cs(cs)
{
    Params params = PARAMS_DEFAULT;

    params.rx = rx;
    params.tx = tx;
    params.cs = cs;
    params.clk = clk;

    prepare(spi, params);
}

SPI::SPI(spi_inst_t* spi, const Params& params)
    : _spi(spi), _cs(params.cs)
{
    prepare(_spi, params);
}

SPI::SPI(uint8_t spi, uint8_t rx, uint8_t tx, uint8_t cs, uint8_t clk)
    : _spi(nullptr), _cs(cs)
{
    if (spi < 2) {
        Params params = PARAMS_DEFAULT;

        _spi = spi ? spi1 : spi0;
        params.rx = rx;
        params.tx = tx;
        params.cs = cs;
        params.clk = clk;

        prepare(_spi, params);
    }
}

SPI::SPI(uint8_t spi, const Params& params)
    : _spi(nullptr), _cs(params.cs)
{
    if (spi < 2) {
        _spi = spi ? spi1 : spi0;
        prepare(_spi, params);
    }
}

void SPI::prepare(spi_inst *spi, const Params& params) {
    const uint8_t cs = params.cs;

    if (cs != INV_CS) {
        gpio_init(cs);
        gpio_set_dir(cs, GPIO_OUT);
        gpio_put(cs, 1); // --> because, active low.
    }

    // --> init gpio.
    gpio_init(params.clk);
    gpio_init(params.rx);
    gpio_init(params.tx);
    
    gpio_put(params.clk, 1);
    gpio_put(params.rx, 1);
    gpio_put(params.tx, 1);

    gpio_set_dir(params.clk, GPIO_OUT);
    gpio_set_dir(params.rx, GPIO_IN);
    gpio_set_dir(params.tx, GPIO_OUT);

    // --> configure SPI port.
    spi_init(spi, params.hz);
    spi_set_format(spi,
        params.bits,
        params.cpol,
        params.cpha,
        params.order);

    // --> enable SPI pin functionalities.
    gpio_set_function(params.clk, GPIO_FUNC_SPI);
    gpio_set_function(params.rx, GPIO_FUNC_SPI);
    gpio_set_function(params.tx, GPIO_FUNC_SPI);
}

void SPI::select() {
    if (_cs == INV_CS) {
        return;
    }

    gpio_put(_cs, 0);
    delayNs();
}

void SPI::deselect() {
    if (_cs == INV_CS) {
        return;
    }

    gpio_put(_cs, 1);
    delayNs();
}

uint32_t SPI::tx(const uint8_t* buf, int32_t len) {
    if (isNull() || len < 0) {
        return 0;
    }

    select();
    len = spi_write_blocking(_spi, buf, len);
    deselect();

    return uint32_t(len < 0 ? 0 : len);
}

uint32_t SPI::rx(uint8_t* buf, int32_t len) {
    if (isNull() || len < 0) {
        return 0;
    }

    select();
    len = spi_read_blocking(_spi, 0, buf, len);
    deselect();

    return uint32_t(len < 0 ? 0 : len);
}

uint32_t SPI::txrx(const uint8_t* wbuf, uint8_t* rbuf, int32_t len, bool ctlCs) {
    if (isNull() || len < 0) {
        return 0;
    }

    if (ctlCs) {
        select();
    }

    len = spi_write_read_blocking(_spi, wbuf, rbuf, len);
    
    if (ctlCs) {
        deselect();
    }

    return uint32_t(len < 0 ? 0 : len);
}

uint32_t SPI::txrx(const uint8_t* wbuf, int32_t wlen, uint8_t* rbuf, int32_t rlen, bool ctlCs) {
    if (isNull() || wlen < 0 || rlen < 0) {
        return 0;
    }

    if (ctlCs) {
        select();
    }
    
    if (spi_write_blocking(_spi, wbuf, wlen) != wlen) {
        if (ctlCs) {
            deselect();
        }

        return 0;
    }

    rlen = spi_read_blocking(_spi, 0, rbuf, rlen);
    if (ctlCs) {
        deselect();
    }

    return uint32_t(rlen < 0 ? 0 : rlen);
}

uint32_t SPI::txtx(const uint8_t *wbuf, int32_t wlen, const uint8_t *rbuf, int32_t rlen, bool ctlCs)
{
    if (isNull() || wlen < 0 || rlen < 0) {
        return 0;
    }

    if (ctlCs) {
        select();
    }

    if ((wlen = spi_write_blocking(_spi, wbuf, wlen)) <= 0) {
        if (ctlCs) {
            deselect();
        }

        return wlen < 0 ? 0 : wlen;
    }

    rlen = spi_write_blocking(_spi, rbuf, rlen);

    if (ctlCs) {
        deselect();
    }

    return uint32_t(rlen < 0 ? 0 : rlen + wlen);
}
