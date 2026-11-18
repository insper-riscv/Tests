"""Roda os programas de boot do Tests num perfil, na plataforma modular simulada, e confere a BOOT montada.

Cada combinação (``COMBINATIONS``) é um perfil do núcleo mais uma arquitetura de memórias. O roteiro copia o
repositório para uma pasta de trabalho, ajusta ali o ``platform.yaml`` da plataforma modular do Insper-RISC-V
(sem tocar no submódulo), regenera o mapa, a ``boot_config.h`` e o ``config_pkg`` para o perfil, monta a BOOT e
roda os programas de ``c-boot/`` (e os de ``cxx-boot/``, quando a toolchain tem a ``libstdc++`` do perfil) na
simulação com o GHDL. Depois confere o que o disassembly e a tabela de símbolos da BOOT têm de mostrar.

As conferências estáticas da BOOT, por perfil, são o tamanho (cabe nos 4 KiB da BOOT_MEMORY), o ponto de espera
``rv32_wait_restart`` em 0x100, o ``fence.i`` só com Zifencei e nenhuma instrução de CSR sem Zicsr.

Roda onde estão o ``riscv-tools``, o GHDL e o GCC RISC-V, isto é, na imagem ``ci-elf`` ou ``dev_tools``.
"""

import argparse
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import yaml

TESTS_ROOT = Path(__file__).resolve().parents[2]
PLATFORM = "Insper-RISC-V/src/simulation/modular"
OBJDUMP = "riscv32-unknown-elf-objdump"
NM = "riscv32-unknown-elf-nm"

# O tamanho da BOOT_MEMORY da plataforma modular, e onde a BOOT de teste fixa o ponto de espera.
BOOT_MEMORY_BYTES = 0x1000
WAIT_RESTART_ADDRESS = 0x100
# O limite da simulação de um programa de boot, em ciclos do núcleo (a plataforma dá 2 000 000, que num programa
# preso leva horas).
SIM_TIMEOUT_CYCLES = 600_000
# O que o riscv-tools escreve quando a toolchain não tem a libstdc++ da variante do perfil.
LIBSTDCXX_MISSING = "libstdc++"
# O limite de um instante, para o runner montar a BOOT quando só ela interessa (--no-sim).
NO_SIM_TIMEOUT_CYCLES = 1_000

# As memórias dentro do chip (arquitetura A) e a ROM e a RAM atrás das caches (arquitetura D).
MEMORY_INTERNAL = "rom=int,ram=int,icache=off,dcache=off"
MEMORY_CACHES = "rom=ext,ram=ext,icache=on,dcache=on"

_VERDICT = re.compile(r"^\s*(PASS|FAIL|TIMEOUT)\s+(\S+)")
_CSR_INSTRUCTION = re.compile(r"\bcsr(?:r[wsc]?|[wsc])i?\b")
_IGNORED_ON_COPY = shutil.ignore_patterns(".git", "build*", "logs", "__pycache__", ".venv", "variations", "sim_build")


@dataclass(frozen=True)
class Combination:
    """Um perfil do núcleo numa arquitetura de memórias.

    Attributes
    ----------
    profile : str
        O nome do perfil da matriz do Core, na ordem canônica das extensões.
    memory : str
        O valor do ``--memory`` do ``riscv-tools``.
    """

    profile: str
    memory: str

    @property
    def extensions(self) -> frozenset[str]:
        """As extensões do nome do perfil, em minúsculas e sem a base.

        Returns
        -------
        frozenset of str
            Por exemplo ``{"m", "zicsr"}`` para ``rv32im_zicsr``.
        """
        head, *rest = self.profile.split("_")
        letters = head.removeprefix("rv32")[1:]
        return frozenset([*letters, *rest])

    @property
    def has_zicsr(self) -> bool:
        """Diz se o núcleo do perfil tem os CSR, que Zicntr e Zihpm exigem.

        Returns
        -------
        bool
            Verdadeiro quando o nome do perfil traz Zicsr, Zicntr ou Zihpm.
        """
        return bool(self.extensions & {"zicsr", "zicntr", "zihpm"})

    @property
    def is_e(self) -> bool:
        """Diz se a base do perfil é a RV32E.

        Returns
        -------
        bool
            Verdadeiro para ``rv32e...``.
        """
        return self.profile.startswith("rv32e")


# O mínimo, a base reduzida, o perfil com Zicsr e o completo com caches (que exige o fence.i da BOOT).
COMBINATIONS: dict[str, Combination] = {
    "rv32i": Combination("rv32i", MEMORY_INTERNAL),
    "rv32e": Combination("rv32e", MEMORY_INTERNAL),
    "rv32im_zicsr": Combination("rv32im_zicsr", MEMORY_INTERNAL),
    "caches": Combination("rv32im_zicclsm_zicntr_zihpm_zifencei", MEMORY_CACHES),
}


def run(command: list[str], cwd: Path, log: Path) -> int:
    """Roda um comando, acrescenta a saída ao log e devolve o status.

    Parameters
    ----------
    command : list of str
        O comando e os argumentos.
    cwd : Path
        A pasta onde roda.
    log : Path
        O arquivo que recebe a saída padrão e a de erro.

    Returns
    -------
    int
        O código de saída.
    """
    with log.open("a", encoding="utf-8") as handle:
        handle.write(f"$ {' '.join(command)}\n")
        handle.flush()
        return subprocess.run(command, cwd=cwd, stdout=handle, stderr=subprocess.STDOUT, check=False).returncode


def drop_interrupts(platform: dict[str, Any]) -> None:
    """Tira de um mapa de plataforma tudo o que depende das interrupções de máquina.

    Parameters
    ----------
    platform : dict of {str: Any}
        O ``platform.yaml`` lido, alterado no lugar: o extra ``Interrupts`` do núcleo, o bloco
        ``interrupts`` e as janelas do CLINT e do PLIC, que só existem com elas.
    """
    core = platform["core"]
    core["extensions"] = [e for e in core["extensions"] if e != "Interrupts"]
    platform.pop("interrupts", None)
    for name in ("CLINT", "PLIC"):
        platform["peripherals"].pop(name, None)


def adjust_platform(work: Path, combination: Combination, cxx: bool) -> None:
    """Ajusta, na cópia, o ``platform.yaml`` e o ``config.yaml`` da plataforma modular.

    Sem Zicsr o núcleo recusa as interrupções, que a plataforma lista como extra (``--profile`` não troca
    os extras), então a cópia as perde junto com o CLINT e o PLIC. Para C++ o arquivo de specs liga os
    objetos que registram os quadros de exceção e o runtime ganha as chamadas de sistema que a
    ``libstdc++`` pede.

    Parameters
    ----------
    work : Path
        A raiz da cópia do repositório.
    combination : Combination
        O perfil.
    cxx : bool
        Se a cópia monta programas em C++.
    """
    platform_file = work / PLATFORM / "platform.yaml"
    platform: dict[str, Any] = yaml.safe_load(platform_file.read_text(encoding="utf-8"))
    if not combination.has_zicsr:
        drop_interrupts(platform)
    if cxx:
        platform["generate"]["specs"]["cxx"] = True
        platform["generate"]["syscalls"] = {"path": f"{PLATFORM.removeprefix('Insper-RISC-V/')}/runtime/syscalls.c"}
    platform_file.write_text(yaml.safe_dump(platform, sort_keys=False), encoding="utf-8")
    if cxx:
        config_file = work / PLATFORM / "config.yaml"
        config: dict[str, Any] = yaml.safe_load(config_file.read_text(encoding="utf-8"))
        config["paths"]["sources"].append(platform["generate"]["syscalls"]["path"])
        config_file.write_text(yaml.safe_dump(config, sort_keys=False), encoding="utf-8")


