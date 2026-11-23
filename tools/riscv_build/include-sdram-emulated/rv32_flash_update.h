/* A atualização de programa da FLASH, do lado do firmware (frente E do plano do Insper-RISC-V).
 *
 * O host envia a imagem de uma banca pela UART JTAG com `riscv-tools flash-image send`: primeiro a página de
 * cabeçalho de 256 bytes e depois a carga, tantos bytes quanto o cabeçalho diz, sem enquadramento. O firmware
 * (um programa que roda da banca A, por exemplo) faz cinco coisas, uma função cada:
 *
 *   rv32_update_receive   lê o cabeçalho e depois a carga, e confere o número mágico, o CRC-32 do cabeçalho e o
 *                         CRC-32 da carga, de modo que um fluxo cortado ou danificado nunca chega à FLASH;
 *   rv32_update_bank      copia a rotina de atualização para a SDRAM e a executa lá: apaga os setores da banca
 *                         de destino, programa as páginas da carga, programa a página de cabeçalho POR ÚLTIMO,
 *                         lê tudo de volta e compara com o buffer, e então reinicia pela memória de boot;
 *   rv32_update_running   diz de qual banca o firmware executa;
 *   rv32_update_check     lê uma banca de volta da FLASH e diz se o cabeçalho e a carga são válidos (o que a
 *                         memória de boot pergunta a ela);
 *   rv32_update_cut       um sinalizador de rv32_update_bank que deixa a página de cabeçalho apagada, o estado
 *                         de uma atualização que perdeu energia antes do fim: a memória de boot deve recusar a banca.
 *
 * A rotina é escrita em assembly, sem endereço absoluto além dos parâmetros que recebe, para que a cópia na SDRAM
 * rode como está. Ela roda da SDRAM porque um chip de FLASH de uma banca não pode ser lido enquanto é apagado;
 * aqui o destino é a outra banca e o modelo continua lendo, mas o firmware não depende disso. Os registradores
 * do controlador estão em Peripherals/docs/<idioma>/flash-controller.
 *
 * O assembly da rotina é um bloco __asm__ de nível de arquivo: `asm` não é palavra-chave da norma (ISO/IEC
 * 9899:2024, 6.4.2), e `__asm__` é a única extensão do GCC que este arquivo usa. Os deslocamentos da estrutura
 * rv32_update_job que a rotina lê por posição fixa são fixados por static_assert (6.7.12).
 *
 * Inclua uma vez por programa. O programa é construído com Zifencei (// RV32_EXT: Zifencei), para o fence.i que
 * segue a cópia da rotina. */
#ifndef RV32_FLASH_UPDATE_H
#define RV32_FLASH_UPDATE_H

#include <stddef.h>
#include <stdint.h>

#include "rv32_platform.h"

/* O que a atualização de uma banca responde. */
enum {
    RV32_UPDATE_OK = 0,
    RV32_UPDATE_REFUSED = 1,  /* o chip recusou um apagamento ou uma programação (setor protegido, endereço inválido) */
    RV32_UPDATE_MISMATCH = 2, /* o que a FLASH guarda após a atualização não é o que foi enviado */
    RV32_UPDATE_TIMEOUT = 3,  /* o host parou de enviar */
    RV32_UPDATE_BAD_HEADER = 4,
    RV32_UPDATE_TOO_BIG = 5,
    RV32_UPDATE_BAD_CRC = 6,
    RV32_UPDATE_NO_BANK = 7
};

/* Sinalizadores de rv32_update_bank. */
/* Não escreve a página de cabeçalho. */
static constexpr uint32_t RV32_UPDATE_CUT = 1u;
/* Em caso de sucesso, reinicia pela memória de boot em vez de retornar. */
static constexpr uint32_t RV32_UPDATE_RESTART = 2u;

/* Tamanho da página de cabeçalho de uma imagem, em bytes. */
static constexpr uint32_t RV32_HEADER_SIZE = 256u;
/* Tamanho de um setor de apagamento da FLASH, em bytes. */
static constexpr uint32_t RV32_SECTOR_SIZE = 4096u;
/* Tamanho de uma página de programação da FLASH, em bytes. */
static constexpr uint32_t RV32_PAGE_SIZE = 256u;
/* Palavra que a FLASH apagada guarda. */
static constexpr uint32_t RV32_FLASH_ERASED_WORD = 0xFFFFFFFFu;
/* Número de leituras do status da UART à espera de um byte ou do host. */
static constexpr uint32_t RV32_UPDATE_POLLS = 40000000u;

/* Registradores de recepção e de estado da UART JTAG. Ficam como macros porque um ponteiro para endereço fixo
 * não pode ser constexpr (ISO/IEC 9899:2024, 6.7.2: só o ponteiro nulo); o acesso é volatile (6.7.4.1) e
 * alinhado a palavra. */
