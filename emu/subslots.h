#pragma once

#include <stdint.h>
#include "memory.h"

typedef struct
{
    uint8_t subslot_register;
    memory_context_t subslots[4];
} subslot_context_t;

uint8_t subslots_read(void *subslot_context, uint16_t address);
void subslots_write(void *subslot_context, uint16_t address, uint8_t value);
void subslots_add_subslot(subslot_context_t *subslot_context, uint32_t subslot, void *context, read_byte_t read_func, write_byte_t write_func);