def config_text(c_dir: str, cxx: bool, timeout_cycles: int = SIM_TIMEOUT_CYCLES) -> str:
    """Escreve o config do Tests que estende o config gerado da plataforma.

    Parameters
    ----------
    c_dir : str
        A pasta dos programas (``c-boot`` ou ``cxx-boot``).
    cxx : bool
        Se liga o ``toolchain.cxx``.
    timeout_cycles : int
        O limite da simulação, em ciclos do núcleo.

    Returns
    -------
    str
        O texto do YAML.
    """
    toolchain = "toolchain:\n  cxx: true\n" if cxx else ""
    return (
        "# Gerado por boot_profile.py: os programas de boot sobre o config da plataforma modular.\n"
        "extends: config.generated.yaml\n"
        f"{toolchain}"
        "paths:\n"
        "  include_dir: tools/riscv_build/include-modular\n"
        "  build_dir: build\n"
        f"  c_dir: {c_dir}\n"
        "  asm_dir: asm-boot\n"
        "sim:\n"
        "  env:\n"
        f"    SIM_TIMEOUT_CYCLES: '{timeout_cycles}'\n"
    )


def prepare(work: Path, combination: Combination, cxx: bool, log: Path, timeout_cycles: int) -> bool:
    """Monta a cópia de trabalho e regenera o mapa, o config e o cabeçalho para o perfil.

    Parameters
    ----------
    work : Path
        A pasta de trabalho, que é recriada.
    combination : Combination
        O perfil e as memórias.
    cxx : bool
        Se a cópia monta programas em C++.
    log : Path
        O log, que recebe a saída dos passos.
    timeout_cycles : int
        O limite da simulação, em ciclos do núcleo.

    Returns
    -------
    bool
        Verdadeiro quando todos os passos deram certo.
    """
    shutil.rmtree(work, ignore_errors=True)
    shutil.copytree(TESTS_ROOT, work, ignore=_IGNORED_ON_COPY, symlinks=True)
    adjust_platform(work, combination, cxx)
    options = ["--profile", combination.profile, "--memory", combination.memory]
    steps = [
        [
            "riscv-tools",
            "--root",
            "Insper-RISC-V",
            *options,
            "generate-memory-map",
            "--platform",
            "src/simulation/modular/platform.yaml",
        ],
        [
            "riscv-tools",
            *options,
            "platform",
            "sync",
            "--prefix",
            "Insper-RISC-V",
            "--source",
            f"{PLATFORM}/config.yaml",
            "--output",
            "tools/riscv_build/config.generated.yaml",
        ],
    ]
    code = all(run(step, work, log) == 0 for step in steps)
    config = work / "tools/riscv_build/config.boot.yaml"
    config.write_text(config_text("cxx-boot" if cxx else "c-boot", cxx, timeout_cycles), encoding="utf-8")
    return code


def parse_verdicts(text: str) -> dict[str, str]:
    """Lê as linhas ``PASS nome`` do resumo da simulação.

    Parameters
    ----------
    text : str
        A saída do ``riscv-tools sim``.

    Returns
    -------
    dict of {str: str}
        O veredito de cada programa que aparece no resumo.
    """
    found: dict[str, str] = {}
    for line in text.splitlines():
        match = _VERDICT.match(line)
        if match:
            found[match.group(2)] = match.group(1)
    return found


