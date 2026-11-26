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
- [memoria.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md): BOOT_MEMORY, FLASH, and RAM architecture.
- [runtime.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/runtime.md): C runtime (picolibc, `crt0`, and toolchain linker script), platform file, `stdout`, and Spike.
- [boot.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md): boot state, picolibc `crt0`, and `boot_memory.S`.
- [atualizacao-de-programa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/atualizacao-de-programa.md): FLASH rewrite over JTAG and boot handoff between tests.
- [simulacao.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/simulacao.md): hardware-top simulation with the real Quartus memories.
- [stepper-motor.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/en/platform/stepper-motor.md): JP2 GPIO, pinout, and test program for the 28BYJ-48 motor with ULN2003.
- [docs/en/bugs/pll-lock-loss.md](docs/en/bugs/pll-lock-loss.md), [docs/en/bugs/small-data-section.md](docs/en/bugs/small-data-section.md), and [docs/en/bugs/data-harvard.md](docs/en/bugs/data-harvard.md): PLL lock loss, small data and the `gp` register, and `.data` initialization on the Harvard core.
- [docs/en/bugs/per-entity-tests-ci-breakage.md](docs/en/bugs/per-entity-tests-ci-breakage.md): the per-entity test suite and the `instructions` group with `skip`.

## 2. Prerequisites

