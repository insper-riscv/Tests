// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// The result of a CSR instruction feeds the next instruction without a wait,
// and its write operand can come from the instruction right before it (ALU
// result or a load).
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x28, 0x40
    csrrw  x6, mscratch, x28      # x6 = 0; mscratch = 0x40
    csrr   x7, mscratch           # x7 = 0x40
    addi   x8, x7, 1              # x8 = 0x41, uses the CSR result at once
    add    x9, x8, x7             # x9 = 0x81
    addi   x29, x0, 0x7F
    csrrw  x10, mscratch, x29     # operand from the ALU result just before: x10 = 0x40
    li   x28, 0xCAFE
    sw   x28, 128(x5)
    lw   x30, 128(x5)
    csrrw  x11, mscratch, x30     # operand from a load just before: x11 = 0x7F
    csrr   x12, mscratch          # x12 = 0xCAFE
    sw   x6, 0(x5)
    sw   x7, 4(x5)
    sw   x8, 8(x5)
    sw   x9, 12(x5)
    sw   x10, 16(x5)
    sw   x11, 20(x5)
    sw   x12, 24(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
