// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 11.3
// ("Load-use hazard: LW seguido imediatamente de instrução
// dependente") — split out from the combined asm/hazards/ test (which
// bundled all four 11.x hazard sub-cases into one pass/fail) so a
// failure points at exactly which hazard type broke instead of "one
// of 4 words in hazards didn't match" — see asm/arithmetic-basic/src.S
// for the same reasoning applied to the original full.S sections.
// Needs a RAM base pointer (x8), carried over from full.S's own
// TEST_DATA_BASE — it writes its own fresh value there rather than
// reading pre-seeded data, so no data-init preamble is needed, just
// the base pointer itself.
#include "rv32_platform.h"


    .section .text
    .globl main

    # RAM starts at 0x00008000 now (Harvard modificado) — both bases
    # shifted up by that same offset, still 0x200 apart.
    .equ RESULTS_BASE,   RV32_RAM_BASE
    .equ TEST_DATA_BASE, RV32_RAM_BASE + 0x200

main:
    li   x5, RESULTS_BASE
    li   x8, TEST_DATA_BASE

    addi x10, x0, 42
    sw   x10, 0x7C(x8)          # RAM[0x27C] = 42 (area de teste, longe dos resultados)
    lw   x11, 0x7C(x8)          # x11 = 42
    add  x12, x11, x0           # x12 = x11 (load-use hazard: stall de 1 ciclo)
    sw   x12, 0(x5)             # esperado: 42

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
