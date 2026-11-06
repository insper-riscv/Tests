"""Programas de interrupção de asm-irq/ rodando no núcleo, em simulação.

Cada programa é montado com a toolchain RISC-V, carregado como imagem
da memória de instruções e executado no `core` com `has_interrupts`.
Um modelo do CLINT (msip, mtimecmp e mtime, no leiaute da SiFive) ocupa
uma região da faixa da RAM interna e acende as linhas `MSIP` e `MTIP`.
O programa se confere sozinho: grava em `tohost` o valor 1 se passou ou
`(código << 1) | 1` se falhou.
"""

import subprocess
from pathlib import Path

import cocotb
from cocotb.clock import Clock
from cocotb.triggers import RisingEdge, Timer

REPO = Path(__file__).resolve().parents[4]
ASM_DIR = REPO / "asm-irq"
INCLUDE_DIR = REPO / "tools" / "riscv_build" / "include-irq"
BUILD_DIR = REPO / "tests" / "python" / "sim_build" / "irq-programs"
CLINT_BASE = 0x00100000
TOHOST = 0x0002FFE8
RAM_BASE = 0x8000
CLINT_SIZE = 0x10000
MAX_CYCLES = 60000
TICKS_PER_CYCLE = 1

# A primeira palavra depois do reset não é executada; rv32_wait_restart
# é o laço em que os programas esperam depois de gravar `tohost`.
CRT = """\
    .section .text
    .globl _start
_start:
    nop
    j    main
    .globl rv32_wait_restart
rv32_wait_restart:
    j    rv32_wait_restart
"""


LINKER = """\
ENTRY(_start)
SECTIONS {
    . = 0;
    .text : { *(.text*) }
    .data : { *(.data*) *(.rodata*) }
}
"""


def build(name):
    """Monta o programa e devolve a imagem como {endereço: palavra}."""
    out = BUILD_DIR / name
    out.mkdir(parents=True, exist_ok=True)
    crt = out / "crt.S"
    crt.write_text(CRT)
    linker = out / "link.ld"
    linker.write_text(LINKER)
    elf = out / "program.elf"
    binary = out / "program.bin"
    flags = ["-march=rv32im_zicsr", "-mabi=ilp32", "-nostdlib"]
    flags += ["-nostartfiles", f"-T{linker}"]
    flags += [f"-DRV32_CLINT_BASE={CLINT_BASE:#x}", f"-I{INCLUDE_DIR}"]
    source = ASM_DIR / name / "src.S"
    subprocess.run(  # noqa: S603
        ["riscv32-unknown-elf-gcc", *flags, "-o", elf, crt, source],  # noqa: S607
        check=True,
    )
    subprocess.run(  # noqa: S603
        ["riscv32-unknown-elf-objcopy", "-O", "binary", elf, binary],  # noqa: S607
        check=True,
    )
    data = binary.read_bytes()
    data += bytes(-len(data) % 4)
    return {
        i: int.from_bytes(data[i : i + 4], "little")
        for i in range(0, len(data), 4)
    }


class Clint:
    """Modelo do CLINT: msip, mtimecmp e mtime de 64 bits."""

    def __init__(self):
        """Começa com mtime em zero e mtimecmp no máximo."""
        self.mtime = 0
        self.mtimecmp = (1 << 64) - 1
        self.msip = 0

    @property
    def timer(self):
        """MTIP: mtime maior ou igual a mtimecmp."""
        return int(self.mtime >= self.mtimecmp)

    def tick(self):
        """Avança mtime."""
        self.mtime = (self.mtime + TICKS_PER_CYCLE) % (1 << 64)

    def read(self, offset):
        """Lê o registrador de 32 bits no deslocamento dado."""
        if offset == 0x0000:
            return self.msip
        words = {
            0x4000: self.mtimecmp & 0xFFFFFFFF,
            0x4004: self.mtimecmp >> 32,
            0xBFF8: self.mtime & 0xFFFFFFFF,
            0xBFFC: self.mtime >> 32,
        }
        return words.get(offset, 0)

    def write(self, offset, value):
        """Grava o registrador de 32 bits no deslocamento dado."""
        if offset == 0x0000:
            self.msip = value & 1
        elif offset == 0x4000:
            self.mtimecmp = (self.mtimecmp & ~0xFFFFFFFF) | value
        elif offset == 0x4004:
            self.mtimecmp = (self.mtimecmp & 0xFFFFFFFF) | (value << 32)


