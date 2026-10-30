// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Between two reads of instret separated by seven simple instructions the difference is eight:
// the seven instructions and the first read, which retires before the second one runs.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    csrr x6, 0xC02                # instret
    addi x20, x0, 1
    addi x21, x0, 2
    addi x22, x0, 3
    addi x23, x0, 4
    addi x24, x0, 5
    addi x25, x0, 6
    addi x26, x0, 7
    csrr x7, 0xC02
    sub  x8, x7, x6               # 8
    csrr x9, 0xC82                # instreth = 0
    sw   x8, 0(x5)
    sw   x9, 4(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
