#pragma once
#include <stdint.h>
#include "memory.h"

typedef struct {
  uint32_t selected_slots[4];
  memory_context_t slots[4];
} slots_context_t;

uint8_t slots_read(void* slots_context, uint16_t address);
void slots_write(void* slots_context, uint16_t address, uint8_t value);

void slots_set_slot_register(slots_context_t* slots_context, uint8_t value);
uint8_t slots_get_slot_register(slots_context_t* slots_context);
void slots_add_slot(slots_context_t* slots_context, uint32_t slot, void* context, read_byte_t read_func, write_byte_t write_func);