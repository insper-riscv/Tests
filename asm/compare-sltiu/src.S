// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 4
// (Comparações) — the SLTIU sub-case, split out from the combined
// asm/comparisons/ test (which bundled 4 different compare opcodes
// into one pass/fail) so a failure points at exactly which one broke
// — see asm/arithmetic-basic/src.S for the same reasoning applied to
// the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # SLTIU: 3 < 1 unsigned → 0
    addi x11, x0, 3
    sltiu x11, x11, 1
    sw   x11, 0(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
