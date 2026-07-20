#include "mappedram.h"
#include "pico.h"
#include <string.h>

void mappedram_init(mappedram_t *m, uint8_t *pool)
{
    m->pool = pool;
    memset(pool, 0, MAPPEDRAM_SIZE);
    // Power-on: BIOS verwacht de omgekeerde identiteit (3/2/1/0) zodat de
    // 64KB lineair aanvoelt: CPU-page 3 (stack) -> mapper-pagina 0, enz.
    for (int i = 0; i < 4; i++)
        m->page[i] = (uint8_t)(3 - i);
}

void mappedram_set_page(mappedram_t *m, int cpu_page, uint8_t value)
{
    m->page[cpu_page & 3] = (uint8_t)(value % MAPPEDRAM_PAGES);
}

uint8_t mappedram_get_page(mappedram_t *m, int cpu_page)
{
    // Echte mappers lezen terug met de ongebruikte hoge bits op 1.
    return (uint8_t)(m->page[cpu_page & 3] | (uint8_t)~(MAPPEDRAM_PAGES - 1));
}

uint8_t __not_in_flash_func(mappedram_read)(void *context, uint16_t address)
{
    mappedram_t *m = (mappedram_t *)context;
    return m->pool[((uint32_t)m->page[address >> 14] << 14) | (address & 0x3FFF)];
}

void __not_in_flash_func(mappedram_write)(void *context, uint16_t address, uint8_t value)
{
    mappedram_t *m = (mappedram_t *)context;
    m->pool[((uint32_t)m->page[address >> 14] << 14) | (address & 0x3FFF)] = value;
}
