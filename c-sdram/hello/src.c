// RV32_EXT: M
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdio.h>

// "Hello, world!" in C, on the console of the JTAG UART.
//
// On the board, start the console first and run the program while it listens:
//
//   riscv-tools --config tools/riscv_build/config.sdram.yaml console --seconds 60
//
// printf also mirrors every byte to the UART once a host has scanned it, so the program waits
// for the host before printing (bit 17 of the UART status register, with a bound so a board
// with nobody attached still finishes). The same bytes land in the stdout buffer, which this
// program checks, so it is self-checking in simulation as well.

#define UART_STATUS ((volatile unsigned int *)0xC0000008u)
#define HOST_ATTACHED(status) (((status) >> 17) & 1u)
#define ATTACH_POLLS 40000000u

struct stdout_buffer {
    volatile unsigned int length;
    volatile unsigned int truncated;
    volatile char data[1024];
};

int main(void) {
    const struct stdout_buffer *out = (const struct stdout_buffer *)RV32_STDOUT_BASE;
    const char expected[] = "Hello, world!\nRISC-V RV32IM in C, 3 of 3\n";

    for (unsigned int polls = 0; polls < ATTACH_POLLS && !HOST_ATTACHED(*UART_STATUS); polls++) {
    }

    puts("Hello, world!");
    printf("RISC-V RV32IM in C, %d of %d\n", 3, 3);

    if (out->length != sizeof(expected) - 1) RV32_FAIL();
    for (unsigned int i = 0; i < out->length; i++) {
        if (out->data[i] != expected[i]) RV32_FAIL();
    }
    if (out->truncated != 0) RV32_FAIL();
    RV32_PASS();
}
