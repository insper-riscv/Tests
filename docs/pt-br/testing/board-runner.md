# Runner das placas

🌐 [Português](board-runner.md) · [English](../../en/testing/board-runner.md)

O `tools/board/board_run.py` roda nas placas DE0-CV, DE0-Nano, DE10-Lite e DE2-115 os programas do Tests e, na DE0-CV, com o config gerado pelo Certification, os ELFs do ACT4. Cada item recebe PASS, FAIL ou TIMEOUT, e o runner escreve um JSON por placa.

## 1. Etapas

Sem `--stages`, o runner roda as etapas da DE0-CV, porque uma sessão trava uma placa só. Cada etapa traz a placa dela, e pedir etapas de placas diferentes na mesma chamada é um erro.

| Etapa | Placa | Config | Programas |
| :--- | :--- | :--- | :--- |
| `regression` | `de0-cv` | `config.sdram-flash-regression.yaml` | `asm/` e `c/`, menos os três da UART física |
| `sdram` | `de0-cv` | `config.sdram-flash.yaml` | `asm-sdram/` e `c-sdram/` |
| `irq` | `de0-cv` | `config.sdram-flash-irq.yaml` | `asm-irq/`, ligados ao CLINT real da plataforma em `0xE0000000` |
| `de0-nano-regression`, `de0-nano-sdram` | `de0-nano` | `config.nano-regression.yaml`, `config.nano-sdram.yaml` | Os de `asm/`, `c/`, `asm-sdram/` e `c-sdram/` que o perfil RV32I com Zicsr e Zifencei comporta |
| `de10-lite-regression`, `de10-lite-sdram`, `de10-lite-irq` | `de10-lite` | `config.lite-regression.yaml`, `config.lite-sdram.yaml`, `config.lite-irq.yaml` | Os mesmos programas da DE0-CV, no perfil completo |
| `de2-115` | `de2-115` | `config.de2-115.yaml` | Os programas de `asm/`, `c/`, `asm-sdram/` e `c-sdram/` com a FLASH NOR e a SDRAM de 32 bits |
| `de2-115-n2` | `de2-115` | `config.de2-115-n2.yaml` | Os programas que cabem na RAM emulada de 128 KiB, sem os da janela de 16 MB da SDRAM |

Os programas `uart-echo`, `uart-tx`, `uart-errors` e `uart-stdio` usam os pinos da UART, que as placas não têm ligados. O `hex-id` pausa para uma pessoa ler o display, e por isso o runner dá a ele um timeout de 90 s.

## 2. Subcomandos

| Subcomando | Onde roda | O que faz |
| :--- | :--- | :--- |
| `prepare` | imagem `ci-elf`, sem placa | `generate-header` e `compile --emit mif` de cada etapa |
| `run` | máquina com Quartus e cabo | escolhe a placa pelo JTAG e toma a trava dela, programa o `.sof`, roda as etapas e escreve `results-<host>.json` |
| `act` | máquina com Quartus e cabo | escolhe e trava a placa, programa o `.sof` e roda `certify-board --skip-build` sobre os ELFs já construídos |

A variável `RISCV_TOOLS` dá o comando do `riscv-tools`, por exemplo `uv run --project <clone-do-Tools> riscv-tools`. O runner não guarda IDCODE nem nome de trava. Quem acha a placa é o `riscv-tools board lock`, pelo IDCODE da cadeia e pela porta USB do cabo (pacote `Insper-RISC-V/boards/<placa>`, com a placa da etapa), nunca pelo nome do cabo; o cabo escolhido segue para o `quartus_pgm` e, por `--cable`, para os `riscv-tools` filhos. Ver o módulo `board` do Tools.

## 3. Duas placas em paralelo

A opção `--part I/N` reparte a lista ordenada: a placa `I` roda os itens de índice `I` módulo `N`. O rótulo do host (`--host`, também aceito como `--board`) é o hostname curto da máquina, dado por `hostname -s`, ou `laptop` no notebook; ele entra no nome da trava, como `de0cv-laptop.lock` ou `de0cv-<hostname>.lock`, o mesmo arquivo que os demais usuários da placa tomam. Com `--part auto` o runner toma uma placa livre do host e reparte a lista entre todas as placas da plataforma que o host tem, na ordem do identificador delas.

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py run --host laptop --part 0/2 --sof de0-cv.sof
```

## 4. ELFs do ACT4

O Certification gera o alvo da placa com `gen_profiles.py --profile rv32im_full_zifencei --board --out build/target` e constrói os ELFs com `riscv-tools --config build/target/config.yaml certify-board --build-only`, na imagem `ci-act4`. O runner roda então:

```bash
python3 tools/board/board_run.py act --host "$(hostname -s)" --part 1/2 --sof de0-cv.sof \
  --config <Certification>/build/target/config.yaml --root <Certification> \
  --target rv32im_full_zifencei__board
