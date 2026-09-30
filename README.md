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
| `rv32im-fpga.specs` | A descrição da plataforma (mapa de memória e `crt0`) para o GCC; o linker script e o `crt0` são os da toolchain, ver [docs/RUNTIME.md](docs/RUNTIME.md) |
| `platform/` | A parte do runtime que é deste hardware: a BOOT_ROM (`boot_rom.S`, `boot_rom.ld`), `_exit.c`, `stdio.c` e `spike_exit.S` |
| `tools/riscv_build/` | Configuração deste projeto pro `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`) |
| `tools/Tools/` | Submódulo do pacote [`riscv-tools`](tools/Tools/README.md) |
| `tests/python/` | Testes de simulação por entidade VHDL (cocotb + GHDL), separados da suíte `asm`/`c` acima; ver [tests/python/README.md](tests/python/README.md) |
| `.github/workflows/` | `real.yml` (hardware real), `sim.yml` (simulação), `sim-fpga.yml` (simulação do topo de hardware) |
| `docs/` | Arquitetura de memória, bugs investigados, referência de boot, guia de programação da placa |

## Uso rápido

```bash
uv run riscv-tools --config tools/riscv_build/config.yaml generate-header
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # hardware real
uv run riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulação
uv run riscv-tools --config tools/riscv_build/config.yaml run                  # suíte de hardware real
uv run riscv-tools --config tools/riscv_build/config.yaml sim                  # suíte de simulação
```

Ver [tools/Tools/README.md](tools/Tools/README.md) pra referência
completa de cada subcomando e [tools/riscv_build/README.md](tools/riscv_build/README.md)
pras convenções específicas deste projeto (formato dos testes, fatos
de hardware, como escrever um teste novo).

## Docs

- [docs/HARDWARE_PROGRAMMING.md](docs/HARDWARE_PROGRAMMING.md): regra permanente
  sobre compilar+programar a placa, e o bug de JTAG já diagnosticado
- [docs/MEMORY_ARCHITECTURE.md](docs/MEMORY_ARCHITECTURE.md): arquitetura BOOT_ROM + FLASH + RAM
- [docs/RUNTIME.md](docs/RUNTIME.md): o runtime C (picolibc, `crt0` e linker script da toolchain), o arquivo de plataforma, o `stdout` e o Spike
- [docs/CRT0_BOOT_REFERENCE.md](docs/CRT0_BOOT_REFERENCE.md): estado de boot, `crt0` da picolibc + `boot_rom.S`
- [docs/PROGRAM_UPDATE_HANDOFF.md](docs/PROGRAM_UPDATE_HANDOFF.md): rewrite de FLASH por JTAG e handoff de boot entre testes
- [docs/SIMULACAO_TOPO_FPGA.md](docs/SIMULACAO_TOPO_FPGA.md): simulação do topo de hardware com as memórias reais do Quartus
- [docs/bugs/PLL_LOCK_LOSS_BUG.md](docs/bugs/PLL_LOCK_LOSS_BUG.md), [docs/bugs/SMALL_DATA_SECTION_BUG.md](docs/bugs/SMALL_DATA_SECTION_BUG.md), [docs/bugs/DATA_HARVARD_BUG.md](docs/bugs/DATA_HARVARD_BUG.md): bugs de hardware já investigados e corrigidos
- [docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md](docs/bugs/PER_ENTITY_TESTS_CI_BREAKAGE.md): bugs no CI dos testes por entidade

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](LICENSE).
