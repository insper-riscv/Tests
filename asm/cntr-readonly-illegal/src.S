// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// A write to cycle, time, instret or cycleh is an illegal instruction (mcause 2) and changes no
// counter; a read, also by csrrs with x0, is legal.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    la   x6, handler
    csrw mtvec, x6
    li   x20, 0
    csrw 0xC00, x0                # illegal
    csrw 0xC01, x0                # illegal
    csrw 0xC02, x0                # illegal
    csrw 0xC80, x0                # illegal
    csrrs x7, 0xC00, x0           # legal read
    csrr x8, 0xB00                # legal
    csrr x21, mcause              # 2
    sltu x9, x0, x7               # cycle kept counting: x9 = 1
    sw   x20, 0(x5)
    sw   x21, 4(x5)
    sw   x9, 8(x5)
    j    done

handler:
    addi x20, x20, 1
    csrr x28, mepc
    addi x28, x28, 4
    csrw mepc, x28
    mret

done:

    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
