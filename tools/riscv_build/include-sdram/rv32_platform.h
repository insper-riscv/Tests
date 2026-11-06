/* The memory map of the SDRAM platform, as plain numbers an assembly program can use as well as
 * a C one. The SDRAM is the RAM. It must match the platform's platform.yaml, which TopLevel's
 * `check-memory-map` verifies. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* first byte of RAM (the SDRAM) */
#define RV32_RAM_END     0x44000000   /* one past the last byte of RAM */
#define RV32_STDOUT_BASE 0x43FFFBE0   /* the stdout buffer, at the top of RAM */

#define RV32_TOHOST      0x43FFFFE8   /* HTIF tohost, 8 bytes; fromhost is 8 bytes above */

#define RV32_UART_BASE       0xC0000000   /* JTAG UART, slot 4: txdata +0x0, rxdata +0x4, status +0x8 */
#define RV32_FLASH_CTRL_BASE 0xB0000000   /* FLASH controller, slot 3: CMD, ADDR, STATUS, BUF_PTR, BUF_DATA (word offsets 0 to 4) */

/* The two banks of the FLASH and the image each one holds (flash.banks of the platform.yaml): the header
 * page of 256 bytes, with this magic, and then the program. */
#define RV32_IMAGE_MAGIC 0x52564946
#define RV32_BANK_A_BASE 0x00001000
#define RV32_BANK_A_SIZE 0x0001F000
#define RV32_BANK_B_BASE 0x00020000
#define RV32_BANK_B_SIZE 0x00020000

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
