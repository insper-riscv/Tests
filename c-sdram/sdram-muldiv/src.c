// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// Multiplications and divisions whose operands come from the SDRAM: the pipeline stops for the
// multiplier or divider while the memory stage may be waiting on the SDRAM, and the other way
// around. The same arithmetic on operands in RAM is the reference.

#define N 64

static unsigned int ref_a[N + 2];

int main(void) {
    volatile unsigned int *m = RV32_SDRAM;

    for (unsigned int i = 0; i < N + 2; i++) {
        ref_a[i] = i + 3;
        m[i] = i + 3;
    }

    unsigned int want = 0;
    for (unsigned int i = 0; i < N; i++) {
        want += ref_a[i] * ref_a[i + 1];
        want += (ref_a[i] * 7u + 1u) / ref_a[i + 1];
        want += (ref_a[i + 2] * 13u) % ref_a[i + 1];
    }

    unsigned int sum = 0;
    for (unsigned int i = 0; i < N; i++) {
        sum += m[i] * m[i + 1];                    // mul between two SDRAM loads
        sum += (m[i] * 7u + 1u) / m[i + 1];        // div whose divisor is loaded next
        sum += (m[i + 2] * 13u) % m[i + 1];        // rem right after a load
    }
    if (sum != want) RV32_FAIL();

    // a store right after a multiply, to the SDRAM, then read back
    for (unsigned int i = 0; i < N; i++) m[256 + i] = ref_a[i] * ref_a[i];
    for (unsigned int i = 0; i < N; i++) {
        if (m[256 + i] != ref_a[i] * ref_a[i]) RV32_FAIL();
    }

    RV32_PASS();
}
