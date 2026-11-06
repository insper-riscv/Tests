// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: unit
//
// mtvec in vectored mode: the software interrupt (cause 3) goes to BASE + 12, the timer interrupt
// (cause 7) to BASE + 28, and the ecall, whose cause is also 11 but is an exception, to BASE. The
// mode bit is kept, bit 1 is dropped, and in vectored mode the base reads with bits 5:2 at zero.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   s0, RV32_RAM_BASE
    sw   x0, 0(s0)
    la   s1, vectors              # 64-byte aligned
    ori  t0, s1, 3                # mode 3 asked: bit 1 is dropped
    csrw mtvec, t0
    csrr t0, mtvec
    ori  t1, s1, 1
    bne  t0, t1, fail1            # the vectored mode is kept
    addi t0, s1, 0x10             # a base with bits 5:2 set, in vectored mode
    ori  t0, t0, 1
    csrw mtvec, t0
    csrr t0, mtvec
    ori  t1, s1, 1
    bne  t0, t1, fail2            # the base reads with bits 5:2 at zero

    li   t0, RV32_MIE_MSIE | RV32_MIE_MTIE
    csrs mie, t0
    csrsi mstatus, 8
    RVIRQ_SOFTWARE_ON(t0, t1)
1:  lw   t0, 0(s0)
    beqz t0, 1b                   # the software interrupt lands in its entry
    RVIRQ_TIMER_AFTER(30)
2:  lw   t0, 0(s0)
    li   t1, 2
    bltu t0, t1, 2b               # the timer interrupt lands in its entry
    ecall                         # the exception lands in the base
    lw   t0, 0(s0)
    li   t1, 3
    bne  t0, t1, fail3
    lw   t0, 0x100(s0)            # entry 1: the id of the entry reached
    li   t1, 3
    bne  t0, t1, fail4
    lw   t0, 0x108(s0)
    li   t1, 7
    bne  t0, t1, fail5
    lw   t0, 0x110(s0)
    bnez t0, fail6                # the ecall went to slot 0
    lw   t0, 0x104(s0)
    li   t1, RV32_CAUSE_MSI
    bne  t0, t1, fail7
    lw   t0, 0x10C(s0)
    li   t1, RV32_CAUSE_MTI
    bne  t0, t1, fail7
    lw   t0, 0x114(s0)
    li   t1, 11
    bne  t0, t1, fail7
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
fail7:
    RVMODEL_HALT_FAIL(7)

    .balign 64
vectors:
    j    entry_exception          # 0: every exception
    j    entry_bad                # 1
    j    entry_bad                # 2
    j    entry_software           # 3: BASE + 12
    j    entry_bad                # 4
    j    entry_bad                # 5
    j    entry_bad                # 6
    j    entry_timer              # 7: BASE + 28
    j    entry_bad                # 8
    j    entry_bad                # 9
    j    entry_bad                # 10
    j    entry_bad                # 11: BASE + 44, the external one
    j    entry_bad                # 12
    j    entry_bad                # 13
    j    entry_bad                # 14
    j    entry_bad                # 15

/* Each entry logs its id and the mcause at RV32_RAM_BASE + 0x100 + 8k. It uses t3 to t6 only. */
    .macro log_entry id
    csrr t3, mcause
    li   t5, RV32_RAM_BASE
    lw   t6, 0(t5)
    slli t4, t6, 3
    add  t4, t4, t5
    li   t5, \id
    sw   t5, 0x100(t4)
    sw   t3, 0x104(t4)
    li   t5, RV32_RAM_BASE
    addi t6, t6, 1
    sw   t6, 0(t5)
    .endm

    .balign 4
entry_software:
    log_entry 3
    RVIRQ_SOFTWARE_OFF(t5)
    mret
entry_timer:
    log_entry 7
    RVIRQ_TIMER_OFF(t5, t6)
    mret
entry_exception:
    log_entry 0
    csrr t4, mepc
    addi t4, t4, 4
    csrw mepc, t4
    mret
entry_bad:
    log_entry 0xFF
    RVIRQ_SOFTWARE_OFF(t5)
    RVIRQ_TIMER_OFF(t5, t6)
    mret
