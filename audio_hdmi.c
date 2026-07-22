#include "audio_hdmi.h"
#include "machine.h" // machine_get_audio

#include "pico/stdlib.h"
#include "hardware/sync.h"

#include "pico_hdmi/hstx_data_island_queue.h"
#include "pico_hdmi/hstx_packet.h"
#include "pico_hdmi/video_output.h" // DI_HSYNC_ACTIVE

// The emulator produces 48 kHz directly (AUDIO_SAMPLE_RATE), matching the HDMI
// audio rate, so there is no resampling — samples are just moved into a ring.
// Beide kanten draaien nu op CORE 1 (pipeline_task): audio_hdmi_generate_burst
// synthetiseert en vult de ring, audio_hdmi_pump leegt 'm in de Data-Island-
// queue. Core 0 doet puur emulatie+render. Enige cross-core data zijn de
// PSG/SCC-registerbytes (core 0 schrijft, de synthese op core 1 leest) —
// losse-byte-reads zijn atomair, dus hooguit één glitchy sample, geen corruptie.

// --- Lock-free SPSC ring of 48 kHz stereo frames ---
// Op core 1 vult generate de head en leegt pump de tail (zelfde core, dus de
// ring-race vervalt); alleen de DI-queue erna is nog core1->IRQ.
#define ARING_BITS 11 // 2048 samples (~43ms) — ruim met de continue pomp
#define ARING (1u << ARING_BITS)
#define ARING_MASK (ARING - 1)
static int16_t aring_l[ARING];
static int16_t aring_r[ARING];
static volatile uint32_t a_head = 0;
static volatile uint32_t a_tail = 0;

#define IN_CHUNK 256
static int16_t in_buf[IN_CHUNK * 2];

static inline uint32_t aring_fill(void) { return (a_head - a_tail) & ARING_MASK; }

void audio_hdmi_init(void)
{
    a_head = a_tail = 0;
}

// Debug-capture (SWD): schrijf aud_cap_pos=0 via de probe; vult eenmalig
// ~0,26s mono-samples, uit te lezen met dump_image.
#define AUD_CAP_N 12032
int16_t aud_cap[AUD_CAP_N];
volatile uint32_t aud_cap_pos = AUD_CAP_N; // vol = inactief
volatile uint32_t aud_test_tone = 0;       // SWD: 1 = 440Hz-testtoon
volatile uint32_t aud_att = 0;             // SWD: verzwakking in stappen van 6dB
volatile uint32_t aud_rd_spikes = 0;       // leeskant-discontinuïteiten (pump)
// Spike-trigger: rolt continu door aud_cap; bevriest ~2000 samples ná een
// discontinuïteit (2e afgeleide > 20000 — SDL-referentie piekt op 15302).
// SWD: mww aud_frozen 0 om te herwapenen; aud_spikes telt events.
volatile uint32_t aud_spikes = 0;
volatile uint32_t aud_frozen = 0;   // 1 = buffer bevroren (spike gevangen)
static int16_t sp_b1 = 0, sp_b2 = 0;
static int32_t sp_hold = -1;        // >0: nog n samples vullen, dan bevriezen

static inline void __not_in_flash_func(aud_spike_scan)(const int16_t *buf, uint32_t nsamp)
{
    if (aud_frozen || aud_test_tone == 7) return; // 7: cap is van de selftest
    for (uint32_t i = 0; i < nsamp; i++) {
        int16_t v = buf[i * 2];
        aud_cap[aud_cap_pos % AUD_CAP_N] = v;
        aud_cap_pos++;
        int32_t dd = (int32_t)v - 2 * (int32_t)sp_b1 + (int32_t)sp_b2;
        if (dd < 0) dd = -dd;
        sp_b2 = sp_b1; sp_b1 = v;
        if (dd > 20000 && sp_hold < 0) { aud_spikes++; sp_hold = 2000; }
        if (sp_hold > 0 && --sp_hold == 0) { aud_frozen = 1; sp_hold = -1; return; }
    }
}

