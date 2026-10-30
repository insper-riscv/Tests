// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// fence.tso (FENCE with fm = 1000, pred = succ = rw) is a legal instruction of the RV32I base of
// the RVI20U32 profile: the core runs it as a plain FENCE and goes on, with no trap and no effect
// on the registers. A plain fence and fence.tso sit around two stores whose order is kept.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x6, 11
    sw   x6, 0(x5)
    .word 0x8330000F              # fence.tso
    li   x7, 22
    sw   x7, 4(x5)
    fence rw, rw
    .word 0x8330000F              # fence.tso
    lw   x8, 0(x5)
    lw   x9, 4(x5)
    add  x10, x8, x9              # 33: nothing was lost across the fences
    sw   x10, 8(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
