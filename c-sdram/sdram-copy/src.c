// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// Copies between RAM and SDRAM, by word and by byte, in both directions.

#define WORDS 256

static unsigned int src[WORDS];
static unsigned int dst[WORDS];

int main(void) {
    volatile unsigned int *m = RV32_SDRAM;
    volatile unsigned char *mb = (volatile unsigned char *)RV32_SDRAM;

    for (unsigned int i = 0; i < WORDS; i++) src[i] = rv32_sdram_data(i);

    // RAM to SDRAM by word, SDRAM to RAM by word
    for (unsigned int i = 0; i < WORDS; i++) m[i] = src[i];
    for (unsigned int i = 0; i < WORDS; i++) dst[i] = m[i];
    for (unsigned int i = 0; i < WORDS; i++) {
        if (dst[i] != src[i]) RV32_FAIL();
    }

    // RAM to SDRAM by byte, SDRAM to RAM by word
    volatile unsigned char *sb = (volatile unsigned char *)src;
    for (unsigned int i = 0; i < WORDS * 4; i++) mb[4096 + i] = sb[i];
    for (unsigned int i = 0; i < WORDS; i++) dst[i] = m[1024 + i];
    for (unsigned int i = 0; i < WORDS; i++) {
        if (dst[i] != src[i]) RV32_FAIL();
    }

    // SDRAM to SDRAM, overlapping a row boundary of the chip (512 words per row)
    for (unsigned int i = 0; i < WORDS; i++) m[400 + i] = m[i];
    for (unsigned int i = 0; i < WORDS; i++) {
        if (m[400 + i] != src[i]) RV32_FAIL();
    }

    RV32_PASS();
}
