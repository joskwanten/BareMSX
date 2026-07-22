#include "mapper.h"
#include "pico.h" // __not_in_flash_func

const char *mapper_name(mapper_type_t t)
{
    switch (t) {
    case MAPPER_PLAIN:      return "plain";
    case MAPPER_KONAMI:     return "konami";
    case MAPPER_ASCII8:     return "ascii8";
    case MAPPER_ASCII16:    return "ascii16";
    case MAPPER_KONAMI_SCC: return "konami-scc";
    default:                return "none";
    }
}

void mapper_init(mapper_t *m, const uint8_t *rom, uint32_t size, mapper_type_t type)
{
    m->rom = rom;
    m->size = size;
    m->type = type;
    m->nbanks8 = size / 8192u;
    if (m->nbanks8 == 0) m->nbanks8 = 1;

    // Default 8K bank mapping for the cartridge area (0x4000-0xBFFF = pages 2..5).
    for (int i = 0; i < 8; i++) m->page[i] = 0;
    switch (type) {
    case MAPPER_KONAMI:   // 0x4000 fixed at bank 0; others default to 1,2,3
    case MAPPER_KONAMI_SCC:
        m->page[2] = 0; m->page[3] = 1; m->page[4] = 2; m->page[5] = 3;
        break;
    case MAPPER_ASCII16:  // 16K bank 0 = 8K banks 0,1 in both windows
        m->page[2] = 0; m->page[3] = 1; m->page[4] = 0; m->page[5] = 1;
        break;
    case MAPPER_ASCII8:   // all banks 0 at reset
    default:
        break;
    }
}

uint8_t __not_in_flash_func(mapper_read)(void *context, uint16_t address)
{
    mapper_t *m = (mapper_t *)context;
    if (m->type == MAPPER_PLAIN) {
        uint32_t off = (uint32_t)address - 0x4000u;
        return (off < m->size) ? m->rom[off] : 0xFF;
    }
    uint32_t bank = m->page[(address >> 13) & 7] % m->nbanks8;
    return m->rom[bank * 8192u + (address & 0x1FFF)];
}

void __not_in_flash_func(mapper_write)(void *context, uint16_t address, uint8_t value)
{
    mapper_t *m = (mapper_t *)context;
    switch (m->type) {
    case MAPPER_KONAMI: // 0x6000/0x8000/0xA000 select the 0x6000/0x8000/0xA000 windows
        if (address >= 0x6000 && address < 0x8000) m->page[3] = value;
        else if (address >= 0x8000 && address < 0xA000) m->page[4] = value;
        else if (address >= 0xA000 && address < 0xC000) m->page[5] = value;
        break;
    case MAPPER_ASCII8:
        if (address >= 0x6000 && address < 0x6800) m->page[2] = value;
        else if (address >= 0x6800 && address < 0x7000) m->page[3] = value;
        else if (address >= 0x7000 && address < 0x7800) m->page[4] = value;
        else if (address >= 0x7800 && address < 0x8000) m->page[5] = value;
        break;
    case MAPPER_ASCII16:
        if (address >= 0x6000 && address < 0x6800) {
            m->page[2] = (uint8_t)(value * 2);
            m->page[3] = (uint8_t)(value * 2 + 1);
        } else if (address >= 0x7000 && address < 0x7800) {
            m->page[4] = (uint8_t)(value * 2);
            m->page[5] = (uint8_t)(value * 2 + 1);
        }
        break;
    case MAPPER_KONAMI_SCC:
        // Alleen de paging (bankregisters op 0x5000/0x7000/0x9000/0xB000);
        // SCC-gelúid zit in het aparte konami_scc-device (slot 1). Deze route
        // wordt gebruikt voor een SCC-game in slot 2.
        if (address >= 0x5000 && address < 0x5800) m->page[2] = value;
        else if (address >= 0x7000 && address < 0x7800) m->page[3] = value;
        else if (address >= 0x9000 && address < 0x9800) m->page[4] = value;
        else if (address >= 0xB000 && address < 0xB800) m->page[5] = value;
        break;
    default:
        break;
    }
}

mapper_type_t mapper_detect(const uint8_t *rom, uint32_t size)
{
    if (rom == 0 || size == 0) return MAPPER_NONE;

    // Small ROMs are plain (16K/32K/48K without a mapper).
    if (size <= 32u * 1024u) return MAPPER_PLAIN;

    // Heuristic: count LD (nnnn),A (0x32) writes to each mapper's bank
    // registers. We tellen alleen de ONDERSCHEIDENDE adressen — 0x6000 en
    // 0x7000 delen (bijna) alle mappers, dus die zeggen niets over SCC/Konami.
    // Een echte SCC-game schrijft naar 0x5000/0x9000/0xB000, een Konami-game
    // naar 0x8000/0xA000, een ASCII8-game naar 0x6800/0x7800. Wie alléén
    // 0x6000/0x7000 gebruikt is ASCII16 (twee 16KB-banks) — dat was Zanac-EX,
    // dat eerder als konami-scc werd misgedetecteerd door de gedeelde adressen.
    int scc_bits = 0, kon_bits = 0, scc_n = 0, kon_n = 0, a8_ex = 0;
    for (uint32_t i = 0; i + 2 < size; i++) {
        if (rom[i] != 0x32) continue; // LD (nnnn),A
        uint16_t a = (uint16_t)(rom[i + 1] | (rom[i + 2] << 8));
        switch (a) {
        case 0x5000: scc_bits |= 1; scc_n++; break; // SCC-exclusieve banks
        case 0x9000: scc_bits |= 2; scc_n++; break;
        case 0xB000: scc_bits |= 4; scc_n++; break;
        case 0x4000: kon_bits |= 1; kon_n++; break; // Konami-exclusieve banks
        case 0x8000: kon_bits |= 2; kon_n++; break;
        case 0xA000: kon_bits |= 4; kon_n++; break;
        case 0x6800: case 0x7800: a8_ex++; break;   // ASCII8-exclusief
        // 0x6000/0x7000 zijn gedeeld (alle mappers) -> geen onderscheidende stem
        default: break;
        }
    }

    // Een échte megaROM paget álle vensters, dus schrijft meerdere DISTINCT
    // bank-registers en doet dat vaak. Eén losse write is toevallige data
    // (Aleste: één 0x5000 -> werd fout als konami-scc gedetecteerd). Drempel:
    // >=2 distinct onderscheidende registers, of >=6 writes.
    int scc_d = __builtin_popcount((unsigned)scc_bits);
    int kon_d = __builtin_popcount((unsigned)kon_bits);
    bool is_scc = scc_d >= 2 || scc_n >= 6;
    bool is_kon = kon_d >= 2 || kon_n >= 6;
    if (is_scc && scc_n >= kon_n) return MAPPER_KONAMI_SCC;
    if (is_kon) return MAPPER_KONAMI;
    if (a8_ex) return MAPPER_ASCII8;
    return MAPPER_ASCII16; // alleen gedeelde/geen writes -> veilige default
}
