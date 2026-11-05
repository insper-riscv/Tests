// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// The 29 event counter indices of Zihpm on a core with 29 counters of 64 bits (hpm_counters = 29,
// hpm_width = 64). The expectations are fixed: every index has a counter whose high half keeps
// all 32 bits. A core with fewer counters or with narrower ones fails this program. For each index the
// program checks that mhpmeventK keeps 4 bits, that mhpmcounterK and mhpmcounterKh keep what the width keeps, that the read-only mirrors
// agree, and that a write to either mirror is an illegal instruction; for each real counter the bit of
// mcountinhibit stops the taken-branch event. The reserved addresses 0x321, 0x322, 0xB01 and 0xB81 are
// illegal instructions. A last block counts across the carry between the halves with the high, low,
// high read, once to a high half of 1 and once from the largest high half to zero (the width truncates).
// The stored words are the AND of every check (1), the number of illegal instructions (62), the shape of
// the mcountinhibit mask (1), the read across the carry (1) and the read across the wrap (1).
#include "rv32_platform.h"

    .equ REAL, 29                      # counters the core has
    .equ HIGH_KEEP, 0xFFFFFFFF              # bits the high half of a counter keeps
    .equ MASK, 5 | (((1 << REAL) - 1) << 3)   # mcountinhibit after writing all ones

    .section .text
    .globl main

# Checks index \k. x28 is the AND of the checks.
    .macro CHECK k
    .if \k - 2 <= REAL
    .equ KEEP_LO, 0xFFFFFFFF
    .equ KEEP_HI, HIGH_KEEP
    .equ KEEP_EV, 9
    .else
    .equ KEEP_LO, 0
    .equ KEEP_HI, 0
    .equ KEEP_EV, 0
    .endif
    # the selector keeps 4 bits
    li   x7, 0xFFFFFFF9
    csrw 0x320+\k, x7
    csrr x8, 0x320+\k
    addi x8, x8, -KEEP_EV
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
    li   x14, KEEP_LO
    xor  x14, x14, x10
    seqz x14, x14
    and  x28, x28, x14
    li   x14, KEEP_HI
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
    .if \k - 2 <= REAL
    # the inhibit bit stops the counter
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
    .endif
    .endm

# Counts 15 taken branches on counter 3 from low half -8 and the high half \hi, reads it with the
# high, low, high sequence and leaves in x25 whether the high half is \want and the low half below 16.
    .macro COUNT hi, want
    li   x7, 3
    csrw 0x323, x7                # taken branch
    li   x7, -8
    csrw 0xB03, x7
    li   x7, \hi
    csrw 0xB83, x7
    li   x14, 16
3:  addi x14, x14, -1
    bnez x14, 3b
4:  csrr x10, 0xC83
    csrr x11, 0xC03
    csrr x12, 0xC83
    bne  x10, x12, 4b
    addi x10, x10, -\want
    sltiu x25, x11, 16
    seqz x10, x10
    and  x25, x25, x10
    csrw 0x323, x0
    csrw 0xB03, x0
    csrw 0xB83, x0
    .endm

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    la   x6, handler
    csrw mtvec, x6
    li   x20, 0
    li   x28, 1

    # mcountinhibit keeps bits 0, 2 and the bits of the counters the core has
    li   x6, -1
    csrw 0x320, x6
    csrr x26, 0x320
    csrw 0x320, x0
    li   x7, MASK
    xor  x26, x26, x7
    seqz x26, x26

    # the reserved addresses are illegal instructions
    csrr x6, 0x321
    csrr x6, 0x322
    csrr x6, 0xB01
    csrr x6, 0xB81

    .irp k, 3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    CHECK \k
    .endr

    COUNT 0, 1
    mv   x24, x25                 # carry into the high half
    COUNT HIGH_KEEP, 0
    sw   x28, 0(x5)
    sw   x20, 4(x5)
    sw   x26, 8(x5)
    sw   x24, 12(x5)
    sw   x25, 16(x5)
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
