// RV32_EXT: Zicsr,Zihintpause
// RV32_TEST_KIND: memory
//
// Zihintpause: pause executes as nop. A busy-wait loop of 8 iterations runs a pause in each one,
// and four pauses run back to back before an instruction that reads a register written just
// before them. The words stored are:
//  0. the iteration count, 8;
//  1. the sum of 3 added in each iteration, 24;
//  2. the exception count, which is 0 because pause is legal;
//  3. the register written before the four pauses, 0x5A, unchanged.
// A trap handler counts the exceptions and skips the instruction.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x20, RV32_RAM_BASE       # x20 = RESULTS_BASE
    li   x30, 0                   # exception count
    la   x6, handler
    csrw mtvec, x6
    li   x6, 0                    # iteration count
    li   x7, 8                    # iteration limit
    li   x8, 0                    # sum

wait_loop:
    pause
    addi x6, x6, 1
    addi x8, x8, 3
    bne  x6, x7, wait_loop

    li   x9, 0x5A
    pause
    pause
    pause
    pause
    addi x10, x9, 0               # reads x9 right behind the pauses

    sw   x6, 0(x20)               # 8
    sw   x8, 4(x20)               # 24
    sw   x30, 8(x20)              # 0
    sw   x10, 12(x20)             # 0x5A

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS

    # Counts the exception and resumes after the faulting instruction.
handler:
    addi x30, x30, 1
    csrr x31, mepc
    addi x31, x31, 4
    csrw mepc, x31
    mret
