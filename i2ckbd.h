#ifndef I2CKBD_H
#define I2CKBD_H

#include <stdint.h>

// PicoCalc toetsenbord: STM32 co-processor als I2C-slave op I2C1.
// Key states (low byte van het 2-byte antwoord op register 0x09):
#define KBD_STATE_IDLE     0
#define KBD_STATE_PRESSED  1
#define KBD_STATE_HOLD     2
#define KBD_STATE_RELEASED 3

void kbd_init(void);

// Leest een key-event uit de FIFO.
// Retourneert 1 als er een event was (state/code ingevuld), anders 0.
int kbd_read(uint8_t *state, uint8_t *code);

#endif // I2CKBD_H
