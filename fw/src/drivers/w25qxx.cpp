#include "w25qxx.h"
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include <string.h>

/**
 * Macros to switch fast-mode, optimizable at compile-time.
 */
#if W25QXX_DISABLE_FASTMODE == 0
#define W25QXX_IS_FASTMODE  fastMode()
#define W25QXX_READ_CMD     (fastMode() ? 0x0b : 0x03)
#elif W25QXX_DEFAULT_FASTMODE == 0
#define W25QXX_IS_FASTMODE  false
#define W25QXX_READ_CMD     0x03
#else
#define W25QXX_IS_FASTMODE  true
#define W25QXX_READ_CMD     0x0b
#endif

/**
 * W25Qxx model precomputed Block-Count table.
 * bcnt = 2 ^ ((n - MODEL_BASE) + 1).
 */
const uint16_t W25QXX::MODELS[MODEL_MAX] = {
    2,      // W25Q10,  0x4011      0
    4,      // W25Q20,  0x4012      1
    8,      // W25Q40,  0x4013      2
    16,     // W25Q80,  0x4014      3
    32,     // W25Q16,  0x4015      4
    64,     // W25Q32,  0x4016      5
    128,    // W25Q64,  0x4017      6
    256,    // W25Q128, 0x4018      7
    512,    // W25Q256, 0x4019      8
    1024,   // W25Q512, 0x401A      9
};

W25QXX::W25QXX(spi_inst_t* dev, uint8_t csn, uint8_t clk, uint8_t miso, uint8_t mosi)
    : _dev(dev), _csn(csn), _clk(clk), _miso(miso), _mosi(mosi), _init(0), _sel(0), _id(0), _bcnt(0)
{
}

bool W25QXX::init() {
    if (!_dev) {
        return false;
    }

    if (_init == 0) {
        _init = 1;
        configure();
    }

    // --> reset the block count.
    _bcnt = 0;

    // --> read ID.
    disableWrite();

    // --> check vendor id.
    if (((_id = readId()) & 0x0000ff00) != 0x4000) {
        return false;
    }

    uint8_t n = _id & 0x000000ff;
    if (n < MODEL_BASE) {
        return false;
    }

    // --> out of range.
    if ((n -= MODEL_BASE) >= MODEL_MAX) {
        return false;
    }

    // --> set the block count.
    _bcnt = MODELS[n];

    // --> read all status registers.
    for (uint8_t i = 0; i < 3; ++i) {
        readStatus(i + 1);
    }

    return true;
}

void W25QXX_InvertBits(uint8_t* buf, const uint8_t* src, uint32_t len) {
    for(uint32_t i = 0; i < len; ++i) {
        buf[i] = ~src[i];
    }
}

bool W25QXX::test() {
    init();

    // --> init fail.
    if (!_id || !_bcnt || !_dev) {
        return false;
    }

    uint8_t temp1[32], temp2[32];

    // --> read bytes.
    if (read(0, temp1, sizeof(temp1)) != sizeof(temp1)) {
        return false;
    }

    W25QXX_InvertBits(temp2, temp1, sizeof(temp1));
    if (write(0, temp2, sizeof(temp2)) != sizeof(temp2)) {
        write(0, temp1, sizeof(temp1)); // --> restore bytes.
        return false;
    }

    if (read(0, temp1, sizeof(temp1)) != sizeof(temp1)) {
        W25QXX_InvertBits(temp1, temp2, sizeof(temp1));
        write(0, temp1, sizeof(temp1)); // --> restore bytes.
        return false;
    }

    bool retval = (memcmp(temp1, temp2, sizeof(temp1)) == 0);
    W25QXX_InvertBits(temp1, temp2, sizeof(temp1));
    write(0, temp1, sizeof(temp1)); // --> restore bytes.
    return retval;
}

#if W25QXX_DISABLE_FASTMODE == 0
bool W25QXX::fastMode(bool val) {
    if (_init == 0 || !_dev) {
        return false;
    }

    #if W25QXX_DEFAULT_FASTMODE
        _init = val ? 1 : 2;
    #else
        _init = val ? 2 : 1;
    #endif
    return true;
}
#endif

void W25QXX::configure() {
    spi_init(_dev, BAUDRATE);
    spi_set_format(_dev, 8, CPOL, CPHA, SPI_MSB_FIRST);

    gpio_set_function(_clk, GPIO_FUNC_SPI);
    gpio_set_function(_miso, GPIO_FUNC_SPI);
    gpio_set_function(_mosi, GPIO_FUNC_SPI);

    gpio_init(_csn);
    gpio_put(_csn, 1);
    gpio_set_dir(_csn, GPIO_OUT);
}

void W25QXX::select() {
    if ((_sel++) == 0) {
        gpio_put(_csn, 0);
    }
}

void W25QXX::deselect() {
    if ((--_sel) == 0) {
        gpio_put(_csn, 1);
    }
}

