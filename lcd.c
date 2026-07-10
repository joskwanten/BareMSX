#include <stdio.h>
#include "lcd.h"
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "hardware/dma.h"
#include "hardware/clocks.h"

// --- PicoCalc LCD pinout (SPI1) ---
#define LCD_SPI   spi1
#define LCD_SCK   10
#define LCD_MOSI  11
#define LCD_MISO  12
#define LCD_CS    13
#define LCD_DC    14
#define LCD_RST   15

// Agressief: 75 MHz = clk_sys(150)/2, de hoogste schone deler. Boven de
// firmware-"50 MHz max", dus checken op glitches. spi_init print de echte baud.
#define LCD_SPI_HZ (75000 * 1000) // 75 MHz doel

static int lcd_dma_chan = -1;
static dma_channel_config lcd_dma_cfg;
static bool lcd_dma_active = false;

// ILI9488 commando's
#define CMD_COLADDR  0x2A
#define CMD_PAGEADDR 0x2B
#define CMD_MEMWRITE 0x2C

static inline void cs_low(void)  { gpio_put(LCD_CS, 0); }
static inline void cs_high(void) { gpio_put(LCD_CS, 1); }

static void lcd_cmd(uint8_t cmd) {
    gpio_put(LCD_DC, 0); // command
    cs_low();
    spi_write_blocking(LCD_SPI, &cmd, 1);
    cs_high();
}

static void lcd_data(const uint8_t *data, size_t len) {
    gpio_put(LCD_DC, 1); // data
    cs_low();
    spi_write_blocking(LCD_SPI, data, len);
    cs_high();
}

static void lcd_cmd_data(uint8_t cmd, const uint8_t *data, size_t len) {
    lcd_cmd(cmd);
    if (len) lcd_data(data, len);
}

// Init-tabel overgenomen uit de officiele clockworkpi/PicoCalc firmware.
// Formaat: cmd, aantal databytes, databytes...
static const uint8_t init_seq[] = {
    0xE0, 15, 0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F,
    0xE1, 15, 0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F,
    0xC0, 2,  0x17, 0x15,       // Power Control 1
    0xC1, 1,  0x41,             // Power Control 2
    0xC5, 3,  0x00, 0x12, 0x80, // VCOM Control
    0x36, 1,  0x48,             // MADCTL: MX + BGR
    0x3A, 1,  0x66,             // Pixel format: RGB666 (3 bytes/pixel over SPI)
    0xB0, 1,  0x00,             // Interface Mode
    0xB1, 1,  0xA0,             // Frame Rate
    0x21, 0,                    // Display Inversion ON
    0xB4, 1,  0x02,             // Inversion Control
    0xB6, 3,  0x02, 0x02, 0x3B, // Display Function Control
    0xB7, 1,  0xC6,             // Entry Mode
    0xE9, 1,  0x00,
    0xF7, 4,  0xA9, 0x51, 0x2C, 0x82,
    0x00, // terminator (cmd 0x00 = einde tabel)
};

static void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x1 = x + w - 1;
    uint16_t y1 = y + h - 1;
    uint8_t col[4] = { x >> 8, x & 0xFF, x1 >> 8, x1 & 0xFF };
    uint8_t page[4] = { y >> 8, y & 0xFF, y1 >> 8, y1 & 0xFF };
    lcd_cmd_data(CMD_COLADDR, col, 4);
    lcd_cmd_data(CMD_PAGEADDR, page, 4);
    lcd_cmd(CMD_MEMWRITE);
}

