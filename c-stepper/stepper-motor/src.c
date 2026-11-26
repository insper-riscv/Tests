// RV32_EXT: Zicsr,Zicntr
// RV32_TEST_KIND: unit
// RV32_TIMEOUT_S: 15
#include <stdint.h>

#include "rv32_test.h"
#include "../../Insper-RISC-V/Peripherals/src/GPIO/soc/driver/c/rv32_gpio.h"

/* Máscara das quatro entradas IN1 a IN4 do ULN2003. */
static constexpr uint32_t MOTOR_OUTPUT_MASK = UINT32_C(0xF);
/* Clock do núcleo no topo físico internal-mem. */
static constexpr uint32_t CORE_CLOCK_HZ = UINT32_C(50'000'000);
/* Intervalo nominal de dois milissegundos entre meios passos. */
static constexpr uint32_t STEP_DELAY_CYCLES = CORE_CLOCK_HZ / UINT32_C(500);
/* Pausa de 250 milissegundos entre os dois sentidos. */
static constexpr uint32_t DIRECTION_PAUSE_CYCLES = CORE_CLOCK_HZ / UINT32_C(4);
/* Quantidade de meios passos em cada sentido, cerca de um quarto de volta. */
static constexpr uint32_t STEPS_PER_DIRECTION = UINT32_C(1024);
/* Quantidade de estados da sequência de meio passo. */
static constexpr uint32_t SEQUENCE_LENGTH = UINT32_C(8);
/* Sequência de meio passo para IN1, IN2, IN3 e IN4, do bit 0 ao bit 3. */
static constexpr uint32_t HALF_STEP_SEQUENCE[SEQUENCE_LENGTH] = {
    UINT32_C(0x1),
    UINT32_C(0x3),
    UINT32_C(0x2),
    UINT32_C(0x6),
    UINT32_C(0x4),
    UINT32_C(0xC),
    UINT32_C(0x8),
    UINT32_C(0x9),
};

/* Instância do driver ligada ao GPIO do motor pela configuração da plataforma. */
static rv32_gpio_t motor_gpio;

/* Lê a metade baixa do contador de ciclos do núcleo. */
static uint32_t read_cycle(void)
{
    uint32_t value;

    /* O C não expressa leitura de CSR; a instrução csrr lê mcycle. */
    __asm__ volatile("csrr %0, mcycle" : "=r"(value));
    return value;
}

/* Aguarda a quantidade pedida de ciclos, inclusive quando mcycle dá a volta. */
static void wait_cycles(uint32_t cycles)
{
    uint32_t start = read_cycle();

    while ((uint32_t)(read_cycle() - start) < cycles) {
    }
}

/* Interrompe o teste caso uma condição obrigatória seja falsa. */
static void require(bool condition)
{
    if (!condition) {
        RV32_FAIL(1);
    }
}

/* Aplica um estado de quatro bits às entradas do ULN2003. */
static void write_coils(uint32_t value)
{
    rv32_gpio_write(motor_gpio, value);
}

/* Confere o caminho CPU/GPIO e expõe uma sequência curta à simulação. */
static void validate_half_step_sequence(void)
{
    for (uint32_t index = 0; index < SEQUENCE_LENGTH; ++index) {
        uint32_t expected = HALF_STEP_SEQUENCE[index];

        write_coils(expected);
        require(rv32_gpio_read_output(motor_gpio) == expected);
    }
    write_coils(UINT32_C(0));
}

/* Executa a quantidade pedida de meios passos no sentido direto. */
static void rotate_forward(uint32_t steps)
{
    for (uint32_t step = 0; step < steps; ++step) {
        write_coils(HALF_STEP_SEQUENCE[step % SEQUENCE_LENGTH]);
        wait_cycles(STEP_DELAY_CYCLES);
    }
}

/* Executa a quantidade pedida de meios passos no sentido inverso. */
static void rotate_reverse(uint32_t steps)
{
    for (uint32_t step = 0; step < steps; ++step) {
        uint32_t index = SEQUENCE_LENGTH - UINT32_C(1)
                         - ((step + UINT32_C(1)) % SEQUENCE_LENGTH);

        write_coils(HALF_STEP_SEQUENCE[index]);
        wait_cycles(STEP_DELAY_CYCLES);
    }
}

/* Gira cerca de um quarto de volta em cada sentido e desenergiza o motor. */
int main(void)
{
    motor_gpio = rv32_gpio_platform_new(RV32_GPIO_DEFAULT_INSTANCE);
    require(rv32_gpio_valid(motor_gpio));

    /* Carrega zero antes de habilitar as quatro linhas como saídas. */
    write_coils(UINT32_C(0));
    rv32_gpio_direction(motor_gpio, MOTOR_OUTPUT_MASK);

    require(rv32_gpio_get_direction(motor_gpio) == MOTOR_OUTPUT_MASK);
    require(rv32_gpio_read_output(motor_gpio) == UINT32_C(0));
    validate_half_step_sequence();

    rotate_forward(STEPS_PER_DIRECTION);
    wait_cycles(DIRECTION_PAUSE_CYCLES);
    rotate_reverse(STEPS_PER_DIRECTION);

    /* Nenhuma bobina permanece energizada depois do teste. */
    write_coils(UINT32_C(0));
    require(rv32_gpio_read_output(motor_gpio) == UINT32_C(0));
    RV32_PASS();
}