Real-hardware tests require a workstation with Quartus, a self-hosted GitHub Actions runner, and the RISC-V and Spike toolchains configured, as described in the [Infra](https://github.com/insper-riscv/Infra) repository. The simulation suite (`sim`) requires only GHDL and the `sim` extra of `riscv-tools`.

## 3. Repository Structure

| Path | Contents |
| :--- | :--- |
| `asm/`, `c/` | The 132 common tests of the suite (91 in assembly and 41 in C), one `<name>/src.S` or `<name>/src.c` per folder |
| `c-stepper/` | The `stepper-motor` and `stepper-uart` programs, exclusive to the `internal-mem` motor GPIO and run with `config.stepper-motor.yaml` |
| `asm-hpm-reduced/` | The `hpm-reduced` program, which expects the core with 4 event counters of 40 bits and so lives apart from `asm/` (which runs on the full profile); it runs with `config.hpm-reduced.yaml` |
| `c-sdram-update/` | The FLASH program update programs, `flash-update` (complete update of bank B) and `flash-update-interrupted` (the header of bank B is not written, the system returns to bank A); each receives the image of `uart_input.txt` over the UART and runs with `config.sdram-flash-update.yaml` (fast chip) and `config.sdram-flash-update-slow.yaml` (slow chip) |
| `c-sdram-bank-b/` | The bank B program that the update tests send (`update-new`), linked from bank B and packed as version 2; `make flash-update-images` rebuilds it and writes the `uart_input.txt` of the tests |
| `c-boot/`, `cxx-boot/`, `c-boot-seq/` | The boot programs per profile: the state the BOOT leaves at `main` and the wait between tests (`c-boot/`), C++ on top of the `crt0` (`cxx-boot/`) and the programs that pass, fail and hang on purpose, for the board sequence (`c-boot-seq/`); they run in simulation, on the DE0-CV and on the DE2-115 ([boot-per-profile.md](docs/en/testing/boot-per-profile.md)) |
| `tools/flash_update/` | `make_images.sh`, which builds the image of bank B and checks it against the `uart_input.txt` with `--check` |
| `tools/riscv_build/` | This project's configuration for `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, `config.sdram-flash.yaml` and its variants for the DE0-CV, `config.hpm-reduced.yaml` and `config.hpm-reduced-asm.yaml` for the core of 4 counters of 40 bits): it extends the `config.platform-*.yaml` that `riscv-tools platform sync` generates from the [Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V) submodule (toolchain, memory map, runtime, Quartus project, and simulation), and adds only the test paths |
| `tools/board/` | `board_run.py`, which runs the programs on the DE0-CV, DE0-Nano, DE10-Lite and DE2-115 boards, and `board_update.py`, which drives the FLASH update programs ([board-runner.md](docs/en/testing/board-runner.md)) |
| `tools/boot/` | `boot_profile.py`, which runs the boot programs of each profile in the simulation of the modular platform and checks the size, the wait point and the `fence.i` of the BOOT that was built ([boot-per-profile.md](docs/en/testing/boot-per-profile.md)) |
| `tools/headers/` | `check_platform_headers.py`, which checks each `include-<board>/rv32_platform.h` against the `platform.yaml` of the platform (`make check-headers`) |
| `tests/host/` | Unit tests of the physical UART harness, the board runner, the regression and the header check, with no board needed |
| `tests/python/` | Remaining per-entity tests in `tests.json`: raw instruction sequences, all marked `skip`, and `irq-programs`, which runs the interrupt programs on the core ([documentation](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (real hardware), `sim.yml` (simulation), and `sim-fpga.yml` (hardware-top simulation) |
| `docs/` | Hardware and test references in `docs/en/bugs/` and `docs/en/testing/`; the memory architecture, boot, runtime, and board programming are in the `docs/` of the `Insper-RISC-V` submodule |

## 4. Quick Start

```bash
riscv-tools --config tools/riscv_build/config.yaml generate-header
riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # real hardware
riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulation
uv run python -m unittest discover -s tests/host -v                     # UART harness, no board
riscv-tools --config tools/riscv_build/config.yaml sim                  # includes all four UART tests

# real hardware: one synthesis, dedicated physical UART, then tohost/JTAG
riscv-tools --config tools/riscv_build/config.yaml program build/real/uart-echo.mif
uv run python tools/riscv_build/uart_hardware_test.py --skip-program
riscv-tools --config tools/riscv_build/config.yaml run \
  --exclude uart-echo,uart-tx,uart-errors,uart-stdio --skip-reconfigure
riscv-tools --root Insper-RISC-V check-memory-map --platform src/simulation/internal-mem/platform.yaml   # memory map (in the platform)
```

The `uart-tx`, `uart-echo`, `uart-errors`, and `uart-stdio` programs run in the simulation of the `internal-mem` platform and on the board because they use the serial pins of the hardware top. `uart-stdio` integrates picolibc `printf` and `scanf` with the TX/RX queues. In the simulations of the `sdram` and `sdram-flash` platforms, whose tops only have the UART over JTAG, they are left out with `--exclude uart-echo,uart-tx,uart-errors,uart-stdio`.

The `stepper-motor` and `stepper-uart` programs use the four-bit GPIO of the `internal-mem` platform and run with `config.stepper-motor.yaml`. The former automatically performs 1024 half steps in each direction. The latter waits for `d` or `e` through `scanf`, performs 1024 half steps in the selected direction, and replies over the UART through `printf`. Simulation injects a command into RX and checks both the TX response and the pin sequence. The JP2-to-ULN2003 wiring is in the motor documentation linked above.

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
riscv-tools --config tools/riscv_build/config.variation.yaml sim --exclude uart-echo,uart-tx,uart-errors,uart-stdio
```


The programs in `asm-plic/` prove the external interrupts end to end on the `modular` platform, which has the timer, the two GPIOs, the two UARTs and the PLIC. For each source the program enables the cause in the peripheral and in the PLIC, raises the event, enters the handler, claims, services, completes and checks the logged id and status: `plic-timer` (overflow), `plic-switches` and `plic-keys` (rising edge, falling edge, high level and low level, and a pin outside the mask), `plic-uart` (transmitter empty, a received byte with TX wired to RX by the testbench, error) and `plic-uart-jtag` (the same three, with the JTAG host simulated by the testbench and commanded by the program through `fromhost`). `plic-priority` checks priority, the lower-id tie-break, the threshold and priority zero with two sources. The simulated `de0-cv` platform has the FLASH chip, and `asm-plic-de0-cv/plic-flash` checks the completion and the refusal of a command and the transmitter-empty cause of the JTAG UART.

The commands are the ones in the Portuguese README. The map of the programs is in `include-plic/` (modular) and `include-plic-de0-cv/`, and the shared macros in `include-plic/rv32_plic_macros.h`. With `SIM_TRACE_IRQ=1` before the simulation command the testbench logs every change of the PLIC lines and of `MEIP`.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](LICENSE).
