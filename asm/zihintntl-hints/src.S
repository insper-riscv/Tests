// RV32_EXT: Zicsr,Zihintntl
// RV32_TEST_KIND: memory
//
// Zihintntl: the four hints ntl.p1, ntl.pall, ntl.s1 and ntl.all (add x0, x0, x2 to x5) execute as
// nop. The registers x2 to x5 hold nonzero values that the hints read, and the hints run
// back to back and right before an instruction that reads them. The words stored are:
//  0 to 3. x2, x3, x4 and x5, which no hint changes;
//  4. x0, which still reads zero;
//  5. x7 = x0 | x5 after the last hint, so x0 forwards zero and the result is x5;
//  6. the exception count, which is 0 because the hints are legal.
// A trap handler counts the exceptions and skips the instruction. The stack pointer (x2) is saved in
// x28 and restored before the end of the test.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    mv   x28, x2                  # save the stack pointer, x2 is used as a hint operand
    li   x20, RV32_RAM_BASE       # x20 = RESULTS_BASE
    li   x30, 0                   # exception count
    la   x6, handler
    csrw mtvec, x6
    li   x2, 0x22
    li   x3, 0x33
    li   x4, 0x44
    li   x5, 0x55

    ntl.p1
    ntl.pall
    ntl.s1
    ntl.all
    ntl.all
    ntl.s1
    ntl.pall
    ntl.p1
    or   x7, x0, x5               # x0 forwards zero right behind the hint

    sw   x2, 0(x20)               # 0x22
    sw   x3, 4(x20)               # 0x33
    sw   x4, 8(x20)               # 0x44
    sw   x5, 12(x20)              # 0x55
    sw   x0, 16(x20)              # 0
    sw   x7, 20(x20)              # 0x55
    sw   x30, 24(x20)             # 0
    mv   x2, x28                  # restore the stack pointer

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS

    # Counts the exception and resumes after the faulting instruction.
handler:
    addi x30, x30, 1
    csrr x31, mepc
    addi x31, x31, 4
    csrw mepc, x31
    mret