#define RV32_UART_RXDATA ((volatile uint32_t *)(RV32_UART_BASE + 4u))
#define RV32_UART_STATUS ((volatile uint32_t *)(RV32_UART_BASE + 8u))

/* Diz se a palavra lida de rxdata traz um byte válido (bit 8). */
static inline bool rv32_uart_rx_valid(uint32_t word) {
    return ((word >> 8) & 1u) != 0u;
}

/* Diz se um host está ligado à UART JTAG (bit 17 do estado). */
static inline bool rv32_uart_host_attached(uint32_t status) {
    return ((status >> 17) & 1u) != 0u;
}

/* Os parâmetros da rotina, lidos pelo assembly abaixo nestes deslocamentos. */
struct rv32_update_job {
    uint32_t image;  /* 0: a imagem na SDRAM, página de cabeçalho primeiro, um número inteiro de páginas */
    uint32_t size;   /* 4: bytes da imagem */
    uint32_t dest;   /* 8: base da banca de destino na FLASH, alinhada a um setor */
    uint32_t ctrl;   /* 12: base do controlador da FLASH */
    uint32_t flags;  /* 16: RV32_UPDATE_CUT, RV32_UPDATE_RESTART */
};

/* O assembly lê os campos por posição fixa (lw s0, 0(a0) a lw s4, 16(a0)); estas asserções falham a compilação
 * se o layout mudar sem a rotina mudar. */
static_assert(offsetof(struct rv32_update_job, image) == 0);
static_assert(offsetof(struct rv32_update_job, size) == 4);
static_assert(offsetof(struct rv32_update_job, dest) == 8);
static_assert(offsetof(struct rv32_update_job, ctrl) == 12);
static_assert(offsetof(struct rv32_update_job, flags) == 16);
static_assert(sizeof(struct rv32_update_job) == 20);

/* CRC-32 do IEEE 802.3, como o zlib e o cabeçalho da imagem: a forma bit a bit, que é pequena e rápida o bastante
 * para os poucos KB de uma banca num teste. */
