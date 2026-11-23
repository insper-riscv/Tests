// RV32_TEST_KIND: unit
#include <stdint.h>

#include "rv32_platform.h"
#include "rv32_test.h"

// O estado em que a BOOT deixa o núcleo, lido logo na main, para cada extensão que o perfil liga.
// O mesmo código serve a todos os perfis: o que depende de uma extensão só é compilado quando o -march
// do perfil a traz (o compilador define __riscv_<extensão>), então o rv32i e o rv32e checam só o que
// existe neles, as palavras que o hospedeiro e o programa dividem.
//
//   1. tohost, fromhost e o cabeçalho da saída padrão estão zerados (a BOOT os limpa a cada entrada);
//   2. com Zicsr, mstatus, mie e mscratch estão zerados e o mtvec aponta, em modo direto, para o
//      tratador seguro da BOOT (uma palavra alinhada, dentro dos 4 KiB da BOOT_MEMORY);
//   3. com Zicntr, mcountinhibit não inibe o ciclo nem as instruções, e os contadores correm desde a
//      BOOT, então minstret e mcycle ainda são pequenos;
//   4. com Zihpm, os 29 seletores, contadores e metades altas estão zerados.

// Tamanho da BOOT_MEMORY, onde mora o tratador seguro (4 KiB em toda plataforma de teste).
[[maybe_unused]] constexpr uint32_t BOOT_MEMORY_SIZE = 0x1000u;
// Quantas instruções e ciclos o núcleo pode ter gasto da BOOT até a main (o crt0 e a cache fria).
[[maybe_unused]] constexpr uint32_t MAX_INSTRET = 100'000u;
[[maybe_unused]] constexpr uint32_t MAX_MCYCLE = 4'000'000u;
// Os bits CY e IR do mcountinhibit, que inibiriam mcycle e minstret.
[[maybe_unused]] constexpr uint32_t INHIBIT_CY_IR = 0b101u;

// Lê uma palavra do tohost, do fromhost ou do cabeçalho da saída padrão.
static uint32_t read_word(uintptr_t address) {
    return *(volatile uint32_t *)address;
}

#ifdef __riscv_zicsr
// Define a função que lê um CSR de endereço constante (o csrr só aceita o endereço como constante).
#define DEFINE_CSR_READER(name, number)                                    \
    static uint32_t name(void) {                                           \
        uint32_t value;                                                    \
        __asm__ volatile("csrr %0, " #number : "=r"(value));               \
        return value;                                                      \
    }

DEFINE_CSR_READER(read_mstatus, 0x300)
DEFINE_CSR_READER(read_mie, 0x304)
DEFINE_CSR_READER(read_mtvec, 0x305)
DEFINE_CSR_READER(read_mscratch, 0x340)
#endif

#ifdef __riscv_zicntr
DEFINE_CSR_READER(read_mcountinhibit, 0x320)
DEFINE_CSR_READER(read_minstret, 0xB02)
DEFINE_CSR_READER(read_mcycle, 0xB00)
DEFINE_CSR_READER(read_minstreth, 0xB82)
DEFINE_CSR_READER(read_mcycleh, 0xB80)
#endif

int main(void) {
    // 1. As palavras do hospedeiro: tohost e fromhost têm duas metades cada, e a saída padrão tem o
    // tamanho e o sinalizador de truncamento.
    if (read_word(RV32_TOHOST) != 0u || read_word(RV32_TOHOST + 4u) != 0u) RV32_FAIL(1);
    if (read_word(RV32_TOHOST + 8u) != 0u || read_word(RV32_TOHOST + 12u) != 0u) RV32_FAIL(2);
    if (read_word(RV32_STDOUT_BASE) != 0u || read_word(RV32_STDOUT_BASE + 4u) != 0u) RV32_FAIL(3);

#ifdef __riscv_zicsr
    // 2. O estado de máquina: nenhuma interrupção habilitada nem global, e o mtvec no tratador da BOOT.
    if (read_mstatus() != 0u) RV32_FAIL(4);
    if (read_mie() != 0u) RV32_FAIL(5);
    if (read_mscratch() != 0u) RV32_FAIL(6);
    const uint32_t mtvec = read_mtvec();
    if ((mtvec & 3u) != 0u || mtvec == 0u || mtvec >= BOOT_MEMORY_SIZE) RV32_FAIL(7);
#endif

#ifdef __riscv_zicntr
    // 3. Os contadores correm desde a BOOT e nada os inibe.
    if ((read_mcountinhibit() & INHIBIT_CY_IR) != 0u) RV32_FAIL(8);
    if (read_minstreth() != 0u || read_mcycleh() != 0u) RV32_FAIL(9);
    const uint32_t instret = read_minstret();
    if (instret == 0u || instret > MAX_INSTRET) RV32_FAIL(10);
    const uint32_t cycles = read_mcycle();
    if (cycles < instret || cycles > MAX_MCYCLE) RV32_FAIL(11);
#endif

#ifdef __riscv_zihpm
    // 4. Seletores (0x323 a 0x33F), contadores (0xB03 a 0xB1F) e metades altas (0xB83 a 0xB9F) zerados.
    // O csr do assembly pede endereço constante, então a soma OU dos 29 de cada grupo roda num .rept.
    uint32_t selectors = 0u;
    uint32_t counters = 0u;
    uint32_t highs = 0u;
    __asm__ volatile(
        ".set .Lhpm_index, 3\n"
        ".rept 29\n"
        "csrr t0, 0x320 + .Lhpm_index\n or %0, %0, t0\n"
        "csrr t0, 0xB00 + .Lhpm_index\n or %1, %1, t0\n"
        "csrr t0, 0xB80 + .Lhpm_index\n or %2, %2, t0\n"
        ".set .Lhpm_index, .Lhpm_index + 1\n"
        ".endr\n"
        : "+r"(selectors), "+r"(counters), "+r"(highs)
        :
        : "t0");
    if (selectors != 0u) RV32_FAIL(12);
    if (counters != 0u) RV32_FAIL(13);
    if (highs != 0u) RV32_FAIL(14);
#endif

    RV32_PASS();
}
