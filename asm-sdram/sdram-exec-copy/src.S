// RV32_EXT: M,Zifencei
// RV32_TEST_KIND: memory
//
// Code that runs from the SDRAM. The program copies a small routine from the FLASH into the SDRAM
// as data, runs FENCE.I, calls the routine, rewrites one of its instructions, runs FENCE.I again
// and calls it a second time. The routine is fetched through the instruction cache: the second
// call sees the new instruction only because FENCE.I dropped the old line.
#include "rv32_platform.h"

    .section .text
    .globl main

#define CODE_AT 0x40100000          /* where the routine lives in the SDRAM, away from data and stack */

main:
    li   x5, RV32_RAM_BASE          # x5 = RESULTS_BASE

    # copy the routine from the FLASH to the SDRAM
    la   x6, payload_begin
    la   x7, payload_end
    li   x8, CODE_AT
copy:
    lw   x9, 0(x6)
    sw   x9, 0(x8)
    addi x6, x6, 4
    addi x8, x8, 4
    bne  x6, x7, copy

    fence.i
    li   x8, CODE_AT
    jalr x1, 0(x8)                  # first call
    sw   x10, 0(x5)                 # x10 = 10 + 9 + ... + 1 = 55
    sw   x12, 4(x5)                 # x12 = 77
    sw   x13, 8(x5)                 # x13 = 1: the instruction as it was copied

    # rewrite `addi x13, x0, 1` (the first instruction of the routine tail) and call again
    li   x14, 0x00200693            # addi x13, x0, 2
    la   x15, patch_offset
    lw   x15, 0(x15)
    add  x15, x15, x8
    sw   x14, 0(x15)
    fence.i
    jalr x1, 0(x8)                  # second call
    sw   x13, 12(x5)                # x13 = 2: the instruction as it was rewritten
    sw   x10, 16(x5)                # the loop still gives 55

    # Signal PASS via the mailbox (see include/rv32_test.h /
    # config.yaml memory.mailbox_addr).
    li   x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)               // mailbox = 1

    j rv32_wait_restart

    .balign 4
patch_offset:
    .word patch_here - payload_begin

    # The routine, position independent; it is read as data and written to the SDRAM.
    .balign 16
payload_begin:
    li   x10, 0
    li   x11, 10
1:  add  x10, x10, x11
    addi x11, x11, -1
    bnez x11, 1b
    addi x12, x0, 77
patch_here:
    addi x13, x0, 1                 # becomes `addi x13, x0, 2`
    ret
payload_end:
