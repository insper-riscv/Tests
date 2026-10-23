// RV32_TEST_KIND: memory
//
// Ported from RV32IM's tests/FPGA/core/asm_tests/full.S, section 9
// ("Teste de x0 — não pode ser modificado") — see
// asm/arithmetic-basic/src.S for why this is split out from the
// original single full.S file.
#include "rv32_platform.h"


    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE (RAM starts at 0x8000)

    addi x0, x0, 5                # deve ser ignorado
    sw   x0, 0(x5)                # grava 0
    addi x5, x5, 4

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr — 0x0002FFFC).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
