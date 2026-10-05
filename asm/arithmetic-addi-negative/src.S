// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 1
// (Aritmética básica) — the ADDI-with-negative-immediate sub-case,
// split out from the combined asm/arithmetic-basic/ test (which
// bundled ADD/SUB/ADDI into one pass/fail) so a failure points at
// exactly which one broke — same reasoning as arithmetic-basic's own
// split from full.S, one level deeper.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # ADDI com negativo: x15 = -1
    addi x15, x0, -1
    sw   x15, 0(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
