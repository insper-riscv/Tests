# Dois bugs no step "Run per-entity VHDL unit tests" (`tests/python/`)

## Contexto

Esse step (`uv run python tests/python/runner.py`) é uma suíte de testes
cocotb **separada** da suíte `riscv-tools` (`Tests/asm`/`Tests/c`) que o
resto deste repositório usa: mais antiga, testa entidades VHDL individuais
(`ALU`, `RegFile`, `ROM_simulation`, etc.) e algumas sequências de instrução
cruas, configuradas em `tests/python/tests.json`, sem passar pelo
compilador/linker/`config.yaml` do `riscv-tools`.

Achados investigando por que o `sim.yml` do CI passou a falhar nesse step
(depois de mexer no cache do Spike, ver
[MEMORY_ARCHITECTURE.md](../MEMORY_ARCHITECTURE.md)): os dois problemas
abaixo já existiam antes, só nunca tinham sido notados por alguém rodando
esse step até o fim.

## Bug 1 (corrigido): `ROM.py` usava API removida do cocotb 1.x

```
ImportError: cannot import name 'TestFailure' from 'cocotb.result'
```

`tests/python/unittests/entities/ROM.py` fazia `from cocotb.result import
TestFailure` e `raise TestFailure(...)`: essa classe **não existe mais no
cocotb 2.0** (o projeto já usa `cocotb>=2.0`, confirmado nos próprios logs:
`Initialized cocotb v2.0.1`). O teste crashava no import do módulo, antes de
rodar qualquer asserção.

**Fix**: removida a importação; `raise TestFailure(...)` virou `raise
AssertionError(...)`: em cocotb 2.0 qualquer exceção levantada dentro do
teste já marca ele como FAIL, não precisa de uma classe especial. Verificado
rodando `python3 tests/python/runner.py ROM` isoladamente: `TESTS=1 PASS=1`.

## Bug 2 (não corrigido de verdade; testes marcados como `skip`): regressão real do redesign BOOT_ROM+FLASH

```
/.../Memory/sim/ROM_simulation.vhd:79:12:error: cannot open file: default.hex
in process ...boot_rom@rom_simulation(rtl).init
```

### Causa raiz

Desde o redesign de memória (BOOT_ROM fixo + FLASH + RAM; ver
[MEMORY_ARCHITECTURE.md](../MEMORY_ARCHITECTURE.md)),
`rv32i3stage_core_sim_test.vhd` instancia
**duas** cópias de `ROM_simulation`: uma para BOOT_ROM (generic
`ROM_FILE => BOOT_ROM_FILE`) e uma para FLASH (generic `ROM_FILE =>
ROM_FILE`). As 10 configs do grupo `instructions` em `tests/python/tests.json`
(`one`, `two`, `three`, `four`, `five`, `six`, `MUL`, `FWD`, `LOAD_USE`,
`MEXT`) só passavam `ROM_FILE`: nunca foram atualizadas depois do redesign
para também passar `BOOT_ROM_FILE`. Sem esse generic, BOOT_ROM cai no valor
padrão hardcoded na própria entidade (`"default.hex"`), que não existe, e a
elaboração falha antes de qualquer teste rodar.

### Por que isso NÃO é um fix de uma linha

Só adicionar `BOOT_ROM_FILE` faria a elaboração passar, mas os testes
continuariam **logicamente errados**, porque cada um desses 10 módulos
assume a arquitetura antiga (reset saindo direto pro conteúdo de
`ROM_FILE`, endereço 0), não a atual (reset sempre entra por BOOT_ROM
primeiro, código de usuário só começa em `FLASH_BASE = 0x800` depois do
boot). Exemplo concreto (`tests/python/unittests/instructions/one.py`):

```python
# PC de cada AUIPC: começa em 4 * nº da instrução (simples modelo single-cycle)
pcs = [i * 4 for i in range(len(auipc_immediates))]
```

Esse `pcs` assume PC = 0, 4, 8, ... desde o primeiro ciclo: com um BOOT_ROM
de verdade na frente, a execução do código do teste só começaria em
`0x800`, e ainda levaria ciclos extras (o próprio salto do BOOT_ROM causa
bolhas no pipeline). Os `.hex` desses testes (`tests/python/unittests/
instructions/codes/**/*.hex`) também foram gerados assumindo que o próprio
conteúdo começa no endereço 0 do array, sem o padding que o `riscv-tools`
real usa (`bin_to_image.read_words`'s `pad_words`, ver
`Tests/tools/Tools/src/riscv_tools/bin_to_image/core.py`) para alinhar o
conteúdo a partir de `FLASH_BASE`.

Um fix de verdade precisa, para cada um dos 10 testes:
1. Um `BOOT_ROM_FILE` real (ex: `boot_rom.hex`, ou um stub mínimo que só dá
   `j 0x800`);
2. Os `.hex` de cada teste re-gerados/re-alinhados com o padding de
   `FLASH_BASE`;
3. As contas de PC/número de ciclos de cada módulo Python reescritas para
   contar o atraso real do boot.

Isso é trabalho de redesenho, não um ajuste de parâmetro: por isso não foi
feito aqui.

### O que foi feito por enquanto

As 10 entradas em `tests/python/tests.json` ganharam `"skip": true` +
`"skip_reason"` (mesmo espírito dos arquivos `.off` já usados na suíte
`asm`/`c`, ver [DATA_HARVARD_BUG.md](DATA_HARVARD_BUG.md); só que adaptado
ao formato de config único deste suite). `tests/python/runner.py` agora
pula qualquer entrada com `"skip": true` quando roda `all` (imprimindo o
motivo), mas ainda tenta rodar normalmente se chamada explicitamente por
nome (`python3 tests/python/runner.py one`): útil para quem for atacar o
redesenho de verdade e quiser ver o erro atual sem editar `tests.json` de
volta.

Verificado localmente: `python3 tests/python/runner.py all` agora termina
com exit code 0 (13 testes passam, 10 pulados, 0 erros): o mesmo comando
que o `sim.yml` do CI roda.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../LICENSE).
