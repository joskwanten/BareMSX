#pragma once

#include <stdbool.h>
#include <stdint.h>

bool read_rom(char* filename, uint32_t size, uint8_t* ptr);