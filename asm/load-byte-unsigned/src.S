// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 6
// (Loads/Stores) — the LBU sub-case, split out from the combined
// asm/loads-stores/ test (which bundled 8 different load/store
// addressing modes into one pass/fail) so a failure points at exactly
// which one broke — see asm/arithmetic-basic/src.S for the same
// reasoning applied to the original full.S sections. Seeds its own
// byte at TEST_DATA_BASE (same value full.S itself seeds there)
// rather than depending on any other test having run first.
#include "rv32_platform.h"


    .section .text
    .globl main

    # RAM starts at 0x00008000 now (Harvard modificado).
    .equ RESULTS_BASE,   RV32_RAM_BASE
    .equ TEST_DATA_BASE, RV32_RAM_BASE + 0x200

main:
    li   x5, RESULTS_BASE
    li   x8, TEST_DATA_BASE

    # byte em [2]: 0xFF
    addi x10, x0, -1              # 0xFF
    sb   x10, 2(x8)

    # LBU de 0xFF → 0x000000FF
    lbu  x14, 2(x8)
    sw   x14, 0(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
