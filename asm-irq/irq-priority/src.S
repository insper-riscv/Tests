// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// With the software and the timer interrupts pending and enabled together, the software one is
// taken first (cause 3) and the timer one right after (cause 7), both over the same instruction:
// the program does not advance between them.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    li   t0, RV32_MIE_MSIE | RV32_MIE_MTIE
    csrs mie, t0
    RVIRQ_SOFTWARE_ON(t0, t1)
    RVIRQ_TIMER_AFTER(5)
wait_both:
    csrr t0, mip
    andi t0, t0, RV32_MIE_MSIE | RV32_MIE_MTIE
    li   t1, RV32_MIE_MSIE | RV32_MIE_MTIE
    bne  t0, t1, wait_both        # both pending, MIE still 0
    lw   t0, 0(s0)
    bnez t0, fail1
    csrsi mstatus, 8
after_enable:
    addi s1, x0, 5
    lw   t0, 0(s0)
    li   t1, 2
    bne  t0, t1, fail2            # two entries
    lw   t0, 0xF8+8(s0)
    li   t1, RV32_CAUSE_MSI
    bne  t0, t1, fail3            # software first
    lw   t0, 0xF8+16(s0)
    li   t1, RV32_CAUSE_MTI
    bne  t0, t1, fail4            # timer second
    lw   t0, 0xFC+8(s0)
    la   t1, after_enable
    bne  t0, t1, fail5            # over the instruction that did not run
    lw   t0, 0xFC+16(s0)
    bne  t0, t1, fail5            # the same one
    li   t0, 5
    bne  s1, t0, fail6            # which runs after the second return
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
fail6:
    RVMODEL_HALT_FAIL(6)

    .balign 4
handler:
    RVIRQ_LOG_HANDLER
