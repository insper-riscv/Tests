// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 8
// (Jumps) — the JAL sub-case, split out from the combined asm/jumps/
// test (which bundled JAL and JALR into one pass/fail) so a failure
// points at exactly which one broke — see asm/arithmetic-basic/src.S
// for the same reasoning applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # --- JAL: testa salto + fluxo de controle ---
    # x13 vai acumular um padrão conhecido se passar
    addi x13, x0, 0

    jal  x1, jal_helper          # salta para jal_helper
after_jal:
    addi x13, x13, 1             # +1 se voltou do helper
    sw   x13, 0(x5)              # valor esperado = 3 (2 no helper + 1 aqui)

    jal  x0, jal_done            # segue para o fim

jal_helper:
    addi x13, x13, 2             # +2 ao entrar
    jal  x0, after_jal

jal_done:

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
