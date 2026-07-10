#pragma once
#include <stdint.h>

typedef uint8_t (*read_byte_t)(void*, uint16_t);
typedef void (*write_byte_t)(void*, uint16_t, uint8_t);

typedef struct {
    void* context;
    read_byte_t read_func;
    write_byte_t write_func;
} memory_context_t;