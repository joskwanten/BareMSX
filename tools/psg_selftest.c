// Host-referentie voor de emu2149-selftest (identiek aan de Pico-kant).
#include <stdio.h>
#include <stdint.h>
#include "emu2149.h"
int main(void)
{
    PSG *tp = PSG_new(3579545, 48000);
    PSG_setVolumeMode(tp, EMU2149_VOL_AY_3_8910);
    PSG_set_quality(tp, 0);
    PSG_reset(tp);
    PSG_writeReg(tp, 0, 0xF7);
    PSG_writeReg(tp, 1, 0x01);
    PSG_writeReg(tp, 7, 0x3E);
    PSG_writeReg(tp, 8, 0x0F);
    FILE *f = fopen("psg_host.bin", "wb");
    for (int i = 0; i < 12288; i++) {
        int16_t v = PSG_calc(tp);
        fwrite(&v, 2, 1, f);
    }
    fclose(f);
    return 0;
}

// Gebruik (host): cc -O2 -I emu -I sdl/compat -o psg_selftest tools/psg_selftest.c emu/emu2149.c
// Pico-kant: aud_test_tone=7 (SWD) na aud_cap_pos=0; dump aud_cap en lijn uit
// op het startpatroon (2040,3060,3570,...). 22-07-2026: byte-identiek bewezen.
