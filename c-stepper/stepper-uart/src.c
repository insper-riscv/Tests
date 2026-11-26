// RV32_EXT: Zicsr,Zicntr
// RV32_TEST_KIND: unit
// RV32_TIMEOUT_S: 15
#include <stdint.h>
#include <stdio.h>

#include "rv32_platform.h"
#include "rv32_test.h"

/* Endereco do registrador de direcao do GPIO do motor. */
static constexpr uintptr_t MOTOR_DIR_ADDRESS = RV32_MOTOR_BASE;
/* Endereco do registrador que carrega as quatro saidas do motor. */
static constexpr uintptr_t MOTOR_OUT_ADDRESS = RV32_MOTOR_BASE + UINT32_C(4);
/* Mascara das quatro entradas IN1 a IN4 do ULN2003. */
static constexpr uint32_t MOTOR_OUTPUT_MASK = UINT32_C(0xF);
/* Clock do nucleo no topo fisico internal-mem. */
static constexpr uint32_t CORE_CLOCK_HZ = UINT32_C(50'000'000);
/* Intervalo nominal de dois milissegundos entre meios passos. */
static constexpr uint32_t STEP_DELAY_CYCLES = CORE_CLOCK_HZ / UINT32_C(500);
/* Quantidade de meios passos executada por comando. */
static constexpr uint32_t STEPS_PER_COMMAND = UINT32_C(1024);
/* Quantidade de estados da sequencia de meio passo. */
static constexpr uint32_t SEQUENCE_LENGTH = UINT32_C(8);

/* Sequencias direta e inversa para IN1, IN2, IN3 e IN4. */
static constexpr uint32_t FORWARD_SEQUENCE[SEQUENCE_LENGTH] = {
    UINT32_C(0x1),
    UINT32_C(0x3),
    UINT32_C(0x2),
    UINT32_C(0x6),
    UINT32_C(0x4),
    UINT32_C(0xC),
    UINT32_C(0x8),
    UINT32_C(0x9),
};

static constexpr uint32_t REVERSE_SEQUENCE[SEQUENCE_LENGTH] = {
    UINT32_C(0x9),
    UINT32_C(0x8),
    UINT32_C(0xC),
    UINT32_C(0x4),
    UINT32_C(0x6),
    UINT32_C(0x2),
    UINT32_C(0x3),
    UINT32_C(0x1),
};

/* Converte um endereco MMIO em ponteiro para registrador de 32 bits. */
static volatile uint32_t *motor_register(uintptr_t address)
{
    return (volatile uint32_t *)address;
}

/* Le a metade baixa do contador de ciclos do nucleo. */
static uint32_t read_cycle(void)
{
    uint32_t value;

    __asm__ volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

/* Aguarda a quantidade pedida de ciclos, inclusive quando mcycle da a volta. */
static void wait_cycles(uint32_t cycles)
{
    uint32_t start = read_cycle();

    while ((uint32_t)(read_cycle() - start) < cycles) {
    }
}

/* Interrompe o programa caso o caminho CPU/GPIO nao funcione. */
static void require(bool condition)
{
    if (!condition) {
        *motor_register(MOTOR_OUT_ADDRESS) = UINT32_C(0);
        RV32_FAIL(1);
    }
}

/* Aplica um estado de quatro bits as entradas do ULN2003. */
static void write_coils(uint32_t value)
{
    *motor_register(MOTOR_OUT_ADDRESS) = value;
}

/*
 * Confere rapidamente a sequencia escolhida antes do movimento temporizado.
 * Alem do autoteste do GPIO, essas oito escritas permitem que a simulacao
 * valide o comando UART sem simular os dois segundos do movimento fisico.
 */
static void validate_sequence(const uint32_t sequence[SEQUENCE_LENGTH])
{
    for (uint32_t index = 0; index < SEQUENCE_LENGTH; ++index) {
        write_coils(sequence[index]);
        require(*motor_register(MOTOR_OUT_ADDRESS) == sequence[index]);
    }
    write_coils(UINT32_C(0));
}

/* Executa um movimento bloqueante e desenergiza as bobinas ao final. */
static void rotate(const uint32_t sequence[SEQUENCE_LENGTH])
{
    validate_sequence(sequence);

    for (uint32_t step = 0; step < STEPS_PER_COMMAND; ++step) {
        write_coils(sequence[step % SEQUENCE_LENGTH]);
        wait_cycles(STEP_DELAY_CYCLES);
    }

    write_coils(UINT32_C(0));
}

/* Espera comandos pela UART e executa um movimento completo por comando. */
int main(void)
{
    char command;

    /* Carrega zero antes de habilitar as quatro linhas como saidas. */
    write_coils(UINT32_C(0));
    *motor_register(MOTOR_DIR_ADDRESS) = MOTOR_OUTPUT_MASK;
    require(*motor_register(MOTOR_DIR_ADDRESS) == MOTOR_OUTPUT_MASK);

    printf("READY\n");

    for (;;) {
        if (scanf(" %c", &command) != 1) {
            write_coils(UINT32_C(0));
            continue;
        }

        switch (command) {
        case 'd':
        case 'D':
            printf("DIREITA\n");
            rotate(FORWARD_SEQUENCE);
            printf("CONCLUIDO\n");
            break;

        case 'e':
        case 'E':
            printf("ESQUERDA\n");
            rotate(REVERSE_SEQUENCE);
            printf("CONCLUIDO\n");
            break;

        default:
            write_coils(UINT32_C(0));
            printf("ERRO\n");
            break;
        }
    }
}
