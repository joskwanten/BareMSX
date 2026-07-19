#ifndef DISKROM_H
#define DISKROM_H

// Slot-device voor de disk-interface: DISK.ROM op 0x4000-0x7FFF met de
// WD2793-registers memory-mapped over de laatste 8 bytes (0x7FF8-0x7FFF),
// zoals in de Philips-machines. Gaat in primair slot 2.

#include <stdint.h>
#include "wd2793.h"

typedef struct {
    const uint8_t *rom;   // DISK.ROM-image (typisch 16KB)
    uint32_t rom_size;
    wd2793_t fdc;
} diskrom_t;

uint8_t diskrom_read(void *context, uint16_t address);
void diskrom_write(void *context, uint16_t address, uint8_t value);

#endif // DISKROM_H
