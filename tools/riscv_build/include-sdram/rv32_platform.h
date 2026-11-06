/* The memory map of the SDRAM platform, as plain numbers an assembly program can use as well as
 * a C one. The SDRAM is the RAM. It must match the platform's platform.yaml, which TopLevel's
 * `check-memory-map` verifies. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* first byte of RAM (the SDRAM) */
#define RV32_RAM_END     0x44000000   /* one past the last byte of RAM */
#define RV32_STDOUT_BASE 0x43FFFBE0   /* the stdout buffer, at the top of RAM */

#define RV32_TOHOST      0x43FFFFE8   /* HTIF tohost, 8 bytes; fromhost is 8 bytes above */

#define RV32_CLINT_BASE  0xE0000000   /* CLINT, slot 6 (64 KiB): msip +0x0, mtimecmp +0x4000, mtime +0xBFF8 */

#ifdef __ASSEMBLER__
/* How an assembly test ends, with the names of the riscv-arch-test model (rv32_test.h has
 * the C ones). tohost = 1 is a pass, (code << 1) | 1 a failure with that code. Clobbers
 * t0 and t1. */
#define RVMODEL_HALT_PASS \
    li   t0, RV32_TOHOST; \
    li   t1, 1; \
    sw   t1, 0(t0); \
    sw   x0, 4(t0); \
    tail rv32_wait_restart
#define RVMODEL_HALT_FAIL(code) \
    li   t0, RV32_TOHOST; \
    li   t1, ((code) << 1) | 1; \
    sw   t1, 0(t0); \
    sw   x0, 4(t0); \
    tail rv32_wait_restart
#endif

#endif
