#ifndef MAPPEDRAM_H
#define MAPPEDRAM_H

// MSX2 memory mapper: een RAM-pool geadresseerd via vier 16KB-pagina's,
// geselecteerd met I/O-poorten 0xFC-0xFF (pagina voor CPU-page 0-3).
// De NMS-8245 heeft 128KB; de BIOS bepaalt de omvang zelf door te proben.
// Slot-device: hangt (via subslot) in het RAM-slot; de poort-writes komen
// via mappedram_set_page vanuit de machine-I/O-dispatch.

#include <stdint.h>

#define MAPPEDRAM_SIZE (128 * 1024)
#define MAPPEDRAM_PAGES (MAPPEDRAM_SIZE / 0x4000)

typedef struct {
    uint8_t *pool;      // MAPPEDRAM_SIZE groot (extern beheerd: SRAM/PSRAM)
    uint8_t page[4];    // mapper-register per CPU-page (0xFC..0xFF)
} mappedram_t;

void mappedram_init(mappedram_t *m, uint8_t *pool);
void mappedram_set_page(mappedram_t *m, int cpu_page, uint8_t value); // 0xFC+n out
uint8_t mappedram_get_page(mappedram_t *m, int cpu_page);             // 0xFC+n in

// Slot-device-interface (context = mappedram_t*).
uint8_t mappedram_read(void *context, uint16_t address);
void mappedram_write(void *context, uint16_t address, uint8_t value);

#endif // MAPPEDRAM_H
