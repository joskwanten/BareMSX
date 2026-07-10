#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t keyboard_row;
    bool keydown[16 * 8];
} ppi_context_t;

void ppi_on_keydown(ppi_context_t* context, uint32_t index);
void ppi_on_keyup(ppi_context_t* context, uint32_t index);
uint8_t ppi_read_a9(ppi_context_t* context);
void ppi_write_aa(ppi_context_t* context, uint8_t value);