static inline uint32_t rv32_crc32(uint32_t crc, const volatile uint8_t *data, uint32_t length) {
    crc = ~crc;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint32_t bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* A palavra que um campo da página de cabeçalho guarda, em little-endian. */
static inline uint32_t rv32_header_word(const volatile uint8_t *header, uint32_t index) {
    return (uint32_t)header[4 * index] | ((uint32_t)header[4 * index + 1] << 8) |
           ((uint32_t)header[4 * index + 2] << 16) | ((uint32_t)header[4 * index + 3] << 24);
}

/* Confere a página de cabeçalho em `header`; em caso de sucesso grava em *size os bytes da carga. */
static inline int rv32_header_valid(const volatile uint8_t *header, uint32_t max_payload, uint32_t *size) {
    uint8_t body[16];
    for (uint32_t i = 0; i < 16; i++) body[i] = header[i];
    uint32_t length = rv32_header_word(header, 1);
    if (rv32_header_word(header, 0) != RV32_IMAGE_MAGIC) return RV32_UPDATE_BAD_HEADER;
    if (rv32_crc32(0, body, 16) != rv32_header_word(header, 4)) return RV32_UPDATE_BAD_HEADER;
    if (length == 0 || (length & 3u)) return RV32_UPDATE_BAD_HEADER;
    if (length > max_payload) return RV32_UPDATE_TOO_BIG;
    *size = length;
    return RV32_UPDATE_OK;
}

/* Lê um byte da UART; -1 quando nenhum chegou dentro das leituras. */
static inline int rv32_update_getc(uint32_t polls) {
    for (uint32_t i = 0; i < polls; i++) {
        uint32_t word = *RV32_UART_RXDATA;
        if (rv32_uart_rx_valid(word)) return (int)(word & 0xFFu);
    }
    return -1;
}

/* Recebe uma imagem em `buffer` (múltiplo de quatro bytes, `capacity` bytes) e a confere. Em caso de sucesso,
 * *length são os bytes da imagem (página de cabeçalho e carga), já completados com o byte de apagado até um
 * número inteiro de páginas para a rotina, e o buffer guarda a imagem. */
static inline int rv32_update_receive(uint8_t *buffer, uint32_t capacity, uint32_t *length) {
    for (uint32_t i = 0; i < RV32_UPDATE_POLLS && !rv32_uart_host_attached(*RV32_UART_STATUS); i++) {
    }
    if (!rv32_uart_host_attached(*RV32_UART_STATUS)) return RV32_UPDATE_TIMEOUT;
    uint32_t received = 0;
    uint32_t payload = 0;
    uint32_t total = RV32_HEADER_SIZE;
    while (received < total) {
        int byte = rv32_update_getc(RV32_UPDATE_POLLS);
        if (byte < 0) return RV32_UPDATE_TIMEOUT;
        buffer[received++] = (uint8_t)byte;
        if (received == RV32_HEADER_SIZE) {
            int status = rv32_header_valid(buffer, capacity - RV32_HEADER_SIZE, &payload);
            if (status != RV32_UPDATE_OK) return status;
            total = RV32_HEADER_SIZE + payload;
        }
    }
    if (rv32_crc32(0, buffer + RV32_HEADER_SIZE, payload) != rv32_header_word(buffer, 2)) return RV32_UPDATE_BAD_CRC;
    uint32_t padded = (total + RV32_PAGE_SIZE - 1u) & ~(RV32_PAGE_SIZE - 1u);
    if (padded > capacity) return RV32_UPDATE_TOO_BIG;
    while (total < padded) buffer[total++] = 0xFFu;
    *length = padded;
    return RV32_UPDATE_OK;
}

/* A banca (RV32_BANK_x_BASE) que guarda o código em `address`; 0 quando ele não está em nenhuma. */
static inline uint32_t rv32_update_bank_of(uint32_t address) {
    if (address >= RV32_BANK_A_BASE && address < RV32_BANK_A_BASE + RV32_BANK_A_SIZE) return RV32_BANK_A_BASE;
    if (address >= RV32_BANK_B_BASE && address < RV32_BANK_B_BASE + RV32_BANK_B_SIZE) return RV32_BANK_B_BASE;
    return 0;
}

/* Lê a banca de volta e diz se a sua imagem é válida, como a memória de boot a vê. */
static inline int rv32_update_check(uint32_t base, uint32_t bank_size, uint32_t *version) {
    const volatile uint8_t *header = (const volatile uint8_t *)base;
    uint32_t payload = 0;
    int status = rv32_header_valid(header, bank_size - RV32_HEADER_SIZE, &payload);
    if (status != RV32_UPDATE_OK) return status;
    if (rv32_crc32(0, header + RV32_HEADER_SIZE, payload) != rv32_header_word(header, 2)) return RV32_UPDATE_BAD_CRC;
    *version = rv32_header_word(header, 3);
    return RV32_UPDATE_OK;
}

/* A rotina, do início ao fim do seu código (copiada como um bloco). int rotina(job *): retorna
 * RV32_UPDATE_OK, RV32_UPDATE_REFUSED ou RV32_UPDATE_MISMATCH, ou não retorna em caso de sucesso com
 * RV32_UPDATE_RESTART. s0 = imagem, s1 = tamanho, s2 = destino, s3 = controlador, s4 = sinalizadores, s5 = deslocamento. */
__asm__(
    ".section .text.rv32_flash_update,\"ax\",@progbits\n"
    ".balign 4\n"
    ".globl rv32_flash_update_start\n"
    ".globl rv32_flash_update_end\n"
    "rv32_flash_update_start:\n"
    "    addi sp, sp, -32\n"
    "    sw   ra, 28(sp)\n"
    "    sw   s0, 24(sp)\n"
    "    sw   s1, 20(sp)\n"
    "    sw   s2, 16(sp)\n"
    "    sw   s3, 12(sp)\n"
    "    sw   s4, 8(sp)\n"
    "    sw   s5, 4(sp)\n"
    "    lw   s0, 0(a0)\n"
    "    lw   s1, 4(a0)\n"
    "    lw   s2, 8(a0)\n"
    "    lw   s3, 12(a0)\n"
    "    lw   s4, 16(a0)\n"
    /* apaga cada setor do destino */
    "    li   s5, 0\n"
    "1:  bgeu s5, s1, 2f\n"
    "    add  a1, s2, s5\n"
    "    li   a2, 2\n"
    "    jal  t6, 8f\n"
    "    bnez a3, 7f\n"
    "    li   t0, 4096\n"
    "    add  s5, s5, t0\n"
    "    j    1b\n"
    /* programa as páginas da carga, da segunda página em diante */
    "2:  li   s5, 256\n"
    "3:  bgeu s5, s1, 4f\n"
    "    jal  ra, 9f\n"
    "    bnez a3, 7f\n"
    "    addi s5, s5, 256\n"
    "    j    3b\n"
    /* a página de cabeçalho por último, a menos que a atualização seja cortada */
    "4:  andi t0, s4, 1\n"
    "    bnez t0, 5f\n"
    "    li   s5, 0\n"
    "    jal  ra, 9f\n"
    "    bnez a3, 7f\n"
    /* lê tudo de volta e compara com o buffer (a página de cabeçalho também, a menos que não tenha sido escrita) */
    "5:  li   s5, 0\n"
    "    andi t0, s4, 1\n"
    "    beqz t0, 51f\n"
    "    li   s5, 256\n"
    "51: bgeu s5, s1, 6f\n"
    "    add  t0, s0, s5\n"
    "    add  t1, s2, s5\n"
    "    lw   t0, 0(t0)\n"
    "    lw   t1, 0(t1)\n"
    "    bne  t0, t1, 71f\n"
    "    addi s5, s5, 4\n"
    "    j    51b\n"
    "6:  li   a0, 0\n"
    "    andi t0, s4, 2\n"
    "    beqz t0, 10f\n"
    "    .word 0x0000100F\n"
    "    li   t0, 0\n"
    "    jr   t0\n"
    "7:  li   a0, 1\n"
    "    j    10f\n"
    "71: li   a0, 2\n"
    "10: lw   ra, 28(sp)\n"
    "    lw   s0, 24(sp)\n"
    "    lw   s1, 20(sp)\n"
    "    lw   s2, 16(sp)\n"
    "    lw   s3, 12(sp)\n"
    "    lw   s4, 8(sp)\n"
    "    lw   s5, 4(sp)\n"
    "    addi sp, sp, 32\n"
    "    ret\n"
    /* 8: comando a2 no endereço a1 com a habilitação de escrita antes; a3 = diferente de zero quando o chip recusou (retorno em t6) */
    "8:  sw   a1, 4(s3)\n"
    "81: lw   t3, 8(s3)\n"
    "    andi t3, t3, 1\n"
    "    bnez t3, 81b\n"
    "    li   t3, 1\n"
    "    sw   t3, 0(s3)\n"
    "82: lw   t3, 8(s3)\n"
    "    andi t3, t3, 1\n"
    "    bnez t3, 82b\n"
    "    sw   a2, 0(s3)\n"
    "83: lw   t3, 8(s3)\n"
    "    andi t3, t3, 1\n"
    "    bnez t3, 83b\n"
    "    lw   a3, 8(s3)\n"
    "    andi a3, a3, 4\n"
    "    jr   t6\n"
    /* 9: programa a página no deslocamento s5 com as 64 palavras do buffer (retorno em ra) */
    "9:  sw   x0, 12(s3)\n"
    "    add  a4, s0, s5\n"
    "    li   a5, 64\n"
    "91: lw   t4, 0(a4)\n"
    "    sw   t4, 16(s3)\n"
    "    addi a4, a4, 4\n"
    "    addi a5, a5, -1\n"
    "    bnez a5, 91b\n"
    "    add  a1, s2, s5\n"
    "    li   a2, 3\n"
    "    jal  t6, 8b\n"
    "    ret\n"
    "rv32_flash_update_end:\n"
    /* fora do bloco copiado: o número de palavras da rotina, calculado pelo montador */
    ".globl rv32_flash_update_words\n"
    "rv32_flash_update_words:\n"
    "    .word (rv32_flash_update_end - rv32_flash_update_start) / 4\n"
    ".text\n");

/* O início do código da rotina e o seu tamanho em palavras, ambos definidos pelo assembly acima. O tamanho vem de
 * uma palavra que o montador calcula, e não da diferença entre dois ponteiros, que só é definida para elementos
 * do mesmo objeto de vetor (ISO/IEC 9899:2024, 6.5.7). */
extern const uint32_t rv32_flash_update_start[];
extern const uint32_t rv32_flash_update_words;

/* Executa a atualização da banca em `dest` com a imagem em `image` (`size` bytes, um número inteiro de páginas) a
 * partir de uma cópia da rotina na SDRAM. `ram_code` é o buffer da cópia, de pelo menos `words` palavras.
 * Retorna RV32_UPDATE_OK, RV32_UPDATE_REFUSED ou RV32_UPDATE_MISMATCH; com RV32_UPDATE_RESTART não retorna em
 * caso de sucesso. A conversão do endereço do buffer para ponteiro de função passa por uintptr_t: a norma não
 * define a conversão de ponteiro para objeto em ponteiro para função (ISO/IEC 9899:2024, 6.3.3.3 e cláusula 4),
 * e a de inteiro em ponteiro tem resultado definido pela implementação, que no GCC para RISC-V preserva o
 * endereço. */
static inline int rv32_update_bank(
    uint32_t dest, uint8_t *image, uint32_t size, uint32_t flags, uint32_t *ram_code, uint32_t words) {
    uint32_t routine = rv32_flash_update_words;
    if (routine > words) return RV32_UPDATE_TOO_BIG;
    for (uint32_t i = 0; i < routine; i++) ram_code[i] = rv32_flash_update_start[i];
    __asm__ volatile("fence.i" ::: "memory");
    volatile struct rv32_update_job job = {
        .image = (uint32_t)(uintptr_t)image, .size = size, .dest = dest, .ctrl = RV32_FLASH_CTRL_BASE, .flags = flags};
    return ((int (*)(volatile struct rv32_update_job *))(uintptr_t)ram_code)(&job);
}

#endif
