// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 10.1
// ("MUL: 32 bits baixos do produto") — split out from the combined
// asm/mul-div-rem/ test (which bundled all 8 M-extension opcodes into
// one pass/fail) so a failure points at exactly which one broke — see
// asm/arithmetic-basic/src.S for the same reasoning applied to the
// original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # MUL: 6 * 7 = 42
    addi x10, x0, 6
    addi x11, x0, 7
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MUL: -3 * 5 = -15
    addi x10, x0, -3
    addi x11, x0, 5
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MUL: -6 * -7 = 42
    addi x10, x0, -6
    addi x11, x0, -7
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MUL: 0 * 12345 = 0
    addi x10, x0, 0
    lui  x11, 0x03              # x11 = 0x3000 (qualquer valor grande)
    addi x11, x0, 123           # simplificamos para 123
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MUL: 1 * -1 = -1 (0xFFFFFFFF)
    addi x10, x0, 1
    addi x11, x0, -1
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MUL: 0x10000 * 0x10000 = 0x100000000 → lower 32 bits = 0
    lui  x10, 0x10              # x10 = 0x10000
    lui  x11, 0x10              # x11 = 0x10000
    mul  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