def simulate(work: Path, log: Path, run_sim: bool = True) -> dict[str, str]:
    """Compila e simula os programas do config de boot da cópia.

    Parameters
    ----------
    work : Path
        A pasta de trabalho já preparada.
    log : Path
        O log da combinação.
    run_sim : bool
        Falso para só montar a BOOT: a simulação roda um instante, para o runner montá-la, e os vereditos dos
        programas são ignorados.

    Returns
    -------
    dict of {str: str}
        O veredito de cada programa, vazio quando a compilação falhou.
    """
    config = ["riscv-tools", "--config", "tools/riscv_build/config.boot.yaml"]
    for step in (["generate-header"], ["compile", "--emit", "hex"]):
        if run([*config, *step], work, log) != 0:
            return {}
    sim_log = work / "sim.log"
    sim_log.unlink(missing_ok=True)
    run([*config, "sim"], work, sim_log)
    text = sim_log.read_text(encoding="utf-8")
    with log.open("a", encoding="utf-8") as handle:
        handle.write(text)
    return parse_verdicts(text) if run_sim else {"só a BOOT, sem simulação": "PASS"}


def disassembly(elf: Path) -> str:
    """Desmonta a BOOT.

    Parameters
    ----------
    elf : Path
        O ELF da BOOT_MEMORY.

    Returns
    -------
    str
        A saída do ``objdump -d``.
    """
    return subprocess.run([OBJDUMP, "-d", str(elf)], capture_output=True, text=True, check=True).stdout


def symbol_address(elf: Path, name: str) -> int | None:
    """Acha o endereço de um símbolo da BOOT.

    Parameters
    ----------
    elf : Path
        O ELF da BOOT_MEMORY.
    name : str
        O nome do símbolo.

    Returns
    -------
    int or None
        O endereço, ou ``None`` quando o símbolo não existe.
    """
    table = subprocess.run([NM, str(elf)], capture_output=True, text=True, check=True).stdout
    for line in table.splitlines():
        fields = line.split()
        if len(fields) == 3 and fields[2] == name:
            return int(fields[0], 16)
    return None


def boot_problems(combination: Combination, size: int, text: str, wait: int | None) -> list[str]:
    """Aponta o que a BOOT montada tem de errado para o perfil.

    Parameters
    ----------
    combination : Combination
        O perfil.
    size : int
        O tamanho da imagem da BOOT, em bytes.
    text : str
        O disassembly da BOOT.
    wait : int or None
        O endereço de ``rv32_wait_restart``.

    Returns
    -------
    list of str
        Um problema por item; vazio quando a BOOT está certa.
    """
    problems: list[str] = []
    if size > BOOT_MEMORY_BYTES:
        problems.append(f"a BOOT tem {size} bytes e a BOOT_MEMORY tem {BOOT_MEMORY_BYTES}")
    if wait != WAIT_RESTART_ADDRESS:
        problems.append(
            f"rv32_wait_restart em {wait if wait is None else hex(wait)}, esperado {WAIT_RESTART_ADDRESS:#x}"
        )
    has_fencei = "fence.i" in text
    if has_fencei != ("zifencei" in combination.extensions):
        problems.append(f"fence.i {'presente' if has_fencei else 'ausente'} contra o perfil {combination.profile}")
    if not combination.has_zicsr and _CSR_INSTRUCTION.search(text):
        problems.append("a BOOT tem instrução de CSR num perfil sem Zicsr")
    return problems


def check_boot(work: Path, combination: Combination) -> tuple[int, list[str]]:
    """Confere a BOOT que a compilação deixou na pasta de trabalho.

    Parameters
    ----------
    work : Path
        A pasta de trabalho já compilada.
    combination : Combination
        O perfil.

    Returns
    -------
    tuple of (int, list of str)
        O tamanho da BOOT em bytes e os problemas achados.
    """
    folder = work / "build/sim/boot_memory"
    size = (folder / "boot_memory.bin").stat().st_size
    elf = folder / "boot_memory.elf"
    problems = boot_problems(combination, size, disassembly(elf), symbol_address(elf, "rv32_wait_restart"))
    return size, problems