uint8_t W25QXX::xfer(uint8_t data) {
    W25QXX_ChipSelect _(this);
    uint8_t read;

    spi_write_read_blocking(_dev, &data, &read, 1);
    return read;
}

uint32_t W25QXX::xfer(uint8_t* buf, uint32_t len) {
    W25QXX_ChipSelect _(this);
    return spi_read_blocking(_dev, DUMMY_BYTE, buf, len);
}

uint32_t W25QXX::xmit(const uint8_t* buf, uint32_t len) {
    W25QXX_ChipSelect _(this);
    return spi_write_blocking(_dev, buf, len);
}

uint32_t W25QXX::readId() {
    W25QXX_ChipSelect _(this);

    xfer(0x9f);

    uint8_t b0 = xfer(DUMMY_BYTE);
    uint8_t b1 = xfer(DUMMY_BYTE);
    uint8_t b2 = xfer(DUMMY_BYTE);

    return (uint32_t(b0) << 16)
         | (uint32_t(b1) << 8)
         | (uint32_t(b2) << 0)
        ;
}

uint32_t W25QXX::readUniqueId(uint8_t* buf, uint32_t len) {
    if (!_dev) {
        return 0;
    }

    if (len > 8) {
        len = 8;
    }

    if (buf && len) {
        W25QXX_ChipSelect _(this);

        xfer(0x4b);
        for(uint8_t i = 0; i < 4; ++i) {
            xfer(DUMMY_BYTE);
        }

        for(uint8_t i = 0; i < len; ++i) {
            buf[i] = xfer(DUMMY_BYTE);
        }

        for(uint8_t i = len; i < 8; ++i) {
            xfer(DUMMY_BYTE);
        }
    }

    return len;
}

uint8_t W25QXX::readStatus(uint8_t number) {
    if (!_dev) {
        return 0xff;
    }
    
    W25QXX_ChipSelect _(this);

    switch(number) {
        case 1: xfer(0x05); break;
        case 2: xfer(0x35); break;
        case 3: xfer(0x15); break;
        default: return 0;
    }
    
    return xfer(DUMMY_BYTE);
}

bool W25QXX::writeStatus(uint8_t number, uint8_t val) {
    if (!_dev) {
        return false;
    }
    
    W25QXX_ChipSelect _(this);

    switch(number) {
        case 1: xfer(0x01); break;
        case 2: xfer(0x31); break;
        case 3: xfer(0x11); break;
        default: return false;
    }
    
    xfer(val);
    return true;
}

void W25QXX::waitForWrite() {
    if (!_dev) {
        return;
    }
    
    W25QXX_ChipSelect _(this);

    uint8_t status = 0xff;
    xfer(0x05);
    do {
        status = xfer(DUMMY_BYTE);
    }

    while((status & 0x01) != 0);
}

void W25QXX::eraseChip() {
    if (!_dev) {
        return;
    }
    
    waitForWrite();
    enableWrite();

    // --
    {
        W25QXX_ChipSelect _(this);

        xfer(0xc7);
    }
    
    waitForWrite();
    disableWrite();
}

void W25QXX::xferAddr(uint32_t addr) {
    W25QXX_ChipSelect _(this);

    if (_bcnt > 256) {
        xfer((addr >> 24) & 0xff);
    }

    xfer((addr >> 16) & 0xff);
    xfer((addr >> 8) & 0xff);
    xfer((addr >> 0) & 0xff);
}

uint8_t W25QXX::xferCmd(uint8_t cmd) {
    if (_bcnt > 256) {
        switch(cmd) {
            case 0x02: cmd = 0x12; break; // write
            case 0x03: cmd = 0x13; break; // read
            case 0x0b: cmd = 0x0c; break; // fast-read.
            case 0x20: cmd = 0x21; break; // erase sector
            case 0xd8: cmd = 0xdc; break; // erase block
            default: break;
        }
    }

    return xfer(cmd);
}

bool W25QXX::eraseSector(uint32_t sector) {
    const uint32_t sectorMax = this->sectorMax();
    if (sector >= sectorMax) {
        return false;
    }

    // --> convert to sector address.
    sector *= SECTOR_SIZE;
    waitForWrite();
    enableWrite();

    // --
    {
        W25QXX_ChipSelect _(this);

        xferCmd(0x20);
        xferAddr(sector);
    }
    
    waitForWrite();
    disableWrite();

    return true;
}

bool W25QXX::eraseBlock(uint32_t block) {
    if (block >= _bcnt) {
        return false;
    }

    block *= BLOCK_SIZE;
    waitForWrite();
    enableWrite();

    // --
    {
        W25QXX_ChipSelect _(this);

        xferCmd(0xd8);
        xferAddr(block);
    }
    
    waitForWrite();
    disableWrite();
    
    return true;
}

