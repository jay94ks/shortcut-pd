#ifndef __DRIVERS_USBD_CDC_H__
#define __DRIVERS_USBD_CDC_H__

#include <stdint.h>

/**
 * CDC message structure.
 */ 
struct SCdcMessage {
    uint8_t opcode;
    uint8_t length;
    uint8_t data[64];
    uint8_t checksum;
};

enum {
    ECDCM_ECHO = 0,
    ECDCM_GET_KEYS,
    ECDCM_SET_KEYS,
    ECDCM_RESET_KEYS,
    ECDCM_SAVE_CONF,
    ECDCM_CHECK_CAPTURE,
    ECDCM_ENTER_CAPTURE,
    ECDCM_LEAVE_CAPTURE,
    ECDCM_KEY_REPORT,
    ECDCM_REBOOT,
    ECDCM_UPLOAD,
};

/**
 * Usb CDC transceiver.
*/
class UsbCdc {
public:
    static constexpr uint16_t MAX_BUF = 512;
    
private:
    uint8_t _wbuf[MAX_BUF];
    uint8_t _rbuf[MAX_BUF];

    uint16_t _wpos;
    uint16_t _rpos;

public:
    UsbCdc();
    ~UsbCdc();

public:
    void updateOnce();

private:
    void receiveOnce();
    void transmitOnce();

public:
    /* reset the CDC transceive-buffers. */
    void reset();

    /* read bytes from the buffer. */
    uint32_t read(uint8_t* buf, uint32_t len);

    /* write bytes into the buffer. */
    uint32_t write(const uint8_t* buf, uint32_t len);

    /* compute checksum of SCdcMessage. */
    uint8_t checksum(const SCdcMessage& msg);

    /* read a message. */
    bool read(SCdcMessage& msg);

    /* write a message. */
    bool write(const SCdcMessage& msg);
};

#endif