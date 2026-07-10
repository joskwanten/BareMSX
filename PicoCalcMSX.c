#include <stdio.h>
#include "pico/stdlib.h" // trekt pico.h (__not_in_flash_func) mee
#include "pico/multicore.h"
#include "hardware/sync.h" // __dmb
#include "lcd.h"
#include "machine.h"
#include "i2ckbd.h"
#include "keymap.h"
#include "keycodes.h"
#include "audio.h"

// PicoCalc MSX host.
//   Core 0: emulatie (Z80/VDP/keyboard), gepaced op 60 Hz.
//   Core 1: blit van de VDP-snapshot naar de LCD (overlapt de emulatie).

#define MSX_W 256
#define MSX_H 192
#define ORIG_X ((LCD_WIDTH - MSX_W) / 2)   // 32  (1:1 gecentreerd)
#define ORIG_Y ((LCD_HEIGHT - MSX_H) / 2)  // 64
#define FRAME_US 16667                     // 60 Hz

// Opgeschaald beeld (aspectcorrect 4:3): 256x192 -> 320x240, gecentreerd.
#define SCALE_W 320
#define SCALE_H 240
#define SCALE_X ((LCD_WIDTH - SCALE_W) / 2)   // 0
#define SCALE_Y ((LCD_HEIGHT - SCALE_H) / 2)  // 40

static uint32_t line_argb[MSX_W];          // één gerenderde MSX-regel (ARGB) - core 1
static uint8_t  rgb_line[2][SCALE_W * 3];  // ping-pong RGB888 (max breedte = opgeschaald)
static uint16_t xmap[SCALE_W];             // bron-x per scherm-x (nearest-neighbor)

// Handshake tussen de cores
static volatile bool g_render_req = false;  // core0 -> core1: snapshot klaar, blit 'm
static volatile bool g_render_done = true;  // core1 -> core0: snapshot vrij
static volatile uint32_t g_blit_count = 0;  // display-fps teller
static volatile int g_scale_mode = 0;       // 0 = origineel 1:1, 1 = opgeschaald 320x240

static uint32_t last_border = 0;
static volatile bool border_valid = false;

// Rand met backdrop-kleur — layout hangt af van de mode.
static void fill_border(uint32_t argb) {
    uint8_t r = (argb >> 16) & 0xFF, g = (argb >> 8) & 0xFF, b = argb & 0xFF;
    if (g_scale_mode == 0) {
        // Origineel 256x192 gecentreerd -> rand aan alle vier de kanten
        lcd_fill_rect_rgb(0, 0, LCD_WIDTH, ORIG_Y, r, g, b);                       // boven
        lcd_fill_rect_rgb(0, ORIG_Y + MSX_H, LCD_WIDTH,
                          LCD_HEIGHT - (ORIG_Y + MSX_H), r, g, b);                 // onder
        lcd_fill_rect_rgb(0, ORIG_Y, ORIG_X, MSX_H, r, g, b);                      // links
        lcd_fill_rect_rgb(ORIG_X + MSX_W, ORIG_Y,
                          LCD_WIDTH - (ORIG_X + MSX_W), MSX_H, r, g, b);           // rechts
    } else {
        // Opgeschaald 320x240 (volle breedte) -> alleen boven/onder
        lcd_fill_rect_rgb(0, 0, LCD_WIDTH, SCALE_Y, r, g, b);
        lcd_fill_rect_rgb(0, SCALE_Y + SCALE_H, LCD_WIDTH,
                          LCD_HEIGHT - (SCALE_Y + SCALE_H), r, g, b);
    }
}

// Blit de VDP-snapshot naar de LCD (core 1, RAM). Twee modes.
static void __not_in_flash_func(draw_frame_snapshot)(void) {
    int mode = g_scale_mode;

    uint32_t border = machine_snapshot_background_color();
    if (!border_valid || border != last_border) {
        fill_border(border);
        last_border = border;
        border_valid = true;
    }

    int cur = 0;
    if (mode == 0) {
        // Origineel 1:1
        lcd_begin_blit(ORIG_X, ORIG_Y, MSX_W, MSX_H);
        for (int y = 0; y < MSX_H; y++) {
            machine_render_snapshot_line(line_argb, y);
            uint8_t *dst = rgb_line[cur];
            for (int i = 0; i < MSX_W; i++) {
                uint32_t px = line_argb[i];
                dst[i * 3 + 0] = (px >> 16) & 0xFF;
                dst[i * 3 + 1] = (px >> 8) & 0xFF;
                dst[i * 3 + 2] = px & 0xFF;
            }
            lcd_push_dma(dst, MSX_W * 3);
            cur ^= 1;
        }
    } else {
        // Opgeschaald 320x240 (nearest-neighbor)
        lcd_begin_blit(SCALE_X, SCALE_Y, SCALE_W, SCALE_H);
        int last_my = -1;
        for (int sy = 0; sy < SCALE_H; sy++) {
            int my = (sy * MSX_H) / SCALE_H;
            if (my != last_my) {
                machine_render_snapshot_line(line_argb, my);
                last_my = my;
            }
            uint8_t *dst = rgb_line[cur];
            for (int sx = 0; sx < SCALE_W; sx++) {
                uint32_t px = line_argb[xmap[sx]];
                dst[sx * 3 + 0] = (px >> 16) & 0xFF;
                dst[sx * 3 + 1] = (px >> 8) & 0xFF;
                dst[sx * 3 + 2] = px & 0xFF;
            }
            lcd_push_dma(dst, SCALE_W * 3);
            cur ^= 1;
        }
    }
    lcd_end_blit();
}

