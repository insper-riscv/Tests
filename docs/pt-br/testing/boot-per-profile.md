# Programas de boot por perfil

🌐 [Português](boot-per-profile.md) · [English](../../en/testing/boot-per-profile.md)

Os programas de `c-boot/`, `cxx-boot/` e `c-boot-seq/` provam que a BOOT_MEMORY de cada perfil do núcleo leva o programa até a `main` num estado conhecido e aceita o teste seguinte. Cada programa é escrito uma vez e roda, sem edição, na simulação, na DE0-CV e na DE2-115. Quem escolhe o ambiente é o config passado ao `riscv-tools`.

## 1. Programas

| Programa | Pasta | O que prova |
| :--- | :--- | :--- |
| `boot-state` | `c-boot/` | O estado na `main`, para cada extensão que o `-march` do perfil traz: `tohost`, `fromhost` e o cabeçalho da saída padrão zerados, `mstatus`, `mie` e `mscratch` zerados com `mtvec` no tratador seguro (Zicsr), contadores correndo e sem inibição (Zicntr) e os 29 seletores, contadores e metades altas zerados (Zihpm) |
| `boot-wait-restart` | `c-boot/` | A espera entre testes: o programa suja as palavras que a BOOT limpa, deixa uma marca na RAM, toca a campainha (`fromhost`) e salta para `rv32_wait_restart`, em `0x100`. A BOOT recomeça, o programa entra de novo, acha a marca e as palavras limpas, e termina |
| `boot-cxx` | `cxx-boot/` | O C++ sobre o `crt0`: construtores globais e estáticos locais, classe com função virtual, `new` e `delete`, `std::array`, `std::vector` e um `throw` capturado com a destruição dos objetos locais |
| `boot-seq-pass`, `boot-seq-fail`, `boot-seq-hang` | `c-boot-seq/` | Os três finais possíveis de um teste (aprovado, reprovado com código e travado), para a sequência da seção 4 |

O `boot-state` e o `boot-wait-restart` rodam em qualquer perfil, inclusive no `rv32i` e no `rv32e`, que não têm CSR, porque o código de cada extensão só é compilado quando o compilador define a macro dela (`__riscv_zicsr`, `__riscv_zicntr`, `__riscv_zihpm`). O `boot-cxx` precisa da `libstdc++` da variante do perfil, e o roteiro pula a fase quando a toolchain não a tem.

## 2. Ambientes

| Ambiente | Como roda | Config |
| :--- | :--- | :--- |
| Simulado | `tools/boot/boot_profile.py`, na imagem `ci-elf` ou `dev_tools`, com o GHDL | Gerado por perfil sobre `Insper-RISC-V/src/simulation/modular` |
| DE0-CV | `tools/board/board_run.py run --stages boot boot-seq`, sob a trava da placa | `config.boot-de0-cv.yaml` e `config.boot-seq-de0-cv.yaml` |
| DE2-115 | `tools/board/board_run.py run --stages boot-de2-115 boot-seq-de2-115`, sob a trava da placa | `config.boot-de2-115.yaml` e `config.boot-seq-de2-115.yaml` |

Na placa o núcleo é o do bitstream gravado, e o `boot-state` confere as extensões desse núcleo. Um perfil novo na placa pede um bitstream novo.

## 3. Simulação por perfil

O `boot_profile.py` copia o repositório para `build-boot/<combinação>/`, e nessa cópia ajusta o `platform.yaml` da plataforma modular (sem tocar no submódulo), regenera o mapa, o `boot_config.h` e o `config_pkg` para o perfil, monta a BOOT e roda os programas.

| Combinação | Perfil | Memórias |
| :--- | :--- | :--- |
| `rv32i` | `rv32i` | ROM e RAM no núcleo |
| `rv32e` | `rv32e` | ROM e RAM no núcleo |
| `rv32im_zicsr` | `rv32im_zicsr` | ROM e RAM no núcleo |
| `caches` | `rv32im_zicclsm_zicntr_zihpm_zifencei` | ROM e RAM atrás das caches de 4 kbit |

Sem Zicsr o núcleo recusa as interrupções, que a plataforma lista como extra. Por isso a cópia dos perfis sem Zicsr perde também o bloco `interrupts` e as janelas do CLINT e do PLIC.

Depois da simulação o roteiro lê o ELF da BOOT montada e confere quatro itens.

| Conferência | Regra |
| :--- | :--- |
| Tamanho | A imagem cabe nos 4 KiB da BOOT_MEMORY |
| Ponto de espera | `rv32_wait_restart` está em `0x100` |
| `fence.i` | Existe no disassembly se e só se o perfil tem Zifencei |
| CSR | Nenhuma instrução de CSR num perfil sem Zicsr |

```bash
docker run --rm -v "$PWD:/w" -w /w ghcr.io/insper-riscv/dev_tools:latest bash -c '
  ulimit -s unlimited
  /opt/riscv-tools/riscv-tools/bin/python -I tools/boot/boot_profile.py'
```

As opções `--combination` e `--phase` (`c` ou `cxx`) escolhem o que roda, e `--timeout-cycles` fixa o limite de cada simulação.

## 4. Sequência na placa

A etapa `boot-seq` do `board_run.py` roda os programas de `c-boot-seq/` um por vez, cada um numa execução própria do `riscv-tools run`, que grava a imagem na FLASH e toca a campainha como entre dois testes de uma suíte.

| Posição | Programa | Veredito esperado |
| ---: | :--- | :--- |
| 1 a 3 | `boot-seq-pass` | PASS |
| 4 | `boot-seq-fail` | FAIL |
| 5 | `boot-seq-pass` | PASS |
| 6 | `boot-seq-hang` | TIMEOUT |
| 7 e 8 | `boot-seq-pass` | PASS |
| 9 | `boot-seq-fail` | FAIL |
| 10 | `boot-seq-pass` | PASS |

O resultado de cada posição é PASS quando o veredito coincide com o esperado, e traz os dois vereditos quando não coincide. Um teste depois de um FAIL ou de um travamento que não rode reprova a etapa.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
