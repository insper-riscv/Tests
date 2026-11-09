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
- [memoria.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md): arquitetura BOOT_MEMORY, FLASH e RAM.
- [runtime.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/runtime.md): runtime C (picolibc, `crt0` e linker script da toolchain), arquivo de plataforma, `stdout` e Spike.
- [boot.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md): estado de boot, `crt0` da picolibc e `boot_memory.S`.
- [atualizacao-de-programa.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/atualizacao-de-programa.md): reescrita da FLASH por JTAG e passagem de boot entre testes.
- [simulacao.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/simulacao.md): simulação do topo de hardware com as memórias reais do Quartus.
- [stepper-motor.md](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/stepper-motor.md): GPIO, pinagem do JP2 e programa de teste do motor 28BYJ-48 com ULN2003.
- [docs/pt-br/bugs/pll-lock-loss.md](docs/pt-br/bugs/pll-lock-loss.md), [docs/pt-br/bugs/small-data-section.md](docs/pt-br/bugs/small-data-section.md) e [docs/pt-br/bugs/data-harvard.md](docs/pt-br/bugs/data-harvard.md): PLL e perda de lock, dados pequenos e o registrador `gp`, e inicialização de `.data` no core Harvard.
- [docs/pt-br/bugs/per-entity-tests-ci-breakage.md](docs/pt-br/bugs/per-entity-tests-ci-breakage.md): a suíte de testes por entidade e o grupo `instructions` com `skip`.

## 2. Pré-requisitos

