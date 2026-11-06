#!/bin/bash
# Builds the program of bank B (c-sdram-bank-b/update-new) as the image of bank B and writes it to the uart_input.txt
# of the update tests. With --check it only compares the files with what the toolchain builds now.
set -euo pipefail
cd "$(dirname "$0")/../.."
config=tools/riscv_build/config.sdram-flash-bank-b.yaml
riscv-tools --config "$config" generate-header
riscv-tools --config "$config" compile --emit hex
image=build-bank-b/sim/update-new.image.bin
riscv-tools flash-image verify "$image"
for test in flash-update flash-update-interrupted; do
    target=c-sdram-update/$test/uart_input.txt
    if [ "${1:-}" = "--check" ]; then
        cmp "$image" "$target"
    else
        cp "$image" "$target"
    fi
done
