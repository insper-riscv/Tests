// RV32_TEST_KIND: memory
//
// Mirrors RV32IM/tests/FPGA/core/asm_tests/full.S section 6
// ("Loads/Stores"), specifically its final LUI+SW+LW sub-case,
// copied verbatim in spirit: address memory via a loaded base
// register (x9) + a small immediate offset — never x0 as the store
// base with a large immediate, which is what our *compiled* C tests
// end up doing for compile-time-constant addresses (GCC folds
// `(volatile unsigned int *)0x10` into "x0 + 16" instead of
// materializing 0x10 in a register). Written in raw assembly, not C,
// specifically so no compiler is free to pick that addressing mode
// instead — this isolates whether x0-as-store-base is what's broken.
//
// full.S's own version (base register x9 = TEMP_SLOT_BASE):
//   lui  x19, 0x7F000         # 0x7F000000
//   sw   x19, 4(x9)
//   lw   x20, 4(x9)
//   sw   x20, 0(x5)           # (full.S copies into its own results log)
//
// This test does the same lui/sw/lw, then re-stores the loaded-back
// value at the same base for the memory check to read.
#include "rv32_platform.h"


    .section .text
    .globl main
main:
    // Base register, loaded (not x0) — matches full.S's TEMP_SLOT_BASE
    // pattern. 0x8100 = RAM base (0x8000, Harvard modificado) +
    // 0x100 (arbitrary, just clear of every other test's addresses
    // and the mailbox at 0x2FFFC).
    li   x9, RV32_RAM_BASE + 0x100

    // --- full.S section 6, LUI+SW+LW sub-case, same instructions ---
    lui  x19, 0x7F000          // 0x7F000000
    sw   x19, 4(x9)
    lw   x20, 4(x9)

    // Store the loaded-back value where the golden JSON below checks
    // it — if SW+LW round-tripped correctly, this is 0x7F000000.
    sw   x20, 0(x9)

    // Signal PASS via the mailbox (see include/rv32_test.h /
    // config.yaml memory.mailbox_addr — 0x0002FFFC). Written directly
    // in assembly rather than #include-ing rv32_test.h and calling
    // RV32_PASS(): it's a `static inline` C function, so it has no
    // linkable symbol a separate .S file could `call`.
    li  x15, RV32_RAM_END
    addi x14, x0, 1
    sw   x14, -4(x15)          // mailbox = 1

    // Jump back into crt0.S's restart loop instead of spinning here —
    // this is what lets build_fpga.py JTAG-reload the next test onto
    // the same, already-programmed bitstream (see rv32_wait_restart).
    j rv32_wait_restart
