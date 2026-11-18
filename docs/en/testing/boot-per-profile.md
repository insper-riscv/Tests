# Boot programs per profile

🌐 [Português](../../pt-br/testing/boot-per-profile.md) · [English](boot-per-profile.md)

The programs in `c-boot/`, `cxx-boot/` and `c-boot-seq/` prove that the BOOT_MEMORY of each core profile takes the program to `main` in a known state and accepts the next test. Each program is written once and runs, unedited, in simulation, on the DE0-CV and on the DE2-115. The config handed to `riscv-tools` chooses the environment.

## 1. Programs

| Program | Folder | What it proves |
| :--- | :--- | :--- |
| `boot-state` | `c-boot/` | The state at `main`, for each extension the `-march` of the profile carries: `tohost`, `fromhost` and the standard output header cleared, `mstatus`, `mie` and `mscratch` cleared with `mtvec` at the safe handler (Zicsr), counters running and not inhibited (Zicntr), and the 29 selectors, counters and high halves cleared (Zihpm) |
| `boot-wait-restart` | `c-boot/` | The wait between tests: the program dirties the words the BOOT clears, leaves a mark in RAM, rings the bell (`fromhost`) and jumps to `rv32_wait_restart`, at `0x100`. The BOOT restarts, the program enters again, finds the mark and the cleared words, and finishes |
| `boot-cxx` | `cxx-boot/` | C++ on top of the `crt0`: global constructors and local statics, a class with a virtual function, `new` and `delete`, `std::array`, `std::vector` and a `throw` caught with the destruction of the local objects |
| `boot-seq-pass`, `boot-seq-fail`, `boot-seq-hang` | `c-boot-seq/` | The three possible endings of a test (passed, failed with a code and hung), for the sequence of section 4 |

`boot-state` and `boot-wait-restart` run on every profile, including `rv32i` and `rv32e`, which have no CSRs, because the code of each extension is compiled only when the compiler defines its macro (`__riscv_zicsr`, `__riscv_zicntr`, `__riscv_zihpm`). `boot-cxx` needs the `libstdc++` of the variant of the profile, and the script skips the phase when the toolchain lacks it.

## 2. Environments

| Environment | How it runs | Config |
| :--- | :--- | :--- |
| Simulated | `tools/boot/boot_profile.py`, in the `ci-elf` or `dev_tools` image, with GHDL | Generated per profile over `Insper-RISC-V/src/simulation/modular` |
| DE0-CV | `tools/board/board_run.py run --stages boot boot-seq`, under the board lock | `config.boot-de0-cv.yaml` and `config.boot-seq-de0-cv.yaml` |
| DE2-115 | `tools/board/board_run.py run --stages boot-de2-115 boot-seq-de2-115`, under the board lock | `config.boot-de2-115.yaml` and `config.boot-seq-de2-115.yaml` |

On the board the core is the one in the programmed bitstream, and `boot-state` checks the extensions of that core. A new profile on the board needs a new bitstream.

## 3. Simulation per profile

`boot_profile.py` copies the repository to `build-boot/<combination>/` and, in that copy, adjusts the `platform.yaml` of the modular platform (leaving the submodule untouched), regenerates the map, the `boot_config.h` and the `config_pkg` for the profile, builds the BOOT and runs the programs.

| Combination | Profile | Memories |
| :--- | :--- | :--- |
| `rv32i` | `rv32i` | ROM and RAM inside the core |
| `rv32e` | `rv32e` | ROM and RAM inside the core |
| `rv32im_zicsr` | `rv32im_zicsr` | ROM and RAM inside the core |
| `caches` | `rv32im_zicclsm_zicntr_zihpm_zifencei` | ROM and RAM behind the 4 kbit caches |

Without Zicsr the core refuses interrupts, which the platform lists as an extra. For that reason the copy of the profiles without Zicsr also loses the `interrupts` block and the CLINT and PLIC windows.

After the simulation the script reads the ELF of the BOOT that was built and checks four items.

| Check | Rule |
| :--- | :--- |
| Size | The image fits the 4 KiB of the BOOT_MEMORY |
| Wait point | `rv32_wait_restart` is at `0x100` |
| `fence.i` | It is in the disassembly if and only if the profile has Zifencei |
| CSR | No CSR instruction in a profile without Zicsr |

```bash
docker run --rm -v "$PWD:/w" -w /w ghcr.io/insper-riscv/dev_tools:latest bash -c '
  ulimit -s unlimited
  /opt/riscv-tools/riscv-tools/bin/python -I tools/boot/boot_profile.py'
```

The `--combination` and `--phase` (`c` or `cxx`) options choose what runs, and `--timeout-cycles` sets the limit of each simulation.

## 4. Sequence on the board

The `boot-seq` stage of `board_run.py` runs the programs of `c-boot-seq/` one at a time, each in its own `riscv-tools run`, which writes the image to the FLASH and rings the bell as between two tests of a suite.

| Position | Program | Expected verdict |
| ---: | :--- | :--- |
| 1 to 3 | `boot-seq-pass` | PASS |
| 4 | `boot-seq-fail` | FAIL |
| 5 | `boot-seq-pass` | PASS |
| 6 | `boot-seq-hang` | TIMEOUT |
| 7 and 8 | `boot-seq-pass` | PASS |
| 9 | `boot-seq-fail` | FAIL |
| 10 | `boot-seq-pass` | PASS |

The result of each position is PASS when the verdict equals the expected one, and carries both verdicts when it does not. A test after a FAIL or a hang that does not run fails the stage.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
