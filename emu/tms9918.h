#include <stdint.h>
#include <stdbool.h>

typedef void (*interrupt_func_t)();

typedef struct {
    uint8_t registers[8];
    uint8_t vram[0x4000];
    uint16_t vramAddress;
    uint8_t vdpStatus;
    uint8_t refreshRate;
    uint8_t lastRefresh;
    bool hasLatchedData;
    uint8_t latchedData;
    interrupt_func_t interrupt_func;
} tms9918_context_t;

void tms9918_write(tms9918_context_t *context, bool mode, uint8_t value);
uint8_t tms9918_read(tms9918_context_t* context, bool mode);
void tms9918_register_interrupt_func(tms9918_context_t* context, interrupt_func_t func);
void check_and_generate_interrupt(tms9918_context_t *context);
void tms9918_render_rgba(tms9918_context_t *context, uint32_t* image);
// Render één displayregel ln (0..191) naar een lijnbuffer van 256 ARGB-pixels.
void tms9918_render_line(tms9918_context_t *context, uint32_t* line, int ln);
uint32_t tms9918_get_backdrop_color(tms9918_context_t *context);