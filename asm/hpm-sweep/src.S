// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// The 29 event counters of Zihpm, index by index, whatever number of them the core has and whatever
// their width. Writing all ones to mcountinhibit shows which indices have a counter (bits 3 and up).
// For each index from 3 to 31 the program checks that mhpmeventK keeps 4 bits (an index without a
// counter reads zero), that mhpmcounterK and mhpmcounterKh read back what the width keeps (zero without
// a counter) and that the read-only mirrors agree, and that a write to either mirror is an illegal
// instruction. For each real counter the bit of mcountinhibit stops the taken-branch event and clearing
// it makes the counter move again. A last block reads the 64-bit value of counter 3 with the
// high, low, high sequence across the carry between the halves. The stored words are the AND of every
// check (1), the number of illegal instructions (58, two per index), the shape of the mcountinhibit
// mask (1) and the result of the 64-bit read (1).
#include "rv32_platform.h"

    .section .text
    .globl main

# Checks index \k: x29 has the mask of mcountinhibit, x30 and x31 the high and low halves a real counter
# keeps when all ones are written. x28 is the AND of the checks, x27 the number of the checks.
    .macro CHECK k
    srli x6, x29, \k
    andi x6, x6, 1                # 1 when the index has a real counter
    sub  x6, x0, x6               # all ones when real, zero when not
    # the selector keeps 4 bits
    li   x7, 0xFFFFFFF9
    csrw 0x320+\k, x7
    csrr x8, 0x320+\k
    andi x9, x6, 9
    xor  x8, x8, x9
    seqz x8, x8
    and  x28, x28, x8
    csrw 0x320+\k, x0
    # the halves keep what the width keeps, the mirrors agree
    li   x7, -1
    csrw 0xB00+\k, x7
    csrw 0xB80+\k, x7
    csrr x10, 0xB00+\k
    csrr x11, 0xC00+\k
    csrr x12, 0xB80+\k
    csrr x13, 0xC80+\k
    and  x14, x31, x6
    xor  x14, x14, x10
    seqz x14, x14
    and  x28, x28, x14
    and  x14, x30, x6
    xor  x14, x14, x12
    seqz x14, x14
    and  x28, x28, x14
    xor  x14, x10, x11
    seqz x14, x14
    and  x28, x28, x14
    xor  x14, x12, x13
    seqz x14, x14
    and  x28, x28, x14
    csrw 0xB00+\k, x0
    csrw 0xB80+\k, x0
    # a write to a mirror is an illegal instruction
    csrw 0xC00+\k, x0
    csrw 0xC80+\k, x0
    # the inhibit bit stops the counter
    beqz x6, 9f
    li   x7, 3
    csrw 0x320+\k, x7             # taken branch
    li   x7, 1 << \k
    csrw 0x320, x7
    csrr x10, 0xB00+\k
    li   x14, 8
1:  addi x14, x14, -1
    bnez x14, 1b
    csrr x11, 0xB00+\k
    sub  x12, x11, x10            # 0: inhibited
    seqz x12, x12
    and  x28, x28, x12
    csrw 0x320, x0
    csrr x10, 0xB00+\k
    li   x14, 8
2:  addi x14, x14, -1
    bnez x14, 2b
    csrr x11, 0xB00+\k
    sub  x13, x11, x10            # 7: counting again
    snez x13, x13
    and  x28, x28, x13
    csrw 0x320+\k, x0
    csrw 0xB00+\k, x0
9:
    .endm

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    la   x6, handler
    csrw mtvec, x6
    li   x20, 0
    li   x28, 1

    # the indices that have a counter: the bits of mcountinhibit from 3 up
    li   x6, -1
    csrw 0x320, x6
    csrr x29, 0x320
    csrw 0x320, x0
    andi x6, x29, 2               # bit 1 reads zero
    seqz x26, x6
    srli x29, x29, 3
    slli x29, x29, 3              # bits 3 and up
    srli x6, x29, 3
    addi x7, x6, 1
    and  x7, x7, x6
    seqz x7, x7                   # the mask is a run of ones from bit 3
    and  x26, x26, x7

    # what a real counter keeps of all ones in each half
    li   x6, -1
    csrw 0xB03, x6
    csrw 0xB83, x6
    csrr x31, 0xB03
    csrr x30, 0xB83
    csrw 0xB03, x0
    csrw 0xB83, x0
    addi x6, x31, 1
    and  x6, x6, x31
    seqz x6, x6                   # the low half keeps a run of low bits
    and  x26, x26, x6
    li   x7, -1
    sub  x8, x31, x7              # zero when the low half keeps 32 bits
    seqz x8, x8
    sltu x9, x0, x30
    sub  x9, x9, x8               # a high half needs the whole low half
    slti x9, x9, 1
    and  x26, x26, x9

    .irp k, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    CHECK \k
    .endr

    # the 64-bit read of counter 3 across the carry, when it has a high half
    li   x25, 1
    beqz x30, 5f
    li   x7, 3
    csrw 0x323, x7                # taken branch
    li   x7, -8
    csrw 0xB03, x7
    csrw 0xB83, x0
    li   x14, 16
3:  addi x14, x14, -1
    bnez x14, 3b
4:  csrr x10, 0xC83
    csrr x11, 0xC03
    csrr x12, 0xC83
    bne  x10, x12, 4b
    addi x10, x10, -1
    sltiu x25, x11, 16
    seqz x10, x10
    and  x25, x25, x10            # high half 1 and a low half below 16
    csrw 0x323, x0
5:
    and  x28, x28, x26
    sw   x28, 0(x5)
    sw   x20, 4(x5)
    sw   x26, 8(x5)
    sw   x25, 12(x5)
    j    done

handler:
    addi x20, x20, 1
    csrr x17, mepc
    addi x17, x17, 4
    csrw mepc, x17
    mret

done:

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS
