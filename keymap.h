#ifndef KEYMAP_H
#define KEYMAP_H

#include <stdint.h>

// Vertaalt een PicoCalc key-event (state + keycode) naar de MSX PPI-matrix
// en roept machine_keydown/keyup aan.
void keymap_handle(uint8_t state, uint8_t code);

#endif // KEYMAP_H
