# Regressão completa e verificação cruzada com o Spike

🌐 [Português](README.md) · [English](README.en.md)

## 1. Uso

`make regression` roda, a partir da raiz do repositório e dentro da imagem `ghcr.io/insper-riscv/ci-elf`, todos os programas de `asm/`, `c/`, `asm-sdram/` e `c-sdram/` em cada plataforma do plano, confere o resultado no Spike e escreve um relatório único. O alvo `make regression-no-spike` pula o Spike.

| Saída | Conteúdo |
| :--- | :--- |
| `build/regression/report.md` | Relatório em Markdown com as etapas, as exclusões e a verificação cruzada |
| `build/regression/report.json` | Veredito de cada programa em cada etapa |
| `build/regression/*.log` | Saída completa de cada comando |

O comando termina com erro se um programa falhar, se uma etapa não preparar ou se o Spike divergir da simulação em um programa sem classificação.

## 2. Plano

O arquivo `plan.json` define tudo o que a regressão roda.

| Campo | Significado |
| :--- | :--- |
| `stages` | Plataformas e configurações, em ordem; a primeira é a referência da verificação cruzada |
| `exclusions` | Programa excluído de uma etapa, com a justificativa escrita; sem justificativa o plano é recusado |
| `spike.divergences` | Programas em que o Spike e a simulação dão veredito diferente, com o motivo da diferença |

## 3. Verificação cruzada

O Spike roda cada programa da etapa de referência até o veredito do `tohost`. Uma diferença entre o veredito do Spike e o da simulação precisa estar em `spike.divergences`. Uma entrada que já não diverge aparece no relatório como classificação obsoleta, para o plano não acumular entradas mortas.

---

Copyright 2026 Insper. Licenciado sob a [Apache License, Version 2.0](../../LICENSE).
