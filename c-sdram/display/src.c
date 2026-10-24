// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"

// The program drives the seven-segment display register (0xD0000000): it shows 000001 to 00000F,
// one after the other with a short pause, then 123456, and reads the register back each time. The
// read-back is the check, and HEX5 to HEX0 are left reading 123456. To look at the digits one by
// one, use the monitor: `poke D0000000 000003`.

#define DISPLAY ((volatile unsigned int *)0xD0000000u)
#define PAUSE_ITERATIONS 100u
#define FINAL_VALUE 0x123456u

static void pause(void) {
    for (volatile unsigned int i = 0; i < PAUSE_ITERATIONS; i++) {
    }
}

int main(void) {
    for (unsigned int value = 1; value <= 0xF; value++) {
        *DISPLAY = value;
        if (*DISPLAY != value) RV32_FAIL();
        pause();
    }
    *DISPLAY = FINAL_VALUE;
    if (*DISPLAY != FINAL_VALUE) RV32_FAIL();
    pause();
    RV32_PASS();
}
