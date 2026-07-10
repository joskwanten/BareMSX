#include "rom.h"
#include "pico.h"

uint8_t __not_in_flash_func(rom_read)(void *context, uint16_t address)
{
    return ((uint8_t *)context)[address];
}

void rom_write(void *context, uint16_t address, uint8_t value)
{
    // Just empty since it is a ROM
}