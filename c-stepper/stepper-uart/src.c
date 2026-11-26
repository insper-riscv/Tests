// RV32_EXT: Zicsr,Zicntr
// RV32_TEST_KIND: unit
// RV32_TIMEOUT_S: 15
#include <stdint.h>
#include <stdio.h>

#include "rv32_test.h"
#include "../../Insper-RISC-V/Peripherals/src/GPIO/soc/driver/c/rv32_gpio.h"

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

/* Instancia do driver ligada ao GPIO do motor pela configuracao da plataforma. */
static rv32_gpio_t motor_gpio;

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
        rv32_gpio_write(motor_gpio, UINT32_C(0));
        RV32_FAIL(1);
    }
}

/* Aplica um estado de quatro bits as entradas do ULN2003. */
static void write_coils(uint32_t value)
{
    rv32_gpio_write(motor_gpio, value);
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
        require(rv32_gpio_read_output(motor_gpio) == sequence[index]);
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

    motor_gpio = rv32_gpio_platform_new(RV32_GPIO_DEFAULT_INSTANCE);
    require(rv32_gpio_valid(motor_gpio));

    /* Carrega zero antes de habilitar as quatro linhas como saidas. */
    write_coils(UINT32_C(0));
    rv32_gpio_direction(motor_gpio, MOTOR_OUTPUT_MASK);
    require(rv32_gpio_get_direction(motor_gpio) == MOTOR_OUTPUT_MASK);

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
