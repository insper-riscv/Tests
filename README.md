# RISC-V Workstation Tests

Suíte de testes de hardware real e simulação do núcleo RISC-V
`RV32IM`, dirigida pelo pacote [`riscv-tools`](tools/Tools/README.md)
(vendorado como submódulo em `tools/Tools`).

## Pré-requisitos

Rodar os testes de hardware real exige uma workstation com Quartus,
um runner self-hosted do GitHub Actions e as toolchains RISC-V/Spike
já configuradas: ver [insper-riscv/Infra](https://github.com/insper-riscv/Infra).
Rodar só a suíte de simulação (`sim`) não precisa de nenhuma dessas
peças (só GHDL e a extra `sim` do `riscv-tools`).

## Estrutura

| Caminho | Conteúdo |
| :--- | :--- |
| `asm/`, `c/` | Os 89 testes da suíte (52 em assembly, 37 em C), um `<name>/src.S` ou `<name>/src.c` por pasta |
| `tools/riscv_build/` | Configuração deste projeto pro `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`): estende a da plataforma, que está no [TopLevel](https://github.com/insper-riscv/TopLevel) (toolchain, mapa de memória, runtime, projeto do Quartus, simulação), e acrescenta só os caminhos dos testes |
| `tools/Tools/` | Submódulo do pacote [`riscv-tools`](tools/Tools/README.md) |
| `tests/python/` | O que sobrou dos testes por entidade: sequências de instrução cruas (todas com `skip`, escritas para a arquitetura antiga), no `tests.json`; ver [tests/python/README.md](tests/python/README.md) |
| `.github/workflows/` | `real.yml` (hardware real), `sim.yml` (simulação), `sim-fpga.yml` (simulação do topo de hardware) |
| `docs/` | Bugs investigados (`docs/bugs/`). A arquitetura de memória, o boot, o runtime e a programação da placa estão em `docs/` do TopLevel |

## Uso rápido

```bash
uv run riscv-tools --config tools/riscv_build/config.yaml generate-header
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # hardware real
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulação
uv run riscv-tools --config tools/riscv_build/config.yaml run                  # suíte de hardware real
uv run riscv-tools --config tools/riscv_build/config.yaml sim                  # suíte de simulação
uv run riscv-tools --root ../TopLevel check-memory-map --platform platforms/internal-mem/platform.yaml   # mapa de memória (no TopLevel)
```

Ver [tools/Tools/README.md](tools/Tools/README.md) pra referência
completa de cada subcomando e [tools/riscv_build/README.md](tools/riscv_build/README.md)
pras convenções específicas deste projeto (formato dos testes, fatos
de hardware, como escrever um teste novo).

## Docs

- [docs/HARDWARE_PROGRAMMING.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/HARDWARE_PROGRAMMING.md): regra permanente
  sobre compilar+programar a placa, e o bug de JTAG já diagnosticado
- [docs/MEMORY_ARCHITECTURE.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/MEMORY_ARCHITECTURE.md): arquitetura BOOT_ROM + FLASH + RAM
- [docs/RUNTIME.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/RUNTIME.md): o runtime C (picolibc, `crt0` e linker script da toolchain), o arquivo de plataforma, o `stdout` e o Spike
- [docs/CRT0_BOOT_REFERENCE.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/CRT0_BOOT_REFERENCE.md): estado de boot, `crt0` da picolibc + `boot_rom.S`
- [docs/PROGRAM_UPDATE_HANDOFF.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/PROGRAM_UPDATE_HANDOFF.md): rewrite de FLASH por JTAG e handoff de boot entre testes
- [docs/SIMULACAO_TOPO_FPGA.md](https://github.com/insper-riscv/TopLevel/blob/main/docs/SIMULACAO_TOPO_FPGA.md): simulação do topo de hardware com as memórias reais do Quartus
- [docs/bugs/PLL_LOCK_LOSS_BUG.md](docs/bugs/PLL_LOCK_LOSS_BUG.md), [docs/bugs/SMALL_DATA_SECTION_BUG.md](docs/bugs/SMALL_DATA_SECTION_BUG.md), [docs/bugs/DATA_HARVARD_BUG.md](docs/bugs/DATA_HARVARD_BUG.md): bugs de hardware já investigados e corrigidos
- [docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md](docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md): bugs no CI dos testes por entidade

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](LICENSE).
