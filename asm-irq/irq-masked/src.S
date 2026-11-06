// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// An interrupt is taken only with mstatus.MIE, the bit of mie and the bit of mip at 1. mip.MTIP
// shows the pending timer the whole time. With MIE at 0, and then with MTIE at 0 and MIE at 1,
// nothing is taken. When MTIE is set the interrupt takes the very next instruction, so mepc is
// the address right after the write to mie and the instruction at that address still runs once.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    RVIRQ_TIMER_AFTER(20)
    li   t0, RV32_MIE_MTIE
    csrs mie, t0                  # MTIE = 1, MIE = 0
wait_pending:
    csrr t0, mip
    andi t0, t0, RV32_MIE_MTIE
    beqz t0, wait_pending         # mip.MTIP rises although MIE = 0
    li   t0, 60
1:  addi t0, t0, -1
    bnez t0, 1b
    lw   t1, 0(s0)
    bnez t1, fail1                # not taken with MIE = 0

    li   t0, RV32_MIE_MTIE
    csrc mie, t0                  # MTIE = 0
    csrsi mstatus, 8              # MIE = 1
    li   t0, 60
2:  addi t0, t0, -1
    bnez t0, 2b
    lw   t1, 0(s0)
    bnez t1, fail2                # not taken with MTIE = 0

    li   t0, RV32_MIE_MTIE
    csrs mie, t0                  # the interrupt becomes enabled here
after_enable:
    addi s1, x0, 7                # aborted by the interrupt, runs after MRET
    lw   t1, 0(s0)
    li   t2, 1
    bne  t1, t2, fail3            # taken exactly once
    lw   t1, 8(s0)
    la   t2, after_enable
    bne  t1, t2, fail4            # mepc is the first instruction that did not run
    li   t2, 7
    bne  s1, t2, fail5            # and that instruction ran after the return
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
    csrr t3, mepc
    li   t6, RV32_RAM_BASE
    sw   t3, 8(t6)
    lw   t3, 0(t6)
    addi t3, t3, 1
    sw   t3, 0(t6)
    RVIRQ_TIMER_OFF(t3, t4)
    mret
