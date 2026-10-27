// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// ecall and ebreak jump to mtvec with the right mcause, mepc (the instruction itself)
// and mtval; the trap closes MIE (MPIE keeps the old one) and MRET brings MIE back.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    addi x27, x5, 0x200           # trap log: cause, mepc, mtval, mstatus per trap
    la   x6, handler
    csrw mtvec, x6
    csrsi mstatus, 8              # MIE = 1
    la   x20, site_ecall
site_ecall:
    ecall
    csrr x10, mstatus             # after MRET: MIE is back to 1
    la   x21, site_ebreak
site_ebreak:
    ebreak
    csrr x11, mstatus
    li   x22, 0x1234              # runs after the second return

    lw   x12, 512(x5)
    sw   x12, 0(x5)
    lw   x12, 516(x5)
    sub  x12, x12, x20
    sw   x12, 4(x5)
    lw   x12, 520(x5)
    sw   x12, 8(x5)
    lw   x12, 524(x5)
    sw   x12, 12(x5)
    mv   x12, x10
    sw   x12, 16(x5)
    lw   x12, 528(x5)
    sw   x12, 20(x5)
    lw   x12, 532(x5)
    sub  x12, x12, x21
    sw   x12, 24(x5)
    lw   x12, 536(x5)
    sw   x12, 28(x5)
    lw   x12, 540(x5)
    sw   x12, 32(x5)
    mv   x12, x11
    sw   x12, 36(x5)
    mv   x12, x22
    sw   x12, 40(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart

    .balign 4
handler:
    csrr x28, mcause
    csrr x29, mepc
    csrr x30, mtval
    csrr x31, mstatus
    sw   x28, 0(x27)
    sw   x29, 4(x27)
    sw   x30, 8(x27)
    sw   x31, 12(x27)
    addi x27, x27, 16
    addi x29, x29, 4           # resume after the instruction that trapped
    csrw mepc, x29
    mret
