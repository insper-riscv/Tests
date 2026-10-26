// RV32_TEST_KIND: unit
#include "rv32_test.h"

#define UART_BASE      0xC0000000u
#define UART_TX_DATA   (*(volatile unsigned int *)(UART_BASE + 0x00u))
#define UART_RX_DATA   (*(volatile unsigned int *)(UART_BASE + 0x04u))
#define UART_STATUS    (*(volatile unsigned int *)(UART_BASE + 0x08u))

#define UART_RX_VALID  (1u << 0)
#define UART_TX_READY  (1u << 1)

#define ECHO_BYTE_COUNT 9u

static unsigned char uart_getc(void)
{
    while ((UART_STATUS & UART_RX_VALID) == 0u) {
    }

    return (unsigned char)(UART_RX_DATA & 0xFFu);
}

static void uart_putc(unsigned char value)
{
    while ((UART_STATUS & UART_TX_READY) == 0u) {
    }

    UART_TX_DATA = value;
}

int main(void)
{
    for (unsigned int i = 0; i < ECHO_BYTE_COUNT; ++i) {
        uart_putc(uart_getc());
    }

    /* Aguarda a transmissão física do último byte. */
    while ((UART_STATUS & UART_TX_READY) == 0u) {
    }

    RV32_PASS();
}
