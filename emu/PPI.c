#include "PPI.h"

void ppi_on_keydown(ppi_context_t* context, uint32_t index)
{
    context->keydown[index] = true;
}

void ppi_on_keyup(ppi_context_t* context, uint32_t index)
{
    context->keydown[index] = false;
}

uint8_t ppi_read_a9(ppi_context_t* context) 
{
    uint8_t value = 0;
    uint32_t offset = context->keyboard_row * 8;
    for(uint32_t i = 0; i < 8; i++) {
        if (context->keydown[offset + i]) {
            value |= 1 << i;
        }
    }

    return ~value;
}

void ppi_write_aa(ppi_context_t* context, uint8_t value)
{
    context->keyboard_row = value & 0xf;
}