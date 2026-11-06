# Testes

🌐 [Português](README.md) · [English](README.en.md)

Suíte de testes de hardware real e de simulação do núcleo RISC-V `RV32IM`, dirigida pelo pacote `riscv-tools`, instalado nas imagens de CI.

## 1. Documentação

- [AUTHORS.md](AUTHORS.md): autores com commits no repositório, com nome e conta do GitHub.
- [CONTRIBUTING.md](CONTRIBUTING.md): regras obrigatórias de contribuição, normas C23 e Python, regras de documentação e verificação antes de abrir Pull Request.
- [CITATION.cff](CITATION.cff): metadados para citação do software.
- [riscv-tools](https://github.com/insper-riscv/Tools/blob/main/README.md): referência completa de cada subcomando do `riscv-tools`.
- [tools/riscv_build/README.md](tools/riscv_build/README.md): convenções específicas desta suíte (formato dos testes, fatos de hardware e como escrever um teste novo).
- [placa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/placa.md): regra permanente sobre compilar e programar a placa, e o bug de JTAG já diagnosticado.
- [memoria.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md): arquitetura BOOT_ROM, FLASH e RAM.
- [runtime.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/runtime.md): runtime C (picolibc, `crt0` e linker script da toolchain), arquivo de plataforma, `stdout` e Spike.
- [boot.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md): estado de boot, `crt0` da picolibc e `boot_rom.S`.
- [atualizacao-de-programa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/atualizacao-de-programa.md): reescrita da FLASH por JTAG e passagem de boot entre testes.
- [simulacao.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/simulacao.md): simulação do topo de hardware com as memórias reais do Quartus.
- [docs/pt-br/bugs/pll-lock-loss.md](docs/pt-br/bugs/pll-lock-loss.md), [docs/pt-br/bugs/small-data-section.md](docs/pt-br/bugs/small-data-section.md) e [docs/pt-br/bugs/data-harvard.md](docs/pt-br/bugs/data-harvard.md): bugs de hardware investigados e corrigidos.
- [docs/pt-br/bugs/per-entity-tests-ci-breakage.md](docs/pt-br/bugs/per-entity-tests-ci-breakage.md): bugs no CI dos testes por entidade.

## 2. Pré-requisitos

Os testes de hardware real exigem uma workstation com Quartus, um runner self-hosted do GitHub Actions e as toolchains RISC-V e Spike configuradas, conforme o repositório [Infra](https://github.com/insper-riscv/Infra). A suíte de simulação (`sim`) exige apenas o GHDL e a extra `sim` do `riscv-tools`.

## 3. Estrutura do Repositório

| Caminho | Conteúdo |
| :--- | :--- |
| `asm/`, `c/` | Os 123 testes da suíte (83 em assembly, 40 em C), um `<name>/src.S` ou `<name>/src.c` por pasta |
| `c-sdram/` | Programas que usam a SDRAM e a FLASH da plataforma `de0-cv`, um `<name>/src.c` por pasta; cada um confere a si mesmo e sinaliza PASS ou FAIL |
| `tools/riscv_build/` | Configuração deste projeto para o `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, `config.sdram-flash.yaml` e as variantes dela para a DE0-CV): estende o `config.platform-*.yaml` da plataforma, que `riscv-tools platform sync` gera a partir do submódulo [Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V) (toolchain, mapa de memória, runtime, projeto do Quartus, simulação), e acrescenta só os caminhos dos testes |
| `tests/host/` | Testes unitários do harness de UART física, sem precisar da placa |
| `tests/python/` | Testes por entidade remanescentes: sequências de instrução cruas, todas com `skip`, no `tests.json` ([documentação](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (hardware real), `sim.yml` (simulação) e `sim-fpga.yml` (simulação do topo de hardware) |
| `docs/` | Bugs investigados em `docs/pt-br/bugs/`; a arquitetura de memória, o boot, o runtime e a programação da placa estão em `docs/` do submódulo `Insper-RISC-V` |

## 4. Uso Rápido

```bash
riscv-tools --config tools/riscv_build/config.yaml generate-header
riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # hardware real
riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulação
uv run python -m unittest discover -s tests/host -v                            # harness UART, sem placa
riscv-tools --config tools/riscv_build/config.yaml sim                  # inclui uart-tx, uart-echo e uart-errors

# hardware real: uma síntese, UART física dedicada, depois tohost/JTAG
riscv-tools --config tools/riscv_build/config.yaml program build/real/uart-echo.mif
uv run python tools/riscv_build/uart_hardware_test.py --skip-program
riscv-tools --config tools/riscv_build/config.yaml run \
  --exclude uart-echo,uart-tx,uart-errors --skip-reconfigure
riscv-tools --root Insper-RISC-V check-memory-map --platform src/simulation/internal-mem/platform.yaml   # mapa de memória (na plataforma)
```

Os programas `uart-tx`, `uart-echo` e `uart-errors` rodam na simulação da plataforma `internal-mem` e na placa, porque usam os pinos seriais do topo de hardware. Nas simulações das plataformas `sdram` e `sdram-flash`, cujos topos só têm a UART sobre JTAG, eles ficam de fora com `--exclude uart-echo,uart-tx,uart-errors`.

A simulação da `de0-cv` (`config.sdram-flash.yaml`) roda todos os programas de `c-sdram/` e `asm-sdram/`, inclusive `flash-const` e `cpi-kernels`, que precisam da FLASH atrás das caches. Só o `hex-id` fica de fora, porque as pausas dele são feitas para uma pessoa olhar o display na placa; o programa `display` cobre a escrita e a leitura do registrador na simulação.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](LICENSE).
