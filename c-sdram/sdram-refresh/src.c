// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_sdram.h"

// The SDRAM keeps its content while the bus is idle for many refresh periods (the controller
// refreshes by itself); read back after a long wait.

int main(void) {
    volatile unsigned int *m = RV32_SDRAM;

    for (unsigned int i = 0; i < 256; i++) m[i * 512] = rv32_sdram_data(i);   // one word per row

    for (volatile unsigned int t = 0; t < 1000; t++) { }                      // far more than a refresh period (7.8 us); t lives in the SDRAM, so each pass costs tens of cycles

    for (unsigned int i = 0; i < 256; i++) {
        if (m[i * 512] != rv32_sdram_data(i)) RV32_FAIL();
    }

    RV32_PASS();
}
