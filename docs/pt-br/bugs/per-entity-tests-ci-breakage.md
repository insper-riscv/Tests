# Testes por entidade (`tests/python/`)

🌐 [Português](per-entity-tests-ci-breakage.md) · [English](../../en/bugs/per-entity-tests-ci-breakage.md)

Este documento descreve a suíte de testes cocotb de entidades VHDL e de sequências cruas de instruções, e as condições para que o grupo `instructions` volte a rodar.

## 1. A suíte

A suíte roda com `uv run python tests/python/runner.py`, separada da suíte do `riscv-tools` (`asm` e `c`). Ela testa entidades VHDL individuais (`ALU`, `RegFile`, `ROM_simulation` e outras) e algumas sequências de instruções cruas, todas configuradas em `tests/python/tests.json`, sem passar pelo compilador, pelo linker nem pelo `config.yaml` do `riscv-tools`.

O `tests.json` tem dois grupos de entradas.

| Grupo | Conteúdo | Estado no `all` |
| :--- | :--- | :--- |
| `entities` | um testbench por entidade | roda |
| `instructions` | `one`, `two`, `three`, `four`, `five`, `six`, `MUL`, `FWD`, `LOAD_USE` e `MEXT`, sequências cruas de instruções | pulado |

## 2. Entradas com `skip`

Uma entrada com `"skip": true` e um `"skip_reason"` fica fora da varredura `all`; o runner imprime o motivo e segue. Chamada pelo nome (`python3 tests/python/runner.py one`), a mesma entrada roda normalmente, o que permite ver o erro atual sem editar o `tests.json`. É o mesmo mecanismo dos arquivos `.off` das suítes `asm` e `c`, adaptado ao formato de configuração único desta suíte.

## 3. Por que o grupo `instructions` está pulado

O topo `rv32i3stage_core_sim_test.vhd` instancia duas cópias de `ROM_simulation`, uma para a BOOT_ROM (generic `ROM_FILE => BOOT_ROM_FILE`) e outra para a FLASH (generic `ROM_FILE => ROM_FILE`). As dez entradas do grupo só passam `ROM_FILE`. Sem `BOOT_ROM_FILE`, a BOOT_ROM usa o valor padrão da entidade, `default.hex`, que não existe, e a elaboração falha antes de qualquer teste:

```
rom_simulation.vhd:79:12:error: cannot open file: default.hex
```

Passar `BOOT_ROM_FILE` faria a elaboração passar, mas os testes seguiriam errados, porque cada módulo supõe que o reset sai direto para o conteúdo de `ROM_FILE` no endereço 0. Na arquitetura de memória, o reset entra sempre pela BOOT_ROM, e o código de usuário começa em `FLASH_BASE = 0x800`, depois do boot. O cálculo do PC em `tests/python/unittests/instructions/one.py` mostra a suposição:

```python
# PC de cada AUIPC: começa em 4 * nº da instrução (modelo single-cycle)
pcs = [i * 4 for i in range(len(auipc_immediates))]
```

Os arquivos `.hex` de `tests/python/unittests/instructions/codes/` também começam no endereço 0 do array, sem o padding que o `riscv-tools` aplica para alinhar o conteúdo a `FLASH_BASE`. O salto da BOOT_ROM ainda acrescenta bolhas ao pipeline.

## 4. Condições para reativar um teste do grupo

Cada um dos dez testes precisa de três itens.

1. Um `BOOT_ROM_FILE` real, como `boot_rom.hex` ou um stub mínimo que só executa `j 0x800`.
2. O `.hex` do teste gerado de novo, com o padding de `FLASH_BASE`.
3. As contas de PC e de número de ciclos do módulo Python reescritas para incluir o atraso do boot.

A arquitetura de memória e o boot estão descritos na [documentação de memória do Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md).

## 5. Falha de teste no cocotb 2.0

No cocotb 2.0, qualquer exceção levantada dentro do teste o marca como FAIL, e a classe `TestFailure` de `cocotb.result` não existe. Os testes devem falhar com `AssertionError`:

```python
raise AssertionError("mensagem")
```

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
