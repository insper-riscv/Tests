# Alvos do repositório de testes. A regressão usa o riscv-tools da imagem do Infra
# (ghcr.io/insper-riscv/ci-elf), que traz GHDL, o GCC RISC-V e o Spike.
.PHONY: regression regression-no-spike flash-update-images flash-update-check

# Todos os programas em cada plataforma, a verificação cruzada com o Spike e o relatório único
# em build/regression/report.md.
regression:
	python3 -I tools/regression/regression.py

regression-no-spike:
	python3 -I tools/regression/regression.py --skip-spike

# O programa da banca B que os testes de atualização enviam (c-sdram-bank-b/update-new), construído como a imagem da
# banca B e escrito no uart_input.txt dos testes; flash-update-check só confere se os arquivos estão em dia.
flash-update-images:
	tools/flash_update/make_images.sh

flash-update-check:
	tools/flash_update/make_images.sh --check
