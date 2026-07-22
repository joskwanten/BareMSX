#ifndef AUDIO_HDMI_H
#define AUDIO_HDMI_H

// HDMI audio backend: streams the emulator's 48 kHz stereo output over HDMI
// Data Islands via pico_hdmi. The emulator (PSG + SCC) is configured to produce
// 48 kHz directly, so no resampling is needed.
//
// Beide kanten draaien op core 1 (pico_hdmi's background task): generate_burst
// synthetiseert 48 kHz in een ring, pump leegt die in de Data-Island-queue.
// Core 0 doet puur emulatie+render, zodat de zwaarste MSX2-games de frame-naad
// niet meer overrunnen (anders stale toplijnen).

#include <stdint.h>

void audio_hdmi_init(void);      // reset ring state

// Synthese: vult de ring bij tot ~halfvol, hooguit max_samples per aanroep
// (begrensd zodat de pump/scanout niet verhongert). Draait op core 1.
void audio_hdmi_generate_burst(uint32_t max_samples);
void audio_hdmi_pump(void);      // core 1: drain the ring into the DI queue

#endif // AUDIO_HDMI_H