def merge(old, new, byteena):
    """Mescla os bytes habilitados de new sobre old."""
    for byte in range(4):
        if byteena & (1 << byte):
            mask = 0xFF << (8 * byte)
            old = (old & ~mask) | (new & mask)
    return old


class Bus:
    """RAM interna síncrona com o CLINT numa região da sua faixa."""

    def __init__(self):
        """Começa com a RAM vazia e o CLINT parado."""
        self.ram = {}
        self.clint = Clint()
        self.queued = 0
        self.held = 0
        self.tohost = None

    def present(self, dut):
        """Mostra o dado lido no ciclo anterior."""
        self.held = self.queued
        dut.i_ram_rdata.value = self.held

    def capture(self, dut):
        """Aplica a escrita deste ciclo e enfileira a leitura."""
        if not int(dut.o_ram_en.value):
            return
        addr = int(dut.o_ram_addr.value) & ~3
        in_clint = CLINT_BASE <= addr < CLINT_BASE + CLINT_SIZE
        if int(dut.o_ram_wren.value):
            self.write(addr, in_clint, dut)
        if int(dut.o_ram_rden.value):
            if in_clint:
                self.queued = self.clint.read(addr - CLINT_BASE)
            else:
                self.queued = self.ram.get(addr, 0)

    def write(self, addr, in_clint, dut):
        """Grava uma palavra na RAM ou num registrador do CLINT."""
        value = int(dut.o_ram_wdata.value)
        if in_clint:
            self.clint.write(addr - CLINT_BASE, value)
            return
        byteena = int(dut.o_ram_byteena.value)
        self.ram[addr] = merge(self.ram.get(addr, 0), value, byteena)
        if addr == TOHOST:
            self.tohost = self.ram[addr]

    def lines(self, dut):
        """Escreve as linhas de interrupção do CLINT nas portas."""
        dut.i_irq_timer.value = self.clint.timer
        dut.i_irq_software.value = self.clint.msip
        dut.i_irq_external.value = 0


async def run(dut, image):
    """Roda a imagem até o programa gravar tohost; devolve seu valor."""
    bus = Bus()
    dut.i_rst.value = 1
    for port in ("i_boot_rom_data", "i_flash_data", "i_ram_rdata"):
        getattr(dut, port).value = 0
    cocotb.start_soon(Clock(dut.i_clk, 10, unit="ns").start())
    for _ in range(3):
        await RisingEdge(dut.i_clk)
    dut.i_rst.value = 0
    for _ in range(MAX_CYCLES):
        await RisingEdge(dut.i_clk)
        await Timer(1, unit="ns")
        bus.present(dut)
        word = image.get(int(dut.o_if_addr.value) & ~3, 0)
        dut.i_boot_rom_data.value = word
        dut.i_flash_data.value = word
        bus.capture(dut)
        bus.clint.tick()
        bus.lines(dut)
        if bus.tohost is not None:
            return bus.tohost
    return None


async def check(dut, name):
    """Roda o programa e confere que ele grava 1 em tohost."""
    got = await run(dut, build(name))
    assert got is not None, f"{name}: o programa não terminou"
    assert got == 1, f"{name}: falha de código {got >> 1}"


@cocotb.test()
async def test_irq_timer_basic(dut):
    """Interrupção de timer em modo direto, com mcause e mret."""
    await check(dut, "irq-timer-basic")


@cocotb.test()
async def test_irq_masked(dut):
    """A interrupção só é tomada com MIE, a máscara e o pendente."""
    await check(dut, "irq-masked")


@cocotb.test()
async def test_irq_mepc_precise(dut):
    """O mepc é a primeira instrução que não completou."""
    await check(dut, "irq-mepc-precise")


@cocotb.test()
async def test_irq_vectored(dut):
    """O modo vetorado de mtvec leva cada causa à sua entrada."""
    await check(dut, "irq-vectored")


@cocotb.test()
async def test_irq_priority(dut):
    """Software vem antes do timer, sobre a mesma instrução."""
    await check(dut, "irq-priority")


@cocotb.test()
async def test_irq_wfi(dut):
    """A wfi executa como nop, com e sem interrupção pendente."""
    await check(dut, "irq-wfi")


@cocotb.test()
async def test_irq_ecall(dut):
    """A interrupção vence o ecall, que trapa depois do retorno."""
    await check(dut, "irq-ecall")