```

## 5. Programas de atualização, Zihpm reduzido e PLIC

O `tools/board/board_update.py` conduz os programas que o `riscv-tools run` sozinho não conduz. Ele toma a trava da DE0-CV com `board_session`, do mesmo `board_run.py`, e exporta o cabo da placa travada em `RISCV_TOOLS_CABLE` para o `riscv-tools` que abre o JTAG. A opção `--host` (também aceita como `--board`) escolhe o rótulo do host, o hostname curto da máquina ou `laptop` no notebook, qualquer texto sem `/` nem espaço em branco, e `--skip-lock` serve a quem já segura a trava e exporta o cabo.

| Etapa | Programas | Condução |
| :--- | :--- | :--- |
| `update` | `flash-update`, `flash-update-interrupted` e `update-new` | Zera a palavra de sobrevivência da SDRAM e o `tohost`, grava a FLASH inteira pelo carregador JTAG, envia a imagem da banca B pela UART (`flash-image send`) e confere o `stdout` com o `uart_expected.txt`. O `update-new` roda sozinho, gravado na banca B |
| `plic` | `plic-flash` | Roda com o console da UART ligado, que esvazia a fila de transmissão |

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  uv run --project ../Tools python tools/board/board_update.py --host laptop
```

O `hpm-reduced` (`config.sdram-flash-hpm-reduced.yaml`) exige o bitstream reduzido, com 4 contadores de 40 bits, e falha no completo. Compila-se com `quartus_sh -t de0-cv_synth.tcl 1 HPM_COUNTERS=4 HPM_WIDTH=40` e roda-se com `riscv-tools --config tools/riscv_build/config.sdram-flash-hpm-reduced.yaml run --skip-reconfigure --skip-recompile`, depois de programar o `.sof` reduzido.

A FLASH da placa é regravada pelo carregador JTAG (`quartus.rom_write_method: jtag_loader`), que reinicia o núcleo a cada programa; a porta de depuração da SDRAM é a instância 3 do Virtual JTAG (`quartus.sdram_debug_instance`).

## 6. Outras placas

Nas plataformas que ligam só parte das extensões, o `prepare` compila só os programas cujo cabeçalho `RV32_EXT` cita extensões que a plataforma tem, e o runner nunca vê os outros.

| Plataforma | Placa | Extensões dos programas | Etapas |
| :--- | :--- | :--- | :--- |
| `de0-nano` | `de0-nano` | Zicsr e Zifencei (RV32I) | `de0-nano-regression`, `de0-nano-sdram` |
| `de10-lite` | `de10-lite` | As do perfil completo | `de10-lite-regression`, `de10-lite-sdram`, `de10-lite-irq` |
| `de2-115` | `de2-115` | As do perfil completo | `de2-115` |
| `de2-115-n2` | `de2-115` | As do perfil completo, sem os programas da janela da SDRAM | `de2-115-n2` |

```bash
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py prepare --stages de0-nano-regression de0-nano-sdram
RISCV_TOOLS="uv run --project ../Tools riscv-tools" \
  python3 tools/board/board_run.py run --stages de0-nano-regression de0-nano-sdram --host "$(hostname -s)" --sof de0-nano.sof
```

Os resultados de cada placa estão em [board-results.md](board-results.md).

## 7. Cabeçalhos e configs das plataformas

O arquivo `include-<plataforma>/rv32_platform.h` de cada plataforma repete o mapa do `platform.yaml` dela, e o `config.platform-<plataforma>.yaml` sai do `platform sync`. Dois comandos conferem os dois.

| Comando | O que confere |
| :--- | :--- |
| `riscv-tools platform sync --prefix Insper-RISC-V --check --source <config.yaml da plataforma> --output tools/riscv_build/config.platform-<plataforma>.yaml` | O config gerado é o que a plataforma produz |
| `make check-headers` | Cada macro de endereço (`RV32_RAM_BASE`, `RV32_RAM_END`, `RV32_STDOUT_BASE`, `RV32_TOHOST`, `RV32_FROMHOST`, `RV32_UART_BASE`, `RV32_FLASH_CTRL_BASE`, `RV32_CLINT_BASE`, `RV32_PLIC_BASE` e as bancas da FLASH) vale o que o `platform.yaml` da plataforma define |

A conferência dos cabeçalhos lê o mapa com o mesmo leitor do `riscv-tools check-memory-map` e usa `tools/headers/headers.json` para ligar cada pasta `include-*` ao `platform.yaml` dela. Ela roda também nos testes do host (`tests/host/test_platform_headers.py`). Com `make check-headers ARGS=--write` os valores divergentes são reescritos no cabeçalho.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
