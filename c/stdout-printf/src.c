// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdio.h>

// printf, puts and putchar write to stdout's buffer in RAM (see
// platform/stdio.c). The layout below is that file's: a header with the
// length and a "truncated" flag, then the bytes. Text only: the integer
// conversions are in stdout-printf-int.
#define STDOUT_BASE RV32_STDOUT_BASE

struct stdout_buffer {
    volatile unsigned int length;
    volatile unsigned int truncated;
    volatile char data[1024];
};

int main(void) {
    const struct stdout_buffer *out = (const struct stdout_buffer *)STDOUT_BASE;
    const char expected[] = "hello riscv c\nsecond line\nx";

    printf("hello %s %c\n", "riscv", 'c');
    puts("second line");
    putchar('x');

    if (out->length != sizeof(expected) - 1) RV32_FAIL();
    for (unsigned int i = 0; i < out->length; i++) {
        if (out->data[i] != expected[i]) RV32_FAIL();
    }
    if (out->truncated != 0) RV32_FAIL();
    RV32_PASS();
}