void __not_in_flash_func(audio_hdmi_generate_burst)(uint32_t max_samples)
{
    // Ring bijvullen tot ~halfvol (~21 ms speling bij 48 kHz), begrensd per
    // aanroep zodat de scanline-IRQ en de pump ertussen blijven komen.
    uint32_t fill = aring_fill();
    const uint32_t target = ARING / 2;
    if (fill >= target) return;
    uint32_t need = target - fill;
    if (need > max_samples) need = max_samples;

    // Testtoon (SWD): zet aud_test_tone=1 -> 440Hz-blokgolf i.p.v. emulatie.
    // Kraakt die óók, dan zit de fout in de leverketen (DI/HSTX/TV), niet in
    // de synthese.
    extern volatile uint32_t aud_test_tone;
    while (need) {
        uint32_t chunk = need < IN_CHUNK ? need : IN_CHUNK;
        // aud_test_tone: 1 = toon i.p.v. synthese (goedkoop); 2 = synthese WEL
        // draaien (zelfde CPU-last als het spel) maar de toon uitsturen — als
        // de toon dan kraakt, is de belasting de oorzaak, niet de samples.
        if (aud_test_tone == 2)
            machine_get_audio(in_buf, chunk * 2);
        if (aud_test_tone) {
            static uint32_t tp = 0, lfsr = 0xACE1u;
            for (uint32_t i = 0; i < chunk; i++, tp++) {
                int16_t v;
                switch (aud_test_tone) {
                default: // 1/2: ~444Hz blok
                    v = ((tp / 54) & 1) ? 8000 : -8000; break;
                case 3: // volle-schaal ruis (alle waarden, snel wisselend)
                    lfsr = (lfsr >> 1) ^ (-(int32_t)(lfsr & 1) & 0xB400u);
                    v = (int16_t)((lfsr << 1) ^ lfsr); break;
                case 4: // zachte ruis (zelfde patroon, kleine waarden)
                    lfsr = (lfsr >> 1) ^ (-(int32_t)(lfsr & 1) & 0xB400u);
                    v = (int16_t)(((lfsr << 1) ^ lfsr)) >> 4; break;
                case 5: // 12kHz blok (max wisselsnelheid, 2 waarden)
                    v = (tp & 2) ? 8000 : -8000; break;
                case 6: // 444Hz-blok met LSB-wobble: klinkt identiek aan
                        // modus 1, maar maakt elk data-island UNIEK.
                    v = (int16_t)((((tp / 54) & 1) ? 8000 : -8000) ^ (tp & 1));
                    break;
                case 7: { // deterministische emu2149-selftest: vaste registers
                          // vanaf reset, eerste 12288 samples in aud_cap ->
                          // via SWD dumpen en byte-diffen met de host-run.
                    extern int16_t emu2149_selftest_sample(void);
                    v = emu2149_selftest_sample();
                    if (aud_cap_pos < AUD_CAP_N) aud_cap[aud_cap_pos++] = v;
                    break;
                }
                }
                in_buf[i * 2] = v; in_buf[i * 2 + 1] = v;
            }
        } else
        machine_get_audio(in_buf, chunk * 2); // 48 kHz stereo, interleaved int16
        if (aud_att)
            for (uint32_t i = 0; i < chunk * 2; i++)
                in_buf[i] = (int16_t)(in_buf[i] >> aud_att);
        aud_spike_scan(in_buf, chunk);
        for (uint32_t i = 0; i < chunk; i++) {
            if (((a_head + 1) & ARING_MASK) == a_tail)
                return; // ring full
            aring_l[a_head] = in_buf[i * 2];
            aring_r[a_head] = in_buf[i * 2 + 1];
            __dmb(); // sample data visible before the head advances
            a_head = (a_head + 1) & ARING_MASK;
        }
        need -= chunk;
    }
}

// Meet: hoe vaak is de sample-ring droog (underrun -> stilte-island = krak)
// en hoe vaak is de DI-queue al vol genoeg (gezond).
volatile uint32_t aud_ring_dry = 0;
volatile uint32_t aud_pump_calls = 0;

void __not_in_flash_func(audio_hdmi_pump)(void)
{
    static int frame_counter = 0;
    aud_pump_calls++;

    // Begrensd per aanroep: de queue in één keer vol encoderen (TERC4) kost
    // ~100+ µs en verhongert dan de lijnproducer op core 1 -> ring-misses op
    // een vást punt in het frame. Een paar islands per keer druppelt net zo
    // hard bij (de taak draait vele malen per scanlijn), zonder burst.
    int budget = 4;
    while (hstx_di_queue_get_level() < 48 && budget-- > 0) { // < DI-capaciteit (64)
        if (aring_fill() < 4) {
            aud_ring_dry++;
            break; // underrun: the library inserts a silence island for us
        }

        audio_sample_t s[4];
        for (int i = 0; i < 4; i++) {
            s[i].left = aring_l[a_tail];
            s[i].right = aring_r[a_tail];
            a_tail = (a_tail + 1) & ARING_MASK;
        }
        // Leeskant-spikedetectie: de schrijfkant is bewezen schoon; ziet deze
        // scanner hier wél discontinuïteiten, dan wordt de ring onderweg
        // overschreven (geheugencorruptie) of is er een indexrace.
        {
            extern volatile uint32_t aud_rd_spikes;
            static int16_t r1 = 0, r2 = 0;
            for (int i = 0; i < 4; i++) {
                int32_t dd = (int32_t)s[i].left - 2 * (int32_t)r1 + (int32_t)r2;
                if (dd < 0) dd = -dd;
                if (dd > 20000) aud_rd_spikes++;
                r2 = r1; r1 = s[i].left;
            }
        }

        hstx_packet_t packet;
        // _cs-variant: stuurt de IEC60958-channel-status mee (o.a. 48kHz-
        // declaratie). De kale variant laat alle CS-bits 0 = "44,1kHz" —
        // sommige TV's resamplen/detecteren daarop en produceren tikken bij
        // niet-periodiek materiaal (PSG-muziek/ruis) terwijl tonen schoon
        // blijven.
        frame_counter = hstx_packet_set_audio_samples_cs(&packet, s, 4, frame_counter);

        hstx_data_island_t island;
        hstx_encode_data_island(&island, &packet, false, DI_HSYNC_ACTIVE);
        hstx_di_queue_push(&island);
    }
}
