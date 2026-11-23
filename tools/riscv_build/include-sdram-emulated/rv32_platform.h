/* O mapa de memória da plataforma SDRAM, como números simples que um programa em assembly usa tanto quanto
 * um em C. A SDRAM é a RAM. Deve coincidir com o platform.yaml da plataforma, que o `check-memory-map`
 * verifica.
 *
 * Os números ficam em #define (e não em constexpr ou enum) porque o assembly (.S) inclui este arquivo e só o
 * pré-processador os enxerga (ISO/IEC 9899:2024, 6.10); o C de 6.7.2 não é visível ao montador. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* primeiro byte da RAM (a SDRAM) */
#define RV32_RAM_END     0x44000000   /* um após o último byte da RAM */
#define RV32_STDOUT_BASE 0x43FFFBE0   /* o buffer de stdout, no topo da RAM */

#define RV32_TOHOST      0x43FFFFE8   /* tohost do HTIF, 8 bytes; o fromhost fica 8 bytes acima */

#define RV32_UART_BASE       0xC0000000   /* UART JTAG, slot 4: txdata +0x0, rxdata +0x4, status +0x8 */
#define RV32_FLASH_CTRL_BASE 0xB0000000   /* controlador da FLASH, slot 3: CMD, ADDR, STATUS, BUF_PTR, BUF_DATA (deslocamentos de palavra 0 a 4) */

/* As duas bancas da FLASH e a imagem que cada uma guarda (flash.banks do platform.yaml): a página de
 * cabeçalho de 256 bytes, com este número mágico, e depois o programa. */
#define RV32_IMAGE_MAGIC 0x52564946
#define RV32_BANK_A_BASE 0x00001000
#define RV32_BANK_A_SIZE 0x0001F000
#define RV32_BANK_B_BASE 0x00020000
#define RV32_BANK_B_SIZE 0x00020000

#define RV32_CLINT_BASE  0xE0000000   /* CLINT, slot 6 (64 KiB): msip +0x0, mtimecmp +0x4000, mtime +0xBFF8 */
#define RV32_TICK_CYCLES 77           /* ciclos de clock por tick do mtime e do csr time (interrupts.clock_hz / mtime_hz) */

#ifdef __ASSEMBLER__
/* Como um teste em assembly termina, com os nomes do modelo do riscv-arch-test (o rv32_test.h tem os
 * de C). tohost = 1 é aprovação, (código << 1) | 1 é falha com esse código. Altera t0 e t1. */
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