bool W25QXX::writeByte(uint32_t addr, uint8_t val) {
    if (addr >= capacity()) {
        return false;
    }

    waitForWrite();
    enableWrite();

    {
        W25QXX_ChipSelect _(this);

        xferCmd(0x02);
        xferAddr(addr);

        xfer(val);
    }

    waitForWrite();
    disableWrite();
    
    return true;
}

uint32_t W25QXX::write(uint32_t addr, const uint8_t* buf, uint32_t len) {
    if (addr >= capacity() || len <= 0) {
        return 0;
    }
    
    uint32_t done = 0;
    uint32_t page = addr / PAGE_SIZE;
    uint32_t offset = addr - (page * PAGE_SIZE);

    while (len > done) {
        const uint32_t left = len - done;
        uint32_t slice = PAGE_SIZE - offset;
        
        if (slice > left) {
            slice = left;
        }

        slice = writePage(page, offset, buf, slice);

        buf += slice;        
        done += slice;
        page ++;

        offset = 0;
    }

    return done;
}

uint32_t W25QXX::writePage(uint32_t page, uint32_t offset, const uint8_t* buf, uint32_t len) {
    if (page >= pageMax() || offset >= PAGE_SIZE || len <= 0) {
        return 0;
    }

    if (len > PAGE_SIZE - offset) {
        len = PAGE_SIZE - offset;
    }
    
    page *= PAGE_SIZE;
    page += offset;

    waitForWrite();
    enableWrite();

    {
        W25QXX_ChipSelect _(this);

        xferCmd(0x02);
        xferAddr(page);

        len = xmit(buf, len);
    }

    waitForWrite();
    disableWrite();
    
    return len;
}

uint32_t W25QXX::writeSector(uint32_t sector, uint32_t offset, const uint8_t* buf, uint32_t len) {
    if (sector >= sectorMax() || offset >= SECTOR_SIZE || len <= 0) {
        return 0;
    }

    if (len > SECTOR_SIZE - offset) {
        len = SECTOR_SIZE - offset;
    }
    
    sector *= SECTOR_SIZE;
    sector += offset;

    return write(sector, buf, len);
}

uint32_t W25QXX::writeBlock(uint32_t block, uint32_t offset, const uint8_t* buf, uint32_t len) {
    if (block >= _bcnt || offset >= BLOCK_SIZE || len <= 0) {
        return 0;
    }

    if (len > BLOCK_SIZE - offset) {
        len = BLOCK_SIZE - offset;
    }
    
    block *= BLOCK_SIZE;
    block += offset;
    
    return write(block, buf, len);
}

bool W25QXX::readByte(uint32_t addr, uint8_t* val) {
    if (addr >= capacity()) {
        return false;
    }

    W25QXX_ChipSelect _(this);

    xferCmd(W25QXX_READ_CMD);
    xferAddr(addr);

    if (W25QXX_IS_FASTMODE) {
        xfer(0x00); // --> 0x0b, 0x0c requires dummy byte.
    }

    uint8_t temp = xfer(DUMMY_BYTE);

    if (val) {
        *val = temp;
    }

    return true;
}

uint32_t W25QXX::read(uint32_t addr, uint8_t* buf, uint32_t len) {
    const uint32_t cap = capacity();

    if (addr >= cap || len <= 0) {
        return 0;
    }

    if (len > cap - addr) {
        len = cap - addr;
    }

    W25QXX_ChipSelect _(this);

    xferCmd(W25QXX_READ_CMD);
    xferAddr(addr);

    if (W25QXX_IS_FASTMODE) {
        xfer(0x00); // --> 0x0b, 0x0c requires dummy byte.
    }

    return xfer(buf, len);
}

uint32_t W25QXX::readPage(uint32_t page, uint32_t offset, uint8_t* buf, uint32_t len) {
    if (page >= pageMax() || offset >= PAGE_SIZE || len <= 0) {
        return 0;
    }

    if (len > PAGE_SIZE - offset) {
        len = PAGE_SIZE - offset;
    }
    
    page *= PAGE_SIZE;
    page += offset;

    return read(page, buf, len);
}

uint32_t W25QXX::readSector(uint32_t sector, uint32_t offset, uint8_t* buf, uint32_t len) {
    if (sector >= sectorMax() || offset >= SECTOR_SIZE || len <= 0) {
        return 0;
    }

    if (len > SECTOR_SIZE - offset) {
        len = SECTOR_SIZE - offset;
    }
    
    sector *= SECTOR_SIZE;
    sector += offset;

    return read(sector, buf, len);
}

uint32_t W25QXX::readBlock(uint32_t block, uint32_t offset, uint8_t* buf, uint32_t len) {
    if (block >= _bcnt || offset >= BLOCK_SIZE || len <= 0) {
        return 0;
    }

    if (len > BLOCK_SIZE - offset) {
        len = BLOCK_SIZE - offset;
    }
    
    block *= BLOCK_SIZE;
    block += offset;
    
    return read(block, buf, len);
}