// Core 1: wacht op een snapshot en blit die.
static void __not_in_flash_func(core1_main)(void) {
    while (true) {
        while (!g_render_req) tight_loop_contents();
        g_render_req = false;
        __dmb(); // snapshot-writes van core 0 zichtbaar maken
        draw_frame_snapshot();
        __dmb();
        g_render_done = true;
        g_blit_count++;
    }
}

int main(void)
{
    stdio_init_all();

    lcd_init();
    lcd_fill_screen(LCD_BLACK);
    kbd_init();

    if (!machine_init()) {
        lcd_fill_screen(LCD_RED);
        while (true) tight_loop_contents();
    }

    audio_init();

    // Nearest-neighbor x-mapping: scherm-x -> bron-x (256 breed -> 320 breed)
    for (int sx = 0; sx < SCALE_W; sx++)
        xmap[sx] = (uint16_t)((sx * MSX_W) / SCALE_W);

    // Core 1 start met de blit-loop; core 0 emuleert
    multicore_launch_core1(core1_main);

    uint32_t frame = 0;
    uint32_t emu_frames = 0;
    uint64_t sec_t0 = time_us_64();

    while (true) {
        uint64_t t0 = time_us_64();

        // Toetsenbord pollen (I2C @10kHz is duur): elke 4e frame
        if (frame % 4 == 0) {
            static bool ctrl_down = false, alt_down = false;
            uint8_t kst, kcd;
            for (int n = 0; n < 8 && kbd_read(&kst, &kcd); n++) {
                bool down = (kst == KBD_STATE_PRESSED || kst == KBD_STATE_HOLD);

                // Host-hotkey: Ctrl + Alt samen = beeldschaal togglen (origineel <-> opgeschaald)
                if (kcd == PC_KEY_CTRL) ctrl_down = down;
                else if (kcd == PC_KEY_ALT) alt_down = down;
                if (kst == KBD_STATE_PRESSED &&
                    (kcd == PC_KEY_CTRL || kcd == PC_KEY_ALT) && ctrl_down && alt_down) {
                    g_scale_mode ^= 1;
                    border_valid = false; // rand opnieuw tekenen (layout wijzigt)
                }

                // Delete = debug-dump van de Z80-staat (tijdelijk, hang-diagnose)
                if (kcd == 0xD4) {
                    if (kst == KBD_STATE_PRESSED) machine_dbg_dump();
                    continue;
                }
                keymap_handle(kst, kcd);
            }
        }

        // Eén MSX-frame emuleren
        machine_do_cycles();
        machine_generate_interrupt();

        // Audio-samples voor dit frame genereren en naar de PWM-DMA voeden
        audio_service();

        // Snapshot doorgeven aan core 1 als die klaar is met de vorige
        // (zo niet -> frame wordt niet getoond, maar de emulatie loopt door)
        if (g_render_done) {
            machine_snapshot_vdp();
            __dmb();
            g_render_done = false;
            g_render_req = true;
        }

        frame++;
        emu_frames++;

        // Pace naar 60 Hz
        uint64_t elapsed = time_us_64() - t0;
        if (elapsed < FRAME_US) sleep_us(FRAME_US - elapsed);

        // Eens per seconde: emulatie-fps, display-fps en audio-samplerate
        if (time_us_64() - sec_t0 >= 1000000) {
            static uint32_t last_gen = 0;
            uint32_t gen = audio_gen_count();
            printf("[fps] emu=%lu display=%lu audio=%lu Hz  pc=%04X\n",
                   (unsigned long)emu_frames, (unsigned long)g_blit_count,
                   (unsigned long)(gen - last_gen), machine_dbg_pc());
            last_gen = gen;
            emu_frames = 0;
            g_blit_count = 0;
            sec_t0 = time_us_64();
        }
    }
}
