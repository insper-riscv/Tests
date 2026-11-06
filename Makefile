# Alvos do repositório de testes. A regressão usa o riscv-tools da imagem do Infra
# (ghcr.io/insper-riscv/ci-elf), que traz GHDL, o GCC RISC-V e o Spike.
.PHONY: regression regression-no-spike

# Todos os programas em cada plataforma, a verificação cruzada com o Spike e o relatório único
# em build/regression/report.md.
regression:
	python3 -I tools/regression/regression.py

regression-no-spike:
	python3 -I tools/regression/regression.py --skip-spike
