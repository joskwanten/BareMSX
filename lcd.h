#ifndef LCD_H
#define LCD_H

#include <stdint.h>
#include <stddef.h>

// PicoCalc LCD: ILI9488/ST7365P over SPI1.
// De controller heeft 320x480 geheugen, maar het paneel toont alleen de
// bovenste 320x320 (geen offset). We werken dus met 320x320 zichtbaar.
#define LCD_WIDTH  320
#define LCD_HEIGHT 320

// 16-bit RGB565 helper (input); de driver zet dit om naar RGB888 voor de bus.
#define LCD_RGB(r, g, b) ((uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)))

#define LCD_BLACK   0x0000
#define LCD_WHITE   0xFFFF
#define LCD_RED     LCD_RGB(255, 0, 0)
#define LCD_GREEN   LCD_RGB(0, 255, 0)
#define LCD_BLUE    LCD_RGB(0, 0, 255)
#define LCD_YELLOW  LCD_RGB(255, 255, 0)
#define LCD_CYAN    LCD_RGB(0, 255, 255)
#define LCD_MAGENTA LCD_RGB(255, 0, 255)

void lcd_init(void);
void lcd_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
// Vul met exacte 8-bit RGB (zelfde pad als de blit, geen 565-kwantisatie).
void lcd_fill_rect_rgb(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                       uint8_t r, uint8_t g, uint8_t b);
void lcd_fill_screen(uint16_t color);

// Streaming-blit API. De caller rendert/converteert regel voor regel (of per
// chunk) en pompt RGB888-buffers via DMA naar een eenmalig gezet window.
//   lcd_begin_blit(x,y,w,h)  - window zetten, CS laag
//   lcd_push_dma(rgb, n)     - wacht op vorige DMA, start deze (non-blocking)
//   lcd_end_blit()           - laatste DMA afwachten, CS hoog
// Gebruik twee RGB-buffers om de buffers heen (ping-pong): terwijl de ene DMA't,
// vul je de andere.
void lcd_begin_blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
void lcd_push_dma(const uint8_t *rgb, size_t nbytes);
void lcd_end_blit(void);

#endif // LCD_H
