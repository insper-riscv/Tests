// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 10.5
// ("DIV: divisão com sinal, trunca em direção a zero") — split out
// from the combined asm/mul-div-rem/ test (which bundled all 8
// M-extension opcodes into one pass/fail) so a failure points at
// exactly which one broke — see asm/arithmetic-basic/src.S for the
// same reasoning applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # DIV: 20 / 4 = 5
    addi x10, x0, 20
    addi x11, x0, 4
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: -21 / 3 = -7
    addi x10, x0, -21
    addi x11, x0, 3
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: 7 / 2 = 3 (trunca)
    addi x10, x0, 7
    addi x11, x0, 2
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: -7 / 2 = -3 (trunca em direção a zero)
    addi x10, x0, -7
    addi x11, x0, 2
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: 3 / 5 = 0
    addi x10, x0, 3
    addi x11, x0, 5
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: 10 / 0 → divisão por zero = 0xFFFFFFFF (-1)
    addi x10, x0, 10
    addi x11, x0, 0
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # DIV: 0x80000000 / -1 → overflow = 0x80000000
    lui  x10, 0x80000           # x10 = 0x80000000 (-2147483648)
    addi x11, x0, -1
    div  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
