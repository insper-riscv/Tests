// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Two reads of cycle with instructions in between: the difference is positive and at least the
// number of instructions between them. The high half of cycle is still zero.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    csrr x6, 0xC00                # cycle
    nop
    nop
    nop
    nop
    csrr x7, 0xC00
    sub  x8, x7, x6
    sltu x9, x0, x8               # x9 = 1: the difference is positive
    li   x10, 5
    sltu x11, x8, x10
    xori x11, x11, 1              # x11 = 1: at least 5 cycles for 5 instructions
    csrr x12, 0xC80               # cycleh = 0
    sw   x9, 0(x5)
    sw   x11, 4(x5)
    sw   x12, 8(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
