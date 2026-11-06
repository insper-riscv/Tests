// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// mepc is the first instruction that did not complete. A block of 400 additions to s1 runs
// while the timer interrupts it every few tens of cycles. Whatever the instant, at each entry s1
// equals the number of additions before mepc, and at the end s1 is 400: no addition is lost or
// done twice.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    li   s1, 0
    RVIRQ_TIMER_AFTER(60)
    li   t0, RV32_MIE_MTIE
    csrs mie, t0
    csrsi mstatus, 8
block_start:
    .rept 400
    addi s1, s1, 1
    .endr
block_end:
    csrci mstatus, 8              # no more interrupts
    li   t0, 400
    bne  s1, t0, fail1            # every addition ran exactly once
    lw   s2, 0(s0)                # entries
    li   t0, 4
    bltu s2, t0, fail2            # the block was interrupted several times
    addi s3, s0, 0x100            # log: mepc and s1 of each entry (the handler writes mepc at +4)
    la   s4, block_start
    la   s5, block_end
check:
    beqz s2, done
    lw   t0, 0(s3)                # mepc
    lw   t1, 4(s3)                # s1 at the entry
    bltu t0, s4, fail3
    bgtu t0, s5, fail3
    sub  t2, t0, s4
    srli t2, t2, 2
    bne  t2, t1, fail4            # s1 = additions before mepc
    addi s3, s3, 8
    addi s2, s2, -1
    j    check
done:
    RVMODEL_HALT_PASS
fail1:
    RVMODEL_HALT_FAIL(1)
fail2:
    RVMODEL_HALT_FAIL(2)
fail3:
    RVMODEL_HALT_FAIL(3)
fail4:
    RVMODEL_HALT_FAIL(4)

    .balign 4
handler:
    csrr t3, mepc
    li   t5, RV32_RAM_BASE
    lw   t6, 0(t5)
    slli t4, t6, 3
    add  t4, t4, t5
    sw   t3, 0x100(t4)
    sw   s1, 0x104(t4)            # s1 as the interrupted program sees it
    addi t6, t6, 1
    sw   t6, 0(t5)
    li   t4, RV32_MTIME
    lw   t3, 0(t4)
    addi t3, t3, 37
    li   t4, RV32_MTIMECMP
    li   t6, -1
    sw   t6, 0(t4)
    sw   x0, 4(t4)
    sw   t3, 0(t4)                # next interrupt in 37 ticks
    mret
