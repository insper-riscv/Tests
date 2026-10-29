// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"

// The program names the six digits of the seven-segment display (register 0xD0000000) so that
// each one can be told apart on the board. Digit k is HEX<k>, HEX0 on the right.
//
// Phase 1 lights one digit at a time with an A, from HEX0 to HEX5, a second or so each; the
// other five digits read 0. Phase 2 writes 543210, so each digit shows its own number, and holds
// it for a few seconds. The display is left reading 543210. The pauses are meant for a person, so
// the program runs on the board with --only (it is not part of the default suite).

#define DISPLAY ((volatile unsigned int *)0xD0000000u)
#define DIGITS 6u
#define BITS_PER_DIGIT 4u
#define MARK 0xAu
#define FINAL_VALUE 0x543210u
#define PAUSE_ITERATIONS 2000000u
#define HOLD_PAUSES 3u

static void pause(void) {
    for (volatile unsigned int i = 0; i < PAUSE_ITERATIONS; i++) {
    }
}

static void show(unsigned int value) {
    *DISPLAY = value;
    if (*DISPLAY != value) RV32_FAIL();
}

int main(void) {
    for (unsigned int digit = 0; digit < DIGITS; digit++) {
        show(MARK << (digit * BITS_PER_DIGIT));
        pause();
    }
    show(FINAL_VALUE);
    for (unsigned int i = 0; i < HOLD_PAUSES; i++) pause();
    RV32_PASS();
}
