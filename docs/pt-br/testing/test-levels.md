# Níveis de teste do VHDL

🌐 [Português](test-levels.md) · [English](../../en/testing/test-levels.md)

O VHDL do RV32IM passa por nove níveis de verificação, do mais barato e rápido ao mais caro e mais próximo do hardware real. Cada nível pega uma classe de erro que os anteriores não pegam, e nenhum substitui o seguinte.

## 1. Visão geral

| Nível | Nome | Verifica | Ferramenta | Onde roda |
| :---: | :--- | :--- | :--- | :--- |
| 0 | Consistência estática | as cópias manuais do mapa de memória e os caminhos de arquivos | `riscv-tools check-memory-map`, `check-paths` | CI de simulação e CI de cada repositório |
| 1 | Análise e elaboração | o VHDL compila e o topo elabora | GHDL | CI de cada repositório |
| 2 | Entidade | o comportamento de um bloco isolado | cocotb + GHDL | CI de cada repositório |
| 3 | Programa no core completo | o pipeline executa programas reais | cocotb + GHDL + Spike | CI de simulação |
| 4 | Plataforma com SDRAM | o core com a SDRAM, o controlador e o modelo do chip | cocotb + GHDL | CI de simulação |
| 5 | Topo de hardware simulado | o topo da placa com os modelos Intel das memórias | GHDL + bibliotecas do Quartus | manual, no runner com Quartus |
| 6 | Síntese | o VHDL é sintetizável e cabe no FPGA com timing | `ghdl --synth`, Quartus | Quartus no CI de hardware real; `ghdl --synth` manual |
| 7 | Formal | propriedades válidas para todos os estados | SymbiYosys | manual |
| 8 | Placa | o resultado na FPGA, com atrasos reais | BIST por JTAG, `riscv-tools run` | suíte real no CI de hardware real (runner self-hosted); BIST manual |

Três termos aparecem ao longo do texto.

| Termo | Significado |
| :--- | :--- |
| Golden | o resultado esperado de um programa, isto é, o conteúdo da memória ao final da execução, gerado pelo Spike (simulador de referência) ou versionado junto do teste |
| `tohost` | a palavra de 64 bits da memória em que o programa escreve o veredito ao terminar (1 é PASS, `(código << 1) | 1` é FAIL), lida pelo testbench ou pelo JTAG |
| Tick | a unidade de tempo da simulação; o clock base tem três ticks por ciclo do core |

## 2. Nível 0, consistência estática

O mapa de memória (onde ficam a boot ROM, a FLASH, a RAM e as palavras reservadas) é copiado à mão em muitos lugares: configuração dos testes, flags do linker, boot ROM, runtime, constantes do VHDL do core, IPs de memória e testbench. O `platform.yaml` de cada plataforma é a fonte de verdade, e `check-memory-map` confere cada cópia contra ele, para a plataforma `internal-mem` e para a `sdram`. `check-paths` confere que todo caminho listado no manifesto existe.

| | |
| :--- | :--- |
| Pega | endereço ou tamanho divergente entre duas cópias, arquivo movido ou renomeado |
| Não pega | qualquer comportamento do hardware |
| Comandos | `riscv-tools --root Insper-RISC-V check-memory-map --platform src/<família>/<plataforma>/platform.yaml` e `make paths` |
| Custo | segundos, sem simulador |

## 3. Nível 1, análise e elaboração

O GHDL analisa os arquivos VHDL-2008 na ordem de dependência que o `riscv-tools vhdl-sort` calcula e elabora o topo de simulação. Um erro de sintaxe, de tipo ou de porta aparece aqui, antes de qualquer simulação.

| | |
| :--- | :--- |
| Pega | erro de sintaxe, tipo, porta ou dependência circular |
| Não pega | lógica errada que compila |
| Comando | `make check`, em cada repositório |
| Custo | segundos |

## 4. Nível 2, entidade

Cada bloco VHDL tem um testbench em Python (cocotb) que dirige as portas diretamente e confere as saídas, sem o compilador nem o fluxo de simulação do `riscv-tools`. Os testes ficam no repositório do bloco.

| Repositório | Blocos testados |
| :--- | :--- |
| Core | banco de registradores, ALU, decodificador de instrução, extensores de imediato e de RAM, gerenciador de store, unidade de controle, decodificação de M, detecção de hazard, forwarding, bubble mux e o stall do pipeline com memória de latência variável |
| Memory | RAM e ROM de simulação, o sistema SDRAM (controlador, ponte e modelo do chip), o autoteste (BIST) com e sem registradores nos pinos, a porta de debug JTAG e o barramento externo (interconnect e UART JTAG) |
| Peripherals | GPIO, TIMER, os decodificadores de operação, MUX4, MUX8 e o buffer tri-state |
| Insper-RISC-V | o gerador de clocks do topo |

