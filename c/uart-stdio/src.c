// RV32_EXT: M
// RV32_TEST_KIND: unit
#include <stdio.h>
#include <string.h>

#include "rv32_test.h"

/* Fixed protocol shared with the cocotb and physical-board harnesses. */
static const char EXPECTED_NAME[] = "UART";
static const int EXPECTED_VALUE = 42;

int main(void)
{
    char name[16];
    int value = 0;

    printf("READY\n");
    if (scanf("%15s %d", name, &value) != 2) {
        RV32_FAIL(1);
    }
    if (strcmp(name, EXPECTED_NAME) != 0) {
        RV32_FAIL(2);
    }
    if (value != EXPECTED_VALUE) {
        RV32_FAIL(3);
    }

    printf("Hello, %s! value=%d\n", name, value);
    RV32_PASS();
}
