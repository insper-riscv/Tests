// RV32_EXT: M,Zifencei
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include "rv32_flash_update.h"
#include <stdio.h>

// An update that loses power before the end: the header page of bank B is never written.
//
// Same program as flash-update up to the routine, which is told to cut the update (RV32_UPDATE_CUT): it erases
// bank B and programs the payload but leaves the header page erased, and restarts. The boot ROM finds no valid
// header in bank B and runs bank A again, which is the survivor word's second run: it reads bank B back, finds
// the header invalid and the payload there (the cut was after the payload, not before everything), prints the
// line of uart_expected.txt and passes.

#define IMAGE_CAPACITY 4096u
#define CODE_WORDS 160u
#define SURVIVOR ((volatile uint32_t *)(RV32_RAM_BASE + 0x03000000u))
#define UPDATING 0x0FEDCBA9u

static uint8_t image[IMAGE_CAPACITY] __attribute__((aligned(4)));
static uint32_t ram_code[CODE_WORDS];

int main(void) {
    if (rv32_update_bank_of((uint32_t)(uintptr_t)&main) != RV32_BANK_A_BASE) RV32_FAIL(1);
    if (*SURVIVOR == UPDATING) {
        uint32_t version = 0;
        if (rv32_update_check(RV32_BANK_B_BASE, RV32_BANK_B_SIZE, &version) == RV32_UPDATE_OK) RV32_FAIL(3);
        if (*(volatile uint32_t *)(RV32_BANK_B_BASE + RV32_HEADER_SIZE) == RV32_FLASH_ERASED_WORD) RV32_FAIL(4);
        *SURVIVOR = 0;
        printf("interrupted update: the boot ROM stayed on bank A\n");
        RV32_PASS();
    }
    uint32_t length = 0;
    int status = rv32_update_receive(image, IMAGE_CAPACITY, &length);
    if (status != RV32_UPDATE_OK) RV32_FAIL(10 + status);
    *SURVIVOR = UPDATING;
    status = rv32_update_bank(
        RV32_BANK_B_BASE, image, length, RV32_UPDATE_CUT | RV32_UPDATE_RESTART, ram_code, CODE_WORDS);
    RV32_FAIL(20 + status);
}
