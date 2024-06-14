#include "cdc.h"
#include <string.h>

#ifdef __INTELLISENSE__
#include <tusb_config.h>
#define CFG_TUD_EXTERN
#endif
#include <tusb.h>

static bool g_usbCdcIntr = false;
static bool g_usbCdcEnable = false;

CFG_TUD_EXTERN void tud_cdc_rx_cb(uint8_t itf) {
    if (!g_usbCdcEnable) {
        tud_cdc_n_read_flush(0);
        return;
    }
    
    g_usbCdcIntr = true;
}

UsbCdc::UsbCdc() {
    _wpos = _rpos = 0;
    memset(_wbuf, 0, sizeof(_wbuf));
    memset(_rbuf, 0, sizeof(_rbuf));

    g_usbCdcEnable = true;
}

UsbCdc::~UsbCdc() {
    g_usbCdcEnable = false;
}

void UsbCdc::updateOnce() {
    receiveOnce();
    transmitOnce();
}

void UsbCdc::receiveOnce() {
    if (!g_usbCdcIntr) {
        return;
    }

    // --> set false to get more interrupts here.
    g_usbCdcIntr = false;
    uint32_t avail = MAX_BUF - _rpos;

    // --> if no buffer available, skip to handle it.
    if (avail <= 0) {
        g_usbCdcIntr = true;
        return;
    }
    
    uint8_t* buf = _rbuf + _rpos;
    uint32_t len = tud_cdc_n_read(0, buf, avail);
    if (len) {
        _rpos += len;
    }
}

void UsbCdc::transmitOnce() {
    uint8_t txbuf[sizeof(SCdcMessage) * 2];
    uint16_t txlen = _wpos;

    if (txlen > sizeof(txbuf)) {
        txlen = sizeof(txbuf);
    }

    if (txlen <= 0) {
        return;
    }
    
    memcpy(txbuf, _wbuf, txlen);
    while(true) {
        txlen = tud_cdc_n_write(0, txbuf, txlen);
        tud_cdc_n_write_flush(0);

        if (txlen) {
            break;
        }

        tud_task();
    }

    memcpy(txbuf, txbuf + txlen, _wpos - txlen);
    _wpos -= txlen;
}

void UsbCdc::reset() {
    _wpos = _rpos = 0;
}

uint32_t UsbCdc::read(uint8_t *buf, uint32_t len) {
    if (_rpos) {
        if (len > _rpos) {
            len = _rpos;
        }

        memcpy(buf, _rbuf, len);
        memcpy(_rbuf, _rbuf + _rpos, _rpos - len);

        _rpos -= len;
        return len;
    }

    return 0;
}

uint32_t UsbCdc::write(const uint8_t *buf, uint32_t len) {
    const uint16_t avail = MAX_BUF - _wpos;
    if (avail <= 0) {
        return 0;
    }

    if (len > avail) {
        len = avail;
    }

    memcpy(_wbuf + _wpos, buf, len);
    _wpos += len;

    return len;
}

uint8_t UsbCdc::checksum(const SCdcMessage& msg) {
    uint16_t sum = msg.opcode + msg.length;
    for(uint8_t i = 0; i < msg.length; ++i) {
        sum += msg.data[i];
    }

    return uint8_t(((sum & 0xff) ^ 0xff) + 1);
}

bool UsbCdc::read(SCdcMessage& msg) {
    if (_rpos < 3) {
        return false;
    }

    if (_rpos < 3 + _rbuf[1]) {
        return false;
    }

    const uint16_t total = 3 + _rbuf[1];
    
    msg.opcode = _rbuf[0];
    msg.length = _rbuf[1];
    
    memcpy(msg.data, _rbuf + 2, msg.length);
    msg.checksum = _rbuf[2 + msg.length];

    if (checksum(msg) != msg.checksum) {
        memcpy(_rbuf, _rbuf + 1, _rpos - 1);
        _rpos--;

        return false;
    }

    if ( _rpos - total > 0) {
        memcpy(_rbuf, _rbuf + total, _rpos - total);
    }
    
    _rpos -= total;
    return true;
}

bool UsbCdc::write(const SCdcMessage& msg) {
    transmitOnce();

    uint8_t checksum = this->checksum(msg);
    if (!write(&msg.opcode, sizeof(uint8_t))) {
        return false;
    }

    if (!write(&msg.length, sizeof(uint8_t))) {
        return false;
    }

    if (write(msg.data, msg.length) != msg.length) {
        return false;
    }

    return write(&checksum, sizeof(uint8_t) == sizeof(uint8_t));
}