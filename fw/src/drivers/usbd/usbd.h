#ifndef __DRIVERS_USBD_USBD_H__
#define __DRIVERS_USBD_USBD_H__

#include <stdint.h>

/**
 * test whether the USB is mounted or not.
 */
bool usbdIsMounted();

/**
 * test whether the USB is reset required or not.
 */
bool usbdIsResetRequired();

/**
 * reset the USB now.
 */
void usbdResetNow();

#endif