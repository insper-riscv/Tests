// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 10.2
// ("MULH: signed × signed → 32 bits altos") — split out from the
// combined asm/mul-div-rem/ test (which bundled all 8 M-extension
// opcodes into one pass/fail) so a failure points at exactly which
// one broke — see asm/arithmetic-basic/src.S for the same reasoning
// applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # MULH: 6 * 7 → high = 0
    addi x10, x0, 6
    addi x11, x0, 7
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MULH: -1 * -1 → produto = +1, high = 0
    addi x10, x0, -1
    addi x11, x0, -1
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MULH: 0x7FFFFFFF * 2 → produto = 0xFFFFFFFE, high = 0
    lui  x10, 0x80000           # 0x80000000
    addi x10, x10, -1           # 0x7FFFFFFF
    addi x11, x0, 2
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MULH: -1 * 1 → produto = -1, high = 0xFFFFFFFF
    addi x10, x0, -1
    addi x11, x0, 1
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MULH: 0x40000000 * 4 → produto = 0x100000000, high = 1
    lui  x10, 0x40000           # 0x40000000
    addi x11, x0, 4
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # MULH: 0x80000000 * 0x80000000 → ambos = -2^31
    #   produto signed = (-2^31)*(-2^31) = 2^62
    #   high = 0x40000000
    lui  x10, 0x80000           # 0x80000000
    lui  x11, 0x80000           # 0x80000000
    mulh x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
