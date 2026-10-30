// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// A division holds the pipeline for the cycles of the divider, while instret counts it as one
// instruction: the cycle difference around it is at least 33, and the instret difference is 3
// (the read of instret itself, the division and the read of cycle between them).
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x11, 1000
    li   x12, 7
    csrr x6, 0xC00                # cycle
    csrr x13, 0xC02               # instret
    div  x10, x11, x12            # 142
    csrr x7, 0xC00
    csrr x14, 0xC02
    sub  x8, x7, x6
    li   x15, 33
    sltu x9, x8, x15
    xori x9, x9, 1                # x9 = 1: at least 33 cycles
    sub  x16, x14, x13            # csrr instret, div, csrr cycle: 3
    sw   x9, 0(x5)
    sw   x16, 4(x5)
    sw   x10, 8(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
