#ifndef RV32_SDRAM_H
#define RV32_SDRAM_H

#include "rv32_platform.h"

/* The SDRAM is the RAM of the SDRAM platform: .data and .bss from its base, the stack from its
 * top, and the words the host shares with the program at the very top. The programs of c-sdram/
 * test a window of it that none of those use: 16 MB from 16 MB above the base, away from the
 * program's own data and from the stack. */
#define RV32_SDRAM_TEST_BASE  (RV32_RAM_BASE + 0x01000000u)
#define RV32_SDRAM_TEST_WORDS (1u << 22)

#define RV32_SDRAM ((volatile unsigned int *)RV32_SDRAM_TEST_BASE)

/* The data of word i: it never repeats for different i inside the chip, so a word that lands at
 * another address shows up as a wrong value. */
static inline unsigned int rv32_sdram_data(unsigned int i) {
    unsigned int x = i ^ 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

#endif
