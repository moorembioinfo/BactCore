#ifndef BITPACK_H
#define BITPACK_H

#include <stdint.h>

extern uint8_t BASE_LUT[256];

static inline void init_base_lut(void) {
    for (int i = 0; i < 256; ++i) BASE_LUT[i] = 0;
    BASE_LUT['A'] = 1u << 0; BASE_LUT['a'] = 1u << 0;
    BASE_LUT['C'] = 1u << 1; BASE_LUT['c'] = 1u << 1;
    BASE_LUT['G'] = 1u << 2; BASE_LUT['g'] = 1u << 2;
    BASE_LUT['T'] = 1u << 3; BASE_LUT['t'] = 1u << 3;
}

#endif
