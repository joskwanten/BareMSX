#include <string.h>
#include "konami-mega-rom-scc.h"
#include "pico.h"

// Konami SCC: mapper (bank switching) + integer-based geluidsgenerator.
// De geluidslogica komt uit het RogueDrive-project (integer, fixed-point fase,
// voorberekende steps) — licht genoeg voor de RP2350, geen double/FIR.

void scc_init(konami_scc_t* context)
{
    memset(context, 0, sizeof(konami_scc_t));
    // Initiele bank-config (zoals de originele mapper)
    context->selected_pages[5] = 1;
    context->selected_pages[6] = 2;
    context->selected_pages[7] = 4;
}

void scc_set_rom(konami_scc_t* context, uint8_t* rom, uint32_t size)
{
    context->rom = rom;
    // Bank-mask = aantal 8KB-banks - 1 (ROM-groottes zijn machten van 2).
    // Voorkomt out-of-bounds reads bij bank-nummers > ROM (o.a. 0x3f = SCC-enable).
    context->bank_mask = (size / SCC_PAGE_SIZE) - 1;
}

// Fase-stap per sample voor een frequentie-register (op write voorberekend,
// zodat scc_process geen 64-bit deling hoeft te doen).
static uint32_t compute_step(konami_scc_t* c, uint32_t chan)
{
    uint8_t hi = c->scc[0x80 + 2 * chan + 1] & 0x0F;
    uint8_t lo = c->scc[0x80 + 2 * chan];
    uint32_t t = (hi << 8) | lo;
    uint64_t step64 = ((uint64_t)3579545 << 27) / (1LL * (t + 1) * SCC_SAMPLE_RATE);
    return (uint32_t)step64;
}

int16_t __not_in_flash_func(scc_process)(konami_scc_t* c)
{
    int32_t mixed = 0;
    for (int chan = 0; chan < 5; chan++) {
        c->phase[chan] += c->step[chan];
        uint32_t pos = (c->phase[chan] >> 27) & 31;
        int wave_off = (chan < 4) ? (chan << 5) : (3 << 5); // kanaal 4 deelt golf 3
        int16_t wave = (int8_t)c->scc[wave_off + pos];
        int16_t vol = (c->scc[0x8F] & (1 << chan)) ? (c->scc[0x8A + chan] & 0x0F) : 0;
        mixed += wave * vol;
    }
    return (int16_t)(mixed * 3);
}

uint8_t __not_in_flash_func(scc_read)(void* context, uint16_t address)
{
    konami_scc_t* c = (konami_scc_t*)context;
    uint32_t page_index = (address >> 13) % 8;
    uint32_t page = c->selected_pages[page_index] & c->bank_mask; // masker tegen out-of-bounds
    return c->rom[(page * SCC_PAGE_SIZE) + (address % SCC_PAGE_SIZE)];
}

void __not_in_flash_func(scc_write)(void* context, uint16_t address, uint8_t value)
{
    konami_scc_t* c = (konami_scc_t*)context;
    if (address % 0x2000 >= 0x1800) {
        if (c->selected_pages[(address >> 13) % 8] == 0x3f) {
            uint32_t reg = (address % 0x2000) - 0x1800;
            if (reg <= 255) {
                c->scc[reg] = value;
                if (reg >= 0x80 && reg <= 0x89) {
                    uint32_t chan = (reg - 0x80) >> 1;
                    c->step[chan] = compute_step(c, chan);
                }
            }
        }
    } else {
        c->selected_pages[(address >> 13) % 8] = value & 0x3f;
    }
}
