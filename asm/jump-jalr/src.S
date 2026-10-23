// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 8
// (Jumps) — the JALR sub-case, split out from the combined asm/jumps/
// test (which bundled JAL and JALR into one pass/fail) so a failure
// points at exactly which one broke — see asm/arithmetic-basic/src.S
// for the same reasoning applied to the original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # --- JALR: usa ra de um JAL como retorno ---
    # x14 conta quantas vezes passou pelos pontos certos
    addi x14, x0, 0

    # Faz um JAL para obter ra em x1
    jal  x1, jalr_helper_entry
after_jal_for_jalr:
    addi x14, x14, 2             # +2 se voltou corretamente via JALR
    sw   x14, 0(x5)              # valor esperado = 3 (1 no helper + 2 aqui)

    jal  x0, jalr_done

jalr_helper_entry:
    addi x14, x14, 1             # +1 ao entrar
    # usa JALR com x1 (ra) como destino pra voltar pra after_jal_for_jalr
    jalr x0, x1, 0

jalr_done:

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
