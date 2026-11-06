// RV32_EXT: M,Zicsr
// RV32_TEST_KIND: memory
//
// mcountinhibit and the selectors of the event counters. This platform has all 29 HPM counters,
// so writing all ones to mcountinhibit leaves bits 0 and 2 to 31 set (0xFFFFFFFD). With the bit of
// counter 3 set, the counter keeps its value while branches are taken, and clearing the bit makes
// it count again. A counter written with a value
// reads it back through the machine register and its read-only mirror, and the selector reads back
// the 4-bit code.
#include "rv32_platform.h"

    .section .text
    .globl main

main:
    li   x5, RV32_RAM_BASE        # x5 = RESULTS_BASE
    li   x6, 3
    csrw 0x323, x6                # mhpmevent3 = 3 (taken branch)
    csrr x20, 0x323               # 3
    li   x6, 0xFFFFFFF7
    csrw 0x324, x6                # only the low 4 bits are kept: 7
    csrr x21, 0x324

    li   x6, 0xFFFFFFFF
    csrw 0x320, x6
    csrr x22, 0x320               # 0xFFFFFFFD
    csrw 0x320, x0

    li   x6, 8
    csrw 0x320, x6                # inhibit counter 3 (bit 3)
    csrr x10, 0xB03
    li   x7, 8
1:  addi x7, x7, -1
    bnez x7, 1b
    csrr x11, 0xB03
    sub  x23, x11, x10            # 0: inhibited

    csrw 0x320, x0
    csrr x10, 0xB03
    li   x7, 8
2:  addi x7, x7, -1
    bnez x7, 2b
    csrr x11, 0xB03
    sub  x24, x11, x10            # 7: counting again

    li   x6, 0x12345678
    csrw 0xB05, x6                # mhpmcounter5 = 0x12345678 (event code 0: it stays)
    csrr x25, 0xC05               # the mirror
    csrr x26, 0xB05
    sw   x20, 0(x5)
    sw   x21, 4(x5)
    sw   x22, 8(x5)
    sw   x23, 12(x5)
    sw   x24, 16(x5)
    sw   x25, 20(x5)
    sw   x26, 24(x5)

    # End of the test: tohost = 1 (HTIF pass), then wait for the host.
    RVMODEL_HALT_PASS
