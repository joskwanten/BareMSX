#include "audio.h"
#include "machine.h"
#include "pico/stdlib.h"
#include "pico.h"
#include "hardware/pwm.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

// --- PicoCalc PWM-audio ---
#define AUDIO_L_PIN 26
#define AUDIO_R_PIN 27
#define PWM_WRAP    1023           // 10-bit duty; carrier = sysclk/1024 (~146 kHz)
#define AUDIO_GAIN  1              // niveau wordt nu in de mix (machine_get_audio) gezet
#define PWM_MID     512

#define BUF_BITS    13
#define BUF_SAMPLES (1u << BUF_BITS)     // 8192 stereo-samples (32 KB)
#define BUF_MASK    (BUF_SAMPLES - 1)
#define LEAD        3072           // ~60ms marge tegen frame-jitter
#define MAX_CHUNK   4096

// Ring moet uitgelijnd zijn op z'n grootte (DMA read-address wrap).
static uint32_t audio_buf[BUF_SAMPLES] __attribute__((aligned(BUF_SAMPLES * 4)));
static int16_t audio_tmp[MAX_CHUNK * 2];

static uint pwm_slice;
static int dma_chan, dma_ctrl_chan;
static uint32_t reload_count = BUF_SAMPLES;
static volatile uint32_t write_idx = 0;
static volatile uint32_t gen_count = 0; // debug: totaal gegenereerde samples

uint32_t audio_gen_count(void) { return gen_count; }

// int16 sample -> 10-bit PWM-duty (0..1023).
static inline uint16_t to_duty(int16_t s) {
    int32_t v = (((int32_t)s * AUDIO_GAIN) >> 6) + PWM_MID;
    if (v < 0) v = 0;
    if (v > PWM_WRAP) v = PWM_WRAP;
    return (uint16_t)v;
}
static inline uint32_t duty_word(int16_t l, int16_t r) {
    return ((uint32_t)to_duty(r) << 16) | to_duty(l); // PWM CC: B(GP27) hoog, A(GP26) laag
}

static inline uint32_t dma_read_index(void) {
    uint32_t addr = dma_hw->ch[dma_chan].read_addr;
    return ((addr - (uint32_t)audio_buf) >> 2) & BUF_MASK;
}

void audio_init(void) {
    // Vul met stilte (mid-duty) om plops te voorkomen
    for (uint32_t i = 0; i < BUF_SAMPLES; i++) audio_buf[i] = duty_word(0, 0);

    // PWM: GP26/27 op dezelfde slice, kanaal A/B
    gpio_set_function(AUDIO_L_PIN, GPIO_FUNC_PWM);
    gpio_set_function(AUDIO_R_PIN, GPIO_FUNC_PWM);
    pwm_slice = pwm_gpio_to_slice_num(AUDIO_L_PIN);
    pwm_config cfg = pwm_get_default_config();
    pwm_config_set_wrap(&cfg, PWM_WRAP);
    pwm_init(pwm_slice, &cfg, true);
    pwm_set_both_levels(pwm_slice, PWM_MID, PWM_MID);

    // DMA-pacing-timer op de sample-rate: rate = sysclk * 1/den
    int t = dma_claim_unused_timer(true);
    uint32_t den = clock_get_hz(clk_sys) / AUDIO_SAMPLE_RATE;
    dma_timer_set_fraction(t, 1, (uint16_t)den);

    // Data-DMA: ringbuffer -> PWM CC-register, gepaced door de timer
    dma_chan = dma_claim_unused_channel(true);
    dma_ctrl_chan = dma_claim_unused_channel(true);

    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, true);
    channel_config_set_write_increment(&c, false);
    channel_config_set_ring(&c, false, BUF_BITS + 2); // wrap read-addr over de buffer
    channel_config_set_dreq(&c, dma_get_timer_dreq(t));
    channel_config_set_chain_to(&c, dma_ctrl_chan);
    dma_channel_configure(dma_chan, &c,
                          &pwm_hw->slice[pwm_slice].cc, audio_buf, BUF_SAMPLES, false);

    // Control-DMA: herlaadt de transfer-count en hertriggert de data-DMA (eindeloos)
    dma_channel_config cc = dma_channel_get_default_config(dma_ctrl_chan);
    channel_config_set_transfer_data_size(&cc, DMA_SIZE_32);
    channel_config_set_read_increment(&cc, false);
    channel_config_set_write_increment(&cc, false);
    dma_channel_configure(dma_ctrl_chan, &cc,
                          &dma_hw->ch[dma_chan].al1_transfer_count_trig,
                          &reload_count, 1, false);

    dma_channel_start(dma_chan);
}

// Vul de ringbuffer bij tot ~LEAD samples voor de DMA-leespositie.
void __not_in_flash_func(audio_service)(void) {
    uint32_t r = dma_read_index();
    uint32_t filled = (write_idx - r) & BUF_MASK;
    int need = LEAD - (int)filled;
    if (need <= 0) return;
    if (need > MAX_CHUNK) need = MAX_CHUNK;

    machine_get_audio(audio_tmp, need * 2);
    for (int i = 0; i < need; i++) {
        audio_buf[write_idx & BUF_MASK] = duty_word(audio_tmp[i * 2], audio_tmp[i * 2 + 1]);
        write_idx++;
    }
    gen_count += need;
}
