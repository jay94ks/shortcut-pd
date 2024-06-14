#ifndef __W25QXX_W25QXX_H__
#define __W25QXX_W25QXX_H__

#include <stdint.h>
#include <hardware/spi.h>

/**
 * W25QXX SPI flash driver configurations.
 * 1. W25QXX_DISABLE_TEST : strips `test` method out.
 * 2. W25QXX_DEFAULT_FASTMODE : make default operation mode to fast-mode.
 * 3. W25QXX_DISABLE_FASTMODE : strips `fastMode(val)` method out.
 */
#ifndef W25QXX_DISABLE_TEST
#define W25QXX_DISABLE_TEST 0
#endif

#ifndef W25QXX_DEFAULT_FASTMODE
#define W25QXX_DEFAULT_FASTMODE 0
#endif

#ifndef W25QXX_DISABLE_FASTMODE
#define W25QXX_DISABLE_FASTMODE 0
#endif

// --> forward decl.
class W25QXX_ChipSelect;

/**
 * W25QXX SPI flash driver.
 * --
 * Datasheets:
 * https://www.elinux.org/images/f/f5/Winbond-w25q32.pdf
 * https://www.mouser.com/datasheet/2/949/Winbond_07022021_W25Q512NW-2451091.pdf
 */
class W25QXX {
    friend class W25QXX_ChipSelect;

private:
    /**
     * Constants.
     */
    static constexpr uint32_t PAGE_SIZE = 256;
    static constexpr uint32_t SECTOR_SIZE = 0x1000;
    static constexpr uint32_t BLOCK_SIZE = 0x10000;
    static constexpr uint8_t DUMMY_BYTE = 0xa5;
    
    /**
     * W25Qxx model precomputed Block-Count table.
     * bcnt = 2 ^ ((n - MODEL_BASE) + 1).
     */
    static constexpr uint16_t MODEL_MAX = 10;
    static constexpr uint16_t MODEL_BASE = 0x11;    // --> starting offset of model number.
    static const uint16_t MODELS[MODEL_MAX];

    /**
     * SPI baud-rate, CPOL and CPHA.
     * RP2040 doesn't support LSB first mode, so it can not be customized.
     */
    static constexpr uint32_t BAUDRATE = 1000 * 1000; // --> 1MHz.    
    static constexpr spi_cpol_t CPOL = SPI_CPOL_1;
    static constexpr spi_cpha_t CPHA = SPI_CPHA_1;

private:
    spi_inst_t* _dev;

    uint8_t _csn, _clk;
    uint8_t _miso, _mosi;

    uint8_t _init;
    int16_t _sel;

    uint32_t _id;
    uint32_t _bcnt;

    /**
     * Note for SPI device:
     * --
     * Null check for erase, write and read methods are not needed.
     * Because, driver will never be initialized if null device specified,
     * 
     * When init fail, _bcnt = 0, then:
     *  1. capacity() = 0.
     *  2. sectorMax() = 0.
     *  3. pageMax() = 0.
     *  4. blockMax() = 0. (== _bcnt)
     * 
     * So, all addressing(page, sector and block) code will cover them gracefully.
     */

public:
    /**
     * Initialize a new W25QXX instance.
     */
    W25QXX(spi_inst_t* dev, uint8_t csn, uint8_t clk, uint8_t miso, uint8_t mosi);
    
    /**
     * Initialize a new W25QXX instance.
     */
    W25QXX(uint8_t bus, uint8_t csn, uint8_t clk, uint8_t miso, uint8_t mosi)
        : W25QXX(bus == 0 ? spi0 : (bus == 1 ? spi1 : nullptr), csn, clk, miso, mosi) { }

public:
    /**
     * Test whether the W25QXX initialized on NULL device or not.
     */
    inline bool isNull() const {
        return _dev == nullptr;
    }

    /**
     * Initialize the W25Qxx driver.
    */
    bool init();

    /**
     * Test the flash chip.
     */
    bool test();

    /**
     * Test whether the flash chip must work fast-mode or not.
    */
    inline bool fastMode() const {
        #if W25QXX_DEFAULT_FASTMODE == 1
            return _init == 1;
        #else
            return _init == 2;
        #endif
    }

#if W25QXX_DISABLE_FASTMODE == 0
    /**
     * set the fast-mode.
     * if the fast-mode set, 0x0b will be used for read.
     */
    bool fastMode(bool val);
#else
    /**
     * set the fast-mode, but this version, not supported.
     * and provided to keep source-code compatibility.
     * returns always false.
     */
    inline bool fastMode(bool) { return false; }
#endif

    /**
     * Get the capacity of the flash.
     * This will be valid after calling `init()` method.
     */
    inline uint32_t capacity() const {
        return _bcnt * BLOCK_SIZE;
    }

    /**
     * Get the max sector count.
     * This will be valid after calling `init()` method.
     */
    inline uint32_t sectorMax() const {
        return _bcnt * 16;
    }

    /**
     * Get the max block count.
     * This will be valid after calling `init()` method.
     */
    inline uint32_t blockMax() const {
        return _bcnt;
    }

    /**
     * Get the max page count.
     * This will be valid after calling `init()` method.
     */
    inline uint32_t pageMax() const {
        return capacity() / PAGE_SIZE;
    }

private:
    /**
     * Configure all GPIO and SPI interfaces.
     * This must be called only once.
     */
    void configure();

protected:
    /**
     * Chip select, this makes CSN pin to low.
     * And this managed by counter for recursive-call protection.
     */
    void select();

