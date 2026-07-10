#pragma once
#include <stdint.h>

uint8_t ram_read(void *context, uint16_t address);
void ram_write(void *context, uint16_t address, uint8_t value);