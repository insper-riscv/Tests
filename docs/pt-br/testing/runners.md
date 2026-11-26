# Onde cada job do CI roda

🌐 [Português](runners.md) · [English](../../en/testing/runners.md)

Os workflows do repositório usam runners hospedados pelo GitHub e runners próprios do laboratório, conforme o que cada job precisa.

## 1. Jobs por runner

| Workflow | Job | Runner | Contêiner |
| :--- | :--- | :--- | :--- |
| `sim.yml` | `sim-host`, `sim-matrix`, `sim-sdram-flash`, `sim-flash-slow`, `sim-update`, `unit-vhdl`, `regression` | `self-hosted` com o rótulo `sim` | `ghcr.io/insper-riscv/ci-elf:latest` |
| `sim.yml` | `test-sim` (agregador) | `ubuntu-26.04` | nenhum |
| `tree.yml` | `tree` | `ubuntu-26.04` | nenhum |
| `sim-fpga.yml` | todos | como definido no próprio arquivo | como definido no próprio arquivo |
| `real.yml` | testes na placa | `self-hosted` com os rótulos `quartus` e `DE0-CV` | como definido no próprio arquivo |

As simulações com GHDL e cocotb do runner `sim` não precisam de Quartus nem de placa.

## 2. Variável RUNNERS_SIM

A variável de repositório `RUNNERS_SIM` escolhe o runner dos jobs de simulação do `sim.yml`.

| Situação | Runner dos jobs de simulação |
| :--- | :--- |
| `RUNNERS_SIM` indefinida ou com qualquer valor diferente de `hosted` | `self-hosted` com o rótulo `sim` |
| `RUNNERS_SIM` igual a `hosted` | `ubuntu-26.04` |
| Pull request vindo de fork, qualquer valor de `RUNNERS_SIM` | `ubuntu-26.04` |

## 3. Runner da placa

A DE0-CV está ligada só à workstation `D-20003267-LINUX`; o runner dela leva os rótulos `DE0-CV`, `quartus` e o nome da máquina, de modo que `real.yml` seleciona a placa pelo rótulo `DE0-CV`.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
