// RV32_EXT: Zifencei
// RV32_TEST_KIND: unit
#include "rv32_test.h"
#include "rv32_platform.h"
#include "rv32_flash_update.h"

// A complete update of bank B, as the firmware of a product does it.
//
// The program is bank A (version 1). The host sends the image of bank B over the JTAG UART (in simulation
// the testbench sends uart_input.txt, the output of `riscv-tools flash-image pack` for update-new); the
// program checks the stream, runs the update routine from the SDRAM (erase, program the payload, program the
// header last, read back) and restarts through the boot ROM. The boot ROM sees two valid banks and runs the
// newer one, so what ends the test is bank B writing its tohost: update-new prints the line of
// uart_expected.txt. If the boot ROM runs bank A again, the survivor word tells it and the test fails.
//
// The FLASH controller exists only in the simulation top with HAS_FLASH_CONTROLLER (config.sdram-flash-update*.yaml).

#define IMAGE_CAPACITY 4096u
#define CODE_WORDS 160u
#define SURVIVOR ((volatile uint32_t *)(RV32_RAM_BASE + 0x03000000u))
#define UPDATING 0x0FEDCBA9u

static uint8_t image[IMAGE_CAPACITY] __attribute__((aligned(4)));
static uint32_t ram_code[CODE_WORDS];

int main(void) {
    if (rv32_update_bank_of((uint32_t)(uintptr_t)&main) != RV32_BANK_A_BASE) RV32_FAIL(1);
    // the update restarted the core and the boot ROM came back here: it did not take bank B
    if (*SURVIVOR == UPDATING) RV32_FAIL(2);
    uint32_t length = 0;
    int status = rv32_update_receive(image, IMAGE_CAPACITY, &length);
    if (status != RV32_UPDATE_OK) RV32_FAIL(10 + status);
    *SURVIVOR = UPDATING;
    status = rv32_update_bank(RV32_BANK_B_BASE, image, length, RV32_UPDATE_RESTART, ram_code, CODE_WORDS);
    RV32_FAIL(20 + status);
}
