// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// Loops over the SDRAM: a load followed by a branch back, the shape that needs the pipeline to keep a
// taken branch while the memory stage waits.

int main(void) {
    volatile unsigned int *m = RV32_SDRAM;
    unsigned int want = 0;

    for (unsigned int i = 0; i < 512; i++) {
        m[i] = i ^ 0x5555u;
        want += i ^ 0x5555u;
    }

    unsigned int sum = 0;
    unsigned int i = 0;
    while (i < 512) {                 // lw, add, addi, bne
        sum += m[i];
        i++;
    }
    if (sum != want) RV32_FAIL();

    // read from the end, with an early exit that depends on the loaded value
    unsigned int n = 0;
    for (int j = 511; j >= 0; j--) {
        if (m[j] == (0u ^ 0x5555u)) break;      // m[0]
        n++;
    }
    if (n != 511) RV32_FAIL();

    // nested: a row of loads inside an outer loop that also stores
    for (unsigned int r = 0; r < 8; r++) {
        for (unsigned int c = 0; c < 16; c++) m[1024 + r * 16 + c] = r + c;
    }
    unsigned int tot = 0;
    for (unsigned int r = 0; r < 8; r++) {
        for (unsigned int c = 0; c < 16; c++) {
            if (m[1024 + r * 16 + c] != r + c) RV32_FAIL();
            tot += m[1024 + r * 16 + c];
        }
    }
    if (tot != 1408) RV32_FAIL();        // 16 * (0+1+...+7) + 8 * (0+1+...+15)

    RV32_PASS();
}
