#include "rom.h"
#include "pico.h"

uint8_t __not_in_flash_func(empty_read)(void *context, uint16_t address)
{
    return 0xff;
}

void empty_write(void *context, uint16_t address, uint8_t value)
{
    // Just empty since it is empty
}