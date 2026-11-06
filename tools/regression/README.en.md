# Full regression and cross-check against Spike

🌐 [Português](README.md) · [English](README.en.md)

## 1. Usage

`make regression` runs, from the repository root and inside the `ghcr.io/insper-riscv/ci-elf` image, every program of `asm/`, `c/`, `asm-sdram/` and `c-sdram/` on each platform of the plan, checks the result on Spike and writes a single report. The `make regression-no-spike` target skips Spike.

| Output | Content |
| :--- | :--- |
| `build/regression/report.md` | Markdown report with the stages, the exclusions and the cross-check |
| `build/regression/report.json` | Verdict of each program in each stage |
| `build/regression/*.log` | Full output of each command |

The command exits with an error when a program fails, when a stage does not prepare, or when Spike differs from the simulation on a program without a classification.

## 2. Plan

The `plan.json` file defines everything the regression runs.

| Field | Meaning |
| :--- | :--- |
| `stages` | Platforms and configurations, in order; the first one is the reference of the cross-check |
| `exclusions` | Program excluded from a stage, with the written justification; the plan is rejected without one |
| `spike.divergences` | Programs where Spike and the simulation give different verdicts, with the reason for the difference |

## 3. Cross-check

Spike runs each program of the reference stage up to the `tohost` verdict. A difference between the Spike verdict and the simulation verdict must be listed in `spike.divergences`. An entry that no longer diverges shows in the report as a stale classification, so the plan does not collect dead entries.

---

Copyright 2026 Insper. Licensed under the [Apache License, Version 2.0](../../LICENSE).
