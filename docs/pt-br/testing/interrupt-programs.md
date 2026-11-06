# Programas de interrupção

🌐 [Português](interrupt-programs.md) · [English](../../en/testing/interrupt-programs.md)

Os programas de `asm-irq/` verificam a tomada de interrupção do `Core` com código executável de verdade. Cada um se confere sozinho e termina gravando em `tohost`. O `tests/python` os monta, carrega no `core` com `has_interrupts` e os executa contra um modelo do CLINT.

## 1. Programas

| Programa | O que verifica |
| :--- | :--- |
| `irq-timer-basic` | Uma interrupção de timer em modo direto entra uma vez, com `mcause = 0x80000007`, `mepc` dentro do laço em execução, `MIE` zero e `MPIE` um dentro do tratador, e o `mret` devolve `MIE` e o controle ao laço |
| `irq-masked` | Nada é tomado com `mstatus.MIE` em zero nem com o bit de `mie` em zero, embora `mip.MTIP` mostre o pendente. Ao ligar o bit de `mie`, a interrupção toma a instrução seguinte, que roda uma vez depois do retorno |
| `irq-mepc-precise` | Um bloco de 400 somas é interrompido a cada poucas dezenas de ciclos. Em cada entrada o registrador somado vale a quantidade de somas antes de `mepc`, e no fim vale 400 |
| `irq-vectored` | Com `mtvec` vetorado, a interrupção de software vai a `BASE + 12`, a de timer a `BASE + 28` e o `ecall` (causa 11, exceção) a `BASE`. O modo é guardado, o bit 1 cai e a `BASE` perde os bits 5 a 2 |
| `irq-priority` | Software e timer pendentes juntos: a de software entra primeiro e a de timer logo depois, ambas sobre a mesma instrução |
| `irq-wfi` | A `wfi` executa como `nop`, com e sem interrupção pendente, e a interrupção tomada sobre ela devolve o controle à própria `wfi` |
| `irq-ecall` | Com uma interrupção pendente no momento do `ecall`, a interrupção entra primeiro e o `ecall` trapa depois do retorno, com o mesmo `mepc` |

## 2. Modelo do CLINT

O modelo tem o leiaute do CLINT da SiFive, com acessos de 32 bits e a metade baixa no endereço menor.

| Deslocamento | Registrador |
| :---: | :--- |
| `0x0000` | `msip` (bit 0), que acende a linha `MSIP` |
| `0x4000` | `mtimecmp` baixa |
| `0x4004` | `mtimecmp` alta |
| `0xBFF8` | `mtime` baixa |
| `0xBFFC` | `mtime` alta |

O `mtime` soma 1 por ciclo, o `mtimecmp` vale tudo em 1 após o reset e a linha `MTIP` vale $mtime \ge mtimecmp$ em 64 bits. O modelo ocupa a região de `RV32_CLINT_BASE` dentro da faixa da RAM interna. Uma plataforma com as interrupções ligadas define essa constante no cabeçalho gerado dela, e os programas usam os mesmos nomes.

## 3. Uso

```bash
uv run python tests/python/runner.py irq-programs
```

O comando exige a toolchain RISC-V no `PATH`, como a imagem `ci-elf`. O ponteiro do submódulo `Insper-RISC-V` precisa trazer o `Core` com a tomada de interrupção.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
