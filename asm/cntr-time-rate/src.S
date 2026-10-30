// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// time advances by one tick every 50 cycles at 50 MHz: the cycles between two ticks, read from
// cycle, are close to 50 (45 to 55) and time never goes backwards.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    csrr x6, 0xC01
1:  csrr x7, 0xC01
    beq  x7, x6, 1b               # wait for a tick edge
    csrr x8, 0xC00                # cycle at the edge
    mv   x6, x7
2:  csrr x7, 0xC01
    beq  x7, x6, 2b               # wait for the next tick
    csrr x9, 0xC00
    sub  x10, x9, x8              # cycles between two ticks
    li   x11, 45
    sltu x12, x10, x11
    xori x12, x12, 1              # x12 = 1: at least 45
    li   x13, 56
    sltu x14, x10, x13            # x14 = 1: below 56
    sub  x15, x7, x6              # time moved by exactly 1
    sw   x12, 0(x5)
    sw   x14, 4(x5)
    sw   x15, 8(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
