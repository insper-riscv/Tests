#ifndef RV32_SDRAM_H
#define RV32_SDRAM_H

/* The SDRAM of the SDRAM platform (TopLevel/platforms/sdram): 64 MB of data at a fixed
 * address, reached by the core as ordinary memory. Keep in step with the region of
 * platform.yaml and memory.sdram_base of the platform's config. */
#define RV32_SDRAM_BASE  0x40000000u
#define RV32_SDRAM_WORDS (1u << 24)

#define RV32_SDRAM ((volatile unsigned int *)RV32_SDRAM_BASE)

/* The data of word i: it never repeats for different i inside the SDRAM, so a word that
 * lands at another address shows up as a wrong value. */
static inline unsigned int rv32_sdram_data(unsigned int i) {
    unsigned int x = i ^ 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

#endif
