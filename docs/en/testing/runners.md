# Where each CI job runs

🌐 [Português](../../pt-br/testing/runners.md) · [English](runners.md)

The repository workflows use GitHub-hosted runners and the lab's own runners, depending on what each job needs.

## 1. Jobs per runner

| Workflow | Job | Runner | Container |
| :--- | :--- | :--- | :--- |
| `sim.yml` | `sim-host`, `sim-matrix`, `sim-sdram-flash`, `sim-flash-slow`, `sim-update`, `unit-vhdl`, `regression` | `self-hosted` with the `sim` label | `ghcr.io/insper-riscv/ci-elf:latest` |
| `sim.yml` | `test-sim` (aggregator) | `ubuntu-26.04` | none |
| `tree.yml` | `tree` | `ubuntu-26.04` | none |
| `sim-fpga.yml` | all | as defined in the file itself | as defined in the file itself |
| `real.yml` | board tests | `self-hosted` with the `quartus` and `DE0-CV` labels | as defined in the file itself |

The GHDL and cocotb simulations on the `sim` runner need neither Quartus nor a board.

## 2. RUNNERS_SIM variable

The `RUNNERS_SIM` repository variable selects the runner of the `sim.yml` simulation jobs.

| Situation | Runner of the simulation jobs |
| :--- | :--- |
| `RUNNERS_SIM` unset or set to any value other than `hosted` | `self-hosted` with the `sim` label |
| `RUNNERS_SIM` equal to `hosted` | `ubuntu-26.04` |
| Pull request from a fork, any `RUNNERS_SIM` value | `ubuntu-26.04` |

## 3. Board runner

The DE0-CV is attached only to the `D-20003267-LINUX` workstation; its runner carries the `DE0-CV`, `quartus` and machine-name labels, so `real.yml` selects the board by the `DE0-CV` label.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../../LICENSE).
