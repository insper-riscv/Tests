// RV32_EXT: M, Zicsr
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include <stdint.h>

// O CLINT gera as linhas MSIP e MTIP do núcleo, e o csr mip as mostra. O programa confere que:
//   1. com o mtimecmp no maior valor e o msip em zero, o mip não tem MSIP nem MTIP;
//   2. escrever 1 no msip acende o bit MSIP do mip, e escrever 0 o apaga;
//   3. armar o mtimecmp poucos ticks à frente do mtime acende o bit MTIP do mip, e voltar o
//      mtimecmp ao maior valor o apaga.
// O mtime do CLINT conta no ritmo do mtime_hz da plataforma e não depende do csr time.

// Registradores do CLINT (RV32_CLINT_BASE, endereços do platform.yaml da plataforma).
constexpr uintptr_t CLINT_MSIP = RV32_CLINT_BASE;
constexpr uintptr_t CLINT_MTIMECMP = RV32_CLINT_BASE + 0x4000u;
constexpr uintptr_t CLINT_MTIME = RV32_CLINT_BASE + 0xBFF8u;

// Bits do mip e quantos ticks do mtime separam o armamento do disparo.
constexpr uint32_t MIP_MSIP = 1u << 3;
constexpr uint32_t MIP_MTIP = 1u << 7;
constexpr uint32_t ARM_TICKS = 4u;
// Limite de leituras do mip à espera do MTIP, para um timer parado não travar o programa.
constexpr uint32_t POLL_LIMIT = 200000u;

// Lê o csr mip.
static uint32_t read_mip(void) {
    uint32_t value;
    __asm__ volatile("csrr %0, mip" : "=r"(value));
    return value;
}

// Lê uma palavra de um registrador do CLINT.
static uint32_t clint_read(uintptr_t address) {
    return *(volatile uint32_t *)address;
}

// Escreve uma palavra num registrador do CLINT.
static void clint_write(uintptr_t address, uint32_t value) {
    *(volatile uint32_t *)address = value;
}

// Escreve o mtimecmp pela sequência do porte do FreeRTOS: a metade baixa vai a todos os bits em
// um, depois a metade alta e por fim a baixa, então a comparação nunca dispara no meio da escrita.
static void write_mtimecmp(uint32_t high, uint32_t low) {
    clint_write(CLINT_MTIMECMP, UINT32_MAX);
    clint_write(CLINT_MTIMECMP + 4u, high);
    clint_write(CLINT_MTIMECMP, low);
}

int main(void) {
    write_mtimecmp(UINT32_MAX, UINT32_MAX);
    clint_write(CLINT_MSIP, 0u);
    // A saída do comparador é registrada: espera alguns ciclos para o MTIP baixar.
    for (volatile uint32_t i = 0; i < 64u; i++) {
    }
    if ((read_mip() & (MIP_MSIP | MIP_MTIP)) != 0u) RV32_FAIL(1);

    clint_write(CLINT_MSIP, 1u);
    if ((read_mip() & MIP_MSIP) == 0u) RV32_FAIL(2);
    clint_write(CLINT_MSIP, 0u);
    if ((read_mip() & MIP_MSIP) != 0u) RV32_FAIL(3);

    // O mtime sobe um tick por mtime_hz: lê a metade baixa e arma o disparo ARM_TICKS à frente.
    write_mtimecmp(0u, clint_read(CLINT_MTIME) + ARM_TICKS);
    uint32_t polls = 0u;
    while ((read_mip() & MIP_MTIP) == 0u) {
        if (++polls >= POLL_LIMIT) RV32_FAIL(4);
    }

    write_mtimecmp(UINT32_MAX, UINT32_MAX);
    for (volatile uint32_t i = 0; i < 64u; i++) {
    }
    if ((read_mip() & MIP_MTIP) != 0u) RV32_FAIL(5);

    RV32_PASS();
}
