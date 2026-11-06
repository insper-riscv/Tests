// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdio.h>

// The program that the update tests send: bank B, version 2. It is not a test of its own (config.sdram-flash.yaml
// does not look in this folder): `make flash-update-images` builds it with config.sdram-flash-bank-b.yaml, links it
// at the start of bank B after the header page, packs it as version 2 and writes the image to the uart_input.txt of
// the update tests. It checks that it runs from bank B, clears the survivor word the update programs leave, prints
// one line and passes.

#define SURVIVOR ((volatile unsigned int *)(RV32_RAM_BASE + 0x03000000u))

int main(void) {
    unsigned int pc = (unsigned int)(unsigned long)&main;
    if (pc < RV32_BANK_B_BASE || pc >= RV32_BANK_B_BASE + RV32_BANK_B_SIZE) RV32_FAIL(1);
    *SURVIVOR = 0;
    printf("bank B running, version 2\n");
    RV32_PASS();
}