def report(name: str, size: int, verdicts: dict[str, str], problems: list[str]) -> bool:
    """Imprime o resultado de uma combinação.

    Parameters
    ----------
    name : str
        O nome da combinação.
    size : int
        O tamanho da BOOT.
    verdicts : dict of {str: str}
        O veredito de cada programa.
    problems : list of str
        Os problemas da BOOT.

    Returns
    -------
    bool
        Verdadeiro quando tudo passou.
    """
    print(f"== {name}: BOOT de {size} bytes de {BOOT_MEMORY_BYTES}")
    for program, verdict in sorted(verdicts.items()):
        print(f"  {verdict}  {program}")
    for problem in problems:
        print(f"  FAIL  boot: {problem}")
    return bool(verdicts) and all(v == "PASS" for v in verdicts.values()) and not problems


def run_phase(work: Path, combination: Combination, cxx: bool, timeout_cycles: int, run_sim: bool = True) -> bool:
    """Roda uma fase (C ou C++) de uma combinação numa pasta de trabalho.

    Parameters
    ----------
    work : Path
        A pasta de trabalho da fase, que é recriada.
    combination : Combination
        O perfil e as memórias.
    cxx : bool
        Verdadeiro na fase de C++.
    timeout_cycles : int
        O limite da simulação, em ciclos do núcleo.

    Returns
    -------
    bool
        Verdadeiro quando a fase passou, ou foi pulada por a toolchain não ter a ``libstdc++`` do perfil.
    """
    log = work.parent / f"{work.name}.log"
    log.unlink(missing_ok=True)
    if not prepare(work, combination, cxx, log, timeout_cycles):
        print(f"== {work.name}: FAIL na preparação, veja {log}")
        return False
    verdicts = simulate(work, log, run_sim)
    if cxx and not verdicts and LIBSTDCXX_MISSING in log.read_text(encoding="utf-8"):
        print(f"== {work.name}: sem libstdc++ para o perfil {combination.profile} nesta toolchain, fase pulada")
        return True
    size, problems = check_boot(work, combination) if verdicts else (0, ["a compilação falhou"])
    return report(work.name, size, verdicts, problems)


def run_combination(name: str, out: Path, phases: set[str], timeout_cycles: int, run_sim: bool = True) -> bool:
    """Roda as fases C e C++ de uma combinação.

    Parameters
    ----------
    name : str
        O nome em ``COMBINATIONS``.
    out : Path
        A pasta que recebe as cópias de trabalho e os logs.
    phases : set of str
        ``c`` e ``cxx``.
    timeout_cycles : int
        O limite da simulação, em ciclos do núcleo.

    Returns
    -------
    bool
        Verdadeiro quando todas as fases pedidas passaram.
    """
    out.mkdir(parents=True, exist_ok=True)
    results = [
        run_phase(
            out / (name if phase == "c" else f"{name}-cxx"), COMBINATIONS[name], phase == "cxx", timeout_cycles, run_sim
        )
        for phase in sorted(phases)
    ]
    return all(results)


def main() -> None:
    """Lê a linha de comando e roda as combinações pedidas."""
    parser = argparse.ArgumentParser(description=(__doc__ or "").split("\n", maxsplit=1)[0])
    parser.add_argument("--combination", nargs="+", default=list(COMBINATIONS), choices=list(COMBINATIONS))
    parser.add_argument("--phase", nargs="+", default=["c", "cxx"], choices=["c", "cxx"])
    parser.add_argument("--out", default="build-boot", help="pasta das cópias de trabalho e dos logs")
    parser.add_argument(
        "--timeout-cycles", type=int, default=SIM_TIMEOUT_CYCLES, help="limite da simulação, em ciclos do núcleo"
    )
    parser.add_argument("--no-sim", action="store_true", help="só compila e confere a BOOT, sem simular")
    args = parser.parse_args()
    results = [
        run_combination(
            name,
            TESTS_ROOT / args.out,
            set(args.phase),
            NO_SIM_TIMEOUT_CYCLES if args.no_sim else args.timeout_cycles,
            not args.no_sim,
        )
        for name in args.combination
    ]
    sys.exit(0 if all(results) else 1)


if __name__ == "__main__":
    main()
