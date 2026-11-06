/* The program update of the FLASH, on the firmware side (front E of the Insper-RISC-V plan).
 *
 * The host sends the image of a bank over the JTAG UART with `riscv-tools flash-image send`: the 256-byte
 * header page first and then the payload, as many bytes as the header says, with no framing. The firmware
 * (a program that runs from bank A, say) does five things, one function each:
 *
 *   rv32_update_receive   reads the header, then the payload, and checks the magic, the CRC-32 of the header
 *                         and the CRC-32 of the payload, so a stream that was cut or damaged never reaches
 *                         the FLASH;
 *   rv32_update_bank      copies the update routine to the SDRAM and runs it there: erase the sectors of the
 *                         destination bank, program the payload pages, program the header page LAST, read
 *                         everything back and compare with the buffer, then restart through the boot ROM;
 *   rv32_update_running   tells which bank the firmware runs from;
 *   rv32_update_check     reads a bank back from the FLASH and says whether its header and payload are valid
 *                         (what the boot ROM asks of it);
 *   rv32_update_cut       a flag of rv32_update_bank that leaves the header page erased, the state of an
 *                         update that lost power before the end: the boot ROM must refuse the bank.
 *
 * The routine is written in assembly, with no absolute address but the parameters it receives, so the
 * copy in the SDRAM runs as it is. It runs from the SDRAM because a FLASH chip of one bank cannot be read
 * while it is erased; here the destination is the other bank and the model keeps reading, but the firmware
 * does not depend on that. The registers of the controller are in Peripherals/docs/<lang>/flash-controller.
 *
 * Include it once per program. The program is built with Zifencei (// RV32_EXT: Zifencei), for the fence.i
 * that follows the copy of the routine. */
#ifndef RV32_FLASH_UPDATE_H
#define RV32_FLASH_UPDATE_H

#include <stdint.h>

#include "rv32_platform.h"

/* What the update of one bank answers. */
enum {
    RV32_UPDATE_OK = 0,
    RV32_UPDATE_REFUSED = 1,  /* the chip refused an erase or a program (protected sector, bad address) */
    RV32_UPDATE_MISMATCH = 2, /* what the FLASH holds after the update is not what was sent */
    RV32_UPDATE_TIMEOUT = 3,  /* the host stopped sending */
    RV32_UPDATE_BAD_HEADER = 4,
    RV32_UPDATE_TOO_BIG = 5,
    RV32_UPDATE_BAD_CRC = 6,
    RV32_UPDATE_NO_BANK = 7
};

/* Flags of rv32_update_bank. */
#define RV32_UPDATE_CUT 1u     /* do not write the header page */
#define RV32_UPDATE_RESTART 2u /* on success, restart through the boot ROM instead of returning */

#define RV32_HEADER_SIZE 256u
#define RV32_SECTOR_SIZE 4096u
#define RV32_PAGE_SIZE 256u
#define RV32_FLASH_ERASED_WORD 0xFFFFFFFFu

#define RV32_UART_RXDATA ((volatile uint32_t *)(RV32_UART_BASE + 4u))
#define RV32_UART_STATUS ((volatile uint32_t *)(RV32_UART_BASE + 8u))
#define RV32_UART_RX_VALID(word) (((word) >> 8) & 1u)
#define RV32_UART_HOST_ATTACHED(status) (((status) >> 17) & 1u)
#define RV32_UPDATE_POLLS 40000000u

/* The parameters of the routine, read by the assembly below at these offsets. */
struct rv32_update_job {
    uint32_t image;  /* 0: the image in the SDRAM, header page first, a whole number of pages */
    uint32_t size;   /* 4: bytes of the image */
    uint32_t dest;   /* 8: base of the destination bank in the FLASH, aligned to a sector */
    uint32_t ctrl;   /* 12: base of the FLASH controller */
    uint32_t flags;  /* 16: RV32_UPDATE_CUT, RV32_UPDATE_RESTART */
};

/* CRC-32 of the IEEE 802.3, as zlib and the header of the image: the bitwise form, which is small and fast
 * enough for the few KB of a bank in a test. */
