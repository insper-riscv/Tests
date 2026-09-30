// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include <stdio.h>

// printf's integer conversions (%d, %x), written to stdout's buffer in RAM
// (see platform/stdio.c). They divide by the base with a div right before a
// rem: two multiply/divide instructions in a row, which the core got wrong
// until insper-riscv/RV32IM#33 (see asm/div-rem-back-to-back).
#define STDOUT_BASE 0x0002FBE0u

struct stdout_buffer {
    volatile unsigned int length;
    volatile unsigned int truncated;
    volatile char data[1024];
};

int main(void) {
    const struct stdout_buffer *out = (const struct stdout_buffer *)STDOUT_BASE;
    const char expected[] = "42 -7 beef";

    printf("%d %d %x", 42, -7, 0xBEEF);

    if (out->length != sizeof(expected) - 1) RV32_FAIL();
    for (unsigned int i = 0; i < out->length; i++) {
        if (out->data[i] != expected[i]) RV32_FAIL();
    }
    RV32_PASS();
}
