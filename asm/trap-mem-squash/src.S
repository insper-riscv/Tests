// RV32_EXT: M,Zicsr,Zicntr
// RV32_TEST_KIND: memory
//
// A load that traps in the MEM stage leaves nothing behind. The load goes to the BOOT_ROM, outside the main memory on every platform, at an
// address that is not word aligned, so the core takes the exception after the load has left EX. The
// instructions right behind it (a write to mscratch, and a store to the RAM) are already in EX, and
// none of them may take effect before the handler runs. The handler also sees that the load itself
// does not count in instret: between the read of instret before the load and the first read in the
// handler there is only that first csrr.
//
// results (words): 0 mcause (4), 1 mepc - site (0), 2 mtval - (x25 + 1) (0), 3 mscratch at entry (0x77),
// 4 marker at entry (0), 5 instret delta (1), 6 mscratch at the end (0), 7 marker at the end (0xAA)
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    sw   x0, 0x300(x5)            # the marker starts at zero: on the board the RAM keeps what an earlier program left
    la   x6, handler
    csrw mtvec, x6
    li   x25, 0x400               # inside the BOOT_ROM: the misaligned load traps
    li   x10, 0x77
    csrw mscratch, x10
    la   x20, site
    csrr x6, 0xC02                # instret before the load
site:
    lw   x7, 1(x25)
    csrw mscratch, x0             # behind the load: must not run before the handler
    li   x12, 0xAA
    sw   x12, 0x300(x5)           # behind the load: must not run before the handler

    csrr x29, mscratch
    sw   x29, 24(x5)
    lw   x29, 0x300(x5)
    sw   x29, 28(x5)

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS

    .balign 4
handler:
    csrr x28, 0xC02
    sub  x28, x28, x6
    sw   x28, 20(x5)
    csrr x29, mcause
    sw   x29, 0(x5)
    csrr x29, mepc
    sub  x29, x29, x20
    sw   x29, 4(x5)
    csrr x29, mtval
    sub  x29, x29, x25
    addi x29, x29, -1
    sw   x29, 8(x5)
    csrr x29, mscratch
    sw   x29, 12(x5)
    lw   x29, 0x300(x5)
    sw   x29, 16(x5)
    csrr x29, mepc
    addi x29, x29, 4
    csrw mepc, x29
    mret
