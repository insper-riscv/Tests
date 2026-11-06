// RV32_TEST_KIND: unit
#include <stdint.h>

#include "rv32_test.h"

/* Endereço do registrador de dados de transmissão da UART. */
static constexpr uintptr_t UART_TX_DATA_ADDRESS = UINT32_C(0xC0000000);
/* Endereço do registrador de dados de recepção da UART. */
static constexpr uintptr_t UART_RX_DATA_ADDRESS = UINT32_C(0xC0000004);
/* Endereço do registrador de estado da UART. */
static constexpr uintptr_t UART_STATUS_ADDRESS = UINT32_C(0xC0000008);
/* Máscara que indica a presença de um byte na FIFO de recepção. */
static constexpr uint32_t UART_RX_VALID = UINT32_C(1) << 0;
/* Máscara que indica espaço disponível na FIFO de transmissão. */
static constexpr uint32_t UART_TX_READY = UINT32_C(1) << 1;
/* Máscara que indica transmissor físico e FIFO completamente ociosos. */
static constexpr uint32_t UART_TX_IDLE = UINT32_C(1) << 6;
/* Quantidade de bytes recebidos e devolvidos pelo programa. */
static constexpr uint32_t ECHO_BYTE_COUNT = UINT32_C(9);

/* Converte um endereço MMIO em ponteiro para registrador de 32 bits. */
static volatile uint32_t *uart_register(uintptr_t address)
{
    return (volatile uint32_t *)address;
}

/* Lê o estado atual da UART. */
static uint32_t uart_status(void)
{
    return *uart_register(UART_STATUS_ADDRESS);
}

/* Aguarda e remove o próximo byte da FIFO de recepção. */
static uint8_t uart_getc(void)
{
    while ((uart_status() & UART_RX_VALID) == UINT32_C(0)) {
    }

    return (uint8_t)(*uart_register(UART_RX_DATA_ADDRESS) & UINT32_C(0xFF));
}

/* Aguarda espaço e insere um byte na FIFO de transmissão. */
static void uart_putc(uint8_t value)
{
    while ((uart_status() & UART_TX_READY) == UINT32_C(0)) {
    }

    *uart_register(UART_TX_DATA_ADDRESS) = value;
}

/* Ecoa nove bytes e encerra após a transmissão física do último byte. */
int main(void)
{
    for (uint32_t index = 0; index < ECHO_BYTE_COUNT; ++index) {
        uart_putc(uart_getc());
    }

    /* TX_READY indica espaço na FIFO; TX_IDLE confirma o último stop bit. */
    while ((uart_status() & UART_TX_IDLE) == UINT32_C(0)) {
    }

    RV32_PASS();
}
