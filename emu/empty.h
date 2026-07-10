#pragma once
#include <stdint.h>

uint8_t empty_read(void *context, uint16_t address);
void empty_write(void *context, uint16_t address, uint8_t value);