// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Consecutive CSR instructions with nothing between them: each sees the value
// the previous one wrote, on the same CSR and on different ones.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x28, 1
    li   x29, 2
    li   x30, 3
    csrw   mscratch, x28
    csrrw  x6, mscratch, x29      # x6 = 1
    csrrw  x7, mscratch, x30      # x7 = 2
    csrrs  x8, mscratch, x0       # x8 = 3
    csrrwi x9, mscratch, 7        # x9 = 3
    csrrsi x10, mscratch, 8       # x10 = 7
    csrr   x11, mscratch          # x11 = 15
    li   x28, 0x100
    csrw   mtvec, x28
    csrw   mscratch, x30
    csrr   x12, mtvec             # 0x100
    csrr   x13, mscratch          # 3
    sw   x6, 0(x5)
    sw   x7, 4(x5)
    sw   x8, 8(x5)
    sw   x9, 12(x5)
    sw   x10, 16(x5)
    sw   x11, 20(x5)
    sw   x12, 24(x5)
    sw   x13, 28(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
