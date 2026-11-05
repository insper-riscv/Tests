# Testes

🌐 [Português](README.md) · [English](README.en.md)

Real-hardware and simulation test suite of the `RV32IM` RISC-V core, driven by the `riscv-tools` package, installed in the CI images.

## 1. Documentation

- [AUTHORS.en.md](AUTHORS.en.md): authors with commits in the repository, with name and GitHub account.
- [CONTRIBUTING.en.md](CONTRIBUTING.en.md): mandatory contribution rules, C23 and Python standards, documentation rules, and verification before opening a Pull Request.
- [CITATION.cff](CITATION.cff): software citation metadata.
- [tools/riscv_build/README.md](tools/riscv_build/README.md): conventions specific to this suite (test format, hardware facts, and how to write a new test).
- [placa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/placa.md): permanent rule on compiling and programming the board, and the JTAG bug already diagnosed.
- [memoria.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md): BOOT_ROM, FLASH, and RAM architecture.
- [runtime.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/runtime.md): C runtime (picolibc, `crt0`, and toolchain linker script), platform file, `stdout`, and Spike.
- [boot.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md): boot state, picolibc `crt0`, and `boot_rom.S`.
- [atualizacao-de-programa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/atualizacao-de-programa.md): FLASH rewrite over JTAG and boot handoff between tests.
- [simulacao.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/simulacao.md): hardware-top simulation with the real Quartus memories.
- [docs/bugs/PLL_LOCK_LOSS_BUG.md](docs/bugs/PLL_LOCK_LOSS_BUG.md), [docs/bugs/SMALL_DATA_SECTION_BUG.md](docs/bugs/SMALL_DATA_SECTION_BUG.md), and [docs/bugs/DATA_HARVARD_BUG.md](docs/bugs/DATA_HARVARD_BUG.md): hardware bugs investigated and fixed.
- [docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md](docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md): bugs in the per-entity tests CI.

## 2. Prerequisites

Real-hardware tests require a workstation with Quartus, a self-hosted GitHub Actions runner, and the RISC-V and Spike toolchains configured, as described in the [Infra](https://github.com/insper-riscv/Infra) repository. The simulation suite (`sim`) requires only GHDL and the `sim` extra of `riscv-tools`.

## 3. Repository Structure

| Path | Contents |
| :--- | :--- |
| `asm/`, `c/` | The 125 tests of the suite (85 in assembly and 40 in C), one `<name>/src.S` or `<name>/src.c` per folder |
| `tools/riscv_build/` | This project's configuration for `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, `config.sdram-flash.yaml` and its variants for the DE0-CV): it extends the `config.platform-*.yaml` that `riscv-tools platform sync` generates from the [Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V) submodule (toolchain, memory map, runtime, Quartus project, and simulation), and adds only the test paths |
| `tests/python/` | Remaining per-entity tests: raw instruction sequences, all marked `skip`, in `tests.json` ([documentation](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (real hardware), `sim.yml` (simulation), and `sim-fpga.yml` (hardware-top simulation) |
| `docs/` | Investigated bugs in `docs/bugs/`; the memory architecture, boot, runtime, and board programming are in the `docs/` of the `Insper-RISC-V` submodule |

## 4. Quick Start

```bash
riscv-tools --config tools/riscv_build/config.yaml generate-header
riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # real hardware
riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulation
riscv-tools --config tools/riscv_build/config.yaml run                  # real-hardware suite
riscv-tools --config tools/riscv_build/config.yaml sim                  # simulation suite
riscv-tools --root Insper-RISC-V check-memory-map --platform src/simulation/internal-mem/platform.yaml   # memory map (in the platform)
```

The `uart-tx`, `uart-echo` and `uart-errors` programs run in the simulation of the `internal-mem` platform and on the board, because they use the serial pins of the hardware top. In the simulations of the `sdram` and `sdram-flash` platforms, whose tops only have the UART over JTAG, they are left out with `--exclude uart-echo,uart-tx,uart-errors`.

The simulation of the `de0-cv` (`config.sdram-flash.yaml`) runs every program of `c-sdram/` and `asm-sdram/`, `flash-const` and `cpi-kernels` included, which need the FLASH behind the caches. Only `hex-id` is left out, because its pauses are made for a person to look at the display on the board; the `display` program covers the write and the read of the register in the simulation.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](LICENSE).
