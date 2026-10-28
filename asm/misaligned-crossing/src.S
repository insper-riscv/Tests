// RV32_EXT: M
// RV32_TEST_KIND: memory
//
// Misaligned loads and stores that cross a word run in the RAM (Zicclsm): lw at offsets 1 to 3
// and lh and lhu at offset 3 read the bytes of both words; sw at offsets 1 to 3 and sh at offset
// 3 write them. The words around each store are read back to show that no other byte changed.
#include "rv32_platform.h"

    .section .text
    .globl main

    .macro reset_buffer
    li   x6, 0x44F6A577
    sw   x6, 0(x14)
    li   x6, 0x00112233
    sw   x6, 4(x14)
    li   x6, 0x8899AABB
    sw   x6, 8(x14)
    .endm

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    addi x14, x5, 0x300           # x14 = the three-word buffer
    li   x15, 0xA1B2C3D4          # value of the stores
    reset_buffer
    lw   x10, 1(x14)
    sw   x10, 0(x5)
    lw   x10, 2(x14)
    sw   x10, 4(x5)
    lw   x10, 3(x14)
    sw   x10, 8(x5)
    lw   x10, 5(x14)
    sw   x10, 12(x5)
    lh   x10, 3(x14)
    sw   x10, 16(x5)
    lh   x10, 7(x14)
    sw   x10, 20(x5)
    lhu  x10, 3(x14)
    sw   x10, 24(x5)
    lhu  x10, 7(x14)
    sw   x10, 28(x5)
    reset_buffer
    sw   x15, 1(x14)
    lw   x10, 0(x14)
    sw   x10, 32(x5)
    lw   x10, 4(x14)
    sw   x10, 36(x5)
    lw   x10, 8(x14)
    sw   x10, 40(x5)
    reset_buffer
    sw   x15, 2(x14)
    lw   x10, 0(x14)
    sw   x10, 44(x5)
    lw   x10, 4(x14)
    sw   x10, 48(x5)
    lw   x10, 8(x14)
    sw   x10, 52(x5)
    reset_buffer
    sw   x15, 3(x14)
    lw   x10, 0(x14)
    sw   x10, 56(x5)
    lw   x10, 4(x14)
    sw   x10, 60(x5)
    lw   x10, 8(x14)
    sw   x10, 64(x5)
    reset_buffer
    sh   x15, 3(x14)
    lw   x10, 0(x14)
    sw   x10, 68(x5)
    lw   x10, 4(x14)
    sw   x10, 72(x5)
    lw   x10, 8(x14)
    sw   x10, 76(x5)

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    j rv32_wait_restart