Os testes da SDRAM rodam contra um modelo do chip que **aborta a simulação** em qualquer violação de timing (tRCD, tRP, tRAS, tRC, tRFC, tWR, tMRD), de ordem de comandos, de sequência de power-up, de registrador de modo, de sobreposição de burst ou de intervalo de refresh. Um defeito injetado de propósito no controlador (tRCD curto, ausência de refresh, ACTIVE cedo demais, captura de leitura cedo demais, contador de preenchimento de 24 bits) precisa fazer algum teste falhar; assim se confirma que os testes não passam em vazio.

| | |
| :--- | :--- |
| Pega | erro de lógica de um bloco, protocolo de um handshake, caso de borda de uma máquina de estados |
| Não pega | erro de integração entre blocos, ou erro que só aparece rodando um programa |
| Comandos | `make test` (todos) ou `make test TEST=<nome>`, em cada repositório |
| Custo | minutos por repositório |

## 5. Nível 3, programa no core completo

O topo de simulação reúne o core, a boot ROM e a FLASH, e roda a suíte de programas em assembly e em C. O fluxo de cada programa tem dois passos.

1. `riscv-tools compile --emit hex` compila o programa com o GCC RISC-V (picolibc) e gera as imagens de memória.
2. `riscv-tools sim` carrega a imagem, roda o core até o programa escrever o veredito no `tohost` e compara o resultado com o golden.

| Tipo de programa | Golden |
| :--- | :--- |
| Assembly | versionado junto do teste |
| C com conferência de memória | gerado pelo Spike |
| C que só sinaliza PASS ou FAIL | nenhum; vale o veredito do `tohost` |

O tempo de cada programa, em ticks, também é registrado e serve de base para medir o custo de uma mudança de memória.

| | |
| :--- | :--- |
| Pega | instrução errada, hazard ou forwarding que só aparece em sequência, integração entre pipeline, memórias e boot |
| Não pega | comportamento da SDRAM, timing de pinos, sintetizabilidade |
| Comandos | `riscv-tools --config tools/riscv_build/config.yaml generate-header`, `compile --emit hex` e `sim` |
| Custo | minutos para a suíte inteira |

## 6. Nível 4, plataforma com SDRAM

A plataforma `sdram` usa a SDRAM de 64 MB como única RAM do core, através da ponte, do árbitro e do controlador, com o modelo do chip do nível 2. Há duas suítes.

| Suíte | Configuração | O que roda | Veredito |
| :--- | :--- | :--- | :--- |
| Regressão | `config.sdram-flash-regression.yaml` | os mesmos programas do nível 3 | o resultado precisa ser idêntico ao da `internal-mem` |
| Programas da SDRAM | `config.sdram-flash.yaml` | programas que usam a SDRAM de propósito (acesso a byte, cópia, laço, teste de memória, multiplicação e divisão com operandos na SDRAM, refresh) | cada programa confere a si mesmo e sinaliza PASS ou FAIL |

No final de um programa que passa, o testbench compara o golden com o **conteúdo do modelo do chip**, e não com as escritas vistas no barramento. Uma palavra que nunca chegou ao chip, ou chegou ao endereço errado, é pega.

A razão entre os ciclos da SDRAM e os da RAM interna, medida nos mesmos programas, dimensiona uma cache L1 de dado.

| | |
| :--- | :--- |
| Pega | erro de protocolo da SDRAM no sistema inteiro, dado que não chega ao chip, regressão do core ao trocar a memória |
| Não pega | atraso de pino, fase de clock e timing real, que o simulador não modela |
| Comandos | os do nível 3 com as duas configurações acima |
| Custo | minutos por suíte |

## 7. Nível 5, topo de hardware simulado

O topo que vai para a placa instancia memórias geradas pelo Quartus. Este nível simula esse topo com as memórias em modelos da Intel (biblioteca `altera_mf`, que acompanha o Quartus) e roda a suíte nele. Nenhuma placa é tocada.

| | |
| :--- | :--- |
| Pega | diferença de comportamento entre o modelo de memória da simulação dos níveis 3 e 4 e o IP de memória real |
| Não pega | síntese e timing |
| Como roda | workflow manual, no runner self-hosted que tem o Quartus, dentro da imagem do toolchain com o Quartus montado em leitura |
| Custo | tempo de uma simulação completa |

## 8. Nível 6, síntese

Dois verificadores independentes respondem a perguntas diferentes.

| Verificador | Pergunta |
| :--- | :--- |
| `ghdl --synth` | o VHDL descreve hardware sintetizável? Um arquivo com `wait for 10 ns`, por exemplo, é rejeitado |
| Quartus (análise e síntese, fitter, timing) | o projeto cabe no FPGA e o timing fecha? |

`ghdl --synth` é mais permissivo que o subconjunto de VHDL-2008 que o Quartus aceita, então passar nele não garante que o Quartus aceite. Ele serve como checagem rápida e independente de licença, e como primeiro passo para a verificação formal do nível 7.

