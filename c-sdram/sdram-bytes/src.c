// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// Byte and halfword accesses to the SDRAM: stores touch only their bytes (the byte mask goes to
// the chip), loads extend with and without sign.

int main(void) {
    volatile unsigned int   *w = RV32_SDRAM;
    volatile unsigned short *h = (volatile unsigned short *)RV32_SDRAM;
    volatile unsigned char  *b = (volatile unsigned char *)RV32_SDRAM;
    volatile signed short   *hs = (volatile signed short *)RV32_SDRAM;
    volatile signed char    *bs = (volatile signed char *)RV32_SDRAM;

    w[0] = 0x11223344u;
    if (b[0] != 0x44 || b[1] != 0x33 || b[2] != 0x22 || b[3] != 0x11) RV32_FAIL();
    if (h[0] != 0x3344 || h[1] != 0x1122) RV32_FAIL();

    b[1] = 0xAA;
    if (w[0] != 0x1122AA44u) RV32_FAIL();
    h[1] = 0xBEEF;
    if (w[0] != 0xBEEFAA44u) RV32_FAIL();
    b[3] = 0x01;
    b[0] = 0x80;
    if (w[0] != 0x01EFAA80u) RV32_FAIL();

    // sign extension
    if (bs[0] != -128) RV32_FAIL();
    if (b[0] != 128) RV32_FAIL();
    w[1] = 0x0000F00Fu;
    if (hs[2] != -4081) RV32_FAIL();
    if (h[2] != 0xF00F) RV32_FAIL();

    // every byte lane of consecutive words, and a store next to a word that must not change
    for (unsigned int i = 0; i < 64; i++) w[16 + i] = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < 64; i++) b[64 + i * 4 + (i & 3)] = (unsigned char)i;
    for (unsigned int i = 0; i < 64; i++) {
        unsigned int want = 0xFFFFFFFFu & ~(0xFFu << (8 * (i & 3)));
        want |= (unsigned int)i << (8 * (i & 3));
        if (w[16 + i] != want) RV32_FAIL();
    }

    RV32_PASS();
}
