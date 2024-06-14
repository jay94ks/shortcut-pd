#include "usbd.h"
#ifdef __INTELLISENSE__
#include <tusb_config.h>
#define CFG_TUD_EXTERN
#endif
#include <tusb.h>

static bool g_usbdIsMounted = false;
static bool g_usbdIsResetRequired = false;

CFG_TUD_EXTERN void tud_mount_cb(void) {
    g_usbdIsMounted = true;
    g_usbdIsResetRequired = true;
}

CFG_TUD_EXTERN void tud_umount_cb(void) {
    g_usbdIsMounted = false;
}

bool usbdIsMounted() {
    return g_usbdIsMounted;
}

bool usbdIsResetRequired() {
    return g_usbdIsResetRequired;
}

void usbdResetNow() {
    g_usbdIsResetRequired = false;
}