// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 11.4
// ("Forwarding após MUL: resultado de instrução M usado imediatamente")
// — split out from the combined asm/hazards/ test (which bundled all
// four 11.x hazard sub-cases into one pass/fail) so a failure points
// at exactly which hazard type broke instead of "one of 4 words in
// hazards didn't match" — see asm/arithmetic-basic/src.S for the same
// reasoning applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

    # RAM starts at 0x00008000 now (Harvard modificado).
    .equ RESULTS_BASE, RV32_RAM_BASE

main:
    li   x5, RESULTS_BASE

    addi x10, x0, 6
    addi x11, x0, 7
    mul  x12, x10, x11          # x12 = 42 (stall multdiv)
    add  x13, x12, x0           # x13 = 42 (forwarding do resultado M)
    sw   x13, 0(x5)             # esperado: 42

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
