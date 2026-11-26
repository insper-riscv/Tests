/* O mapa de memória da plataforma para a qual os programas são construídos, como números simples que um
 * programa em assembly usa tanto quanto um em C. É o da internal-mem (a RAM dentro da FPGA); a plataforma
 * SDRAM tem a sua própria cópia em include-sdram/. Devem coincidir com o platform.yaml da plataforma, que o
 * `check-memory-map` verifica.
 *
 * Os números ficam em #define (e não em constexpr ou enum) porque o assembly (.S) inclui este arquivo e só o
 * pré-processador os enxerga (ISO/IEC 9899:2024, 6.10); o C de 6.7.2 não é visível ao montador. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x00008000   /* primeiro byte da RAM */
#define RV32_RAM_END     0x00030000   /* um após o último byte da RAM */
#define RV32_STDOUT_BASE 0x0002FBE0   /* o buffer de stdout, no topo da RAM */

#define RV32_TOHOST      0x0002FFE8   /* tohost do HTIF, 8 bytes; o fromhost fica 8 bytes acima */
#define RV32_MOTOR_BASE  0x90000000   /* GPIO de quatro bits ligado ao driver do motor */

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
