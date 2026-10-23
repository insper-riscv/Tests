// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 10.7
// ("REM: resto com sinal, sinal segue dividendo") — split out from
// the combined asm/mul-div-rem/ test (which bundled all 8 M-extension
// opcodes into one pass/fail) so a failure points at exactly which
// one broke — see asm/arithmetic-basic/src.S for the same reasoning
// applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # REM: 20 % 4 = 0
    addi x10, x0, 20
    addi x11, x0, 4
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # REM: 7 % 2 = 1
    addi x10, x0, 7
    addi x11, x0, 2
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # REM: -7 % 2 = -1 (sinal segue o dividendo)
    addi x10, x0, -7
    addi x11, x0, 2
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # REM: 7 % -2 = 1 (sinal segue o dividendo)
    addi x10, x0, 7
    addi x11, x0, -2
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # REM: 10 % 0 → divisão por zero, resto = dividendo = 10
    addi x10, x0, 10
    addi x11, x0, 0
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # REM: 0x80000000 % -1 → overflow, resto = 0
    lui  x10, 0x80000           # 0x80000000
    addi x11, x0, -1
    rem  x12, x10, x11
    sw   x12, 0(x5)
    addi x5, x5, 4

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
