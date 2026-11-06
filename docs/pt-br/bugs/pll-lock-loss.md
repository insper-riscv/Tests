# Perda de lock do PLL na placa

🌐 [Português](pll-lock-loss.md) · [English](../../en/bugs/pll-lock-loss.md)

Este documento descreve como o PLL do topo da placa perde e recupera o lock, e como reconhecer o quadro em que o JTAG básico responde mas o design não.

## 1. Sintoma

O `jtagconfig` mostra a placa saudável (`02B050DD 5CE(BA4|FA4)`), mas as operações no In-System Memory Content Editor (`riscv-tools run`, `write_rom`, `mailbox.read_mailbox`) falham ou nunca respondem. As mensagens típicas são `no mailbox result after 15.0s`, `quartus_stp` travado indefinidamente e `No editable memory instance was found`. Reprogramar a placa (`quartus_pgm`, mesmo com o `.sof`) não resolve; sem a ligação da seção 4, só o power-cycle físico resolve.

Este quadro difere da perda da cadeia JTAG ("Can't scan JTAG chain") que ocorre quando `quartus_sh --flow compile` e `quartus_pgm` rodam em chamadas separadas. Nela o `jtagconfig` deixa de enxergar o chip; aqui ele continua enxergando.

## 2. As duas camadas de JTAG

| Camada | Quem usa | Depende do clock do design |
| :--- | :--- | :--- |
| TAP JTAG básico | `jtagconfig` | não; é um bloco fixo do chip, que responde mesmo com o design quebrado, travado ou ausente |
| In-System Memory Content Editor | `read_words.tcl`, `write_full.tcl` e `write_word.tcl`, via `begin_memory_edit` e `update_content_to_memory_from_file` | sim; passa por um hub de debug embutido na fabric (`ENABLE_RUNTIME_MOD=YES`), que só responde se a lógica e os clocks que a alimentam estiverem rodando |

Um `jtagconfig` saudável garante só a camada 1. A camada 2 fica morta quando o design interno está preso, por exemplo em reset permanente.

## 3. Cadeia de causa

O `core_fpga_test.vhd` deriva todos os clocks do design (`pll_clk_if`, `pll_clk_idexmem`, `pll_clk_wb`) de um único PLL, e o core usa `pll_clk_idexmem`. O reset do core é:

```vhdl
core_reset <= (not pll_locked) or (not FPGA_RESET_N);
```

Se `pll_locked` cai, o core inteiro, e com ele o hub de debug da camada 2, fica em reset. O PLL só tenta travar de novo se receber um pulso na porta `rst`, porque a IP é gerada com `gui_pll_auto_reset` em `Off` e não re-trava sozinha. O Quartus avisa quando essa porta está solta:

```
Warning: RST port on the PLL is not properly connected on instance
pll:pll_inst|pll_0002:pll_inst|altera_pll:altera_pll_i|general[0].gpll.
```

Com `rst` fixo em `'0'`, nada pulsa o PLL, e o lock perdido nunca volta. Retentar a operação não resolve, reprogramar pode não resolver, e o power-cycle sempre resolve porque reinicializa o PLL.

## 4. Ligação do reset do PLL

O reset do PLL fica ligado ao botão físico de reset:

```vhdl
pll_inst : entity work.pll
  port map (
    refclk   => CLOCK_50,
    rst      => not FPGA_RESET_N,
    ...
```

Apertar o botão força o PLL a travar de novo, além de segurar o core em reset enquanto `pll_locked` está baixo.

## 5. Procedimento de recuperação

Quando as operações da camada 2 falham com o `jtagconfig` saudável, a ordem de recuperação é a seguinte.

1. Apertar o botão de reset físico e repetir a operação.
2. Se persistir, fazer o power-cycle da placa e programar de novo com `quartus_sh` e `quartus_pgm` em um único processo de shell.

O passo 2 segue a regra de programação da placa, descrita no [guia da placa do Insper-RISC-V](https://github.com/insper-riscv/Insper-RISC-V/blob/main/docs/pt-br/guide/placa.md).

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../../LICENSE).
