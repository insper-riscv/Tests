// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 7
// (Branches) — the BNE sub-case, split out from the combined
// asm/branches/ test (which bundled 6 different branch opcodes into
// one pass/fail) so a failure points at exactly which one broke — see
// asm/arithmetic-basic/src.S for the same reasoning applied to the
// original full.S sections. Writes 1 if the branch was taken, 0 if
// not.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # BNE: 5 != 6 → 1
    addi x10, x0, 5
    addi x11, x0, 6
    bne  x10, x11, bne_taken
    addi x12, x0, 0
    sw   x12, 0(x5)
    jal  x0, bne_done
bne_taken:
    addi x12, x0, 1
    sw   x12, 0(x5)
bne_done:

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
