// RV32_TEST_KIND: unit
#include <stdint.h>

#include "rv32_platform.h"
#include "rv32_test.h"

// A espera entre testes da BOOT: o programa entra duas vezes, e a segunda entrada só existe se o
// ponto de espera funcionou.
//
//   Entrada 1: sujo o que a BOOT promete limpar, deixo uma marca na RAM que ela não limpa, toco a
//              campainha (fromhost diferente de zero) e salto para o ponto de espera.
//   Ponto de espera: a BOOT, em 0x100, lê o fromhost, vê a campainha e recomeça em _reset.
//   Entrada 2: a marca da RAM sobreviveu (nada apagou a memória de dados), as duas metades do tohost
//              e do fromhost e o cabeçalho da saída padrão voltaram a zero, e o teste termina.
//
// A marca vive nos bytes de dados do buffer da saída padrão, logo depois do cabeçalho, que o programa
// não usa e a BOOT não limpa. Um .data ou .bss não serve, porque o crt0 os reinicia a cada entrada.

// O ponto de espera da BOOT de teste mora num endereço fixo, que o arquivo de specs exporta.
constexpr uintptr_t WAIT_RESTART_ADDRESS = 0x100u;
// A marca, e o seu complemento ao lado, para um valor solto da RAM não passar por ela.
constexpr uint32_t MARK = 0x5EC0DE01u;
// O que a entrada 1 deixa nas palavras que a BOOT limpa.
constexpr uint32_t DIRT = 0xD1127000u;

// A marca e o seu complemento, nas duas primeiras palavras de dados da saída padrão.
#define MARK_WORD ((volatile uint32_t *)(RV32_STDOUT_BASE + 8u))

// Lê uma palavra da memória.
static uint32_t read_word(uintptr_t address) {
    return *(volatile uint32_t *)address;
}

// Escreve uma palavra na memória.
static void write_word(uintptr_t address, uint32_t value) {
    *(volatile uint32_t *)address = value;
}

int main(void) {
    // O símbolo que o _exit usa para terminar um teste tem de ser o do endereço fixo da BOOT.
    if ((uintptr_t)rv32_wait_restart != WAIT_RESTART_ADDRESS) RV32_FAIL(1);

    if (MARK_WORD[0] == MARK && MARK_WORD[1] == ~MARK) {
        // Entrada 2, depois do reinício. A marca é apagada antes de terminar, para o próximo teste não
        // a encontrar.
        MARK_WORD[0] = 0u;
        MARK_WORD[1] = 0u;
        if (read_word(RV32_TOHOST) != 0u) RV32_FAIL(2);
        if (read_word(RV32_TOHOST + 4u) != 0u) RV32_FAIL(3);
        if (read_word(RV32_TOHOST + 8u) != 0u) RV32_FAIL(4);
        if (read_word(RV32_TOHOST + 12u) != 0u) RV32_FAIL(5);
        if (read_word(RV32_STDOUT_BASE) != 0u) RV32_FAIL(6);
        if (read_word(RV32_STDOUT_BASE + 4u) != 0u) RV32_FAIL(7);
        RV32_PASS();
    }

    // Entrada 1. A metade baixa do tohost fica de fora, porque diferente de zero ela já encerraria o
    // teste: só a metade alta é suja. O fromhost leva a campainha na metade baixa.
    MARK_WORD[0] = MARK;
    MARK_WORD[1] = ~MARK;
    write_word(RV32_STDOUT_BASE, DIRT | 1u);
    write_word(RV32_STDOUT_BASE + 4u, DIRT | 2u);
    write_word(RV32_TOHOST + 4u, DIRT | 3u);
    write_word(RV32_TOHOST + 12u, DIRT | 4u);
    write_word(RV32_TOHOST + 8u, DIRT | 5u);
    rv32_wait_restart();
}
