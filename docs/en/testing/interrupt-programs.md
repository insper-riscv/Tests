# Interrupt programs

🌐 [Português](../../pt-br/testing/interrupt-programs.md) · [English](interrupt-programs.md)

The programs in `asm-irq/` verify the interrupt entry of the `Core` with real executable code. Each one checks itself and ends by writing to `tohost`. `tests/python` builds them, loads them into the `core` with `has_interrupts` and runs them against a CLINT model.

## 1. Programs

| Program | What it verifies |
| :--- | :--- |
| `irq-timer-basic` | A timer interrupt in direct mode enters once, with `mcause = 0x80000007`, `mepc` inside the running loop, `MIE` zero and `MPIE` one inside the handler, and `mret` gives `MIE` and control back to the loop |
| `irq-masked` | Nothing is taken with `mstatus.MIE` at zero or with the `mie` bit at zero, although `mip.MTIP` shows the pending line. When the `mie` bit is set, the interrupt takes the next instruction, which runs once after the return |
| `irq-mepc-precise` | A block of 400 additions is interrupted every few tens of cycles. At each entry the summed register equals the number of additions before `mepc`, and at the end it is 400 |
| `irq-vectored` | With vectored `mtvec`, the software interrupt goes to `BASE + 12`, the timer one to `BASE + 28` and the `ecall` (cause 11, an exception) to `BASE`. The mode is kept, bit 1 drops and `BASE` loses bits 5 to 2 |
| `irq-priority` | Software and timer pending together: the software one enters first and the timer one right after, both over the same instruction |
| `irq-wfi` | `wfi` executes as a `nop`, with and without a pending interrupt, and an interrupt taken on it gives control back to the `wfi` itself |
| `irq-ecall` | With an interrupt pending when the `ecall` arrives, the interrupt enters first and the `ecall` traps after the return, with the same `mepc` |

## 2. CLINT model

The model has the layout of the SiFive CLINT, with 32-bit accesses and the low half at the lower address.

| Offset | Register |
| :---: | :--- |
| `0x0000` | `msip` (bit 0), which drives the `MSIP` line |
| `0x4000` | `mtimecmp` low |
| `0x4004` | `mtimecmp` high |
| `0xBFF8` | `mtime` low |
| `0xBFFC` | `mtime` high |

`mtime` adds 1 per cycle, `mtimecmp` is all ones after reset and the `MTIP` line is $mtime \ge mtimecmp$ in 64 bits. The model occupies the region of `RV32_CLINT_BASE` inside the internal RAM range. A platform with the interrupts wired defines that constant in its generated header, and the programs use the same names.

## 3. Usage

```bash
uv run python tests/python/runner.py irq-programs
```

The command needs the RISC-V toolchain on the `PATH`, as in the `ci-elf` image. The pointer of the `Insper-RISC-V` submodule must bring the `Core` with the interrupt entry.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
