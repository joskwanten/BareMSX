#ifndef RTC_H
#define RTC_H

// RP-5C01 RTC (poorten 0xB4/0xB5) — zie rtc.c. Vereist voor de echte
// MSX2-BIOS (bootinstellingen in CMOS-bank 2/3).

#include <stdint.h>

typedef struct {
    uint8_t reg;          // registerindex, gelatcht door 0xB4
    uint8_t mode;         // reg 13: bankselect in bits 0-1
    uint8_t banks[4][13]; // bank 0 wordt op reads door de hostklok geschaduwd
} rtc_t;

void rtc_init(rtc_t *rtc);
void rtc_select(rtc_t *rtc, uint8_t value); // 0xB4 out
void rtc_write(rtc_t *rtc, uint8_t value);  // 0xB5 out
uint8_t rtc_read(rtc_t *rtc);               // 0xB5 in

#endif // RTC_H
