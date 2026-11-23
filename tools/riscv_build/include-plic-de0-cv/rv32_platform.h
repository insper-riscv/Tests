/* O mapa de memória e as fontes de interrupção da plataforma de0-cv (Insper-RISC-V, src/cyclone-v/de0-cv) para os
 * quais os programas do PLIC (asm-plic-de0-cv/) são construídos. Deve coincidir com o platform.yaml da plataforma,
 * cujo cabeçalho de runtime gerado (memory_map.h) tem os mesmos nomes com o prefixo RV32_.
 *
 * Os números ficam em #define (e não em constexpr ou enum) porque o assembly (.S) inclui este arquivo e só o
 * pré-processador os enxerga (ISO/IEC 9899:2024, 6.10); o C de 6.7.2 não é visível ao montador. */
#ifndef RV32_PLATFORM_H
#define RV32_PLATFORM_H

#define RV32_RAM_BASE    0x40000000   /* primeiro byte da RAM (a SDRAM) */
#define RV32_RAM_END     0x42000000   /* um após o último byte da RAM */
#define RV32_STDOUT_BASE 0x41FFFBE0   /* o buffer de stdout, no topo da RAM */
#define RV32_TOHOST      0x41FFFFE8   /* tohost do HTIF, 8 bytes */
#define RV32_FROMHOST    0x41FFFFF0   /* fromhost do HTIF */

#define RV32_FLASH_CTRL_BASE 0xB0000000
#define RV32_UART_BASE       0xC0000000   /* a UART JTAG, slot 4 */
#define RV32_CLINT_BASE      0xE0000000
#define RV32_PLIC_BASE       0xF0000000

#define RV32_PLIC_PRIORITY   (RV32_PLIC_BASE + 0x000000)
#define RV32_PLIC_PENDING    (RV32_PLIC_BASE + 0x001000)
#define RV32_PLIC_ENABLE     (RV32_PLIC_BASE + 0x002000)
#define RV32_PLIC_THRESHOLD  (RV32_PLIC_BASE + 0x200000)
#define RV32_PLIC_CLAIM      (RV32_PLIC_BASE + 0x200004)

/* Os ids das fontes, os mesmos de interrupts.sources do platform.yaml. */
#define RV32_IRQ_UART_JTAG   2
#define RV32_IRQ_FLASH       6

#define RV32_MIE_MEIE    0x00000800
#define RV32_CAUSE_MEI   0x8000000B

/* Os registradores do controlador da FLASH, como deslocamentos em bytes, e os comandos do chip. */
#define FL_CMD         0x00
#define FL_ADDR        0x04
#define FL_STATUS      0x08   /* bit 0 ocupado, bit 1 trava de escrita, bit 2 recusado */
#define FL_IRQ_ENABLE  0x14   /* bit 0 concluído, bit 1 recusado */
#define FL_IRQ_STATUS  0x18   /* escrever 1 limpa */
#define FL_WREN        1
#define FL_ERASE       2
#define FL_PROGRAM     3
#define FL_PROTECT     4
#define FL_UNPROTECT   5

/* Os registradores da UART JTAG. */
#define UJ_TXDATA        0x00
#define UJ_RXDATA        0x04
#define UJ_STATUS        0x08   /* uma leitura limpa o bit de descarte (16) */
#define UJ_CONTROL       0x0C   /* bit 0 RX, bit 1 TX vazio, bit 2 erro */

/* Na placa, o console que esvazia a UART JTAG se conecta segundos depois de o programa começar (cerca de 16 s de
 * sondagem aqui); o host da simulação responde de uma vez, então o limite só importa para um programa que falha. */
#define RVPLIC_WAIT_POLLS 100000000

#include "../include-plic/rv32_plic_macros.h"

#endif
