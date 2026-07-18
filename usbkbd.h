#ifndef USBKBD_H
#define USBKBD_H

#include <stdint.h>

// USB keyboard input via TinyUSB host on the RP2350 native USB port.
// Translates USB HID keycodes into the MSX key matrix (machine_keydown/keyup).

void usbkbd_init(void); // initialise the TinyUSB host stack
void usbkbd_task(void); // pump the host stack; call often (e.g. every frame)

#endif // USBKBD_H
