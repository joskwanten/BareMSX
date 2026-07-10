#include "i2ckbd.h"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// --- PicoCalc keyboard I2C pinout (I2C1) ---
#define KBD_I2C   i2c1
#define KBD_SDA   6
#define KBD_SCL   7
#define KBD_ADDR  0x1F
#define KBD_SPEED 10000    // 10 kHz — vereist voor stabiele comms met de STM32

#define KBD_REG_FIFO 0x09  // register met de key-event FIFO

// Open-drain helpers: nooit hoog dríjven, alleen laag trekken of loslaten
// (dan trekt de pull-up 'm hoog). Zo blijft het echt I2C-compatibel.
static inline void pin_low(uint pin)     { gpio_set_dir(pin, GPIO_OUT); gpio_put(pin, 0); }
static inline void pin_release(uint pin) { gpio_set_dir(pin, GPIO_IN); }

// Bus-recovery: onze SWD-flash/reset kan de RP2350 midden in een I2C-read
// resetten, waarna de STM32-slave SDA laag blijft houden en de bus vasthangt.
// Klok SCL tot 9x om de slave los te klokken en genereer daarna een STOP.
static void i2c_bus_recover(void) {
    gpio_init(KBD_SDA); gpio_init(KBD_SCL);
    gpio_pull_up(KBD_SDA); gpio_pull_up(KBD_SCL);
    pin_release(KBD_SDA); pin_release(KBD_SCL);
    sleep_us(10);

    // Zolang SDA laag hangt: pulse SCL (max 9 keer = 1 byte + ack).
    for (int i = 0; i < 9 && gpio_get(KBD_SDA) == 0; i++) {
        pin_low(KBD_SCL);     sleep_us(10);
        pin_release(KBD_SCL); sleep_us(10);
    }

    // STOP-conditie: SDA laag->hoog terwijl SCL hoog is.
    pin_low(KBD_SDA);     sleep_us(10);
    pin_release(KBD_SCL); sleep_us(10);
    pin_release(KBD_SDA); sleep_us(10);
}

void kbd_init(void) {
    i2c_bus_recover();

    i2c_init(KBD_I2C, KBD_SPEED);
    gpio_set_function(KBD_SDA, GPIO_FUNC_I2C);
    gpio_set_function(KBD_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(KBD_SDA);
    gpio_pull_up(KBD_SCL);
}

int kbd_read(uint8_t *state, uint8_t *code) {
    uint8_t reg = KBD_REG_FIFO;
    // Schrijf het register-adres (repeated start: nostop = true).
    if (i2c_write_blocking(KBD_I2C, KBD_ADDR, &reg, 1, true) < 0) {
        return 0; // STM32 niet klaar / NAK
    }

    uint8_t buf[2] = {0, 0};
    if (i2c_read_blocking(KBD_I2C, KBD_ADDR, buf, 2, false) < 0) {
        return 0;
    }

    uint8_t st = buf[0]; // low byte  = state
    uint8_t cd = buf[1]; // high byte = keycode
    if (st == KBD_STATE_IDLE || cd == 0) {
        return 0; // FIFO leeg
    }

    *state = st;
    *code = cd;
    return 1;
}
