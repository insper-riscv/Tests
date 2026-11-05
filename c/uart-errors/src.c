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
/* Máscara do erro persistente causado por recepção com FIFO cheia. */
static constexpr uint32_t UART_RX_OVERRUN = UINT32_C(1) << 2;
/* Máscara do erro persistente causado por transmissão com FIFO cheia. */
static constexpr uint32_t UART_TX_DROPPED = UINT32_C(1) << 3;
/* Máscara que indica FIFO de recepção cheia. */
static constexpr uint32_t UART_RX_FULL = UINT32_C(1) << 4;
/* Máscara que indica transmissor físico e FIFO completamente ociosos. */
static constexpr uint32_t UART_TX_IDLE = UINT32_C(1) << 6;
/* Deslocamento do nível da FIFO de recepção no registrador de estado. */
static constexpr uint32_t UART_RX_LEVEL_SHIFT = UINT32_C(8);
/* Deslocamento do nível da FIFO de transmissão no registrador de estado. */
static constexpr uint32_t UART_TX_LEVEL_SHIFT = UINT32_C(16);
/* Máscara dos campos de nível das FIFOs. */
static constexpr uint32_t UART_LEVEL_MASK = UINT32_C(0xFF);
/* Máscara conjunta dos erros persistentes da UART. */
static constexpr uint32_t UART_ERROR_MASK = UART_RX_OVERRUN | UART_TX_DROPPED;
/* Profundidade configurada para cada FIFO da UART. */
static constexpr uint32_t FIFO_DEPTH = UINT32_C(16);
/* Quantidade aceita entre o transmissor físico e a FIFO cheia. */
static constexpr uint32_t TX_ACCEPTED_COUNT = FIFO_DEPTH + UINT32_C(1);
/* Quantidade de bytes usada para testar retorno circular das FIFOs. */
static constexpr uint32_t BURST_BYTE_COUNT = UINT32_C(32);

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

/* Escreve no registrador de estado para limpar erros persistentes. */
static void uart_write_status(uint32_t value)
{
    *uart_register(UART_STATUS_ADDRESS) = value;
}

/* Interrompe o teste caso uma condição obrigatória seja falsa. */
static void require(bool condition)
{
    if (!condition) {
        RV32_FAIL(1);
    }
}

/* Aguarda espaço disponível na FIFO de transmissão. */
static void uart_wait_tx_ready(void)
{
    while ((uart_status() & UART_TX_READY) == UINT32_C(0)) {
    }
}

/* Extrai do estado o nível atual da FIFO de recepção. */
static uint32_t uart_rx_level(uint32_t status)
{
    return (status >> UART_RX_LEVEL_SHIFT) & UART_LEVEL_MASK;
}

/* Extrai do estado o nível atual da FIFO de transmissão. */
static uint32_t uart_tx_level(uint32_t status)
{
    return (status >> UART_TX_LEVEL_SHIFT) & UART_LEVEL_MASK;
}

/* Aguarda o esvaziamento da FIFO e a conclusão do último stop bit. */
static void uart_wait_tx_idle(void)
{
    while ((uart_status() & UART_TX_IDLE) == UINT32_C(0)) {
    }
}

/* Insere um byte na FIFO de transmissão após aguardar espaço. */
static void uart_putc(uint8_t value)
{
    uart_wait_tx_ready();
    *uart_register(UART_TX_DATA_ADDRESS) = value;
}

/* Aguarda e remove o próximo byte da FIFO de recepção. */
static uint8_t uart_getc(void)
{
    while ((uart_status() & UART_RX_VALID) == UINT32_C(0)) {
    }

    return (uint8_t)(*uart_register(UART_RX_DATA_ADDRESS) & UINT32_C(0xFF));
}

/* Valida capacidade, descarte, erros persistentes e retorno circular. */
int main(void)
{
    uint32_t status;

    /* Inicia com erros persistentes limpos e filas vazias. */
    uart_write_status(UART_ERROR_MASK);
    status = uart_status();
    require((status & UART_ERROR_MASK) == UINT32_C(0));
    require((status & UART_TX_IDLE) != UINT32_C(0));
    require(uart_rx_level(status) == UINT32_C(0));
    require(uart_tx_level(status) == UINT32_C(0));

    /*
     * O primeiro byte entra no transmissor físico e os 16 seguintes preenchem
     * a FIFO. O byte 'X' deve ser rejeitado sem alterar os bytes de 'A' a 'Q'.
     */
    for (uint32_t index = 0; index < TX_ACCEPTED_COUNT; ++index) {
        *uart_register(UART_TX_DATA_ADDRESS) = (uint8_t)('A' + index);
    }
    *uart_register(UART_TX_DATA_ADDRESS) = (uint8_t)'X';

    status = uart_status();
    require((status & UART_TX_READY) == UINT32_C(0));
    require((status & UART_TX_DROPPED) != UINT32_C(0));
    require(uart_tx_level(status) == FIFO_DEPTH);

    uart_write_status(UART_TX_DROPPED);
    require((uart_status() & UART_TX_DROPPED) == UINT32_C(0));
    uart_wait_tx_idle();

    /* Solicita 17 bytes ao host sem consumir o registrador de recepção. */
    uart_putc((uint8_t)'R');
    uart_wait_tx_idle();

    while ((uart_status() & UART_RX_OVERRUN) == UINT32_C(0)) {
    }

    status = uart_status();
    require((status & UART_RX_VALID) != UINT32_C(0));
    require((status & UART_RX_FULL) != UINT32_C(0));
    require((status & UART_RX_OVERRUN) != UINT32_C(0));
    require(uart_rx_level(status) == FIFO_DEPTH);

    /* A recepção preserva os 16 primeiros bytes e descarta apenas o 17º. */
    for (uint32_t index = 0; index < FIFO_DEPTH; ++index) {
        require(uart_getc() == (uint8_t)index);
    }

    status = uart_status();
    require((status & UART_RX_VALID) == UINT32_C(0));
    require((status & UART_RX_OVERRUN) != UINT32_C(0));
    require(uart_rx_level(status) == UINT32_C(0));

    uart_write_status(UART_RX_OVERRUN);
    require((uart_status() & UART_ERROR_MASK) == UINT32_C(0));

    /* Confirma a recuperação antes da rajada que testa o retorno circular. */
    uart_putc((uint8_t)'O');
    uart_wait_tx_idle();

    for (uint32_t index = 0; index < BURST_BYTE_COUNT; ++index) {
        uart_putc(uart_getc());
    }
    uart_wait_tx_idle();
    require((uart_status() & UART_ERROR_MASK) == UINT32_C(0));

    uart_putc((uint8_t)'P');
    uart_wait_tx_idle();
    RV32_PASS();
}
