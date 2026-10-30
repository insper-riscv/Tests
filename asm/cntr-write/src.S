// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// mcycle and minstret accept writes and keep counting: the value read right after the write
// is the written one (a write wins over the increment of its own cycle).
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x6, 0x1000
    csrw 0xB00, x6                # mcycle = 0x1000
    csrr x7, 0xB00
    li   x8, 0x77
    csrw 0xB02, x8                # minstret = 0x77
    csrr x9, 0xB02
    sub  x10, x7, x6              # small: the read is a few cycles after the write
    li   x11, 16
    sltu x12, x10, x11            # x12 = 1: 0 <= difference < 16
    sw   x12, 0(x5)
    sw   x9, 4(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
