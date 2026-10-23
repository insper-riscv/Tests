// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// Unimplemented CSR, a write to a read-only CSR, sret and the reserved SYSTEM funct3
// raise an illegal instruction (mcause 2, mtval 0) and do not write rd.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    addi x27, x5, 0x200           # trap log: cause, mepc, mtval, mstatus per trap
    la   x6, handler
    csrw mtvec, x6
    li   x6, 4369
    li   x8, 4369
    li   x7, 1
    la   x20, site_csr
site_csr:
    csrr x6, 0x7C0                # unimplemented CSR: rd must stay untouched
    la   x21, site_ro
site_ro:
    csrrs x8, 0xF11, x7           # effective write to the read-only mvendorid
    la   x22, site_sret
site_sret:
    .word 0x10200073              # sret: not supported
    la   x23, site_f3
site_f3:
    .word 0x00004073              # SYSTEM with the reserved funct3 = 100
    csrrs x9, 0xF11, x0           # a read of mvendorid is legal
    csrr  x24, mstatus            # the CSR state survived the traps

    lw   x12, 512(x5)
    sw   x12, 0(x5)
    lw   x12, 516(x5)
    sub  x12, x12, x20
    sw   x12, 4(x5)
    lw   x12, 520(x5)
    sw   x12, 8(x5)
    lw   x12, 528(x5)
    sw   x12, 12(x5)
    lw   x12, 532(x5)
    sub  x12, x12, x21
    sw   x12, 16(x5)
    lw   x12, 536(x5)
    sw   x12, 20(x5)
    lw   x12, 544(x5)
    sw   x12, 24(x5)
    lw   x12, 548(x5)
    sub  x12, x12, x22
    sw   x12, 28(x5)
    lw   x12, 552(x5)
    sw   x12, 32(x5)
    lw   x12, 560(x5)
    sw   x12, 36(x5)
    lw   x12, 564(x5)
    sub  x12, x12, x23
    sw   x12, 40(x5)
    lw   x12, 568(x5)
    sw   x12, 44(x5)
    mv   x12, x6
    sw   x12, 48(x5)
    mv   x12, x8
    sw   x12, 52(x5)
    mv   x12, x9
    sw   x12, 56(x5)

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
