# Testes

🌐 [Português](README.md) · [English](README.en.md)

Suíte de testes de hardware real e de simulação do núcleo RISC-V `RV32IM`, dirigida pelo pacote [`riscv-tools`](tools/Tools/README.md), vendorado como submódulo em `tools/Tools`.

## 1. Documentação

- [AUTHORS.md](AUTHORS.md): autores com commits no repositório, com nome e conta do GitHub.
- [CONTRIBUTING.md](CONTRIBUTING.md): regras obrigatórias de contribuição, normas C23 e Python, regras de documentação e verificação antes de abrir Pull Request.
- [CITATION.cff](CITATION.cff): metadados para citação do software.
- [tools/Tools/README.md](tools/Tools/README.md): referência completa de cada subcomando do `riscv-tools`.
- [tools/riscv_build/README.md](tools/riscv_build/README.md): convenções específicas desta suíte (formato dos testes, fatos de hardware e como escrever um teste novo).
- [docs/HARDWARE_PROGRAMMING.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/HARDWARE_PROGRAMMING.md): regra permanente sobre compilar e programar a placa, e o bug de JTAG já diagnosticado.
- [docs/MEMORY_ARCHITECTURE.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/MEMORY_ARCHITECTURE.md): arquitetura BOOT_ROM, FLASH e RAM.
- [docs/RUNTIME.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/RUNTIME.md): runtime C (picolibc, `crt0` e linker script da toolchain), arquivo de plataforma, `stdout` e Spike.
- [docs/CRT0_BOOT_REFERENCE.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/CRT0_BOOT_REFERENCE.md): estado de boot, `crt0` da picolibc e `boot_rom.S`.
- [docs/PROGRAM_UPDATE_HANDOFF.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/PROGRAM_UPDATE_HANDOFF.md): reescrita da FLASH por JTAG e passagem de boot entre testes.
- [docs/SIMULACAO_TOPO_FPGA.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/SIMULACAO_TOPO_FPGA.md): simulação do topo de hardware com as memórias reais do Quartus.
- [docs/bugs/PLL_LOCK_LOSS_BUG.md](docs/bugs/PLL_LOCK_LOSS_BUG.md), [docs/bugs/SMALL_DATA_SECTION_BUG.md](docs/bugs/SMALL_DATA_SECTION_BUG.md) e [docs/bugs/DATA_HARVARD_BUG.md](docs/bugs/DATA_HARVARD_BUG.md): bugs de hardware investigados e corrigidos.
- [docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md](docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md): bugs no CI dos testes por entidade.

## 2. Pré-requisitos

Os testes de hardware real exigem uma workstation com Quartus, um runner self-hosted do GitHub Actions e as toolchains RISC-V e Spike configuradas, conforme o repositório [Infra](https://github.com/insper-riscv/Infra). A suíte de simulação (`sim`) exige apenas o GHDL e a extra `sim` do `riscv-tools`.

## 3. Estrutura do Repositório

| Caminho | Conteúdo |
| :--- | :--- |
| `asm/`, `c/` | Os 92 testes da suíte (52 em assembly, 40 em C), um `<name>/src.S` ou `<name>/src.c` por pasta |
| `c-sdram/` | Programas que usam a SDRAM da plataforma `sdram`, um `<name>/src.c` por pasta; cada um confere a si mesmo e sinaliza PASS ou FAIL |
| `tools/riscv_build/` | Configuração deste projeto para o `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, e `config.sdram.yaml` e `config.sdram-regression.yaml` para a plataforma com SDRAM): estende a da plataforma, que está no [TopLevel](https://github.com/insper-riscv/TopLevel) (toolchain, mapa de memória, runtime, projeto do Quartus, simulação), e acrescenta só os caminhos dos testes |
| `tools/Tools/` | Submódulo do pacote [`riscv-tools`](tools/Tools/README.md) |
| `tests/host/` | Testes unitários do harness de UART física, sem precisar da placa |
| `tests/python/` | Testes por entidade remanescentes: sequências de instrução cruas, todas com `skip`, no `tests.json` ([documentação](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (hardware real), `sim.yml` (simulação) e `sim-fpga.yml` (simulação do topo de hardware) |
| `docs/` | Bugs investigados em `docs/bugs/`; a arquitetura de memória, o boot, o runtime e a programação da placa estão em `docs/` do TopLevel |

## 4. Uso Rápido

```bash
uv run riscv-tools --config tools/riscv_build/config.yaml generate-header
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # hardware real
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulação
uv run python -m unittest discover -s tests/host -v                            # harness UART, sem placa
uv run riscv-tools --config tools/riscv_build/config.yaml sim --exclude uart-echo,uart-tx,uart-errors

# hardware real: uma síntese, UART física dedicada, depois mailbox/JTAG
uv run riscv-tools --config tools/riscv_build/config.yaml program build/real/uart-echo.mif
uv run python tools/riscv_build/uart_hardware_test.py --skip-program
uv run riscv-tools --config tools/riscv_build/config.yaml run \
  --exclude uart-echo,uart-tx,uart-errors --skip-reconfigure
uv run riscv-tools --root ../TopLevel check-memory-map --platform platforms/internal-mem/platform.yaml   # mapa de memória (no TopLevel)
```

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](LICENSE).
