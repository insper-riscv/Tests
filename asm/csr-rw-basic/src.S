// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// csrrw/csrrs/csrrc and the three immediate forms on mscratch: the old value
// lands in rd and the new one is the operation applied to it. Then the
// field masks of mstatus and mepc, and the read-only misa and mhartid.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x28, 0x12345678
    csrrw  x6, mscratch, x28      # x6 = 0 (reset value)
    csrrw  x7, mscratch, x0       # x7 = 0x12345678; mscratch = 0
    li   x29, 0x0F0F
    csrrs  x8, mscratch, x29      # x8 = 0; mscratch = 0x0F0F
    li   x30, 0x00FF
    csrrc  x9, mscratch, x30      # x9 = 0x0F0F; mscratch = 0x0F00
    csrrwi x10, mscratch, 21      # x10 = 0x0F00; mscratch = 21
    csrrsi x11, mscratch, 10      # x11 = 21; mscratch = 31
    csrrci x12, mscratch, 5       # x12 = 31; mscratch = 26
    csrr   x13, mscratch          # x13 = 26
    li   x28, -1
    csrw   mstatus, x28
    csrr   x16, mstatus           # MPP = 11, MPIE and MIE set, nothing else
    li   x28, 0x80000003
    csrw   mepc, x28
    csrr   x17, mepc              # bits 1:0 read as zero
    csrr   x18, misa              # RV32, I and M
    csrr   x19, mhartid           # 0
    sw   x6, 0(x5)
    sw   x7, 4(x5)
    sw   x8, 8(x5)
    sw   x9, 12(x5)
    sw   x10, 16(x5)
    sw   x11, 20(x5)
    sw   x12, 24(x5)
    sw   x13, 28(x5)
    sw   x16, 32(x5)
    sw   x17, 36(x5)
    sw   x18, 40(x5)
    sw   x19, 44(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
