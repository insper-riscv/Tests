#ifndef RV32_SDRAM_H
#define RV32_SDRAM_H

#include <stdint.h>

#include "rv32_platform.h"

/* A SDRAM é a RAM da plataforma SDRAM: .data e .bss a partir da base, a pilha a partir do topo e, bem no topo,
 * as palavras que o host compartilha com o programa. Os programas de c-sdram/ testam uma janela dela que
 * nenhum desses usa: 16 MB a partir de 16 MB acima da base, longe dos dados do próprio programa e da pilha. */

/* Endereço da primeira palavra da janela de teste da SDRAM. */
static constexpr uint32_t RV32_SDRAM_TEST_BASE = RV32_RAM_BASE + 0x01000000u;

/* Número de palavras da janela de teste da SDRAM. */
static constexpr uint32_t RV32_SDRAM_TEST_WORDS = 1u << 22;

/* A janela de teste como vetor de palavras. Fica como macro porque um ponteiro para endereço fixo não pode ser
 * constexpr (ISO/IEC 9899:2024, 6.7.2: só o ponteiro nulo); o acesso é volatile (6.7.4.1) porque a SDRAM é
 * lida e escrita por um dispositivo, e o endereço é alinhado a palavra. */
#define RV32_SDRAM ((volatile uint32_t *)RV32_SDRAM_TEST_BASE)

/* O dado da palavra i: nunca se repete para i diferentes dentro do chip, então uma palavra que cai em outro
 * endereço aparece como um valor errado. */
static inline uint32_t rv32_sdram_data(uint32_t i) {
    uint32_t x = i ^ 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

#endif
