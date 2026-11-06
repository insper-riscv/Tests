// RV32_TEST_KIND: unit
#include <stdint.h>

#include "rv32_test.h"

/* Endereço do registrador de dados de transmissão da UART. */
static constexpr uintptr_t UART_TX_DATA_ADDRESS = UINT32_C(0xC0000000);
/* Endereço do registrador de estado da UART. */
static constexpr uintptr_t UART_STATUS_ADDRESS = UINT32_C(0xC0000008);
/* Máscara que indica espaço disponível na FIFO de transmissão. */
static constexpr uint32_t UART_TX_READY = UINT32_C(1) << 1;
/* Máscara que indica transmissor físico e FIFO completamente ociosos. */
static constexpr uint32_t UART_TX_IDLE = UINT32_C(1) << 6;
/* Mensagem transmitida pelo programa. */
static constexpr char MESSAGE[] = "Hello, world!\r\n";

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

/* Aguarda espaço disponível na FIFO de transmissão. */
static void uart_wait_tx_ready(void)
{
    while ((uart_status() & UART_TX_READY) == UINT32_C(0)) {
    }
}

/* Insere um byte na FIFO de transmissão. */
static void uart_putc(uint8_t value)
{
    uart_wait_tx_ready();
    *uart_register(UART_TX_DATA_ADDRESS) = value;
}

/* Aguarda o esvaziamento da FIFO e a conclusão do último stop bit. */
static void uart_wait_tx_idle(void)
{
    while ((uart_status() & UART_TX_IDLE) == UINT32_C(0)) {
    }
}

/* Transmite a mensagem de referência e encerra após o último byte. */
int main(void)
{
    for (uint32_t index = 0; MESSAGE[index] != '\0'; ++index) {
        uart_putc((uint8_t)MESSAGE[index]);
    }

    /* TX_READY indica espaço na FIFO; TX_IDLE confirma o último stop bit. */
    uart_wait_tx_idle();

    RV32_PASS();
}