static inline uint32_t rv32_crc32(uint32_t crc, const volatile uint8_t *data, uint32_t length) {
    crc = ~crc;
    for (uint32_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

/* The word a field of the header page holds, little-endian. */
static inline uint32_t rv32_header_word(const volatile uint8_t *header, unsigned int index) {
    return (uint32_t)header[4 * index] | ((uint32_t)header[4 * index + 1] << 8) |
           ((uint32_t)header[4 * index + 2] << 16) | ((uint32_t)header[4 * index + 3] << 24);
}

/* Checks the header page at `header`; on success sets *size to the bytes of the payload. */
static inline int rv32_header_valid(const volatile uint8_t *header, uint32_t max_payload, uint32_t *size) {
    uint8_t body[16];
    for (int i = 0; i < 16; i++) body[i] = header[i];
    uint32_t length = rv32_header_word(header, 1);
    if (rv32_header_word(header, 0) != RV32_IMAGE_MAGIC) return RV32_UPDATE_BAD_HEADER;
    if (rv32_crc32(0, body, 16) != rv32_header_word(header, 4)) return RV32_UPDATE_BAD_HEADER;
    if (length == 0 || (length & 3u)) return RV32_UPDATE_BAD_HEADER;
    if (length > max_payload) return RV32_UPDATE_TOO_BIG;
    *size = length;
    return RV32_UPDATE_OK;
}

/* Reads one byte from the UART; -1 when none arrived within the polls. */
static inline int rv32_update_getc(uint32_t polls) {
    for (uint32_t i = 0; i < polls; i++) {
        uint32_t word = *RV32_UART_RXDATA;
        if (RV32_UART_RX_VALID(word)) return (int)(word & 0xFFu);
    }
    return -1;
}

/* Receives an image into `buffer` (a multiple of four bytes, `capacity` bytes) and checks it. On success
 * *length is the bytes of the image (header page and payload), already padded with the erased byte to a
 * whole number of pages for the routine, and the buffer holds the image. */
static inline int rv32_update_receive(uint8_t *buffer, uint32_t capacity, uint32_t *length) {
    for (uint32_t i = 0; i < RV32_UPDATE_POLLS && !RV32_UART_HOST_ATTACHED(*RV32_UART_STATUS); i++) {
    }
    if (!RV32_UART_HOST_ATTACHED(*RV32_UART_STATUS)) return RV32_UPDATE_TIMEOUT;
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

/* The bank (RV32_BANK_x_BASE) that holds the code at `address`; 0 when it is in neither. */
static inline uint32_t rv32_update_bank_of(uint32_t address) {
    if (address >= RV32_BANK_A_BASE && address < RV32_BANK_A_BASE + RV32_BANK_A_SIZE) return RV32_BANK_A_BASE;
    if (address >= RV32_BANK_B_BASE && address < RV32_BANK_B_BASE + RV32_BANK_B_SIZE) return RV32_BANK_B_BASE;
    return 0;
}

/* Reads the bank back and says whether its image is valid, as the boot ROM sees it. */
static inline int rv32_update_check(uint32_t base, uint32_t bank_size, uint32_t *version) {
    const volatile uint8_t *header = (const volatile uint8_t *)base;
    uint32_t payload = 0;
    int status = rv32_header_valid(header, bank_size - RV32_HEADER_SIZE, &payload);
    if (status != RV32_UPDATE_OK) return status;
    if (rv32_crc32(0, header + RV32_HEADER_SIZE, payload) != rv32_header_word(header, 2)) return RV32_UPDATE_BAD_CRC;
    *version = rv32_header_word(header, 3);
    return RV32_UPDATE_OK;
}

/* The routine, at the start and at the end of its code (copied as a block). int routine(job *): returns
 * RV32_UPDATE_OK, RV32_UPDATE_REFUSED or RV32_UPDATE_MISMATCH, or does not return on success with
 * RV32_UPDATE_RESTART. s0 = image, s1 = size, s2 = dest, s3 = controller, s4 = flags, s5 = offset. */
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
    /* erase every sector of the destination */
    "    li   s5, 0\n"
    "1:  bgeu s5, s1, 2f\n"
    "    add  a1, s2, s5\n"
    "    li   a2, 2\n"
    "    jal  t6, 8f\n"
    "    bnez a3, 7f\n"
    "    li   t0, 4096\n"
    "    add  s5, s5, t0\n"
    "    j    1b\n"
    /* program the payload pages, from the second page on */
    "2:  li   s5, 256\n"
    "3:  bgeu s5, s1, 4f\n"
    "    jal  ra, 9f\n"
    "    bnez a3, 7f\n"
    "    addi s5, s5, 256\n"
    "    j    3b\n"
    /* the header page last, unless the update is cut */
    "4:  andi t0, s4, 1\n"
    "    bnez t0, 5f\n"
    "    li   s5, 0\n"
    "    jal  ra, 9f\n"
    "    bnez a3, 7f\n"
    /* read everything back and compare with the buffer (the header page too unless it was not written) */
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
    /* 8: command a2 on the address a1 with write enable first; a3 = nonzero when the chip refused (link in t6) */
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
    /* 9: program the page at offset s5 with the 64 words of the buffer (link in ra) */
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
    ".text\n");

extern const uint32_t rv32_flash_update_start[];
extern const uint32_t rv32_flash_update_end[];

/* Runs the update of the bank at `dest` with the image in `image` (`size` bytes, a whole number of pages) from
 * a copy of the routine in the SDRAM. `ram_code` is the buffer for the copy, at least `words` words. Returns
 * RV32_UPDATE_OK, RV32_UPDATE_REFUSED or RV32_UPDATE_MISMATCH; with RV32_UPDATE_RESTART it does not return on
 * success. */
static inline int rv32_update_bank(
    uint32_t dest, uint8_t *image, uint32_t size, uint32_t flags, uint32_t *ram_code, uint32_t words) {
    uint32_t routine = (uint32_t)(rv32_flash_update_end - rv32_flash_update_start);
    if (routine > words) return RV32_UPDATE_TOO_BIG;
    for (uint32_t i = 0; i < routine; i++) ram_code[i] = rv32_flash_update_start[i];
    __asm__ volatile("fence.i" ::: "memory");
    volatile struct rv32_update_job job = {
        .image = (uint32_t)image, .size = size, .dest = dest, .ctrl = RV32_FLASH_CTRL_BASE, .flags = flags};
    return ((int (*)(volatile struct rv32_update_job *))ram_code)(&job);
}

#endif