O timing do Quartus é conferido no modelo lento a 85 °C, com os tempos do chip do datasheet da SDRAM (ISSI IS42/45S16320D, grau -7). Uma margem negativa em qualquer caminho reprova o projeto. Os caminhos conferidos na plataforma `sdram` são os seguintes.

| Caminho | Domínio |
| :--- | :--- |
| leitura de dado da SDRAM | controlador |
| comandos e dado de escrita | pino do clock da SDRAM |
| lógica do core | clock do core |

| | |
| :--- | :--- |
| Pega | construção que simula e não sintetiza, subconjunto do Quartus, recursos e timing |
| Não pega | comportamento funcional |
| Comandos | `ghdl --synth --std=08 --out=verilog <entidade>` e `quartus_sh --flow compile <projeto>` |
| Custo | segundos para o `ghdl --synth`; alguns minutos para o Quartus |

## 9. Nível 7, verificação formal

O SymbiYosys prova propriedades para **todos** os estados possíveis, não só para os que um testbench exercita. O VHDL é convertido em Verilog pelo `ghdl --synth`, e as propriedades ficam em SystemVerilog em volta do netlist. O ambiente é descrito por suposições: um mestre só muda o toggle depois de ver a resposta e mantém os barramentos até lá, e o controlador responde em até 4 clocks.

O bloco verificado é o árbitro que divide o controlador da SDRAM entre o core e a porta de debug.

| Propriedade | Tipo de prova |
| :--- | :--- |
| nunca há duas requisições ao controlador ao mesmo tempo, e os barramentos ficam estáveis enquanto ela está pendente | indução, sem limite de profundidade |
| cada concessão leva a palavra de um mestre que esperava | indução |
| cada requisição recebe uma só resposta, com o toggle certo, e o dado lido é o devolvido pelo controlador | indução |
| uma requisição é respondida em no máximo 14 clocks, com o controlador respondendo em até 4 | verificação até a profundidade 45; 13 clocks não bastam |

Um defeito injetado no árbitro (dado de leitura trocado entre os mestres, ou resposta sem esperar o controlador) é pego pelas propriedades. Duas alterações da regra de prioridade entre os mestres não são pegas, porque o protocolo de toggle já impede a inanição: um mestre só refaz a requisição depois de ver a resposta, e nesse intervalo o árbitro atende o outro.

| | |
| :--- | :--- |
| Pega | violação de protocolo em algum estado raro, que nenhum teste simulado alcança |
| Não pega | tudo fora do modelo, isto é, um único reset no início, controlador com resposta limitada e clocks tratados como um só |
| Comando | `sby -f <arquivo>.sby` |
| Custo | segundos por prova neste bloco |

## 10. Nível 8, placa

Dois testes na FPGA, com atrasos de pino, fase de clock e timing reais.

| Teste | Projeto gravado | O que roda |
| :--- | :--- | :--- |
| Autoteste da SDRAM (BIST) | `sdram-bringup`, sem o core | quatro modos pelo JTAG: rápido com máscaras de byte, padrões, varredura do chip inteiro e endereços aleatórios; o critério é zero palavras erradas |
| Suíte real | `core_fpga_sdram`, com o core | `riscv-tools run`: carrega cada programa pela porta de debug JTAG e lê o resultado de volta, conferindo os goldens |

A suíte real mede correção, não ciclos. O tempo de cada programa é dominado pelo carregamento e pela leitura pelo JTAG, e o custo em ciclos vem do nível 4.

| | |
| :--- | :--- |
| Pega | o que só o hardware mostra, como atraso de pino, fase do clock, captura de leitura e cadeia JTAG |
| Não pega | variação entre placas, temperaturas e lotes, que exige repetir em outras condições |
| Comandos | `quartus_stp -t jtag/sdram_bist.tcl <modo>` e `riscv-tools --config <config> run` |
| Custo | minutos por rodada, mais a compilação do Quartus |

## 11. Qual nível pega qual erro

| Classe de erro | Primeiro nível que pega |
| :--- | :--- |
| endereço do mapa de memória diferente em duas cópias | 0 |
| erro de sintaxe, tipo ou porta | 1 |
| lógica errada em um bloco | 2 |
| instrução ou hazard errado em sequência de programa | 3 |
| protocolo da SDRAM violado, dado que não chega ao chip | 4 |
| IP de memória real que se comporta diferente do modelo | 5 |
| VHDL que simula e não sintetiza | 6 |
| violação de protocolo em um estado que nenhum teste alcança | 7 |
| atraso de pino e fase de clock | 8 |

A captura de leitura da SDRAM é uma classe do nível 8. O modelo de simulação com registradores nos pinos supõe um ciclo a mais entre o pino e o registrador do que o hardware tem, e nenhum nível de 0 a 7 mostra a diferença; só o autoteste na placa acusa as palavras lidas erradas.
