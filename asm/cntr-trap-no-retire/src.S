// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// An ECALL that traps does not retire: between two reads of instret there are the first read, the
// four instructions of the handler (the MRET included) and nothing for the ECALL itself, so the
// difference is five.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    la   x6, handler
    csrw mtvec, x6
    csrr x6, 0xC02                # instret
    ecall                         # trap: not retired
    csrr x7, 0xC02
    sub  x8, x7, x6               # 5
    sw   x8, 0(x5)
    j    done

handler:
    csrr x28, mepc
    addi x28, x28, 4
    csrw mepc, x28
    mret

done:

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
