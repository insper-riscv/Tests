# Per-entity tests (`tests/python/`)

🌐 [Português](../../pt-br/bugs/per-entity-tests-ci-breakage.md) · [English](per-entity-tests-ci-breakage.md)

This document describes the cocotb test suite for VHDL entities and raw instruction sequences, and the conditions for the `instructions` group to run again.

## 1. The suite

The suite runs with `uv run python tests/python/runner.py`, separate from the `riscv-tools` suite (`asm` and `c`). It tests individual VHDL entities (`ALU`, `RegFile`, `ROM_simulation`, and others) and a few raw instruction sequences, all configured in `tests/python/tests.json`, without going through the `riscv-tools` compiler, linker, or `config.yaml`.

`tests.json` has two groups of entries.

| Group | Content | State in `all` |
| :--- | :--- | :--- |
| `entities` | one testbench per entity | runs |
| `instructions` | `one`, `two`, `three`, `four`, `five`, `six`, `MUL`, `FWD`, `LOAD_USE`, and `MEXT`, raw instruction sequences | skipped |

## 2. Entries with `skip`

An entry with `"skip": true` and a `"skip_reason"` stays out of the `all` sweep; the runner prints the reason and moves on. Called by name (`python3 tests/python/runner.py one`), the same entry runs normally, which lets one see the current error without editing `tests.json`. It is the same mechanism as the `.off` files of the `asm` and `c` suites, adapted to the single configuration format of this suite.

## 3. Why the `instructions` group is skipped

The top `rv32i3stage_core_sim_test.vhd` instantiates two copies of `ROM_simulation`, one for the BOOT_ROM (generic `ROM_FILE => BOOT_ROM_FILE`) and one for the FLASH (generic `ROM_FILE => ROM_FILE`). The ten entries only pass `ROM_FILE`. Without `BOOT_ROM_FILE`, the BOOT_ROM uses the default value of the entity, `default.hex`, which does not exist, and the elaboration fails before any test:

```
rom_simulation.vhd:79:12:error: cannot open file: default.hex
```

Passing `BOOT_ROM_FILE` would make the elaboration pass, but the tests would still be wrong, because each module assumes that reset goes straight to the contents of `ROM_FILE` at address 0. In the memory architecture, reset always enters through the BOOT_ROM, and user code starts at `FLASH_BASE = 0x800`, after the boot. The PC computation in `tests/python/unittests/instructions/one.py` shows the assumption:

```python
# PC of each AUIPC: starts at 4 * instruction number (single-cycle model)
pcs = [i * 4 for i in range(len(auipc_immediates))]
```

The `.hex` files in `tests/python/unittests/instructions/codes/` also start at address 0 of the array, without the padding that `riscv-tools` applies to align the content to `FLASH_BASE`. The BOOT_ROM jump also adds bubbles to the pipeline.

## 4. Conditions to re-enable a test of the group

Each of the ten tests needs three items.

1. A real `BOOT_ROM_FILE`, such as `boot_rom.hex` or a minimal stub that only executes `j 0x800`.
2. The `.hex` of the test generated again, with the `FLASH_BASE` padding.
3. The PC and cycle-count computations of the Python module rewritten to include the boot delay.

The memory architecture and the boot are described in the [Insper-RISC-V memory documentation](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/en/platform/memoria.md).

## 5. Test failure in cocotb 2.0

In cocotb 2.0, any exception raised inside the test marks it as FAIL, and the `TestFailure` class of `cocotb.result` does not exist. Tests must fail with `AssertionError`:

```python
raise AssertionError("message")
```

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
