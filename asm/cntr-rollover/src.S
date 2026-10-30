// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// mcycle loaded with 0xFFFFFFF0 wraps into the high half: mcycleh goes from 0 to 1 and every
// pair of reads is coherent (the low half is small after the wrap).
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    csrw 0xB80, x0                # mcycleh = 0
    li   x6, 0xFFFFFFF0
    csrw 0xB00, x6                # mcycle = 0xFFFFFFF0
    li   x20, 40
1:  addi x20, x20, -1
    bnez x20, 1b                  # well over 16 cycles
    csrr x7, 0xB80                # mcycleh
    csrr x8, 0xB00                # mcycle, small
    li   x9, 0x1000
    sltu x10, x8, x9              # x10 = 1: the low half wrapped
    sw   x7, 0(x5)
    sw   x10, 4(x5)

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
