# PLL lock loss on the board

🌐 [Português](../../pt-br/bugs/pll-lock-loss.md) · [English](pll-lock-loss.md)

This document describes how the PLL of the board top loses and recovers lock, and how to recognize the situation in which the basic JTAG responds but the design does not.

## 1. Symptom

`jtagconfig` shows a healthy board (`02B050DD 5CE(BA4|FA4)`), but operations on the In-System Memory Content Editor (`riscv-tools run`, `write_rom`, `mailbox.read_mailbox`) fail or never respond. Typical messages are `no mailbox result after 15.0s`, `quartus_stp` hanging indefinitely, and `No editable memory instance was found`. Reprogramming the board (`quartus_pgm`, even with the `.sof`) does not fix it; without the wiring of section 4, only the physical power cycle does.

This situation differs from the loss of the JTAG chain ("Can't scan JTAG chain") that occurs when `quartus_sh --flow compile` and `quartus_pgm` run in separate calls. There `jtagconfig` stops seeing the chip; here it keeps seeing it.

## 2. The two JTAG layers

| Layer | Who uses it | Depends on the design clock |
| :--- | :--- | :--- |
| Basic JTAG TAP | `jtagconfig` | no; it is a fixed block of the chip, which responds even with the design broken, stuck, or absent |
| In-System Memory Content Editor | `read_words.tcl`, `write_full.tcl`, and `write_word.tcl`, via `begin_memory_edit` and `update_content_to_memory_from_file` | yes; it goes through a debug hub embedded in the fabric (`ENABLE_RUNTIME_MOD=YES`), which only responds if the logic and the clocks that feed it are running |

A healthy `jtagconfig` guarantees only layer 1. Layer 2 stays dead when the internal design is stuck, for example in permanent reset.

## 3. Cause chain

`core_fpga_test.vhd` derives all the clocks of the design (`pll_clk_if`, `pll_clk_idexmem`, `pll_clk_wb`) from a single PLL, and the core uses `pll_clk_idexmem`. The core reset is:

```vhdl
core_reset <= (not pll_locked) or (not FPGA_RESET_N);
```

If `pll_locked` drops, the whole core, and with it the layer 2 debug hub, stays in reset. The PLL only tries to lock again if it receives a pulse on the `rst` port, because the IP is generated with `gui_pll_auto_reset` set to `Off` and does not re-lock by itself. Quartus warns when this port is left unconnected:

```
Warning: RST port on the PLL is not properly connected on instance
pll:pll_inst|pll_0002:pll_inst|altera_pll:altera_pll_i|general[0].gpll.
```

With `rst` fixed at `'0'`, nothing pulses the PLL, and a lost lock never comes back. Retrying the operation does not help, reprogramming may not help, and the power cycle always helps because it reinitializes the PLL.

## 4. PLL reset wiring

The PLL reset is wired to the physical reset button:

```vhdl
pll_inst : entity work.pll
  port map (
    refclk   => CLOCK_50,
    rst      => not FPGA_RESET_N,
    ...
```

Pressing the button forces the PLL to lock again, besides holding the core in reset while `pll_locked` is low.

## 5. Recovery procedure

When layer 2 operations fail with a healthy `jtagconfig`, the recovery order is the following.

1. Press the physical reset button and repeat the operation.
2. If it persists, power-cycle the board and program it again with `quartus_sh` and `quartus_pgm` in a single shell process.

Step 2 follows the board programming rule, described in the [Insper-RISC-V board guide](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/en/guide/placa.md).

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
