#include "rom.h"
#include "pico.h"

uint8_t __not_in_flash_func(ram_read)(void *context, uint16_t address)
{
    return ((uint8_t *)context)[address];
}

void __not_in_flash_func(ram_write)(void *context, uint16_t address, uint8_t value)
{
    ((uint8_t *)context)[address] = value;
}
