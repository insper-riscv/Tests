// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// The classic memory tests on the SDRAM: the data bus, the address bus and a window.

int main(void) {
    volatile unsigned int *m = RV32_SDRAM;

    // data bus: a walking one and a walking zero in the same word
    for (unsigned int b = 0; b < 32; b++) {
        m[0] = 1u << b;
        if (m[0] != (1u << b)) RV32_FAIL();
        m[0] = ~(1u << b);
        if (m[0] != ~(1u << b)) RV32_FAIL();
    }

    // address bus: one word at every power-of-two word offset (and offset 0), all written
    // before any is read, so a stuck or crossed address line puts a value in another word
    for (unsigned int k = 0; k < 24; k++) m[1u << k] = 0xA0000000u | k;
    m[0] = 0xDEADBEEFu;
    for (unsigned int k = 0; k < 24; k++) {
        if (m[1u << k] != (0xA0000000u | k)) RV32_FAIL();
    }
    if (m[0] != 0xDEADBEEFu) RV32_FAIL();

    // a window of words with address-dependent data (they cross rows and banks)
    for (unsigned int i = 0; i < 4096; i++) m[i + 8] = rv32_sdram_data(i);
    for (unsigned int i = 0; i < 4096; i++) {
        if (m[i + 8] != rv32_sdram_data(i)) RV32_FAIL();
    }

    // the last word of the chip
    m[RV32_SDRAM_WORDS - 1] = 0x600DF00Du;
    if (m[RV32_SDRAM_WORDS - 1] != 0x600DF00Du) RV32_FAIL();

    RV32_PASS();
}
