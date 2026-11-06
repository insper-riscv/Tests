// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Event 9 (fused multiply or divide operation) on the highest event counter the core has. Writing all
// ones to mcountinhibit shows how many counters there are, and the program selects the event in the
// last of them (index 2 + number of counters). MULH followed by MUL and DIV followed by REM with the
// same operands run once each, so the counter moves by 2. The stored words are that count and a flag that
// says the last counter was reached. A core without event counters stores the same words.
#include "rv32_platform.h"

    .section .text
    .globl main

# Runs the fused pairs on the counter of index \k when it is the last one (x27 = index of the last counter).
    .macro FUSED k
    li   x6, \k
    bne  x6, x27, 9f
    li   x7, 9
    csrw 0x320+\k, x7             # mhpmevent = 9 (fused operation)
    csrr x20, 0xB00+\k
    mulh x15, x11, x12
    mul  x16, x11, x12
    div  x17, x13, x14
    rem  x18, x13, x14
    csrr x21, 0xB00+\k
    sub  x26, x21, x20            # operations fused
    li   x25, 1
    csrw 0x320+\k, x0
9:
    .endm

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x11, 0x89ABCDEF
    li   x12, 0x01234567
    li   x13, 1000
    li   x14, 7

    li   x6, -1
    csrw 0x320, x6
    csrr x29, 0x320
    csrw 0x320, x0
    srli x29, x29, 3              # one bit per counter from bit 0
    li   x27, 2
6:  beqz x29, 7f
    addi x27, x27, 1
    srli x29, x29, 1
    j    6b
7:  li   x26, 2                   # a core without counters stores the expected words
    li   x25, 1

    .irp k, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    FUSED \k
    .endr

    sw   x26, 0(x5)
    sw   x25, 4(x5)

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS
