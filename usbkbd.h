#ifndef USBKBD_H
#define USBKBD_H

#include <stdint.h>
#include <stdbool.h>

// USB keyboard input via TinyUSB host on the RP2350 native USB port.
// Translates USB HID keycodes into the MSX key matrix (machine_keydown/keyup).

void usbkbd_init(void); // initialise the TinyUSB host stack
void usbkbd_task(void); // pump the host stack; call often (e.g. every frame)

// Menu mode: while on, keypresses are queued as menu navigation events
// instead of driving the MSX matrix. usbkbd_menu_poll() returns a
// menu_input_t, of USBKBD_MENU_CHAR_BASE | teken voor letters/cijfers
// (het zoekveld van de browse-lijst), of -1 als de queue leeg is.
#define USBKBD_MENU_CHAR_BASE 0x100
void usbkbd_menu_mode(bool on);
int usbkbd_menu_poll(void);

// Hotkey: F12 (diskwissel). True precies één keer per druk (clear-on-read).
bool usbkbd_swap_requested(void);

// Hotkey: F11 (reset naar het bootmenu). Clear-on-read.
bool usbkbd_reset_requested(void);

#endif // USBKBD_H
