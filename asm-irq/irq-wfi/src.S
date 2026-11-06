// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// wfi executes as a nop: with an interrupt pending and mstatus.MIE at 0 it does not stop the
// core and nothing is taken. With MIE at 1 the interrupt aborts the wfi, which runs again after
// the return and also as a nop.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    RVIRQ_TIMER_AFTER(10)
    li   t0, RV32_MIE_MTIE
    csrs mie, t0                  # MTIE = 1, MIE = 0
    wfi
    wfi
    wfi
    csrr t0, mip
    andi t0, t0, RV32_MIE_MTIE
    beqz t0, fail1                # still running, and the timer is pending
    lw   t1, 0(s0)
    bnez t1, fail2                # nothing was taken
    csrsi mstatus, 8
site:
    wfi
    addi s1, x0, 9
    lw   t1, 0(s0)
    li   t2, 1
    bne  t1, t2, fail3
    lw   t1, 0xFC+8(s0)
    la   t2, site
    bne  t1, t2, fail4            # the interrupt aborted the wfi
    li   t2, 9
    bne  s1, t2, fail5            # and the program went on after it
    RVMODEL_HALT_PASS
fail1:
    RVMODEL_HALT_FAIL(1)
fail2:
    RVMODEL_HALT_FAIL(2)
fail3:
    RVMODEL_HALT_FAIL(3)
fail4:
    RVMODEL_HALT_FAIL(4)
fail5:
    RVMODEL_HALT_FAIL(5)

    .balign 4
handler:
    RVIRQ_LOG_HANDLER
