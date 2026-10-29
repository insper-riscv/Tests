// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Misaligned accesses outside the main memory trap (mcause 4 and 6, mtval = the effective
// address): loads from the flash window (lw at offset 1, lh and lhu at 3) and stores to the
// peripheral window (sw at 2, sh at 3). The load does not write rd and the store does not reach
// the bus. The same accesses to the RAM run in two steps instead (see misaligned-crossing).
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    addi x27, x5, 0x200           # trap log: cause, mepc, mtval, mstatus per trap
    la   x6, handler
    csrw mtvec, x6
    li   x13, 0x11223344
    addi x14, x5, 0x300           # x14 = aligned data word
    sw   x13, 0(x14)
    li   x6, 4369
    li   x7, 4369
    li   x8, 4369
    li   x15, 0xDEADBEEF
    li   x25, 0x1000              # inside the flash window: the loads trap
    li   x26, 0x80000000          # peripheral window: the stores trap
    la   x20, site_lw
site_lw:
    lw   x6, 1(x25)
    la   x21, site_lh
site_lh:
    lh   x7, 3(x25)
    la   x22, site_lhu
site_lhu:
    lhu  x8, 3(x25)
    la   x23, site_sw
site_sw:
    sw   x15, 2(x26)
    la   x24, site_sh
site_sh:
    sh   x15, 3(x26)
    lw   x9, 0(x14)               # the RAM word is untouched
    lh   x16, 2(x14)              # aligned accesses still work
    lb   x17, 1(x14)
    sw   x0, 4(x14)
    sh   x15, 4(x14)
    lw   x19, 4(x14)

    lw   x12, 512(x5)
    sw   x12, 0(x5)
    lw   x12, 516(x5)
    sub  x12, x12, x20
    sw   x12, 4(x5)
    lw   x12, 520(x5)
    sub  x12, x12, x25
    sw   x12, 8(x5)
    lw   x12, 528(x5)
    sw   x12, 12(x5)
    lw   x12, 532(x5)
    sub  x12, x12, x21
    sw   x12, 16(x5)
    lw   x12, 536(x5)
    sub  x12, x12, x25
    sw   x12, 20(x5)
    lw   x12, 544(x5)
    sw   x12, 24(x5)
    lw   x12, 548(x5)
    sub  x12, x12, x22
    sw   x12, 28(x5)
    lw   x12, 552(x5)
    sub  x12, x12, x25
    sw   x12, 32(x5)
    lw   x12, 560(x5)
    sw   x12, 36(x5)
    lw   x12, 564(x5)
    sub  x12, x12, x23
    sw   x12, 40(x5)
    lw   x12, 568(x5)
    sub  x12, x12, x26
    sw   x12, 44(x5)
    lw   x12, 576(x5)
    sw   x12, 48(x5)
    lw   x12, 580(x5)
    sub  x12, x12, x24
    sw   x12, 52(x5)
    lw   x12, 584(x5)
    sub  x12, x12, x26
    sw   x12, 56(x5)
    mv   x12, x6
    sw   x12, 60(x5)
    mv   x12, x7
    sw   x12, 64(x5)
    mv   x12, x8
    sw   x12, 68(x5)
    mv   x12, x9
    sw   x12, 72(x5)
    mv   x12, x16
    sw   x12, 76(x5)
    mv   x12, x17
    sw   x12, 80(x5)
    mv   x12, x19
    sw   x12, 84(x5)

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
