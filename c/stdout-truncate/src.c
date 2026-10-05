// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdio.h>

// stdout's buffer is linear and 1024 bytes (platform/stdio.c): writing
// more fills it, drops the rest and sets the "truncated" flag.
#define STDOUT_BASE RV32_STDOUT_BASE

struct stdout_buffer {
    volatile unsigned int length;
    volatile unsigned int truncated;
    volatile char data[1024];
};

int main(void) {
    const struct stdout_buffer *out = (const struct stdout_buffer *)STDOUT_BASE;

    for (int i = 0; i < 1024; i++) putchar('a');
    if (out->length != 1024 || out->truncated != 0) RV32_FAIL();

    putchar('b');
    if (out->length != 1024 || out->truncated != 1) RV32_FAIL();
    if (out->data[0] != 'a' || out->data[1023] != 'a') RV32_FAIL();
    RV32_PASS();
}