void lcd_init(void) {
    // Forceer clk_peri = clk_sys zodat SPI hoog kan klokken
    uint32_t fsys = clock_get_hz(clk_sys);
    clock_configure(clk_peri, 0, CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLK_SYS, fsys, fsys);

    // SPI + GPIO opzetten
    uint actual = spi_init(LCD_SPI, LCD_SPI_HZ);
    printf("[lcd] clk_sys=%u clk_peri=%u | SPI aangevraagd=%u behaald=%u Hz\n",
           (unsigned)fsys, (unsigned)clock_get_hz(clk_peri),
           (unsigned)LCD_SPI_HZ, actual);
    gpio_set_function(LCD_SCK, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(LCD_MISO, GPIO_FUNC_SPI);

    // DMA-kanaal voor SPI1 TX (voor non-blocking blits)
    lcd_dma_chan = dma_claim_unused_channel(true);
    lcd_dma_cfg = dma_channel_get_default_config(lcd_dma_chan);
    channel_config_set_transfer_data_size(&lcd_dma_cfg, DMA_SIZE_8);
    channel_config_set_dreq(&lcd_dma_cfg, spi_get_dreq(LCD_SPI, true));
    channel_config_set_read_increment(&lcd_dma_cfg, true);
    channel_config_set_write_increment(&lcd_dma_cfg, false);

    gpio_init(LCD_CS);  gpio_set_dir(LCD_CS, GPIO_OUT);  gpio_put(LCD_CS, 1);
    gpio_init(LCD_DC);  gpio_set_dir(LCD_DC, GPIO_OUT);  gpio_put(LCD_DC, 1);
    gpio_init(LCD_RST); gpio_set_dir(LCD_RST, GPIO_OUT); gpio_put(LCD_RST, 1);

    // Hardware reset
    gpio_put(LCD_RST, 1); sleep_ms(10);
    gpio_put(LCD_RST, 0); sleep_ms(20);
    gpio_put(LCD_RST, 1); sleep_ms(150);

    // Init-tabel afspelen
    const uint8_t *p = init_seq;
    while (*p != 0x00) {
        uint8_t cmd = *p++;
        uint8_t n = *p++;
        lcd_cmd_data(cmd, p, n);
        p += n;
    }

    lcd_cmd(0x11); sleep_ms(120); // Sleep Out
    lcd_cmd(0x29); sleep_ms(120); // Display On
}

void lcd_fill_rect_rgb(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint8_t r, uint8_t g, uint8_t b) {
    // Een rij als buffer opbouwen en per scanline wegschrijven.
    static uint8_t line[LCD_WIDTH * 3];
    for (uint16_t i = 0; i < w && i < LCD_WIDTH; i++) {
        line[i * 3 + 0] = r;
        line[i * 3 + 1] = g;
        line[i * 3 + 2] = b;
    }

    set_window(x, y, w, h);
    gpio_put(LCD_DC, 1); // data
    cs_low();
    for (uint16_t row = 0; row < h; row++) {
        spi_write_blocking(LCD_SPI, line, (size_t)w * 3);
    }
    cs_high();
}

void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // RGB565 -> RGB888 (3 bytes/pixel) voor de ILI9488 over SPI.
    lcd_fill_rect_rgb(x, y, w, h,
                      (color >> 8) & 0xF8, (color >> 3) & 0xFC, (color << 3) & 0xF8);
}

void lcd_fill_screen(uint16_t color) {
    lcd_fill_rect(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
}

// Wacht tot de lopende DMA klaar is en de SPI-FIFO leeg. Raakt CS niet aan.
static void dma_wait_idle(void) {
    if (!lcd_dma_active) return;
    dma_channel_wait_for_finish_blocking(lcd_dma_chan);
    while (spi_is_busy(LCD_SPI)) tight_loop_contents();
    lcd_dma_active = false;
}

void lcd_begin_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    dma_wait_idle();
    set_window(x, y, w, h);
    gpio_put(LCD_DC, 1); // data
    cs_low();
}

void lcd_push_dma(const uint8_t *rgb, size_t nbytes) {
    dma_wait_idle(); // vorige transfer moet klaar zijn
    dma_channel_configure(lcd_dma_chan, &lcd_dma_cfg,
                          &spi_get_hw(LCD_SPI)->dr, rgb, nbytes, true);
    lcd_dma_active = true;
}

void lcd_end_blit(void) {
    dma_wait_idle();
    cs_high();
}
