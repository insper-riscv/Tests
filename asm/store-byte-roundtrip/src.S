// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 6
// (Loads/Stores) — the SB/LB round-trip sub-case, split out from the
// combined asm/loads-stores/ test (which bundled 8 different
// load/store addressing modes into one pass/fail) so a failure points
// at exactly which one broke — see asm/arithmetic-basic/src.S for the
// same reasoning applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

    # RAM starts at 0x00008000 now (Harvard modificado).
    .equ RESULTS_BASE,   RV32_RAM_BASE
    .equ TEMP_SLOT_BASE, RV32_RAM_BASE + 0x300

main:
    li   x5, RESULTS_BASE
    li   x9, TEMP_SLOT_BASE

    # Teste de SB/LB em TEMP_SLOT_BASE
    addi x15, x0, 0x55
    sb   x15, 0(x9)
    lb   x16, 0(x9)
    sw   x16, 0(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x21, RV32_RAM_END
    addi x22, x0, 1
    sw   x22, -4(x21)          // mailbox = 1

    j rv32_wait_restart
