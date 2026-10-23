/* The memory map of the SDRAM platform, as plain numbers an assembly program can use as well as
 * a C one. The SDRAM is the RAM. It must match the platform's platform.yaml, which TopLevel's
 * `check-memory-map` verifies. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* first byte of RAM (the SDRAM) */
#define RV32_RAM_END     0x44000000   /* one past the last byte of RAM */
#define RV32_STDOUT_BASE 0x43FFFBE0   /* the stdout buffer, at the top of RAM */

#endif