    /**
     * Chip de-select, this makes CSN pin to high.
     * And this managed by counter for recursive-call protection.
     */
    void deselect();

private:
    /* write data and read one byte. */
    uint8_t xfer(uint8_t data);

    /* read multiple bytes from SPI. */
    uint32_t xfer(uint8_t* buf, uint32_t len);

    /* write data through SPI. */
    uint32_t xmit(const uint8_t* buf, uint32_t len);

private:
    /**
     * Enable write.
     * Cmd: 0x06.
     */
    void enableWrite() { xfer(0x06); }

    /**
     * Disable write.
     * Cmd: 0x04.
     */
    void disableWrite() { xfer(0x04); }

    /**
     * Read ID. (JEDECID)
     * Cmd: 0x9f.
     */
    uint32_t readId();

public:
    /**
     * Read the Unique ID and returns length.
     * Cmd: 0x4b.
     */
    uint32_t readUniqueId(uint8_t* buf, uint32_t len);

    /**
     * Read status register #1 ~ #3.
     * Cmd: 0x05 (#1), 0x35 (#2), 0x15 (#3).
    */
    uint8_t readStatus(uint8_t number);

    /**
     * Write status register #1 ~ #3.
     * Cmd: 0x01 (#1), 0x31 (#2), 0x11 (#3).
     */
    bool writeStatus(uint8_t number, uint8_t val);

    /**
     * Wait for write to be completed.
     * Cmd: 0x05.
    */
    void waitForWrite();

    /**
     * Erase the chip.
     * Cmd: 0xc7.
     */
    void eraseChip();
    
private:
    /**
     * Xfer a command byte to the chip.
     * This translate 3-Byte based command to 4-Byte command if required.
     */
    uint8_t xferCmd(uint8_t cmd);

    /**
     * Xfer an address to the chip.
     */
    void xferAddr(uint32_t addr);

public:

    /**
     * Erase a sector.
     * Cmd: 0x20 (24-bit), 0x21 (32-bit).
    */
    bool eraseSector(uint32_t sector);

    /**
     * Erase a block.
     * Cmd: 0xd8 (24-bit), 0xdc (32-bit).
     */
    bool eraseBlock(uint32_t block);

    /**
     * Write a byte.
     * Cmd: 0x02 (24-bit), 0x12 (32-bit).
     */
    bool writeByte(uint32_t addr, uint8_t val);

    /**
     * Write multiple bytes and returns written bytes.
     * Uses: writePage(...) method.
     */
    uint32_t write(uint32_t addr, const uint8_t* buf, uint32_t len);

    /**
     * Write a page and returns written bytes.
     * Cmd: 0x02 (24-bit), 0x12 (32-bit).
     */
    uint32_t writePage(uint32_t page, uint32_t offset, const uint8_t* buf, uint32_t len);
    
    /**
     * Write a sector and returns written bytes.
     * Uses: write(...) method.
     */
    uint32_t writeSector(uint32_t sector, uint32_t offset, const uint8_t* buf, uint32_t len);

    /**
     * Write a block and returns written bytes.
     * Uses: write(...) method.
     */
    uint32_t writeBlock(uint32_t block, uint32_t offset, const uint8_t* buf, uint32_t len);

    /**
     * Read a byte.
     * Cmd: 0x03 (24-bit), 0x13 (32-bit, fast), 0x0b (24-bit), 0x0c (32-bit, fast).
     */
    bool readByte(uint32_t addr, uint8_t* val);

    /**
     * Read multiple bytes and returns read bytes.
     * Cmd: 0x03 (24-bit), 0x13 (32-bit, fast), 0x0b (24-bit), 0x0c (32-bit, fast).
     */
    uint32_t read(uint32_t addr, uint8_t* buf, uint32_t len);

    /**
     * Read a page and returns read bytes.
     * Uses: read(...) method.
    */
    uint32_t readPage(uint32_t page, uint32_t offset, uint8_t* buf, uint32_t len);

    /**
     * Read a sector and returns read bytes.
     * Uses: read(...) method.
     */
    uint32_t readSector(uint32_t sector, uint32_t offset, uint8_t* buf, uint32_t len);

    /**
     * Read a block and returns read bytes.
     * Uses: read(...) method.
     */
    uint32_t readBlock(uint32_t block, uint32_t offset, uint8_t* buf, uint32_t len);
    
public:
    /**
     * Read a structure and returns true if full bytes loaded. 
     */
    template<typename T>
    bool read(uint32_t addr, T* buf) {
        return read(addr, (uint8_t*) buf, sizeof(T)) == sizeof(T);
    }

    /**
     * Write a structure and returns true if full bytes stored. 
     */
    template<typename T>
    bool write(uint32_t addr, const T* buf) {
        return write(addr, (const uint8_t*) buf, sizeof(T)) == sizeof(T);
    }
};

/**
 * Make CSN to low when the statement context started,
 * And make CSN to high when the statement context ended.
 * Aka, this controls CSN automatically by the statement context state.
 * 
 * --
 * Usage: 
 *  void test() {
 *    W25QXX_ChipSelect _(&w25qxx);
 *  
 *    // --> do something here.
 *  }
 */
class W25QXX_ChipSelect {
private:
    W25QXX* _w25qxx;

public:
    /* ctor: select the target W25QXX chip. */
    W25QXX_ChipSelect(W25QXX* w25qxx) : _w25qxx(w25qxx) {
        w25qxx->select();
    }

    /* dtor: deselect the target W25QXX chip. */
    ~W25QXX_ChipSelect() { _w25qxx->deselect(); }
};

#endif