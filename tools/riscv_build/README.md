# RISC-V test pipeline

Compiles bare-metal test programs (C under `c/`, assembly under
`asm/`) and runs them two ways, and reports PASS/FAIL:

- **real**: every test, on an actual FPGA over JTAG.
- **sim**: every test, in a GHDL/cocotb simulation, no hardware
  needed; a `memory`-kind test's RAM is reconstructed from the bus
  writes the simulation observes and compared against `golden.json`,
  the same check the real suite does from a JTAG dump.

Both are driven by [`riscv-tools`](https://github.com/insper-riscv/Tools),
vendored here as the `tools/Tools` git submodule, not a copy. This
folder only holds what's genuinely project-specific: `config.yaml`
(memory map, Quartus project, toolchain), `config.fpga-sim.yaml` (the
simulation profile of the hardware top), and `sim/test_c_program.py`
(the cocotb testbench that knows the DUT's actual VHDL signal
hierarchy; `riscv-tools` can't know that, only this project can).
Everything else (compiling, JTAG programming/readback, RAM-vs-golden
comparison, running the simulation) is `riscv-tools` code, so it's
fixed/extended in one place ([insper-riscv/Tools](https://github.com/insper-riscv/Tools))
instead of drifting between copies.

This repo is itself vendored as [RV32IM](https://github.com/insper-riscv/RV32IM)'s
own `Tests/` git submodule; RV32IM's root is one level up from this
project's own root (see `quartus.project_dir` in `config.yaml`).

The **real** suite is wired to
[`tests/FPGA/core/quartus/core_fpga_test.qpf`](../../tests/FPGA/core/quartus/core_fpga_test.qpf)
in RV32IM, the only working core + Quartus project on this
workstation.

The **sim** suite is wired to a *different* RV32IM toplevel,
`rv32i3stage_core_sim_test`, not `core_fpga_test`: the real hardware
top instantiates Quartus' `ROM1PORT`/`RAM1PORT` IP (`altsyncram`-based),
which GHDL/cocotb can't inspect the contents of at all.
`rv32i3stage_core_sim_test` instantiates `ROM_simulation`/
`RAM_simulation` instead: plain VHDL processes a testbench can
actually observe. Two things worth knowing if this ever needs
revisiting:
- `sim.test_module` (`sim/test_c_program.py`) watches the RAM *write
  bus* (`ram_addr`/`ram_wren`/`ram_en`/`ram_wdata`, all visible via
  VPI) for a write to the mailbox word, rather than reading
  `RAM_simulation`'s internal `mem` array directly: this GHDL
  install's VPI (mcode backend) doesn't expose array-of-vector
  ("memory") signals as child objects at all, confirmed empirically.
  If a different GHDL backend/version does support it, reading `mem`
  directly would also work; watching the bus doesn't depend on that
  either way.
- `ROM_simulation`/`RAM_simulation` default to a 512-word memory array
  (`memoryAddrWidth := 9`), too small to hold `memory.mailbox_addr`'s
  word offset (4095); `rv32i3stage_core_sim_test` exposes
  `rom_addr_width`/`ram_addr_width` generics (defaulting to the same
  9, so RV32IM's own existing instruction-level cocotb tests are
  unaffected) that `config.yaml`'s `sim.parameters` overrides to 13/12
  to match `memory.rom_words`/`ram_words`.

## Writing a test

Create a folder named for what the test does, under `c/` (for a `.c`
source) or `asm/` (for a `.S` source), containing a `src.c`/`src.S`:

```
c/
└── my-test/
    └── src.c
```

Two optional header comments configure it:

```c
// RV32_EXT: M          // extensions ADDED to the implicit rv32i base.
// RV32_EXT: M,A        // order doesn't matter: both this and "A,M" become rv32ima.
// RV32_TEST_KIND: unit          // default. Checked via the PASS/FAIL mailbox
                                  // alone. Builds for both real hardware and sim.
// RV32_TEST_KIND: memory        // Mailbox + a full RAM dump compared against
                                  // c/my-test/golden.json. Verified on both
                                  // real hardware (JTAG dump) and sim (bus-
                                  // snooped writes).
#include "rv32_test.h"

int main(void) {
    // ... RV32_PASS() or RV32_FAIL() when done ...
}
```

`rv32_test.h` is **generated**, not checked in (see `.gitignore`);
`riscv-tools generate-header` writes it from `config.yaml`'s
`memory.mailbox_addr` (see "Running locally" below). Regenerate it
after changing that address; don't hand-edit the generated file.

A `memory` test needs a matching `golden.json` next to its
`src.c`/`src.S`: byte address (hex string) -> expected value (0-255).
`riscv-tools compile` fails fast if it's missing. See
[`docs/creating-a-c-test.md`](https://github.com/insper-riscv/Tools/blob/main/docs/creating-a-c-test.md)
in `tools/Tools` for the full reference.

## Running locally

Needs `riscv32-unknown-elf-gcc` on `PATH`. `/opt/riscv-foundation/riscv32-elf/`
holds it: a shared, workstation-wide cache (not repo-local: it's meant to
serve every RISC-V project on this machine, not just this one), owned
`runner:runner` with `picow` added to the `runner` group so it's writable
without sudo either way. Matches the `riscv32-elf-ubuntu-22.04-gcc.tar.xz`
build `real.yml` downloads on the runner, since this workstation is Ubuntu
22.04:

```bash
export PATH="/opt/riscv-foundation/riscv32-elf/bin:$PATH"
```

To (re)populate it, or bump it to whatever the latest nightly is (if
your shell's group list doesn't include `runner` yet (`id` to check),
prefix these with `sg runner -c '...'` instead of running directly):

```bash
TAG=$(curl -fsSL https://api.github.com/repos/riscv-collab/riscv-gnu-toolchain/releases/latest \
    | grep -m1 '"tag_name"' | cut -d'"' -f4)
CACHE_DIR=/opt/riscv-foundation/riscv32-elf
rm -rf "$CACHE_DIR" && mkdir -p "$CACHE_DIR"
curl -fsSL -o /tmp/riscv-gcc.tar.xz \
  "https://github.com/riscv-collab/riscv-gnu-toolchain/releases/download/$TAG/riscv32-elf-ubuntu-22.04-gcc.tar.xz"
tar -xf /tmp/riscv-gcc.tar.xz -C "$CACHE_DIR" --strip-components=1
echo "$TAG" > "$CACHE_DIR/.tag"
rm /tmp/riscv-gcc.tar.xz
```

This repo's own Python side is `uv`-managed: a single `pyproject.toml`
at the repo root depends on `riscv-tools` as an **editable path
dependency on the `tools/Tools` submodule** (see `[tool.uv.sources]`),
so `git submodule update --remote tools/Tools` + `uv sync` is the whole
upgrade story, no separate `pip install` step, no version drift between
what this repo's own `tests/python/` unit tests and `riscv-tools`
itself use for cocotb:

```bash
git submodule update --init --recursive
uv sync

# real (needs Quartus + a board on JTAG)
uv run riscv-tools --config tools/riscv_build/config.yaml generate-header  # once, or after changing mailbox_addr
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit mif
uv run riscv-tools --config tools/riscv_build/config.yaml run

# sim (needs GHDL; cocotb/cocotb-tools already come from riscv-tools[sim])
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit hex
uv run riscv-tools --config tools/riscv_build/config.yaml sim

# per-entity VHDL unit tests (ALU, RAM, control_unit, ...): see tests/python/README.md
uv run python tests/python/runner.py          # all of them
uv run python tests/python/runner.py ALU      # just one
```

`compile` iterates every `<name>/src.c`/`<name>/src.S` folder under
`c/`/`asm/` and writes `build/{real,sim}/manifest.json`; both
`--emit mif` and `--emit hex` build every test regardless of kind and
attach a `memory`-kind test's `golden.json` to its manifest entry,
which `run`/`sim` then consume.

## CI

Each suite has its own workflow file:

- [`.github/workflows/sim.yml`](../../.github/workflows/sim.yml):
  runs on any push/PR/branch, on a standard GitHub-hosted runner
  (`ghdl/ghdl:6.0.0-mcode-ubuntu-24.04`, no license needed). Also
  triggerable manually via **Actions → sim tests → Run workflow**.
- [`.github/workflows/real.yml`](../../.github/workflows/real.yml):
  only on `main`, or on demand via **Actions → real tests (FPGA) → Run
  workflow**. Needs a self-hosted runner with Quartus and a board
  attached.

Both check out submodules recursively (`tools/Tools`, plus
`vendor/riscv-arch-test`) and set up `uv` the same way local dev does
above.

## Configuring the real suite on this workstation

This is the actual setup running on this machine: repo
[`insper-riscv/RISC-V-Workstation-Tests`](https://github.com/insper-riscv/RISC-V-Workstation-Tests)
(private), runner named `WS-C621E-SAGE`, runner group `Workstation -
FPGA` (Repository access → Selected repositories → only this repo;
"Allow public repositories" left unchecked; otherwise the public
`RV32IM`, same org, could reach this exact machine through the same
runner).

For the generic, project-agnostic side of this (creating the
`runner` service account from scratch, registering it with GitHub,
the systemd unit, the `/opt/altera_lite`/`/opt/riscv-foundation` cache pattern, the
manual-dispatch secret, and the JTAG USB-autosuspend gotcha), see
[RUNNER_SETUP.md](https://github.com/insper-riscv/Infra/blob/main/RUNNER_SETUP.md)
(repo `insper-riscv/Infra`: org-wide infra docs, not specific to this
project). What's specific to *this* project:

**Hardware/board facts, already reflected in `config.yaml`/`rv32im-fpga.specs`
(and in the generated `rv32_test.h`, see "Writing a test" above):**
- JTAG: `jtagconfig` reports hardware `USB-Blaster [1-4]`, device
  `@1: 5CE(BA4|FA4) (0x02B050DD)`.
- `core_fpga_test`'s ROM1PORT/RAM1PORT IPs both have
  `ENABLE_RUNTIME_MOD=YES` (`INSTANCE_NAME=ROM` / `RAM`), so the
  In-System Memory Content Editor can read/write them over JTAG; this
  is what `riscv-tools`' `mailbox`/`ram_dump`/`rom_writer` modules rely
  on (via their own bundled `.tcl` scripts, not anything project-local
  anymore).
- Memory map: `rv32im_pipeline_core` is Harvard (separate `rom_addr` /
  `ram_addr` buses, each its own space starting at `0x0`); ROM is
  8192 words (32K), RAM is 4096 words (16K), mailbox at the last RAM
  word (`0x00003FFC`).
- **Runner OS is Ubuntu 22.04** (glibc 2.35, confirmed via
  `lsb_release -a`/`ldd --version`); `real.yml` downloads the
  `riscv32-elf-ubuntu-22.04-gcc.tar.xz` riscv-collab asset specifically
  for this reason; the `ubuntu-24.04` one (needs glibc ≥2.38) fails to
  even start with a `GLIBC_2.38 not found` error. If this runner is
  ever reinstalled on a newer Ubuntu, update that asset name in
  `real.yml` and clear `/opt/riscv-foundation/riscv32-elf` (the
  toolchain cache won't redetect the mismatch on its own, since it
  only compares release tags, not compatibility).

## Design decisions worth knowing

- **Toolchain**: both `sim.yml` and `real.yml` always track the latest
  nightly from
  [riscv-collab/riscv-gnu-toolchain](https://github.com/riscv-collab/riscv-gnu-toolchain)
  rather than pinning a tag: the project ships no "stable" release,
  updates often specifically to fix bugs, and this is meant to track
  the same toolchain used in production. `sim.yml` runs on an
  ephemeral GitHub-hosted runner, so it just re-downloads every time.
  `real.yml` runs on a persistent machine: it checks the latest
  release tag via the GitHub API every run (a ~1KB request) and caches
  the ~200MB toolchain at `/opt/riscv-foundation/riscv32-elf`,
  only re-downloading when that tag actually changed; always current,
  without paying the download cost on every push when nothing changed
  upstream.
- **Recompile-per-test**: `riscv-tools run` overwrites the ROM IP's
  `.mif` and runs a full `quartus_sh --flow compile` once up front,
  then JTAG-reloads each test's ROM content live on that same
  bitstream (falling back to a full recompile+reprogram only if a
  test's mailbox never responds; see `riscv-tools`'
  [orchestrator](../Tools/docs/modules/orchestrator.md)/[docs/configuration.md](../Tools/docs/configuration.md)). Simpler than recompiling
  per test, and the fallback still exists for when the board itself
  wedges.
- **JTAG readback**: reading the PASS/FAIL mailbox and dumping RAM
  both go through Quartus' In-System Memory Content Editor
  (`quartus_stp` + the `insystem_memory_edit` package): this is the
  only realistic channel for getting data off real silicon without
  UART or LEDs, so both `RV32_TEST_KIND` variants use it after
  programming the board.
- **GHDL container tag**: `ghdl/ghdl:6.0.0-mcode-ubuntu-24.04`; the
  Docker Hub scheme moved from loose tags like `ubuntu22-mcode` to
  versioned ones (`<ghdl-version>-<backend>-ubuntu-<version>`); this
  is the current stable GHDL release on an LTS base.

## `<<< ADJUST` checklist

Both `real` and `sim` are fully wired up now (verified end to end:
`real` runs in CI on this workstation; `sim` was verified locally
against RV32IM's `rv32i3stage_core_sim_test` with both a PASS and a
FAIL test, see above). What's still open:

| File | What |
|---|---|
| `.github/workflows/real.yml` | `riscv32-elf-ubuntu-22.04-gcc.tar.xz` asset name and `/opt/altera_lite/...` PATH, if this ever moves to a different/newer runner OS or a different Quartus install location |

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../LICENSE).