Os testes de hardware real exigem uma workstation com Quartus, um runner self-hosted do GitHub Actions e as toolchains RISC-V e Spike configuradas, conforme o repositório [Infra](https://github.com/insper-riscv/Infra). A suíte de simulação (`sim`) exige apenas o GHDL e a extra `sim` do `riscv-tools`.

## 3. Estrutura do Repositório

| Caminho | Conteúdo |
| :--- | :--- |
| `asm/`, `c/` | Os 132 testes comuns da suíte (91 em assembly, 41 em C), um `<name>/src.S` ou `<name>/src.c` por pasta |
| `c-stepper/` | Os programas `stepper-motor` e `stepper-uart`, exclusivos do GPIO de motor da plataforma `internal-mem` e executados com `config.stepper-motor.yaml` |
| `asm-hpm-reduced/` | O programa `hpm-reduced`, que espera o núcleo com 4 contadores de eventos de 40 bits e por isso fica fora do `asm/` (que roda no perfil completo); roda com `config.hpm-reduced.yaml` |
| `c-sdram/` | Programas que usam a SDRAM da plataforma `sdram`, um `<name>/src.c` por pasta; cada um confere a si mesmo e sinaliza PASS ou FAIL |
| `c-sdram-update/` | Os programas de atualização de programa da FLASH, `flash-update` (atualização completa da banca B) e `flash-update-interrupted` (cabeçalho da banca B não gravado, volta à banca A); cada um recebe pela UART a imagem de `uart_input.txt` e roda com `config.sdram-flash-update.yaml` (chip rápido) e `config.sdram-flash-update-slow.yaml` (chip lento) |
| `c-sdram-bank-b/` | O programa da banca B que os testes de atualização enviam (`update-new`), ligado a partir da banca B e empacotado como versão 2; `make flash-update-images` o reconstrói e escreve o `uart_input.txt` dos testes |
| `tools/flash_update/` | `make_images.sh`, que constrói a imagem da banca B e a confere contra o `uart_input.txt` com `--check` |
| `tools/riscv_build/` | Configuração deste projeto para o `riscv-tools` (`config.yaml`, `config.fpga-sim.yaml`, `config.sdram-flash.yaml` e as variantes dela para a DE0-CV, `config.hpm-reduced.yaml` e `config.hpm-reduced-asm.yaml` para o núcleo de 4 contadores de 40 bits): estende o `config.platform-*.yaml` da plataforma, que `riscv-tools platform sync` gera a partir do submódulo [Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V) (toolchain, mapa de memória, runtime, projeto do Quartus, simulação), e acrescenta só os caminhos dos testes |
| `tools/board/` | `board_run.py`, que roda os programas nas placas DE0-CV, DE0-Nano, DE10-Lite e DE2-115, e `board_update.py`, que conduz os programas de atualização da FLASH ([board-runner.md](docs/pt-br/testing/board-runner.md)) |
| `tools/headers/` | `check_platform_headers.py`, que confere cada `include-<placa>/rv32_platform.h` com o `platform.yaml` da plataforma (`make check-headers`) |
| `tests/host/` | Testes unitários do harness de UART física, do runner de placa, da regressão e da conferência dos cabeçalhos, sem precisar da placa |
| `tests/python/` | Testes por entidade remanescentes no `tests.json`: sequências de instrução cruas, todas com `skip`, e `irq-programs`, que roda os programas de interrupção no núcleo ([documentação](tests/python/README.md)) |
| `.github/workflows/` | `real.yml` (hardware real), `sim.yml` (simulação) e `sim-fpga.yml` (simulação do topo de hardware) |
| `docs/` | Referências de hardware e de testes em `docs/pt-br/bugs/` e `docs/pt-br/testing/`; a arquitetura de memória, o boot, o runtime e a programação da placa estão em `docs/` do submódulo `Insper-RISC-V` |

## 4. Uso Rápido

```bash
riscv-tools --config tools/riscv_build/config.yaml generate-header
riscv-tools --config tools/riscv_build/config.yaml compile --emit mif   # hardware real
riscv-tools --config tools/riscv_build/config.yaml compile --emit hex   # simulação
uv run python -m unittest discover -s tests/host -v                            # harness UART, sem placa
riscv-tools --config tools/riscv_build/config.yaml sim                  # inclui os quatro testes UART

# hardware real: uma síntese, UART física dedicada, depois tohost/JTAG
riscv-tools --config tools/riscv_build/config.yaml program build/real/uart-echo.mif
uv run python tools/riscv_build/uart_hardware_test.py --skip-program
riscv-tools --config tools/riscv_build/config.yaml run \
  --exclude uart-echo,uart-tx,uart-errors,uart-stdio --skip-reconfigure
riscv-tools --root Insper-RISC-V check-memory-map --platform src/simulation/internal-mem/platform.yaml   # mapa de memória (na plataforma)
```

Os programas `uart-tx`, `uart-echo`, `uart-errors` e `uart-stdio` rodam na simulação da plataforma `internal-mem` e na placa, porque usam os pinos seriais do topo de hardware. `uart-stdio` integra o `printf` e o `scanf` da picolibc com as filas TX/RX. Nas simulações das plataformas `sdram` e `sdram-flash`, cujos topos só têm a UART sobre JTAG, eles ficam de fora com `--exclude uart-echo,uart-tx,uart-errors,uart-stdio`.

Os programas `stepper-motor` e `stepper-uart` usam o GPIO de quatro bits da plataforma `internal-mem` e rodam com `config.stepper-motor.yaml`. O primeiro executa automaticamente 1024 meios passos em cada sentido. O segundo espera `d` ou `e` com `scanf`, executa 1024 meios passos no sentido escolhido e responde pela UART com `printf`. A simulação injeta um comando no RX e confere a resposta no TX e a sequência nos pinos. A ligação do JP2 ao ULN2003 está na documentação do motor acima.

Os programas `hpm-*` cobrem os 29 índices do Zihpm. Em `asm/`, `hpm-full` espera o perfil completo (29 contadores de 64 bits), e `hpm-sweep`, `hpm-fused-high` e `hpm-inhibit-select` se ajustam ao perfil do núcleo. O `hpm-reduced` espera 4 contadores de 40 bits e roda em outra configuração:

```bash
riscv-tools --config tools/riscv_build/config.hpm-reduced.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.hpm-reduced.yaml sim
riscv-tools --config tools/riscv_build/config.hpm-reduced-asm.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.hpm-reduced-asm.yaml sim --only hpm-sweep,hpm-fused-high,hpm-inhibit-select
```

A simulação da `de0-cv` (`config.sdram-flash.yaml`) roda todos os programas de `c-sdram/` e `asm-sdram/`, inclusive `flash-const` e `cpi-kernels`, que precisam da FLASH atrás das caches. Só o `hex-id` fica de fora, porque as pausas dele são feitas para uma pessoa olhar o display na placa; o programa `display` cobre a escrita e a leitura do registrador na simulação.

A plataforma `modular` do submódulo roda os programas de `c/` e `asm/` em qualquer combinação de ROM e RAM internas ou externas com e sem cache de instruções e de dados, escolhida na linha de comando, sem alterar arquivo versionado. O `include-modular/` guarda o cabeçalho com o mapa dela. O exemplo roda a arquitetura C (ROM externa atrás da cache de instruções, RAM interna). As plataformas `fpga-*` das dez plataformas do submódulo usam o cabeçalho de `include-fpga-integrated/`, `include-fpga-external-instruction/` ou `include-fpga-sdram/`, conforme o mapa da RAM, e as `test-*` usam o de `include-modular/`.

```bash
ulimit -s unlimited
riscv-tools --profile rv32im_zicclsm_zicntr_zihpm_zifencei --memory rom=ext,ram=int,icache=on,dcache=off \
  platform sync --prefix Insper-RISC-V --source Insper-RISC-V/src/simulation/modular/config.yaml \
  --output tools/riscv_build/config.generated.yaml
printf 'extends: config.generated.yaml\npaths: {include_dir: tools/riscv_build/include-modular, build_dir: build, c_dir: c, asm_dir: asm}\n' \
  > tools/riscv_build/config.variation.yaml
riscv-tools --config tools/riscv_build/config.variation.yaml generate-header
riscv-tools --config tools/riscv_build/config.variation.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.variation.yaml sim --exclude uart-echo,uart-tx,uart-errors,uart-stdio
```


Os programas de `asm-plic/` provam as interrupções externas de ponta a ponta na plataforma `modular`, que tem o temporizador, as duas GPIO, as duas UART e o PLIC. Para cada fonte, o programa habilita a causa no periférico e no PLIC, provoca o evento, entra no handler, faz claim, trata, completa e confere o identificador e o status registrados: `plic-timer` (estouro), `plic-switches` e `plic-keys` (borda de subida, de descida, nível alto e nível baixo, e um pino fora da máscara), `plic-uart` (transmissor vazio, byte recebido com o TX ligado ao RX pelo testbench, erro) e `plic-uart-jtag` (os mesmos três, com o hospedeiro JTAG simulado pelo testbench e comandado pelo programa por `fromhost`). `plic-priority` confere com duas fontes a prioridade, o desempate pelo menor identificador, o limiar e a prioridade zero. A plataforma `de0-cv` simulada tem o chip de FLASH, e `asm-plic-de0-cv/plic-flash` confere a conclusão e a recusa de um comando e a causa de transmissor vazio da UART sobre JTAG.

```bash
ulimit -s unlimited
riscv-tools --profile rv32im_zicclsm_zicsr --memory rom=ext,ram=int,icache=on,dcache=off \
  platform sync --prefix Insper-RISC-V --source Insper-RISC-V/src/simulation/modular/config.yaml \
  --output tools/riscv_build/config.generated-plic.yaml
riscv-tools --config tools/riscv_build/config.plic.yaml generate-header
riscv-tools --config tools/riscv_build/config.plic.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.plic.yaml sim
riscv-tools --config tools/riscv_build/config.plic-de0-cv.yaml compile --emit hex
riscv-tools --config tools/riscv_build/config.plic-de0-cv.yaml sim
```

O mapa dos programas está em `include-plic/` (modular) e `include-plic-de0-cv/`, e as macros comuns em `include-plic/rv32_plic_macros.h`. Com `SIM_TRACE_IRQ=1` na frente do comando de simulação, o testbench registra cada mudança das linhas do PLIC e do `MEIP`. Os mesmos programas rodam na placa com o `fpga-*` que tiver as fontes, depois de `generate-header` e `compile`.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](LICENSE).
