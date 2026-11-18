# Board runner

🌐 [Português](../../pt-br/testing/board-runner.md) · [English](board-runner.md)

`tools/board/board_run.py` runs the programs of Tests on the DE0-CV, DE0-Nano, DE10-Lite and DE2-115 boards and, on the DE0-CV, with the config that Certification generates, the ACT4 ELFs. Each item gets PASS, FAIL or TIMEOUT, and the runner writes one JSON per board.

## 1. Stages

Without `--stages`, the runner runs the DE0-CV stages, because a session locks one board only. Each stage carries its board, and asking for stages of different boards in the same call is an error.

| Stage | Board | Config | Programs |
| :--- | :--- | :--- | :--- |
| `regression` | `de0-cv` | `config.sdram-flash-regression.yaml` | `asm/` and `c/`, minus the three physical UART ones |
| `sdram` | `de0-cv` | `config.sdram-flash.yaml` | `asm-sdram/` and `c-sdram/` |
| `irq` | `de0-cv` | `config.sdram-flash-irq.yaml` | `asm-irq/`, wired to the real CLINT of the platform at `0xE0000000` |
| `de0-nano-regression`, `de0-nano-sdram` | `de0-nano` | `config.nano-regression.yaml`, `config.nano-sdram.yaml` | Those of `asm/`, `c/`, `asm-sdram/` and `c-sdram/` that the RV32I profile with Zicsr and Zifencei supports |
| `de10-lite-regression`, `de10-lite-sdram`, `de10-lite-irq` | `de10-lite` | `config.lite-regression.yaml`, `config.lite-sdram.yaml`, `config.lite-irq.yaml` | The same programs as the DE0-CV, on the full profile |
| `de2-115` | `de2-115` | `config.de2-115.yaml` | The programs of `asm/`, `c/`, `asm-sdram/` and `c-sdram/` with the NOR FLASH and the 32-bit SDRAM |
| `de2-115-n2` | `de2-115` | `config.de2-115-n2.yaml` | The programs that fit the 128 KiB emulated RAM, without those of the 16 MB SDRAM window |
| `boot`, `boot-seq` | `de0-cv` | `config.boot-de0-cv.yaml`, `config.boot-seq-de0-cv.yaml` | The boot programs of `c-boot/` and the ten-test sequence of `c-boot-seq/`, run one at a time ([boot-per-profile.md](boot-per-profile.md)) |
| `boot-de2-115`, `boot-seq-de2-115` | `de2-115` | `config.boot-de2-115.yaml`, `config.boot-seq-de2-115.yaml` | The same as on the DE0-CV, on the DE2-115 |

`uart-echo`, `uart-tx`, `uart-errors` and `uart-stdio` use the UART pins, which the boards do not have wired. `hex-id` pauses for a person to read the display, so the runner gives it a 90 s timeout.

## 2. Subcommands

| Subcommand | Where it runs | What it does |
| :--- | :--- | :--- |
| `prepare` | `ci-elf` image, no board | `generate-header` and `compile --emit mif` of each stage |
| `run` | machine with Quartus and the cable | selects the board through JTAG and takes its lock, programs the `.sof`, runs the stages and writes `results-<host>.json` |
| `act` | machine with Quartus and the cable | selects and locks the board, programs the `.sof` and runs `certify-board --skip-build` on the ELFs already built |

The `RISCV_TOOLS` variable gives the `riscv-tools` command, for example `uv run --project <Tools-clone> riscv-tools`. The runner keeps no IDCODE or lock name. `riscv-tools board lock` finds the board by the IDCODE of the chain and the USB port of the cable (package `Insper-RISC-V/boards/<board>`, with the board of the stage), never by the cable name; the chosen cable goes to `quartus_pgm` and, through `--cable`, to the child `riscv-tools`. See the `board` module of Tools.

## 3. Two boards in parallel

The `--part I/N` option splits the sorted list: board `I` runs the items of index `I` modulo `N`. The host label (`--host`, also accepted as `--board`) is the short hostname of the machine, given by `hostname -s`, or `laptop` on the notebook; it goes into the lock name, such as `de0cv-laptop.lock` or `de0cv-<hostname>.lock`, the same file the other users of the board take. With `--part auto` the runner takes a free board of the host and splits the list among all the boards of the platform that the host has, in the order of their identifiers.

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py run --host laptop --part 0/2 --sof de0-cv.sof
```

## 4. ACT4 ELFs

Certification generates the board target with `gen_profiles.py --profile rv32im_full_zifencei --board --out build/target` and builds the ELFs with `riscv-tools --config build/target/config.yaml certify-board --build-only`, in the `ci-act4` image. The runner then runs:

```bash
python3 tools/board/board_run.py act --host "$(hostname -s)" --part 1/2 --sof de0-cv.sof \
  --config <Certification>/build/target/config.yaml --root <Certification> \
  --target rv32im_full_zifencei__board
