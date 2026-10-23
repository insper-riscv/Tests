// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 2
// ("LUI e AUIPC") — the AUIPC sub-case, split out from the combined
// asm/lui-auipc/ test (which bundled LUI and AUIPC into one pass/fail)
// so a failure points at exactly which one broke — see
// asm/arithmetic-basic/src.S for the same reasoning applied to the
// original full.S sections.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM now starts at 0x8000)

    # AUIPC: diferença entre dois AUIPC com mesmo imediato — deve ser
    # uma constante (a distância entre as duas instruções, 4 bytes),
    # independente de onde o programa foi carregado.
auipc_test_1:
    auipc x17, 0x1                # x17 = PC1 + 0x1000
auipc_test_2:
    auipc x18, 0x1                # x18 = PC2 + 0x1000
    sub   x19, x18, x17           # = PC2 - PC1 (constante)
    sw    x19, 0(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
