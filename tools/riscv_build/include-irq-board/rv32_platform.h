/* O mapa de memória e o temporizador da máquina (CLINT) para os quais os programas de interrupção (asm-irq/)
 * são construídos, como números simples que um programa em assembly usa. O mapa é o da internal-mem; o CLINT
 * tem o layout do SiFive: msip em +0x0000, mtimecmp em +0x4000 e mtime em +0xBFF8, todos com acessos de 32
 * bits e a metade baixa no endereço menor. Uma plataforma que liga as interrupções define RV32_CLINT_BASE no
 * seu cabeçalho gerado; a simulação de tests/python o define como uma região do seu próprio modelo do CLINT.
 *
 * Os números ficam em #define (e não em constexpr ou enum) porque o assembly (.S) inclui este arquivo e só o
 * pré-processador os enxerga (ISO/IEC 9899:2024, 6.10); o C de 6.7.2 não é visível ao montador. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* primeiro byte da RAM: a SDRAM da de0-cv */
#define RV32_RAM_END     0x42000000   /* um após o último byte da RAM */
#define RV32_TOHOST      0x41FFFFE8   /* tohost do HTIF, 8 bytes; o fromhost fica 8 bytes acima */

#ifndef RV32_CLINT_BASE
#define RV32_CLINT_BASE  0xE0000000   /* o CLINT da de0-cv (slot 6) */
#endif
#define RV32_MSIP        (RV32_CLINT_BASE + 0x0000)
#define RV32_MTIMECMP    (RV32_CLINT_BASE + 0x4000)
#define RV32_MTIME       (RV32_CLINT_BASE + 0xBFF8)

/* Acréscimos do irq-mepc-precise e a distância, em ticks do mtime, entre as suas interrupções: a placa conta o
 * mtime a 1 MHz (50 ciclos de clock por tick), então o seu bloco é longo o bastante para várias interrupções. */
#define RV32_PRECISE_ADDS 12000
#define RV32_PRECISE_STEP 37

/* Endereços dos programas de falha de acesso: um byte da ROM somente leitura, um endereço em nenhuma região do
 * mapa e um endereço num slot do barramento de periféricos onde nenhum periférico responde (o núcleo os falha com
 * as causas 5 e 7, e uma busca no slot vazio ou no buraco com a causa 1). */
#define RV32_ROM_ADDR    0x00002000
#define RV32_HOLE_ADDR   0x10000000
#define RV32_EMPTY_SLOT  0x90000000

#define RV32_MIE_MSIE    0x00000008   /* bit 3 de mie e mip */
#define RV32_MIE_MTIE    0x00000080   /* bit 7 de mie e mip */
#define RV32_MIE_MEIE    0x00000800   /* bit 11 de mie e mip */
#define RV32_CAUSE_MSI   0x80000003
#define RV32_CAUSE_MTI   0x80000007
#define RV32_CAUSE_MEI   0x8000000B

#ifdef __ASSEMBLER__
/* Como um teste em assembly termina, com os nomes do modelo do riscv-arch-test. tohost = 1 é aprovação,
 * (código << 1) | 1 é falha com esse código. Altera t0 e t1. */
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

/* O estado que um reset deixa, para uma plataforma que reinicia um programa sem reiniciar o núcleo nem o CLINT:
 * nenhuma interrupção habilitada ou pendente e o temporizador fora de alcance. É a primeira coisa de todo
 * programa de asm-irq/. Altera t0 e t1. */
#define RVIRQ_INIT \
    csrci mstatus, 8; \
    csrw  mie, x0; \
    li    t0, RV32_MSIP; \
    sw    x0, 0(t0); \
    li    t0, RV32_MTIMECMP; \
    li    t1, -1; \
    sw    t1, 4(t0); \
    sw    t1, 0(t0)

/* Arma o temporizador para disparar `delta` ticks adiante (a metade baixa do mtime, então para uma espera
 * curta). A metade baixa vai a todos os bits em um primeiro e ao alvo por último, como faz o porte do
 * FreeRTOS, para que nenhum estado intermediário dispare. Altera t0, t1 e t2. */
#define RVIRQ_TIMER_AFTER(delta) \
    li   t0, RV32_MTIME; \
    lw   t1, 0(t0); \
    addi t1, t1, (delta); \
    li   t0, RV32_MTIMECMP; \
    li   t2, -1; \
    sw   t2, 0(t0); \
    sw   x0, 4(t0); \
    sw   t1, 0(t0)

/* Move o temporizador para fora de alcance (mtimecmp com todos os bits em um). Altera os dois registradores
 * recebidos. */
#define RVIRQ_TIMER_OFF(ra, rb) \
    li   ra, RV32_MTIMECMP; \
    li   rb, -1; \
    sw   rb, 4(ra); \
    sw   rb, 0(ra)

/* Levanta e baixa a interrupção de software. Alteram os registradores recebidos. */
#define RVIRQ_SOFTWARE_ON(ra, rb) \
    li   ra, RV32_MSIP; \
    li   rb, 1; \
    sw   rb, 0(ra)
#define RVIRQ_SOFTWARE_OFF(ra) \
    li   ra, RV32_MSIP; \
    sw   x0, 0(ra)

/* O tratador dos programas que registram: conta as entradas em RV32_RAM_BASE + 0, escreve o mcause e o mepc
 * da entrada k em RV32_RAM_BASE + 0xF8 + 8k, reconhece a interrupção de software e a do temporizador e pula a
 * instrução de uma exceção. Usa só t3 a t6. */
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
