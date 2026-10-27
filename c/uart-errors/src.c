// RV32_TEST_KIND: unit
#include "rv32_test.h"

#define UART_BASE      0xC0000000u
#define UART_TX_DATA   (*(volatile unsigned int *)(UART_BASE + 0x00u))
#define UART_RX_DATA   (*(volatile unsigned int *)(UART_BASE + 0x04u))
#define UART_STATUS    (*(volatile unsigned int *)(UART_BASE + 0x08u))

#define UART_RX_VALID   (1u << 0)
#define UART_TX_READY   (1u << 1)
#define UART_RX_OVERRUN (1u << 2)
#define UART_TX_DROPPED (1u << 3)
#define UART_ERROR_MASK (UART_RX_OVERRUN | UART_TX_DROPPED)

#define OVERRUN_LAST_BYTE 0x34u
#define BURST_BYTE_COUNT  32u

static void require(int condition)
{
    if (!condition) {
        RV32_FAIL();
    }
}

static void uart_wait_tx_ready(void)
{
    while ((UART_STATUS & UART_TX_READY) == 0u) {
    }
}

static void uart_putc(unsigned char value)
{
    uart_wait_tx_ready();
    UART_TX_DATA = value;
}

static unsigned char uart_getc(void)
{
    while ((UART_STATUS & UART_RX_VALID) == 0u) {
    }

    return (unsigned char)(UART_RX_DATA & 0xFFu);
}

int main(void)
{
    unsigned int status;

    /* Start from known sticky-error state. */
    UART_STATUS = UART_ERROR_MASK;
    require((UART_STATUS & UART_ERROR_MASK) == 0u);
    require((UART_STATUS & UART_TX_READY) != 0u);

    /*
     * 'A' starts a real frame. The immediately following 'X' is issued while
     * TX is busy and must be discarded while setting TX_DROPPED. The host
     * therefore observes "ATR", never "AXTR".
     */
    UART_TX_DATA = (unsigned int)'A';
    UART_TX_DATA = (unsigned int)'X';
    uart_wait_tx_ready();
    require((UART_STATUS & UART_TX_DROPPED) != 0u);

    UART_STATUS = UART_TX_DROPPED;
    require((UART_STATUS & UART_TX_DROPPED) == 0u);
    uart_putc((unsigned char)'T');
    uart_putc((unsigned char)'R');
    uart_wait_tx_ready();

    /*
     * After the 'R' prompt, the host sends 0x12 and 0x34 back-to-back. Do not
     * consume RX_DATA until the second byte has raised RX_OVERRUN.
     */
    while ((UART_STATUS & UART_RX_OVERRUN) == 0u) {
    }

    status = UART_STATUS;
    require((status & UART_RX_VALID) != 0u);
    require((status & UART_RX_OVERRUN) != 0u);
    require((unsigned int)uart_getc() == OVERRUN_LAST_BYTE);

    status = UART_STATUS;
    require((status & UART_RX_VALID) == 0u);
    require((status & UART_RX_OVERRUN) != 0u);

    UART_STATUS = UART_RX_OVERRUN;
    require((UART_STATUS & UART_RX_OVERRUN) == 0u);
    uart_putc((unsigned char)'O');
    uart_putc((unsigned char)'B');
    uart_wait_tx_ready();

    /* Echo one uninterrupted host write and require that it creates no error. */
    for (unsigned int index = 0; index < BURST_BYTE_COUNT; ++index) {
        uart_putc(uart_getc());
    }
    uart_wait_tx_ready();
    require((UART_STATUS & UART_ERROR_MASK) == 0u);

    uart_putc((unsigned char)'P');
    uart_wait_tx_ready();
    RV32_PASS();
}
