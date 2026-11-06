# Dados pequenos (`.sdata`, `.sbss`) e o registrador `gp`

🌐 [Português](small-data-section.md) · [English](../../en/bugs/small-data-section.md)

Este documento descreve como o GCC RISC-V coloca globais pequenos em seções próprias, o que o `crt0.S` e o `link.ld` precisam fazer para que esses globais funcionem e como diagnosticar uma falha nessa região.

## 1. Globais pequenos e `gp`

Por padrão da ABI, o GCC RISC-V coloca globais de até 8 bytes em `.sdata` (inicializados) e `.sbss` (zerados), e os acessa relativos ao registrador `gp` (`lb rd, offset(gp)`) em vez de por endereço absoluto. As instruções ficam mais curtas, mas o acesso só é correto se `gp` apontar para perto do meio da região de dados pequenos, pela convenção `__global_pointer$`.

Dois requisitos decorrem disso.

| Requisito | Onde é atendido |
| :--- | :--- |
| `gp` inicializado antes de qualquer acesso relativo a ele | `crt0.S` |
| `.sdata`, `.sbss` e `.srodata` dentro do intervalo que o `crt0.S` copia e zera | `link.ld` |

## 2. Inicialização de `gp`

A primeira instrução de `_start` carrega `gp`, antes de qualquer outro `la`. O `.option norelax` impede que o assembler troque esse cálculo por um acesso relativo ao próprio `gp`, que nesse ponto não tem valor válido:

```asm
.option push
.option norelax
1:  auipc gp, %pcrel_hi(__global_pointer$)
    addi  gp, gp, %pcrel_lo(1b)
.option pop
```

O registrador `tp` (x4) é inicializado do mesmo modo; o estado completo no instante em que `main()` é chamado está descrito no [documento de boot do Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/boot.md).

## 3. Regras do linker

O `link.ld` casa `.sdata`, `.srodata` e `.sbss` explicitamente. Um padrão `*(.data*)` sozinho não casa essas seções, e o linker as posicionaria depois que `_data_end` já foi calculado, fora do intervalo de cópia:

```ld
.data : {
    _data_start = .;
    *(.data*)
    . = ALIGN(4);
    PROVIDE(__global_pointer$ = . + 0x800);
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2)
    *(.srodata .srodata.*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
    _data_end = .;
} > RAM AT> ROM

.sbss (NOLOAD) : {
    _bss_start = .;
    *(.dynsbss)
    *(.sbss .sbss.* .gnu.linkonce.sb.*)
    *(.scommon)
} > RAM
```

O `__global_pointer$` fica em `0x800` além do início de `.sdata`. Com imediatos de 12 bits com sinal, `gp` alcança `.data` para trás e `.sbss` para a frente, ambos dentro do alcance.

## 4. Efeito de violar as regras

Com `.sdata` fora do intervalo de cópia, `_data_start` e `_data_end` ficam iguais e o laço de cópia do `crt0.S` termina na primeira iteração sem copiar nada:

```asm
1:
    bge t1, t2, 2f      // _data_start >= _data_end, sai imediatamente
    lw  t3, 0(t0)
    sw  t3, 0(t1)
    addi t0, t0, 4
    addi t1, t1, 4
    j   1b
2:
```

O global fica com o conteúdo indefinido da RAM, que no hardware real começa em branco. Com `gp` sem inicializar, todo acesso relativo a ele vai para um endereço arbitrário. O core é Harvard, sem MMU e sem tratamento de exceção, então um endereço fora da RAM e da ROM válidas pode travar o pipeline. O resultado visível é um programa que nunca escreve o veredito, e não uma falha limpa.

## 5. Diagnóstico

Os comandos abaixo, sobre o `.elf` gerado, confirmam a situação.

| Comando | O que procurar |
| :--- | :--- |
| `riscv32-unknown-elf-nm -S build/real/<teste>.elf \| grep -E "_data_\|<símbolo>"` | `_data_start` igual a `_data_end` com objetos reais em `.data` |
| `riscv32-unknown-elf-readelf -S build/real/<teste>.elf \| grep -E "\.data\|\.sdata"` | objeto em `.sdata` ou `.sbss` fora do intervalo copiado |

O teste `data-init-byte` (`c/data-init-byte/src.c`) isola o caso: um único `static volatile unsigned char` inicializado com `0xA5u`, que o GCC coloca em `.sdata`.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
