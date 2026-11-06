# Inicialização de `.data` no core Harvard

🌐 [Português](data-harvard.md) · [English](../../en/bugs/data-harvard.md)

Este documento descreve como o core acessa a FLASH a partir do estágio MEM, como o boot copia o `.data` para a RAM e o que isso permite aos programas de teste.

## 1. Barramentos do core

O core (`rv32im_pipeline_core`) é Harvard. Os barramentos de instrução e de dado são fisicamente independentes.

| Barramento | Origem do endereço | Alcança |
| :--- | :--- | :--- |
| `rom_addr` | `pc_fetch` | BOOT_ROM e FLASH, para a busca de instrução |
| `ram_addr` | `exmem_alu_out`, isto é, o resultado da ALU de qualquer `lw` ou `sw` | RAM |
| segunda porta de leitura do estágio MEM | `exmem_alu_out` | FLASH, só para a cópia de `.data` no boot |

Sem a segunda porta, nenhum `lw` ou `sw` alcançaria a FLASH, e o mecanismo padrão de `.data` não teria como funcionar. A separação dos barramentos é uma escolha deste core, e a ISA RISC-V não a exige.

A divisão da memória em BOOT_ROM (bootloader fixo), FLASH (firmware de cada teste) e RAM (dado), com o mapa de endereços, está descrita na [documentação de memória do Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/platform/memoria.md).

## 2. Cópia de `.data` no boot

O `crt0.S` copia o `.data` da FLASH para a RAM com um laço de `lw` e `sw`, usando o endereço de carga `_data_load` e o intervalo `_data_start` a `_data_end` do `link.ld`. Cada `lw` da FLASH passa pela segunda porta do estágio MEM. O `.bss` é zerado por `sw`, sem ler a ROM. A regra de posicionamento das seções de dados pequenos que entram nesse intervalo está no documento de [dados pequenos](small-data-section.md).

## 3. O que isso permite a um teste

Um inicializador C comum em escopo de arquivo funciona:

```c
static volatile unsigned int value = 0xCAFEBABEu;

int main(void) {
    if (value != 0xCAFEBABEu) RV32_FAIL();
    RV32_PASS();
}
```

Os testes `c/data-init-byte`, `c/data-init-halfword`, `c/data-init-word`, `c/data-init-word-array` e `c/data-initialization` verificam essa inicialização, cada um com um tamanho ou forma de dado diferente.

## 4. Leitura da mailbox pelo JTAG

O orquestrador lê o veredito do programa na mailbox por polling, e cada leitura é uma chamada `quartus_stp` que abre e fecha uma sessão de JTAG e do In-System Memory Editor. Um volume alto de chamadas rápidas é frágil, da mesma classe da perda da cadeia JTAG entre `quartus_sh` e `quartus_pgm`. O `poll_interval_seconds` da configuração do build define o intervalo entre as leituras; um valor maior reduz o número de sessões abertas na janela de timeout, trocando latência de detecção por confiabilidade de leitura.

Para saber se um programa travou ou apenas não foi lido, a mailbox pode ser lida uma única vez, depois de dar tempo ao programa:

```bash
quartus_stp -t write_full.tcl "USB-Blaster [1-4]" "@1: ..." 0 data-init-byte.mif
quartus_stp -t write_word.tcl "USB-Blaster [1-4]" "@1: ..." 1 4094 1   # go-flag
sleep 5
quartus_stp -t read_words.tcl "USB-Blaster [1-4]" "@1: ..." 1 4095 1   # mailbox
```

Um valor `2` na mailbox é FAIL: a CPU terminou e escreveu o veredito.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
