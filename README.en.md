# Testes

🌐 [Português](README.md) · [English](README.en.md)

Real-hardware and simulation test suite of the `RV32IM` RISC-V core, driven by the `riscv-tools` package, installed in the CI images.

## 1. Documentation

- [AUTHORS.en.md](AUTHORS.en.md): authors with commits in the repository, with name and GitHub account.
- [CONTRIBUTING.en.md](CONTRIBUTING.en.md): mandatory contribution rules, C23 and Python standards, documentation rules, and verification before opening a Pull Request.
- [CITATION.cff](CITATION.cff): software citation metadata.
- [riscv-tools](https://github.com/insper-riscv/Tools/blob/main/README.en.md): complete reference of each `riscv-tools` subcommand.
- [tools/riscv_build/README.md](tools/riscv_build/README.md): conventions specific to this suite (test format, hardware facts, and how to write a new test).
- [placa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/placa.md): permanent rule on compiling and programming the board, and the JTAG bug already diagnosed.
- [memoria.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md): BOOT_ROM, FLASH, and RAM architecture.
- [runtime.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/runtime.md): C runtime (picolibc, `crt0`, and toolchain linker script), platform file, `stdout`, and Spike.
- [boot.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md): boot state, picolibc `crt0`, and `boot_rom.S`.
- [atualizacao-de-programa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/atualizacao-de-programa.md): FLASH rewrite over JTAG and boot handoff between tests.
- [simulacao.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/simulacao.md): hardware-top simulation with the real Quartus memories.
- [docs/en/bugs/pll-lock-loss.md](docs/en/bugs/pll-lock-loss.md), [docs/en/bugs/small-data-section.md](docs/en/bugs/small-data-section.md), and [docs/en/bugs/data-harvard.md](docs/en/bugs/data-harvard.md): hardware bugs investigated and fixed.
- [docs/en/bugs/per-entity-tests-ci-breakage.md](docs/en/bugs/per-entity-tests-ci-breakage.md): bugs in the per-entity tests CI.

## 2. Prerequisites

Real-hardware tests require a workstation with Quartus, a self-hosted GitHub Actions runner, and the RISC-V and Spike toolchains configured, as described in the [Infra](https://github.com/insper-riscv/Infra) repository. The simulation suite (`sim`) requires only GHDL and the `sim` extra of `riscv-tools`.

## 3. Repository Structure

| Path | Contents |
| :--- | :--- |
| `asm/`, `c/` | The 130 tests of the suite (90 in assembly and 40 in C), one `<name>/src.S` or `<name>/src.c` per folder |
| `asm-hpm-reduced/` | The `hpm-reduced` program, which expects the core with 4 event counters of 40 bits and so lives apart from `asm/` (which runs on the full profile); it runs with `config.hpm-reduced.yaml` |
| `c-sdram-update/` | The FLASH program update programs, `flash-update` (complete update of bank B) and `flash-update-interrupted` (the header of bank B is not written, the system returns to bank A); each receives the image of `uart_input.txt` over the UART and runs with `config.sdram-flash-update.yaml` (fast chip) and `config.sdram-flash-update-slow.yaml` (slow chip) |
| `c-sdram-bank-b/` | The bank B program that the update tests send (`update-new`), linked from bank B and packed as version 2; `make flash-update-images` rebuilds it and writes the `uart_input.txt` of the tests |
| `tools/flash_update/` | `make_images.sh`, which builds the image of bank B and checks it against the `uart_input.txt` with `--check` |
| `tools/riscv_build/` | This project's configuration for `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, `config.sdram-flash.yaml` and its variants for the DE0-CV, `config.hpm-reduced.yaml` and `config.hpm-reduced-asm.yaml` for the core of 4 counters of 40 bits): it extends the `config.platform-*.yaml` that `riscv-tools platform sync` generates from the [Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V) submodule (toolchain, memory map, runtime, Quartus project, and simulation), and adds only the test paths |
| `tests/python/` | Remaining per-entity tests: raw instruction sequences, all marked `skip`, in `tests.json` ([documentation](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (real hardware), `sim.yml` (simulation), and `sim-fpga.yml` (hardware-top simulation) |
| `docs/` | Investigated bugs in `docs/en/bugs/`; the memory architecture, boot, runtime, and board programming are in the `docs/` of the `Insper-RISC-V` submodule |

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

The `hpm-*` programs cover the 29 Zihpm indices. In `asm/`, `hpm-full` expects the full profile (29 counters of 64 bits), while `hpm-sweep`, `hpm-fused-high` and `hpm-inhibit-select` adapt to the profile of the core. `hpm-reduced` expects 4 counters of 40 bits and runs in another configuration:

```bash
riscv-tools --config tools/riscv_build/config.hpm-reduced.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.hpm-reduced.yaml sim
riscv-tools --config tools/riscv_build/config.hpm-reduced-asm.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.hpm-reduced-asm.yaml sim --only hpm-sweep,hpm-fused-high,hpm-inhibit-select
```

The simulation of the `de0-cv` (`config.sdram-flash.yaml`) runs every program of `c-sdram/` and `asm-sdram/`, `flash-const` and `cpi-kernels` included, which need the FLASH behind the caches. Only `hex-id` is left out, because its pauses are made for a person to look at the display on the board; the `display` program covers the write and the read of the register in the simulation.

The `modular` platform of the submodule runs the programs of `c/` and `asm/` in any combination of internal or external ROM and RAM with and without instruction and data caches, chosen on the command line, without changing any versioned file. `include-modular/` holds the header with its map. The example runs architecture C (external ROM behind the instruction cache, internal RAM). The `fpga-*` platforms of the submodule's ten platforms use the header of `include-fpga-integrated/`, `include-fpga-external-instruction/` or `include-fpga-sdram/`, according to the RAM map, and the `test-*` ones use the header of `include-modular/`.

```bash
ulimit -s unlimited
riscv-tools --profile rv32im_zicclsm_zicntr_zihpm_zifencei --memory rom=ext,ram=int,icache=on,dcache=off \
  platform sync --prefix Insper-RISC-V --source Insper-RISC-V/src/simulation/modular/config.yaml \
  --output tools/riscv_build/config.generated.yaml
printf 'extends: config.generated.yaml\npaths: {include_dir: tools/riscv_build/include-modular, build_dir: build, c_dir: c, asm_dir: asm}\n' \
  > tools/riscv_build/config.variation.yaml
riscv-tools --config tools/riscv_build/config.variation.yaml generate-header
riscv-tools --config tools/riscv_build/config.variation.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.variation.yaml sim --exclude uart-echo,uart-tx,uart-errors
```


---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](LICENSE).
