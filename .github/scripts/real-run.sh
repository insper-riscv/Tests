#!/usr/bin/env bash
# Runs inside Insper/Infra's ci-elf image (see .github/workflows/real.yml): clones the sibling
# repositories, builds the ROM images and goldens, and runs the suite on the FPGA. Quartus is the
# host's, mounted read-only at $QUARTUS_ROOT, and the board is reached through the host's jtagd.
set -euo pipefail

quartus_root="${QUARTUS_ROOT:-/opt/altera_lite}"
export PATH="$quartus_root/25.1std/quartus/bin:$PATH"

# Core, Memory and TopLevel next to this checkout (main of each, or the branch
# .github/sibling-branches names for a companion pull request).
.github/scripts/clone-siblings.sh

uv sync

# The board runs the SDRAM platform (the SDRAM is the RAM; the host reaches it through the JTAG
# debug port): the existing programs, then the ones that use the SDRAM.
regression=tools/riscv_build/config.sdram-regression.yaml
sdram=tools/riscv_build/config.sdram.yaml

uv run riscv-tools --config "$regression" generate-header
# compile also generates each C memory test's golden under Spike.
uv run riscv-tools --config "$regression" compile --emit mif
uv run riscv-tools --config "$sdram" compile --emit mif

# The first run compiles the Quartus project and programs the board once; the second reuses it.
mkdir -p build/real build-sdram/real
uv run riscv-tools --config "$regression" run | tee build/real/latest.log
uv run riscv-tools --config "$sdram" run --skip-reconfigure | tee build-sdram/real/latest.log
