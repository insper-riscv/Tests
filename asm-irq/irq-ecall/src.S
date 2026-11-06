// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// An interrupt pending when the ecall reaches EX wins over it: the first entry is the interrupt,
// with mepc at the ecall, and after the return the ecall runs and traps with cause 11 and the
// same mepc. A synchronous exception never hides a pending interrupt for good.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    la   t0, handler
    csrw mtvec, t0
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    li   t0, RV32_MIE_MSIE
    csrs mie, t0
    RVIRQ_SOFTWARE_ON(t0, t1)
wait_pending:
    csrr t0, mip
    andi t0, t0, RV32_MIE_MSIE
    beqz t0, wait_pending
    csrsi mstatus, 8
site:
    ecall
    addi s1, x0, 3
    lw   t0, 0(s0)
    li   t1, 2
    bne  t0, t1, fail1            # the interrupt and then the exception
    lw   t0, 0xF8+8(s0)
    li   t1, RV32_CAUSE_MSI
    bne  t0, t1, fail2            # first the interrupt
    lw   t0, 0xF8+16(s0)
    li   t1, 11
    bne  t0, t1, fail3            # then the ecall
    la   t1, site
    lw   t0, 0xFC+8(s0)
    bne  t0, t1, fail4            # the interrupt over the ecall
    lw   t0, 0xFC+16(s0)
    bne  t0, t1, fail4            # and the ecall at the same address
    li   t0, 3
    bne  s1, t0, fail5
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
