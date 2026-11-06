# `.data` initialization on the Harvard core

🌐 [Português](../../pt-br/bugs/data-harvard.md) · [English](data-harvard.md)

This document describes how the core accesses the FLASH from the MEM stage, how the boot copies `.data` to the RAM, and what this allows the test programs to do.

## 1. Core buses

The core (`rv32im_pipeline_core`) is Harvard. The instruction and data buses are physically independent.

| Bus | Address source | Reaches |
| :--- | :--- | :--- |
| `rom_addr` | `pc_fetch` | BOOT_ROM and FLASH, for instruction fetch |
| `ram_addr` | `exmem_alu_out`, that is, the ALU result of any `lw` or `sw` | RAM |
| second read port of the MEM stage | `exmem_alu_out` | FLASH, only for the `.data` copy at boot |

Without the second port, no `lw` or `sw` would reach the FLASH, and the standard `.data` mechanism could not work. The bus separation is a choice of this core, and the RISC-V ISA does not require it.

The split of the memory into BOOT_ROM (fixed bootloader), FLASH (firmware of each test), and RAM (data), with the address map, is described in the [Insper-RISC-V memory documentation](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/en/platform/memoria.md).

## 2. `.data` copy at boot

`crt0.S` copies `.data` from the FLASH to the RAM with a loop of `lw` and `sw`, using the load address `_data_load` and the range `_data_start` to `_data_end` from `link.ld`. Each `lw` from the FLASH goes through the second port of the MEM stage. `.bss` is zeroed by `sw`, without reading the ROM. The placement rule for the small-data sections that enter this range is in the [small data](small-data-section.md) document.

## 3. What this allows a test to do

A plain C initializer at file scope works:

```c
static volatile unsigned int value = 0xCAFEBABEu;

int main(void) {
    if (value != 0xCAFEBABEu) RV32_FAIL();
    RV32_PASS();
}
```

The tests `c/data-init-byte`, `c/data-init-halfword`, `c/data-init-word`, `c/data-init-word-array`, and `c/data-initialization` verify this initialization, each with a different data size or shape.

## 4. Reading the mailbox over JTAG

The orchestrator reads the program verdict from the mailbox by polling, and each read is a `quartus_stp` call that opens and closes a JTAG and In-System Memory Editor session. A high volume of fast calls is fragile, in the same class as the loss of the JTAG chain between `quartus_sh` and `quartus_pgm`. The `poll_interval_seconds` of the build configuration sets the interval between reads; a larger value reduces the number of sessions opened within the timeout window, trading detection latency for read reliability.

To tell whether a program hung or was merely not read, the mailbox can be read once, after giving the program time:

```bash
quartus_stp -t write_full.tcl "USB-Blaster [1-4]" "@1: ..." 0 data-init-byte.mif
quartus_stp -t write_word.tcl "USB-Blaster [1-4]" "@1: ..." 1 4094 1   # go-flag
sleep 5
quartus_stp -t read_words.tcl "USB-Blaster [1-4]" "@1: ..." 1 4095 1   # mailbox
```

A mailbox value of `2` is FAIL: the CPU finished and wrote the verdict.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
