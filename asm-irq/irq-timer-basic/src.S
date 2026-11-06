// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// The machine timer interrupt in direct mode: with mstatus.MIE, mie.MTIE and mip.MTIP at 1 the
// core jumps to mtvec once, with mcause 0x80000007, mepc inside the loop that was running, MIE
// closed and MPIE open inside the handler, and MRET brings MIE back and returns to the loop.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)                # entries counted by the handler
    RVIRQ_TIMER_AFTER(100)
    li   t0, RV32_MIE_MTIE
    csrs mie, t0
    csrsi mstatus, 8              # MIE = 1
spin:
    lw   t0, 0(s0)
    beqz t0, spin                 # waits for the interrupt
spin_end:
    lw   t0, 0(s0)
    li   t1, 1
    bne  t0, t1, fail1            # exactly one entry
    lw   t0, 4(s0)
    li   t1, RV32_CAUSE_MTI
    bne  t0, t1, fail2            # mcause
    lw   t0, 8(s0)
    la   t1, spin
    bltu t0, t1, fail3            # mepc in [spin, spin_end)
    la   t1, spin_end
    bgeu t0, t1, fail3
    lw   t0, 12(s0)
    andi t0, t0, 0x88
    li   t1, 0x80
    bne  t0, t1, fail4            # inside the handler MIE = 0 and MPIE = 1
    csrr t0, mstatus
    andi t0, t0, 8
    beqz t0, fail5                # MRET opened MIE again
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
    csrr t3, mcause
    csrr t4, mepc
    csrr t5, mstatus
    li   t6, RV32_RAM_BASE
    sw   t3, 4(t6)
    sw   t4, 8(t6)
    sw   t5, 12(t6)
    lw   t3, 0(t6)
    addi t3, t3, 1
    sw   t3, 0(t6)
    RVIRQ_TIMER_OFF(t3, t4)
    mret
