/* The memory map of the platform the programs are built for, as plain numbers an assembly
 * program can use as well as a C one. This is internal-mem's (the RAM inside the FPGA); the
 * SDRAM platform has its own copy in include-sdram/. They must match the platform's
 * platform.yaml, which TopLevel's `check-memory-map` verifies. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x00008000   /* first byte of RAM */
#define RV32_RAM_END     0x00030000   /* one past the last byte of RAM */
#define RV32_STDOUT_BASE 0x0002FBE0   /* the stdout buffer, at the top of RAM */

#endif
