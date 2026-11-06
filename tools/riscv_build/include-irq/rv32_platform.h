/* The memory map and the machine timer (CLINT) the interrupt programs (asm-irq/) are built for,
 * as plain numbers an assembly program can use. The map is the one of internal-mem; the CLINT
 * has the layout of the SiFive one: msip at +0x0000, mtimecmp at +0x4000 and mtime at +0xBFF8,
 * all with 32-bit accesses and the low half at the lower address. A platform that wires the
 * interrupts defines RV32_CLINT_BASE in its generated header; the simulation of tests/python
 * defines it as a region of its own model of the CLINT. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x00008000   /* first byte of RAM */
#define RV32_RAM_END     0x00030000   /* one past the last byte of RAM */
#define RV32_TOHOST      0x0002FFE8   /* HTIF tohost, 8 bytes; fromhost is 8 bytes above */

#ifndef RV32_CLINT_BASE
#define RV32_CLINT_BASE  0x00100000   /* the CLINT of the simulation model */
#endif
#define RV32_MSIP        (RV32_CLINT_BASE + 0x0000)
#define RV32_MTIMECMP    (RV32_CLINT_BASE + 0x4000)
#define RV32_MTIME       (RV32_CLINT_BASE + 0xBFF8)

#define RV32_MIE_MSIE    0x00000008   /* bit 3 of mie and mip */
#define RV32_MIE_MTIE    0x00000080   /* bit 7 of mie and mip */
#define RV32_MIE_MEIE    0x00000800   /* bit 11 of mie and mip */
#define RV32_CAUSE_MSI   0x80000003
#define RV32_CAUSE_MTI   0x80000007
#define RV32_CAUSE_MEI   0x8000000B

#ifdef __ASSEMBLER__
/* How an assembly test ends, with the names of the riscv-arch-test model. tohost = 1 is a pass,
 * (code << 1) | 1 a failure with that code. Clobbers t0 and t1. */
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

/* Arms the timer to fire `delta` ticks from now (the low half of mtime, so for a short wait).
 * The low half goes to all ones first and back to the target last, as the FreeRTOS port does,
 * so that no state in between fires. Clobbers t0, t1 and t2. */
#define RVIRQ_TIMER_AFTER(delta) \
    li   t0, RV32_MTIME; \
    lw   t1, 0(t0); \
    addi t1, t1, (delta); \
    li   t0, RV32_MTIMECMP; \
    li   t2, -1; \
    sw   t2, 0(t0); \
    sw   x0, 4(t0); \
    sw   t1, 0(t0)

/* Moves the timer out of reach (mtimecmp all ones). Clobbers the two registers it is given. */
#define RVIRQ_TIMER_OFF(ra, rb) \
    li   ra, RV32_MTIMECMP; \
    li   rb, -1; \
    sw   rb, 4(ra); \
    sw   rb, 0(ra)

/* Raises and clears the software interrupt. Clobber the two registers they are given. */
#define RVIRQ_SOFTWARE_ON(ra, rb) \
    li   ra, RV32_MSIP; \
    li   rb, 1; \
    sw   rb, 0(ra)
#define RVIRQ_SOFTWARE_OFF(ra) \
    li   ra, RV32_MSIP; \
    sw   x0, 0(ra)

/* The handler of the programs that log: it counts the entries in RV32_RAM_BASE + 0, writes the
 * mcause and the mepc of entry k at RV32_RAM_BASE + 0xF8 + 8k, acknowledges the software and
 * the timer interrupt, and skips the instruction of an exception. It uses t3 to t6 only. */
#define RVIRQ_LOG_HANDLER \
    csrr t3, mcause; \
    csrr t4, mepc; \
    li   t5, RV32_RAM_BASE; \
    lw   t6, 0(t5); \
    addi t6, t6, 1; \
    sw   t6, 0(t5); \
    slli t6, t6, 3; \
    add  t6, t6, t5; \
    sw   t3, 0xF8(t6); \
    sw   t4, 0xFC(t6); \
    bgez t3, 8f; \
    li   t6, RV32_CAUSE_MSI; \
    bne  t3, t6, 1f; \
    RVIRQ_SOFTWARE_OFF(t5); \
    mret; \
1:  RVIRQ_TIMER_OFF(t5, t6); \
    mret; \
8:  addi t4, t4, 4; \
    csrw mepc, t4; \
    mret
#endif

#endif
