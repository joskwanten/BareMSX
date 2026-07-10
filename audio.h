#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

// PWM-audio-uitvoer (PicoCalc GP26=links, GP27=rechts).
// Een DMA zet op sample-rate de PWM-duty uit een ringbuffer; core 0 vult die
// buffer via audio_service() (roept machine_get_audio aan).

void audio_init(void);
void audio_service(void);      // elke frame aanroepen op core 0
uint32_t audio_gen_count(void); // debug: totaal gegenereerde samples

#endif // AUDIO_H
