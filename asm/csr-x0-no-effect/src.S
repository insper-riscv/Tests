// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// rs1 = x0 (or zimm = 0) makes csrrs/csrrc read without writing, a register
// that merely holds zero does the same, csrrw always writes, and rd = x0
// leaves x0 at zero.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x28, 0xAA
    csrw   mscratch, x28
    csrrs  x6, mscratch, x0       # x6 = 0xAA
    csrrc  x7, mscratch, x0       # x7 = 0xAA
    csrrsi x8, mscratch, 0        # x8 = 0xAA
    csrrci x9, mscratch, 0        # x9 = 0xAA
    csrr   x10, mscratch          # still 0xAA
    li   x28, 0x55
    csrw   mscratch, x28
    li   x29, 0
    csrrs  x11, mscratch, x29     # x11 = 0x55
    csrrc  x12, mscratch, x29     # x12 = 0x55
    csrr   x13, mscratch          # still 0x55
    csrrw  x14, mscratch, x0      # x14 = 0x55; csrrw writes even with rs1 = x0
    csrr   x16, mscratch          # 0
    csrrw  x0, mscratch, x28      # rd = x0: no register is written
    mv     x17, x0                # x0 still reads zero
    sw   x6, 0(x5)
    sw   x7, 4(x5)
    sw   x8, 8(x5)
    sw   x9, 12(x5)
    sw   x10, 16(x5)
    sw   x11, 20(x5)
    sw   x12, 24(x5)
    sw   x13, 28(x5)
    sw   x14, 32(x5)
    sw   x16, 36(x5)
    sw   x17, 40(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