```

## 5. Update programs, reduced Zihpm and PLIC

`tools/board/board_update.py` drives the programs that `riscv-tools run` alone does not. It takes the lock of the DE0-CV with `board_session`, from the same `board_run.py`, and exports the cable of the locked board in `RISCV_TOOLS_CABLE` for the `riscv-tools` that opens the JTAG. The `--host` option (also accepted as `--board`) picks the host label, the short hostname of the machine or `laptop` on the notebook, any text without `/` or whitespace, and `--skip-lock` is for whoever already holds the lock and exports the cable.

| Stage | Programs | Driving |
| :--- | :--- | :--- |
| `update` | `flash-update`, `flash-update-interrupted` and `update-new` | Clears the survivor word of the SDRAM and `tohost`, writes the whole FLASH through the JTAG loader, sends the image of bank B over the UART (`flash-image send`) and checks `stdout` against `uart_expected.txt`. `update-new` runs alone, written to bank B |
| `plic` | `plic-flash` | Runs with the UART console attached, which drains the transmit queue |

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  uv run --project ../Tools python tools/board/board_update.py --host laptop
```

`hpm-reduced` (`config.sdram-flash-hpm-reduced.yaml`) needs the reduced bitstream, with 4 counters of 40 bits, and fails on the full one. It is compiled with `quartus_sh -t de0-cv_synth.tcl 1 HPM_COUNTERS=4 HPM_WIDTH=40` and run with `riscv-tools --config tools/riscv_build/config.sdram-flash-hpm-reduced.yaml run --skip-reconfigure --skip-recompile`, after programming the reduced `.sof`.

The FLASH of the board is rewritten through the JTAG loader (`quartus.rom_write_method: jtag_loader`), which restarts the core for every program; the SDRAM debug port is Virtual JTAG instance 3 (`quartus.sdram_debug_instance`).

## 6. Other boards

On the platforms that turn on only part of the extensions, `prepare` compiles only the programs whose `RV32_EXT` header names extensions that the platform has, and the runner never sees the others.

| Platform | Board | Extensions of the programs | Stages |
| :--- | :--- | :--- | :--- |
| `de0-nano` | `de0-nano` | Zicsr and Zifencei (RV32I) | `de0-nano-regression`, `de0-nano-sdram` |
| `de10-lite` | `de10-lite` | Those of the full profile | `de10-lite-regression`, `de10-lite-sdram`, `de10-lite-irq` |
| `de2-115` | `de2-115` | Those of the full profile | `de2-115` |
| `de2-115-n2` | `de2-115` | Those of the full profile, without the programs of the SDRAM window | `de2-115-n2` |

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py prepare --stages de0-nano-regression de0-nano-sdram
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py run --stages de0-nano-regression de0-nano-sdram --host "$(hostname -s)" --sof de0-nano.sof
```

The results of each board are in [board-results.md](board-results.md).

## 7. Platform headers and configs

The file `include-<platform>/rv32_platform.h` of each platform repeats the map of its `platform.yaml`, and `config.platform-<platform>.yaml` comes from `platform sync`. Two commands check both.

| Command | What it checks |
| :--- | :--- |
| `riscv-tools platform sync --prefix Insper-RISC-V --check --source <platform config.yaml> --output tools/riscv_build/config.platform-<platform>.yaml` | The generated config is what the platform produces |
| `make check-headers` | Each address macro (`RV32_RAM_BASE`, `RV32_RAM_END`, `RV32_STDOUT_BASE`, `RV32_TOHOST`, `RV32_FROMHOST`, `RV32_UART_BASE`, `RV32_FLASH_CTRL_BASE`, `RV32_CLINT_BASE`, `RV32_PLIC_BASE` and the FLASH banks) is worth what the `platform.yaml` of the platform defines |

The header check reads the map with the same reader as `riscv-tools check-memory-map` and uses `tools/headers/headers.json` to tie each `include-*` folder to its `platform.yaml`. It also runs in the host tests (`tests/host/test_platform_headers.py`). With `make check-headers ARGS=--write` the divergent values are rewritten in the header.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
